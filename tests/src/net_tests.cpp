// Frame protocol tests: codec, malformed/oversized/checksum, real loopback.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"

#include <cstring>

namespace {
using namespace contention;

std::vector<std::uint8_t> raw_frame(MsgType type, const std::uint8_t* p, std::size_t n) {
  return encode_frame(type, p, n);
}
}  // namespace

TEST(net_codec_roundtrip) {
  using namespace contention;
  std::vector<std::uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
  std::vector<std::uint8_t> bytes = raw_frame(MsgType::HELLO, payload.data(), payload.size());
  std::vector<Frame> frames;
  Status st = decode_frames(bytes.data(), bytes.size(), frames);
  CHECK(st.ok());
  CHECK(frames.size() == 1);
  CHECK(frames[0].type == MsgType::HELLO);
  CHECK(frames[0].payload == payload);
}

TEST(net_codec_rejects_malformed_magic) {
  using namespace contention;
  std::vector<std::uint8_t> bytes = raw_frame(MsgType::HELLO, nullptr, 0);
  bytes[0] ^= 0xFF;
  std::vector<Frame> frames;
  Status st = decode_frames(bytes.data(), bytes.size(), frames);
  CHECK(st.code == StatusCode::PROTOCOL_ERROR);
}

TEST(net_codec_rejects_oversized) {
  using namespace contention;
  // Manually craft a frame with a huge declared length (below header constraints).
  std::vector<std::uint8_t> bytes = encode_frame(MsgType::HELLO, nullptr, 0);
  // Patch the length field (bytes 12..15) to be oversized.
  std::uint32_t huge = kMaxFramePayload + 10;
  for (int i = 0; i < 4; ++i) bytes[12 + i] = static_cast<std::uint8_t>((huge >> (8 * i)) & 0xFF);
  std::vector<Frame> frames;
  Status st = decode_frames(bytes.data(), bytes.size(), frames);
  CHECK(st.code == StatusCode::PROTOCOL_ERROR);
}

TEST(net_codec_rejects_checksum_mismatch) {
  using namespace contention;
  std::vector<std::uint8_t> payload = {'d', 'a', 't', 'a'};
  std::vector<std::uint8_t> bytes = raw_frame(MsgType::EVIDENCE, payload.data(), payload.size());
  bytes[bytes.size() - 1] ^= 0xAA;  // corrupt the trailing crc
  std::vector<Frame> frames;
  Status st = decode_frames(bytes.data(), bytes.size(), frames);
  CHECK(st.code == StatusCode::PROTOCOL_ERROR);
}

TEST(net_codec_rejects_truncation) {
  using namespace contention;
  std::vector<std::uint8_t> payload = {'d', 'a', 't', 'a', 'x'};
  std::vector<std::uint8_t> bytes = raw_frame(MsgType::EVIDENCE, payload.data(), payload.size());
  std::vector<Frame> frames;
  Status st = decode_frames(bytes.data(), bytes.size() - 2, frames);
  CHECK(st.code == StatusCode::PROTOCOL_ERROR);
}

TEST(net_real_loopback_send_receive) {
  using namespace contention;
  socket_init();
  Listener listener;
  Status st = listener.bind_and_listen(0);
  CHECK(st.ok());
  std::uint16_t port = listener.bound_port();

  std::thread server([&]() {
    auto conn = listener.accept(5000);
    if (!conn) return;
    Frame in;
    Status r = conn->receive(in, true);
    CHECK(r.ok());
    CHECK(in.type == MsgType::HELLO);
    // Respond.
    std::uint8_t okb = 1;
    conn->send(MsgType::RPC_RESPONSE, &okb, 1);
  });

  auto client = connect_to("127.0.0.1", port);
  CHECK(client != nullptr);
  std::uint8_t hello = 7;
  CHECK(client->send(MsgType::HELLO, &hello, 1).ok());
  Frame resp;
  Status rr = client->receive(resp, true);
  CHECK(rr.ok());
  CHECK(resp.type == MsgType::RPC_RESPONSE);
  server.join();
  socket_shutdown();
}

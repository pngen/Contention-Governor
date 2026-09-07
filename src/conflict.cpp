// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/conflict.hpp"

namespace contention {

// Helpers that aggregate group residual effect and severity are implemented
// here so they can be unit-tested directly.
double group_effect_upper_bound(double pairwiseMax, std::size_t memberCount) noexcept {
  // The joint effect cannot exceed the sum of independent pairwise effects but
  // should not allocate the residual to any single member.
  return pairwiseMax * static_cast<double>(memberCount);
}

}  // namespace contention

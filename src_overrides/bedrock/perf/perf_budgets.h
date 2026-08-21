// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PERF_PERF_BUDGETS_H_
#define BEDROCK_PERF_PERF_BUDGETS_H_

#include <string>
#include <vector>

// Performance budgets (roadmap item 46).
//
// "Performance is good" is not a statement anyone can act on. Every claim
// Bedrock makes about speed is a number with a budget, a measurement method
// and a place where it is checked.
//
// Two kinds of budget live here, and the distinction is the honest part:
//
//   kMeasuredOnHost   — the component is dependency-free, so the number is
//                       measured in the host test suite on every commit.
//                       A regression fails CI.
//   kRequiresBuild    — startup, idle memory, JS benchmarks: these need a
//                       real Chromium build on real hardware. The budget is
//                       written down and the measurement is *pending*; it is
//                       never reported as passing until someone measures it.
//
// A budget with no measurement is listed as pending, not quietly dropped. The
// point is that the gap is visible.

namespace bedrock {
namespace perf {

enum class Method {
  kMeasuredOnHost,
  kRequiresBuild,
};

struct Budget {
  const char* id;
  const char* metric;      // what is measured
  double limit;            // the budget
  const char* unit;
  Method method;
  const char* how;         // how it is measured, precisely enough to repeat
};

class PerfBudgets {
 public:
  static const std::vector<Budget>& All();
  static const Budget& Get(const std::string& id);
  // Budgets that CI can actually enforce today.
  static std::vector<Budget> HostEnforced();
  // Budgets waiting on a real build. Listed, not hidden.
  static std::vector<Budget> Pending();
};

}  // namespace perf
}  // namespace bedrock

#endif  // BEDROCK_PERF_PERF_BUDGETS_H_

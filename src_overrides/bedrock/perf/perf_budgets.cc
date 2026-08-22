// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/perf/perf_budgets.h"

#include <string>
#include <vector>

namespace bedrock {
namespace perf {

const std::vector<Budget>& PerfBudgets::All() {
  static const std::vector<Budget> kBudgets = {
      {"filter_match", "Filter match per network request", 20.0,
       "microseconds", Method::kMeasuredOnHost,
       "50,000 rules loaded; 10,000 URL matches timed in "
       "filter_engine_test.cc; budget is the mean."},
      {"filter_list_parse", "Filter list parse throughput", 2.0,
       "seconds per 100k rules", Method::kMeasuredOnHost,
       "100,000 generated rules parsed by FilterEngine::AddList in "
       "perf_budgets_test.cc."},
      {"pipeline_decision", "Full blocking pipeline decision", 30.0,
       "microseconds", Method::kMeasuredOnHost,
       "BlockingPipeline::Evaluate over a loaded engine, 10,000 requests, "
       "mean, in perf_budgets_test.cc."},
      {"tab_model_op", "Tab model operation with 200 tabs", 200.0,
       "microseconds", Method::kMeasuredOnHost,
       "200 tabs; open/pin/group/sleep/close cycles timed in "
       "perf_budgets_test.cc; budget is the mean per operation."},
      {"theme_apply", "Theme property change", 50.0, "microseconds",
       Method::kMeasuredOnHost,
       "ThemeEngine set + validate + resolve, 10,000 iterations, mean."},
      {"history_search", "History search over 20,000 visits", 30.0,
       "milliseconds", Method::kMeasuredOnHost,
       "20,000 recorded visits, substring query, mean of 20 searches."},

      // Everything below needs a browser binary. The numbers are the targets
      // the design is aiming at, and they are marked pending until a build
      // exists to measure them on.
      {"startup_cold", "Cold start to first paint", 1200.0, "milliseconds",
       Method::kRequiresBuild,
       "Reference hardware, cold page cache, empty profile, 10 runs, median."},
      {"startup_warm", "Warm start to first paint", 400.0, "milliseconds",
       Method::kRequiresBuild,
       "Reference hardware, warm page cache, existing profile with 20 tabs "
       "restored lazily, 10 runs, median."},
      {"tab_open", "New tab to interactive", 150.0, "milliseconds",
       Method::kRequiresBuild, "about:blank tab, 20 runs, median."},
      {"idle_memory", "Idle private memory, 1 blank tab", 350.0, "MB",
       Method::kRequiresBuild,
       "60 s after startup, PSS on Linux, no extensions."},
      {"memory_per_tab", "Additional memory per idle content tab", 45.0, "MB",
       Method::kRequiresBuild, "10 identical pages, PSS delta, mean."},
      {"idle_cpu", "CPU while idle with 10 open tabs", 0.5, "percent",
       Method::kRequiresBuild, "5 minutes idle, mean of one core."},
      {"network_overhead", "Added latency per request from the privacy stack",
       1.0, "milliseconds", Method::kRequiresBuild,
       "Loopback server, 1,000 requests, difference against the same build "
       "with the pipeline disabled."},
      {"js_benchmark", "JS benchmark score loss versus stock Chromium", 3.0,
       "percent", Method::kRequiresBuild,
       "Speedometer-class benchmark, same hardware, 5 runs, median; "
       "fingerprint protection at the default level."},
  };
  return kBudgets;
}

const Budget& PerfBudgets::Get(const std::string& id) {
  for (const Budget& budget : All()) {
    if (id == budget.id)
      return budget;
  }
  return All().front();
}

std::vector<Budget> PerfBudgets::HostEnforced() {
  std::vector<Budget> budgets;
  for (const Budget& budget : All()) {
    if (budget.method == Method::kMeasuredOnHost)
      budgets.push_back(budget);
  }
  return budgets;
}

std::vector<Budget> PerfBudgets::Pending() {
  std::vector<Budget> budgets;
  for (const Budget& budget : All()) {
    if (budget.method == Method::kRequiresBuild)
      budgets.push_back(budget);
  }
  return budgets;
}

}  // namespace perf
}  // namespace bedrock

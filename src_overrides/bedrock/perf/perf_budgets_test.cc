// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.
//
// Measures the budgets that can be measured without a browser build, and
// prints every number it measured. A performance claim with no number in the
// log is not a claim this project makes.

#include "bedrock/perf/perf_budgets.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include "bedrock/blocking/blocking_pipeline.h"
#include "bedrock/blocking/filter_engine.h"
#include "bedrock/blocking/tracker_heuristic.h"
#include "bedrock/data/history_store.h"
#include "bedrock/privacy/protection_controller.h"
#include "bedrock/ui/tab_model.h"
#include "bedrock/ui/theme_engine.h"

namespace {

using bedrock::perf::Budget;
using bedrock::perf::Method;
using bedrock::perf::PerfBudgets;
using Clock = std::chrono::steady_clock;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

void Report(const std::string& id, double measured, const char* unit) {
  const Budget& budget = PerfBudgets::Get(id);
  std::cout << "  " << std::left << std::setw(20) << id << std::right
            << std::setw(10) << std::fixed << std::setprecision(3) << measured
            << " " << unit << "  (budget " << budget.limit << " " << unit
            << ")\n";
  Check(measured <= budget.limit,
        id + " is within budget: " + std::to_string(measured) + " " + unit +
            " > " + std::to_string(budget.limit));
}

double SecondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

std::string GenerateRules(int count) {
  std::string list;
  for (int i = 0; i < count; ++i) {
    list += "||ads" + std::to_string(i) + ".example^$third-party\n";
    if (i % 7 == 0)
      list += "/track" + std::to_string(i) + "/*$script\n";
  }
  return list;
}

}  // namespace

int main() {
  std::cout << "perf budgets (measured on host):\n";

  // ---- filter list parse throughput ----
  {
    const std::string list = GenerateRules(100000);
    bedrock::blocking::FilterEngine engine;
    const auto start = Clock::now();
    const size_t accepted = engine.AddList(list);
    const double seconds = SecondsSince(start);
    Check(accepted > 100000, "the generated list was actually parsed");
    Report("filter_list_parse", seconds, "s/100k");
  }

  // ---- pipeline decision ----
  {
    bedrock::blocking::FilterEngine engine;
    engine.AddList(GenerateRules(20000));
    bedrock::blocking::TrackerHeuristic heuristic;
    bedrock::privacy::ProtectionController controls;
    bedrock::blocking::BlockingPipeline pipeline(&engine, &heuristic, &controls);

    bedrock::blocking::Request request;
    request.top_host = "news.example";
    request.top_etld1 = "news.example";
    request.type = bedrock::blocking::ResourceType::kScript;

    const int iterations = 10000;
    const auto start = Clock::now();
    for (int i = 0; i < iterations; ++i) {
      request.url = "https://ads" + std::to_string(i % 5000) +
                    ".example/tag.js?id=" + std::to_string(i);
      request.host = "ads" + std::to_string(i % 5000) + ".example";
      request.etld1 = request.host;
      pipeline.Evaluate(request);
    }
    Report("pipeline_decision", SecondsSince(start) / iterations * 1e6, "us");
  }

  // ---- tab model with 200 tabs ----
  {
    bedrock::ui::TabModel tabs;
    const int count = 200;
    const auto start = Clock::now();
    int operations = 0;
    for (int i = 0; i < count; ++i) {
      const int id = tabs.Add("Tab " + std::to_string(i),
                              "https://site" + std::to_string(i) +
                                  ".example/page",
                              1787000000000LL + i);
      ++operations;
      if (i % 10 == 0) {
        tabs.SetPinned(id, true);
        ++operations;
      }
      if (i % 4 == 0) {
        tabs.Activate(id, 1787000000000LL + i);
        ++operations;
      }
    }
    tabs.SleepIdleTabs(1787000600000LL, 300000);
    ++operations;
    tabs.FindDuplicates();
    ++operations;
    tabs.Search("Tab 1");
    ++operations;
    Report("tab_model_op", SecondsSince(start) / operations * 1e6, "us");
  }

  // ---- theme apply ----
  {
    bedrock::ui::ThemeEngine theme;
    const int iterations = 10000;
    const auto start = Clock::now();
    for (int i = 0; i < iterations; ++i) {
      theme.Set(bedrock::ui::Property::kCornerRadius, 4 + (i % 12));
      theme.Validate();
    }
    Report("theme_apply", SecondsSince(start) / iterations * 1e6, "us");
  }

  // ---- history search ----
  {
    bedrock::data::HistoryStore history;
    for (int i = 0; i < 20000; ++i) {
      history.Record("https://site" + std::to_string(i % 900) +
                         ".example/page" + std::to_string(i),
                     "Page " + std::to_string(i), 1787000000 + i, i % 5 == 0);
    }
    const int searches = 20;
    const auto start = Clock::now();
    for (int i = 0; i < searches; ++i)
      history.Search("page" + std::to_string(i * 37));
    Report("history_search", SecondsSince(start) / searches * 1e3, "ms");
  }

  // ---- the budget table itself has to stay honest ----
  Check(!PerfBudgets::HostEnforced().empty(), "some budgets are enforced here");
  Check(!PerfBudgets::Pending().empty(),
        "and the ones needing a real build are listed rather than dropped");
  for (const Budget& budget : PerfBudgets::All()) {
    Check(budget.limit > 0, std::string(budget.id) + " has a number");
    Check(std::string(budget.how).size() > 30,
          std::string(budget.id) + " says how it is measured, repeatably");
    Check(std::string(budget.unit).size() > 0,
          std::string(budget.id) + " has a unit");
  }
  std::cout << "  " << PerfBudgets::Pending().size()
            << " budgets pending a real build (see docs/performance/BUDGETS.md)\n";

  if (failures == 0)
    std::cout << "perf_budgets_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}

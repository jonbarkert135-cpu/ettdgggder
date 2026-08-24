// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/tracker_blocker/url_cleaner.h"

#include <iostream>
#include <string>

#include "bedrock/privacy/tracker_blocker/blocking_pipeline.h"

namespace {

using bedrock::blocking::CleanResult;
using bedrock::blocking::UrlCleaner;
using bedrock::blocking::UrlUse;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

CleanResult Nav(const std::string& url) {
  return UrlCleaner::Clean(url, UrlUse::kTopLevelNavigation);
}

}  // namespace

int main() {
  // Stripping: the identifier goes, everything the page needs stays, in order.
  {
    const CleanResult r =
        Nav("https://shop.test/item?id=42&utm_source=mail&color=red&fbclid=XYZ");
    Check(r.url == "https://shop.test/item?id=42&color=red",
          "tracking parameters removed, functional ones kept in order");
    Check(r.stripped.size() == 2, "both identifiers reported");
    Check(r.changed(), "changed() is true when something was stripped");
  }

  // A URL with nothing to strip comes back byte-identical — no normalisation,
  // no re-encoding, no dropped empty parameter. Rewriting a URL that needed no
  // cleaning is how a cleaner breaks sites without buying any privacy.
  {
    const std::string url = "https://shop.test/item?id=42&flag=&x#frag";
    const CleanResult r = Nav(url);
    Check(r.url == url, "untouched when there is nothing to strip");
    Check(!r.changed(), "and reported as unchanged");
  }

  // The fragment survives cleaning; application state lives there.
  {
    const CleanResult r = Nav("https://shop.test/i?gclid=1&q=2#section-3");
    Check(r.url == "https://shop.test/i?q=2#section-3", "fragment preserved");
  }

  // Query gone entirely: no trailing '?'.
  {
    const CleanResult r = Nav("https://shop.test/i?fbclid=1");
    Check(r.url == "https://shop.test/i", "no empty query left behind");
  }

  // Case: parameter names are matched case-insensitively, values never.
  {
    const CleanResult r = Nav("https://shop.test/i?FBCLID=Keep&ref=FBCLID");
    Check(r.url == "https://shop.test/i?ref=FBCLID",
          "name matched case-insensitively, a value that merely looks like one "
          "is left alone");
  }

  // Subresources are never cleaned — a "tracking-looking" parameter on a
  // script is often load-bearing.
  {
    const std::string url = "https://cdn.test/app.js?utm_source=x";
    Check(UrlCleaner::Clean(url, UrlUse::kSubresource).url == url,
          "subresource URLs are returned untouched");
    Check(!UrlCleaner::Clean(url, UrlUse::kSubresource).changed(),
          "and unchanged");
  }

  // Debouncing: the wrapper hop is skipped and the target is cleaned too.
  {
    const CleanResult r =
        Nav("https://out.reddit.com/?url=https%3A%2F%2Freal.test%2Fa%3Fgclid%3D9"
            "&token=abc");
    Check(r.url == "https://real.test/a", "redirector unwrapped and cleaned");
    Check(r.unwrapped_hops == 1, "one hop reported");
  }

  // href.li style: the whole query string is the target.
  {
    const CleanResult r = Nav("https://href.li/?https://real.test/x");
    Check(r.url == "https://real.test/x", "whole-query redirector unwrapped");
  }

  // Refusals. A cleaner that can retarget a navigation is an open redirector,
  // so anything that is not an absolute http(s) URL from a known redirector
  // stays exactly as it was.
  {
    Check(Nav("https://out.reddit.com/?url=javascript%3Aalert(1)").url ==
              "https://out.reddit.com/?url=javascript%3Aalert(1)",
          "javascript: target refused");
    Check(Nav("https://out.reddit.com/?url=%2Flocal%2Fpath").url ==
              "https://out.reddit.com/?url=%2Flocal%2Fpath",
          "relative target refused");
    Check(Nav("https://out.reddit.com/?url=data%3Atext%2Fhtml%2Chi").url ==
              "https://out.reddit.com/?url=data%3Atext%2Fhtml%2Chi",
          "data: target refused");
    Check(Nav("https://unknown.test/?url=https%3A%2F%2Freal.test%2F").url ==
              "https://unknown.test/?url=https%3A%2F%2Freal.test%2F",
          "unlisted host is not treated as a redirector");
    Check(Nav("https://out.reddit.com/?url=https%3A%2F%2Fout.reddit.com%2Fx")
                  .unwrapped_hops == 0,
          "a redirector pointing at itself is not followed");
  }

  // Chains terminate at kMaxHops even when every hop is a valid redirector.
  {
    std::string url = "https://real.test/end";
    for (int i = 0; i < 6; ++i) {
      std::string encoded;
      for (char c : url) {
        if (c == ':') {
          encoded += "%3A";
        } else if (c == '/') {
          encoded += "%2F";
        } else if (c == '?') {
          encoded += "%3F";
        } else if (c == '=') {
          encoded += "%3D";
        } else {
          encoded += c;
        }
      }
      // Alternating hosts: a redirector is never followed to itself.
      url = (i % 2 == 0 ? "https://out.reddit.com/?url=" : "https://away.vk.com/?to=") +
            encoded;
    }
    const CleanResult r = Nav(url);
    Check(r.unwrapped_hops == UrlCleaner::kMaxHops, "hop budget is enforced");
    Check(r.url.rfind("https://out.reddit.com/", 0) == 0,
          "the remaining wrapper is left intact rather than unwrapped blindly");
  }

  // Not a URL at all: no crash, no change.
  {
    Check(Nav("").url.empty(), "empty input survives");
    Check(Nav("about:blank").url == "about:blank", "non-http scheme untouched");
    Check(Nav("?fbclid=1").url == "?fbclid=1",
          "a bare query with no scheme is not a navigation target we rewrite "
          "into something else — it still strips nothing above the path");
  }

  // One table, two callers: the pipeline's CleanUrl() is this component.
  {
    Check(bedrock::blocking::BlockingPipeline::CleanUrl(
              "https://shop.test/i?utm_id=7&q=1") == "https://shop.test/i?q=1",
          "BlockingPipeline::CleanUrl delegates to the cleaner");
    Check(UrlCleaner::IsTrackingParam("utm_id") &&
              !UrlCleaner::IsTrackingParam("q"),
          "the panel can ask about a parameter without its own table");
  }

  if (failures == 0) {
    std::cout << "url_cleaner_test: OK\n";
  }
  return failures == 0 ? 0 : 1;
}

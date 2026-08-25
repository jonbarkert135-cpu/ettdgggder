// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.
//
// Every case here is an attack, not a unit. The bugs this file locks out were
// all live in the tree: a name that looks like an address, a suffix that is not
// a label, and a name that differs only in case or in a trailing root dot.

#include "bedrock/privacy/network/host_match.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

void TestNormalisation() {
  Check(NormalizeHost("EXAMPLE.COM") == "example.com", "case is normalised");
  Check(NormalizeHost("example.com.") == "example.com",
        "one trailing root dot is dropped");
  Check(NormalizeHost("example.com..") == "example.com..",
        "a malformed name is not silently repaired");
  Check(NormalizeHost(".") == ".", "the root alone is left alone");
  Check(NormalizeHost("") == "", "the empty host stays empty");
}

void TestSubdomainMatching() {
  Check(IsOrSubdomainOf("example.com", "example.com"), "exact match");
  Check(IsOrSubdomainOf("a.b.example.com", "example.com"), "deep subdomain");
  Check(IsOrSubdomainOf("EXAMPLE.com", "example.COM"),
        "matching ignores case on both sides");
  Check(IsOrSubdomainOf("tracker.example.com.", "example.com"),
        "a trailing root dot does not evade a rule");
  Check(IsOrSubdomainOf("x.example.com", "example.com."),
        "a rule written with a root dot still matches");

  Check(!IsOrSubdomainOf("evilexample.com", "example.com"),
        "a suffix that is not a label boundary does not match");
  Check(!IsOrSubdomainOf("example.com.evil.test", "example.com"),
        "the domain appearing in the middle does not match");
  Check(!IsOrSubdomainOf("example.com", "a.example.com"),
        "a parent is not a subdomain of its child");
  Check(!IsOrSubdomainOf("anything.test", ""),
        "an empty rule matches nothing, rather than everything");
  Check(!IsOrSubdomainOf("", "example.com"), "an empty host matches nothing");
}

void TestFinalLabel() {
  Check(HasFinalLabel("service.onion", "onion"), "a real .onion name");
  Check(HasFinalLabel("PRINTER.Local", "local"), "case-insensitive label");
  Check(HasFinalLabel("host.internal.", "internal"), "root dot tolerated");

  Check(!HasFinalLabel("notonion.example.com", "onion"),
        "the label must be final");
  Check(!HasFinalLabel("evil-onion.test", "onion"),
        "a hyphen is not a label boundary");
  Check(!HasFinalLabel("onion", "onion"),
        "the bare label is not a name in that zone");
  Check(!HasFinalLabel("x.onion.evil.test", "onion"),
        "the label in the middle is not final");
}

void TestAddressParsing() {
  uint8_t octet[4] = {0, 0, 0, 0};
  Check(ParseIPv4("192.168.1.1", octet) && octet[0] == 192 && octet[3] == 1,
        "a dotted quad parses");
  Check(!ParseIPv4("10.example.com", octet), "a name is not an address");
  Check(!ParseIPv4("1.2.3", octet), "three octets is not an address");
  Check(!ParseIPv4("1.2.3.4.5", octet), "five octets is not an address");
  Check(!ParseIPv4("256.1.1.1", octet), "an octet over 255 is rejected");
  Check(!ParseIPv4("01.2.3.4444", octet), "an over-long octet is rejected");
  Check(!ParseIPv4("1.2.3.", octet), "a trailing dot is not an octet");
  Check(!ParseIPv4("", octet), "the empty string is not an address");
}

void TestPrivateAddresses() {
  Check(IsPrivateAddress("127.0.0.1"), "loopback");
  Check(IsPrivateAddress("10.0.0.5"), "RFC 1918 /8");
  Check(IsPrivateAddress("172.16.0.1") && IsPrivateAddress("172.31.255.255"),
        "RFC 1918 /12 at both ends");
  Check(IsPrivateAddress("192.168.0.1"), "RFC 1918 /16");
  Check(IsPrivateAddress("169.254.1.1"), "link-local");
  Check(IsPrivateAddress("[::1]") && IsPrivateAddress("[fd00::1]") &&
            IsPrivateAddress("[FE80::1]"),
        "IPv6 loopback, ULA and link-local, any case");

  // F1 itself: each of these was accepted as private by the prefix check, and
  // each is a name an attacker can register.
  Check(!IsPrivateAddress("10.example.com"), "10.example.com is a name");
  Check(!IsPrivateAddress("127.evil.test"), "127.evil.test is a name");
  Check(!IsPrivateAddress("192.168.attacker.net"),
        "192.168.attacker.net is a name");
  Check(!IsPrivateAddress("172.32.0.1"), "172.32/16 is public");
  Check(!IsPrivateAddress("172.15.0.1"), "172.15/16 is public");
  Check(!IsPrivateAddress("8.8.8.8"), "a public address is public");
  Check(!IsPrivateAddress("[fe00::1]"), "fe00:: is not link-local");
}

}  // namespace

int main() {
  TestNormalisation();
  TestSubdomainMatching();
  TestFinalLabel();
  TestAddressParsing();
  TestPrivateAddresses();
  if (failures == 0) {
    std::cout << "host_match_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}

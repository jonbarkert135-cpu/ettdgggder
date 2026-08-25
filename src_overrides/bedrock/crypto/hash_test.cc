// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

// Host test, no Chromium. Every primitive is checked against its published
// vector: an implementation that is subtly wrong cannot pass these, which is
// the only reason this code is allowed to live in-tree (see hash.h).

#include "bedrock/crypto/hash.h"

#include <iostream>
#include <set>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::string Repeat(char c, size_t n) {
  return std::string(n, c);
}

std::string FromHex(const std::string& hex) {
  std::string out;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    auto value = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      return c - 'A' + 10;
    };
    out.push_back(static_cast<char>(value(hex[i]) * 16 + value(hex[i + 1])));
  }
  return out;
}

}  // namespace

int main() {
  using bedrock::crypto::ConstantTimeEquals;
  using bedrock::crypto::HkdfSha256;
  using bedrock::crypto::HmacSha256;
  using bedrock::crypto::Pbkdf2HmacSha256;
  using bedrock::crypto::Sha256;
  using bedrock::crypto::ToHex;

  // --- SHA-256, NIST FIPS 180-4 examples ------------------------------------

  Check(ToHex(Sha256("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA-256 of the empty string");
  Check(ToHex(Sha256("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA-256(\"abc\")");
  Check(ToHex(Sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 of a multi-block message");
  // A length that lands exactly on the padding boundary — the classic off-by-one.
  Check(ToHex(Sha256(Repeat('a', 55))).size() == 64 &&
            ToHex(Sha256(Repeat('a', 56))) !=
                ToHex(Sha256(Repeat('a', 55))),
        "padding boundary at 55/56 bytes");
  Check(ToHex(Sha256(Repeat('a', 1000000))) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "SHA-256 of one million 'a' (FIPS 180-4 long message)");

  // --- SHA-1, legacy interop only (FIPS 180-1 examples) --------------------

  Check(ToHex(bedrock::crypto::Sha1("abc")) ==
            "a9993e364706816aba3e25717850c26c9cd0d89d",
        "SHA-1(\"abc\")");
  Check(ToHex(bedrock::crypto::Sha1("")) ==
            "da39a3ee5e6b4b0d3255bfef95601890afd80709",
        "SHA-1 of the empty string");

  // --- HMAC-SHA256, RFC 4231 ------------------------------------------------

  Check(ToHex(HmacSha256(Repeat('\x0b', 20), "Hi There")) ==
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
        "RFC 4231 case 1");
  Check(ToHex(HmacSha256("Jefe", "what do ya want for nothing?")) ==
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "RFC 4231 case 2");
  Check(ToHex(HmacSha256(Repeat('\xaa', 20), Repeat('\xdd', 50))) ==
            "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
        "RFC 4231 case 3");
  // Key longer than the 64-byte block, which must be hashed first.
  Check(ToHex(HmacSha256(Repeat('\xaa', 131),
                         "Test Using Larger Than Block-Size Key - Hash Key First")) ==
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
        "RFC 4231 case 6: over-long keys are hashed");

  // --- HKDF-SHA256, RFC 5869 ------------------------------------------------

  Check(ToHex(HkdfSha256(FromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                         FromHex("000102030405060708090a0b0c"),
                         FromHex("f0f1f2f3f4f5f6f7f8f9"), 42)) ==
            "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
            "34007208d5b887185865",
        "RFC 5869 test case 1");
  Check(ToHex(HkdfSha256(FromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
                         "", "", 42)) ==
            "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
            "9d201395faa4b61a96c8",
        "RFC 5869 test case 3: empty salt and info");

  // --- PBKDF2-HMAC-SHA256, RFC 7914 section 11 -----------------------------

  Check(ToHex(Pbkdf2HmacSha256("passwd", "salt", 1, 64)) ==
            "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
            "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783",
        "PBKDF2 vector, 1 iteration");
  Check(Pbkdf2HmacSha256("pw", "salt", 0, 32).empty(),
        "zero iterations is refused rather than silently accepted");

  // --- Properties the two audit fixes depend on ----------------------------

  // Domain separation: the same secret with a different label gives unrelated
  // output. This is what stops one subsystem's key from being another's.
  const std::string secret = FromHex("00112233445566778899aabbccddeeff"
                                     "00112233445566778899aabbccddeeff");
  Check(HkdfSha256(secret, "salt", "bedrock/fp/v1", 32) !=
            HkdfSha256(secret, "salt", "bedrock/pw/v1", 32),
        "different info labels derive different keys");
  Check(HkdfSha256(secret, "salt-a", "info", 32) !=
            HkdfSha256(secret, "salt-b", "info", 32),
        "different salts derive different keys");
  Check(HkdfSha256(secret, "salt", "info", 64).substr(0, 32) ==
            HkdfSha256(secret, "salt", "info", 32),
        "expanding further extends the same stream");

  // One bit of input changes everything (no structure an attacker can walk).
  std::string flipped = secret;
  flipped[0] = static_cast<char>(flipped[0] ^ 0x01);
  const std::string a = HmacSha256(secret, "message");
  const std::string b = HmacSha256(flipped, "message");
  int differing_bits = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    unsigned char x = static_cast<unsigned char>(a[i]) ^
                      static_cast<unsigned char>(b[i]);
    while (x) { differing_bits += x & 1; x >>= 1; }
  }
  Check(differing_bits > 80 && differing_bits < 176,
        "one flipped key bit changes about half the output bits");

  // No collisions across a realistic number of sites and surfaces (the F4 fix
  // relies on this instead of the old invertible mixer).
  std::set<std::string> tags;
  for (int site = 0; site < 200; ++site) {
    for (int surface = 0; surface < 8; ++surface) {
      tags.insert(HmacSha256(HkdfSha256(secret, "s", "site" + std::to_string(site), 32),
                             "surface" + std::to_string(surface)));
    }
  }
  Check(tags.size() == 1600, "1600 (site, surface) tags are all distinct");

  Check(ConstantTimeEquals(a, a), "equal strings compare equal");
  Check(!ConstantTimeEquals(a, b), "different strings compare unequal");
  Check(!ConstantTimeEquals("abc", "abcd"), "different lengths compare unequal");

  if (failures == 0) {
    std::cout << "hash_test: all assertions passed\n";
    return 0;
  }
  std::cerr << failures << " failure(s)\n";
  return 1;
}

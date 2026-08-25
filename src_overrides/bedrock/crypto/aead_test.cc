// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

// Host test, no Chromium. What is asserted here is the AEAD contract every
// caller depends on: nothing opens with the wrong key, the wrong aad, or a
// flipped bit; and the same plaintext under two nonces looks unrelated.

#include "bedrock/crypto/aead.h"

#include <iostream>
#include <set>
#include <string>

#include "bedrock/crypto/hash.h"

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::string Key(char filler) {
  return std::string(bedrock::crypto::kKeySize, filler);
}

std::string Nonce(char filler) {
  return std::string(bedrock::crypto::kNonceSize, filler);
}

}  // namespace

int main() {
  using bedrock::crypto::kNonceSize;
  using bedrock::crypto::kTagSize;
  using bedrock::crypto::Open;
  using bedrock::crypto::Seal;
  const std::string key = Key('\x11');
  const std::string plaintext = "correct horse battery staple";
  const std::string aad = "bedrock/pw/v1";

  const std::string sealed = Seal(key, Nonce('\x01'), aad, plaintext);
  Check(sealed.size() == kNonceSize + plaintext.size() + kTagSize,
        "the blob is nonce + ciphertext + tag");
  Check(sealed.find(plaintext) == std::string::npos,
        "the plaintext does not appear in the blob");

  std::string opened;
  Check(Open(key, aad, sealed, &opened) && opened == plaintext,
        "the right key and aad open it");

  // Every one of these must fail, and fail the same way.
  Check(!Open(Key('\x12'), aad, sealed, &opened), "a wrong key fails");
  Check(!Open(key, "bedrock/other/v1", sealed, &opened),
        "a wrong aad fails: a blob cannot be moved between contexts");
  Check(!Open(key, aad, sealed.substr(0, sealed.size() - 1), &opened),
        "a truncated blob fails");
  Check(!Open(key, aad, std::string(), &opened), "an empty blob fails");
  for (size_t i = 0; i < sealed.size(); ++i) {
    std::string tampered = sealed;
    tampered[i] = static_cast<char>(tampered[i] ^ 0x01);
    if (Open(key, aad, tampered, &opened)) {
      Check(false, "flipping byte " + std::to_string(i) + " must be detected");
      break;
    }
  }

  // `out` is only written on success, so a failed Open cannot leak a partial
  // decryption into the caller's buffer.
  std::string untouched = "sentinel";
  Check(!Open(Key('\x99'), aad, sealed, &untouched) &&
            untouched == "sentinel",
        "a failed Open leaves the output alone");

  // Two nonces, same plaintext: no relation visible, and both open.
  const std::string other = Seal(key, Nonce('\x02'), aad, plaintext);
  Check(other != sealed, "a fresh nonce gives a different blob");
  Check(other.substr(kNonceSize, plaintext.size()) !=
            sealed.substr(kNonceSize, plaintext.size()),
        "and a different keystream, not just a different header");
  Check(Open(key, aad, other, &opened) && opened == plaintext,
        "the second blob opens too");

  // Misuse is refused rather than producing a weak blob.
  Check(Seal("short", Nonce('\x01'), aad, plaintext).empty(),
        "a wrong-size key produces no blob at all");
  Check(Seal(key, "short", aad, plaintext).empty(),
        "a wrong-size nonce produces no blob at all");

  // Empty and long payloads.
  const std::string empty_blob = Seal(key, Nonce('\x03'), aad, "");
  Check(Open(key, aad, empty_blob, &opened) && opened.empty(),
        "an empty plaintext round-trips and is still authenticated");
  const std::string long_text(10000, 'x');
  const std::string long_blob = Seal(key, Nonce('\x04'), aad, long_text);
  Check(Open(key, aad, long_blob, &opened) && opened == long_text,
        "a payload spanning many keystream blocks round-trips");
  std::set<char> distinct(long_blob.begin() + kNonceSize, long_blob.end());
  Check(distinct.size() > 200,
        "the keystream does not repeat across blocks");

  if (failures == 0) {
    std::cout << "aead_test: all assertions passed\n";
    return 0;
  }
  std::cerr << failures << " failure(s)\n";
  return 1;
}

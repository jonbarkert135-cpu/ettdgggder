// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_CRYPTO_HASH_H_
#define BEDROCK_CRYPTO_HASH_H_

#include <cstddef>
#include <cstdint>
#include <string>

// SHA-256 and the three standard constructions built on it: HMAC (RFC 2104),
// HKDF (RFC 5869) and PBKDF2 (RFC 8018). Byte strings in, byte strings out.
//
// Why this exists in a repository whose rule is "crypto belongs to the
// platform": the two findings F3 and F4 of docs/security/AUDIT-2026-08-25.md
// are *key-derivation* bugs — a master password that is not a key, and an
// invertible unkeyed mixer standing in for a PRF. Fixing them needs a keyed
// one-way function, and Bedrock's own privacy code must be testable without a
// Chromium checkout. So:
//
//   * These are standards with published test vectors, and every function
//     here is checked against those vectors in hash_test.cc. That is the
//     entire reason it is acceptable to have them in-tree: a wrong
//     implementation cannot pass RFC 4231 / RFC 5869 / NIST vectors.
//   * Inside the Chromium build these are replaced by BoringSSL, which is
//     audited, constant-time and hardware-accelerated. The switch is
//     BEDROCK_USE_BORINGSSL (see hash.cc); the interface does not change.
//   * No block cipher is implemented here, and none should be. AES belongs to
//     BoringSSL — see aead.h for how that boundary is drawn.
//
// This file replaces the hand-rolled SHA-1 that used to live in
// passwords/password_store.cc.

namespace bedrock {
namespace crypto {

constexpr size_t kSha256Size = 32;

// Raw 32-byte digest (not hex).
std::string Sha256(const std::string& data);

// RFC 2104. Key of any length, 32-byte tag.
std::string HmacSha256(const std::string& key, const std::string& message);

// RFC 5869, extract-and-expand. `length` may exceed 32 bytes.
std::string HkdfSha256(const std::string& secret,
                       const std::string& salt,
                       const std::string& info,
                       size_t length);

// RFC 8018. Deliberately not the default password KDF — see kdf.h.
std::string Pbkdf2HmacSha256(const std::string& password,
                             const std::string& salt,
                             uint32_t iterations,
                             size_t length);

// Comparison whose running time does not depend on where the mismatch is.
// Every tag and verifier comparison must use this, never operator==.
bool ConstantTimeEquals(const std::string& a, const std::string& b);

// SHA-1. Present for exactly one reason: the k-anonymity breach-range APIs
// that exist in the world are defined on SHA-1, and a password manager that
// cannot talk to any of them is not a feature. It must never be used to
// protect anything — that is what the AEAD and the KDF above are for.
std::string Sha1(const std::string& data);

// Lowercase hex, for logs, docs and test vectors only.
std::string ToHex(const std::string& bytes);

}  // namespace crypto
}  // namespace bedrock

#endif  // BEDROCK_CRYPTO_HASH_H_

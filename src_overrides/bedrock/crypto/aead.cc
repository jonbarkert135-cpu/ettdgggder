// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/crypto/aead.h"

#include "bedrock/crypto/hash.h"

namespace bedrock {
namespace crypto {
namespace {

// Two independent keys from one: encryption and authentication must never share
// a key, and the labels keep this construction distinct from every other use of
// the same master key elsewhere in Bedrock.
struct SubKeys {
  std::string encryption;
  std::string mac;
};

SubKeys Derive(const std::string& key, const std::string& nonce) {
  SubKeys keys;
  keys.encryption = HkdfSha256(key, nonce, "bedrock/aead/v1/enc", kKeySize);
  keys.mac = HkdfSha256(key, nonce, "bedrock/aead/v1/mac", kKeySize);
  return keys;
}

std::string Keystream(const std::string& encryption_key, size_t length) {
  std::string stream;
  for (uint64_t counter = 0; stream.size() < length; ++counter) {
    std::string block(8, '\0');
    for (int i = 7; i >= 0; --i)
      block[7 - i] = static_cast<char>((counter >> (i * 8)) & 0xFF);
    stream += HmacSha256(encryption_key, block);
  }
  stream.resize(length);
  return stream;
}

std::string Xor(const std::string& data, const std::string& stream) {
  std::string out = data;
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = static_cast<char>(out[i] ^ stream[i]);
  return out;
}

// The length prefix stops an attacker from moving bytes between the aad and the
// ciphertext while keeping the tag valid.
std::string LengthPrefixed(const std::string& value) {
  std::string out(8, '\0');
  const uint64_t length = value.size();
  for (int i = 7; i >= 0; --i)
    out[7 - i] = static_cast<char>((length >> (i * 8)) & 0xFF);
  return out + value;
}

}  // namespace

std::string Seal(const std::string& key,
                 const std::string& nonce,
                 const std::string& aad,
                 const std::string& plaintext) {
  if (key.size() != kKeySize || nonce.size() != kNonceSize)
    return std::string();  // a caller bug, and it must not produce a blob
  const SubKeys keys = Derive(key, nonce);
  const std::string ciphertext =
      Xor(plaintext, Keystream(keys.encryption, plaintext.size()));
  // Encrypt-then-MAC, over everything that must not change: nonce, aad, text.
  const std::string tag =
      HmacSha256(keys.mac,
                 LengthPrefixed(nonce) + LengthPrefixed(aad) +
                     LengthPrefixed(ciphertext))
          .substr(0, kTagSize);
  return nonce + ciphertext + tag;
}

bool Open(const std::string& key,
          const std::string& aad,
          const std::string& sealed,
          std::string* out) {
  if (key.size() != kKeySize || sealed.size() < kNonceSize + kTagSize)
    return false;
  const std::string nonce = sealed.substr(0, kNonceSize);
  const std::string ciphertext =
      sealed.substr(kNonceSize, sealed.size() - kNonceSize - kTagSize);
  const std::string tag = sealed.substr(sealed.size() - kTagSize);

  const SubKeys keys = Derive(key, nonce);
  const std::string expected =
      HmacSha256(keys.mac,
                 LengthPrefixed(nonce) + LengthPrefixed(aad) +
                     LengthPrefixed(ciphertext))
          .substr(0, kTagSize);
  // Verify before decrypting, and compare in constant time: releasing
  // plaintext from an unauthenticated blob is how padding oracles happen.
  if (!ConstantTimeEquals(tag, expected))
    return false;
  if (out)
    *out = Xor(ciphertext, Keystream(keys.encryption, ciphertext.size()));
  return true;
}

}  // namespace crypto
}  // namespace bedrock

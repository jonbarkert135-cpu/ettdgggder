// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/crypto/hash.h"

#include <array>
#include <cstring>
#include <vector>

// When Bedrock is built inside Chromium, BoringSSL provides all of this and
// this file becomes a thin forwarding layer. The reference implementation below
// exists so the privacy and password logic can be built and tested on a host
// with nothing but a C++ compiler.
#if defined(BEDROCK_USE_BORINGSSL)
#error "BoringSSL backend not wired yet: see docs/design/051-crypto-primitives.md"
#endif

namespace bedrock {
namespace crypto {
namespace {

constexpr size_t kBlockSize = 64;

constexpr uint32_t kRoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

uint32_t RotateLeft(uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

uint32_t RotateRight(uint32_t value, int bits) {
  return (value >> bits) | (value << (32 - bits));
}

}  // namespace

std::string Sha256(const std::string& data) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  std::string message = data;
  const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8;
  message.push_back(static_cast<char>(0x80));
  while (message.size() % kBlockSize != 56)
    message.push_back('\0');
  for (int i = 7; i >= 0; --i)
    message.push_back(static_cast<char>((bit_length >> (i * 8)) & 0xFF));

  for (size_t block = 0; block < message.size(); block += kBlockSize) {
    uint32_t w[64];
    const unsigned char* p =
        reinterpret_cast<const unsigned char*>(message.data()) + block;
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(p[i * 4]) << 24) |
             (static_cast<uint32_t>(p[i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(p[i * 4 + 2]) << 8) |
             static_cast<uint32_t>(p[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const uint32_t s0 =
          RotateRight(w[i - 15], 7) ^ RotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const uint32_t s1 =
          RotateRight(w[i - 2], 17) ^ RotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const uint32_t s1 =
          RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t temp1 = hh + s1 + ch + kRoundConstants[i] + w[i];
      const uint32_t s0 =
          RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  std::string digest(kSha256Size, '\0');
  for (int i = 0; i < 8; ++i) {
    digest[i * 4] = static_cast<char>((h[i] >> 24) & 0xFF);
    digest[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xFF);
    digest[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xFF);
    digest[i * 4 + 3] = static_cast<char>(h[i] & 0xFF);
  }
  return digest;
}

std::string HmacSha256(const std::string& key, const std::string& message) {
  std::string block_key = key.size() > kBlockSize ? Sha256(key) : key;
  block_key.resize(kBlockSize, '\0');

  std::string inner(kBlockSize, '\0');
  std::string outer(kBlockSize, '\0');
  for (size_t i = 0; i < kBlockSize; ++i) {
    inner[i] = static_cast<char>(block_key[i] ^ 0x36);
    outer[i] = static_cast<char>(block_key[i] ^ 0x5c);
  }
  return Sha256(outer + Sha256(inner + message));
}

std::string HkdfSha256(const std::string& secret,
                       const std::string& salt,
                       const std::string& info,
                       size_t length) {
  // RFC 5869: extract, then expand. A zero-length salt is legal and becomes a
  // block of zeros; `info` is what separates one use of the same secret from
  // another, so callers must always pass a distinct, versioned label.
  const std::string prk =
      HmacSha256(salt.empty() ? std::string(kSha256Size, '\0') : salt, secret);

  std::string out;
  std::string previous;
  for (uint8_t counter = 1; out.size() < length; ++counter) {
    if (counter == 0)  // more than 255 blocks requested
      return std::string();
    previous = HmacSha256(prk, previous + info + std::string(1, static_cast<char>(counter)));
    out += previous;
  }
  out.resize(length);
  return out;
}

std::string Pbkdf2HmacSha256(const std::string& password,
                             const std::string& salt,
                             uint32_t iterations,
                             size_t length) {
  if (iterations == 0)
    return std::string();
  std::string out;
  for (uint32_t block = 1; out.size() < length; ++block) {
    std::string index(4, '\0');
    index[0] = static_cast<char>((block >> 24) & 0xFF);
    index[1] = static_cast<char>((block >> 16) & 0xFF);
    index[2] = static_cast<char>((block >> 8) & 0xFF);
    index[3] = static_cast<char>(block & 0xFF);

    std::string u = HmacSha256(password, salt + index);
    std::string result = u;
    for (uint32_t i = 1; i < iterations; ++i) {
      u = HmacSha256(password, u);
      for (size_t j = 0; j < result.size(); ++j)
        result[j] = static_cast<char>(result[j] ^ u[j]);
    }
    out += result;
  }
  out.resize(length);
  return out;
}

bool ConstantTimeEquals(const std::string& a, const std::string& b) {
  // Length is not secret; content is. Once the lengths match, every byte is
  // compared and the result is folded, so the timing carries no information
  // about the position of the first difference.
  if (a.size() != b.size())
    return false;
  unsigned char difference = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    difference |= static_cast<unsigned char>(a[i]) ^
                  static_cast<unsigned char>(b[i]);
  }
  return difference == 0;
}

// Legacy interop only; see hash.h.
std::string Sha1(const std::string& data) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                   0xC3D2E1F0u};
  std::string message = data;
  const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8;
  message.push_back(static_cast<char>(0x80));
  while (message.size() % 64 != 56)
    message.push_back('\0');
  for (int i = 7; i >= 0; --i)
    message.push_back(static_cast<char>((bit_length >> (i * 8)) & 0xFF));

  for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      const unsigned char* p =
          reinterpret_cast<const unsigned char*>(message.data()) + chunk + i * 4;
      w[i] = (static_cast<uint32_t>(p[0]) << 24) |
             (static_cast<uint32_t>(p[1]) << 16) |
             (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }
    for (int i = 16; i < 80; ++i)
      w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t temp = RotateLeft(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = RotateLeft(b, 30);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }

  std::string digest(20, '\0');
  for (int i = 0; i < 5; ++i) {
    digest[i * 4] = static_cast<char>((h[i] >> 24) & 0xFF);
    digest[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xFF);
    digest[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xFF);
    digest[i * 4 + 3] = static_cast<char>(h[i] & 0xFF);
  }
  return digest;
}

std::string ToHex(const std::string& bytes) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (char c : bytes) {
    const unsigned char byte = static_cast<unsigned char>(c);
    out.push_back(kDigits[byte >> 4]);
    out.push_back(kDigits[byte & 0x0F]);
  }
  return out;
}

}  // namespace crypto
}  // namespace bedrock

# 051 — Crypto primitives

**Status: landed and host-tested** (`src_overrides/bedrock/crypto/`). Written to close **F3** and
**F4** of [`../security/AUDIT-2026-08-25.md`](../security/AUDIT-2026-08-25.md), the two findings the
audit called cryptographic and therefore worth fixing *before* anything is wired into the engine: a
port preserves a bad key schedule.

## Why any crypto lives in this repository

The project rule is "crypto belongs to the platform", and it still holds for ciphers. But both
findings were key-derivation bugs, and both fixes need a keyed one-way function in code that must
build and test on a bare host with no Chromium checkout:

- F3: the master password was not a key. Fixing it needs a password KDF and an AEAD.
- F4: seeds came from an invertible unkeyed mixer. Fixing it needs a PRF.

So the module contains **hashes and constructions over them, and no cipher**:

| Function | Standard | Used for |
| --- | --- | --- |
| `Sha256` | FIPS 180-4 | everything below |
| `HmacSha256` | RFC 2104 | PRF for fingerprint seeds, AEAD tag, keystream |
| `HkdfSha256` | RFC 5869 | per-site subkeys, per-purpose subkeys |
| `Pbkdf2HmacSha256` | RFC 8018 | master password stretching |
| `Sha1` | FIPS 180-1 | **legacy interop only**: breach range APIs are defined on it |
| `Seal` / `Open` | HKDF split + HMAC-CTR + encrypt-then-MAC | wrapping the password data key, sealing entries |

**Every one is checked against its published vector** in `hash_test.cc` — including the one-million-
character SHA-256 case and the over-long-key HMAC case. That is the whole argument for allowing this
code in-tree: a subtly wrong implementation cannot pass RFC 4231, RFC 5869 and FIPS 180-4. There is
no "we tested it against ourselves" in this module.

## What replaces it in the real build

Inside Chromium these become BoringSSL: audited, constant-time, hardware-accelerated, and
`Seal`/`Open` become AES-256-GCM. The blob layout (`nonce || ciphertext || tag`), the key size and
the callers do not change. `hash.cc` fails the build loudly if `BEDROCK_USE_BORINGSSL` is defined
before that wiring exists, so the two cannot silently diverge.

Deliberately absent: a block cipher, a platform RNG (the host build has no correct portable CSPRNG
to reach for, and a plausible-looking wrong one is worse than none — `RandomSource` is injected), and
Argon2id (preferred over PBKDF2, but it belongs to the library, not to this file).

## Design rules the module enforces

1. **Domain separation, always.** Every derivation carries a versioned label
   (`bedrock/fp/v1/site`, `bedrock/pw/v1/master-key`, `bedrock/aead/v1/enc`). The same secret used
   for two purposes must not produce the same key, and a sealed blob must not be replayable into
   another context — which is what the AAD is for.
2. **Encryption and authentication never share a key.** `Seal` derives two subkeys from
   (key, nonce).
3. **Verify before decrypting, and compare in constant time.** `Open` checks the tag first and uses
   `ConstantTimeEquals`; the output parameter is untouched on failure.
4. **Nonce reuse is fatal**, so nonces come from the injected `RandomSource` and are stored beside
   the ciphertext, never derived from the message.
5. **Misuse produces nothing, not something weak.** A wrong-size key or nonce makes `Seal` return an
   empty string, and every caller treats that as an error.

## What the callers look like now

Fingerprint seeds (F4) — keyed, one-way, per site and per surface:

```
site_key    = HKDF(session_secret, "bedrock/fp/v1/site", "site:" + eTLD+1)
surface_key = HMAC(site_key, "surface:" + surface_id)
sample(i)   = HMAC(surface_key, "unit:" + counter(i)) -> 53 bits -> [0,1)
```

The session secret is 32 bytes now, not 64 bits. A site that recovers its own surface key learns
nothing about the secret or any other site, and two surfaces' sample streams cannot overlap — the
three properties the old `Mix(Mix(secret ^ FNV1a(site)) + surface)` did not have.

Passwords (F3) — envelope encryption; see [`034-passwords.md`](034-passwords.md).

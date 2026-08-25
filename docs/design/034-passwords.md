# 034 — Passwords

**Roadmap item 34.** Status: landed and host-tested
(`src_overrides/bedrock/passwords/password_store.{h,cc}`).

Password manager · passkeys/WebAuthn · autofill · breach warning · master protection.
**No Bedrock server is involved in any of it.** Nothing syncs, nothing uploads.

## Storage

Local. Every entry is sealed with AEAD under a random 256-bit **data key**, and the data key is
itself wrapped — by the platform keystore (macOS Keychain, Windows DPAPI, libsecret), or by a key
derived from the master password, or by both. The keystore `Cipher` and the random source are
**injected, not implemented here**; the AEAD and the KDF come from
[`bedrock/crypto`](051-crypto-primitives.md), which is BoringSSL inside the Chromium build.

- The store starts **locked**; reads and writes both fail while locked, and the plaintext output
  parameter is left untouched.
- `List()` returns the stored form — ciphertext, site and username. A list view needs no secrets.
- Changing the protection mode **relocks** the store.
- Idle auto-lock defaults to 900 s; `0` means the user turned it off, which is their call.
- `Lock()` wipes the data key. Locked means the key is gone, not that a flag flipped.
- The AAD of each entry binds it to its own row (`origin`, `username`), so swapping two blobs in
  the file — or moving one to another site — fails the tag check.

## The master password is a key, not a gate

Rewritten after **F3** in [`../security/AUDIT-2026-08-25.md`](../security/AUDIT-2026-08-25.md),
which found three defects in the old five-line verifier: the master password protected nothing
(entries were encrypted by the keystore regardless of it), verification compared ciphertexts (so
the first real AES-GCM cipher would have rejected every correct password), and an empty verifier
meant the first password typed silently *became* the master password.

Envelope encryption instead:

```
kek       = HKDF(PBKDF2-HMAC-SHA256(password, salt, 600 000 iterations))
envelope  = keystore(Seal(kek, nonce, "bedrock/pw/v1/master-key", data_key))
unlock    = Open(kek, ..., envelope) -> data_key, or failure
```

- **The AEAD tag is the verifier.** There is nothing stored to compare against: the wrapped key
  either opens or it does not. Constant-time, and no oracle beyond "wrong".
- **No trust-on-first-use.** `Unlock(password)` with no envelope fails; `SetMasterProtection`
  refuses to require a password that has not been set, because that would lock the user out.
- **Changing the password re-wraps the same data key**, so stored entries stay readable and
  nothing has to be re-encrypted.
- **Both layers.** The password envelope is stored inside the keystore blob, so a stolen profile
  directory without the OS session cannot even start guessing offline.
- 600 000 PBKDF2 iterations is the OWASP 2023 floor and the default; tests lower it, production
  may not. Argon2id is better and is what the Chromium build should switch to — the iteration
  count is the one knob, not a parameter framework.

## Autofill — where passwords are actually lost

Every fill goes through `ShouldAutofill()`:

| Situation | Decision |
| --- | --- |
| same origin, https, visible field, one account | `kFill` |
| two accounts saved for the site, no user gesture | `kOfferOnClick` — asking beats guessing |
| form posts to a different origin | `kRefusedCrossOrigin` |
| http page, credential saved for https on that host | `kRefusedInsecure` |
| invisible password field | `kRefusedHiddenField`, gesture or not |

The last two are the ones that matter: the http downgrade is invisible to the user, and there is
no legitimate reason to fill a hidden field.

## Passkeys

Recorded so the UI can show the account; the private key stays in the platform authenticator and
the browser never holds exportable key material. A passkey entry is not offered as a password to
type into a form.

## Breach warning

Off by default. With it off, `BuildBreachQuery()` returns an empty disabled query — there is
literally nothing for the caller to send. With it on, the query is **k-anonymous**: the first five
characters of the SHA-1 of the password go to a **third-party**, user-configurable range endpoint;
the suffix match happens locally. The password, the username and the site never leave the machine,
and Bedrock operates no breach service. SHA-1 is used because that is what the range APIs in the
world are defined on; it protects nothing here, and it now lives in `bedrock/crypto` with its
published test vectors rather than hand-rolled in this component.
Changing a password clears its breach flag.

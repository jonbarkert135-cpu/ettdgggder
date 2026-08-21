# 034 — Passwords

**Roadmap item 34.** Status: landed and host-tested
(`src_overrides/bedrock/passwords/password_store.{h,cc}`).

Password manager · passkeys/WebAuthn · autofill · breach warning · master protection.
**No Bedrock server is involved in any of it.** Nothing syncs, nothing uploads.

## Storage

Local, encrypted by the platform keystore (macOS Keychain, Windows DPAPI, libsecret), optionally
under a master password on top. The `Cipher` interface is **injected, not implemented here**:
crypto belongs to the platform, and a hand-rolled cipher in a browser repository is a liability.
The host test supplies a fake so the *policy* can be tested without inventing one.

- The store starts **locked**; reads and writes both fail while locked, and the plaintext output
  parameter is left untouched.
- `List()` returns the stored form — ciphertext, site and username. A list view needs no secrets.
- Changing the protection mode **relocks** the store.
- Idle auto-lock defaults to 900 s; `0` means the user turned it off, which is their call.
- The master password is never stored, encrypted or not: what is kept is a verifier — a known
  string encrypted under the derived key. (Integration note: the real build derives the key with
  the platform KDF; the store only ever compares verifiers.)

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
and Bedrock operates no breach service. `Sha1Hex()` is verified against published test vectors.
Changing a password clears its breach flag.

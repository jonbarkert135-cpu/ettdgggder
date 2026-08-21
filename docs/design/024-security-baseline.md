# 024 — Security baseline

**Roadmap item 24.** Status: landed and host-tested
(`src_overrides/bedrock/security/security_baseline.{h,cc}`).

## The failure mode this prevents

Privacy work goes wrong in a standard way: a feature is easier to build with some Chromium
security mechanism out of the way, the switch gets flipped "temporarily", and the browser ships
less safe than the Chrome it came from. A privacy browser that is easier to exploit is not a
privacy browser — memory disclosure beats every shim in this repo.

## Enforced, not promised

`SecurityBaseline::Audit(config)` returns violations for a build configuration, and the host test
fails on any violation for the shipping default. Disabling a protection therefore cannot land
quietly: it requires deleting an assertion, which shows up in review.

Required, all of them: process sandbox · site isolation · origin isolation · renderer isolation ·
IPC validation · secure origins model · permission boundaries · memory-safety mitigations ·
V8 sandbox · network service sandbox · GPU sandbox · Spectre mitigations · local malware lists.

Each carries a `WhyItMatters()` sentence, so the security page is generated rather than written
once and left to rot.

Forbidden in a shipping build, as switches **and** as build flags: `--no-sandbox`,
`--disable-site-isolation-trials`, `--disable-web-security`, `--allow-running-insecure-content`,
`--ignore-certificate-errors`, `--disable-gpu-sandbox`, `--disable-seccomp-filter-sandbox`,
`--unsafely-treat-insecure-origin-as-secure`, and the rest of the list in the header. Each has
been used somewhere to make a privacy feature easier. None is worth it.

A genuine platform gap (a feature the OS cannot provide) is declared in `platform_unsupported`
and is not counted as a violation — it is a fact about the platform, not a decision we made. It
still appears on the security page rather than being forgotten.

## Consequences accepted elsewhere in the design

- Fingerprinting shims live in Blink bindings, inside the sandbox, not in a privileged process.
- The content blocker runs in the network service, which already parses hostile input.
- No feature in this repo asks for a relaxed process model to be implementable.

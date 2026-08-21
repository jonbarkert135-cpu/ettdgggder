# 023 — Extension system

**Roadmap item 23.** Status: registry + permission model landed and host-tested
(`src_overrides/bedrock/extensions/extension_registry.{h,cc}`).

Compatibility is the point: Bedrock keeps the Chromium extension API surface, so the extensions
people already rely on keep working, and install / remove / disable / update / configure work the
way users expect. What Bedrock changes is **disclosure**.

## Disclosure is computed, not written

Chromium's prompt lists capabilities. It does not tell you that an extension also runs a
background page that talks to a server whenever the browser is open. `Analyze()` derives a
`Disclosure` from the manifest — capabilities, host patterns, storage, network, background
activity — plus one **headline warning** in plain language about the worst thing the extension
can do, e.g. *"This extension can read the data of every website you visit."* (roadmap item 23's
exact requirement). The author never writes any of it.

Unknown permissions are kept and shown verbatim. A permission we do not recognise is precisely
the one the user most needs to see; dropping it would be the worst possible default.

Risk ladder: `Limited → Moderate → Broad → Full control`. `debugger`, `nativeMessaging` and
`proxy` outrank all-site access, because they reach outside the page: the debugger API can read
any tab including the one you sign in with, native messaging talks to a program on the device,
and proxy control decides where every byte goes.

## Updates cannot grow powers quietly

`Update()` compares the new disclosure with the installed one. If it asks for *anything* more —
a new capability, a new host pattern, a higher risk level — the update is **staged, not applied**:
the extension is disabled with `pending_review`, the old version stays in place, and it cannot be
re-enabled until the user approves. Auto-update is the natural way for a benign extension to
become a malicious one after it is sold, and this closes it.

## Configuration the manifest does not control

- **Host narrowing**: the user can restrict an extension to fewer sites than the manifest asked
  for, and the narrowing always wins.
- **Private windows**: off by default. Someone else's code is the last thing that should run in
  the window opened specifically to leave no trace.
- **Storage**: each extension gets its own area inside the profile
  (`profiles/<profile>/extensions/<id>`), never shared with web content or another extension.
- **No silent installs**: `Install()` refuses without `user_confirmed`. Not "installed disabled
  for later" — not installed.

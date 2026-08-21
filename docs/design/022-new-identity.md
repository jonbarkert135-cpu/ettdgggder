# 022 — New Identity

**Roadmap item 22.** Status: landed and host-tested
(`src_overrides/bedrock/profiles/new_identity.{h,cc}`).

## Shown before, not after

The confirmation dialog is built from `PlanForNewIdentity()`, so the user reads **exactly** what
will happen before it happens — two lists, always together:

**Cleared:** cookies · site storage · cache · Service Workers · temporary permissions · form
data · tabs and session history · history entries from this session · network state (sockets,
DNS cache, reused TLS sessions) · Tor circuits · media device salts · **the fingerprint seed**.

**Kept:** bookmarks · settings · saved passwords · installed extensions · files already
downloaded.

The fingerprint seed matters and is easy to forget: without a new seed, the canvas and audio
values a site reads after the reset match the ones from before it, and the "new identity" is
recognisably the old one.

Tabs close. A surviving tab keeps its JavaScript state and its open connections, which defeats
the reset it was supposed to be part of.

## It reports what it could not do

`Execute()` takes the list of targets the platform cannot honour and puts them in
`failed`, never in `cleared`. A shared OS-level DNS cache we cannot flush is reported as not
flushed. Circuits still rotate even when storage clearing is partial, because that part always
works and is worth having.

## It does not overpromise

> This clears what this browser stored on this device and starts new network connections. It
> cannot undo what already happened: sites you signed in to still know it was you, and your
> network provider still saw the connections you made.

That sentence ships next to the lists, and the same honesty test that guards the mode strings
(no *anonymous*, no *100%*, no *untraceable*) covers every string here too.

# 021 — Profiles

**Roadmap item 21.** Status: landed and host-tested
(`src_overrides/bedrock/session/profile_manager.{h,cc}`).

Personal, Work, School, Temporary, Custom. A profile owns **everything**: cookies, storage,
history, bookmarks, extensions, permissions, privacy settings, downloads and passwords.

`SharesData(a, b, type)` returns true only when `a` and `b` are the same profile — for every
type, with no exception, and a test walks the enum to prove it. Bookmarks and passwords are
included on purpose: "let's just share the bookmarks" is exactly how a work profile leaks into a
personal one, and the user has no way to notice.

Each data type has its own path under the profile's root, and a test asserts no two collide.

**Temporary profiles never touch disk.** `PathFor()` returns a `memory://` path for them, so a
code path that writes a file has to opt out of the profile API to do it. A "temporary" profile
that leaves a directory behind is the failure this returns-no-path design prevents.

The last profile cannot be deleted, and deleting the active one moves the user to a real profile
rather than to no profile at all.

// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UPDATER_RELEASE_CHANNELS_H_
#define BEDROCK_UPDATER_RELEASE_CHANNELS_H_

#include <string>
#include <vector>

#include "bedrock/updater/update_provider.h"

// Release channels and what a release must state (roadmap item 71).
//
// Three channels — nightly, beta, stable — are easy to declare and hard to
// keep honest. The failure is always the same shape: "beta" quietly becomes
// the branch where a change waits until someone remembers it, "nightly" is
// built when a maintainer has time, and by the time a user asks what is
// different between the build they run and the one they downloaded, nobody can
// answer. A privacy browser cannot afford that answer to be missing: the
// difference between two builds is exactly the set of protections the user does
// or does not have.
//
// So this module states, in code:
//
//   - what each channel is for, how often it is built, and whether it may
//     carry changes that have not been through the pipeline;
//   - what promotion between channels requires — a soak time and a clean bill,
//     not a feeling that it is probably fine;
//   - what every release must publish, from item 71: version number, the
//     Chromium base it is built on, security fixes, privacy changes,
//     dependencies and known issues. All six, every time, including nightly.
//
// "Known issues" is the field that gets dropped first and matters most. A
// release note without it reads like a build with no known issues, which is
// never true and teaches users to distrust the whole document.
//
// Nothing here builds or publishes anything; it validates descriptions of
// releases, so CI, a release script and a test all reach the same verdict.

namespace bedrock {
namespace update {

// The six things every release states (item 71). Order is the publication
// order in docs/RELEASES.md.
enum class NoteField {
  kVersion,
  kChromiumBase,
  kSecurityFixes,
  kPrivacyChanges,
  kDependencies,
  kKnownIssues,
};

const char* NoteFieldName(NoteField field);

// Every field, in publication order.
std::vector<NoteField> RequiredNoteFields();

struct ChannelProperties {
  // Nominal build cadence in days. Nightly is 1; stable is driven by upstream
  // Chromium's roughly four-week cycle, not by a feature calendar.
  int cadence_days = 0;
  // Version suffix, e.g. "-nightly.20260821". Empty for stable.
  std::string version_suffix_prefix;
  // May a change reach this channel without having landed in the channel above
  // it in the funnel? Only nightly may.
  bool accepts_direct_landings = false;
  // Minimum days a build soaks here before it may be promoted.
  int soak_days = 0;
  // Automatic update checks on by default for users of this channel.
  bool automatic_updates_default = true;
};

ChannelProperties PropertiesFor(Channel channel);

const char* ChannelName(Channel channel);

// A release as described by whoever is proposing to publish it.
struct ReleaseDescription {
  Channel channel = Channel::kNightly;
  std::string version;            // "1.2.0.4412"
  std::string chromium_version;   // must equal build/chromium.pin
  // Which of the six fields the notes actually contain. A field listed here is
  // present *and* written; an empty section does not count and the release
  // tooling is expected to say so rather than pass an empty string along.
  std::vector<NoteField> notes_present;
  // Days this exact build has been available in the channel below stable.
  int days_in_previous_channel = 0;
  // Open issues the team classified as blocking for this channel.
  int blocking_issues = 0;
  // Has the pipeline from docs/UPSTREAM_SYNC.md been run for this build?
  bool pipeline_complete = false;
  // Is the artifact signed with a release key and its provenance published
  // (item 70)? Unsigned builds may exist locally; they are never a release.
  bool signed_and_attested = false;
};

enum class ReleaseVerdict {
  kPublishable,
  kMissingNotes,          // one of the six fields is absent
  kChromiumMismatch,      // built against something other than the pin
  kInsufficientSoak,      // promoted faster than the channel allows
  kBlockingIssues,
  kPipelineIncomplete,
  kUnsigned,
};

const char* VerdictName(ReleaseVerdict verdict);

// Fields the description is missing, in publication order. Empty when complete.
std::vector<NoteField> MissingNoteFields(const ReleaseDescription& release);

// May this build be published on its channel? Checks are ordered so the first
// failure reported is the one a human should fix first.
ReleaseVerdict Evaluate(const ReleaseDescription& release,
                        const std::string& pinned_chromium_version);

// May a build on `from` be promoted to `to`? Promotion is one step at a time —
// nightly to beta to stable — because the point of the funnel is the soak, and
// a build that skips a channel has not soaked anywhere.
bool IsPromotionAllowed(Channel from, Channel to);

}  // namespace update
}  // namespace bedrock

#endif  // BEDROCK_UPDATER_RELEASE_CHANNELS_H_

// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/updater/release_channels.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bedrock {
namespace update {

const char* NoteFieldName(NoteField field) {
  switch (field) {
    case NoteField::kVersion:
      return "version number";
    case NoteField::kChromiumBase:
      return "Chromium base version";
    case NoteField::kSecurityFixes:
      return "security fixes";
    case NoteField::kPrivacyChanges:
      return "privacy changes";
    case NoteField::kDependencies:
      return "dependencies";
    case NoteField::kKnownIssues:
      return "known issues";
  }
  return "unknown field";
}

std::vector<NoteField> RequiredNoteFields() {
  return {NoteField::kVersion,        NoteField::kChromiumBase,
          NoteField::kSecurityFixes,  NoteField::kPrivacyChanges,
          NoteField::kDependencies,   NoteField::kKnownIssues};
}

ChannelProperties PropertiesFor(Channel channel) {
  ChannelProperties properties;
  switch (channel) {
    case Channel::kNightly:
      properties.cadence_days = 1;
      properties.version_suffix_prefix = "-nightly.";
      // The only channel where a change may appear first: that is what it is
      // for. Users are told, in the download page and in the About box, that
      // a nightly is a build under test.
      properties.accepts_direct_landings = true;
      properties.soak_days = 7;
      properties.automatic_updates_default = true;
      break;
    case Channel::kBeta:
      properties.cadence_days = 7;
      properties.version_suffix_prefix = "-beta.";
      properties.accepts_direct_landings = false;
      // Two weeks in beta is roughly one upstream security cycle: long enough
      // that a privacy regression has a chance to be noticed by someone other
      // than the person who wrote it.
      properties.soak_days = 14;
      properties.automatic_updates_default = true;
      break;
    case Channel::kStable:
      // Follows upstream Chromium's cycle rather than a feature calendar; a
      // security roll can and does publish sooner (docs/UPSTREAM_SYNC.md).
      properties.cadence_days = 28;
      properties.version_suffix_prefix = "";
      properties.accepts_direct_landings = false;
      properties.soak_days = 0;
      properties.automatic_updates_default = true;
      break;
  }
  return properties;
}

const char* ChannelName(Channel channel) {
  switch (channel) {
    case Channel::kNightly:
      return "nightly";
    case Channel::kBeta:
      return "beta";
    case Channel::kStable:
      return "stable";
  }
  return "unknown channel";
}

const char* VerdictName(ReleaseVerdict verdict) {
  switch (verdict) {
    case ReleaseVerdict::kPublishable:
      return "publishable";
    case ReleaseVerdict::kMissingNotes:
      return "release notes incomplete";
    case ReleaseVerdict::kChromiumMismatch:
      return "not built against the pinned Chromium";
    case ReleaseVerdict::kInsufficientSoak:
      return "promoted before the soak period elapsed";
    case ReleaseVerdict::kBlockingIssues:
      return "blocking issues are open";
    case ReleaseVerdict::kPipelineIncomplete:
      return "release pipeline not complete";
    case ReleaseVerdict::kUnsigned:
      return "artifact is not signed and attested";
  }
  return "unknown verdict";
}

std::vector<NoteField> MissingNoteFields(const ReleaseDescription& release) {
  std::vector<NoteField> missing;
  for (NoteField field : RequiredNoteFields()) {
    const auto& present = release.notes_present;
    if (std::find(present.begin(), present.end(), field) == present.end())
      missing.push_back(field);
  }
  return missing;
}

ReleaseVerdict Evaluate(const ReleaseDescription& release,
                        const std::string& pinned_chromium_version) {
  // Order matters: report the thing to fix first, not the alphabetically first
  // thing that is wrong.
  if (release.chromium_version != pinned_chromium_version)
    return ReleaseVerdict::kChromiumMismatch;

  // Nightly is built from a branch under test, so blocking issues and an
  // incomplete pipeline are expected there; they are exactly what a nightly
  // exists to surface. Everything else is not negotiable on any channel.
  if (release.channel != Channel::kNightly) {
    if (release.blocking_issues > 0)
      return ReleaseVerdict::kBlockingIssues;
    if (!release.pipeline_complete)
      return ReleaseVerdict::kPipelineIncomplete;
    const ChannelProperties previous =
        PropertiesFor(release.channel == Channel::kStable ? Channel::kBeta
                                                          : Channel::kNightly);
    if (release.days_in_previous_channel < previous.soak_days)
      return ReleaseVerdict::kInsufficientSoak;
  }

  // A build users can download is signed, whatever the channel. An unsigned
  // nightly is not a faster nightly, it is an unverifiable one.
  if (!release.signed_and_attested)
    return ReleaseVerdict::kUnsigned;

  if (!MissingNoteFields(release).empty())
    return ReleaseVerdict::kMissingNotes;

  return ReleaseVerdict::kPublishable;
}

bool IsPromotionAllowed(Channel from, Channel to) {
  return (from == Channel::kNightly && to == Channel::kBeta) ||
         (from == Channel::kBeta && to == Channel::kStable);
}

}  // namespace update
}  // namespace bedrock

// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_PROFILE_MENU_H_
#define BEDROCK_UI_PROFILE_MENU_H_

#include <string>

#include "bedrock/profiles/profile_manager.h"
#include "bedrock/themes/theme_css.h"

// The profile selector (design item 26).
//
// Small on purpose: the active profile, the other profiles, the window mode,
// and two actions. No avatars to manage, no marketing for an account, and no
// sync badge that lies.
//
// The sync line is the one that matters. Bedrock has no sync service, so this
// menu says "Not synced — this profile stays on this device" instead of
// leaving a hopeful blank where other browsers put a sign-in prompt. If sync
// is ever added as an optional, self-hosted feature (item 95), this string is
// where its real state appears — never a badge that implies a server we do not
// have.

namespace bedrock {
namespace ui {

// The menu as JSON for the popup that draws it.
std::string ProfileMenuJson(const session::ProfileManager& profiles,
                            WindowMode mode);

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_PROFILE_MENU_H_

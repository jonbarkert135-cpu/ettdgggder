// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_ACCESSIBILITY_H_
#define BEDROCK_UI_ACCESSIBILITY_H_

#include <string>
#include <vector>

// Accessibility (roadmap item 60).
//
// Chromium already ships the hard part: the platform accessibility trees, the
// screen-reader bridges, focus management, caret browsing. A Chromium overlay
// that reimplements any of that would make things worse. What an overlay *can*
// get wrong — and what almost every browser fork does get wrong — is its own
// surfaces: a privacy panel of unlabelled icons, a settings row no keyboard can
// reach, a dialog that does not trap focus, an animation that ignores the
// user's "reduce motion".
//
// So this file is a conformance surface, in the same shape as the feature
// registry of item 55: every requirement names where it is met and how far it
// has got. `Status::kEnforced` is only allowed once the mechanism exists in
// code *here*; requirements that live in Chromium are marked kInherited and say
// what we must not break. Nothing claims conformance the project cannot show.
//
// The rule underneath all of it: **if a control can be operated, it can be
// operated from the keyboard and it has a name a screen reader can read.** An
// icon is not a name.

namespace bedrock {
namespace ui {

enum class A11yRequirement {
  kKeyboardNavigation,
  kScreenReaders,
  kHighContrast,
  kReducedMotion,
  kScalableUi,
  kVisibleFocus,
  kAccessibleLabels,
  kDialogSemantics,
  kMaxValue = kDialogSemantics,
};

enum class A11yStatus {
  kInherited,  // Chromium provides it; our job is not to break it
  kEnforced,   // a mechanism in this repository implements or checks it
  kPlanned,    // designed, not yet implemented — never shown as done
};

struct A11yRule {
  A11yRequirement requirement;
  const char* name;
  const char* rule;      // what Bedrock's own surfaces must do
  const char* evidence;  // file or gate that shows it
  A11yStatus status;
};

// A control in one of Bedrock's own surfaces, as the accessibility tree sees
// it. Built from the surfaces themselves so a new control cannot be added
// without a name.
struct AccessibleControl {
  std::string surface;   // "Reset", "Advanced settings", "Sidebar"
  std::string name;      // the accessible name; never an icon, never empty
  std::string role;      // "button", "dialog", "tab", "switch"
  bool keyboard_reachable = true;
};

class Accessibility {
 public:
  static const std::vector<A11yRule>& Rules();
  static const A11yRule& Get(A11yRequirement requirement);

  // Every control Bedrock adds to the browser, with its accessible name. The
  // test walks this list; the settings and reset surfaces feed it, so a control
  // added there without a name fails the build rather than shipping mute.
  static std::vector<AccessibleControl> Controls();

  // Dialog semantics: what a Bedrock dialog must declare. Returned as data so
  // the reset and permission dialogs can be checked against it.
  struct DialogContract {
    const char* role;              // "dialog" / "alertdialog"
    bool focus_trapped;            // Tab cycles inside, not behind
    bool escape_closes;            // and Escape means cancel, never confirm
    bool initial_focus_is_safe;    // focus lands on the harmless button
    const char* labelled_by;       // the dialog's own title, not a generic one
  };
  static DialogContract ContractFor(bool destructive);

  // UI scale range Bedrock's own surfaces must survive without clipping.
  static constexpr int kMinUiScalePercent = 50;
  static constexpr int kMaxUiScalePercent = 300;

  static std::vector<std::string> AllUserVisibleStrings();
};

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_ACCESSIBILITY_H_

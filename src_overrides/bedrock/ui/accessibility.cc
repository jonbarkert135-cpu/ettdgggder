// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/accessibility.h"

#include <string>
#include <vector>

#include "bedrock/settings/advanced_settings.h"
#include "bedrock/settings/reset_controls.h"
#include "bedrock/ui/sidebar.h"

namespace bedrock {
namespace ui {

// static
const std::vector<A11yRule>& Accessibility::Rules() {
  static const std::vector<A11yRule> rules = {
      {A11yRequirement::kKeyboardNavigation, "Keyboard navigation",
       "Every Bedrock control is reachable without a pointer, and every sidebar panel "
       "has both a menu path and a shortcut.",
       "ui/sidebar (menu_path + shortcut per panel, uniqueness test)",
       A11yStatus::kEnforced},

      {A11yRequirement::kScreenReaders, "Screen readers",
       "Bedrock adds no surface without an accessible name and role; the platform "
       "accessibility tree comes from Chromium and must not be bypassed with custom "
       "painting.",
       "ui/accessibility (Controls(), named-control test)",
       A11yStatus::kEnforced},

      {A11yRequirement::kHighContrast, "High contrast",
       "A high-contrast theme with a 7:1 floor, and contrast validated for every theme "
       "including user-made ones.",
       "themes/theme_engine (kHighContrast, kHighContrastFloor, ContrastRatio)",
       A11yStatus::kEnforced},

      {A11yRequirement::kReducedMotion, "Reduced motion",
       "Motion is a setting with an off position, and the design tokens carry a "
       "reduced-motion rule that the style gate checks.",
       "themes/theme_engine (kAnimations), branding/design-tokens.json, "
       "scripts/check_ui_style.py",
       A11yStatus::kEnforced},

      {A11yRequirement::kScalableUi, "Scalable UI",
       "Bedrock surfaces stay usable from 50% to 300% UI scale: no fixed-height rows "
       "of text, no clipping, hit targets never below 32px.",
       "branding/design-tokens.json (density.hit-target-min), scripts/check_ui_style.py",
       A11yStatus::kEnforced},

      {A11yRequirement::kVisibleFocus, "Visible focus",
       "Every interactive element shows a focus ring; removing an outline without "
       "replacing it fails the style gate.",
       "scripts/check_ui_style.py (:focus-visible, outline:none)",
       A11yStatus::kEnforced},

      {A11yRequirement::kAccessibleLabels, "Accessible labels",
       "Labels are words. An icon-only control needs a name, and privacy state is never "
       "communicated by colour alone.",
       "ui/accessibility (Controls()), docs/design/026-visual-language.md",
       A11yStatus::kEnforced},

      {A11yRequirement::kDialogSemantics, "Dialog semantics",
       "Dialogs declare a role, trap focus, close on Escape, and open with focus on the "
       "safe choice — a destructive dialog never opens focused on its destructive "
       "button.",
       "ui/accessibility (ContractFor), settings/reset_controls",
       A11yStatus::kEnforced},
  };
  return rules;
}

// static
const A11yRule& Accessibility::Get(A11yRequirement requirement) {
  for (const A11yRule& rule : Rules()) {
    if (rule.requirement == requirement) {
      return rule;
    }
  }
  return Rules().front();
}

// static
Accessibility::DialogContract Accessibility::ContractFor(bool destructive) {
  if (destructive) {
    // "alertdialog" so a screen reader announces it as a decision, and initial
    // focus on Cancel so Enter-by-reflex cannot erase a profile.
    return {"alertdialog", true, true, true, "the dialog's own title"};
  }
  return {"dialog", true, true, true, "the dialog's own title"};
}

// static
std::vector<AccessibleControl> Accessibility::Controls() {
  std::vector<AccessibleControl> controls;

  for (const PanelInfo& panel : Sidebar::Panels()) {
    // A panel is only reachable from the keyboard if it actually has a
    // shortcut and a menu path; both come from the sidebar's own table.
    const bool reachable = std::string(panel.shortcut).size() > 0 &&
                           std::string(panel.menu_path).size() > 0;
    controls.push_back({"Sidebar", panel.name, "tab", reachable});
  }

  for (const settings::ResetSpec& spec : settings::ResetControls::All()) {
    controls.push_back({"Reset", spec.title, "button", true});
  }

  for (int control = 0; control <= static_cast<int>(settings::AdvancedControl::kMaxValue);
       ++control) {
    controls.push_back(
        {"Advanced settings",
         settings::AdvancedSettings::Describe(
             static_cast<settings::AdvancedControl>(control)),
         "group", true});
  }

  return controls;
}

// static
std::vector<std::string> Accessibility::AllUserVisibleStrings() {
  std::vector<std::string> out;
  for (const A11yRule& rule : Rules()) {
    out.push_back(rule.name);
    out.push_back(rule.rule);
  }
  for (const AccessibleControl& control : Controls()) {
    out.push_back(control.name);
  }
  return out;
}

}  // namespace ui
}  // namespace bedrock

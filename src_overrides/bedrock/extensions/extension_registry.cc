// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/extensions/extension_registry.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace extensions {
namespace {

bool Has(const std::vector<Capability>& capabilities, Capability capability) {
  return std::find(capabilities.begin(), capabilities.end(), capability) !=
         capabilities.end();
}

bool IsAllSitesPattern(const std::string& pattern) {
  return pattern == "<all_urls>" || pattern == "*://*/*" ||
         pattern == "http://*/*" || pattern == "https://*/*" ||
         pattern == "*://*/" || pattern == "*";
}

}  // namespace

ExtensionRegistry::ExtensionRegistry(const std::string& profile_id)
    : profile_id_(profile_id) {}

ExtensionRegistry::~ExtensionRegistry() = default;

// static
Disclosure ExtensionRegistry::Analyze(
    const std::vector<std::string>& manifest_permissions,
    const std::vector<std::string>& host_permissions,
    bool has_background,
    std::vector<std::string>* unknown) {
  Disclosure disclosure;
  disclosure.host_patterns = host_permissions;
  disclosure.background_activity = has_background;
  if (has_background) {
    disclosure.capabilities.push_back(Capability::kBackgroundPage);
  }

  bool all_sites = false;
  for (const std::string& pattern : host_permissions) {
    all_sites = all_sites || IsAllSitesPattern(pattern);
  }

  for (const std::string& permission : manifest_permissions) {
    if (permission == "tabs" || permission == "history") {
      disclosure.capabilities.push_back(Capability::kReadBrowsingHistory);
    } else if (permission == "cookies") {
      disclosure.capabilities.push_back(Capability::kReadCookies);
    } else if (permission == "downloads") {
      disclosure.capabilities.push_back(Capability::kManageDownloads);
    } else if (permission == "clipboardRead") {
      disclosure.capabilities.push_back(Capability::kReadClipboard);
    } else if (permission == "nativeMessaging") {
      disclosure.capabilities.push_back(Capability::kNativeMessaging);
    } else if (permission == "proxy") {
      disclosure.capabilities.push_back(Capability::kProxyControl);
    } else if (permission == "webRequestBlocking" ||
               permission == "webRequest") {
      disclosure.capabilities.push_back(Capability::kWebRequestBlocking);
      disclosure.network_access = true;
    } else if (permission == "declarativeNetRequest") {
      disclosure.capabilities.push_back(Capability::kDeclarativeNetRequest);
    } else if (permission == "unlimitedStorage") {
      disclosure.capabilities.push_back(Capability::kStorageUnlimited);
      disclosure.persistent_storage = true;
    } else if (permission == "storage") {
      disclosure.persistent_storage = true;
    } else if (permission == "debugger") {
      disclosure.capabilities.push_back(Capability::kDebugger);
    } else if (permission == "activeTab") {
      disclosure.capabilities.push_back(Capability::kActiveTabOnly);
    } else if (IsAllSitesPattern(permission)) {
      all_sites = true;
    } else if (permission.find("://") != std::string::npos) {
      disclosure.host_patterns.push_back(permission);
    } else if (unknown) {
      // Never dropped: a permission we do not recognise is exactly the one the
      // user most needs to see.
      unknown->push_back(permission);
    }
  }

  if (all_sites) {
    disclosure.capabilities.push_back(Capability::kAllSiteAccess);
    disclosure.network_access = true;
  } else if (!disclosure.host_patterns.empty()) {
    disclosure.capabilities.push_back(Capability::kSpecificSiteAccess);
    disclosure.network_access = true;
  }

  // The headline. One sentence, in the user's language, about the worst thing
  // this extension can do — not a list to scroll past.
  if (Has(disclosure.capabilities, Capability::kDebugger)) {
    disclosure.headline_warning =
        "This extension can inspect and control every page you open, "
        "including pages where you sign in.";
  } else if (Has(disclosure.capabilities, Capability::kNativeMessaging)) {
    disclosure.headline_warning =
        "This extension can exchange data with a program installed on your "
        "computer, outside the browser.";
  } else if (Has(disclosure.capabilities, Capability::kProxyControl)) {
    disclosure.headline_warning =
        "This extension can route all your traffic through a server of its "
        "choice.";
  } else if (all_sites) {
    disclosure.headline_warning =
        "This extension can read the data of every website you visit.";
  } else if (Has(disclosure.capabilities, Capability::kReadCookies)) {
    disclosure.headline_warning =
        "This extension can read the cookies that keep you signed in.";
  }
  return disclosure;
}

// static
RiskLevel ExtensionRegistry::Risk(const Disclosure& disclosure) {
  if (Has(disclosure.capabilities, Capability::kDebugger) ||
      Has(disclosure.capabilities, Capability::kNativeMessaging) ||
      Has(disclosure.capabilities, Capability::kProxyControl)) {
    return RiskLevel::kCritical;
  }
  if (Has(disclosure.capabilities, Capability::kAllSiteAccess) ||
      Has(disclosure.capabilities, Capability::kReadCookies) ||
      Has(disclosure.capabilities, Capability::kReadBrowsingHistory)) {
    return RiskLevel::kHigh;
  }
  if (Has(disclosure.capabilities, Capability::kSpecificSiteAccess) ||
      disclosure.persistent_storage || disclosure.background_activity) {
    return RiskLevel::kModerate;
  }
  return RiskLevel::kLow;
}

// static
const char* ExtensionRegistry::CapabilityName(Capability capability) {
  switch (capability) {
    case Capability::kAllSiteAccess:      return "All websites";
    case Capability::kSpecificSiteAccess: return "Specific websites";
    case Capability::kActiveTabOnly:      return "The tab you are using";
    case Capability::kReadBrowsingHistory:return "Browsing history";
    case Capability::kReadCookies:        return "Cookies";
    case Capability::kManageDownloads:    return "Downloads";
    case Capability::kReadClipboard:      return "Clipboard";
    case Capability::kNativeMessaging:    return "Programs on your computer";
    case Capability::kBackgroundPage:     return "Background activity";
    case Capability::kProxyControl:       return "Network routing";
    case Capability::kWebRequestBlocking: return "Network requests";
    case Capability::kDeclarativeNetRequest: return "Request filtering rules";
    case Capability::kStorageUnlimited:   return "Unlimited storage";
    case Capability::kDebugger:           return "Full page control";
  }
  return "";
}

// static
const char* ExtensionRegistry::CapabilityExplanation(Capability capability) {
  switch (capability) {
    case Capability::kAllSiteAccess:
      return "Reads and can change the content of every page you open, "
             "including your email and your bank.";
    case Capability::kSpecificSiteAccess:
      return "Reads and can change the pages of the listed sites only.";
    case Capability::kActiveTabOnly:
      return "Sees a page only when you click the extension on it.";
    case Capability::kReadBrowsingHistory:
      return "Sees which pages you open and when.";
    case Capability::kReadCookies:
      return "Reads the cookies that keep you signed in to sites.";
    case Capability::kManageDownloads:
      return "Sees and can start downloads.";
    case Capability::kReadClipboard:
      return "Reads what you copied, including passwords copied from a "
             "password manager.";
    case Capability::kNativeMessaging:
      return "Sends and receives data from a program installed on your "
             "computer.";
    case Capability::kBackgroundPage:
      return "Keeps running while the browser is open, even with no tab of "
             "its own.";
    case Capability::kProxyControl:
      return "Decides which server your traffic goes through.";
    case Capability::kWebRequestBlocking:
      return "Sees every request the browser makes and can block or change it.";
    case Capability::kDeclarativeNetRequest:
      return "Blocks or changes requests using rules the browser applies for "
             "it, without seeing the requests itself.";
    case Capability::kStorageUnlimited:
      return "Stores an unlimited amount of data on your device.";
    case Capability::kDebugger:
      return "Attaches to pages the way developer tools do: it can read and "
             "control anything on them.";
  }
  return "";
}

// static
const char* ExtensionRegistry::RiskName(RiskLevel risk) {
  switch (risk) {
    case RiskLevel::kLow:      return "Limited access";
    case RiskLevel::kModerate: return "Moderate access";
    case RiskLevel::kHigh:     return "Broad access";
    case RiskLevel::kCritical: return "Full control";
  }
  return "";
}

Extension* ExtensionRegistry::Install(const std::string& id,
                                      const std::string& name,
                                      const std::string& version,
                                      const Disclosure& disclosure,
                                      bool user_confirmed) {
  if (id.empty() || extensions_.count(id) != 0 || !user_confirmed) {
    // No silent installs. An extension the user did not confirm is not
    // installed disabled "for later" — it is not installed.
    return nullptr;
  }
  Entry entry;
  entry.extension.id = id;
  entry.extension.name = name;
  entry.extension.version = version;
  entry.extension.disclosure = disclosure;
  entry.extension.storage_path =
      "profiles/" + profile_id_ + "/extensions/" + id;
  auto [it, inserted] = extensions_.emplace(id, std::move(entry));
  (void)inserted;
  return &it->second.extension;
}

bool ExtensionRegistry::Remove(const std::string& id) {
  return extensions_.erase(id) != 0;
}

bool ExtensionRegistry::SetEnabled(const std::string& id, bool enabled) {
  auto it = extensions_.find(id);
  if (it == extensions_.end()) {
    return false;
  }
  if (it->second.pending_review && enabled) {
    return false;  // review the new permissions first
  }
  it->second.extension.state = enabled ? State::kEnabled : State::kDisabled;
  return true;
}

ExtensionRegistry::UpdateResult ExtensionRegistry::Update(
    const std::string& id,
    const std::string& version,
    const Disclosure& disclosure) {
  auto it = extensions_.find(id);
  if (it == extensions_.end()) {
    return UpdateResult::kNotFound;
  }
  const Disclosure& current = it->second.extension.disclosure;
  bool grows = Risk(disclosure) > Risk(current);
  for (Capability capability : disclosure.capabilities) {
    grows = grows || !Has(current.capabilities, capability);
  }
  for (const std::string& pattern : disclosure.host_patterns) {
    grows = grows ||
            std::find(current.host_patterns.begin(),
                      current.host_patterns.end(),
                      pattern) == current.host_patterns.end();
  }
  if (grows) {
    it->second.pending = disclosure;
    it->second.pending_version = version;
    it->second.pending_review = true;
    it->second.extension.state = State::kDisabledByPolicy;
    return UpdateResult::kNeedsReview;
  }
  it->second.extension.version = version;
  it->second.extension.disclosure = disclosure;
  return UpdateResult::kUpdated;
}

bool ExtensionRegistry::HasPendingReview(const std::string& id) const {
  auto it = extensions_.find(id);
  return it != extensions_.end() && it->second.pending_review;
}

bool ExtensionRegistry::ApprovePendingReview(const std::string& id) {
  auto it = extensions_.find(id);
  if (it == extensions_.end() || !it->second.pending_review) {
    return false;
  }
  it->second.extension.disclosure = it->second.pending;
  it->second.extension.version = it->second.pending_version;
  it->second.pending_review = false;
  it->second.extension.state = State::kEnabled;
  return true;
}

void ExtensionRegistry::SetHostAccess(
    const std::string& id,
    const std::vector<std::string>& allowed_hosts) {
  auto it = extensions_.find(id);
  if (it != extensions_.end()) {
    it->second.host_overrides = allowed_hosts;
  }
}

std::vector<std::string> ExtensionRegistry::HostAccess(
    const std::string& id) const {
  auto it = extensions_.find(id);
  if (it == extensions_.end()) {
    return {};
  }
  // The user's narrowing always wins over the manifest.
  return it->second.host_overrides.empty()
             ? it->second.extension.disclosure.host_patterns
             : it->second.host_overrides;
}

void ExtensionRegistry::SetAllowedInPrivateWindows(const std::string& id,
                                                   bool allowed) {
  auto it = extensions_.find(id);
  if (it != extensions_.end()) {
    it->second.private_windows = allowed;
  }
}

bool ExtensionRegistry::AllowedInPrivateWindows(const std::string& id) const {
  auto it = extensions_.find(id);
  return it != extensions_.end() && it->second.private_windows;
}

Extension* ExtensionRegistry::Find(const std::string& id) {
  auto it = extensions_.find(id);
  return it == extensions_.end() ? nullptr : &it->second.extension;
}

const Extension* ExtensionRegistry::Find(const std::string& id) const {
  auto it = extensions_.find(id);
  return it == extensions_.end() ? nullptr : &it->second.extension;
}

std::vector<std::string> ExtensionRegistry::Ids() const {
  std::vector<std::string> ids;
  for (const auto& [id, entry] : extensions_) {
    (void)entry;
    ids.push_back(id);
  }
  return ids;
}

}  // namespace extensions
}  // namespace bedrock

// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_EXTENSIONS_EXTENSION_REGISTRY_H_
#define BEDROCK_EXTENSIONS_EXTENSION_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

// Extension system (roadmap item 23).
//
// Compatibility is the point: Bedrock keeps the Chromium extension API surface
// so the extensions people already use keep working. What Bedrock changes is
// *disclosure*. Chromium's install prompt is a list of capabilities; a user
// cannot tell from "Read and change all your data on all websites" that the
// extension also runs a background page that talks to a server whenever the
// browser is open.
//
// So every extension carries a Disclosure: what it may read, where, what it
// stores, who it talks to, and what it does when nobody is looking. It is
// computed from the manifest, not written by the extension author.

namespace bedrock {
namespace extensions {

// Manifest permissions we classify. Not exhaustive — unknown permissions are
// kept and shown verbatim rather than dropped, because hiding a permission we
// do not recognise is the one failure mode that matters here.
enum class Capability {
  kAllSiteAccess,       // <all_urls>, *://*/*
  kSpecificSiteAccess,
  kActiveTabOnly,
  kReadBrowsingHistory,
  kReadCookies,
  kManageDownloads,
  kReadClipboard,
  kNativeMessaging,     // can talk to a program on the device
  kBackgroundPage,      // runs with no tab open
  kProxyControl,
  kWebRequestBlocking,
  kDeclarativeNetRequest,
  kStorageUnlimited,
  kDebugger,            // can inspect any page: the most dangerous of all
  kMaxValue = kDebugger,
};

enum class RiskLevel {
  kLow,       // scoped to a page the user activates
  kModerate,  // named sites, or persistent storage
  kHigh,      // all sites, cookies, history
  kCritical,  // debugger, native messaging, proxy control
};

enum class State {
  kEnabled,
  kDisabled,
  kDisabledByPolicy,  // blocked because of what it asks for
};

struct Disclosure {
  std::vector<Capability> capabilities;
  std::vector<std::string> host_patterns;  // where it may run
  bool persistent_storage = false;
  bool network_access = false;
  bool background_activity = false;
  // Sentence shown in bold in the install prompt, empty when nothing warrants
  // a warning. Generated, never author-supplied.
  std::string headline_warning;
};

struct Extension {
  std::string id;
  std::string name;
  std::string version;
  State state = State::kEnabled;
  Disclosure disclosure;
  // Extensions get their own storage partition per profile; they never share
  // one with web content or with each other.
  std::string storage_path;
};

class ExtensionRegistry {
 public:
  explicit ExtensionRegistry(const std::string& profile_id);
  ~ExtensionRegistry();

  // Builds the disclosure from a manifest's permission list. Unknown strings
  // are preserved in `unknown` so the UI can show them verbatim.
  static Disclosure Analyze(const std::vector<std::string>& manifest_permissions,
                            const std::vector<std::string>& host_permissions,
                            bool has_background,
                            std::vector<std::string>* unknown = nullptr);

  static RiskLevel Risk(const Disclosure& disclosure);
  static const char* CapabilityName(Capability capability);
  static const char* CapabilityExplanation(Capability capability);
  static const char* RiskName(RiskLevel risk);

  // Install. Returns nullptr if the id is taken. The caller must have shown
  // the disclosure first; `user_confirmed` records that it did.
  Extension* Install(const std::string& id,
                     const std::string& name,
                     const std::string& version,
                     const Disclosure& disclosure,
                     bool user_confirmed);

  bool Remove(const std::string& id);
  bool SetEnabled(const std::string& id, bool enabled);

  // Update. A version that asks for *more* than the installed one is staged,
  // not applied: it lands disabled with `pending_review` set, so an extension
  // cannot grow its powers through an auto-update the user never saw.
  enum class UpdateResult { kNotFound, kUpdated, kNeedsReview };
  UpdateResult Update(const std::string& id,
                      const std::string& version,
                      const Disclosure& disclosure);
  bool HasPendingReview(const std::string& id) const;
  bool ApprovePendingReview(const std::string& id);

  // Per-extension configuration the user controls, independent of the manifest.
  void SetHostAccess(const std::string& id,
                     const std::vector<std::string>& allowed_hosts);
  std::vector<std::string> HostAccess(const std::string& id) const;
  void SetAllowedInPrivateWindows(const std::string& id, bool allowed);
  bool AllowedInPrivateWindows(const std::string& id) const;

  Extension* Find(const std::string& id);
  const Extension* Find(const std::string& id) const;
  std::vector<std::string> Ids() const;
  size_t size() const { return extensions_.size(); }

 private:
  struct Entry {
    Extension extension;
    Disclosure pending;
    std::string pending_version;
    bool pending_review = false;
    bool private_windows = false;  // off by default: a private window is the
                                   // last place to run someone else's code
    std::vector<std::string> host_overrides;
  };

  std::string profile_id_;
  std::map<std::string, Entry> extensions_;
};

}  // namespace extensions
}  // namespace bedrock

#endif  // BEDROCK_EXTENSIONS_EXTENSION_REGISTRY_H_

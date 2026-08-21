// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_NET_WEBRTC_POLICY_H_
#define BEDROCK_NET_WEBRTC_POLICY_H_

#include <string>

// WebRTC IP exposure (roadmap item 18).
//
// WebRTC gathers ICE candidates to find the shortest path between two peers.
// Those candidates contain IP addresses, and a page can read them with no
// permission prompt and no call in progress — the well-known "WebRTC leak".
//
// Chromium exposes exactly one lever for this, `WebRTCIPHandlingPolicy`, and
// pretending we have finer control than the engine gives us would be a lie in
// the settings UI. So the three modes below map onto that lever, and each one
// states plainly what still leaks.

namespace bedrock {
namespace net {

enum class WebRtcMode {
  // Chromium's default: all interfaces. Best call quality, local IPs visible.
  kDefault,
  // Public interface only, and only after the user grants camera/microphone
  // permission. Local addresses stay hidden; calls still work through the
  // public path or a relay. This is Bedrock's default.
  kPrivacy,
  // No candidates without permission, and proxy-only routing when a proxy is
  // configured. Peer-to-peer calls may fail outright.
  kStrict,
};

// The Chromium policy string this mode maps to. Named exactly as the engine
// names them so the mapping can be checked against upstream.
enum class IpHandlingPolicy {
  kDefault,
  kDefaultPublicAndPrivateInterfaces,
  kDefaultPublicInterfaceOnly,
  kDisableNonProxiedUdp,
};

class WebRtcPolicy {
 public:
  explicit WebRtcPolicy(WebRtcMode mode = WebRtcMode::kPrivacy);

  void set_mode(WebRtcMode mode) { mode_ = mode; }
  WebRtcMode mode() const { return mode_; }

  // `media_permission_granted`: the site has camera or microphone access, i.e.
  // the user knowingly started a call. Before that, no mode exposes a local
  // address.
  IpHandlingPolicy Handling(bool media_permission_granted) const;

  // True if any local (RFC1918 / mDNS-resolvable) address may appear in ICE
  // candidates under this mode and permission state.
  bool ExposesLocalAddresses(bool media_permission_granted) const;

  // mDNS candidate obfuscation: local addresses are replaced with random
  // .local names, so a LAN path can still be found without handing the page a
  // routable local IP. Enabled everywhere except kDefault.
  bool UseMdnsCandidates() const { return mode_ != WebRtcMode::kDefault; }

  // Plain-language risk description for the settings UI. Must state what is
  // still exposed, not only what is hidden.
  static const char* Explain(WebRtcMode mode);
  static const char* Tradeoff(WebRtcMode mode);
  static const char* ModeName(WebRtcMode mode);

 private:
  WebRtcMode mode_;
};

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_NET_WEBRTC_POLICY_H_

// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/net/webrtc_policy.h"

namespace bedrock {
namespace net {

WebRtcPolicy::WebRtcPolicy(WebRtcMode mode) : mode_(mode) {}

IpHandlingPolicy WebRtcPolicy::Handling(bool media_permission_granted) const {
  switch (mode_) {
    case WebRtcMode::kDefault:
      return IpHandlingPolicy::kDefault;
    case WebRtcMode::kPrivacy:
      // Without permission there is no legitimate reason for a page to gather
      // candidates at all, so it gets the narrowest policy the engine has.
      return media_permission_granted
                 ? IpHandlingPolicy::kDefaultPublicInterfaceOnly
                 : IpHandlingPolicy::kDisableNonProxiedUdp;
    case WebRtcMode::kStrict:
      return IpHandlingPolicy::kDisableNonProxiedUdp;
  }
  return IpHandlingPolicy::kDefaultPublicInterfaceOnly;
}

bool WebRtcPolicy::ExposesLocalAddresses(bool media_permission_granted) const {
  // Permission deliberately does not enter this answer: granting camera access
  // lets a site place a call, it does not license reading the LAN layout. With
  // mDNS obfuscation the candidate carries a random .local name instead of the
  // address, so only kDefault ever hands out a real local IP.
  (void)media_permission_granted;
  return mode_ == WebRtcMode::kDefault;
}

// static
const char* WebRtcPolicy::ModeName(WebRtcMode mode) {
  switch (mode) {
    case WebRtcMode::kDefault:
      return "Default";
    case WebRtcMode::kPrivacy:
      return "Privacy";
    case WebRtcMode::kStrict:
      return "Strict";
  }
  return "";
}

// static
const char* WebRtcPolicy::Explain(WebRtcMode mode) {
  switch (mode) {
    case WebRtcMode::kDefault:
      return "Video and voice calls in the browser can see every network "
             "address of this device, including your address on the local "
             "network — and a page can read them without asking. Your public "
             "IP address is visible in every mode, because the site you are "
             "connected to already knows it.";
    case WebRtcMode::kPrivacy:
      return "Your local network address is replaced with a random name, and "
             "no addresses are gathered at all until you allow a site to use "
             "your camera or microphone. Your public IP address is still "
             "visible during a call: that is how the connection is made.";
    case WebRtcMode::kStrict:
      return "Calls only go through your configured proxy, and direct "
             "connections are refused. Nothing is gathered without "
             "permission. If you use no proxy, most calls will not connect.";
  }
  return "";
}

// static
const char* WebRtcPolicy::Tradeoff(WebRtcMode mode) {
  switch (mode) {
    case WebRtcMode::kDefault:
      return "Best call quality. No protection against address leaks.";
    case WebRtcMode::kPrivacy:
      return "Calls work; a direct connection over your local network may fall "
             "back to a relay, which can add delay.";
    case WebRtcMode::kStrict:
      return "Breaks most video and voice calls unless you route them through "
             "a proxy. Choose this if you never make calls in the browser.";
  }
  return "";
}

}  // namespace net
}  // namespace bedrock

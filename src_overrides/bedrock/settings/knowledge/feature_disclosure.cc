// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/knowledge/feature_disclosure.h"

#include <vector>

namespace bedrock {
namespace settings {

namespace {

using privacy::Feature;

// Order matches the feature registry in privacy/core/privacy_engine.cc; the
// test fails if a feature there has no row here.
//
// Writing "what it cannot protect" is the point of this table. Every line below
// was written by asking: if a competent adversary read our own description,
// what would they do next? That sentence is the answer.
const std::vector<Disclosure>& Table() {
  static const std::vector<Disclosure> table = {
      // --- Content blocker ---
      {Feature::kTrackerProtection, "tracker_protection",
       "Requests are matched against loaded filter lists and a behavioural "
       "heuristic before they leave the browser; a match is never sent. A "
       "subdomain of the site you are on is also checked for a DNS alias "
       "pointing at a tracker, so a tracker cannot hide behind the site's own "
       "name.",
       "Third-party requests to known tracking endpoints, so those companies "
       "do not receive your IP address, referrer or cookies at all.",
       "Trackers served from the site's own domain are not on a list and are "
       "not blocked by this feature alone. An alias is recognised only after "
       "it has been looked up, so the first request to a newly seen aliased "
       "name still goes out. First-party analytics running in the page still "
       "sees you.",
       "Some sites detect blocking and ask you to disable it; a few login "
       "flows that route through an ad network need an exception.",
       {3, 1, 1, 0, 2,
        "The filter engine is the most complex privacy component (ADR 0002); "
        "it earns that with the largest single privacy gain."}},

      {Feature::kAdBlocking, "ad_blocking",
       "The same filter engine, with the advertising rule sets enabled: "
       "matching network requests are cancelled before connection.",
       "Ad delivery, which is also the delivery path for most tracking and for "
       "a meaningful share of drive-by malware.",
       "Ads served from the first party, sponsored content written into the "
       "page, and native placements are indistinguishable from content at the "
       "network layer.",
       "Sites funded by ads may show empty regions or anti-adblock notices.",
       {2, 2, 1, 0, 1, ""}},

      {Feature::kCosmeticFiltering, "cosmetic_filtering",
       "Element-hiding rules from the lists are applied as stylesheet rules "
       "after the network layer has already refused the request.",
       "The leftover holes and placeholders where a blocked element was, so a "
       "page still reads normally.",
       "Nothing at all at the network layer — this is presentation only. An "
       "element hidden by CSS was still delivered if it was not blocked.",
       "Aggressive rules can hide a real control on a redesigned page; this is "
       "the most common cause of a 'broken site' report.",
       {1, 0, 2, 1, 1,
        "Marked as breaking sites in the registry: hiding is guesswork about "
        "layout, and layouts change."},
       "On by default despite scoring as opt-in: blocking a request without "
       "hiding the hole it leaves produces a page the user reads as broken, and "
       "they blame the browser rather than the ad. The privacy gain really is "
       "low; the cost of shipping it off is that blocking looks broken."},

      {Feature::kCrossSiteTrackingProtection, "cross_site_tracking_protection",
       "Third-party state and requests are evaluated per top-level site, so a "
       "party embedded on two sites cannot join the two visits.",
       "The join itself: the ability to link your session on one site to your "
       "session on another through a shared embedded party.",
       "Tracking that happens server-to-server after you log in with the same "
       "identity on both sites. Nothing in the browser can see that exchange.",
       "Embedded widgets that expect a shared session (a comment box, a social "
       "login) may ask you to sign in again per site.",
       {3, 1, 2, 1, 2,
        "The privacy gain is the highest available: breaking the cross-site "
        "join is the single change trackers cannot work around client-side."}},

      // --- Network ---
      {Feature::kReferrerControl, "referrer_control",
       "Cross-origin requests carry only the origin, or no referrer at all in "
       "strict mode; same-origin navigation is untouched.",
       "The full URL of the page you came from, which routinely contains "
       "search terms, document titles and account identifiers.",
       "Sites that pass the same information in a query parameter, a redirect "
       "chain or a POST body. The referrer is one channel of several.",
       "A few sites use the referrer for hotlink protection and will refuse "
       "images or downloads without it.",
       {2, 1, 1, 0, 1, ""}},

      {Feature::kQueryParamStripping, "query_param_stripping",
       "Known tracking parameters (gclid, fbclid, msclkid and similar) are "
       "removed from a URL before the request is made.",
       "Click identifiers that tie an ad impression to your visit and that "
       "survive being pasted into a message or a bookmark.",
       "Parameters the site itself needs and uses for tracking at the same "
       "time, and any identifier moved into the path or a fragment.",
       "Rarely, a campaign landing page shows the wrong variant or a coupon "
       "does not apply.",
       {2, 0, 2, 0, 1, ""}},

      {Feature::kHttpsOnly, "https_only",
       "Navigations are upgraded to HTTPS; in strict mode a site that cannot "
       "be reached over HTTPS shows an interstitial instead of falling back.",
       "Passive interception and content injection on the network path between "
       "you and the site.",
       "The site itself, its hosting provider, and anyone with a certificate "
       "the browser trusts. HTTPS proves the transport, not the recipient.",
       "Older sites and devices on a local network may have no HTTPS endpoint "
       "and become unreachable in strict mode.",
       {2, 3, 2, 0, 1,
        "The largest security gain in the list: it removes the whole class of "
        "on-path attacks."}},

      {Feature::kSecureDns, "secure_dns",
       "DNS queries go to a user-chosen DoH resolver over HTTPS instead of to "
       "whatever resolver the network handed out.",
       "Your network operator's plain-text view of every hostname you resolve, "
       "and their ability to answer with a different address.",
       "The resolver you chose, which now sees the same queries. This moves "
       "trust; it does not remove it. TLS SNI and IP addresses still leak the "
       "destination to the network.",
       "Captive portals and split-horizon corporate DNS need the feature off "
       "or an exception list.",
       {2, 2, 2, 1, 2, ""}},

      {Feature::kThirdPartyRequestControl, "third_party_requests",
       "All requests to origins other than the top-level site are blocked "
       "unless the user allows them per site.",
       "Every third-party channel at once, including ones no filter list has "
       "ever heard of.",
       "First-party tracking, and anything the site proxies through its own "
       "domain on the server side.",
       "Off by default because it breaks a large share of the web: CDNs, "
       "fonts, payment frames and video players are all third parties.",
       {3, 2, 3, 0, 1,
        "Highest compatibility loss in the table, which is exactly why the "
        "standard default is off and only Strict enables it."}},

      // --- Storage ---
      {Feature::kCookieIsolation, "cookie_isolation",
       "Third-party cookies are refused, and remaining cookies are keyed by "
       "the top-level site so the same party gets a different jar per site.",
       "The classic cross-site cookie identifier that follows you between "
       "unrelated sites.",
       "First-party cookies on each site, server-side profiling, and any "
       "identifier stored somewhere other than a cookie.",
       "Third-party login and 'continue with' buttons may need the storage "
       "access prompt; some embedded checkout flows need an exception.",
       {3, 1, 2, 0, 2,
        "Cookie isolation is the protection trackers most actively work around, "
        "so its privacy gain is scored at the top of the scale."}},

      {Feature::kStoragePartitioning, "storage_partitioning",
       "localStorage, IndexedDB, cache and service-worker scope are keyed by "
       "the top-level site as well as by origin.",
       "Storage-based identifiers, which are what tracking moved to once "
       "third-party cookies became unreliable.",
       "State the user is knowingly signed into, and re-identification through "
       "fingerprinting rather than through storage.",
       "A site embedded in two places keeps two separate caches, so it may "
       "load fresh data more often; a very small number of widgets lose state.",
       {3, 1, 1, 1, 3,
        "Partitioning touches every storage backend in the engine; the "
        "complexity is real and is why it is one subsystem, not many."}},

      {Feature::kEphemeralThirdPartyStorage, "ephemeral_third_party_storage",
       "A third party that is not otherwise allowed gets storage that lives in "
       "memory for the tab's lifetime and is discarded on close.",
       "Persistence: an embedded party can still function during a visit but "
       "cannot recognise you on the next one.",
       "Anything recorded server-side during the visit, and re-identification "
       "by other means within the same session.",
       "Widgets that expect to remember a preference (a dismissed banner, a "
       "chosen tab) will ask again on the next visit.",
       {2, 0, 2, 1, 2, ""}},

      // --- Fingerprinting ---
      {Feature::kCanvasProtection, "canvas_protection",
       "Readback from canvas and WebGL is perturbed with a per-site, "
       "per-session value derived deterministically, so a read is stable within "
       "a site and different across sites.",
       "The canvas fingerprint as a cross-site identifier: the same machine "
       "produces a different value on every site.",
       "Detection that protection is on — an unusual response is itself a "
       "signal — and any identifier that does not come from canvas.",
       "Image editors, colour pickers and some games that read back pixels can "
       "show artefacts; per-site exceptions exist for this reason.",
       {3, 0, 2, 1, 3,
        "Deterministic per-site derivation is subtle: get it wrong in either "
        "direction and you either break sites or create a new identifier."}},

      {Feature::kWebglControls, "webgl_controls",
       "Renderer and vendor strings are reduced to a common value and debug "
       "extensions that expose the exact GPU are withheld.",
       "The precise GPU model, which is one of the most distinguishing values a "
       "page can read.",
       "Timing and capability differences that reveal the class of hardware "
       "anyway, and any site allowed to use WebGL normally.",
       "3D applications that select code paths by GPU string may pick a slower "
       "path or warn about an unsupported device.",
       {2, 0, 2, 1, 2, ""},
       "On by default although the cost edges past the gain: the GPU string is "
       "the second most distinguishing value a page can read, and the sites "
       "that branch on it degrade rather than fail."},

      {Feature::kFontExposureControl, "font_exposure",
       "Font enumeration is limited to a standard set per platform, and the "
       "local font access API is refused.",
       "The installed-font list, which is close to unique for people who have "
       "installed design or language packs.",
       "Fonts inferred from measured text metrics, which is slower for the "
       "attacker but still possible.",
       "Pages that select a locally installed font fall back to a standard "
       "one; document editors lose the font picker's local list.",
       {2, 0, 2, 0, 2, ""}},

      {Feature::kClientHintsControl, "client_hints",
       "High-entropy client hints are not sent unless a site requests them and "
       "the request is allowed; low-entropy hints are normalised.",
       "Architecture, platform version, model and full browser version being "
       "attached to requests that never asked for them.",
       "The same values read from JavaScript where an API still exposes them, "
       "and the User-Agent string itself.",
       "A few sites use hints to pick an installer or an image format and will "
       "serve a generic one instead.",
       {2, 0, 1, 0, 2, ""}},

      {Feature::kLanguageNormalization, "language_normalization",
       "navigator.languages and Accept-Language report the UI language only, "
       "without the ordered list of secondary languages.",
       "The language list, which is highly distinguishing for multilingual "
       "users and often reveals nationality.",
       "Content-based inference: a site can still guess from what you read and "
       "from your IP address.",
       "Sites that auto-select a secondary language will offer the primary one "
       "and require a manual switch.",
       {2, 0, 2, 0, 1, ""}},

      {Feature::kTimezoneNormalization, "timezone_normalization",
       "The timezone reported to pages is UTC, or the timezone of the exit "
       "point in Tor mode, rather than the system clock's zone.",
       "Coarse location inferred from the timezone offset, and the ability to "
       "correlate a session by a rare offset.",
       "Location inferred from IP address, language or content — the timezone "
       "is one weak signal among several.",
       "Calendars, booking sites and anything showing local times will display "
       "them in the reported zone unless the site asks you.",
       {1, 0, 2, 0, 1, ""}},

      {Feature::kScreenMetricNormalization, "screen_metrics",
       "Screen and window dimensions are reported rounded, and content is "
       "letterboxed to a standard size so the viewport is one of few values.",
       "The exact window size, which is both distinguishing and stable across "
       "a session.",
       "Sites that measure layout indirectly, and the fact that letterboxing "
       "itself is visible to the page.",
       "Visible margins around the page in some window sizes; responsive "
       "layouts may pick a different breakpoint than expected.",
       {2, 0, 2, 0, 2, ""}},

      {Feature::kHardwareInfoReduction, "hardware_info",
       "navigator.hardwareConcurrency and deviceMemory report values from a "
       "small standard set instead of the machine's real ones.",
       "CPU core count and memory size, which split the population into small "
       "buckets and are stable forever.",
       "Performance measurement: a page that times work can estimate the same "
       "properties without asking.",
       "Applications that size worker pools from the reported value may run "
       "fewer workers than the machine could support.",
       {2, 0, 1, 1, 1, ""}},

      {Feature::kBatteryApiProtection, "battery_api",
       "The Battery Status API is not exposed to pages.",
       "A short-lived but precise identifier: charge level and discharge time "
       "were shown to be usable for re-identification across sessions.",
       "Nothing else — this is one API. Power state may still be inferred from "
       "throttling behaviour.",
       "Effectively none; the API is used almost exclusively for tracking.",
       {1, 0, 0, 0, 0, ""}},

      {Feature::kWebrtcPolicy, "webrtc_policy",
       "WebRTC uses the public interface only, and host candidates with local "
       "IP addresses are not surfaced to pages.",
       "The local-network IP leak that revealed your machine behind a VPN or "
       "NAT to any page, without a permission prompt.",
       "The public IP address, which the page learns from the connection "
       "anyway, and media device metadata once you grant access.",
       "Some peer-to-peer applications connect more slowly or fail on "
       "restrictive networks without host candidates.",
       {3, 2, 2, 0, 2,
        "A local-IP leak identifies the machine itself, not the session, which "
        "is why the privacy gain is scored at the top."}},

      {Feature::kTimerCoarsening, "timer_coarsening",
       "High-resolution timers are rounded and jittered so measurements below "
       "the coarsening step are unreliable.",
       "Timing side channels used both for fingerprinting and for cache and "
       "speculative-execution attacks.",
       "Attacks that amplify a signal by repeating it many times; coarsening "
       "raises the cost rather than closing the channel.",
       "Benchmarks and some audio and animation code report imprecise timings; "
       "a few report a warning.",
       {2, 3, 2, 1, 2,
        "The security gain is the reason this is on by default: it is a "
        "mitigation for a family of side-channel attacks, not only tracking."}},

      // --- Permissions ---
      {Feature::kMediaDeviceEnumeration, "media_devices",
       "Camera and microphone lists are empty until permission is granted, and "
       "device ids are per-site.",
       "The device list — names and counts — which is distinguishing even when "
       "no capture ever happens.",
       "What a site learns after you grant access, which is necessarily the "
       "real device.",
       "A call site cannot show a device picker before you grant permission, "
       "so the flow becomes 'allow, then choose'.",
       {2, 1, 2, 0, 1, ""}},

      {Feature::kGamepadAndSensorExposure, "gamepad_and_sensors",
       "Gamepad, accelerometer, gyroscope and ambient light are gated behind a "
       "permission and are silent until it is granted.",
       "Sensor readings usable as an identifier (calibration differences are "
       "device-specific) and as a side channel for input.",
       "Nothing once granted: a game that needs the gamepad needs the gamepad.",
       "Games and VR pages need one extra click before the controller works.",
       {1, 1, 2, 0, 1, ""},
       "On by default although it scores as opt-in: sensor calibration is a "
       "durable device identifier, and the cost is one extra click in the small "
       "number of pages that use a gamepad or a motion sensor at all."},

      {Feature::kClipboardPermission, "clipboard_permission",
       "Reading the clipboard requires a permission and a user gesture; "
       "writing is allowed only in response to a gesture.",
       "Silent clipboard reads, which can capture passwords, addresses and "
       "wallet identifiers you copied for another purpose.",
       "What you deliberately paste into the page.",
       "Web apps that offer 'paste from clipboard' buttons need a prompt the "
       "first time.",
       {2, 3, 1, 0, 1,
        "A silent clipboard read is a credential-theft primitive, not only a "
        "privacy problem."}},

      {Feature::kGeolocationPermission, "geolocation_permission",
       "Precise location requires an explicit prompt per site, and the prompt "
       "offers an approximate answer as the first option.",
       "Precise coordinates being handed to a site that only needed a city.",
       "Location inferred from IP address, Wi-Fi environment reported by the "
       "operating system, or content you enter.",
       "Maps and delivery sites need a click, and get lower precision unless "
       "you choose otherwise.",
       {2, 1, 1, 0, 1, ""}},

      {Feature::kNotificationPermission, "notification_permission",
       "Notification requests are held back until a genuine interaction, "
       "rather than fired on page load.",
       "Prompt spam, which trains people to click Allow on everything — the "
       "real damage is to every later prompt.",
       "Notifications from sites you did allow, which can still be used to "
       "re-engage and profile you.",
       "A site cannot ask on arrival; some walk-through flows need adjusting.",
       {1, 1, 1, 0, 1, ""},
       "On by default despite scoring as opt-in: the benefit is not to the one "
       "site being gated but to every later prompt, and that benefit only "
       "exists if gating is on for everyone. The cost is one click on the rare "
       "site that genuinely wants notifications."},

      {Feature::kAutoplayControl, "autoplay_control",
       "Media with sound does not start without a user gesture on the site.",
       "Bandwidth, attention and an easy signal of presence sent back to the "
       "media host on page load.",
       "Muted background video, which most sites use anyway.",
       "Video sites need one click to start playback; a few embeds show a "
       "poster frame instead of playing.",
       {1, 0, 1, 0, 1, ""},
       "On by default although it scores as opt-in: autoplay with sound is a "
       "presence signal sent to a media host before the user has decided to "
       "watch anything, and the cost is a single click to start playback."},

      {Feature::kPermissionIsolation, "permission_isolation",
       "A permission is granted to the top-level site, not to every embedded "
       "party on it; embedded frames must be granted separately.",
       "A widget inheriting camera or location access that the user granted to "
       "the page around it.",
       "Anything the top-level site chooses to pass on to its embeds after "
       "receiving it.",
       "Embedded video calls and map widgets ask for their own permission, "
       "which users may find repetitive.",
       {2, 2, 2, 0, 2, ""}},
  };
  return table;
}

}  // namespace

const std::vector<Disclosure>& GetDisclosures() {
  return Table();
}

const Disclosure* FindDisclosure(privacy::Feature feature) {
  for (const Disclosure& row : Table()) {
    if (row.feature == feature) {
      return &row;
    }
  }
  return nullptr;
}

bool IsDefaultable(const Tradeoff& tradeoff) {
  const int gain = tradeoff.privacy_gain > tradeoff.security_gain
                       ? tradeoff.privacy_gain
                       : tradeoff.security_gain;
  if (gain < 1) {
    return false;
  }
  // Cost is dominated by what the user notices: a broken page is worse than a
  // millisecond. Complexity is our problem, not theirs, so it is not counted
  // here — it is counted in review.
  if (tradeoff.compatibility_loss >= 3) {
    // Whatever it buys: a protection that breaks a large share of the web is
    // an opt-in, because the user who cannot load their bank will turn off
    // everything rather than find the one switch.
    return false;
  }
  const int cost = tradeoff.compatibility_loss + tradeoff.performance_cost;
  if (cost == 0) {
    // A protection nobody pays for is on, however small the gain. Refusing a
    // free improvement because it is small is how a browser ends up with a
    // long list of "minor" exposures nobody ever closed.
    return true;
  }
  return gain >= 2 && cost <= gain;
}

}  // namespace settings
}  // namespace bedrock

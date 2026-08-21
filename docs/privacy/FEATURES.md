# Privacy features, in full

**Roadmap item 82.** Generated from the disclosure table in
[`src_overrides/bedrock/settings/knowledge/feature_disclosure.cc`](../../src_overrides/bedrock/settings/knowledge/feature_disclosure.cc)
by `scripts/check_transparency.py --write`; the gate fails if this file drifts from it.

A checkbox labelled "Ultimate Privacy" is a marketing claim wearing a control's clothes.
Every protection here states four things, and the settings UI shows them next to the
control rather than in a help centre nobody opens. The third one — what it *cannot* do —
is the one worth reading.

## tracker_protection

**How it works.** Requests are matched against loaded filter lists and a behavioural heuristic before they leave the browser; a match is never sent.

**What it protects.** Third-party requests to known tracking endpoints, so those companies do not receive your IP address, referrer or cookies at all.

**What it cannot protect.** Trackers served from the site's own domain, or through a CNAME that points at a tracker, are not on a list and are not blocked by this feature alone. First-party analytics running in the page still sees you.

**Compatibility impact.** Some sites detect blocking and ask you to disable it; a few login flows that route through an ad network need an exception.

## ad_blocking

**How it works.** The same filter engine, with the advertising rule sets enabled: matching network requests are cancelled before connection.

**What it protects.** Ad delivery, which is also the delivery path for most tracking and for a meaningful share of drive-by malware.

**What it cannot protect.** Ads served from the first party, sponsored content written into the page, and native placements are indistinguishable from content at the network layer.

**Compatibility impact.** Sites funded by ads may show empty regions or anti-adblock notices.

## cosmetic_filtering

**How it works.** Element-hiding rules from the lists are applied as stylesheet rules after the network layer has already refused the request.

**What it protects.** The leftover holes and placeholders where a blocked element was, so a page still reads normally.

**What it cannot protect.** Nothing at all at the network layer — this is presentation only. An element hidden by CSS was still delivered if it was not blocked.

**Compatibility impact.** Aggressive rules can hide a real control on a redesigned page; this is the most common cause of a 'broken site' report.

## cross_site_tracking_protection

**How it works.** Third-party state and requests are evaluated per top-level site, so a party embedded on two sites cannot join the two visits.

**What it protects.** The join itself: the ability to link your session on one site to your session on another through a shared embedded party.

**What it cannot protect.** Tracking that happens server-to-server after you log in with the same identity on both sites. Nothing in the browser can see that exchange.

**Compatibility impact.** Embedded widgets that expect a shared session (a comment box, a social login) may ask you to sign in again per site.

## referrer_control

**How it works.** Cross-origin requests carry only the origin, or no referrer at all in strict mode; same-origin navigation is untouched.

**What it protects.** The full URL of the page you came from, which routinely contains search terms, document titles and account identifiers.

**What it cannot protect.** Sites that pass the same information in a query parameter, a redirect chain or a POST body. The referrer is one channel of several.

**Compatibility impact.** A few sites use the referrer for hotlink protection and will refuse images or downloads without it.

## query_param_stripping

**How it works.** Known tracking parameters (gclid, fbclid, msclkid and similar) are removed from a URL before the request is made.

**What it protects.** Click identifiers that tie an ad impression to your visit and that survive being pasted into a message or a bookmark.

**What it cannot protect.** Parameters the site itself needs and uses for tracking at the same time, and any identifier moved into the path or a fragment.

**Compatibility impact.** Rarely, a campaign landing page shows the wrong variant or a coupon does not apply.

## https_only

**How it works.** Navigations are upgraded to HTTPS; in strict mode a site that cannot be reached over HTTPS shows an interstitial instead of falling back.

**What it protects.** Passive interception and content injection on the network path between you and the site.

**What it cannot protect.** The site itself, its hosting provider, and anyone with a certificate the browser trusts. HTTPS proves the transport, not the recipient.

**Compatibility impact.** Older sites and devices on a local network may have no HTTPS endpoint and become unreachable in strict mode.

## secure_dns

**How it works.** DNS queries go to a user-chosen DoH resolver over HTTPS instead of to whatever resolver the network handed out.

**What it protects.** Your network operator's plain-text view of every hostname you resolve, and their ability to answer with a different address.

**What it cannot protect.** The resolver you chose, which now sees the same queries. This moves trust; it does not remove it. TLS SNI and IP addresses still leak the destination to the network.

**Compatibility impact.** Captive portals and split-horizon corporate DNS need the feature off or an exception list.

## third_party_requests

**How it works.** All requests to origins other than the top-level site are blocked unless the user allows them per site.

**What it protects.** Every third-party channel at once, including ones no filter list has ever heard of.

**What it cannot protect.** First-party tracking, and anything the site proxies through its own domain on the server side.

**Compatibility impact.** Off by default because it breaks a large share of the web: CDNs, fonts, payment frames and video players are all third parties.

## cookie_isolation

**How it works.** Third-party cookies are refused, and remaining cookies are keyed by the top-level site so the same party gets a different jar per site.

**What it protects.** The classic cross-site cookie identifier that follows you between unrelated sites.

**What it cannot protect.** First-party cookies on each site, server-side profiling, and any identifier stored somewhere other than a cookie.

**Compatibility impact.** Third-party login and 'continue with' buttons may need the storage access prompt; some embedded checkout flows need an exception.

## storage_partitioning

**How it works.** localStorage, IndexedDB, cache and service-worker scope are keyed by the top-level site as well as by origin.

**What it protects.** Storage-based identifiers, which are what tracking moved to once third-party cookies became unreliable.

**What it cannot protect.** State the user is knowingly signed into, and re-identification through fingerprinting rather than through storage.

**Compatibility impact.** A site embedded in two places keeps two separate caches, so it may load fresh data more often; a very small number of widgets lose state.

## ephemeral_third_party_storage

**How it works.** A third party that is not otherwise allowed gets storage that lives in memory for the tab's lifetime and is discarded on close.

**What it protects.** Persistence: an embedded party can still function during a visit but cannot recognise you on the next one.

**What it cannot protect.** Anything recorded server-side during the visit, and re-identification by other means within the same session.

**Compatibility impact.** Widgets that expect to remember a preference (a dismissed banner, a chosen tab) will ask again on the next visit.

## canvas_protection

**How it works.** Readback from canvas and WebGL is perturbed with a per-site, per-session value derived deterministically, so a read is stable within a site and different across sites.

**What it protects.** The canvas fingerprint as a cross-site identifier: the same machine produces a different value on every site.

**What it cannot protect.** Detection that protection is on — an unusual response is itself a signal — and any identifier that does not come from canvas.

**Compatibility impact.** Image editors, colour pickers and some games that read back pixels can show artefacts; per-site exceptions exist for this reason.

## webgl_controls

**How it works.** Renderer and vendor strings are reduced to a common value and debug extensions that expose the exact GPU are withheld.

**What it protects.** The precise GPU model, which is one of the most distinguishing values a page can read.

**What it cannot protect.** Timing and capability differences that reveal the class of hardware anyway, and any site allowed to use WebGL normally.

**Compatibility impact.** 3D applications that select code paths by GPU string may pick a slower path or warn about an unsupported device.

## font_exposure

**How it works.** Font enumeration is limited to a standard set per platform, and the local font access API is refused.

**What it protects.** The installed-font list, which is close to unique for people who have installed design or language packs.

**What it cannot protect.** Fonts inferred from measured text metrics, which is slower for the attacker but still possible.

**Compatibility impact.** Pages that select a locally installed font fall back to a standard one; document editors lose the font picker's local list.

## client_hints

**How it works.** High-entropy client hints are not sent unless a site requests them and the request is allowed; low-entropy hints are normalised.

**What it protects.** Architecture, platform version, model and full browser version being attached to requests that never asked for them.

**What it cannot protect.** The same values read from JavaScript where an API still exposes them, and the User-Agent string itself.

**Compatibility impact.** A few sites use hints to pick an installer or an image format and will serve a generic one instead.

## language_normalization

**How it works.** navigator.languages and Accept-Language report the UI language only, without the ordered list of secondary languages.

**What it protects.** The language list, which is highly distinguishing for multilingual users and often reveals nationality.

**What it cannot protect.** Content-based inference: a site can still guess from what you read and from your IP address.

**Compatibility impact.** Sites that auto-select a secondary language will offer the primary one and require a manual switch.

## timezone_normalization

**How it works.** The timezone reported to pages is UTC, or the timezone of the exit point in Tor mode, rather than the system clock's zone.

**What it protects.** Coarse location inferred from the timezone offset, and the ability to correlate a session by a rare offset.

**What it cannot protect.** Location inferred from IP address, language or content — the timezone is one weak signal among several.

**Compatibility impact.** Calendars, booking sites and anything showing local times will display them in the reported zone unless the site asks you.

## screen_metrics

**How it works.** Screen and window dimensions are reported rounded, and content is letterboxed to a standard size so the viewport is one of few values.

**What it protects.** The exact window size, which is both distinguishing and stable across a session.

**What it cannot protect.** Sites that measure layout indirectly, and the fact that letterboxing itself is visible to the page.

**Compatibility impact.** Visible margins around the page in some window sizes; responsive layouts may pick a different breakpoint than expected.

## hardware_info

**How it works.** navigator.hardwareConcurrency and deviceMemory report values from a small standard set instead of the machine's real ones.

**What it protects.** CPU core count and memory size, which split the population into small buckets and are stable forever.

**What it cannot protect.** Performance measurement: a page that times work can estimate the same properties without asking.

**Compatibility impact.** Applications that size worker pools from the reported value may run fewer workers than the machine could support.

## battery_api

**How it works.** The Battery Status API is not exposed to pages.

**What it protects.** A short-lived but precise identifier: charge level and discharge time were shown to be usable for re-identification across sessions.

**What it cannot protect.** Nothing else — this is one API. Power state may still be inferred from throttling behaviour.

**Compatibility impact.** Effectively none; the API is used almost exclusively for tracking.

## webrtc_policy

**How it works.** WebRTC uses the public interface only, and host candidates with local IP addresses are not surfaced to pages.

**What it protects.** The local-network IP leak that revealed your machine behind a VPN or NAT to any page, without a permission prompt.

**What it cannot protect.** The public IP address, which the page learns from the connection anyway, and media device metadata once you grant access.

**Compatibility impact.** Some peer-to-peer applications connect more slowly or fail on restrictive networks without host candidates.

## timer_coarsening

**How it works.** High-resolution timers are rounded and jittered so measurements below the coarsening step are unreliable.

**What it protects.** Timing side channels used both for fingerprinting and for cache and speculative-execution attacks.

**What it cannot protect.** Attacks that amplify a signal by repeating it many times; coarsening raises the cost rather than closing the channel.

**Compatibility impact.** Benchmarks and some audio and animation code report imprecise timings; a few report a warning.

## media_devices

**How it works.** Camera and microphone lists are empty until permission is granted, and device ids are per-site.

**What it protects.** The device list — names and counts — which is distinguishing even when no capture ever happens.

**What it cannot protect.** What a site learns after you grant access, which is necessarily the real device.

**Compatibility impact.** A call site cannot show a device picker before you grant permission, so the flow becomes 'allow, then choose'.

## gamepad_and_sensors

**How it works.** Gamepad, accelerometer, gyroscope and ambient light are gated behind a permission and are silent until it is granted.

**What it protects.** Sensor readings usable as an identifier (calibration differences are device-specific) and as a side channel for input.

**What it cannot protect.** Nothing once granted: a game that needs the gamepad needs the gamepad.

**Compatibility impact.** Games and VR pages need one extra click before the controller works.

## clipboard_permission

**How it works.** Reading the clipboard requires a permission and a user gesture; writing is allowed only in response to a gesture.

**What it protects.** Silent clipboard reads, which can capture passwords, addresses and wallet identifiers you copied for another purpose.

**What it cannot protect.** What you deliberately paste into the page.

**Compatibility impact.** Web apps that offer 'paste from clipboard' buttons need a prompt the first time.

## geolocation_permission

**How it works.** Precise location requires an explicit prompt per site, and the prompt offers an approximate answer as the first option.

**What it protects.** Precise coordinates being handed to a site that only needed a city.

**What it cannot protect.** Location inferred from IP address, Wi-Fi environment reported by the operating system, or content you enter.

**Compatibility impact.** Maps and delivery sites need a click, and get lower precision unless you choose otherwise.

## notification_permission

**How it works.** Notification requests are held back until a genuine interaction, rather than fired on page load.

**What it protects.** Prompt spam, which trains people to click Allow on everything — the real damage is to every later prompt.

**What it cannot protect.** Notifications from sites you did allow, which can still be used to re-engage and profile you.

**Compatibility impact.** A site cannot ask on arrival; some walk-through flows need adjusting.

## autoplay_control

**How it works.** Media with sound does not start without a user gesture on the site.

**What it protects.** Bandwidth, attention and an easy signal of presence sent back to the media host on page load.

**What it cannot protect.** Muted background video, which most sites use anyway.

**Compatibility impact.** Video sites need one click to start playback; a few embeds show a poster frame instead of playing.

## permission_isolation

**How it works.** A permission is granted to the top-level site, not to every embedded party on it; embedded frames must be granted separately.

**What it protects.** A widget inheriting camera or location access that the user granted to the page around it.

**What it cannot protect.** Anything the top-level site chooses to pass on to its embeds after receiving it.

**Compatibility impact.** Embedded video calls and map widgets ask for their own permission, which users may find repetitive.

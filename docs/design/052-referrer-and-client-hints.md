# 052 — Referrer policy and client hints

The top item of the research queue in `.ai/memory/STATE.md`: *referrer /
Client-Hints policy*. Both are headers the page never asked for that describe
the user, both are decided per request, and both were still `kDesigned` /
implemented only as a strategy row — so they are implemented together as one
host-tested component, `privacy/network/request_headers`
(`RequestHeaderPolicy`).

One component, because they are one decision with two outputs: *what does this
outgoing request tell the other side about the person making it?* Splitting it
gives two places that answer for the same request, which non-negotiable 4 of
`.ai/MEMORY.md` forbids.

Feature `referrer_control` moves from `designed` to `policy-landed`;
`client_hints` keeps its status and now has the header layer under it, not only
the fingerprinting strategy row. Nothing here runs inside a browser yet — the
entry points that hand a real request to this class are phase 3 in
`docs/PHASES.md`.

## The three rules

**1. A site may ask for less, never for more.** The floor comes from
`Control::kReferrer` (`kAllow` full · `kReduce` origin cross-site · `kBlock`
nothing cross-site · `kBlockStrict` nothing at all). A page-declared
`Referrer-Policy` is then applied only if it is *stricter*, using the same
`Stricter()` ordering (`kFullUrl < kOriginOnly < kNone`). `unsafe-url` is
therefore parsed and refused rather than unrecognised — a request to leak the
user's full URL is a request, not a configuration — and
`DeclaredPolicyRefused()` exists so the site panel can say the page asked and
was told no, instead of silently claiming its policy applied.

**2. A hint goes only to the party that asked for it.** High-entropy client
hints reach the first party that requested them via `Accept-CH` and are never
delegated to a third party, whatever `Permissions-Policy` says. This rule is
the one thing `kCompatibility` does *not* relax: it is a question of who asked,
not of how much a value reveals. Delegation is precisely how a hint the user
granted to one site becomes a header on twenty tracker requests.

**3. A header must agree with the JavaScript surface.** Every value comes from
`privacy/fingerprinting/fingerprint_policy.h` — `NormalizedDeviceMemoryGb`,
`QuantizeWindowSize`, `NormalizedLanguage`. A `Device-Memory: 16` header next to
a `navigator.deviceMemory` of 8 is *more* identifying than either value alone,
and the test asserts the equality rather than trusting it.

## The referrer decision

| Situation | Sent | Why |
| --- | --- | --- |
| Same site, default | full URL minus credentials and fragment | the site already knows the page you were on |
| Cross-site, default | origin only | "which site sent me" is what a referrer is legitimately for |
| Cross-site, Strict | nothing | |
| `https:` → `http:` | nothing, at every setting including `kAllow` | the one case every browser agrees on; the floor may be lowered, a downgrade may not |
| Opaque origin (`data:`, `blob:`, `about:`) | nothing | there is no origin to trim to |
| Unknown eTLD+1 on either side | treated as third party | the safe direction for a header that leaks |
| Credentials or a fragment in the URL | always removed | a referrer has never needed either, and both have leaked in shipped browsers |

Host names are compared only after `NormalizeHost()` (finding F10): `EXAMPLE.test.`
and `example.test` are the same site, and a party check that says otherwise
turns a first-party request into a "third party" one and vice versa.

## The client-hint decision

| Level | Low-entropy (`Sec-CH-UA`, `-Mobile`, `-Platform`) | UA identity (`-Arch`, `-Bitness`, `-Model`, `-Platform-Version`, `-Full-Version-List`) | Layout (`Device-Memory`, `DPR`, `Viewport-Width`, `Width`, `Prefers-Color-Scheme`) | Network quality (`RTT`, `Downlink`, `ECT`) |
| --- | --- | --- | --- | --- |
| `kCompatibility` | real values | requested, first party only | requested, first party only | requested, first party only |
| `kBalanced` (default) | normalised | never on the wire | requested, first party only, normalised values | never |
| `kStrict` | normalised | never | never | never |
| `kMaximum` | none | none | none | none |

The UA-identity column is not a judgement call: `docs/privacy/fingerprinting/client-hints.md`
states that from level 1 no `Sec-CH-UA-*` beyond the low-entropy set appears on
the wire, and a stated claim is a claim the code has to keep. A first party that
genuinely needs them reads the same normalised values from
`navigator.userAgentData.getHighEntropyValues()`, which is the renderer side and
phase 3. The layout hints are a different question — they describe how to render
a page, their values already come from the normalisers, and dropping them buys
nothing while breaking responsive images.

The low-entropy three are kept, normalised, rather than dropped: a browser that
sends none of them is itself distinctive, and content negotiation depends on
them. Reduced values are the ones UA reduction settled on — major version only,
empty platform version, `x86`/`64`, no model, DPR 1, letterboxed viewport width.
An emptied hint is an **absent header**, not an empty one, because `Sec-CH-UA-Model: ""`
still says "this browser answers model requests".

`Save-Data` is the exception in the other direction: it is the user's own signal,
sent when they enabled it and a site asked, never invented to look like a
population.

## What this does not do

- It does not stop the same information travelling in a query parameter, a
  redirect chain or a POST body. The referrer is one channel of several — the
  honest sentence is already in `docs/privacy/FEATURES.md` and stays there.
- It does not touch `document.referrer` inside the page; that is the renderer
  side and needs the Chromium build.
- It does not decide *whether* a request is made. That is
  `BlockingPipeline::Evaluate()`, and this component runs after it.

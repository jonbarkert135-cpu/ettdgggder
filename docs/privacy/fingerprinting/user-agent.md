# User-Agent

**Surface id:** `user-agent` · **Levels:** 0 allow · 1–3 normalize

## Attack vector
The UA string carries the exact browser and OS version. A niche browser's UA is, by itself, a near-unique identifier — and a browser that announces itself as "Bedrock" would be *more* identifying than Chrome.

## Mitigation
Bedrock reports a Chrome UA with the same major version and a frozen minor version, and a coarse platform (`Windows NT 10.0; Win64; x64`, `Macintosh; Intel Mac OS X 10_15_7`, `X11; Linux x86_64`). No product token identifies Bedrock. `navigator.userAgentData` is kept consistent with it (see `client-hints`).

## Compatibility impact
Server-side sniffing treats Bedrock as Chrome, which is what we want for compatibility. Sites that need the real client for support purposes get a wrong answer — an accepted trade documented in the FAQ.

## Performance impact
None.

## Test cases
- UA string is byte-identical across Bedrock installs of the same major version and OS family.
- UA, client hints and `navigator.platform` agree.
- The string contains no "Bedrock" token.

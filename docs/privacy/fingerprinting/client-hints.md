# Client Hints

**Surface id:** `client-hints` · **Levels:** 0 allow · 1–2 normalize · 3 block

## Attack vector
High-entropy UA client hints (`Sec-CH-UA-Full-Version-List`, `-Platform-Version`, `-Model`, `-Arch`, `-Bitness`) hand out the exact OS build and CPU on request, both in headers and via `navigator.userAgentData.getHighEntropyValues()`.

## Mitigation
Only low-entropy hints (brand list, mobileness, coarse platform) are sent from level 1, and their values are normalized to the same set Bedrock's User-Agent reports. High-entropy requests resolve with those same normalized values instead of rejecting, so feature-detection code keeps working. Level 3 omits client hint headers entirely.

## Compatibility impact
Minimal: sites use hints for OS-specific download links and image formats; they receive a consistent, plausible answer. Level 3 can produce a wrong download suggestion.

## Performance impact
None; fewer header bytes are sent.

## Test cases
- `getHighEntropyValues()` agrees with the User-Agent string.
- Two machines on different OS builds send identical hint headers.
- No `Sec-CH-UA-*` beyond the low-entropy set appears on the wire.

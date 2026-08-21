# Web Audio

**Surface id:** `audio` · **Levels:** 0 allow · 1–2 farble · 3 block

## Attack vector
`AudioContext` + `OfflineAudioContext` produce floating-point output whose last bits depend on the audio stack and CPU. Hashing an oscillator's rendered buffer yields a stable identifier requiring no user permission and no sound output.

## Mitigation
Deterministic perturbation of the values returned by `getFloatFrequencyData`, `getByteTimeDomainData` and offline rendering, seeded with `kAudio`. Magnitude is far below audibility (~1e-7 relative), so real playback and visualisers are unaffected. Level 3 makes `AudioContext` construction fail after a prompt.

## Compatibility impact
Levels 1–2: inaudible; breaks only bit-exact audio analysis. Level 3 breaks all web audio, including players that route through `AudioContext`.

## Performance impact
Perturbation is applied to analysis buffers only, not to the realtime rendering path — no added latency, no dropouts.

## Test cases
- Two reads of the same oscillator in one session → identical hash.
- Different sites → different hashes.
- Playback of a reference tone stays within 1e-6 of the original samples.

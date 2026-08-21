# Speech synthesis voices

**Surface id:** `speech-voices` · **Levels:** 0 allow · 1–2 normalize · 3 block

## Attack vector
`speechSynthesis.getVoices()` lists installed TTS voices, which vary by OS, language packs and third-party software — high entropy, no permission required.

## Mitigation
Level 1+ returns only the voices that ship with the platform by default, in a fixed order, with normalized names. Level 3 returns an empty list.

## Compatibility impact
Level 2+ removes voices the user installed themselves; sites offering voice selection show fewer options. Flagged at level 2.

## Performance impact
None.

## Test cases
- Voice list identical on two machines with different language packs.
- Order is stable across calls.

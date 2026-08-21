# Building Bedrock

## Requirements

| | |
|---|---|
| Disk | ~100 GB free (Chromium checkout + one out/ dir) |
| RAM | 16 GB minimum, 32 GB comfortable |
| CPU | 8+ cores (first build: 3–8 h; incremental: minutes) |
| OS | Linux x64 reference; macOS/Windows follow once the Linux target is green |
| Tools | git, python3, and Chromium's build deps |

## Steps

```bash
git clone https://github.com/jonbarkert135-cpu/ettdgggder bedrock
cd bedrock

# 1. Fetch pinned Chromium + apply the Bedrock overlay (long; grab coffee)
python3 build/sync.py --workspace ~/bedrock-src

# 2. Linux only, once: install Chromium build dependencies
~/bedrock-src/src/build/install-build-deps.sh

# 3. Configure and build (sync.py prints these exact commands with paths filled in)
export PATH=~/bedrock-src/depot_tools:$PATH
cd ~/bedrock-src/src
gn gen out/Bedrock --args="$(grep -v '^#' ~/bedrock/build/args/bedrock-release.gn | tr '\n' ' ')"
autoninja -C out/Bedrock chrome

# 4. Run
./out/Bedrock/chrome
```

After changing `patches/` or `src_overrides/`, re-run the overlay only:

```bash
python3 build/sync.py --workspace ~/bedrock-src --overlay-only
```

## Rolling to a new Chromium

1. Update `build/chromium.pin` (version + 40-char commit) and the Chromium row in
   `docs/THIRD_PARTY.md` — `scripts/check_provenance.py` fails if they disagree.
2. `python3 build/sync.py --workspace ~/bedrock-src`
3. Fix any patch that fails to apply. Prefer converting the patch into an override file.

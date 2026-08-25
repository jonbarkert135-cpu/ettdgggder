# Renting a machine for the first build

[`BUILD.md`](BUILD.md) says *how* to build. This page answers the other question:
**what hardware, for how long, and what it costs** — because a full Chromium build
is the one step no laptop finishes overnight.

Nothing here is required to contribute. Every gate the project enforces
(`scripts/run_host_tests.sh`) runs on one core in minutes. This is only for
producing a running browser binary.

If renting is not an option, the build can be done on a developer laptop instead —
slowly, and with different arguments: [`BUILD.md` → Building on 8 GB](BUILD.md#building-on-8-gb).

## What the build actually needs

| Resource | Minimum | Why |
| --- | --- | --- |
| CPU cores | 16 (dedicated, not shared/burstable) | the reference build was 56 105 steps, 12 h 16 min on 17 cores |
| RAM | 32 GB, 64 GB comfortable | link steps are the peak; with less, cap jobs (`ninja -j`) instead of swapping |
| Disk | 150 GB free | ~80 GB checkout + ~10 GB `out/` + git objects and headroom |
| OS | Ubuntu 22.04+ / Debian 12+ x64 | what `install-build-deps.sh` targets |

Budget **~13 h of building on 16 cores** plus **~1–2 h** for `gclient sync` and
dependencies. Every rebuild after that is incremental: 30 s – 2 min per changed
overlay file plus the link.

## Prices checked 2026-08-24

All hourly, billed per hour, no setup fee. Prices move — re-check before ordering.

| Option | Spec | Price | Note |
| --- | --- | --- | --- |
| **Hetzner Cloud CCX43** | 16 dedicated AMD vCPU, 64 GB, 360 GB NVMe | **€0.4423/h**, capped at €275.99/mo | the recommended default; instant, deletable |
| Hetzner Cloud CCX53 | 32 dedicated vCPU, 128 GB, 600 GB NVMe | €0.8550/h | halves the build, costs the same overall |
| Hetzner server auction | e.g. EPYC 7502P 32C/64T, 32 GB, 960 GB SSD | €0.4006/h | cheapest per core, but offers rotate and setup is not instant |
| AWS `c7i.8xlarge` | 32 vCPU, 64 GB | $1.428/h on demand, ~$0.54/h spot | spot can be reclaimed mid-build |
| OVH `c3-64` | 32 vCPU, 64 GB, 400 GB NVMe | ~$1.17/h | |
| Scaleway POP2-HC-32C-64G | 32 vCPU, 64 GB | ~€1.18/h | |

**One full build on a CCX43 ≈ 15 h ≈ €7.** Deleting the server stops billing;
the monthly cap is the worst case if it is left running.

Avoid shared-vCPU plans (Hetzner CPX, AWS `t`-family). Thirteen hours at 100 %
CPU is exactly the workload their fair-use terms are written against.

## Exact sequence

```bash
# 1. On the provider: Ubuntu 24.04 image, 16+ dedicated cores, 150+ GB disk, your SSH key.
ssh root@YOUR_SERVER_IP

# 2. Basics the Chromium fetch needs before its own installer can run.
apt update && apt install -y git python3 curl lsb-release sudo file

# 3. Bedrock + the pinned Chromium tree (this is the long download, ~80 GB).
git clone https://github.com/jonbarkert135-cpu/ettdgggder bedrock
cd bedrock
python3 build/sync.py --workspace ~/bedrock-src

# 4. Chromium's own dependency list for the pinned revision.
~/bedrock-src/src/build/install-build-deps.sh --no-prompt

# 5. Configure with the reviewed release args (never hand-edited).
export PATH=~/bedrock-src/depot_tools:$PATH
cd ~/bedrock-src/src
gn gen out/Bedrock --args="$(grep -v '^#' ~/bedrock/build/args/bedrock-release.gn | tr '\n' ' ')"

# 6. Build. This is the 13 hours. Run it under tmux so an SSH drop does not kill it.
tmux new -s build
autoninja -C out/Bedrock chrome

# 7. Check that Bedrock is actually inside the binary, then run it.
nm -C out/Bedrock/chrome | grep -c 'bedrock::'
./out/Bedrock/chrome
```

Then, for every later change to `src_overrides/`:

```bash
cd ~/bedrock && git pull
python3 build/sync.py --workspace ~/bedrock-src --overlay-only
cd ~/bedrock-src/src && autoninja -C out/Bedrock chrome     # minutes, not hours
```

Never run `gn clean`, and never `gn gen` `out/Bedrock` with different args — both
throw away the expensive part.

## Keeping the build between sessions

`out/` is what costs 13 hours, and it is not in git (GitHub's limit is 100 MB per
file; binaries ship through Releases). Three ways to not pay for it twice:

1. **Keep the server** — cheapest if the next session is within days.
2. **Snapshot the disk before deleting** — provider-priced per GB-month, far below
   the running server.
3. **Rebuild with a warm `ccache`** kept on a cheap volume — a re-run then costs a
   fraction of the first, though `gclient sync` still has to run.

## Before renting anything

Land every overlay change you want in the binary *first*. Renting is charged by
the hour, so the goal is one session that syncs, builds, and verifies a batch —
not a rental per feature.

Expect **compile errors on first contact**: host tests build with the system
compiler, while the in-tree build adds `-fno-exceptions`, the `chromium-rawptr`
plugin and C++20 module rules that reject code `g++` accepts
([`BUILD.md`](BUILD.md) has the table). Those fixes are incremental — minutes
each — but budget a few hours for them in the first session.

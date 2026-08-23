#!/usr/bin/env python3
"""Compile one translation unit with the exact command Chromium's build would use.

Escape hatch for the siso scheduler stall documented in build/LOCAL_BUILD_HANDOFF.md
(siso spins without spawning a compiler). It reads the variable block of the target's
generated .ninja file and expands Chromium's `cxx` rule by hand.

Usage (from anywhere):
  python3 scripts/manual_compile.py <ninja-file> <source-path> <object-name>

Example:
  python3 scripts/manual_compile.py obj/bedrock/bedrock.ninja \
      ../../bedrock/integration/startup.cc startup

Paths are relative to /work/chromium/src/out/Release, which is also the cwd of the
compiler invocation. Prints "== <name> rc=<code> <seconds>s".
"""
import re, subprocess, os, time, sys
out = "/work/chromium/src/out/Release"
def vars_from(ninja):
    d = {}
    for line in open(os.path.join(out, ninja)):
        if line.startswith("build "):
            break
        m = re.match(r"^(\w+) = (.*)$", line.rstrip("\n"))
        if m:
            d[m.group(1)] = m.group(2)
    return d
TPL = ' ../../third_party/llvm-build/Release+Asserts/bin/clang++ -MMD -MF ${target_out_dir}/${label_name}/${source_name_part}.o.d ${defines} ${include_dirs} ${cflags} ${cflags_cc} ${module_deps} -fmodule-name="${cc_module_name}_Private" -c ${in} -o ${target_out_dir}/${label_name}/${source_name_part}.o'
ninja, src, name = sys.argv[1], sys.argv[2], sys.argv[3]
v = vars_from(ninja); v["source_name_part"] = name; v["in"] = src
cmd = re.sub(r"\$\{(\w+)\}", lambda m: v.get(m.group(1), ""), TPL)
t = time.time()
p = subprocess.run(cmd, shell=True, cwd=out, capture_output=True, text=True)
print(f"== {name} rc={p.returncode} {time.time()-t:.0f}s")
print(p.stdout[-3000:]); print(p.stderr[-6000:])

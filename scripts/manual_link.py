#!/usr/bin/env python3
"""Link out/Release/chrome by hand, reproducing the ninja `link` rule.

Companion to scripts/manual_compile.py; see build/LOCAL_BUILD_HANDOFF.md. Reads the
`build ./chrome: link ...` block of obj/chrome/chrome_initial.ninja and runs the same
clang++/mold command. Note that a failed link deletes the previous binary, and a cold
link takes ~20 min while a warm one takes ~15 s.

Usage: python3 scripts/manual_link.py    (prints "== link rc=<code> <seconds>s")
"""
import re, subprocess, os, time, sys
out = "/work/chromium/src/out/Release"
nf = os.path.join(out, "obj/chrome/chrome_initial.ninja")
lines = open(nf).read().split("\n")
i = next(i for i, l in enumerate(lines) if l.startswith("build ./chrome: link "))
v = {}
for l in lines[i+1:]:
    m = re.match(r"^  (\w+) = ?(.*)$", l)
    if not m:
        break
    v[m.group(1)] = m.group(2)
TPL = ('"python3" "../../build/toolchain/gcc_link_wrapper.py" '
       '--output="${output_dir}/${target_output_name}${output_extension}" -- '
       '../../third_party/llvm-build/Release+Asserts/bin/clang++ ${ldflags} '
       '-o "${output_dir}/${target_output_name}${output_extension}" -Wl,--start-group '
       '@"${output_dir}/${target_output_name}${output_extension}.rsp" -Wl,--end-group '
       '${solibs} ${libs} -Wl,--start-group ${rlibs} -Wl,--end-group')
v.setdefault("output_extension", "")
v["target_output_name"] = "chrome"
cmd = re.sub(r"\$\{(\w+)\}", lambda m: v.get(m.group(1), ""), TPL)
t = time.time()
p = subprocess.run(cmd, shell=True, cwd=out, capture_output=True, text=True)
print(f"== link rc={p.returncode} {time.time()-t:.0f}s")
print(p.stdout[-4000:]); print(p.stderr[-8000:])

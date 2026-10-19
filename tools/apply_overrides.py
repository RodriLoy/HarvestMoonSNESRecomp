#!/usr/bin/env python3
"""Post-generation overrides for Harvest Moon (recomp/generate.toml).

1. Names. Every compiled routine that HM-Decomp names is renamed from
   `bank_BB_AAAA_MmXx` to `Name_MmXx` (recomp/decomp_names.tsv, written by
   tools/import_decomp_names.py). Identifiers only: the program is unchanged.

2. Decompilation checks. Every routine registered in src/decomp/hm_decomp.c
   gets a wrapper: its compiled body is renamed `Name_MmXx__rom` and a new
   `Name_MmXx` calls it, and with HM_DECOMP_CHECK=1 also runs the hand-written
   rewrite on a snapshot and compares (src/decomp/hm_decomp.h).

3. Widescreen sprite clip.
   (WIDESCREEN_PATTERNS P8). PrepareOAMData ($85:83E0)
drops an object component whose screen X is outside [-16, 256):

    85:853B  CMP #$0100   ; BCC on-screen
    85:8540  CMP #$FFF0   ; BCS on-screen, else JMP not-on-screen

The recompiler folds both immediates into the C. This replaces the two
constants with calls that return exactly 0x0100 / 0xFFF0 unless widescreen
is on (src/hm_widescreen.c), so the 4:3 build behaves bit-for-bit as the ROM.
The edit is anchored on the instruction blocks' own trace PCs; if they are not
found (another ROM revision, a different code shape) the script fails loudly
instead of silently shipping a narrower clip.
"""
import argparse
import glob
import os
import re
import sys

DECL = ("\n/* hm overrides (tools/apply_overrides.py) */\n"
        "extern uint16 hm_ws_oam_right_limit(void);\n"
        "extern uint16 hm_ws_oam_left_limit(void);\n")

# (block trace PC suffix, constant in that block, replacement)
EDITS = [
    ("8537", "0x100", "hm_ws_oam_right_limit()"),
    ("8540", "0xfff0", "hm_ws_oam_left_limit()"),
]


def patch_text(text):
    count = 0
    for pc, const, repl in EDITS:
        pat = re.compile(r"(cpu_trace_block\(cpu, 0x[08]5%s\);.*?)"
                         r"(uint16 _v\d+ = )%s;" % (pc, re.escape(const)),
                         re.S)
        def sub(m):
            # stay inside this block: no new label between anchor and constant
            if re.search(r"\n  L_[0-9A-F]{4}_", m.group(1)):
                return m.group(0)
            return m.group(1) + m.group(2) + repl + ";"
        text, n = pat.subn(sub, text)
        count += n if repl in text else 0
    return text


FUNC_ID = re.compile(r"\bbank_([0-9a-f]{2})_([0-9A-F]{4})_(M[01]X[01])\b")


def load_names(path):
    names = {}
    if path and os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#"):
                pc, name = line.split("\t")
                names[int(pc, 16)] = name
    return names


def rename(gen_dir, names):
    if not names:
        return 0
    def sub(m):
        name = names.get(int(m.group(1), 16) << 16 | int(m.group(2), 16))
        return f"{name}_{m.group(3)}" if name else m.group(0)
    count = 0
    for path in sorted(glob.glob(os.path.join(gen_dir, "*.c")) + glob.glob(os.path.join(gen_dir, "*.h"))):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new = FUNC_ID.sub(sub, text)
        if new != text:
            with open(path, "w", encoding="utf-8") as f:
                f.write(new)
            count += 1
    return count


WRAP_DECL = """
/* hm decomp wrappers (tools/apply_overrides.py) */
extern int g_hm_decomp_check;
void *hm_decomp_check_begin(const struct CpuState *cpu);
void hm_decomp_check_end(const char *name, void *snapshot, const struct CpuState *cpu, int completed);
"""
WRAPPER = """
RecompReturn {fn}(CpuState *cpu) {{
  if (!g_hm_decomp_check) return {fn}__rom(cpu);
  void *_snap = hm_decomp_check_begin(cpu);
  RecompReturn _r = {fn}__rom(cpu);
  hm_decomp_check_end("{name}", _snap, cpu, _r == RECOMP_RETURN_NORMAL);
  return _r;
}}
"""


def decomp_registry(path):
    if not path or not os.path.exists(path):
        return []
    return re.findall(r'\{\s*"([A-Za-z_][A-Za-z0-9_]*)",\s*hm_', open(path, encoding="utf-8").read())


def wrap(gen_dir, routines):
    wrapped = 0
    for path in sorted(glob.glob(os.path.join(gen_dir, "bank*_v2.c"))):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        if "__rom(CpuState *cpu)" in text:
            continue                                    # already wrapped
        tail = []
        for name in routines:
            pat = re.compile(r"^RecompReturn ((?:%s|%s_lo)_M[01]X[01])\(CpuState \*cpu\) \{" % (name, name), re.M)
            for fn in sorted(set(pat.findall(text))):
                text = text.replace("RecompReturn %s(CpuState *cpu) {" % fn,
                                    "RecompReturn %s__rom(CpuState *cpu) {" % fn, 1)
                text = text.replace('#include "funcs.h"\n',
                                    '#include "funcs.h"\nRecompReturn %s__rom(CpuState *cpu);\n' % fn, 1)
                tail.append(WRAPPER.format(fn=fn, name=name))
                wrapped += 1
        if tail:
            text = text.replace('#include "funcs.h"\n', '#include "funcs.h"\n' + WRAP_DECL, 1)
            text += "".join(tail)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
    return wrapped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen-dir", required=True)
    ap.add_argument("--names", default=None, help="recomp/decomp_names.tsv")
    ap.add_argument("--decomp", default=None, help="src/decomp/hm_decomp.c (registry)")
    args = ap.parse_args()
    names = load_names(args.names)
    n = rename(args.gen_dir, names)
    print("apply_overrides: %d decomp names applied in %d files" % (len(names), n))
    routines = decomp_registry(args.decomp)
    w = wrap(args.gen_dir, routines)
    print("apply_overrides: %d decompiled routines, %d compiled variants wrapped" % (len(routines), w))
    missing = [r for r in routines if r not in names.values()]
    if missing:
        print("apply_overrides: decompiled routines with no compiled body: %s" % ", ".join(missing),
              file=sys.stderr)
    patched = 0
    for path in sorted(glob.glob(os.path.join(args.gen_dir, "bank*_v2.c"))):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        if "cpu_trace_block(cpu, 0x058537)" not in text and \
           "cpu_trace_block(cpu, 0x858537)" not in text:
            continue
        new = patch_text(text)
        if new == text:
            if "hm_ws_oam_right_limit()" in text:
                patched += 1          # already applied (idempotent)
            continue
        new = new.replace('#include "funcs.h"\n', '#include "funcs.h"\n' + DECL, 1)
        if "hm_ws_oam_right_limit()" not in new or "hm_ws_oam_left_limit()" not in new:
            print("apply_overrides: OAM clip anchors incomplete in %s" % path, file=sys.stderr)
            return 1
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
        patched += 1
        print("apply_overrides: widescreen OAM clip -> %s" % os.path.basename(path))
    if not patched:
        print("apply_overrides: PrepareOAMData clip ($85:853B) not found in generated code",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

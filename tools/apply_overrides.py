#!/usr/bin/env python3
"""Post-generation overrides for Harvest Moon (recomp/generate.toml).

Widescreen sprite clip (WIDESCREEN_PATTERNS P8). PrepareOAMData ($85:83E0)
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen-dir", required=True)
    args = ap.parse_args()
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

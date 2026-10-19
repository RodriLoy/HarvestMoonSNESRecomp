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

Endless game (src/hm_endless.c): the ending's date check and the diary's
year ordinal table, see EDITS below. Every hook returns the ROM's own value
unless its feature needs otherwise.
"""
import argparse
import glob
import os
import re
import sys

DECL = ("\n/* hm overrides (tools/apply_overrides.py) */\n"
        "extern uint16 hm_ws_oam_right_limit(void);\n"
        "extern uint16 hm_ws_oam_left_limit(void);\n"
        "extern uint8 hm_endless_ending_day(void);\n"
        "extern uint16 hm_year_suffix(uint16 x, uint16 rom_value);\n")

# Each edit: name, regex over a generated bank file, replacement builder,
# marker proving it is applied, minimum number of sites across all files.
def _in_block(m):
    """Stay inside the anchored block: no new label before the constant."""
    return not re.search(r"\n  L_[0-9A-F]{4}_", m.group(1))


def _const_edit(pc, const, repl):
    pat = re.compile(r"(cpu_trace_block\(cpu, 0x[08]%s\);.*?)(uint(?:8|16) _v\d+ = )%s;"
                     % (pc, re.escape(const)), re.S | re.I)
    def sub(m):
        return m.group(1) + m.group(2) + repl + ";" if _in_block(m) else m.group(0)
    return pat, sub


SUFFIX_READ = ("cpu_read16(cpu, (uint8)((((uint32)0x82f298 + (uint32)cpu->X)) >> 16), "
               "(uint16)(((uint32)0x82f298 + (uint32)cpu->X)))")

EDITS = [
    # Widescreen sprite clip, PrepareOAMData $85:853B / $85:8540.
    ("oam right", *_const_edit("58537", "0x100", "hm_ws_oam_right_limit()"),
     "hm_ws_oam_right_limit()", 1),
    ("oam left", *_const_edit("58540", "0xfff0", "hm_ws_oam_left_limit()"),
     "hm_ws_oam_left_limit()", 1),
    # Endless game: the house map handler's "day == 30" ($83:DEEB CMP #$1E)
    # that, with Year 3 / Summer, starts the ending (src/hm_endless.c).
    ("ending day", *_const_edit("3DEE7", "0x1e", "hm_endless_ending_day()"),
     "hm_endless_ending_day()", 1),
    # Endless game: diary ordinal table $82:F298 has only 1st..3rd.
    ("year suffix", re.compile(r"(?<!hm_year_suffix\(cpu->X, )" + re.escape(SUFFIX_READ)),
     lambda m: "hm_year_suffix(cpu->X, %s)" % m.group(0),
     "= hm_year_suffix(cpu->X", 2),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gen-dir", required=True)
    args = ap.parse_args()
    found = {name: 0 for name, *_ in EDITS}
    for path in sorted(glob.glob(os.path.join(args.gen_dir, "bank*_v2.c"))):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        new = text
        for name, pat, sub, marker, _ in EDITS:
            if marker in new and not pat.search(new):
                found[name] += new.count(marker)       # already applied
                continue
            new, n = pat.subn(sub, new)
            found[name] += new.count(marker)
        if new != text:
            if "hm overrides (tools/apply_overrides.py)" not in new:
                new = new.replace('#include "funcs.h"\n', '#include "funcs.h"\n' + DECL, 1)
            with open(path, "w", encoding="utf-8") as f:
                f.write(new)
            print("apply_overrides: %s" % os.path.basename(path))
    bad = [n for n, *_, need in EDITS if found[n] < need]
    for n, *_, need in EDITS:
        print("apply_overrides: %-12s %d site(s)" % (n, found[n]))
    if bad:
        print("apply_overrides: anchors not found: %s" % ", ".join(bad), file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())

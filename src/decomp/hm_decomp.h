/*
 * hm_decomp.h — the hand-written decompilation of Harvest Moon.
 *
 * The recompiler already turns every routine into C, but that C is a literal
 * translation of 65816 instructions. The decompilation rewrites routines as
 * C a person can read, named after what they do, using the names in
 * hm_ram.h. Each routine lives in src/decomp/<system>.c.
 *
 * How we know a rewrite is right (differential checking): every decompiled
 * routine has a generated wrapper (tools/apply_overrides.py) around the
 * recompiled original. Run the game with HM_DECOMP_CHECK=1 and, on every
 * call, the wrapper snapshots RAM and registers, lets the original run, then
 * runs the rewrite on the snapshot and compares the results byte for byte.
 * Any difference is logged with the inputs. Without HM_DECOMP_CHECK the
 * original runs alone, so the game is unchanged.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The CPU state a routine sees and returns. Rewrites read the inputs they
 * use and write the outputs they produce; `out_mask` says which outputs the
 * checker compares. */
typedef struct HmRegs {
  uint16_t a, x, y;
  uint8_t c;          /* carry flag */
  uint8_t db;         /* data bank */
} HmRegs;

enum { HM_OUT_A = 1, HM_OUT_X = 2, HM_OUT_Y = 4, HM_OUT_C = 8 };

typedef struct HmDecompFn {
  const char *name;                       /* HM-Decomp label */
  void (*fn)(uint8_t *ram, HmRegs *r);    /* the rewrite: ram is WRAM (128 KB) */
  unsigned out_mask;                      /* outputs to compare */
  const char *source;                     /* where it is written */
} HmDecompFn;

/* Registry, defined in hm_decomp.c. */
extern const HmDecompFn g_hm_decomp_fns[];
extern const int g_hm_decomp_count;

/* Check mode (HM_DECOMP_CHECK=1); read by the generated wrappers. */
extern int g_hm_decomp_check;

struct CpuState;
void *hm_decomp_check_begin(const struct CpuState *cpu);
void hm_decomp_check_end(const char *name, void *snapshot, const struct CpuState *cpu,
                         int completed);

/* ── The decompiled routines (one line each; see the .c files) ───────── */
void hm_GetRNG(uint8_t *ram, HmRegs *r);

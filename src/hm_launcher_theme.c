/* Harvest Moon look for the launcher.
 *
 * The shared launcher (recomp-ui) has one layout and a palette per console.
 * This file gives it a palette of its own through gi->custom_theme: a warm
 * night-on-the-farm look, with the harvest moon's amber as the main accent
 * and leaf green for headings. Layout, sizes and fonts stay the shared ones.
 *
 * Colors are written as 0xRRGGBB so they are easy to tweak by eye. */
#include "recomp_launcher.h"
#include "common/launcher_theme.h"
#include "hm_launcher_theme.h"

static LngColor hex(unsigned rgb) {
  return lng_rgba(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f,
                  (rgb & 0xFF) / 255.0f, 1.0f);
}

static LauncherTheme g_theme;

void hm_configure_launcher(struct RecompLauncherCGameInfo *gi) {
  LauncherTheme t = launcher_theme_default(); /* spacing, fonts, sizes */
  t.background      = hex(0x13160F); /* night field */
  t.background2     = hex(0x1C2216); /* lifted centre of the vignette */
  t.panel           = hex(0x1E2418);
  t.panel_hovered   = hex(0x28311F);
  t.control         = hex(0x2A3220);
  t.control_hovered = hex(0x354029);
  t.border          = hex(0x3F4A2E);
  t.accent          = hex(0xF2B33D); /* harvest moon amber */
  t.accent_dim      = hex(0xC98A1F);
  t.accent_text     = hex(0x1A1206); /* dark text on amber buttons */
  t.accent2         = hex(0x8FCB5A); /* leaf green headings */
  t.text            = hex(0xF3EEDC); /* cream */
  t.text_muted      = hex(0xA9A58C);
  t.good            = hex(0x8FCB5A);
  t.warn            = hex(0xE8743B); /* barn red-orange */
  t.focus_ring      = hex(0xF7E08A); /* moonlight */
  t.scanline        = lng_rgba(0, 0, 0, 0);
  t.scanlines       = 0;             /* a picture book, not a CRT */
  g_theme = t;
  gi->custom_theme = &g_theme;
  /* Launcher window icon on Linux/macOS (Windows uses the .exe's own icon,
   * src/hm_app.rc). Relative paths resolve against the working directory,
   * which the host sets to the program's folder. */
  if (!gi->window_icon_path || !gi->window_icon_path[0])
    gi->window_icon_path = "assets/img/icon.png";
}

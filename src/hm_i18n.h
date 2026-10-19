#ifndef HM_I18N_H
#define HM_I18N_H

/* Harvest Moon's own translatable text (pause menu, mod feature names).
 * English is the source; translations are registered with the launcher's
 * localization (hm_i18n.c), so the language the player picks in the
 * launcher applies to the game's menus too. */

const char *launcher_i18n_text(const char *english);   /* recomp-ui */
#define T(s) launcher_i18n_text(s)

#endif

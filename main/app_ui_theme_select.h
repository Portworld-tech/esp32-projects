/**
 * Compile-time UI theme selection — edit ONLY `APP_UI_THEME_ID` below.
 *
 * Then rebuild (theme switch changes linked sources):
 *   idf.py fullclean && idf.py build
 * Or at least:
 *   idf.py reconfigure && idf.py build
 *
 * Packs live under ui/themes/<name>/  (see docs/UI_THEME_PACKS.md)
 */
#pragma once

#define APP_UI_THEME_DEFAULT  0
#define APP_UI_THEME_SLATE    1
#define APP_UI_THEME_SAND     2
#define APP_UI_THEME_INK      3
#define APP_UI_THEME_FOREST   4
#define APP_UI_THEME_DUSK     5
#define APP_UI_THEME_OCEAN    6
#define APP_UI_THEME_ZEN      7
#define APP_UI_THEME_PULSE    8
#define APP_UI_THEME_BLOOM    9
#define APP_UI_THEME_METRO    10

/* ===== select theme here (one of APP_UI_THEME_* above) ===== */
#define APP_UI_THEME_ID APP_UI_THEME_BLOOM 

#if (APP_UI_THEME_ID) == (APP_UI_THEME_DEFAULT)
#define APP_UI_THEME_IS_DEFAULT 1
#else
#define APP_UI_THEME_IS_HUB 1
#endif

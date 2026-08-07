# UI theme packs (LVGL 8)

Compile-time themes live under `themes/`.

| Pack | Path | Kind |
|------|------|------|
| **default** | [`themes/default/`](themes/default/) | SquareLine product UI + `main/ui_runtime` |
| slate … metro | `themes/<id>/` | Hub UI; shared pages in `main/hub_ui/` |

**选型：** 编辑 [`main/app_ui_theme_select.h`](../main/app_ui_theme_select.h) 的 `APP_UI_THEME_ID`，再 `idf.py reconfigure && idf.py build`。详见 [`docs/UI_THEME_PACKS.md`](../docs/UI_THEME_PACKS.md)。

Board pins are **not** per-theme; see `config/sdkconfig.board.395hsd.defaults`.

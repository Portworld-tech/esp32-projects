#!/usr/bin/env python3
"""Generate self-contained theme_ui.c for all hub themes from slate + home overrides."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "ui" / "themes"
SLATE = (THEMES / "slate" / "theme_ui.c").read_text(encoding="utf-8")

HELPERS = re.search(r"(.*?)/\* ── HOME", SLATE, re.S).group(1)
PAGES = re.search(r"/\* ── ROOM ──.*", SLATE, re.S).group(0)


def wrap(name: str, home_fn: str) -> str:
    return (
        f"/**\n * {name} theme pack — self-contained UI (home + all routes).\n"
        f" * No shared hub_pages_*; events mutate hub_model then refresh/navigate.\n */\n"
        + HELPERS
        + "/* ── HOME ────────────────────────────────────────────────────────── */\n\n"
        + home_fn.strip()
        + "\n\n"
        + PAGES
    )


HOMES: dict[str, str] = {}

HOMES["sand"] = r'''
static void build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 18, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lbl(root, "SAND HUB", p->accent, hub_font());
    lbl(root, "15:30", p->t1, hub_font_clock_lg());
    char clim[64];
    snprintf(clim, sizeof(clim), "室内 %.1f° · 湿度 %d%% · %.1f kW",
             (double)m->indoor_c, m->rh, (double)m->power_kw);
    lbl(root, clim, p->t3, hub_font());
    lv_obj_t *pwr = lv_btn_create(root);
    lv_obj_remove_style_all(pwr);
    hub_apply_card(pwr, false);
    lv_obj_set_size(pwr, 72, 72);
    lv_obj_set_style_radius(pwr, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(pwr, LV_ALIGN_TOP_RIGHT, -18, 18);
    lv_obj_add_event_cb(pwr, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)HUB_ROUTE_ENERGY);
    lbl(pwr, LV_SYMBOL_CHARGE, p->accent, hub_font());
    char pb[16];
    snprintf(pb, sizeof(pb), "%.1f", (double)m->power_kw);
    lv_obj_t *pv = lv_label_create(pwr);
    lv_label_set_text(pv, pb);
    hub_style_label(pv, p->t1, hub_font());
    lv_obj_align(pv, LV_ALIGN_CENTER, 0, 12);
    const char *subs[] = {"空调 · 窗帘 · 灯", "地暖 · 灯光", "插座 · 水阀", "灯光 · 窗帘"};
    for (int i = 0; i < 4; i++) {
        bool on = hub_model_room_active(i);
        lv_obj_t *row = lv_btn_create(root);
        lv_obj_remove_style_all(row);
        hub_apply_card(row, on);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 56);
        lv_obj_add_event_cb(row, room_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *n = lv_label_create(row);
        lv_label_set_text(n, hub_model_room_name(i));
        hub_style_label(n, p->t1, hub_font());
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, -10);
        lv_obj_t *d = lv_label_create(row);
        lv_label_set_text(d, subs[i]);
        hub_style_label(d, p->t3, hub_font());
        lv_obj_align(d, LV_ALIGN_LEFT_MID, 4, 10);
        lv_obj_t *st = lv_label_create(row);
        lv_label_set_text(st, on ? "运行" : "待机");
        hub_style_label(st, on ? p->on : p->t4, hub_font());
        lv_obj_align(st, LV_ALIGN_RIGHT_MID, -4, 0);
    }
    quickbar(root);
    lv_obj_t *cta = lv_obj_create(root);
    lv_obj_remove_style_all(cta);
    lv_obj_set_width(cta, LV_PCT(100));
    lv_obj_set_height(cta, 44);
    lv_obj_set_flex_flow(cta, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(cta, 8, 0);
    lv_obj_t *h = lv_btn_create(cta);
    lv_obj_remove_style_all(h);
    hub_apply_card(h, true);
    lv_obj_set_style_bg_color(h, p->accent, 0);
    lv_obj_set_flex_grow(h, 1);
    lv_obj_set_height(h, LV_PCT(100));
    lv_obj_add_event_cb(h, scene_cb, LV_EVENT_CLICKED, (void *)"home");
    lv_obj_t *hl = lv_label_create(h);
    lv_label_set_text(hl, "回家");
    hub_style_label(hl, p->ink_on, hub_font());
    lv_obj_center(hl);
    lv_obj_t *a = lv_btn_create(cta);
    lv_obj_remove_style_all(a);
    hub_apply_card(a, false);
    lv_obj_set_flex_grow(a, 1);
    lv_obj_set_height(a, LV_PCT(100));
    lv_obj_add_event_cb(a, scene_cb, LV_EVENT_CLICKED, (void *)"away");
    lv_obj_t *al = lv_label_create(a);
    lv_label_set_text(al, "离家");
    hub_style_label(al, p->t1, hub_font());
    lv_obj_center(al);
}
'''

HOMES["ink"] = r'''
static void build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    hub_room_dev_t *d = &m->rooms[0];
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *rail = lv_obj_create(root);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, 112, 480);
    lv_obj_set_style_pad_all(rail, 12, 0);
    lv_obj_set_style_pad_row(rail, 8, 0);
    lv_obj_set_style_border_width(rail, 1, 0);
    lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(rail, p->line, 0);
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lbl(rail, "INK", p->accent, hub_font());
    lbl(rail, "15:30", p->t1, hub_font_clock());
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_btn_create(rail);
        lv_obj_remove_style_all(b);
        lv_obj_add_event_cb(b, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)HUB_ROUTE_GATEWAY);
        lbl(b, m->protos[i].name, m->protos[i].ok ? p->on : p->alert, hub_font());
    }
    lv_obj_t *pwr = btn_go(rail, "PWR", HUB_ROUTE_ENERGY, true);
    lv_obj_set_width(pwr, LV_PCT(100));
    lv_obj_set_height(pwr, 36);
    lv_obj_t *mainw = lv_obj_create(root);
    lv_obj_remove_style_all(mainw);
    lv_obj_set_flex_grow(mainw, 1);
    lv_obj_set_height(mainw, 480);
    lv_obj_set_style_pad_all(mainw, 12, 0);
    lv_obj_set_style_pad_row(mainw, 8, 0);
    lv_obj_set_flex_flow(mainw, LV_FLEX_FLOW_COLUMN);
    lbl(mainw, "ZONE MATRIX", p->t1, hub_font());
    lv_obj_t *grid = lv_obj_create(mainw);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    const char *cells[] = {"L1", "L2", "AC", "HEAT", "CUR", "SHUT", "FAN", "PUMP", "VALVE"};
    bool ons[] = {d->ceiling, d->strip > 0, d->ac_on, d->heat_on, d->curtain > 50,
                  d->curtain > 80, d->ac_on, d->heat_on, d->ceiling};
    for (int i = 0; i < 9; i++) {
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, ons[i]);
        if (ons[i]) lv_obj_set_style_bg_color(b, p->accent, 0);
        lv_obj_set_style_radius(b, p->radius, 0);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
        lv_obj_add_event_cb(b, zone_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, cells[i]);
        hub_style_label(l, ons[i] ? p->ink_on : p->t1, hub_font());
        lv_obj_center(l);
    }
    lv_obj_t *go = lv_btn_create(mainw);
    lv_obj_remove_style_all(go);
    hub_apply_card(go, false);
    lv_obj_set_width(go, LV_PCT(100));
    lv_obj_set_height(go, 40);
    lv_obj_add_event_cb(go, room_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)0);
    lv_obj_t *gl = lv_label_create(go);
    lv_label_set_text(gl, "客厅 →");
    hub_style_label(gl, p->t1, hub_font());
    lv_obj_center(gl);
    quickbar(mainw);
}
'''


def generic_home(title: str, note: str) -> str:
    return f'''
static void build_home(lv_obj_t *parent)
{{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 14, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *head = lv_obj_create(root);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 40);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lbl(head, "15:30", p->t1, hub_font_clock());
    lbl(head, "{title}", p->t1, hub_font());
    lbl(head, m->armed ? "ARMED" : "HOME", m->armed ? p->on : p->t3, hub_font());
    lbl(root, "{note}", p->t3, hub_font());
    char line[64];
    snprintf(line, sizeof(line), "%.1fkW · 室内 %.1f° · %d%%",
             (double)m->power_kw, (double)m->indoor_c, m->rh);
    lbl(root, line, p->accent, hub_font());
    lv_obj_t *grid = lv_obj_create(root);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    static lv_coord_t cols[] = {{ LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST }};
    static lv_coord_t rows[] = {{ LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST }};
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < 4; i++) {{
        bool on = hub_model_room_active(i);
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, on);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(b, room_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lbl(b, LV_SYMBOL_HOME, p->accent, hub_font());
        lbl(b, hub_model_room_name(i), p->t1, hub_font());
        lbl(b, on ? "运行" : "待机", on ? p->on : p->t3, hub_font());
    }}
    lv_obj_t *sc = lv_obj_create(root);
    lv_obj_remove_style_all(sc);
    lv_obj_set_width(sc, LV_PCT(100));
    lv_obj_set_height(sc, 40);
    lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(sc, 8, 0);
    const char *ids[] = {{"home", "movie", "sleep", "away"}};
    const char *labs[] = {{"回家", "观影", "睡眠", "离家"}};
    for (int i = 0; i < 4; i++) {{
        bool on = strcmp(m->active_scene, ids[i]) == 0;
        lv_obj_t *b = lv_btn_create(sc);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, on);
        if (on) lv_obj_set_style_bg_color(b, p->accent, 0);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, LV_PCT(100));
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)ids[i]);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, labs[i]);
        hub_style_label(l, on ? p->ink_on : p->t1, hub_font());
        lv_obj_center(l);
    }}
    quickbar(root);
}}
'''


def main() -> None:
    # slate already hand-written
    specs = {
        "sand": HOMES["sand"],
        "ink": HOMES["ink"],
        "forest": generic_home("Forest", "Eco ring · scene capsules"),
        "dusk": generic_home("Dusk", "Evening glass · security"),
        "ocean": generic_home("Ocean Hub", "Ops · protocol health"),
        "zen": generic_home("ZEN", "Focus room · minimal"),
        "pulse": generic_home("PULSE // NODE", "HUD telemetry"),
        "bloom": generic_home("Bloom", "Soft bubbles"),
        "metro": generic_home("Metro Hub", "Color mosaic"),
    }
    for tid, home in specs.items():
        out = THEMES / tid / "theme_ui.c"
        text = wrap(tid.capitalize(), home)
        out.write_text(text, encoding="utf-8", newline="\n")
        print("wrote", out)


if __name__ == "__main__":
    main()

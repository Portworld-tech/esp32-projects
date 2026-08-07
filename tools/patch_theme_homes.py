#!/usr/bin/env python3
import re
from pathlib import Path

FOREST = r"""
static void build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 14, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *head = lv_obj_create(root);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 36);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lbl(head, "15:30", p->t1, hub_font());
    lbl(head, "Forest", p->t1, hub_font());
    lv_obj_t *eco = btn_go(head, "Eco", HUB_ROUTE_ENERGY, true);
    lv_obj_set_size(eco, 56, 28);
    lv_obj_t *ring = lv_btn_create(root);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 148, 148);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 10, 0);
    lv_obj_set_style_border_color(ring, p->accent, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(ring, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)HUB_ROUTE_ENERGY);
    lbl(ring, "今日用电", p->t3, hub_font());
    lv_obj_align(lv_obj_get_child(ring, 0), LV_ALIGN_CENTER, 0, -28);
    char kwh[16];
    snprintf(kwh, sizeof(kwh), "%.1f", (double)(12.0 + m->power_kw * 2));
    lv_obj_t *kv = lv_label_create(ring);
    lv_label_set_text(kv, kwh);
    hub_style_label(kv, p->t1, hub_font_clock());
    lv_obj_align(kv, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *ku = lv_label_create(ring);
    lv_label_set_text(ku, "kWh");
    hub_style_label(ku, p->t3, hub_font());
    lv_obj_align(ku, LV_ALIGN_CENTER, 0, 28);
    lv_obj_t *stats = lv_obj_create(root);
    lv_obj_remove_style_all(stats);
    lv_obj_set_width(stats, LV_PCT(100));
    lv_obj_set_height(stats, 48);
    lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char ib[16], rb[16];
    snprintf(ib, sizeof(ib), "%.0f°", (double)m->indoor_c);
    snprintf(rb, sizeof(rb), "%d%%", m->rh);
    const char *vals[] = { ib, rb, hub_model_ok_count() == HUB_PROTO_COUNT ? "OK" : "CHK" };
    const char *labs[] = { "室内", "湿度", "总线" };
    hub_route_t rts[] = { HUB_ROUTE_ROOM, HUB_ROUTE_ROOM, HUB_ROUTE_GATEWAY };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_btn_create(stats);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 90, 48);
        lv_obj_add_event_cb(b, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)rts[i]);
        lv_obj_t *v = lv_label_create(b);
        lv_label_set_text(v, vals[i]);
        hub_style_label(v, (i == 2 && hub_model_ok_count() == HUB_PROTO_COUNT) ? p->on : p->t1, hub_font());
        lv_obj_align(v, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, labs[i]);
        hub_style_label(l, p->t3, hub_font());
        lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, -2);
    }
    lv_obj_t *scenes = lv_obj_create(root);
    lv_obj_remove_style_all(scenes);
    lv_obj_set_width(scenes, LV_PCT(100));
    lv_obj_set_flex_grow(scenes, 1);
    lv_obj_set_flex_flow(scenes, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(scenes, 8, 0);
    lv_obj_set_style_pad_column(scenes, 8, 0);
    const char *ids[] = {"morning", "guest", "movie", "sleep", "eco", "alloff"};
    const char *slabs[] = {"晨起", "会客", "影院", "睡眠模式", "节能", "全关"};
    for (int i = 0; i < 6; i++) {
        bool on = strcmp(m->active_scene, ids[i]) == 0;
        lv_obj_t *b = lv_btn_create(scenes);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, on);
        if (on) {
            lv_obj_set_style_bg_color(b, p->accent, 0);
        }
        lv_obj_set_style_radius(b, 999, 0);
        lv_obj_set_height(b, 36);
        lv_obj_set_style_pad_hor(b, 12, 0);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)ids[i]);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, slabs[i]);
        hub_style_label(l, on ? p->ink_on : p->t1, hub_font());
        lv_obj_center(l);
    }
    quickbar(root);
}
"""

METRO = r"""
static void build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 8, 0);
    lv_obj_set_style_pad_row(root, 6, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *head = lv_obj_create(root);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_height(head, 36);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lbl(head, "15:30", p->t1, hub_font());
    lbl(head, "Metro Hub", p->t1, hub_font());
    lv_obj_t *set = lv_btn_create(head);
    lv_obj_remove_style_all(set);
    lv_obj_add_event_cb(set, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)HUB_ROUTE_SETTINGS);
    lbl(set, "设置", p->t2, hub_font());
    lv_obj_t *grid = lv_obj_create(root);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(125), LV_GRID_FR(100), LV_GRID_FR(100), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    char sub0[16], sub1[16], sub_e[16], sub_b[16];
    snprintf(sub0, sizeof(sub0), "%s", hub_model_room_active(0) ? "运行" : "待机");
    snprintf(sub1, sizeof(sub1), "%s", hub_model_room_active(1) ? "运行" : "待机");
    snprintf(sub_e, sizeof(sub_e), "%.1f kW", (double)m->power_kw);
    snprintf(sub_b, sizeof(sub_b), "%d/%d", hub_model_ok_count(), HUB_PROTO_COUNT);
    const char *labels[] = {"客厅", "主卧", "厨房", "情景", "总线", "能耗", "安防", "设置"};
    const char *subs[] = {sub0, sub1, "4 pts", m->active_scene, sub_b, sub_e, m->armed ? "布防" : "撤防", "CFG"};
    lv_color_t colors[] = {
        LV_COLOR_MAKE(0x00, 0xbc, 0xf2), LV_COLOR_MAKE(0x87, 0x64, 0xb8), LV_COLOR_MAKE(0x10, 0x7c, 0x10),
        LV_COLOR_MAKE(0xff, 0x8c, 0x00), LV_COLOR_MAKE(0xe7, 0x48, 0x56), LV_COLOR_MAKE(0x00, 0x78, 0xd4),
        LV_COLOR_MAKE(0x5d, 0x5a, 0x58), LV_COLOR_MAKE(0x00, 0x78, 0xd4),
    };
    uint8_t cs[] = {2, 1, 1, 1, 1, 1, 1, 1};
    uint8_t col[] = {0, 2, 0, 1, 2, 0, 1, 2};
    uint8_t row[] = {0, 0, 1, 1, 1, 2, 2, 2};
    int rooms[] = {0, 1, 2, -1, -1, -1, -1, -1};
    hub_route_t routes[] = {
        HUB_ROUTE_ROOM, HUB_ROUTE_ROOM, HUB_ROUTE_ROOM, HUB_ROUTE_SCENES,
        HUB_ROUTE_GATEWAY, HUB_ROUTE_ENERGY, HUB_ROUTE_SECURITY, HUB_ROUTE_SETTINGS,
    };
    for (int i = 0; i < 8; i++) {
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        lv_obj_set_style_bg_color(b, colors[i], 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, col[i], cs[i], LV_GRID_ALIGN_STRETCH, row[i], 1);
        if (rooms[i] >= 0) {
            lv_obj_add_event_cb(b, room_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)rooms[i]);
        } else {
            lv_obj_add_event_cb(b, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)routes[i]);
        }
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, labels[i]);
        hub_style_label(n, lv_color_hex(0xffffff), cs[i] == 2 ? hub_font_clock() : hub_font());
        lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 10, -22);
        lv_obj_t *s = lv_label_create(b);
        lv_label_set_text(s, subs[i]);
        hub_style_label(s, lv_color_hex(0xffffff), hub_font());
        lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 10, -4);
    }
    quickbar(root);
}
"""


def patch(tid: str, home: str) -> None:
    path = Path(f"e:/show/lvglframe/ui/themes/{tid}/theme_ui.c")
    text = path.read_text(encoding="utf-8")
    text2, n = re.subn(
        r"static void build_home\(lv_obj_t \*parent\)\s*\{.*?\n\}\n\n/\* ── ROOM",
        home.strip() + "\n\n/* ── ROOM",
        text,
        count=1,
        flags=re.S,
    )
    if n != 1:
        raise SystemExit(f"failed {tid} n={n}")
    path.write_text(text2, encoding="utf-8", newline="\n")
    print("patched", tid)


if __name__ == "__main__":
    patch("forest", FOREST)
    patch("metro", METRO)

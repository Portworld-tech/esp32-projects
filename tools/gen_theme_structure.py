#!/usr/bin/env python3
"""Split ui/themes/<id>/theme.c into a clear multi-file pack (project-style layout).

Layout (same for all 10 hub themes; themes do not include each other):
  theme_local.h   — pack-private API
  palette.c       — colors
  theme_local.c   — callbacks / chrome / helpers
  home.c          — home IA
  pages_room.c    — room widgets
  pages_scenes.c  — scenes (theme-styled)
  pages_ops.c     — energy / gateway / network / points
  pages_life.c    — settings / security / schedule / hvac
  boot.c          — hub_theme_build + app_ui_start

Also deepens scenes/ops/life chrome per theme language.
Run: python tools/gen_theme_structure.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THEMES = ROOT / "ui" / "themes"
THEME_IDS = (
    "slate",
    "sand",
    "ink",
    "forest",
    "dusk",
    "ocean",
    "zen",
    "pulse",
    "bloom",
    "metro",
)

INCLUDES = """\
#include "theme_local.h"
#include "hub_ui.h"
#include "hub_theme.h"
#include "hub_font.h"
#include "hub_model.h"
#include "hub_icons.h"
#include "app_ui.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
"""

LOCAL_H = """\
/**
 * {name} theme pack — private API (not shared across themes).
 */
#pragma once

#include "hub_ui.h"
#include "hub_theme.h"
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {{
#endif

/* events */
void go_cb(lv_event_t *e);
void scene_cb(lv_event_t *e);
void proto_cb(lv_event_t *e);
void room_cb(lv_event_t *e);
void arm_cb(lv_event_t *e);
void zone_cb(lv_event_t *e);
void toggle_dev_cb(lv_event_t *e);
void step_ac_cb(lv_event_t *e);
void step_curtain_cb(lv_event_t *e);
void strip_step_cb(lv_event_t *e);
void curtain_set_cb(lv_event_t *e);

/* helpers */
lv_obj_t *lbl(lv_obj_t *par, const char *txt, lv_color_t c, const lv_font_t *f);
lv_obj_t *btn_go(lv_obj_t *par, const char *txt, hub_route_t r, bool accent);
void quickbar(lv_obj_t *par);
void theme_card_chrome(lv_obj_t *obj, bool active);

/* pages */
void build_home(lv_obj_t *parent);
void build_room(lv_obj_t *parent);
void build_scenes(lv_obj_t *parent);
void build_energy(lv_obj_t *parent);
void build_gateway(lv_obj_t *parent);
void build_network(lv_obj_t *parent);
void build_points(lv_obj_t *parent);
void build_settings(lv_obj_t *parent);
void build_security(lv_obj_t *parent);
void build_schedule(lv_obj_t *parent);
void build_hvac(lv_obj_t *parent);

#ifdef __cplusplus
}}
#endif
"""

# Theme-specific scenes page (deepen)
SCENES = {
    "metro": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct {
        const char *id;
        const char *name;
        lv_color_t color;
    } sc[] = {
        { "home", "回家", LV_COLOR_MAKE(0x00, 0xbc, 0xf2) },
        { "away", "离家", LV_COLOR_MAKE(0xe7, 0x48, 0x56) },
        { "movie", "观影", LV_COLOR_MAKE(0x87, 0x64, 0xb8) },
        { "sleep", "睡眠", LV_COLOR_MAKE(0x5d, 0x5a, 0x58) },
        { "guest", "会客", LV_COLOR_MAKE(0x10, 0x7c, 0x10) },
        { "eco", "节能", LV_COLOR_MAKE(0x00, 0x78, 0xd4) },
    };
    lv_obj_t *grid = lv_obj_create(body);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_bg_color(b, sc[i].color, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        if (active) {
            lv_obj_set_style_border_width(b, 3, 0);
            lv_obj_set_style_border_color(b, p->t1, 0);
        }
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, lv_color_white(), hub_font());
        lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    }
}
''',
    "zen": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);
    lv_obj_set_style_pad_row(body, 14, 0);

    struct { const char *id; const char *name; } sc[] = {
        { "home", "回家" }, { "away", "离家" }, { "movie", "观影" },
        { "sleep", "睡眠" }, { "guest", "会客" }, { "eco", "节能" },
    };
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(body);
        lv_obj_remove_style_all(b);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, 48);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, p->line, 0);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, active ? p->accent : p->t1, hub_font());
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
        if (active) {
            lv_obj_t *dot = lv_obj_create(b);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, p->accent, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_align(dot, LV_ALIGN_RIGHT_MID, -4, 0);
        }
    }
}
''',
    "forest": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct { const char *id; const char *name; } sc[] = {
        { "home", "晨起" }, { "guest", "会客" }, { "movie", "影院" },
        { "sleep", "睡眠" }, { "eco", "节能" }, { "alloff", "全关" },
    };
    lv_obj_t *wrap = lv_obj_create(body);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_width(wrap, LV_PCT(100));
    lv_obj_set_flex_grow(wrap, 1);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(wrap, 10, 0);
    lv_obj_set_style_pad_column(wrap, 10, 0);
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(wrap);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        lv_obj_set_style_radius(b, 999, 0);
        if (active) {
            lv_obj_set_style_bg_color(b, p->accent, 0);
        }
        lv_obj_set_size(b, 140, 44);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, active ? p->ink_on : p->t1, hub_font());
        lv_obj_center(n);
    }
}
''',
    "bloom": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct {
        const char *id;
        const char *name;
        const char *sub;
        lv_color_t hue;
    } sc[] = {
        { "home", "回家", "柔光开启", LV_COLOR_MAKE(0xe8, 0x91, 0xa8) },
        { "movie", "观影", "影院氛围", LV_COLOR_MAKE(0x7e, 0xb8, 0xc9) },
        { "sleep", "晚安", "已入睡", LV_COLOR_MAKE(0xb8, 0xa0, 0xd0) },
        { "guest", "会客", "明亮会客", LV_COLOR_MAKE(0xe8, 0xa0, 0x6a) },
        { "away", "离家", "全关布防", LV_COLOR_MAKE(0xc4, 0x8a, 0x9a) },
        { "eco", "节能", "低功耗", LV_COLOR_MAKE(0x9a, 0xc4, 0xb0) },
    };
    lv_obj_t *grid = lv_obj_create(body);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        lv_obj_set_style_radius(b, 28, 0);
        if (active) {
            lv_obj_set_style_bg_color(b, sc[i].hue, 0);
            lv_obj_set_style_bg_opa(b, LV_OPA_50, 0);
        }
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        hub_ico_add(b, HUB_ICO_HOME, sc[i].hue, 20);
        lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_TOP_LEFT, 12, 12);
        lbl(b, sc[i].name, p->t1, hub_font());
        lv_obj_align(lv_obj_get_child(b, 1), LV_ALIGN_LEFT_MID, 12, 4);
        lbl(b, sc[i].sub, p->t3, hub_font());
        lv_obj_align(lv_obj_get_child(b, 2), LV_ALIGN_BOTTOM_LEFT, 12, -10);
    }
}
''',
    "pulse": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "SCENES", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct { const char *id; const char *name; } sc[] = {
        { "home", "SCENE_HOME" }, { "away", "SCENE_AWAY" },
        { "movie", "SCENE_MOVIE" }, { "sleep", "SCENE_SLEEP" },
        { "guest", "SCENE_GUEST" }, { "eco", "SCENE_ECO" },
    };
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(body);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        hub_hud_bevel(b, p->accent);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, 48);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, active ? p->accent : p->t2, hub_font());
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 12, 0);
        lbl(b, active ? "ACTIVE" : "IDLE", active ? p->on : p->t4, hub_font());
        lv_obj_align(lv_obj_get_child(b, 1), LV_ALIGN_RIGHT_MID, -12, 0);
    }
}
''',
    "ink": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "SCENES", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    lbl(body, "ID", p->t4, hub_font());
    struct { const char *id; const char *name; } sc[] = {
        { "home", "HOME" }, { "away", "AWAY" }, { "movie", "MOVIE" },
        { "sleep", "SLEEP" }, { "guest", "GUEST" }, { "eco", "ECO" },
    };
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *row = lv_btn_create(body);
        lv_obj_remove_style_all(row);
        hub_apply_card(row, active);
        theme_card_chrome(row, active);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 44);
        lv_obj_add_event_cb(row, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *id = lv_label_create(row);
        lv_label_set_text(id, sc[i].id);
        hub_style_label(id, p->accent, hub_font());
        lv_obj_align(id, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_t *n = lv_label_create(row);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, active ? p->ink_on : p->t1, hub_font());
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 72, 0);
        lbl(row, active ? "ON" : "OFF", active ? p->on : p->t4, hub_font());
        lv_obj_align(lv_obj_get_child(row, 2), LV_ALIGN_RIGHT_MID, -8, 0);
    }
}
''',
    "dusk": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);
    hub_theme_wash(body, p->violet, LV_OPA_20);

    struct { const char *id; const char *name; } sc[] = {
        { "home", "回家" }, { "movie", "观影" }, { "sleep", "睡眠模式" },
        { "away", "离家" }, { "guest", "会客" }, { "eco", "节能" },
    };
    lv_obj_t *row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(row, 10, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(row);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        lv_obj_set_style_radius(b, 14, 0);
        lv_obj_set_style_bg_opa(b, active ? LV_OPA_COVER : LV_OPA_70, 0);
        if (active) {
            lv_obj_set_style_bg_color(b, p->accent, 0);
        }
        lv_obj_set_size(b, 140, 48);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        lv_obj_t *n = lv_label_create(b);
        lv_label_set_text(n, sc[i].name);
        hub_style_label(n, active ? p->ink_on : p->t1, hub_font());
        lv_obj_center(n);
    }
}
''',
    "ocean": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct {
        const char *id;
        const char *name;
        const char *sub;
        hub_ico_t ico;
    } sc[] = {
        { "home", "回家", "恢复运行态", HUB_ICO_HOME },
        { "away", "离家", "低功耗+布防", HUB_ICO_AWAY },
        { "movie", "观影", "影院负载", HUB_ICO_LAYERS },
        { "sleep", "睡眠", "夜间曲线", HUB_ICO_MOON },
        { "guest", "会客", "峰值照明", HUB_ICO_BULB },
        { "eco", "节能", "功率封顶", HUB_ICO_GAUGE },
    };
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(body);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, 52);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        hub_ico_add(b, sc[i].ico, active ? p->cool : p->accent, 20);
        lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 8, 0);
        lbl(b, sc[i].name, p->t1, hub_font());
        lv_obj_align(lv_obj_get_child(b, 1), LV_ALIGN_LEFT_MID, 40, -8);
        lbl(b, sc[i].sub, p->t3, hub_font());
        lv_obj_align(lv_obj_get_child(b, 2), LV_ALIGN_LEFT_MID, 40, 10);
        lbl(b, active ? "RUN" : "—", active ? p->on : p->t4, hub_font());
        lv_obj_align(lv_obj_get_child(b, 3), LV_ALIGN_RIGHT_MID, -8, 0);
    }
}
''',
    "sand": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct {
        const char *id;
        const char *name;
        const char *sub;
        hub_ico_t ico;
    } sc[] = {
        { "home", "回家", "灯+空调", HUB_ICO_HOME },
        { "away", "离家", "全关布防", HUB_ICO_AWAY },
        { "movie", "观影", "影院灯光", HUB_ICO_LAYERS },
        { "sleep", "睡眠", "夜间模式", HUB_ICO_MOON },
        { "guest", "会客", "明亮会客", HUB_ICO_BULB },
        { "eco", "节能", "低功耗", HUB_ICO_GAUGE },
    };
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(body);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, 56);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        hub_ico_add(b, sc[i].ico, active ? p->ink_on : p->accent, 22);
        lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 12, 0);
        lbl(b, sc[i].name, active ? p->ink_on : p->t1, hub_font());
        lv_obj_align(lv_obj_get_child(b, 1), LV_ALIGN_LEFT_MID, 48, -8);
        lbl(b, sc[i].sub, active ? p->ink_on : p->t3, hub_font());
        lv_obj_align(lv_obj_get_child(b, 2), LV_ALIGN_LEFT_MID, 48, 10);
        if (active) {
            lv_obj_set_style_bg_color(b, p->accent, 0);
        }
    }
}
''',
    "slate": r'''
void build_scenes(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    hub_model_t *m = hub_model();
    lv_obj_t *shell = hub_shell_create(parent, "情景", true);
    lv_obj_t *body = hub_shell_body(shell);
    hub_shell_set_dots(shell, 4, 2);

    struct {
        const char *id;
        const char *name;
        const char *sub;
        hub_ico_t ico;
    } sc[] = {
        { "home", "回家", "灯+空调", HUB_ICO_HOME },
        { "away", "离家", "全关布防", HUB_ICO_AWAY },
        { "movie", "观影", "影院灯光", HUB_ICO_LAYERS },
        { "sleep", "睡眠", "夜间模式", HUB_ICO_MOON },
        { "guest", "会客", "明亮会客", HUB_ICO_BULB },
        { "eco", "节能", "低功耗", HUB_ICO_GAUGE },
    };
    lv_obj_t *grid = lv_obj_create(body);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (int i = 0; i < 6; i++) {
        bool active = strcmp(m->active_scene, sc[i].id) == 0;
        lv_obj_t *b = lv_btn_create(grid);
        lv_obj_remove_style_all(b);
        hub_apply_card(b, active);
        theme_card_chrome(b, active);
        if (active) {
            lv_obj_set_style_bg_color(b, p->accent, 0);
        }
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_border_color(b, p->accent, 0);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
        lv_obj_add_event_cb(b, scene_cb, LV_EVENT_CLICKED, (void *)sc[i].id);
        hub_ico_add(b, sc[i].ico, active ? p->ink_on : p->accent, 22);
        lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_TOP_LEFT, 8, 8);
        lbl(b, sc[i].name, active ? p->ink_on : p->t1, hub_font());
        lv_obj_align(lv_obj_get_child(b, 1), LV_ALIGN_LEFT_MID, 8, 4);
        lbl(b, sc[i].sub, active ? p->ink_on : p->t3, hub_font());
        lv_obj_align(lv_obj_get_child(b, 2), LV_ALIGN_BOTTOM_LEFT, 8, -8);
    }
}
''',
}


def deepen_energy_prefix(tid: str) -> str:
    """Extra widgets inserted after energy hero card tip — theme flavored."""
    if tid == "ocean":
        return r'''
    /* protocol health strip */
    lv_obj_t *ph = lv_obj_create(body);
    lv_obj_remove_style_all(ph);
    lv_obj_set_width(ph, LV_PCT(100));
    lv_obj_set_height(ph, 72);
    lv_obj_set_flex_flow(ph, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ph, 6, 0);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = lv_btn_create(ph);
        lv_obj_remove_style_all(c);
        hub_apply_card(c, m->protos[i].ok);
        theme_card_chrome(c, m->protos[i].ok);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_height(c, LV_PCT(100));
        lv_obj_add_event_cb(c, proto_cb, LV_EVENT_CLICKED, (void *)m->protos[i].id);
        lbl(c, m->protos[i].name, p->t3, hub_font());
        lv_obj_align(lv_obj_get_child(c, 0), LV_ALIGN_TOP_MID, 0, 8);
        char hv[16];
        snprintf(hv, sizeof(hv), "%d%%", m->protos[i].health);
        lv_obj_t *hv_l = lv_label_create(c);
        lv_label_set_text(hv_l, hv);
        hub_style_label(hv_l, m->protos[i].ok ? p->on : p->alert, hub_font());
        lv_obj_align(hv_l, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
'''
    if tid == "metro":
        return r'''
    lv_obj_t *tiles = lv_obj_create(body);
    lv_obj_remove_style_all(tiles);
    lv_obj_set_width(tiles, LV_PCT(100));
    lv_obj_set_height(tiles, 72);
    lv_obj_set_flex_flow(tiles, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tiles, 6, 0);
    lv_color_t cols3[] = {
        LV_COLOR_MAKE(0x00, 0x78, 0xd4), LV_COLOR_MAKE(0x10, 0x7c, 0x10), LV_COLOR_MAKE(0xff, 0x8c, 0x00),
    };
    const char *tl[] = { "今日", "峰值", "总线" };
    char a2[24], b2[24], c2[24];
    snprintf(a2, sizeof(a2), "%.1fkWh", (double)(12.0 + m->power_kw * 2));
    snprintf(b2, sizeof(b2), "%.1fkW", (double)m->power_kw);
    snprintf(c2, sizeof(c2), "%d/%d", hub_model_ok_count(), HUB_PROTO_COUNT);
    const char *tv[] = { a2, b2, c2 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *t = lv_obj_create(tiles);
        lv_obj_remove_style_all(t);
        lv_obj_set_style_radius(t, 0, 0);
        lv_obj_set_style_bg_color(t, cols3[i], 0);
        lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
        lv_obj_set_flex_grow(t, 1);
        lv_obj_set_height(t, LV_PCT(100));
        lbl(t, tl[i], lv_color_white(), hub_font());
        lv_obj_align(lv_obj_get_child(t, 0), LV_ALIGN_TOP_LEFT, 8, 8);
        lbl(t, tv[i], lv_color_white(), hub_font());
        lv_obj_align(lv_obj_get_child(t, 1), LV_ALIGN_BOTTOM_LEFT, 8, -8);
    }
'''
    if tid == "dusk":
        return r'''
    lv_obj_t *sec = lv_btn_create(body);
    lv_obj_remove_style_all(sec);
    hub_apply_card(sec, m->armed);
    theme_card_chrome(sec, m->armed);
    lv_obj_set_width(sec, LV_PCT(100));
    lv_obj_set_height(sec, 56);
    lv_obj_add_event_cb(sec, go_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)HUB_ROUTE_SECURITY);
    hub_ico_add(sec, HUB_ICO_SHIELD, p->violet, 22);
    lv_obj_align(lv_obj_get_child(sec, 0), LV_ALIGN_LEFT_MID, 10, 0);
    lbl(sec, "夜间安防", p->t1, hub_font());
    lv_obj_align(lv_obj_get_child(sec, 1), LV_ALIGN_LEFT_MID, 44, 0);
    lbl(sec, m->armed ? "已布防" : "撤防", m->armed ? p->on : p->t3, hub_font());
    lv_obj_align(lv_obj_get_child(sec, 2), LV_ALIGN_RIGHT_MID, -10, 0);
'''
    return ""


def strip_file_header_includes(text: str) -> str:
    text = re.sub(r"^/\*\*.*?\*/\s*", "", text, count=1, flags=re.S)
    out = []
    for line in text.splitlines(keepends=True):
        if line.startswith("#include"):
            continue
        out.append(line)
    return "".join(out).strip() + "\n"


def destatic_public(text: str) -> str:
    """Remove static from pack API symbols (listed in theme_local.h)."""
    names = (
        "go_cb",
        "scene_cb",
        "proto_cb",
        "room_cb",
        "arm_cb",
        "zone_cb",
        "toggle_dev_cb",
        "step_ac_cb",
        "step_curtain_cb",
        "strip_step_cb",
        "curtain_set_cb",
        "lbl",
        "btn_go",
        "quickbar",
        "theme_card_chrome",
        "build_home",
        "build_room",
        "build_scenes",
        "build_energy",
        "build_gateway",
        "build_network",
        "build_points",
        "build_settings",
        "build_security",
        "build_schedule",
        "build_hvac",
    )
    for n in names:
        text = re.sub(rf"^static void {n}\(", rf"void {n}(", text, flags=re.M)
        text = re.sub(rf"^static lv_obj_t \*{n}\(", rf"lv_obj_t *{n}(", text, flags=re.M)
    return text


def extract_section(text: str, marker: str, next_markers: tuple[str, ...]) -> str:
    m = re.search(rf"/\* ── {re.escape(marker)}.*?\n", text)
    if not m:
        raise RuntimeError(f"marker missing: {marker}")
    start = m.end()
    end = len(text)
    for nm in next_markers:
        nm_m = re.search(rf"/\* ── {re.escape(nm)}.*?\n", text[start:])
        if nm_m:
            end = start + nm_m.start()
            break
    return text[start:end].strip() + "\n"


def default_scenes_from_pack(sec: str) -> str:
    return destatic_public(sec)


def split_theme(tid: str) -> None:
    src = THEMES / tid / "theme.c"
    if not src.exists():
        # already split?
        if (THEMES / tid / "home.c").exists():
            print(f"skip {tid}: already split")
            return
        raise FileNotFoundError(src)

    text = src.read_text(encoding="utf-8")
    name = tid.capitalize()
    d = THEMES / tid

    palette = extract_section(text, "palette.c", ("theme_local.c",))
    local = extract_section(text, "theme_local.c", ("home.c",))
    home = extract_section(text, "home.c", ("pages_room.c",))
    room = extract_section(text, "pages_room.c", ("pages_scenes.c",))
    scenes = extract_section(text, "pages_scenes.c", ("pages_ops.c",))
    ops = extract_section(text, "pages_ops.c", ("pages_life.c",))
    life = extract_section(text, "pages_life.c", ("boot.c",))
    boot = extract_section(text, "boot.c", ())

    palette = destatic_public(palette)  # hub_palette already public
    # palette has static const s_pal — keep static
    local = destatic_public(local)
    home = destatic_public(home)
    room = destatic_public(room)
    scenes = destatic_public(scenes)
    ops = destatic_public(ops)
    life = destatic_public(life)
    boot = destatic_public(boot)

    if tid in SCENES:
        scenes = SCENES[tid].lstrip("\n")

    # Deepen energy in ops
    extra = deepen_energy_prefix(tid)
    if extra:
        needle = "lbl(tip, \"查看计量总线 →\", p->t2, hub_font());"
        if needle in ops:
            ops = ops.replace(needle, needle + "\n" + extra, 1)
        else:
            # insert after first hub_shell_set_dots in energy
            ops = ops.replace(
                "hub_shell_set_dots(shell, 4, 3);",
                "hub_shell_set_dots(shell, 4, 3);" + extra,
                1,
            )

    # zen: more pad on room/settings via life/room — light touch
    if tid == "zen":
        room = room.replace(
            "lv_obj_t *body = hub_shell_body(shell);",
            "lv_obj_t *body = hub_shell_body(shell);\n    lv_obj_set_style_pad_row(body, 12, 0);",
            1,
        )

    (d / "theme_local.h").write_text(LOCAL_H.format(name=name), encoding="utf-8", newline="\n")

    def write(fname: str, title: str, body: str) -> None:
        (d / fname).write_text(
            f"/** {name} — {title} */\n{INCLUDES}\n{body.strip()}\n",
            encoding="utf-8",
            newline="\n",
        )

    write("palette.c", "palette", palette)
    write("theme_local.c", "helpers / chrome / callbacks", local)
    write("home.c", "home", home)
    write("pages_room.c", "room page", room)
    write("pages_scenes.c", "scenes page", scenes)
    write("pages_ops.c", "energy / gateway / network / points", ops)
    write("pages_life.c", "settings / security / schedule / hvac", life)
    write("boot.c", "router + app_ui_start", boot)

    (d / "README.md").write_text(
        f"""# {name} theme pack

| File | Role |
|------|------|
| `palette.c` | 色板 |
| `theme_local.h/.c` | 本主题回调、chrome、控件辅助 |
| `home.c` | 首页 IA |
| `pages_room.c` | 房间控件 |
| `pages_scenes.c` | 情景 |
| `pages_ops.c` | 能耗 / 总线 / 网络 / 点表 |
| `pages_life.c` | 设置 / 安防 / 日程 / 温控 |
| `boot.c` | `hub_theme_build` + `app_ui_start` |

主题之间不互相引用；只依赖 `main/hub_ui/` 薄运行时。
选型：`APP_UI_THEME_{tid.upper()}`
""",
        encoding="utf-8",
        newline="\n",
    )

    src.unlink()
    print(f"split {tid} → multi-file pack (deleted theme.c)")


def main() -> None:
    for tid in THEME_IDS:
        split_theme(tid)
    print(f"done: {len(THEME_IDS)} theme folders")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)

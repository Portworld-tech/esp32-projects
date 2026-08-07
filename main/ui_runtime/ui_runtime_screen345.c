#include "ui_runtime_screen345.h"
#include "ui_runtime.h"
#include "ui.h"
#include "ui_bg_task.h"

#include "core/lv_refr.h"

#include "esp_log.h"

static const char *TAG_SC345 = "ui_sc345";

/* 由 ui_runtime.c 提供的共享状态 */
extern bool s_power_sw1;
extern bool s_power_sw3;
extern bool s_power_sw4;
extern bool s_screen3_bound;
extern bool s_screen4_bound;
extern bool s_screen5_bound;
extern int s_screen3_temp;
extern int s_screen5_temp;

/* 通用能力 */
extern void ui_runtime_set_hidden(lv_obj_t *obj, bool hidden);
extern bool ui_runtime_switch_is_on(lv_obj_t *sw);
extern void ui_runtime_switch_set_on(lv_obj_t *sw, bool on);
extern void ui_runtime_storage_save_settings(void);
extern void ui_runtime_storage_save_temps(void);
extern void ui_runtime_cloud_notify_changed(void);
extern int ui_runtime_clamp_int(int v, int lo, int hi);

/* ---- 圆弧仪表：仅变化指示弧 + 外侧光条（无底轨）---- */
#define UI_SC345_COLOR_AC       0x87CEEB
#define UI_SC345_COLOR_FLOOR    0xFF8A3D
#define UI_SC345_COLOR_OFF      0xB8B8B8
#define UI_SC345_COLOR_TICK_DIM 0x3A4555
#define UI_SC345_COLOR_BTN_OFF  0x6A6A6A

#define UI_SC345_ARC_SIZE       360       /* 放大仪表，合理占满中部 */
#define UI_SC345_ARC_WIDTH      4
#define UI_SC345_TICK_GAP       5
#define UI_SC345_TICK_LEN       14
#define UI_SC345_TICK_WIDTH     3
#define UI_SC345_TICK_N         16
#define UI_SC345_BG_ANG_START   150
#define UI_SC345_BG_ANG_SPAN    240
#define UI_SC345_TOP_H          36
#define UI_SC345_BOTTOM_H       88
#define UI_SC345_GAUGE_H        356       /* 480 - top - bottom */

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *temp_label;
    lv_obj_t *tick_host;
    lv_obj_t *ticks[UI_SC345_TICK_N];
    lv_point_t tick_pts[UI_SC345_TICK_N][2]; /* lv_line 需要持久点集 */
    uint32_t accent_hex;
    int vmin;
    int vmax;
    bool ready;
} ui_sc345_gauge_t;

static ui_sc345_gauge_t s_gauge_ac;    /* Screen3 */
static ui_sc345_gauge_t s_gauge_floor; /* Screen5 */

/* 避免切屏反复 apply_power 整页脏区（S3↔S4↔S5 轻量切屏时易撕裂）。 */
static bool s_s3_power_vis_valid;
static bool s_s3_power_vis_on;
static bool s_s4_power_vis_valid;
static bool s_s4_power_vis_on;
static bool s_s5_power_vis_valid;
static bool s_s5_power_vis_on;

static void ui_sc345_overflow_chain(lv_obj_t *obj)
{
    for (int i = 0; obj != NULL && i < 6; i++) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        obj = lv_obj_get_parent(obj);
    }
}

static void ui_sc345_set_tick_style(lv_obj_t *tick, lv_color_t c, lv_opa_t opa)
{
    if (tick == NULL) {
        return;
    }
    lv_obj_set_style_line_color(tick, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(tick, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(tick, UI_SC345_TICK_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(tick, true, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* 按温度点亮外侧光条；指示弧由 lv_arc_set_value 驱动。 */
static void ui_sc345_sync_ticks_value(ui_sc345_gauge_t *g, int value, bool on)
{
    if (g == NULL || !g->ready) {
        return;
    }
    const int span = g->vmax - g->vmin;
    int lit = 0;
    if (on && span > 0) {
        lit = ((value - g->vmin) * UI_SC345_TICK_N + span / 2) / span;
        if (lit < 1) {
            lit = 1;
        }
        if (lit > UI_SC345_TICK_N) {
            lit = UI_SC345_TICK_N;
        }
    }
    const lv_color_t c_on = lv_color_hex(on ? g->accent_hex : UI_SC345_COLOR_OFF);
    const lv_color_t c_dim = lv_color_hex(UI_SC345_COLOR_TICK_DIM);
    for (int i = 0; i < UI_SC345_TICK_N; i++) {
        const bool active = on && (i < lit);
        ui_sc345_set_tick_style(g->ticks[i], active ? c_on : c_dim,
                                active ? LV_OPA_COVER : LV_OPA_60);
    }
    if (g->tick_host) {
        lv_obj_invalidate(g->tick_host);
    }
}

static void ui_sc345_create_tick_ring(ui_sc345_gauge_t *g)
{
    if (g == NULL || g->arc == NULL || g->ready) {
        return;
    }
    lv_obj_t *parent = lv_obj_get_parent(g->arc);
    if (parent == NULL) {
        return;
    }

    lv_obj_set_size(g->arc, UI_SC345_ARC_SIZE, UI_SC345_ARC_SIZE);

    const int host = UI_SC345_ARC_SIZE + 2 * (UI_SC345_TICK_GAP + UI_SC345_TICK_LEN + 4);
    const int cx = host / 2;
    const int cy = host / 2;
    const int r_outer = UI_SC345_ARC_SIZE / 2;
    const int r_in = r_outer + UI_SC345_TICK_GAP;
    const int r_out = r_in + UI_SC345_TICK_LEN;

    g->tick_host = lv_obj_create(parent);
    lv_obj_remove_style_all(g->tick_host);
    lv_obj_set_size(g->tick_host, host, host);
    lv_obj_clear_flag(g->tick_host,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(g->tick_host, LV_OBJ_FLAG_OVERFLOW_VISIBLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    ui_sc345_overflow_chain(parent);
    lv_obj_align_to(g->tick_host, g->arc, LV_ALIGN_CENTER, 0, 0);
    /* 置于弧后方：中心按钮不被挡；弧外光条仍露出。 */
    lv_obj_move_background(g->tick_host);
    lv_obj_move_foreground(g->arc);

    for (int i = 0; i < UI_SC345_TICK_N; i++) {
        const int ang = UI_SC345_BG_ANG_START + (UI_SC345_BG_ANG_SPAN * i) / (UI_SC345_TICK_N - 1);
        const int32_t s = lv_trigo_sin(ang);
        const int32_t c = lv_trigo_sin(ang + 90);

        g->tick_pts[i][0].x = (lv_coord_t)(cx + ((r_in * c) >> LV_TRIGO_SHIFT));
        g->tick_pts[i][0].y = (lv_coord_t)(cy + ((r_in * s) >> LV_TRIGO_SHIFT));
        g->tick_pts[i][1].x = (lv_coord_t)(cx + ((r_out * c) >> LV_TRIGO_SHIFT));
        g->tick_pts[i][1].y = (lv_coord_t)(cy + ((r_out * s) >> LV_TRIGO_SHIFT));

        lv_obj_t *tick = lv_line_create(g->tick_host);
        lv_line_set_points(tick, g->tick_pts[i], 2);
        lv_obj_clear_flag(tick, LV_OBJ_FLAG_CLICKABLE);
        ui_sc345_set_tick_style(tick, lv_color_hex(UI_SC345_COLOR_TICK_DIM), LV_OPA_COVER);
        g->ticks[i] = tick;
    }
    g->ready = true;
    ESP_LOGI(TAG_SC345, "tick ring ready host=%d r_in=%d r_out=%d", host, r_in, r_out);
}

static void ui_sc345_style_arc(lv_obj_t *arc, uint32_t accent_hex, bool on)
{
    if (arc == NULL) {
        return;
    }
    const lv_color_t indic = lv_color_hex(on ? accent_hex : UI_SC345_COLOR_OFF);

    lv_obj_set_style_arc_width(arc, UI_SC345_ARC_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(arc, UI_SC345_ARC_WIDTH, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    /* 去掉背景底轨，只保留随温度变化的指示弧。 */
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(arc, indic, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(arc, on ? LV_OPA_COVER : LV_OPA_50, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_blend_mode(arc, LV_BLEND_MODE_NORMAL, LV_PART_KNOB | LV_STATE_DEFAULT);
}

/* 固定加减按钮：取消 flex，温度固定宽度居中，± 绝对定位。 */
static void ui_sc345_fix_temp_controls(lv_obj_t *row, lv_obj_t *btn_m, lv_obj_t *lab, lv_obj_t *btn_p)
{
    if (row == NULL) {
        return;
    }
    lv_obj_set_layout(row, 0);
    lv_obj_set_size(row, 340, 64);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -4);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (btn_m) {
        lv_obj_align(btn_m, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_radius(btn_m, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_m, lv_color_hex(0x4A5560), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn_m, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (btn_p) {
        lv_obj_align(btn_p, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_set_style_radius(btn_p, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x4A5560), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn_p, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (lab) {
        lv_obj_set_width(lab, 140);
        lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(lab, LV_ALIGN_CENTER, 0, 0);
    }
}

static void ui_sc345_layout_gauge_center(lv_obj_t *arc,
                                         lv_obj_t *title_row,
                                         lv_obj_t *temp_row,
                                         lv_obj_t *indoor_row,
                                         lv_obj_t *minmax_row)
{
    if (arc == NULL) {
        return;
    }
    /* 去掉 SquareLine 在 arc 上开的 column flex，改绝对布局。 */
    lv_obj_set_layout(arc, 0);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    if (title_row) {
        lv_obj_set_layout(title_row, 0);
        lv_obj_set_size(title_row, 220, 28);
        lv_obj_align(title_row, LV_ALIGN_CENTER, 0, -92);
        uint32_t n = lv_obj_get_child_cnt(title_row);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *ch = lv_obj_get_child(title_row, i);
            if (ch) {
                lv_obj_align(ch, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
    if (temp_row) {
        lv_obj_align(temp_row, LV_ALIGN_CENTER, 0, -8);
    }
    if (indoor_row) {
        lv_obj_set_layout(indoor_row, 0);
        lv_obj_set_size(indoor_row, 220, 28);
        lv_obj_align(indoor_row, LV_ALIGN_CENTER, 0, 56);
        uint32_t n = lv_obj_get_child_cnt(indoor_row);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *ch = lv_obj_get_child(indoor_row, i);
            if (ch) {
                lv_obj_align(ch, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
    if (minmax_row) {
        lv_obj_set_layout(minmax_row, 0);
        lv_obj_set_size(minmax_row, 300, 28);
        lv_obj_align(minmax_row, LV_ALIGN_CENTER, 0, 138);
        uint32_t n = lv_obj_get_child_cnt(minmax_row);
        if (n >= 1) {
            lv_obj_align(lv_obj_get_child(minmax_row, 0), LV_ALIGN_LEFT_MID, 18, 0);
        }
        if (n >= 2) {
            lv_obj_align(lv_obj_get_child(minmax_row, 1), LV_ALIGN_RIGHT_MID, -18, 0);
        }
    }
}

static void ui_sc345_fix_action_col(lv_obj_t *col)
{
    if (col == NULL) {
        return;
    }
    lv_obj_set_size(col, 80, UI_SC345_BOTTOM_H - 8);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    const uint32_t n = lv_obj_get_child_cnt(col);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(col, i);
        if (ch == NULL) {
            continue;
        }
        lv_obj_set_x(ch, 0);
        lv_obj_set_y(ch, 0);
        lv_obj_set_align(ch, LV_ALIGN_CENTER);
    }
}

/* 返回键 / 三点：脱离顶栏容器，单独钉在页面顶部。 */
static void ui_sc345_pin_top_chrome(lv_obj_t *root, lv_obj_t *old_bar, lv_obj_t *back_btn, lv_obj_t *dots)
{
    if (root == NULL) {
        return;
    }
    if (old_bar) {
        lv_obj_add_flag(old_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(old_bar, 0, 0);
    }
    if (back_btn) {
        lv_obj_set_parent(back_btn, root);
        lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(back_btn, 72, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 4, 2);
        lv_obj_move_foreground(back_btn);
    }
    if (dots) {
        lv_obj_set_parent(dots, root);
        lv_obj_clear_flag(dots, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_move_foreground(dots);
    }
}

static void ui_sc345_place_bottom_bar(lv_obj_t *bar, lv_obj_t *cols[], unsigned ncol)
{
    if (bar == NULL) {
        return;
    }
    lv_obj_set_layout(bar, 0);
    /* 底栏整体偏左，贴近参考图电源键偏左布局。 */
    lv_obj_set_size(bar, 300, UI_SC345_BOTTOM_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 18, -4);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(bar);

    if (ncol == 0 || cols == NULL) {
        return;
    }
    if (ncol == 1) {
        ui_sc345_fix_action_col(cols[0]);
        lv_obj_align(cols[0], LV_ALIGN_LEFT_MID, 8, 0);
    } else if (ncol == 2) {
        ui_sc345_fix_action_col(cols[0]);
        ui_sc345_fix_action_col(cols[1]);
        lv_obj_align(cols[0], LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_align(cols[1], LV_ALIGN_LEFT_MID, 108, 0);
    } else {
        ui_sc345_fix_action_col(cols[0]);
        ui_sc345_fix_action_col(cols[1]);
        ui_sc345_fix_action_col(cols[2]);
        lv_obj_align(cols[0], LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_align(cols[1], LV_ALIGN_LEFT_MID, 100, 0);
        lv_obj_align(cols[2], LV_ALIGN_LEFT_MID, 196, 0);
    }
}

/* 屏3：取消 flex 挤压；顶栏独立；圆弧区上移；底栏钉底。 */
static void ui_sc345_relayout_screen3(void)
{
    if (ui_Container12) {
        lv_obj_set_layout(ui_Container12, 0);
        lv_obj_clear_flag(ui_Container12, LV_OBJ_FLAG_SCROLLABLE);
        ui_sc345_overflow_chain(ui_Container12);
    }
    ui_sc345_pin_top_chrome(ui_Container12, ui_Container47, ui_Button14, ui_Label46);

    if (ui_Container49) {
        lv_obj_set_layout(ui_Container49, 0);
        lv_obj_set_size(ui_Container49, 480, UI_SC345_GAUGE_H);
        lv_obj_align(ui_Container49, LV_ALIGN_TOP_MID, 0, UI_SC345_TOP_H);
        lv_obj_clear_flag(ui_Container49, LV_OBJ_FLAG_SCROLLABLE);
        ui_sc345_overflow_chain(ui_Container49);
    }
    if (ui_Arc2) {
        lv_obj_set_size(ui_Arc2, UI_SC345_ARC_SIZE, UI_SC345_ARC_SIZE);
        lv_obj_align(ui_Arc2, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *cols[] = { ui_Container62, ui_Container63, ui_Container64 };
    ui_sc345_place_bottom_bar(ui_Container48, cols, 3);
}

/* 屏5：同上；修正原先底栏 x=115 / 半宽导致按钮跑出可视区。 */
static void ui_sc345_relayout_screen5(void)
{
    if (ui_Container26) {
        lv_obj_set_layout(ui_Container26, 0);
        lv_obj_clear_flag(ui_Container26, LV_OBJ_FLAG_SCROLLABLE);
        ui_sc345_overflow_chain(ui_Container26);
    }
    ui_sc345_pin_top_chrome(ui_Container26, ui_Container53, ui_Button23, ui_Label49);

    if (ui_Container54) {
        lv_obj_set_layout(ui_Container54, 0);
        lv_obj_set_size(ui_Container54, 480, UI_SC345_GAUGE_H);
        lv_obj_set_x(ui_Container54, 0);
        lv_obj_set_y(ui_Container54, 0);
        lv_obj_align(ui_Container54, LV_ALIGN_TOP_MID, 0, UI_SC345_TOP_H);
        lv_obj_clear_flag(ui_Container54, LV_OBJ_FLAG_SCROLLABLE);
        ui_sc345_overflow_chain(ui_Container54);
    }
    if (ui_Arc3) {
        lv_obj_set_size(ui_Arc3, UI_SC345_ARC_SIZE, UI_SC345_ARC_SIZE);
        lv_obj_align(ui_Arc3, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t *cols[] = { ui_Container67, ui_Container68 };
    ui_sc345_place_bottom_bar(ui_Container55, cols, 2);
}

/* 屏4：顶栏返回/三点独立固定；内容区与底栏偏左。 */
static void ui_sc345_relayout_screen4(void)
{
    if (ui_Container1) {
        lv_obj_set_layout(ui_Container1, 0);
        lv_obj_clear_flag(ui_Container1, LV_OBJ_FLAG_SCROLLABLE);
        ui_sc345_overflow_chain(ui_Container1);
    }
    ui_sc345_pin_top_chrome(ui_Container1, ui_Container50, ui_Button36, ui_Label48);
    if (ui_Panel5) {
        lv_obj_add_flag(ui_Panel5, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_Container51) {
        lv_obj_set_layout(ui_Container51, 0);
        lv_obj_set_size(ui_Container51, 480, UI_SC345_GAUGE_H);
        lv_obj_set_x(ui_Container51, 0);
        lv_obj_set_y(ui_Container51, 0);
        lv_obj_align(ui_Container51, LV_ALIGN_TOP_MID, 0, UI_SC345_TOP_H);
        lv_obj_clear_flag(ui_Container51, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *cols[] = { ui_Container65, ui_Container66 };
    ui_sc345_place_bottom_bar(ui_Container52, cols, 2);
}

static void ui_sc345_apply_gauge_theme(ui_sc345_gauge_t *g, uint32_t accent_hex, bool on)
{
    if (g == NULL) {
        return;
    }
    g->accent_hex = accent_hex;
    ui_sc345_style_arc(g->arc, accent_hex, on);
    if (g->temp_label) {
        lv_obj_set_style_text_color(g->temp_label,
                                    lv_color_hex(on ? accent_hex : UI_SC345_COLOR_OFF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g->tick_host && g->arc) {
        lv_obj_align_to(g->tick_host, g->arc, LV_ALIGN_CENTER, 0, 0);
        /* tick 在弧后，避免挡住中心 ±；弧外光条仍可见 */
        lv_obj_move_background(g->tick_host);
        lv_obj_move_foreground(g->arc);
    }
}

static void ui_sc345_beautify_screen3_once(void)
{
    if (ui_Arc2 == NULL) {
        return;
    }
    ui_sc345_relayout_screen3();

    if (s_gauge_ac.ready) {
        if (s_gauge_ac.tick_host) {
            lv_obj_align_to(s_gauge_ac.tick_host, ui_Arc2, LV_ALIGN_CENTER, 0, 0);
            lv_obj_move_background(s_gauge_ac.tick_host);
            lv_obj_move_foreground(ui_Arc2);
        }
        return;
    }

    s_gauge_ac.arc = ui_Arc2;
    s_gauge_ac.temp_label = ui_Label20;
    s_gauge_ac.accent_hex = UI_SC345_COLOR_AC;
    s_gauge_ac.vmin = 16;
    s_gauge_ac.vmax = 30;

    lv_arc_set_bg_angles(ui_Arc2, UI_SC345_BG_ANG_START, UI_SC345_BG_ANG_START + UI_SC345_BG_ANG_SPAN);
    lv_arc_set_range(ui_Arc2, 16, 30);
    lv_arc_set_value(ui_Arc2, s_screen3_temp);
    lv_obj_clear_flag(ui_Arc2, LV_OBJ_FLAG_CLICKABLE);

    ui_sc345_layout_gauge_center(ui_Arc2, ui_Container56, ui_Container57, ui_Container114, ui_Container58);
    ui_sc345_fix_temp_controls(ui_Container57, ui_Button5, ui_Label20, ui_Button13);
    ui_sc345_create_tick_ring(&s_gauge_ac);
    ui_sc345_apply_gauge_theme(&s_gauge_ac, UI_SC345_COLOR_AC, s_power_sw4);
    ui_sc345_sync_ticks_value(&s_gauge_ac, s_screen3_temp, s_power_sw4);
}

static void ui_sc345_beautify_screen5_once(void)
{
    if (ui_Arc3 == NULL) {
        return;
    }
    ui_sc345_relayout_screen5();

    if (s_gauge_floor.ready) {
        if (s_gauge_floor.tick_host) {
            lv_obj_align_to(s_gauge_floor.tick_host, ui_Arc3, LV_ALIGN_CENTER, 0, 0);
            lv_obj_move_background(s_gauge_floor.tick_host);
            lv_obj_move_foreground(ui_Arc3);
        }
        return;
    }

    s_gauge_floor.arc = ui_Arc3;
    s_gauge_floor.temp_label = ui_Label32;
    s_gauge_floor.accent_hex = UI_SC345_COLOR_FLOOR;
    s_gauge_floor.vmin = 16;
    s_gauge_floor.vmax = 32;

    lv_arc_set_bg_angles(ui_Arc3, UI_SC345_BG_ANG_START, UI_SC345_BG_ANG_START + UI_SC345_BG_ANG_SPAN);
    lv_arc_set_range(ui_Arc3, 16, 32);
    lv_arc_set_value(ui_Arc3, s_screen5_temp);
    lv_obj_clear_flag(ui_Arc3, LV_OBJ_FLAG_CLICKABLE);

    ui_sc345_layout_gauge_center(ui_Arc3, ui_Container59, ui_Container60, ui_Container116, ui_Container61);
    ui_sc345_fix_temp_controls(ui_Container60, ui_Button24, ui_Label32, ui_Button25);
    ui_sc345_create_tick_ring(&s_gauge_floor);
    ui_sc345_apply_gauge_theme(&s_gauge_floor, UI_SC345_COLOR_FLOOR, s_power_sw1);
    ui_sc345_sync_ticks_value(&s_gauge_floor, s_screen5_temp, s_power_sw1);
}

static void ui_sc345_style_power_btn(lv_obj_t *btn, uint32_t on_hex, bool on)
{
    if (btn == NULL) {
        return;
    }
    const lv_color_t c = lv_color_hex(on ? on_hex : UI_SC345_COLOR_BTN_OFF);
    lv_obj_set_style_bg_color(btn, c, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, c, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, c, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
}

/* ± 温度每次写 NVS 会 nvs_commit 阻塞 LVGL；连点时与 RGB flush 叠在一起易花屏 → 防抖提交 */
static lv_timer_t *s_temps_save_debounce_timer;

static void ui_extra_temps_save_debounce_cb(lv_timer_t *t)
{
    (void)t;
    s_temps_save_debounce_timer = NULL;
    (void)ui_bg_task_post_save_temps(s_screen3_temp, s_screen5_temp);
    ESP_LOGD(TAG_SC345, "NV temps queued (debounced)");
}

static void ui_extra_temps_save_debounced(void)
{
    if (s_temps_save_debounce_timer != NULL) {
        lv_timer_del(s_temps_save_debounce_timer);
        s_temps_save_debounce_timer = NULL;
    }
    s_temps_save_debounce_timer = lv_timer_create(ui_extra_temps_save_debounce_cb, 400, NULL);
    if (s_temps_save_debounce_timer != NULL) {
        lv_timer_set_repeat_count(s_temps_save_debounce_timer, 1);
    } else {
        (void)ui_bg_task_post_save_temps(s_screen3_temp, s_screen5_temp);
    }
}

void ui_runtime_screen345_force_coherent_frame(void)
{
    lv_obj_t *s = lv_scr_act();
    if (s != ui_Screen3 && s != ui_Screen4 && s != ui_Screen5) {
        return;
    }
    lv_obj_invalidate(s);
}

void ui_runtime_screen345_flush_pending_temp_save(void)
{
    if (s_temps_save_debounce_timer == NULL) {
        return;
    }
    lv_timer_del(s_temps_save_debounce_timer);
    s_temps_save_debounce_timer = NULL;
    (void)ui_bg_task_post_save_temps(s_screen3_temp, s_screen5_temp);
    ESP_LOGD(TAG_SC345, "NV temps queued before switch");
}

static bool ui_sc345_ignore_if_not_scr(lv_obj_t *expect, const char *who)
{
    if (ui_runtime_screen_switch_busy()) {
        ESP_LOGD(TAG_SC345, "busy drop %s", who);
        return true;
    }
    lv_obj_t *act = lv_scr_act();
    if (act != expect) {
        ESP_LOGD(TAG_SC345, "stale %s act=%p expect=%p", who, (void *)act, (void *)expect);
        return true;
    }
    return false;
}

/* 应用 Screen3（空调）总开关视觉与控件状态。 */
void ui_extra_apply_screen3_power(bool on)
{
    s_power_sw4 = on;
    if (s_s3_power_vis_valid && s_s3_power_vis_on == on && s_gauge_ac.ready) {
        return;
    }
    const lv_color_t c_txt = on ? lv_color_hex(0xFFFFFF) : lv_color_hex(UI_SC345_COLOR_OFF);

    ui_sc345_beautify_screen3_once();

    ui_runtime_set_hidden(ui_Button5, !on);
    ui_runtime_set_hidden(ui_Button13, !on);
    ui_runtime_set_hidden(ui_Container63, !on);
    ui_runtime_set_hidden(ui_Container64, !on);

    if (ui_Label7) {
        lv_label_set_text(ui_Label7, on ? "Open" : "Close");
        lv_obj_set_style_text_color(ui_Label7, c_txt, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_Label7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    ui_sc345_style_power_btn(ui_Button15, UI_SC345_COLOR_AC, on);
    ui_sc345_apply_gauge_theme(&s_gauge_ac, UI_SC345_COLOR_AC, on);
    ui_sc345_sync_ticks_value(&s_gauge_ac, s_screen3_temp, on);
    s_s3_power_vis_valid = true;
    s_s3_power_vis_on = on;

    if (lv_scr_act() == ui_Screen3) {
        ui_runtime_screen345_force_coherent_frame();
    }
}

/* 应用 Screen5（地暖）总开关视觉与控件状态。 */
void ui_extra_apply_screen5_power(bool on)
{
    s_power_sw1 = on;
    if (s_s5_power_vis_valid && s_s5_power_vis_on == on && s_gauge_floor.ready) {
        return;
    }
    const lv_color_t c_txt = on ? lv_color_hex(0xFFFFFF) : lv_color_hex(UI_SC345_COLOR_OFF);

    ui_sc345_beautify_screen5_once();

    ui_runtime_set_hidden(ui_Button24, !on);
    ui_runtime_set_hidden(ui_Button25, !on);
    ui_runtime_set_hidden(ui_Container68, !on);

    if (ui_Label38) {
        lv_label_set_text(ui_Label38, on ? "Open" : "Close");
        lv_obj_set_style_text_color(ui_Label38, c_txt, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_Label38, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    ui_sc345_style_power_btn(ui_Button26, UI_SC345_COLOR_FLOOR, on);
    ui_sc345_apply_gauge_theme(&s_gauge_floor, UI_SC345_COLOR_FLOOR, on);
    ui_sc345_sync_ticks_value(&s_gauge_floor, s_screen5_temp, on);
    s_s5_power_vis_valid = true;
    s_s5_power_vis_on = on;

    if (lv_scr_act() == ui_Screen5) {
        ui_runtime_screen345_force_coherent_frame();
    }
}

/* 应用 Screen4（暖气）总开关视觉与控件状态。 */
void ui_extra_apply_screen4_power(bool on)
{
    s_power_sw3 = on;
    if (s_s4_power_vis_valid && s_s4_power_vis_on == on) {
        return;
    }
    const lv_color_t c_txt = on ? lv_color_hex(0xFFFFFF) : lv_color_hex(UI_SC345_COLOR_OFF);

    ui_sc345_relayout_screen4();

    ui_runtime_set_hidden(ui_Container66, !on);
    ui_runtime_set_hidden(ui_Container75, !on);
    if (ui_Container75) {
        /* 跟风气流层：冷青白雾自上而下融入背景，无橙色。 */
        lv_obj_set_style_radius(ui_Container75, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_Container75, lv_color_hex(0xC5D9E8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(ui_Container75, lv_color_hex(0x111F2B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(ui_Container75, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_Container75, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_stop(ui_Container75, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_Container75, on ? 150 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_mark_layout_as_dirty(lv_obj_get_parent(ui_Container75));
        lv_obj_invalidate(ui_Container75);
    }

    if (ui_Panel6) {
        lv_obj_set_style_bg_color(ui_Panel6,
                                  on ? lv_color_hex(0x93CCBD) : lv_color_hex(0xFF0000),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_Panel6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_Panel7) {
        lv_obj_set_height(ui_Panel7, on ? 2 : 19);
        lv_obj_set_style_bg_color(ui_Panel7, lv_color_hex(0x525052), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_invalidate(ui_Panel7);
    }
    if (ui_Label36) {
        lv_label_set_text(ui_Label36, on ? "Open" : "Close");
        lv_obj_set_style_text_color(ui_Label36, c_txt, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_Label36, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    /* 电源键沿用蓝色，不做橙色修饰。 */
    ui_sc345_style_power_btn(ui_Button20, 0x2095F6, on);
    s_s4_power_vis_valid = true;
    s_s4_power_vis_on = on;
    if (lv_scr_act() == ui_Screen4) {
        ui_runtime_screen345_force_coherent_frame();
    }
}

void ui_extra_event_switch4_device1(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen1, "Sw4")) return;
    ui_extra_apply_screen3_power(ui_runtime_switch_is_on(ui_Switch4));
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_switch1_device2(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen1, "Sw1")) return;
    ui_extra_apply_screen5_power(ui_runtime_switch_is_on(ui_Switch1));
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_switch3_device3(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen1, "Sw3")) return;
    ui_extra_apply_screen4_power(ui_runtime_switch_is_on(ui_Switch3));
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_button15_toggle_device1(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen3, "Btn15")) return;
    bool on = !ui_runtime_switch_is_on(ui_Switch4);
    ui_runtime_switch_set_on(ui_Switch4, on);
    ui_extra_apply_screen3_power(on);
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_button26_toggle_device2(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen5, "Btn26")) return;
    bool on = !ui_runtime_switch_is_on(ui_Switch1);
    ui_runtime_switch_set_on(ui_Switch1, on);
    ui_extra_apply_screen5_power(on);
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_button20_toggle_device3(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen4, "Btn20")) return;
    bool on = !ui_runtime_switch_is_on(ui_Switch3);
    ui_runtime_switch_set_on(ui_Switch3, on);
    ui_extra_apply_screen4_power(on);
    ui_runtime_storage_save_settings();
    ui_runtime_cloud_notify_changed();
}

/* 应用 Screen3 温度到圆弧与标签并保存。 */
void ui_extra_screen3_arc_apply(void)
{
    s_screen3_temp = ui_runtime_clamp_int(s_screen3_temp, 16, 30);
    if (ui_Arc2) {
        lv_arc_set_range(ui_Arc2, 16, 30);
        lv_arc_set_value(ui_Arc2, s_screen3_temp);
        lv_obj_invalidate(ui_Arc2);
    }
    if (ui_Label20) lv_label_set_text_fmt(ui_Label20, "%d°C", s_screen3_temp);
    if (ui_Label9)  lv_label_set_text_fmt(ui_Label9,  "%d°C", s_screen3_temp);
    ui_sc345_sync_ticks_value(&s_gauge_ac, s_screen3_temp, s_power_sw4);
    ui_extra_temps_save_debounced();
    /* 不整屏 invalidate：± 连点时整屏脏区会拖垮 RGB bounce，出现撕裂。 */
}

/* 应用 Screen5 温度到圆弧与标签并保存。 */
void ui_extra_screen5_arc_apply(void)
{
    s_screen5_temp = ui_runtime_clamp_int(s_screen5_temp, 16, 32);
    if (ui_Arc3) {
        lv_arc_set_range(ui_Arc3, 16, 32);
        lv_arc_set_value(ui_Arc3, s_screen5_temp);
        lv_obj_invalidate(ui_Arc3);
    }
    if (ui_Label32) lv_label_set_text_fmt(ui_Label32, "%d°C", s_screen5_temp);
    if (ui_Label4)  lv_label_set_text_fmt(ui_Label4,  "%d°C", s_screen5_temp);
    ui_sc345_sync_ticks_value(&s_gauge_floor, s_screen5_temp, s_power_sw1);
    ui_extra_temps_save_debounced();
}

void ui_extra_event_Button5(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen3, "Btn5-")) return;
    s_screen3_temp--;
    ui_extra_screen3_arc_apply();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_Button13(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen3, "Btn13+")) return;
    s_screen3_temp++;
    ui_extra_screen3_arc_apply();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_Button24(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen5, "Btn24-")) return;
    s_screen5_temp--;
    ui_extra_screen5_arc_apply();
    ui_runtime_cloud_notify_changed();
}

void ui_extra_event_Button25(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (ui_sc345_ignore_if_not_scr(ui_Screen5, "Btn25+")) return;
    s_screen5_temp++;
    ui_extra_screen5_arc_apply();
    ui_runtime_cloud_notify_changed();
}

void ui_runtime_step_screen3_temp(int delta)
{
    if (delta == 0) return;
    s_screen3_temp += delta;
    ui_extra_screen3_arc_apply();
}

void ui_runtime_step_screen5_temp(int delta)
{
    if (delta == 0) return;
    s_screen5_temp += delta;
    ui_extra_screen5_arc_apply();
}

void ui_runtime_screen345_bind_apply_for_active_screen(lv_obj_t *act)
{
    if (act == ui_Screen3) {
        ui_sc345_beautify_screen3_once();
        if (!s_screen3_bound) {
            if (ui_Button15) lv_obj_add_event_cb(ui_Button15, ui_extra_event_button15_toggle_device1, LV_EVENT_CLICKED, NULL);
            if (ui_Button5)  lv_obj_add_event_cb(ui_Button5,  ui_extra_event_Button5,  LV_EVENT_CLICKED, NULL);
            if (ui_Button13) lv_obj_add_event_cb(ui_Button13, ui_extra_event_Button13, LV_EVENT_CLICKED, NULL);
            s_screen3_bound = true;
        }
        ui_extra_apply_screen3_power(s_power_sw4);
        ui_runtime_indoor_labels_sync_from_cache();
    } else if (act == ui_Screen4) {
        if (!s_screen4_bound) {
            if (ui_Button20) lv_obj_add_event_cb(ui_Button20, ui_extra_event_button20_toggle_device3, LV_EVENT_CLICKED, NULL);
            s_screen4_bound = true;
        }
        ui_extra_apply_screen4_power(s_power_sw3);
    } else if (act == ui_Screen5) {
        ui_sc345_beautify_screen5_once();
        if (!s_screen5_bound) {
            if (ui_Button26) lv_obj_add_event_cb(ui_Button26, ui_extra_event_button26_toggle_device2, LV_EVENT_CLICKED, NULL);
            if (ui_Button24) lv_obj_add_event_cb(ui_Button24, ui_extra_event_Button24, LV_EVENT_CLICKED, NULL);
            if (ui_Button25) lv_obj_add_event_cb(ui_Button25, ui_extra_event_Button25, LV_EVENT_CLICKED, NULL);
            s_screen5_bound = true;
        }
        ui_extra_apply_screen5_power(s_power_sw1);
        ui_runtime_indoor_labels_sync_from_cache();
    }
}


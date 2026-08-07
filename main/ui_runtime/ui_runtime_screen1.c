#include "ui.h"
#include "lvgl.h"

#include <stddef.h>

/* Screen1 导航卡：按下不变大，背景天蓝；内容反色（白底/黑字/黑图标）。 */
#define UI_S1_TILE_BG_DEFAULT   0x2F3844
#define UI_S1_TILE_BG_PRESSED   0x87CEEB  /* 天蓝色 */
#define UI_S1_FG_DEFAULT        0xFFFFFF
#define UI_S1_FG_PRESSED        0x000000
#define UI_S1_ICON_BG_DEFAULT   0x000000
#define UI_S1_ICON_BG_PRESSED   0xFFFFFF

typedef struct {
    lv_obj_t *left_panel; /* 左上角圆形图标底 */
    lv_obj_t *left_img;
    lv_obj_t *right_img;  /* 右上角图标（无底） */
    lv_obj_t *label_a;
    lv_obj_t *label_b;
} ui_s1_tile_content_t;

static ui_s1_tile_content_t s_content_btn1;
static ui_s1_tile_content_t s_content_btn3;
static ui_s1_tile_content_t s_content_btn4;

static void ui_runtime_screen1_tile_kill_grow(lv_obj_t *btn, lv_style_selector_t sel)
{
    /* 覆盖 LVGL default theme 的 styles->grow（transform_width/height +3）。 */
    lv_obj_set_style_transform_width(btn, 0, sel);
    lv_obj_set_style_transform_height(btn, 0, sel);
    lv_obj_set_style_transform_zoom(btn, LV_IMG_ZOOM_NONE, sel);
    lv_obj_set_style_transform_angle(btn, 0, sel);
}

static void ui_s1_set_img_tone(lv_obj_t *img, bool pressed)
{
    if (img == NULL) {
        return;
    }
    if (pressed) {
        /* 按下：图标强制黑色。 */
        lv_obj_set_style_img_recolor(img, lv_color_hex(UI_S1_FG_PRESSED), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        /* 松开：取消着色，恢复资源原色（通常为浅色图标）。 */
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void ui_s1_set_label_fg(lv_obj_t *lab, bool pressed)
{
    if (lab == NULL) {
        return;
    }
    lv_obj_set_style_text_color(lab,
                                lv_color_hex(pressed ? UI_S1_FG_PRESSED : UI_S1_FG_DEFAULT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void ui_s1_tile_apply_content(const ui_s1_tile_content_t *c, bool pressed)
{
    if (c == NULL) {
        return;
    }

    if (c->left_panel) {
        lv_obj_set_style_bg_color(c->left_panel,
                                  lv_color_hex(pressed ? UI_S1_ICON_BG_PRESSED : UI_S1_ICON_BG_DEFAULT),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(c->left_panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    ui_s1_set_img_tone(c->left_img, pressed);
    ui_s1_set_img_tone(c->right_img, pressed);
    ui_s1_set_label_fg(c->label_a, pressed);
    ui_s1_set_label_fg(c->label_b, pressed);
}

static void ui_s1_tile_press_content_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const ui_s1_tile_content_t *c = (const ui_s1_tile_content_t *)lv_event_get_user_data(e);
    if (c == NULL) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        ui_s1_tile_apply_content(c, true);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ui_s1_tile_apply_content(c, false);
    }
}

static void ui_s1_tile_refresh_content_ptrs(void)
{
    s_content_btn1 = (ui_s1_tile_content_t){
        .left_panel = ui_Panel2,
        .left_img = ui_Image29,
        .right_img = ui_Image33,
        .label_a = ui_Label4,
        .label_b = ui_Label5,
    };
    s_content_btn3 = (ui_s1_tile_content_t){
        .left_panel = ui_Panel13,
        .left_img = ui_Image32,
        .right_img = ui_Image28,
        .label_a = ui_Label2,
        .label_b = NULL,
    };
    s_content_btn4 = (ui_s1_tile_content_t){
        .left_panel = ui_Panel12,
        .left_img = ui_Image30,
        .right_img = ui_Image21,
        .label_a = ui_Label9,
        .label_b = ui_Label6,
    };
}

static void ui_s1_tile_bind_content(lv_obj_t *btn, ui_s1_tile_content_t *content)
{
    if (btn == NULL || content == NULL) {
        return;
    }
    lv_obj_remove_event_cb(btn, ui_s1_tile_press_content_cb);
    lv_obj_add_event_cb(btn, ui_s1_tile_press_content_cb, LV_EVENT_ALL, content);
    ui_s1_tile_apply_content(content, false);
}

/* 对 Screen1 卡片统一设置：无按压缩放，按压/长按背景天蓝色，内容反色。 */
void ui_runtime_screen1_tile_press_stable(lv_obj_t *btn)
{
    if (btn == NULL) {
        return;
    }
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    const lv_color_t c_def = lv_color_hex(UI_S1_TILE_BG_DEFAULT);
    const lv_color_t c_pr = lv_color_hex(UI_S1_TILE_BG_PRESSED);

    const lv_style_selector_t sels[] = {
        LV_PART_MAIN | LV_STATE_DEFAULT,
        LV_PART_MAIN | LV_STATE_PRESSED,
        LV_PART_MAIN | LV_STATE_FOCUSED,
        LV_PART_MAIN | LV_STATE_FOCUS_KEY,
        LV_PART_MAIN | LV_STATE_CHECKED,
        LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED),
    };
    for (unsigned i = 0; i < sizeof(sels) / sizeof(sels[0]); i++) {
        ui_runtime_screen1_tile_kill_grow(btn, sels[i]);
    }

    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_bg_opa(btn, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 0, LV_PART_INDICATOR | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, 0, LV_PART_INDICATOR | LV_STATE_FOCUSED);

    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x6B7F99), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(btn, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_bg_color(btn, c_def, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, c_pr, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, c_def, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(btn, c_pr, LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED));
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED));

    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t *parent = lv_obj_get_parent(btn);
    if (parent) {
        lv_obj_set_style_outline_width(parent, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_outline_width(parent, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(parent, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(parent, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(parent, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(parent, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    ui_s1_tile_refresh_content_ptrs();
    if (btn == ui_Button1) {
        ui_s1_tile_bind_content(btn, &s_content_btn1);
    } else if (btn == ui_Button3) {
        ui_s1_tile_bind_content(btn, &s_content_btn3);
    } else if (btn == ui_Button4) {
        ui_s1_tile_bind_content(btn, &s_content_btn4);
    }
}

/* Screen2 情景卡：按下/长按背景天蓝，抑制主题 grow。 */
void ui_runtime_screen2_card_press_sky(lv_obj_t *btn)
{
    if (btn == NULL) {
        return;
    }
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    const lv_style_selector_t sels[] = {
        LV_PART_MAIN | LV_STATE_DEFAULT,
        LV_PART_MAIN | LV_STATE_PRESSED,
        LV_PART_MAIN | LV_STATE_FOCUSED,
        LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED),
    };
    for (unsigned i = 0; i < sizeof(sels) / sizeof(sels[0]); i++) {
        ui_runtime_screen1_tile_kill_grow(btn, sels[i]);
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4F5C5C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 125, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_S1_TILE_BG_PRESSED), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_S1_TILE_BG_PRESSED),
                              LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED));
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED));
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
}

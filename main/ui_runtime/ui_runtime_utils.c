#include "lvgl.h"
#include "ui.h"

#include <stdbool.h>

/* 关闭对象阴影，减少重绘开销并降低闪烁概率。 */
void ui_runtime_disable_shadow(lv_obj_t *obj)
{
    if (obj == NULL) return;
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* 整型限幅工具函数。 */
int ui_runtime_clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* 通用显隐辅助函数。 */
void ui_runtime_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

/* 读取开关当前状态。 */
bool ui_runtime_switch_is_on(lv_obj_t *sw)
{
    if (sw == NULL) return false;
    return lv_obj_has_state(sw, LV_STATE_CHECKED);
}

/* 设置开关状态并刷新对象。 */
void ui_runtime_switch_set_on(lv_obj_t *sw, bool on)
{
    if (sw == NULL) return;
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw, LV_STATE_CHECKED);
    lv_obj_invalidate(sw);
}

/*
 * 覆盖 LVGL default theme 的 styles->grow / list_item_grow
 *（按下 transform_width/height 放大），避免按钮撑开破坏布局。
 */
static void ui_runtime_obj_kill_press_grow(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    static const lv_style_selector_t sels[] = {
        LV_PART_MAIN | LV_STATE_DEFAULT,
        LV_PART_MAIN | LV_STATE_PRESSED,
        LV_PART_MAIN | LV_STATE_FOCUSED,
        LV_PART_MAIN | LV_STATE_FOCUS_KEY,
        LV_PART_MAIN | LV_STATE_CHECKED,
        LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUSED),
        LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_CHECKED),
        LV_PART_MAIN | (LV_STATE_PRESSED | LV_STATE_FOCUS_KEY),
    };
    for (unsigned i = 0; i < sizeof(sels) / sizeof(sels[0]); i++) {
        lv_obj_set_style_transform_width(obj, 0, sels[i]);
        lv_obj_set_style_transform_height(obj, 0, sels[i]);
        lv_obj_set_style_transform_zoom(obj, LV_IMG_ZOOM_NONE, sels[i]);
        lv_obj_set_style_transform_angle(obj, 0, sels[i]);
    }
}

static bool ui_runtime_obj_is_pressable_btn(const lv_obj_t *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (lv_obj_check_type(obj, &lv_btn_class)) {
        return true;
    }
#if LV_USE_IMGBTN
    if (lv_obj_check_type(obj, &lv_imgbtn_class)) {
        return true;
    }
#endif
    return false;
}

static void ui_runtime_walk_kill_btn_grow(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    if (ui_runtime_obj_is_pressable_btn(obj)) {
        ui_runtime_obj_kill_press_grow(obj);
    }
    const uint32_t n = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < n; i++) {
        ui_runtime_walk_kill_btn_grow(lv_obj_get_child(obj, i));
    }
}

void ui_runtime_disable_btn_grow_on(lv_obj_t *root)
{
    ui_runtime_walk_kill_btn_grow(root);
}

void ui_runtime_disable_btn_grow_everywhere(void)
{
    lv_obj_t *const screens[] = {
        ui_Screen1, ui_Screen2, ui_Screen3, ui_Screen4,
        ui_Screen5, ui_Screen6, ui_Screen7, ui_Screen8,
        ui_Screen9, ui_Screen10, ui_Screen11, ui_Screen12,
        ui_Screen13, ui_Screen14, ui_Screen15, ui_Screen16,
    };
    for (unsigned i = 0; i < sizeof(screens) / sizeof(screens[0]); i++) {
        if (screens[i]) {
            ui_runtime_walk_kill_btn_grow(screens[i]);
        }
    }
}


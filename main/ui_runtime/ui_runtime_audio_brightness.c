#include "ui.h"
#include "ui_runtime.h"
#include "lvgl.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "withthewind_board_lvgl_init.h"

/* 外部工具函数（定义在 ui_runtime_utils.c）。 */
extern int ui_runtime_clamp_int(int v, int lo, int hi);

/* 由 ui_runtime.c 提供：用于在状态变化后做持久化。 */
extern void ui_runtime_ab_on_changed(void);
extern int ui_runtime_ab_get_last_brightness(void);

/* audio + brightness 运行时状态 */
static bool s_brightness_inited = false;
static int s_last_brightness = -1;
static int s_last_brightness_slider = -1;
static uint32_t s_last_brightness_tick = 0;
static bool s_sound_muted = true;
static esp_timer_handle_t s_beep_off_timer = NULL;

static void ui_extra_beep_off_cb(void *arg)
{
    (void)arg;
    (void)board_beep_set(0);
}

static void ui_extra_ensure_brightness_inited(void)
{
    if (s_brightness_inited) return;
    if (board_backlight_init() == ESP_OK) s_brightness_inited = true;
}

void ui_runtime_ab_apply_sound_state(void)
{
    (void)board_beep_set(0);

    if (ui_Image26 != NULL) {
        if (s_sound_muted) lv_img_set_src(ui_Image26, UI_SRC_SOUNDCLOSE);
        else lv_img_set_src(ui_Image26, UI_SRC_SOUNDOPEN);
    }
}

void ui_runtime_ab_beep_click_once(void)
{
    if (s_sound_muted) return;
    (void)board_beep_set(1);

    if (s_beep_off_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = ui_extra_beep_off_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "beep_off",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&timer_args, &s_beep_off_timer) != ESP_OK) {
            s_beep_off_timer = NULL;
            (void)board_beep_set(0);
            return;
        }
    }

    (void)esp_timer_stop(s_beep_off_timer);
    if (esp_timer_start_once(s_beep_off_timer, 30000) != ESP_OK) {
        (void)board_beep_set(0);
    }
}

static void ui_extra_event_button_click_beep(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_event_get_target(e) == ui_Button32) return;
    ui_runtime_ab_beep_click_once();
}

void ui_runtime_ab_bind_button_beep(lv_obj_t *btn)
{
    if (btn != NULL) lv_obj_add_event_cb(btn, ui_extra_event_button_click_beep, LV_EVENT_CLICKED, NULL);
}

void ui_runtime_ab_bind_default_buttons(void)
{
    ui_runtime_ab_bind_button_beep(ui_Button1);
    ui_runtime_ab_bind_button_beep(ui_Button2);
    ui_runtime_ab_bind_button_beep(ui_Button3);
    ui_runtime_ab_bind_button_beep(ui_Button4);
    ui_runtime_ab_bind_button_beep(ui_Button5);
    ui_runtime_ab_bind_button_beep(ui_Button6);
    ui_runtime_ab_bind_button_beep(ui_Button7);
    ui_runtime_ab_bind_button_beep(ui_Button8);
    ui_runtime_ab_bind_button_beep(ui_Button9);
    ui_runtime_ab_bind_button_beep(ui_Button10);
    ui_runtime_ab_bind_button_beep(ui_Button11);
    ui_runtime_ab_bind_button_beep(ui_Button12);
    ui_runtime_ab_bind_button_beep(ui_Button13);
    ui_runtime_ab_bind_button_beep(ui_Button14);
    ui_runtime_ab_bind_button_beep(ui_Button15);
    ui_runtime_ab_bind_button_beep(ui_Button16);
    ui_runtime_ab_bind_button_beep(ui_Button17);
    ui_runtime_ab_bind_button_beep(ui_Button18);
    ui_runtime_ab_bind_button_beep(ui_Button19);
    ui_runtime_ab_bind_button_beep(ui_Button20);
    ui_runtime_ab_bind_button_beep(ui_Button21);
    ui_runtime_ab_bind_button_beep(ui_Button22);
    ui_runtime_ab_bind_button_beep(ui_Button23);
    ui_runtime_ab_bind_button_beep(ui_Button24);
    ui_runtime_ab_bind_button_beep(ui_Button25);
    ui_runtime_ab_bind_button_beep(ui_Button26);
    ui_runtime_ab_bind_button_beep(ui_Button27);
    ui_runtime_ab_bind_button_beep(ui_Button28);
    ui_runtime_ab_bind_button_beep(ui_Button29);
    ui_runtime_ab_bind_button_beep(ui_Button30);
    ui_runtime_ab_bind_button_beep(ui_Button31);
    ui_runtime_ab_bind_button_beep(ui_Button32);
    ui_runtime_ab_bind_button_beep(ui_Button33);
    ui_runtime_ab_bind_button_beep(ui_Button36);
}

void ui_runtime_ab_apply_brightness(int brightness, bool force)
{
    s_last_brightness_slider = ui_runtime_clamp_int(brightness, 0, 100);
    s_last_brightness_slider = board_backlight_clamp_percent(s_last_brightness_slider);
    int band = board_backlight_quantize_percent(s_last_brightness_slider);

    if (!force && band == s_last_brightness) {
        return;
    }
    if (!force && lv_scr_act() != ui_Screen6) {
        return;
    }

    ui_extra_ensure_brightness_inited();

    esp_err_t err = board_backlight_set(band);
    if (err != ESP_OK) {
        s_brightness_inited = false;
        ui_extra_ensure_brightness_inited();
        (void)board_backlight_set(band);
    }

    s_last_brightness = band;
    s_last_brightness_tick = (uint32_t)lv_tick_get();
}

static void ui_extra_event_slider1_brightness(lv_event_t *e)
{
    if (ui_Slider1 == NULL) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(e);
    if (lv_scr_act() != ui_Screen6) {
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        /* v1.2：拖动时直接写背光，仅 invalidate 滑条，不强制全屏 refr。 */
        int val = (int)lv_slider_get_value(ui_Slider1);
        ui_runtime_ab_apply_brightness(val, false);
        lv_obj_invalidate(ui_Slider1);
        return;
    }
    if (code != LV_EVENT_RELEASED) {
        return;
    }

    int val = (int)lv_slider_get_value(ui_Slider1);
    ui_runtime_ab_apply_brightness(val, true);
    ui_runtime_ab_on_changed();
    val = ui_runtime_ab_get_last_brightness();
    if (val >= 0) {
        lv_slider_set_value(ui_Slider1, val, LV_ANIM_OFF);
    }
    lv_obj_invalidate(ui_Slider1);
}

static void ui_extra_event_button32_sound_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_scr_act() != ui_Screen6) return;

    s_sound_muted = !s_sound_muted;
    ui_runtime_ab_apply_sound_state();
    ui_runtime_ab_beep_click_once();
    ui_runtime_ab_on_changed();
    if (ui_Image26) {
        lv_obj_invalidate(ui_Image26);
    }
}

void ui_runtime_ab_bind_apply_for_screen6(bool *screen6_bound)
{
    if (screen6_bound && !(*screen6_bound)) {
        if (ui_Slider1) {
            lv_obj_add_event_cb(ui_Slider1, ui_extra_event_slider1_brightness, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_add_event_cb(ui_Slider1, ui_extra_event_slider1_brightness, LV_EVENT_RELEASED, NULL);
        }
        if (ui_Button32) lv_obj_add_event_cb(ui_Button32, ui_extra_event_button32_sound_toggle, LV_EVENT_CLICKED, NULL);
        *screen6_bound = true;
    }

    if (ui_Slider1) {
        lv_slider_set_range(ui_Slider1,
                            CONFIG_BOARD_BACKLIGHT_MIN_PERCENT,
                            100);
        int slider_val = (s_last_brightness_slider >= 0) ? s_last_brightness_slider : (int)lv_slider_get_value(ui_Slider1);
        slider_val = board_backlight_clamp_percent(ui_runtime_clamp_int(slider_val, 0, 100));
        lv_slider_set_value(ui_Slider1, slider_val, LV_ANIM_OFF);
        ui_runtime_ab_apply_brightness(slider_val, s_last_brightness < 0);
    }
    ui_runtime_ab_apply_sound_state();
}

bool ui_runtime_ab_get_sound_muted(void) { return s_sound_muted; }
void ui_runtime_ab_set_sound_muted(bool muted) { s_sound_muted = muted; }
int ui_runtime_ab_get_last_brightness(void) { return s_last_brightness; }
int ui_runtime_ab_get_last_brightness_slider(void) { return s_last_brightness_slider; }
void ui_runtime_ab_set_last_brightness(int v) { s_last_brightness = v; }
void ui_runtime_ab_set_last_brightness_slider(int v) { s_last_brightness_slider = v; }
void ui_runtime_ab_mark_boot_default_100(void)
{
    s_last_brightness = 100;
    s_last_brightness_slider = 100;
    s_last_brightness_tick = (uint32_t)lv_tick_get();
    s_brightness_inited = true;
}

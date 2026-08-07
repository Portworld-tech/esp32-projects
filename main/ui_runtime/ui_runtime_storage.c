#include "ui.h"
#include "lvgl.h"

#include "nvs.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "withthewind_board_lvgl_init.h"

/* 由 ui_runtime.c 提供的共享状态 */
extern int s_screen3_temp;
extern int s_screen5_temp;
extern bool s_power_sw1;
extern bool s_power_sw3;
extern bool s_power_sw4;
extern int s_screen10_selected_image;
extern bool s_screen10_selection_loaded_from_nvs;
extern bool s_screen10_selection_needs_repair;
extern bool s_ui_loading_nvs_settings;
extern bool s_ui_settings_loaded_once;
extern bool s_nvs_settings_logged;
extern uint32_t s_nvs_last_save_log_tick;

/* 由其它模块提供的能力 */
extern void ui_runtime_ab_set_last_brightness_slider(int v);
extern void ui_runtime_ab_apply_brightness(int brightness, bool force);
extern void ui_runtime_ab_set_sound_muted(bool muted);
extern void ui_runtime_ab_apply_sound_state(void);
extern bool ui_runtime_ab_get_sound_muted(void);
extern int ui_runtime_ab_get_last_brightness(void);
extern int ui_runtime_ab_get_last_brightness_slider(void);

#define UI_EXTRA_TEMP_NS        "ui_extra_temp"
#define UI_EXTRA_KEY_SCREEN3    "screen3_temp"
#define UI_EXTRA_KEY_SCREEN5    "screen5_temp"
#define UI_EXTRA_KEY_BRIGHTNESS "brightness"
#define UI_EXTRA_KEY_SOUND      "sound"
#define UI_EXTRA_KEY_POWER_SW1  "power_sw1"
#define UI_EXTRA_KEY_POWER_SW3  "power_sw3"
#define UI_EXTRA_KEY_POWER_SW4  "power_sw4"
#define UI_EXTRA_KEY_SC10_IMAGE "screen10_img"

static const char *TAG_STORAGE = "ui_storage";

static bool s_storage_inited = false;
static lv_timer_t *s_settings_save_debounce_timer = NULL;

static void ui_runtime_storage_init(void)
{
    if (s_storage_inited) {
        return;
    }
    /* NVS is initialized once in app_main / wifi_management_foundation_init. */
    s_storage_inited = true;
}

static void ui_runtime_storage_save_settings_now(void)
{
    if (s_ui_loading_nvs_settings || !s_ui_settings_loaded_once) {
        return;
    }
    ui_runtime_storage_init();

    nvs_handle_t h;
    if (nvs_open(UI_EXTRA_TEMP_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }

    if (ui_runtime_ab_get_last_brightness_slider() >= 0) {
        (void)nvs_set_u32(h, UI_EXTRA_KEY_BRIGHTNESS, (uint32_t)ui_runtime_ab_get_last_brightness_slider());
    } else if (ui_runtime_ab_get_last_brightness() >= 0) {
        (void)nvs_set_u32(h, UI_EXTRA_KEY_BRIGHTNESS, (uint32_t)ui_runtime_ab_get_last_brightness());
    }
    uint8_t snd = ui_runtime_ab_get_sound_muted() ? 1 : 0;
    (void)nvs_set_u8(h, UI_EXTRA_KEY_SOUND, snd);

    (void)nvs_set_u8(h, UI_EXTRA_KEY_POWER_SW1, s_power_sw1 ? 1 : 0);
    (void)nvs_set_u8(h, UI_EXTRA_KEY_POWER_SW3, s_power_sw3 ? 1 : 0);
    (void)nvs_set_u8(h, UI_EXTRA_KEY_POWER_SW4, s_power_sw4 ? 1 : 0);
    (void)nvs_set_u8(h, UI_EXTRA_KEY_SC10_IMAGE, (uint8_t)s_screen10_selected_image);

    (void)nvs_commit(h);
    nvs_close(h);

    uint32_t now = (uint32_t)lv_tick_get();
    if (now - s_nvs_last_save_log_tick > 500U) {
        s_nvs_last_save_log_tick = now;
        ESP_LOGI("ui_runtime", "NVS save: sound_muted=%d sw1=%d sw3=%d sw4=%d bright_slider=%d",
                 (int)ui_runtime_ab_get_sound_muted(),
                 (int)s_power_sw1, (int)s_power_sw3, (int)s_power_sw4,
                 (int)ui_runtime_ab_get_last_brightness_slider());
    }
}

static void ui_runtime_storage_settings_debounce_cb(lv_timer_t *t)
{
    (void)t;
    s_settings_save_debounce_timer = NULL;
    ui_runtime_storage_save_settings_now();
    ESP_LOGD(TAG_STORAGE, "settings NVS committed (debounced)");
}

void ui_runtime_storage_load_temps(void)
{
    ui_runtime_storage_init();

    nvs_handle_t h;
    if (nvs_open(UI_EXTRA_TEMP_NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    uint32_t v = 0;
    if (nvs_get_u32(h, UI_EXTRA_KEY_SCREEN3, &v) == ESP_OK) {
        s_screen3_temp = (int)v;
    }
    if (nvs_get_u32(h, UI_EXTRA_KEY_SCREEN5, &v) == ESP_OK) {
        s_screen5_temp = (int)v;
    }
    nvs_close(h);
}

void ui_runtime_storage_save_temps(void)
{
    ui_runtime_storage_init();

    nvs_handle_t h;
    if (nvs_open(UI_EXTRA_TEMP_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u32(h, UI_EXTRA_KEY_SCREEN3, (uint32_t)s_screen3_temp);
    (void)nvs_set_u32(h, UI_EXTRA_KEY_SCREEN5, (uint32_t)s_screen5_temp);
    (void)nvs_commit(h);
    nvs_close(h);
}

void ui_runtime_storage_load_settings(void)
{
    ui_runtime_storage_init();
    s_ui_loading_nvs_settings = true;
    s_ui_settings_loaded_once = false;

    nvs_handle_t h;
    if (nvs_open(UI_EXTRA_TEMP_NS, NVS_READONLY, &h) != ESP_OK) {
        s_ui_loading_nvs_settings = false;
        return;
    }

    uint32_t b = 0;
    if (nvs_get_u32(h, UI_EXTRA_KEY_BRIGHTNESS, &b) == ESP_OK) {
        int saved = (int)b;
        if (saved < 0) {
            saved = 0;
        }
        if (saved > 100) {
            saved = 100;
        }
        int slider_val = saved;
#if CONFIG_BOARD_BACKLIGHT_MIN_PERCENT > 0
        if (slider_val > 0 && slider_val < CONFIG_BOARD_BACKLIGHT_MIN_PERCENT) {
            slider_val = CONFIG_BOARD_BACKLIGHT_MIN_PERCENT;
        }
#else
        if (slider_val == 20) {
            slider_val = 0;
        }
#endif
        slider_val = board_backlight_clamp_percent(slider_val);
        if (ui_Slider1) {
            lv_slider_set_range(ui_Slider1, CONFIG_BOARD_BACKLIGHT_MIN_PERCENT, 100);
            lv_slider_set_value(ui_Slider1, slider_val, LV_ANIM_OFF);
        }
        ui_runtime_ab_set_last_brightness_slider(slider_val);
        ui_runtime_ab_apply_brightness(slider_val, true);
    }

    uint8_t snd = 0;
    if (nvs_get_u8(h, UI_EXTRA_KEY_SOUND, &snd) == ESP_OK) {
        ui_runtime_ab_set_sound_muted(snd != 0);
        ui_runtime_ab_apply_sound_state();
    }

    uint8_t p = 0;
    if (nvs_get_u8(h, UI_EXTRA_KEY_POWER_SW1, &p) == ESP_OK) {
        s_power_sw1 = (p != 0);
    }
    if (nvs_get_u8(h, UI_EXTRA_KEY_POWER_SW3, &p) == ESP_OK) {
        s_power_sw3 = (p != 0);
    }
    if (nvs_get_u8(h, UI_EXTRA_KEY_POWER_SW4, &p) == ESP_OK) {
        s_power_sw4 = (p != 0);
    }

    uint8_t sc10 = 0;
    s_screen10_selection_loaded_from_nvs = false;
    s_screen10_selection_needs_repair = false;
    if (nvs_get_u8(h, UI_EXTRA_KEY_SC10_IMAGE, &sc10) == ESP_OK) {
        if (sc10 >= 1 && sc10 <= 4) {
            s_screen10_selected_image = (int)sc10;
            s_screen10_selection_loaded_from_nvs = true;
        } else {
            s_screen10_selected_image = 1;
            s_screen10_selection_needs_repair = true;
        }
    }

    if (!s_nvs_settings_logged) {
        s_nvs_settings_logged = true;
        ESP_LOGI("ui_runtime", "NVS load: sound_muted=%d sw1=%d sw3=%d sw4=%d bright_slider=%d bright_hw=%d",
                 (int)ui_runtime_ab_get_sound_muted(),
                 (int)s_power_sw1, (int)s_power_sw3, (int)s_power_sw4,
                 (int)ui_runtime_ab_get_last_brightness_slider(), (int)ui_runtime_ab_get_last_brightness());
    }

    nvs_close(h);
    s_ui_loading_nvs_settings = false;
    s_ui_settings_loaded_once = true;
}

void ui_runtime_storage_save_settings(void)
{
    if (s_settings_save_debounce_timer != NULL) {
        lv_timer_reset(s_settings_save_debounce_timer);
        return;
    }
    s_settings_save_debounce_timer = lv_timer_create(ui_runtime_storage_settings_debounce_cb, 500, NULL);
    if (s_settings_save_debounce_timer != NULL) {
        lv_timer_set_repeat_count(s_settings_save_debounce_timer, 1);
    } else {
        ui_runtime_storage_save_settings_now();
    }
}

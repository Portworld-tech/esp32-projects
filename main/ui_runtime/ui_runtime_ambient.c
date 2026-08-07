#include "ui_runtime_ambient.h"

#include "ui_runtime.h"
#include "ui.h"

#include "sdkconfig.h"
#include "withthewind_board_lvgl_init.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lvgl.h"
#include "lv_font_source_han_ambient_time_150.h"
#include "src/draw/lv_img_cache.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

extern int ui_runtime_ab_get_last_brightness(void);

static const char *TAG = "ui_ambient";

#define AMB_NVS_NS "amb_cfg"
#define AMB_KEY_TMO "tmo_s"
#define AMB_KEY_EN "en"
#define AMB_KEY_WS "ws"
#define AMB_KEY_WE "we"
#define AMB_KEY_DH0 "dh0"
#define AMB_KEY_DH1 "dh1"

static uint32_t s_idle_sec_cfg = 120u;
static bool s_amb_ss_en = true;
static uint8_t s_dim_h0 = 0u;
static uint8_t s_dim_h1 = 23u;

static uint32_t amb_default_timeout_sec(void)
{
#if defined(CONFIG_UI_AMBIENT_TIMEOUT_SEC)
    return (uint32_t)CONFIG_UI_AMBIENT_TIMEOUT_SEC;
#else
    return 120u;
#endif
}

static bool amb_timeout_valid(uint32_t s)
{
    static const uint32_t allowed[] = {15u, 30u, 60u, 120u, 300u, 600u};
    for (unsigned i = 0; i < (unsigned)(sizeof(allowed) / sizeof(allowed[0])); i++) {
        if (allowed[i] == s) {
            return true;
        }
    }
    return false;
}

void ui_runtime_ambient_load_settings(void)
{
    uint32_t def = amb_default_timeout_sec();
    if (!amb_timeout_valid(def)) {
        def = 120u;
    }
    s_idle_sec_cfg = def;
    s_amb_ss_en = true;

    nvs_handle_t h;
    if (nvs_open(AMB_NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    uint32_t t = 0;
    if (nvs_get_u32(h, AMB_KEY_TMO, &t) == ESP_OK && amb_timeout_valid(t)) {
        s_idle_sec_cfg = t;
    }

    uint8_t en = 1;
    if (nvs_get_u8(h, AMB_KEY_EN, &en) == ESP_OK) {
        s_amb_ss_en = (en != 0);
    }

    s_dim_h0 = 0u;
    s_dim_h1 = 23u;
    uint8_t h0 = 255u;
    uint8_t h1 = 255u;
    if (nvs_get_u8(h, AMB_KEY_DH0, &h0) == ESP_OK && nvs_get_u8(h, AMB_KEY_DH1, &h1) == ESP_OK && h0 <= 23u && h1 <= 23u) {
        s_dim_h0 = h0;
        s_dim_h1 = h1;
    } else {
        uint16_t ws = 0;
        uint16_t we = 1439;
        if (nvs_get_u16(h, AMB_KEY_WS, &ws) == ESP_OK && ws < (24u * 60u)) {
            s_dim_h0 = (uint8_t)(ws / 60u);
        }
        if (nvs_get_u16(h, AMB_KEY_WE, &we) == ESP_OK && we < (24u * 60u)) {
            s_dim_h1 = (uint8_t)(we / 60u);
        }
    }

    nvs_close(h);
}

uint32_t ui_runtime_ambient_get_idle_timeout_sec(void)
{
    return s_idle_sec_cfg;
}

void ui_runtime_ambient_set_idle_timeout_sec(uint32_t sec)
{
    if (!amb_timeout_valid(sec)) {
        return;
    }
    s_idle_sec_cfg = sec;

    nvs_handle_t h;
    if (nvs_open(AMB_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u32(h, AMB_KEY_TMO, s_idle_sec_cfg);
    (void)nvs_commit(h);
    nvs_close(h);
}

bool ui_runtime_ambient_screensaver_enabled(void)
{
    return s_amb_ss_en;
}

void ui_runtime_ambient_set_screensaver_enabled(bool on)
{
    s_amb_ss_en = on;

    nvs_handle_t h;
    if (nvs_open(AMB_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u8(h, AMB_KEY_EN, on ? 1u : 0u);
    (void)nvs_commit(h);
    nvs_close(h);
}

void ui_runtime_ambient_get_schedule_hours(uint8_t *start_hour_out, uint8_t *end_hour_out)
{
    if (start_hour_out != NULL) {
        *start_hour_out = s_dim_h0;
    }
    if (end_hour_out != NULL) {
        *end_hour_out = s_dim_h1;
    }
}

void ui_runtime_ambient_set_schedule_hours(uint8_t start_hour, uint8_t end_hour)
{
    if (start_hour > 23u || end_hour > 23u) {
        return;
    }
    s_dim_h0 = start_hour;
    s_dim_h1 = end_hour;

    nvs_handle_t h;
    if (nvs_open(AMB_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u8(h, AMB_KEY_DH0, s_dim_h0);
    (void)nvs_set_u8(h, AMB_KEY_DH1, s_dim_h1);
    (void)nvs_commit(h);
    nvs_close(h);
}

bool ui_runtime_ambient_schedule_allows_dim_now(void)
{
    time_t now = 0;
    struct tm info = {0};
    time(&now);
    localtime_r(&now, &info);
    int ch = info.tm_hour;
    int h0 = (int)s_dim_h0;
    int h1 = (int)s_dim_h1;

    if (h0 <= h1) {
        return ch >= h0 && ch <= h1;
    }
    return ch >= h0 || ch <= h1;
}

#if defined(CONFIG_UI_AMBIENT_ENABLE) && CONFIG_UI_AMBIENT_ENABLE

static lv_obj_t *s_amb_scr;
static lv_obj_t *s_lbl_time;
static lv_obj_t *s_lbl_date;
static lv_obj_t *s_lbl_tip;

static lv_timer_t *s_idle_timer;
static lv_timer_t *s_clock_timer;

static lv_obj_t *s_prev_scr;
static volatile bool s_ambient_active;

static void ambient_apply_clock(void);
static void ambient_exit_now(void);

static void ambient_clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_ambient_active || s_amb_scr == NULL || lv_scr_act() != s_amb_scr) {
        return;
    }
    ambient_apply_clock();
}

static void ambient_apply_clock(void)
{
    time_t now = 0;
    struct tm info = {0};
    time(&now);
    localtime_r(&now, &info);

    char time_buf[32];
    char date_buf[48];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &info);

    static const char *mon_abbr[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static const char *wday_abbr[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    const char *m = (info.tm_mon >= 0 && info.tm_mon < 12) ? mon_abbr[info.tm_mon] : "";
    const char *w = (info.tm_wday >= 0 && info.tm_wday < 7) ? wday_abbr[info.tm_wday] : "";
    /* Month, day, weekday (no year), e.g. "May 11 Wed" */
    snprintf(date_buf, sizeof(date_buf), "%s %d %s", m, info.tm_mday, w);

    if (s_lbl_time) {
        lv_label_set_text(s_lbl_time, time_buf);
    }
    if (s_lbl_date) {
        lv_label_set_text(s_lbl_date, date_buf);
    }
}

static void ambient_scr_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ambient_exit_now();
}

static void ambient_indev_wait_all_release(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        const lv_indev_type_t t = lv_indev_get_type(indev);
        if (t == LV_INDEV_TYPE_POINTER || t == LV_INDEV_TYPE_BUTTON) {
            lv_indev_wait_release(indev);
        }
        indev = lv_indev_get_next(indev);
    }
}

static void ambient_exit_now(void)
{
    if (!s_ambient_active) {
        return;
    }
    s_ambient_active = false;

    if (s_clock_timer) {
        lv_timer_pause(s_clock_timer);
    }

    ambient_indev_wait_all_release();

    lv_obj_t *back = s_prev_scr;
    if (back == NULL) {
        back = ui_Screen1;
    }
    s_prev_scr = NULL;
    ui_runtime_screen_change_by_obj(back);

    int br = ui_runtime_ab_get_last_brightness();
#if CONFIG_BOARD_BACKLIGHT_VIA_IO_EXPANDER
    if (br > 0 && br < CONFIG_BOARD_BACKLIGHT_MIN_PERCENT) {
        br = CONFIG_BOARD_BACKLIGHT_MIN_PERCENT;
    }
#else
    if (br < 20) {
        br = 80;
    }
#endif
    (void)board_backlight_set(br);

    lv_disp_t *d = lv_disp_get_default();
    if (d != NULL) {
        lv_disp_trig_activity(d);
    }
}

static void ambient_idle_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_amb_scr == NULL || s_ambient_active) {
        return;
    }
    if (!s_amb_ss_en) {
        return;
    }
    if (ui_runtime_screen_switch_busy()) {
        ESP_LOGD(TAG, "idle: skip ambient (screen switch busy)");
        return;
    }

    lv_disp_t *d = lv_disp_get_default();
    if (d == NULL) {
        return;
    }
    lv_obj_t *act = lv_scr_act();
    if (act == NULL || act == s_amb_scr) {
        return;
    }

    const uint32_t need_ms = s_idle_sec_cfg * 1000u;
    if (lv_disp_get_inactive_time(d) < need_ms) {
        return;
    }
    if (!ui_runtime_ambient_schedule_allows_dim_now()) {
        return;
    }

    s_prev_scr = act;
    s_ambient_active = true;

#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    /* Release decoded PNG buffers before idle clock screen — long-run PSRAM stability. */
    lv_img_cache_invalidate_src(NULL);
#endif

    lv_scr_load(s_amb_scr);
    ambient_apply_clock();
    (void)board_backlight_set(board_backlight_quantize_percent(CONFIG_UI_AMBIENT_BACKLIGHT_PCT));

    if (s_clock_timer) {
        lv_timer_resume(s_clock_timer);
    }
}

void ui_runtime_ambient_init(void)
{
    if (s_amb_scr != NULL) {
        return;
    }

    ui_runtime_ambient_load_settings();

    s_amb_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_amb_scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_grad_dir(s_amb_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_amb_scr, lv_color_hex(0x0A1022), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(s_amb_scr, lv_color_hex(0x020308), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_amb_scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_amb_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_amb_scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_amb_scr, ambient_scr_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *card = lv_obj_create(s_amb_scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(86));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(card, 44, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(card, 40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(card, 36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(card, 36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x151E32), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(0x0C121F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 235, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, 36, LV_PART_MAIN | LV_STATE_DEFAULT);
    /* 无边框/无投影，避免在暗背景下像「刻度框线」或分层条纹 */
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    s_lbl_time = lv_label_create(card);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(0xF2F6FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(s_lbl_time, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(s_lbl_time, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_source_han_ambient_time_150, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(s_lbl_time, "--:--");

    s_lbl_date = lv_label_create(card);
    lv_obj_set_style_text_color(s_lbl_date, lv_color_hex(0xB8C9E6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(s_lbl_date, 230, LV_PART_MAIN | LV_STATE_DEFAULT);
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
#elif LV_FONT_MONTSERRAT_30
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
#elif LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
#else
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
    lv_label_set_text(s_lbl_date, "");

    s_lbl_tip = lv_label_create(card);
    lv_obj_set_width(s_lbl_tip, lv_pct(100));
    lv_obj_set_style_text_align(s_lbl_tip, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_lbl_tip, lv_color_hex(0x7D92B8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(s_lbl_tip, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_lbl_tip, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
#else
    lv_obj_set_style_text_font(s_lbl_tip, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
    lv_label_set_text(s_lbl_tip, "tick screen to wake");
    lv_obj_clear_flag(s_lbl_tip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_clear_flag(s_lbl_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_lbl_date, LV_OBJ_FLAG_CLICKABLE);

    s_idle_timer = lv_timer_create(ambient_idle_timer_cb, 1000, NULL);
    if (s_idle_timer) {
        lv_timer_set_repeat_count(s_idle_timer, -1);
    }

    s_clock_timer = lv_timer_create(ambient_clock_timer_cb, 1000, NULL);
    if (s_clock_timer) {
        lv_timer_set_repeat_count(s_clock_timer, -1);
        lv_timer_pause(s_clock_timer);
    }

    ESP_LOGI(TAG, "ambient UI init: idle=%lus ss_en=%d BL=%d%% time=SourceHan150",
             (unsigned long)s_idle_sec_cfg, s_amb_ss_en ? 1 : 0, CONFIG_UI_AMBIENT_BACKLIGHT_PCT);
}

bool ui_runtime_ambient_is_active(void)
{
    return s_ambient_active;
}

#else /* !CONFIG_UI_AMBIENT_ENABLE */

void ui_runtime_ambient_init(void)
{
    ui_runtime_ambient_load_settings();
}

bool ui_runtime_ambient_is_active(void)
{
    return false;
}

#endif

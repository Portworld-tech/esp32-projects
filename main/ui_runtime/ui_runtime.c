#include "ui_runtime.h"

#include "ui_runtime_ambient.h"
#include "core/lv_disp.h"
#include "core/lv_refr.h"
#include "ui.h"
#include "ui_helpers.h"
#include "ui_runtime_screen345.h"
#include "withthewind_board_lvgl_init.h"
#include "lvgl.h"
#include "lv_font_source_han_screen1_time_100.h"
#include "wifi_management.h"
#include "bt_management.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string.h>

/* 云同步入口（定义在 ui_runtime_cloud.c）。 */
void ui_runtime_cloud_notify_changed(void);

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG_UI_SW = "ui_switch";

/* 手势切屏：S1↔S2、S6 hub、S3–S5 横向均即时切换。 */
#define UI_SWITCH_UNLOCK_MS_DEFAULT  8U
#define UI_SWITCH_UNLOCK_MS_HOME_PAGER 0U
#define UI_SWITCH_UNLOCK_MS_HUB      0U
#define UI_SWITCH_UNLOCK_MS_345      0U

/*
 * 注意（给以后维护的人看）：
 * 1) 所有“重要逻辑修复”尽量集中在本文件完成，而不是直接改 SquareLine 生成的 ui 源码：
 *    - Screen1/Screen3/4/5 的整体开关机联动、NVS 持久化；
 *    - WiFi SSID 中文字体、下拉刷新与密码清空逻辑；
 *    - 时间日期格式（示例：Apr 3, Fri）；
 *    - 手势导航、按键点击效果、蜂鸣器与背光亮度等。
 *
 * 2) ui 目录中的代码主要视为“布局 + 资源声明”，可以在 SquareLine 中重新导出。
 *    若重新导出 ui：
 *      - 不要在 ui 中重新加入业务逻辑（例如 WiFi 连接、SSID 刷新等），保持这些逻辑仅存在于本文件；
 *      - 若 SquareLine 生成了占位图片符号（例如 ui__temporary_image），应在 ui 工程中改为正式图片，
 *        或参考当前版本的 ui_Screen1.c，将其替换为已有的 ui_img_xxx 资源，避免重新出现链接错误；
 *      - 若生成的 ui.c 再次包含多余的 (e); 或未使用的 target 变量，可简单删除，或仿照当前版本精简。
 *
 * 3) 若添加了新的 ui_img 源文件或 ui_Screen 源文件而链接缺少符号，可以通过：
 *      - 执行 idf.py fullclean 再 idf.py build，或
 *      - 删除 build 目录后重新配置编译，
 *    让 main/CMakeLists.txt 中使用的 file(GLOB ...) 重新扫描 ui/images 与 ui/screens。
 */


/* 工具模块入口（定义在 ui_runtime_utils.c）。 */
void ui_runtime_disable_shadow(lv_obj_t *obj);
int ui_runtime_clamp_int(int v, int lo, int hi);
void ui_runtime_set_hidden(lv_obj_t *obj, bool hidden);
bool ui_runtime_switch_is_on(lv_obj_t *sw);
void ui_runtime_switch_set_on(lv_obj_t *sw, bool on);

int s_screen3_temp = 16;
int s_screen5_temp = 16;
/* Overall power states (persist in NVS) */
bool s_power_sw1 = true;
bool s_power_sw3 = true;
bool s_power_sw4 = true;
int s_screen10_selected_image = 1; /* 1..4: Screen10 选中的主题图 */
bool s_screen10_selection_loaded_from_nvs = false;
bool s_screen10_selection_needs_repair = false;
/* pick_and_home 已 apply 主题时，跳过 SCREEN_LOADED 上的二次 apply。 */
static bool s_skip_next_s1_theme_apply = false;
static bool s_ui_switch_lock = false;
static lv_timer_t *s_ui_unlock_timer = NULL;
static lv_timer_t *s_ui_full_refresh_restore_timer = NULL;
static uint8_t s_ui_full_refresh_prev = 0;
static bool s_ui_full_refresh_active = false;
static lv_timer_t *s_ui_boot_prewarm_timer = NULL;

/* 切屏忙时：只保留最后一次「立即切屏」请求，解锁后执行，避免连点丢事件叠绘。 */
static bool s_ui_switch_queued_valid;
static lv_obj_t **s_ui_switch_queued_target;
static lv_scr_load_anim_t s_ui_switch_queued_fademode;
static int s_ui_switch_queued_spd;
static int s_ui_switch_queued_delay;
static void (*s_ui_switch_queued_init)(void);
static uint32_t s_ui_switch_seq;
static lv_obj_t *s_ui_switch_from_scr;

/* While restoring NVS values we must not call ui_runtime_storage_save_settings(),
 * otherwise we will overwrite stored values with in-memory defaults. */
bool s_ui_loading_nvs_settings = false;
/* ui_init() 已创建全部 Screen；切换时仍须 bind_apply / 状态还原。 */
bool s_screen3_bound = false;
bool s_screen4_bound = false;
bool s_screen5_bound = false;
static bool s_screen6_bound = false;
bool s_nvs_settings_logged = false;
uint32_t s_nvs_last_save_log_tick = 0;
/* 防止“开机阶段默认值”把 NVS 覆盖掉：
 * 只有完成一次 load_settings() 后，才允许 save_settings() 写入。 */
bool s_ui_settings_loaded_once = false;

/* Forward declaration: used before its definition */
void ui_runtime_storage_save_settings(void);
void ui_runtime_storage_load_settings(void);
static bool ui_extra_is_home_pager_pair(lv_obj_t *from, lv_obj_t *to);
static bool ui_extra_is_hub_screen(const lv_obj_t *scr);
static bool ui_extra_is_screen345(const lv_obj_t *scr);
static bool ui_extra_is_hub_nav_pair(lv_obj_t *from, lv_obj_t *to);
static bool ui_extra_use_lightweight_switch(lv_obj_t *from, lv_obj_t *to);
static void ui_extra_gesture_switch_to_screen6(void);
void ui_runtime_storage_load_temps(void);
static void ui_extra_bind_apply_for_active_screen(void);
static void ui_runtime_bind_all_screens(void);
static void ui_runtime_on_screen_loaded(lv_event_t *e);
void ui_runtime_screen1_tile_press_stable(lv_obj_t *btn);
void ui_runtime_screen2_card_press_sky(lv_obj_t *btn);
static void ui_runtime_register_screen_loaded(lv_obj_t *scr);
static void ui_extra_screen_change_no_wait(lv_obj_t **target,
                                           lv_scr_load_anim_t fademode,
                                           int spd,
                                           int delay,
                                           void (*target_init)(void));
static bool ui_runtime_disp_uses_full_frame_buffer(lv_disp_t *disp);
static void ui_runtime_apply_screen10_image_selection(void);
static void ui_runtime_screen10_on_loaded(lv_event_t *e);
static void ui_extra_screen10_theme_pick_cb(lv_event_t *e);
static void ui_runtime_boot_prewarm_start(void);
static void ui_runtime_boot_prewarm_cb(lv_timer_t *t);
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
static const void *ui_runtime_theme_src_for_pick(uint8_t pick);
#endif

/* 网络/Wi-Fi 模块入口与回调（定义在 ui_runtime_network.c）。 */
void ui_extra_screen7_wifi_scan_cb(lv_event_t *e);
void ui_extra_event_dropdown1(lv_event_t *e);
void ui_extra_event_textarea1(lv_event_t *e);
void ui_extra_event_keyboard2(lv_event_t *e);
void ui_extra_apply_wifi_ssid_runtime(void);
void ui_runtime_network_refresh_labels(void);

/* 定时器模块入口（定义在 ui_runtime_timers.c）。 */
void ui_runtime_timers_start(void);

/* Audio + Brightness 模块入口（定义在 ui_runtime_audio_brightness.c）。 */
void ui_runtime_ab_apply_sound_state(void);
void ui_runtime_ab_beep_click_once(void);
void ui_runtime_ab_bind_button_beep(lv_obj_t *btn);
void ui_runtime_ab_bind_default_buttons(void);
void ui_runtime_ab_apply_brightness(int brightness, bool force);
void ui_runtime_ab_bind_apply_for_screen6(bool *screen6_bound);
bool ui_runtime_ab_get_sound_muted(void);
void ui_runtime_ab_set_sound_muted(bool muted);
int ui_runtime_ab_get_last_brightness(void);
int ui_runtime_ab_get_last_brightness_slider(void);
void ui_runtime_ab_set_last_brightness(int v);
void ui_runtime_ab_set_last_brightness_slider(int v);
void ui_runtime_ab_mark_boot_default_100(void);

/* 由 audio_brightness 模块在状态变化后调用。 */
void ui_runtime_ab_on_changed(void)
{
    ui_runtime_storage_save_settings();
}

bool ui_runtime_screen_switch_busy(void)
{
    return s_ui_switch_lock;
}

/* 按当前激活屏幕执行对应 bind/apply 与运行时刷新。 */
static void ui_extra_bind_apply_for_active_screen(void)
{
    lv_obj_t *act = lv_scr_act();
    if (act == ui_Screen6) {
        ui_runtime_ab_bind_apply_for_screen6(&s_screen6_bound);
    } else {
        ui_runtime_screen345_bind_apply_for_active_screen(act);
    }
    if (act == ui_Screen1) {
        /* S2→S1：背景未变。pick_and_home 已 apply：避免二次 set_src。 */
        if (s_skip_next_s1_theme_apply) {
            s_skip_next_s1_theme_apply = false;
        } else if (s_ui_switch_from_scr != ui_Screen2) {
            ui_runtime_apply_screen10_image_selection();
        }
    } else if (act == ui_Screen11) {
        ui_runtime_network_refresh_labels();
    }
}

static void ui_runtime_bind_all_screens(void)
{
    if (ui_Screen3) {
        ui_runtime_screen345_bind_apply_for_active_screen(ui_Screen3);
    }
    if (ui_Screen4) {
        ui_runtime_screen345_bind_apply_for_active_screen(ui_Screen4);
    }
    if (ui_Screen5) {
        ui_runtime_screen345_bind_apply_for_active_screen(ui_Screen5);
    }
    if (ui_Screen6) {
        ui_runtime_ab_bind_apply_for_screen6(&s_screen6_bound);
    }
}

static void ui_runtime_on_screen_loaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }

    ui_extra_bind_apply_for_active_screen();
    /* 只处理当前屏：全树遍历会拖慢本帧 RGB flush，易撕裂。 */
    ui_runtime_disable_btn_grow_on(lv_scr_act());
    /* 轻量切屏：仅 invalidate，避免 lv_refr_now 整屏同步刷新造成卡顿。 */
    if (ui_extra_use_lightweight_switch(s_ui_switch_from_scr, lv_scr_act())) {
        ui_runtime_rgb_commit_active_screen();
    } else {
        ui_runtime_rgb_commit_full_screen();
    }
}

static void ui_runtime_register_screen_loaded(lv_obj_t *scr)
{
    if (scr != NULL) {
        lv_obj_add_event_cb(scr, ui_runtime_on_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    }
}

void ui_runtime_screen_change_by_obj(lv_obj_t *scr)
{
    if (scr == NULL) {
        return;
    }
    if (scr == ui_Screen1) {
        ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
    } else if (scr == ui_Screen2) {
        ui_extra_screen_change_no_wait(&ui_Screen2, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen2_screen_init);
    } else if (scr == ui_Screen3) {
        ui_extra_screen_change_no_wait(&ui_Screen3, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen3_screen_init);
    } else if (scr == ui_Screen4) {
        ui_extra_screen_change_no_wait(&ui_Screen4, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen4_screen_init);
    } else if (scr == ui_Screen5) {
        ui_extra_screen_change_no_wait(&ui_Screen5, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen5_screen_init);
    } else if (scr == ui_Screen6) {
        ui_extra_screen_change_no_wait(&ui_Screen6, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen6_screen_init);
    } else if (scr == ui_Screen7) {
        ui_extra_screen_change_no_wait(&ui_Screen7, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen7_screen_init);
    } else if (scr == ui_Screen8) {
        ui_extra_screen_change_no_wait(&ui_Screen8, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen8_screen_init);
    } else if (scr == ui_Screen9) {
        ui_extra_screen_change_no_wait(&ui_Screen9, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen9_screen_init);
    } else if (scr == ui_Screen10) {
        ui_extra_screen_change_no_wait(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen10_screen_init);
    } else if (scr == ui_Screen11) {
        ui_extra_screen_change_no_wait(&ui_Screen11, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen11_screen_init);
    } else if (scr == ui_Screen12) {
        ui_extra_screen_change_no_wait(&ui_Screen12, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen12_screen_init);
    } else if (scr == ui_Screen13) {
        ui_extra_screen_change_no_wait(&ui_Screen13, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen13_screen_init);
    } else if (scr == ui_Screen14) {
        ui_extra_screen_change_no_wait(&ui_Screen14, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen14_screen_init);
    } else if (scr == ui_Screen15) {
        ui_extra_screen_change_no_wait(&ui_Screen15, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen15_screen_init);
    } else if (scr == ui_Screen16) {
        ui_extra_screen_change_no_wait(&ui_Screen16, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen16_screen_init);
    } else {
        lv_scr_load(scr);
    }
}

static const char *ui_extra_scr_log_name(const lv_obj_t *scr)
{
    if (scr == NULL) {
        return "?";
    }
    if (scr == (const lv_obj_t *)ui_Screen1) {
        return "S1";
    }
    if (scr == (const lv_obj_t *)ui_Screen2) {
        return "S2";
    }
    if (scr == (const lv_obj_t *)ui_Screen3) {
        return "S3";
    }
    if (scr == (const lv_obj_t *)ui_Screen4) {
        return "S4";
    }
    if (scr == (const lv_obj_t *)ui_Screen5) {
        return "S5";
    }
    if (scr == (const lv_obj_t *)ui_Screen6) {
        return "S6";
    }
    if (scr == (const lv_obj_t *)ui_Screen9) {
        return "S9";
    }
    if (scr == (const lv_obj_t *)ui_Screen10) {
        return "S10";
    }
    return "Sx";
}

/* Forward declaration */
static void ui_extra_screen_change_no_wait(lv_obj_t **target,
                                           lv_scr_load_anim_t fademode,
                                           int spd,
                                           int delay,
                                           void (*target_init)(void));

static bool ui_extra_is_home_pager_pair(lv_obj_t *from, lv_obj_t *to)
{
    return (from == ui_Screen1 && to == ui_Screen2) || (from == ui_Screen2 && to == ui_Screen1);
}

static bool ui_extra_is_hub_screen(const lv_obj_t *scr)
{
    return scr == ui_Screen6 || scr == ui_Screen7 || scr == ui_Screen8 || scr == ui_Screen9 ||
           scr == ui_Screen10 || scr == ui_Screen11 || scr == ui_Screen12 || scr == ui_Screen13 ||
           scr == ui_Screen14 || scr == ui_Screen15 || scr == ui_Screen16;
}

static bool ui_extra_is_screen345(const lv_obj_t *scr)
{
    return scr == ui_Screen3 || scr == ui_Screen4 || scr == ui_Screen5;
}

/** S6 设置 hub 内切屏，或 S6↔S7–S16 子页。 */
static bool ui_extra_is_hub_nav_pair(lv_obj_t *from, lv_obj_t *to)
{
    if (from == NULL || to == NULL || from == to) {
        return false;
    }
    return ui_extra_is_hub_screen(from) && ui_extra_is_hub_screen(to);
}

/** 不做 lv_refr_now 的切屏路径（S1↔S2、S3↔S4↔S5、S6 hub、S6↔345/S1、hub→S1 等）。 */
static bool ui_extra_use_lightweight_switch(lv_obj_t *from, lv_obj_t *to)
{
    if (ui_extra_is_home_pager_pair(from, to)) {
        return true;
    }
    if (ui_extra_is_hub_nav_pair(from, to)) {
        return true;
    }
    if (ui_extra_is_screen345(from) && ui_extra_is_screen345(to) && from != to) {
        return true;
    }
    if (from == ui_Screen6 && (ui_extra_is_screen345(to) || to == ui_Screen1 || to == ui_Screen2)) {
        return true;
    }
    if (to == ui_Screen6 &&
        (ui_extra_is_screen345(from) || from == ui_Screen1 || from == ui_Screen2)) {
        return true;
    }
    /* Theme 选题回家 / hub 子页回首页：与 S6→S1 同轻量，避免同步 lv_refr_now。 */
    if (to == ui_Screen1 && ui_extra_is_hub_screen(from)) {
        return true;
    }
    return false;
}

static void ui_extra_gesture_switch_to_screen6(void)
{
    ui_extra_screen_change_no_wait(&ui_Screen6, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen6_screen_init);
}

/** S1↔S2 横向分页：手势识别后立即切屏，不等松手、不 defer、不遮罩。 */
static void ui_extra_gesture_switch_home_pager(lv_obj_t **target, void (*target_init)(void))
{
    if (target == NULL || target_init == NULL || *target == NULL) {
        return;
    }
    if (lv_scr_act() == *target) {
        return;
    }
    ui_extra_screen_change_no_wait(target, LV_SCR_LOAD_ANIM_NONE, 0, 0, target_init);
}

/** S3↔S4↔S5 横向：与 S1↔S2 同路径，走轻量刷新 + 0ms 解锁。 */
static void ui_extra_gesture_switch_345_pager(lv_obj_t **target, void (*target_init)(void))
{
    ui_extra_gesture_switch_home_pager(target, target_init);
}

/* Gesture deferral helper was removed: unused in current navigation logic. */

static bool ui_runtime_disp_uses_full_frame_buffer(lv_disp_t *disp)
{
    if (disp == NULL || disp->driver == NULL || disp->driver->draw_buf == NULL) {
        return false;
    }
    const uint32_t full = (uint32_t)disp->driver->hor_res * (uint32_t)disp->driver->ver_res;
    return disp->driver->draw_buf->size >= full;
}

static void ui_runtime_rgb_invalidate_scr(lv_obj_t *scr)
{
    if (scr != NULL) {
        lv_obj_invalidate(scr);
    }
}

/**
 * v1.2 风格：控件交互只 mark dirty，交给 LVGL refr 定时器 + direct_mode flush 上屏。
 * 禁止 lv_refr_now / full_refresh / present_frame，避免与 flush 双写导致抖动。
 */
void ui_runtime_rgb_commit_active_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    ui_runtime_rgb_invalidate_scr(lv_scr_act());
    if (disp != NULL) {
        lv_disp_trig_activity(disp);
    }
}

/** 切屏后同步刷新一次（SCREEN_LOADED）；不做 full_refresh 切换。 */
void ui_runtime_rgb_commit_full_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL) {
        return;
    }
    ui_runtime_rgb_invalidate_scr(lv_scr_act());
    lv_refr_now(disp);
}

/* 切屏后恢复 full_refresh 原始值。 */
static void ui_extra_restore_full_refresh_cb(lv_timer_t *t)
{
    (void)t;
    lv_disp_t *disp = lv_disp_get_default();
    /* RGB full-frame path must keep full_refresh=1; toggling breaks panel FB updates. */
    if (disp && disp->driver && !ui_runtime_disp_uses_full_frame_buffer(disp)) {
        disp->driver->full_refresh = s_ui_full_refresh_prev;
    }
    s_ui_full_refresh_restore_timer = NULL;
    s_ui_full_refresh_active = false;
    ESP_LOGD(TAG_UI_SW, "full_refresh restored");
}

/* 切屏互斥锁解锁回调。 */
static void ui_extra_unlock_switch_cb(lv_timer_t *t)
{
    (void)t;
    s_ui_switch_lock = false;
    s_ui_unlock_timer = NULL;
    ESP_LOGD(TAG_UI_SW, "unlock seq=%lu act=%s q=%d", (unsigned long)s_ui_switch_seq,
             ui_extra_scr_log_name(lv_scr_act()), s_ui_switch_queued_valid ? 1 : 0);

    if (s_ui_switch_queued_valid && s_ui_switch_queued_target != NULL && s_ui_switch_queued_init != NULL) {
        lv_obj_t **qt = s_ui_switch_queued_target;
        lv_scr_load_anim_t qf = s_ui_switch_queued_fademode;
        int qs = s_ui_switch_queued_spd;
        int qd = s_ui_switch_queued_delay;
        void (*qi)(void) = s_ui_switch_queued_init;
        s_ui_switch_queued_valid = false;
        s_ui_switch_queued_target = NULL;
        s_ui_switch_queued_init = NULL;
        ESP_LOGD(TAG_UI_SW, "flush queued -> %s", ui_extra_scr_log_name(*qt));
        ui_extra_screen_change_no_wait(qt, qf, qs, qd, qi);
    }
}

#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
static void ui_runtime_boot_prewarm_images_light(void)
{
    const void *theme = ui_runtime_theme_src_for_pick((uint8_t)s_screen10_selected_image);
    const void *small_srcs[] = {
        UI_SRC_HOME,
        UI_SRC_WIFI,
        UI_SRC_WIFICLOSE,
        UI_SRC_SOUNDOPEN,
        UI_SRC_SOUNDCLOSE,
        theme
    };

    for (unsigned i = 0; i < sizeof(small_srcs) / sizeof(small_srcs[0]); i++) {
        (void)_lv_img_cache_open(small_srcs[i], lv_color_white(), 0);
    }
}
#endif

/* 启动后预热常用 PNG 解码缓存，避免首次进入设置页卡顿。 */
static void ui_runtime_boot_prewarm_cb(lv_timer_t *t)
{
    (void)t;
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    ui_runtime_boot_prewarm_images_light();
#endif
    if (s_ui_boot_prewarm_timer) {
        lv_timer_del(s_ui_boot_prewarm_timer);
        s_ui_boot_prewarm_timer = NULL;
    }
}

static void ui_runtime_boot_prewarm_start(void)
{
    if (s_ui_boot_prewarm_timer == NULL) {
        s_ui_boot_prewarm_timer = lv_timer_create(ui_runtime_boot_prewarm_cb, 200, NULL);
        if (s_ui_boot_prewarm_timer) {
            lv_timer_set_repeat_count(s_ui_boot_prewarm_timer, 1);
        }
    }
}

/* Screen3/4/5 电源与温度事件逻辑已拆分到 ui_runtime_screen345.c。 */

/* 填充云端同步快照。 */
void ui_runtime_fill_cloud_snapshot(ui_runtime_cloud_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    out->i1 = (ui_Switch4 && ui_runtime_switch_is_on(ui_Switch4)) ? 1 : 0;
    out->i2 = (ui_Switch3 && ui_runtime_switch_is_on(ui_Switch3)) ? 1 : 0;
    out->i3 = (ui_Switch1 && ui_runtime_switch_is_on(ui_Switch1)) ? 1 : 0;
    out->t3 = (int16_t)s_screen3_temp;
    out->t5 = (int16_t)s_screen5_temp;
    out->e3 = s_power_sw4 ? 1 : 0;
    out->e5 = s_power_sw1 ? 1 : 0;
    out->m = bt_management_mesh_prov_is_enabled() ? 1 : 0;
}

/* 判断指定 MQTT 温度条目当前是否允许上报。 */
bool ui_runtime_allow_mqtt_temp_item(uint8_t item_id)
{
    if (item_id == 4 || item_id == 5) {
        return s_power_sw4;
    }
    if (item_id == 6 || item_id == 7) {
        return s_power_sw1;
    }
    return false;
}

#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
#include "src/draw/lv_img_cache.h"

static const void *ui_runtime_theme_src_for_pick(uint8_t pick)
{
    switch (pick) {
        case 1:
            return UI_SRC_SC1;
        case 2:
            return UI_SRC_SC2;
        case 3:
            return UI_SRC_SC3;
        case 4:
            return UI_SRC_SC4;
        default:
            return UI_SRC_SC1;
    }
}

/* 若缩略图解码失败仍写入 NVS 并回首页，Image1 会画 "No data"。先强制能打开再提交。 */
static bool ui_runtime_spiffs_theme_png_openable(const void *src)
{
    _lv_img_cache_entry_t *e = _lv_img_cache_open(src, lv_color_white(), 0);
    if (e != NULL) {
        return true;
    }
    lv_img_cache_invalidate_src(NULL);
    e = _lv_img_cache_open(src, lv_color_white(), 0);
    return e != NULL;
}
#endif

void ui_runtime_screen10_pick_and_home(uint8_t pick)
{
    if (pick < 1 || pick > 4) {
        return;
    }

    const bool same_pick = s_screen10_selection_loaded_from_nvs &&
                           (s_screen10_selected_image == (int)pick);

#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    if (!same_pick) {
        const void *want = ui_runtime_theme_src_for_pick(pick);
        if (!ui_runtime_spiffs_theme_png_openable(want)) {
            ESP_LOGW("ui_runtime", "theme PNG decode failed, skip NVS/home (pick=%u)", (unsigned)pick);
            return;
        }
    }
#endif

    if (!same_pick) {
        s_screen10_selected_image = (int)pick;
        s_screen10_selection_loaded_from_nvs = true;
        s_screen10_selection_needs_repair = false;
        ui_runtime_apply_screen10_image_selection();
        ui_runtime_storage_save_settings();
    }

    /* LOADED 上勿再 apply；走 no_wait 以统一锁/轻量 refr。 */
    s_skip_next_s1_theme_apply = true;
    ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
}

#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
/*
 * Screen10 四个格子容器默认主题下常为白底；PNG 解码未完成或缓存抖动时像「白块无内容」。
 * 做成与整页一致的深色底，避免误判为 bug。
 */
static void ui_runtime_screen10_tile_containers_dark(void)
{
    lv_obj_t *tiles[] = { ui_Container97, ui_Container99, ui_Container100, ui_Container101 };
    for (unsigned i = 0; i < sizeof(tiles) / sizeof(tiles[0]); i++) {
        if (tiles[i]) {
            lv_obj_set_style_bg_color(tiles[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(tiles[i], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

/*
 * Screen10 用 sc*_t 缩略图（~188px）。仅驱逐全分辨率主题槽，保留小图标与已解码 thumbs。
 */
static void ui_runtime_screen10_bind_thumbs_after_home_evict(void)
{
    static const struct {
        lv_obj_t **img;
        const void *src;
    } tiles[] = {
        { &ui_Image27, UI_SRC_SC1_T },
        { &ui_Image35, UI_SRC_SC2_T },
        { &ui_Image36, UI_SRC_SC3_T },
        { &ui_Image37, UI_SRC_SC4_T },
    };

    bool any_changed = false;
    for (unsigned i = 0; i < sizeof(tiles) / sizeof(tiles[0]); i++) {
        lv_obj_t *img = *tiles[i].img;
        if (img == NULL) {
            continue;
        }
        const void *cur = lv_img_get_src(img);
        if (cur != NULL && strcmp((const char *)cur, (const char *)tiles[i].src) == 0) {
            continue;
        }
        lv_img_set_src(img, tiles[i].src);
        lv_img_set_zoom(img, 256);
        lv_obj_invalidate(img);
        any_changed = true;
    }
    if (any_changed && ui_Container96) {
        lv_obj_update_layout(ui_Container96);
    }
}
#endif

static void ui_runtime_screen10_on_loaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    /* 只腾出全分辨率主题图，勿清掉 thumbs / 小图标（否则 S1↔S2 轮回变慢）。 */
    lv_img_cache_invalidate_src(UI_SRC_SC1);
    lv_img_cache_invalidate_src(UI_SRC_SC2);
    lv_img_cache_invalidate_src(UI_SRC_SC3);
    lv_img_cache_invalidate_src(UI_SRC_SC4);
    ui_runtime_screen10_bind_thumbs_after_home_evict();
#endif
}

static void ui_runtime_apply_screen10_image_selection(void)
{
    if (ui_Image1 == NULL) return;

    if (!s_screen10_selection_loaded_from_nvs) {
        const void *src = lv_img_get_src(ui_Image1);
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
        if (src) {
            if (strcmp((const char *)src, UI_SRC_SC2) == 0) s_screen10_selected_image = 2;
            else if (strcmp((const char *)src, UI_SRC_SC3) == 0) s_screen10_selected_image = 3;
            else if (strcmp((const char *)src, UI_SRC_SC4) == 0) s_screen10_selected_image = 4;
            else s_screen10_selected_image = 1; /* 默认保持 Screen1 生成图（sc1） */
        } else {
            s_screen10_selected_image = 1;
        }
#else
        if (src == &ui_img_sc2_png) s_screen10_selected_image = 2;
        else if (src == &ui_img_sc3_png) s_screen10_selected_image = 3;
        else if (src == &ui_img_sc4_png) s_screen10_selected_image = 4;
        else s_screen10_selected_image = 1; /* 默认保持 Screen1 生成图（sc1） */
#endif

        if (s_screen10_selection_needs_repair) {
            s_screen10_selection_needs_repair = false;
            ui_runtime_storage_save_settings();
        }
        return;
    }

    if (s_screen10_selected_image < 1 || s_screen10_selected_image > 4) s_screen10_selected_image = 1;

    const void *want;
    if (s_screen10_selected_image == 1) {
        want = UI_SRC_SC1;
    } else if (s_screen10_selected_image == 2) {
        want = UI_SRC_SC2;
    } else if (s_screen10_selected_image == 3) {
        want = UI_SRC_SC3;
    } else {
        want = UI_SRC_SC4;
    }

    const void *cur = lv_img_get_src(ui_Image1);
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    if (cur != NULL && strcmp((const char *)cur, (const char *)want) == 0) {
        return;
    }
#else
    if (cur == want) {
        return;
    }
#endif
    lv_img_set_src(ui_Image1, want);
}

/* Screen1 按钮4导航到 Screen3。 */
static void ui_extra_event_nav_Button4(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_scr_act() != ui_Screen1) {
        ESP_LOGW(TAG_UI_SW, "Btn4 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen3, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen3_screen_init);
}

/* Screen1 按钮3导航到 Screen4。 */
static void ui_extra_event_nav_Button3(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_scr_act() != ui_Screen1) {
        ESP_LOGW(TAG_UI_SW, "Btn3 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen4, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen4_screen_init);
}

/* Screen1 按钮1导航到 Screen5。 */
static void ui_extra_event_nav_Button1(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_scr_act() != ui_Screen1) {
        ESP_LOGW(TAG_UI_SW, "Btn1 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen5, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen5_screen_init);
}

/* Screen3 返回按钮导航到 Screen1。 */
static void ui_extra_event_nav_Button14(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_scr_act() != ui_Screen3) {
        ESP_LOGW(TAG_UI_SW, "Btn14 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
}

/* Screen4 返回按钮导航到 Screen1。 */
static void ui_extra_event_nav_Button36(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_scr_act() != ui_Screen4) {
        ESP_LOGW(TAG_UI_SW, "Btn36 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
}

/* Screen5 返回按钮导航到 Screen1。 */
static void ui_extra_event_nav_Button23(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_scr_act() != ui_Screen5) {
        ESP_LOGW(TAG_UI_SW, "Btn23 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
}

/* Screen6 返回按钮导航到 Screen1。 */
static void ui_extra_event_nav_Button18(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (lv_scr_act() != ui_Screen6) {
        ESP_LOGW(TAG_UI_SW, "Btn18 nav ignored act=%s", ui_extra_scr_log_name(lv_scr_act()));
        return;
    }
    ui_extra_screen_change_no_wait(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, ui_Screen1_screen_init);
}

/* 立即切屏：可选短时 full_refresh，含切屏锁与恢复逻辑。 */
static void ui_extra_screen_change_no_wait(lv_obj_t **target,
                                           lv_scr_load_anim_t fademode,
                                           int spd,
                                           int delay,
                                           void (*target_init)(void))
{
    if (s_ui_switch_lock) {
        if (target != NULL && target_init != NULL && *target != NULL) {
            if (s_ui_switch_queued_valid) {
                ESP_LOGD(TAG_UI_SW, "busy: queue overwrite -> %s", ui_extra_scr_log_name(*target));
            } else {
                ESP_LOGW(TAG_UI_SW, "busy: queue -> %s (act=%s)", ui_extra_scr_log_name(*target),
                         ui_extra_scr_log_name(lv_scr_act()));
            }
            s_ui_switch_queued_valid = true;
            s_ui_switch_queued_target = target;
            s_ui_switch_queued_fademode = fademode;
            s_ui_switch_queued_spd = spd;
            s_ui_switch_queued_delay = delay;
            s_ui_switch_queued_init = target_init;
        } else {
            ESP_LOGW(TAG_UI_SW, "busy: drop invalid switch request");
        }
        return;
    }

    lv_disp_t *disp = lv_disp_get_default();
    lv_obj_t *old_scr = lv_scr_act();
    if (ui_extra_is_screen345(old_scr)) {
        ui_runtime_screen345_flush_pending_temp_save();
    }

    s_ui_switch_lock = true;
    const unsigned long this_seq = ++s_ui_switch_seq;

    s_ui_switch_from_scr = old_scr;
    lv_obj_t *dest_scr = (target != NULL) ? *target : NULL;
    bool full_refresh_enabled_for_anim = false;
    const bool rgb_full_fb = ui_runtime_disp_uses_full_frame_buffer(disp);
    const bool lightweight = ui_extra_use_lightweight_switch(old_scr, dest_scr);
    const bool old345 = (old_scr == ui_Screen3 || old_scr == ui_Screen4 || old_scr == ui_Screen5);
    const bool dest345 = (dest_scr == ui_Screen3 || dest_scr == ui_Screen4 || dest_scr == ui_Screen5);
    const bool among345 = old345 && dest345 && old_scr != dest_scr;
    const bool home_to_345 = (old_scr == ui_Screen1 && dest345);
    const bool from345_home = (old345 && dest_scr == ui_Screen1);

    if (disp && disp->driver && !rgb_full_fb && !lightweight) {
        bool switching_to_screen1 = (dest_scr == ui_Screen1);
        /* S3↔S4↔S5 横向切换：禁止 full_refresh。整缓冲下反复 toggling full_refresh + restore
         * 定时器易与 RGB 驱动不同步（日志 seq 正常仍花屏）；仅用下方双屏 invalidate。 */
        const bool want_full_refresh = switching_to_screen1 || home_to_345 || from345_home;

        if (want_full_refresh) {
            /* Only force full_refresh when the draw buffer can cover the whole screen.
             * Otherwise forcing full_refresh may lead to black screen / artifacts in partial-buffer mode.
             */
            if (disp->driver->draw_buf) {
                uint32_t buf_size = (uint32_t)disp->driver->draw_buf->size;
                uint32_t full_size = (uint32_t)disp->driver->hor_res * (uint32_t)disp->driver->ver_res;
                if (buf_size >= full_size) {
                    full_refresh_enabled_for_anim = true;
                    if(!s_ui_full_refresh_active) {
                        s_ui_full_refresh_prev = (uint8_t)disp->driver->full_refresh;
                        s_ui_full_refresh_active = true;
                    }
                    disp->driver->full_refresh = 1;
                }
            }
        }
    }

    ESP_LOGD(TAG_UI_SW, "begin seq=%lu %s -> %s full=%d", this_seq,
             ui_extra_scr_log_name(old_scr), ui_extra_scr_log_name(dest_scr), full_refresh_enabled_for_anim ? 1 : 0);

    _ui_screen_load_only(target, fademode, spd, delay, target_init);

    /* bind_apply + RGB refresh run from ui_runtime_on_screen_loaded (SCREEN_LOADED). */

    if (full_refresh_enabled_for_anim || rgb_full_fb) {
        if (!lightweight && old_scr) {
            lv_obj_invalidate(old_scr);
        }
        lv_obj_invalidate(lv_scr_act());
    } else {
        /* 部分缓冲无法开 full_refresh 时，仍尽量标记整屏脏区 */
        lv_obj_t *new_scr = lv_scr_act();
        if (old_scr && new_scr && old_scr != new_scr) {
            const bool o345 = (old_scr == ui_Screen3 || old_scr == ui_Screen4 || old_scr == ui_Screen5);
            const bool n345 = (new_scr == ui_Screen3 || new_scr == ui_Screen4 || new_scr == ui_Screen5);
            const bool cross_home_345 = (old_scr == ui_Screen1 && n345) || (o345 && new_scr == ui_Screen1);
            if ((o345 && n345) || cross_home_345) {
                lv_obj_invalidate(old_scr);
                lv_obj_invalidate(new_scr);
            }
        }
    }

    if (disp != NULL) {
        lv_disp_trig_activity(disp);
    }

    /* Keep full_refresh enabled slightly longer to cover animation frames.
     * Too short restore window can leave partial artifacts.
     */
    uint32_t restore_ms = (uint32_t)((spd > 0 ? spd : 0) + (delay > 0 ? delay : 0) + 200);
    if(full_refresh_enabled_for_anim) {
        if(s_ui_full_refresh_restore_timer == NULL) {
            s_ui_full_refresh_restore_timer = lv_timer_create(ui_extra_restore_full_refresh_cb, restore_ms, NULL);
            if(s_ui_full_refresh_restore_timer) {
                lv_timer_set_repeat_count(s_ui_full_refresh_restore_timer, 1);
            } else {
                ui_extra_restore_full_refresh_cb(NULL);
            }
        } else {
            lv_timer_set_period(s_ui_full_refresh_restore_timer, restore_ms);
            lv_timer_set_repeat_count(s_ui_full_refresh_restore_timer, 1);
            lv_timer_reset(s_ui_full_refresh_restore_timer);
        }
    }

    /* Unlock shortly after load (v1.2 tab switch had no lock). */
    uint32_t unlock_ms = (spd > 0 ? (uint32_t)spd : 0U) + UI_SWITCH_UNLOCK_MS_DEFAULT;
    if (lightweight) {
        unlock_ms = (spd > 0 ? (uint32_t)spd : 0U) + UI_SWITCH_UNLOCK_MS_HOME_PAGER;
    } else if (old345 || dest345 || among345 || home_to_345 || from345_home) {
        unlock_ms = (spd > 0 ? (uint32_t)spd : 0U) + UI_SWITCH_UNLOCK_MS_345;
    }
    ESP_LOGD(TAG_UI_SW, "unlock_timer=%lums", (unsigned long)unlock_ms);

    if(s_ui_unlock_timer == NULL) {
        s_ui_unlock_timer = lv_timer_create(ui_extra_unlock_switch_cb, unlock_ms, NULL);
        if(s_ui_unlock_timer == NULL) {
            ESP_LOGW(TAG_UI_SW, "unlock timer alloc failed; unlock now");
            s_ui_switch_lock = false;
            if (s_ui_switch_queued_valid && s_ui_switch_queued_target != NULL && s_ui_switch_queued_init != NULL) {
                lv_obj_t **qt = s_ui_switch_queued_target;
                lv_scr_load_anim_t qf = s_ui_switch_queued_fademode;
                int qs = s_ui_switch_queued_spd;
                int qd = s_ui_switch_queued_delay;
                void (*qi)(void) = s_ui_switch_queued_init;
                s_ui_switch_queued_valid = false;
                s_ui_switch_queued_target = NULL;
                s_ui_switch_queued_init = NULL;
                ui_extra_screen_change_no_wait(qt, qf, qs, qd, qi);
            }
        } else {
            lv_timer_set_repeat_count(s_ui_unlock_timer, 1);
        }
    } else {
        lv_timer_set_period(s_ui_unlock_timer, unlock_ms);
        lv_timer_set_repeat_count(s_ui_unlock_timer, 1);
        lv_timer_reset(s_ui_unlock_timer);
    }

    ESP_LOGD(TAG_UI_SW, "done seq=%lu act=%s", this_seq, ui_extra_scr_log_name(lv_scr_act()));
}

/* Screen1 手势导航处理。 */
static void ui_extra_gesture_screen1(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if(dir == LV_DIR_BOTTOM) {
        ui_extra_gesture_switch_to_screen6();
        /* 先切屏再吞残留触摸，避免等抬手拖慢进设置。 */
        lv_indev_wait_release(indev);
    } else if(dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        ui_extra_gesture_switch_home_pager(&ui_Screen2, ui_Screen2_screen_init);
    }
}

/* Screen2 手势导航处理。 */
static void ui_extra_gesture_screen2(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if(dir == LV_DIR_BOTTOM) {
        ui_extra_gesture_switch_to_screen6();
        lv_indev_wait_release(indev);
    } else if(dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        ui_extra_gesture_switch_home_pager(&ui_Screen1, ui_Screen1_screen_init);
    }
}

/* 手势切屏前吞掉当前按下：避免松手落成 CLICKED（stale Btn±）并带动整页重绘撕裂。 */
static void ui_extra_gesture_cancel_press(lv_indev_t *indev)
{
    if (indev == NULL) {
        return;
    }
    lv_obj_t *obj = lv_indev_get_obj_act();
    if (obj != NULL) {
        lv_obj_clear_state(obj, LV_STATE_PRESSED | LV_STATE_FOCUSED);
    }
    lv_indev_wait_release(indev);
}

/* Screen3 手势导航处理（横向顺序 3→4→5：LEFT 下一页，RIGHT 上一页/回环）。 */
static void ui_extra_gesture_screen3(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if(dir == LV_DIR_BOTTOM) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_to_screen6();
    } else if(dir == LV_DIR_LEFT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen4, ui_Screen4_screen_init);
    } else if(dir == LV_DIR_RIGHT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen5, ui_Screen5_screen_init);
    }
}

/* Screen4 手势导航处理。 */
static void ui_extra_gesture_screen4(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if(dir == LV_DIR_BOTTOM) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_to_screen6();
    } else if(dir == LV_DIR_LEFT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen5, ui_Screen5_screen_init);
    } else if(dir == LV_DIR_RIGHT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen3, ui_Screen3_screen_init);
    }
}

/* Screen5 手势导航处理。 */
static void ui_extra_gesture_screen5(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    lv_indev_t *indev = lv_indev_get_act();
    if(indev == NULL) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if(dir == LV_DIR_BOTTOM) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_to_screen6();
    } else if(dir == LV_DIR_LEFT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen3, ui_Screen3_screen_init);
    } else if(dir == LV_DIR_RIGHT) {
        ui_extra_gesture_cancel_press(indev);
        ui_extra_gesture_switch_345_pager(&ui_Screen4, ui_Screen4_screen_init);
    }
}

static void ui_extra_screen10_theme_pick_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_scr_act() != ui_Screen10) {
        return;
    }
    const uint8_t pick = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ui_runtime_screen10_pick_and_home(pick);
}

/* S6 hub + S7–S13：统一 CLICKED 轻量切屏，剥离 SquareLine 的 PRESSED/LV_EVENT_ALL 导航。 */
typedef struct {
    lv_obj_t *btn;
    lv_event_cb_t old_cb;
    lv_obj_t **dest;
    void (*init)(void);
    lv_obj_t *expect_scr;
} ui_extra_hub_nav_t;

static void ui_extra_hub_nav_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const ui_extra_hub_nav_t *n = lv_event_get_user_data(e);
    if (n == NULL || n->dest == NULL || n->init == NULL) {
        return;
    }
    if (n->expect_scr != NULL && lv_scr_act() != n->expect_scr) {
        return;
    }
    if (*n->dest != NULL && lv_scr_act() == *n->dest) {
        return;
    }
    ui_extra_screen_change_no_wait(n->dest, LV_SCR_LOAD_ANIM_NONE, 0, 0, n->init);
}

static void ui_extra_rewire_hub_nav(ui_extra_hub_nav_t *n)
{
    if (n == NULL || n->btn == NULL || n->dest == NULL || n->init == NULL) {
        return;
    }
    if (n->old_cb != NULL) {
        lv_obj_remove_event_cb(n->btn, n->old_cb);
    }
    lv_obj_remove_event_cb(n->btn, ui_extra_hub_nav_cb);
    lv_obj_add_event_cb(n->btn, ui_extra_hub_nav_cb, LV_EVENT_CLICKED, n);
}

static void ui_extra_rewire_screen10_theme_tile(lv_obj_t *obj, lv_event_cb_t old_cb, uint8_t pick)
{
    if (obj == NULL) {
        return;
    }
    if (old_cb != NULL) {
        lv_obj_remove_event_cb(obj, old_cb);
    }
    lv_obj_remove_event_cb(obj, ui_extra_screen10_theme_pick_cb);
    lv_obj_add_event_cb(obj, ui_extra_screen10_theme_pick_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)pick);
}

static void ui_extra_hub_nav_bind(ui_extra_hub_nav_t *n,
                                  lv_obj_t *btn,
                                  lv_event_cb_t old_cb,
                                  lv_obj_t **dest,
                                  void (*init)(void),
                                  lv_obj_t *expect_scr)
{
    if (n == NULL) {
        return;
    }
    n->btn = btn;
    n->old_cb = old_cb;
    n->dest = dest;
    n->init = init;
    n->expect_scr = expect_scr;
    ui_extra_rewire_hub_nav(n);
}


/* 运行时总入口：替换事件、绑定逻辑、恢复持久化、启动定时器。 */
void ui_runtime_apply(void)
{
    ui_runtime_ambient_load_settings();

    /* 1) 手势：LVGL 将 GESTURE 发到「自触点向上首个未设 GESTURE_BUBBLE 的祖先」——无父的 Screen 根无该标志，
     *    故事件落在 ui_Screen3 等根上，而非 ui_Container12。必须在 Screen 根挂 runtime 手势，并从容
     *    器上摘掉 SquareLine 里仍直连 _ui_screen_change 的旧回调，避免双路径花屏。 */
    if (ui_Container2) {
        lv_obj_remove_event_cb(ui_Container2, ui_event_Container2);
    }
    if (ui_Screen1) {
        lv_obj_remove_event_cb(ui_Screen1, ui_extra_gesture_screen1);
        lv_obj_add_event_cb(ui_Screen1, ui_extra_gesture_screen1, LV_EVENT_GESTURE, NULL);
        if (ui_Image1) {
            lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_GESTURE_BUBBLE);
        }
    }
    if (ui_Container4) {
        lv_obj_remove_event_cb(ui_Container4, ui_event_Container4);
    }
    if (ui_Screen2) {
        lv_obj_remove_event_cb(ui_Screen2, ui_extra_gesture_screen2);
        lv_obj_add_event_cb(ui_Screen2, ui_extra_gesture_screen2, LV_EVENT_GESTURE, NULL);
    }
    if (ui_Container12) {
        lv_obj_remove_event_cb(ui_Container12, ui_event_Container12);
    }
    if (ui_Screen3) {
        lv_obj_remove_event_cb(ui_Screen3, ui_extra_gesture_screen3);
        lv_obj_add_event_cb(ui_Screen3, ui_extra_gesture_screen3, LV_EVENT_GESTURE, NULL);
    }
    if (ui_Container1) {
        lv_obj_remove_event_cb(ui_Container1, ui_event_Container1);
    }
    if (ui_Screen4) {
        lv_obj_remove_event_cb(ui_Screen4, ui_extra_gesture_screen4);
        lv_obj_add_event_cb(ui_Screen4, ui_extra_gesture_screen4, LV_EVENT_GESTURE, NULL);
    }
    if (ui_Container26) {
        lv_obj_remove_event_cb(ui_Container26, ui_event_Container26);
    }
    if (ui_Screen5) {
        lv_obj_remove_event_cb(ui_Screen5, ui_extra_gesture_screen5);
        lv_obj_add_event_cb(ui_Screen5, ui_extra_gesture_screen5, LV_EVENT_GESTURE, NULL);
    }

    /* 2) Screen1 底部三卡：CLICKED 单次完整点击，避免 PRESSED 与切屏锁/松手叠触导致花屏或丢事件。 */
    lv_obj_remove_event_cb(ui_Button4, ui_event_Button4);
    lv_obj_add_event_cb(ui_Button4, ui_extra_event_nav_Button4, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button3, ui_event_Button3);
    lv_obj_add_event_cb(ui_Button3, ui_extra_event_nav_Button3, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button1, ui_event_Button1);
    lv_obj_add_event_cb(ui_Button1, ui_extra_event_nav_Button1, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button14, ui_event_Button14);
    lv_obj_add_event_cb(ui_Button14, ui_extra_event_nav_Button14, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button36, ui_event_Button36);
    lv_obj_add_event_cb(ui_Button36, ui_extra_event_nav_Button36, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button23, ui_event_Button23);
    lv_obj_add_event_cb(ui_Button23, ui_extra_event_nav_Button23, LV_EVENT_CLICKED, NULL);

    lv_obj_remove_event_cb(ui_Button18, ui_event_Button18);
    lv_obj_add_event_cb(ui_Button18, ui_extra_event_nav_Button18, LV_EVENT_CLICKED, NULL);

    /* S6 入口 / S7–S13 子页与返回：hub CLICKED 轻切屏（勿 PRESSED + LV_EVENT_ALL）。 */
    static ui_extra_hub_nav_t s_hub_s7;
    static ui_extra_hub_nav_t s_hub_s12;
    static ui_extra_hub_nav_t s_hub_s11;
    static ui_extra_hub_nav_t s_hub_s13;
    static ui_extra_hub_nav_t s_hub_s8;
    static ui_extra_hub_nav_t s_hub_s8_to_s9;
    static ui_extra_hub_nav_t s_hub_s9_to_s10;
    static ui_extra_hub_nav_t s_hub_s9_to_s14;
    static ui_extra_hub_nav_t s_hub_s9_to_s15;
    static ui_extra_hub_nav_t s_hub_s9_to_s16;
    static ui_extra_hub_nav_t s_back_s6_from_s7;
    static ui_extra_hub_nav_t s_back_s6_from_s8;
    static ui_extra_hub_nav_t s_back_s8_from_s9;
    static ui_extra_hub_nav_t s_back_s10_from_s9;
    static ui_extra_hub_nav_t s_back_s6_from_s12;
    static ui_extra_hub_nav_t s_back_s6_from_s13;
    static ui_extra_hub_nav_t s_back_s6_from_s11;
    ui_extra_hub_nav_bind(&s_hub_s7, ui_Button19, ui_event_Button19, &ui_Screen7, ui_Screen7_screen_init, ui_Screen6);
    ui_extra_hub_nav_bind(&s_hub_s12, ui_Button22, ui_event_Button22, &ui_Screen12, ui_Screen12_screen_init, ui_Screen6);
    ui_extra_hub_nav_bind(&s_hub_s11, ui_Button29, ui_event_Button29, &ui_Screen11, ui_Screen11_screen_init, ui_Screen6);
    ui_extra_hub_nav_bind(&s_hub_s13, ui_Button30, ui_event_Button30, &ui_Screen13, ui_Screen13_screen_init, ui_Screen6);
    ui_extra_hub_nav_bind(&s_hub_s8, ui_Button31, ui_event_Button31, &ui_Screen8, ui_Screen8_screen_init, ui_Screen6);
    ui_extra_hub_nav_bind(&s_hub_s8_to_s9, ui_Button41, ui_event_Button41, &ui_Screen9, ui_Screen9_screen_init, ui_Screen8);
    ui_extra_hub_nav_bind(&s_hub_s9_to_s10, ui_Button43, ui_event_Button43, &ui_Screen10, ui_Screen10_screen_init, ui_Screen9);
    ui_extra_hub_nav_bind(&s_hub_s9_to_s14, ui_Button44, NULL, &ui_Screen14, ui_Screen14_screen_init, ui_Screen9);
    ui_extra_hub_nav_bind(&s_hub_s9_to_s15, ui_Button45, NULL, &ui_Screen15, ui_Screen15_screen_init, ui_Screen9);
    ui_extra_hub_nav_bind(&s_hub_s9_to_s16, ui_Button46, NULL, &ui_Screen16, ui_Screen16_screen_init, ui_Screen9);
    ui_extra_hub_nav_bind(&s_back_s6_from_s7, ui_Button6, ui_event_Button6, &ui_Screen6, ui_Screen6_screen_init, ui_Screen7);
    ui_extra_hub_nav_bind(&s_back_s6_from_s8, ui_Button34, ui_event_Button34, &ui_Screen6, ui_Screen6_screen_init, ui_Screen8);
    ui_extra_hub_nav_bind(&s_back_s8_from_s9, ui_Button35, ui_event_Button35, &ui_Screen8, ui_Screen8_screen_init, ui_Screen9);
    ui_extra_hub_nav_bind(&s_back_s10_from_s9, ui_Button37, ui_event_Button37, &ui_Screen9, ui_Screen9_screen_init, ui_Screen10);
    ui_extra_hub_nav_bind(&s_back_s6_from_s11, ui_Button47, ui_event_Button47, &ui_Screen6, ui_Screen6_screen_init, ui_Screen11);
    ui_extra_hub_nav_bind(&s_back_s6_from_s12, ui_Button50, ui_event_Button50, &ui_Screen6, ui_Screen6_screen_init, ui_Screen12);
    ui_extra_hub_nav_bind(&s_back_s6_from_s13, ui_Button51, ui_event_Button51, &ui_Screen6, ui_Screen6_screen_init, ui_Screen13);

    ui_extra_rewire_screen10_theme_tile(ui_Container97, ui_event_Container97, 1);
    ui_extra_rewire_screen10_theme_tile(ui_Container99, ui_event_Container99, 2);
    ui_extra_rewire_screen10_theme_tile(ui_Container100, ui_event_Container100, 3);
    ui_extra_rewire_screen10_theme_tile(ui_Container101, ui_event_Container101, 4);

    /* 2.1) Make Screen7 runtime logic independent from generated ui files */
    if (ui_Screen7) {
        lv_obj_add_event_cb(ui_Screen7, ui_extra_screen7_wifi_scan_cb, LV_EVENT_SCREEN_LOADED, NULL);
    }

    /* Every screen load: restore persisted UI state + commit RGB frame buffer. */
    ui_runtime_register_screen_loaded(ui_Screen1);
    ui_runtime_register_screen_loaded(ui_Screen2);
    ui_runtime_register_screen_loaded(ui_Screen3);
    ui_runtime_register_screen_loaded(ui_Screen4);
    ui_runtime_register_screen_loaded(ui_Screen5);
    ui_runtime_register_screen_loaded(ui_Screen6);
    ui_runtime_register_screen_loaded(ui_Screen7);
    ui_runtime_register_screen_loaded(ui_Screen8);
    ui_runtime_register_screen_loaded(ui_Screen9);
    ui_runtime_register_screen_loaded(ui_Screen10);
    ui_runtime_register_screen_loaded(ui_Screen11);
    ui_runtime_register_screen_loaded(ui_Screen12);
    ui_runtime_register_screen_loaded(ui_Screen13);
    ui_runtime_register_screen_loaded(ui_Screen14);
    ui_runtime_register_screen_loaded(ui_Screen15);
    ui_runtime_register_screen_loaded(ui_Screen16);

    if (ui_Dropdown1) {
        /* 新版 ui/ 可能不为 Dropdown 生成 ui_event_Dropdown1，仅在此挂载运行时逻辑 */
        lv_obj_add_event_cb(ui_Dropdown1, ui_extra_event_dropdown1, LV_EVENT_ALL, NULL);
    }
    if (ui_TextArea1) {
        lv_obj_remove_event_cb(ui_TextArea1, ui_event_TextArea1);
        lv_obj_add_event_cb(ui_TextArea1, ui_extra_event_textarea1, LV_EVENT_ALL, NULL);
        lv_textarea_set_password_mode(ui_TextArea1, false);
    }
    if (ui_Keyboard2) {
        lv_obj_remove_event_cb(ui_Keyboard2, ui_event_Keyboard2);
        lv_obj_add_event_cb(ui_Keyboard2, ui_extra_event_keyboard2, LV_EVENT_ALL, NULL);
        if (ui_TextArea1) _ui_keyboard_set_target(ui_Keyboard2, ui_TextArea1);
    }

    /* 2.2) Screen1: time + date in one column, vertically centered block; larger fonts */
    if (ui_Container3 && ui_Label1 && ui_Label3) {
        lv_obj_set_parent(ui_Label3, ui_Container3);
        if (ui_Container5) {
            lv_obj_add_flag(ui_Container5, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_set_width(ui_Container3, 460);
        lv_obj_set_height(ui_Container3, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(ui_Container3, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_Container3,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(ui_Container3, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ui_Container3, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);
        lv_obj_set_pos(ui_Label1, 0, 0);

        lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);
        lv_obj_set_pos(ui_Label3, 0, 0);

        lv_obj_set_style_text_align(ui_Label1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_Label3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_font(ui_Label1, &lv_font_source_han_screen1_time_100, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(ui_Label1, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Screen1 底部三枚导航卡：略下移 + 统一圆角/描边；按压态由 tile_press_stable 设为天蓝且不变大。 */
    if (ui_Container35) {
        lv_obj_set_y(ui_Container35, 172);
    }
    {
        lv_obj_t *nav_btns[] = { ui_Button1, ui_Button3, ui_Button4 };
        for (unsigned i = 0; i < sizeof(nav_btns) / sizeof(nav_btns[0]); i++) {
            lv_obj_t *b = nav_btns[i];
            if (b == NULL) {
                continue;
            }
            lv_obj_set_style_radius(b, 22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x2F3844), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(b, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(b, lv_color_hex(0x6B7F99), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(b, LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_spread(b, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            /* 本地样式压过主题 grow，避免按下变大。 */
            lv_obj_set_style_transform_width(b, 0, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_transform_height(b, 0, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_transform_zoom(b, LV_IMG_ZOOM_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x87CEEB), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(b, LV_OPA_50, LV_PART_MAIN | LV_STATE_PRESSED);
        }
        if (ui_Button4) {
            lv_obj_set_scrollbar_mode(ui_Button4, LV_SCROLLBAR_MODE_OFF);
        }
    }

    /* 3) Reduce draw cost: disable shadows on hotspots. */
    ui_runtime_disable_shadow(ui_Button1);
    ui_runtime_disable_shadow(ui_Button3);
    ui_runtime_disable_shadow(ui_Button4);

    ui_runtime_disable_shadow(ui_Button2);
    ui_runtime_disable_shadow(ui_Button7);
    ui_runtime_disable_shadow(ui_Button8);
    ui_runtime_disable_shadow(ui_Button9);
    ui_runtime_disable_shadow(ui_Button10);
    ui_runtime_disable_shadow(ui_Button11);
    ui_runtime_disable_shadow(ui_Button12);
    ui_runtime_disable_shadow(ui_Button28);

    ui_runtime_disable_shadow(ui_Button5);
    ui_runtime_disable_shadow(ui_Button13);
    ui_runtime_disable_shadow(ui_Button14);
    ui_runtime_disable_shadow(ui_Button15);
    ui_runtime_disable_shadow(ui_Button16);
    ui_runtime_disable_shadow(ui_Button17);

    ui_runtime_disable_shadow(ui_Button20);
    ui_runtime_disable_shadow(ui_Button21);
    ui_runtime_disable_shadow(ui_Button36);

    ui_runtime_disable_shadow(ui_Button23);
    ui_runtime_disable_shadow(ui_Button24);
    ui_runtime_disable_shadow(ui_Button25);
    ui_runtime_disable_shadow(ui_Button26);
    ui_runtime_disable_shadow(ui_Button27);

    ui_runtime_disable_shadow(ui_Button18);
    ui_runtime_disable_shadow(ui_Button19);
    ui_runtime_disable_shadow(ui_Button22);
    ui_runtime_disable_shadow(ui_Button29);
    ui_runtime_disable_shadow(ui_Button30);
    ui_runtime_disable_shadow(ui_Button31);
    ui_runtime_disable_shadow(ui_Button32);
    ui_runtime_disable_shadow(ui_Button33);

    /* 3.2) WiFi SSID CJK font + layout (generated ui/ keeps Montserrat placeholders). */
    ui_extra_apply_wifi_ssid_runtime();
    /* Screen1 press-state suppression is disabled temporarily for testing. */

    /* 3.1) Screen6 sound toggle button */
    /* Button32 belongs to Screen6 and is created lazily.
     * Bind it when Screen6 is entered (see ui_runtime_ab_bind_apply_for_screen6). */
    ui_runtime_ab_apply_sound_state();
    ui_runtime_ab_bind_default_buttons();

    /* 4) Arc: disable manual drag/slide, only +/- buttons change the value. */
    if(ui_Arc2) {
        lv_obj_clear_flag(ui_Arc2, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_Arc2, LV_OBJ_FLAG_SCROLLABLE);
    }
    if(ui_Arc3) {
        lv_obj_clear_flag(ui_Arc3, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_Arc3, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* Screen6 starts with backlight_on() (100%) in app_main, sync slider knob.
     * Slider最底为0，但实际背光最低强制为20%。后续若NVS里有保存的亮度，会覆盖这个默认值。
     */
    if (ui_Slider1) {
        lv_slider_set_value(ui_Slider1, 100, LV_ANIM_OFF);
        ui_runtime_ab_mark_boot_default_100(); /* app_main already init+enable */
    }

    /* 4.1) Screen6 brightness slider -> LCD backlight PWM (bound lazily on Screen6 enter) */

    /* Load persisted temps & settings (defaults if NVS has no data yet). */
    s_screen3_temp = 16;
    s_screen5_temp = 16;
    ui_runtime_storage_load_temps();
    ui_runtime_storage_load_settings();
    ui_runtime_apply_screen10_image_selection();
    ui_extra_screen3_arc_apply();
    ui_extra_screen5_arc_apply();

    /* Apply overall power OFF/ON state based on Screen1 switches */
    if (ui_Switch4) ui_runtime_switch_set_on(ui_Switch4, s_power_sw4);
    if (ui_Switch1) ui_runtime_switch_set_on(ui_Switch1, s_power_sw1);
    if (ui_Switch3) ui_runtime_switch_set_on(ui_Switch3, s_power_sw3);

    if (ui_Switch4) {
        ui_extra_apply_screen3_power(ui_runtime_switch_is_on(ui_Switch4));
    }
    if (ui_Switch1) {
        ui_extra_apply_screen5_power(ui_runtime_switch_is_on(ui_Switch1));
    }
    if (ui_Switch3) {
        ui_extra_apply_screen4_power(ui_runtime_switch_is_on(ui_Switch3));
    }

    /* ui_init() builds all screens up front — bind runtime handlers once at boot. */
    ui_runtime_bind_all_screens();

    /* Apply/bind for the boot screen (Screen1). */
    ui_extra_bind_apply_for_active_screen();

    /* Hook +/- buttons. */
    /* +/- buttons are bound lazily when Screen3/5 are created */

    /* Hook overall power switches (Screen1 <-> Screen3/4/5 open/close buttons) */
    if (ui_Switch4) lv_obj_add_event_cb(ui_Switch4, ui_extra_event_switch4_device1, LV_EVENT_VALUE_CHANGED, NULL);
    if (ui_Switch1) lv_obj_add_event_cb(ui_Switch1, ui_extra_event_switch1_device2, LV_EVENT_VALUE_CHANGED, NULL);
    if (ui_Switch3) lv_obj_add_event_cb(ui_Switch3, ui_extra_event_switch3_device3, LV_EVENT_VALUE_CHANGED, NULL);

    /* v1.2 风格：首页卡片无按压缩放/白闪，避免点击时视觉抖动。 */
    if (ui_Button1) ui_runtime_screen1_tile_press_stable(ui_Button1);
    if (ui_Button3) ui_runtime_screen1_tile_press_stable(ui_Button3);
    if (ui_Button4) ui_runtime_screen1_tile_press_stable(ui_Button4);

    /* Screen2 情景卡：触摸/点击/长按背景天蓝。 */
    {
        lv_obj_t *s2_cards[] = {
            ui_Button2, ui_Button7, ui_Button8, ui_Button9,
            ui_Button10, ui_Button11, ui_Button12, ui_Button28,
        };
        for (unsigned i = 0; i < sizeof(s2_cards) / sizeof(s2_cards[0]); i++) {
            if (s2_cards[i]) {
                ui_runtime_screen2_card_press_sky(s2_cards[i]);
            }
        }
    }

    /* 全局取消按钮按下放大，避免破坏布局（切屏时在 SCREEN_LOADED 再刷一次）。 */
    ui_runtime_disable_btn_grow_everywhere();

    if (ui_Screen10) {
        lv_obj_add_event_cb(ui_Screen10, ui_runtime_screen10_on_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    }
    /* 命中的是 lv_img 子对象时，事件不会冒泡到 Container；去掉 img 可点击，让点击落在容器上 */
    if (ui_Image27) {
        lv_obj_clear_flag(ui_Image27, LV_OBJ_FLAG_CLICKABLE);
    }
    if (ui_Image35) {
        lv_obj_clear_flag(ui_Image35, LV_OBJ_FLAG_CLICKABLE);
    }
    if (ui_Image36) {
        lv_obj_clear_flag(ui_Image36, LV_OBJ_FLAG_CLICKABLE);
    }
    if (ui_Image37) {
        lv_obj_clear_flag(ui_Image37, LV_OBJ_FLAG_CLICKABLE);
    }
#if defined(CONFIG_APP_USE_SPIFFS_UI_ASSETS) && CONFIG_APP_USE_SPIFFS_UI_ASSETS
    ui_runtime_screen10_tile_containers_dark();
#endif

    /* Open/Close buttons are bound lazily when Screen3/4/5 are created */

    /* Screen1 "status" second icons binding (from other screens). */
    if(ui_Image21 && ui_Image12) lv_img_set_src(ui_Image21, lv_img_get_src(ui_Image12));
    if(ui_Image28 && ui_Image14) lv_img_set_src(ui_Image28, lv_img_get_src(ui_Image14));
    if(ui_Image33 && ui_Image17) lv_img_set_src(ui_Image33, lv_img_get_src(ui_Image17));

    /* 启动时钟与室内温度两个周期定时器（模块内部幂等处理）。 */
    ui_runtime_timers_start();

    /* 无操作超时 + 时段门控 + 屏保开关 → 熄屏待机层，见 ui_runtime_ambient.c */
    ui_runtime_ambient_init();

    /* 启动后分批完成常用屏创建 + 轻量图片预热，降低首次切屏偶发卡顿。 */
    ui_runtime_boot_prewarm_start();
}


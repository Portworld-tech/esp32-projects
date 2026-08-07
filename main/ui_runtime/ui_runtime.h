#pragma once

#include <stdbool.h>
#include <stdint.h>

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply runtime UI/LVGL optimizations and event rewiring.
 *
 * Call this once after `ui_init()` while holding the LVGL mutex.
 * This file lives outside `ui/` so regenerating the UI won't overwrite it.
 *
 * WiFi 中文 SSID 字体、Screen7 键盘与连接逻辑、手势与导航等“重要行为”均在此实现；
 * `ui/` 为 SquareLine 生成的布局；部分控件可能无占位事件，由本文件直接 `add_event_cb`。
 */
void ui_runtime_apply(void);

/** 全局取消按钮按下放大（覆盖 LVGL default theme grow），避免破坏布局。 */
void ui_runtime_disable_btn_grow_everywhere(void);

/** 仅处理指定屏（切屏时用，避免遍历全树拖慢 RGB 刷新导致撕裂）。 */
void ui_runtime_disable_btn_grow_on(lv_obj_t *root);

/** True while a runtime-managed screen transition is in progress (LVGL thread only). */
bool ui_runtime_screen_switch_busy(void);

/**
 * Mark active screen dirty; LVGL refr timer + direct_mode flush updates the panel.
 * Do not call from screen-switch path — use ui_runtime_rgb_commit_full_screen().
 */
void ui_runtime_rgb_commit_active_screen(void);

/** One lv_refr_now after SCREEN_LOADED (screen switches only). */
void ui_runtime_rgb_commit_full_screen(void);

/**
 * @brief Update Screen11 network info labels (LVGL thread only).
 *
 * This updates `ui_Label79` and `ui_Label80` on Screen11 to include current
 * WiFi MAC address and IP address. Call it from the GUI/LVGL context
 * (e.g., via gui_task_post_lvgl()).
 *
 * @param mac  MAC string like "AA:BB:CC:DD:EE:FF" (NULL allowed)
 * @param ip   IP string like "192.168.1.10" (NULL allowed; shows "-")
 */
void ui_runtime_set_network_info(const char *mac, const char *ip);

/**
 * @brief Step Screen3 (air conditioner) temperature by delta (LVGL thread only).
 * Screen3 temperature is clamped to [16, 30] and will persist to NVS.
 */
void ui_runtime_step_screen3_temp(int delta);

/**
 * @brief Step Screen5 (underfloor heating) temperature by delta (LVGL thread only).
 * Screen5 temperature is clamped to [16, 32] and will persist to NVS.
 */
void ui_runtime_step_screen5_temp(int delta);

/**
 * Snapshot for cloud sync (LVGL thread only). item i1/i2/i3 map to MQTT items 1/2/3 (Switch4/3/1).
 */
typedef struct {
    uint8_t i1;
    uint8_t i2;
    uint8_t i3;
    int16_t t3;
    int16_t t5;
    uint8_t e3; /* screen3 (AC) power: allow temp step */
    uint8_t e5; /* screen5 power: allow temp step */
    uint8_t m;  /* mesh prov on */
} ui_runtime_cloud_snapshot_t;

void ui_runtime_fill_cloud_snapshot(ui_runtime_cloud_snapshot_t *out);

/** Whether MQTT temp items 4–7 are allowed (power on for that screen). LVGL thread only. */
bool ui_runtime_allow_mqtt_temp_item(uint8_t item_id);

/**
 * Screen10 theme tile: persist selection (1..4), apply Home bg image, go to Screen1.
 * Call from LVGL thread only (e.g. SquareLine CLICKED handler).
 */
void ui_runtime_screen10_pick_and_home(uint8_t pick);

/**
 * Switch to an already-created SquareLine screen object (LVGL thread only).
 * Uses the same managed path as _ui_screen_change (bind_apply + RGB refresh).
 */
void ui_runtime_screen_change_by_obj(lv_obj_t *scr);

#ifdef __cplusplus
}
#endif


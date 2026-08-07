#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_extra_apply_screen3_power(bool on);
void ui_extra_apply_screen4_power(bool on);
void ui_extra_apply_screen5_power(bool on);

void ui_extra_event_switch4_device1(lv_event_t *e);
void ui_extra_event_switch1_device2(lv_event_t *e);
void ui_extra_event_switch3_device3(lv_event_t *e);

void ui_extra_event_button15_toggle_device1(lv_event_t *e);
void ui_extra_event_button26_toggle_device2(lv_event_t *e);
void ui_extra_event_button20_toggle_device3(lv_event_t *e);

void ui_extra_event_Button5(lv_event_t *e);
void ui_extra_event_Button13(lv_event_t *e);
void ui_extra_event_Button24(lv_event_t *e);
void ui_extra_event_Button25(lv_event_t *e);

void ui_extra_screen3_arc_apply(void);
void ui_extra_screen5_arc_apply(void);

void ui_runtime_step_screen3_temp(int delta);
void ui_runtime_step_screen5_temp(int delta);

void ui_runtime_screen345_bind_apply_for_active_screen(lv_obj_t *act);

/** 切屏前调用：立即提交防抖中的温度 NVS，避免跨屏丢保存、并缩短 LVGL 内阻塞链。 */
void ui_runtime_screen345_flush_pending_temp_save(void);

/** 室内温度文案同步（实现位于 ui_runtime_timers.c，进入 S3/S5 时由 bind 调用） */
void ui_runtime_indoor_labels_sync_from_cache(void);

/** 更新当前活动屏上的室内温度标签（LVGL 线程）。 */
void ui_runtime_indoor_apply_temp(int temp_c);

/**
 * RGB + direct_mode 双缓冲：样式变更后 invalidate 当前屏即可；勿 lv_refr_now / present 双写。
 * 在设备页大动作后整屏脏区 + 立即跑完一次 refr，强制两缓冲与面板一致。
 */
void ui_runtime_screen345_force_coherent_frame(void);

#ifdef __cplusplus
}
#endif

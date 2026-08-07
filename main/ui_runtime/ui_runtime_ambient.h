#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从 NVS 读取熄屏超时与屏保开关（可重复调用以刷新缓存）。
 */
void ui_runtime_ambient_load_settings(void);

/** 无活动多久后允许进入熄屏层（秒；Screen14「休眠」档位 15/30/60/120/300/600）。 */
uint32_t ui_runtime_ambient_get_idle_timeout_sec(void);

/** 写入 NVS。sec 须为上述允许档位之一。 */
void ui_runtime_ambient_set_idle_timeout_sec(uint32_t sec);

/** 是否允许自动进入熄屏层（Screen15；与 CONFIG_UI_AMBIENT_ENABLE 独立存 NVS）。 */
bool ui_runtime_ambient_screensaver_enabled(void);

void ui_runtime_ambient_set_screensaver_enabled(bool on);

/**
 * 本地「允许熄屏」的小时区间（Screen16）：0–23 整点小时；若 start > end 则跨午夜。
 * 例如 start=22 end=6 表示 22:00–06:59。
 */
void ui_runtime_ambient_get_schedule_hours(uint8_t *start_hour_out, uint8_t *end_hour_out);

void ui_runtime_ambient_set_schedule_hours(uint8_t start_hour, uint8_t end_hour);

/** 当前本地时钟小时是否落在上述区间内。 */
bool ui_runtime_ambient_schedule_allows_dim_now(void);

/**
 * 创建熄屏待机界面（时间/日期）与无操作超时逻辑。
 * 须在 LVGL 线程中调用一次（例如 ui_runtime_apply 末尾）。
 */
void ui_runtime_ambient_init(void);

/** 当前是否正在显示熄屏待机层（LVGL 任意线程可读；内部为原子标志）。 */
bool ui_runtime_ambient_is_active(void);

#ifdef __cplusplus
}
#endif

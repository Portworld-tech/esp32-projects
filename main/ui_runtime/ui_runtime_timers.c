#include "ui.h"
#include "ui_runtime_screen345.h"
#include "lvgl.h"

#include "ui_bg_task.h"

#include "sdkconfig.h"

#include <stdio.h>
#include <time.h>

/* Screen1 时钟定时器句柄。 */
static lv_timer_t *s_ui_clock_timer = NULL;

static int s_last_indoor_temp_shown = 999999;

void ui_runtime_indoor_apply_temp(int ti)
{
    lv_obj_t *act = lv_scr_act();
    if (ui_Label91 && act == ui_Screen3) {
        lv_label_set_text_fmt(ui_Label91, "%d°C indoors", ti);
    }
    if (ui_Label93 && act == ui_Screen5) {
        lv_label_set_text_fmt(ui_Label93, "%d°C indoors", ti);
    }
    s_last_indoor_temp_shown = ti;
}

/**
 * 进入 Screen3 / Screen5 时调用：把缓存的室内温一次性写到标签（避免切回来要等 2s 定时器）。
 * 必须在 LVGL 线程调用。
 */
void ui_runtime_indoor_labels_sync_from_cache(void)
{
#if defined(CONFIG_AHT20_ENABLE) && CONFIG_AHT20_ENABLE
    const int cached = ui_bg_task_get_indoor_temp_cached();
    if (cached != 999999) {
        ui_runtime_indoor_apply_temp(cached);
        return;
    }
#endif
    if (s_last_indoor_temp_shown == 999999) {
        return;
    }
    ui_runtime_indoor_apply_temp(s_last_indoor_temp_shown);
}

/* 周期刷新 Screen1 时间与日期文本。 */
static void ui_extra_clock_timer_cb(lv_timer_t *t)
{
    (void)t;

    time_t now = 0;
    struct tm info = {0};
    time(&now);
    localtime_r(&now, &info);

    char date_buf[32];
    char time_buf[32];

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
    snprintf(date_buf, sizeof(date_buf), "%s %d, %s", m, info.tm_mday, w);

    if (ui_Label1) lv_label_set_text(ui_Label1, time_buf);
    if (ui_Label3) lv_label_set_text(ui_Label3, date_buf);
}

/* 启动运行时定时器（重复调用安全）。 */
void ui_runtime_timers_start(void)
{
    if (s_ui_clock_timer == NULL) {
        s_ui_clock_timer = lv_timer_create(ui_extra_clock_timer_cb, 1000, NULL);
        if (s_ui_clock_timer) lv_timer_set_repeat_count(s_ui_clock_timer, -1);
    }
}

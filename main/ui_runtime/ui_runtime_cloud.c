#include "ui_runtime.h"

extern void wifi_bemfa_client_schedule_sync(void);

/* 通知云端：本地状态已变化，触发一次同步调度。 */
void ui_runtime_cloud_notify_changed(void)
{
    wifi_bemfa_client_schedule_sync();
}


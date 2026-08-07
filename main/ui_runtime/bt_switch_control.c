#include "bt_management.h"

#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "gui_task.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_runtime.h"
#include "wifi_bemfa_client.h"

static const char *TAG = "bt_sw";

/* Force this object to be linked even if the weak default handler already satisfied references. */
void bt_switch_control_linker_keep(void) {}

typedef struct {
    uint8_t item_id;
    uint8_t value;
} bt_sw_cmd_t;

static void bt_sw_apply_lvgl(void *user_data)
{
    bt_sw_cmd_t *cmd = (bt_sw_cmd_t *)user_data;
    if (cmd == NULL) {
        return;
    }

    /* Temperature control:
     * item_id mapping:
     *   4: screen3 temp -1
     *   5: screen3 temp +1
     *   6: screen5 temp -1
     *   7: screen5 temp +1
     *
     * value is treated as enable (0=no-op, !=0=apply).
     */
    if (cmd->item_id >= 4 && cmd->item_id <= 7) {
        if (cmd->value != 0) {
            if (!ui_runtime_allow_mqtt_temp_item(cmd->item_id)) {
                ESP_LOGW(TAG, "temp step rejected (screen power off) item=%u", (unsigned)cmd->item_id);
                wifi_bemfa_client_publish_status_u8(cmd->item_id, 0, false);
                free(cmd);
                return;
            }
            switch (cmd->item_id) {
            case 4: ui_runtime_step_screen3_temp(-1); break;
            case 5: ui_runtime_step_screen3_temp(+1); break;
            case 6: ui_runtime_step_screen5_temp(-1); break;
            case 7: ui_runtime_step_screen5_temp(+1); break;
            default: break;
            }
            wifi_bemfa_client_publish_status_u8(cmd->item_id, 1, true);
            wifi_bemfa_client_schedule_sync();
        }
        free(cmd);
        return;
    }

    /* Mesh provisioning control (gateway side).
     * item_id=10:
     *   value=0 -> disable mesh PB-ADV provisioning
     *   value!=0 -> enable mesh PB-ADV provisioning
     */
    if (cmd->item_id == 10) {
        const bool enable = (cmd->value != 0);
        (void)bt_management_mesh_set_prov_enable(enable);
        ESP_LOGI(TAG, "mesh prov enable request: %d", enable ? 1 : 0);
        wifi_bemfa_client_schedule_sync();
        free(cmd);
        return;
    }

    lv_obj_t *sw = NULL;
    switch (cmd->item_id) {
    case 1: sw = ui_Switch4; break;
    case 2: sw = ui_Switch3; break;
    case 3: sw = ui_Switch1; break;
    default: break;
    }

    if (sw == NULL) {
        free(cmd);
        return;
    }

    const bool on = (cmd->value != 0);
    const bool cur_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (cur_on != on) {
        if (on) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }

        /* Let existing ui_runtime.c handlers run (save NVS + apply linked UI states). */
        (void)lv_event_send(sw, LV_EVENT_VALUE_CHANGED, NULL);
    } else {
        ESP_LOGI(TAG, "no-op SET_STATE item=%u already=%u", (unsigned)cmd->item_id, (unsigned)on);
    }

    wifi_bemfa_client_schedule_sync();
    free(cmd);
}

esp_err_t bt_management_apply_set_state(uint8_t item_id, uint8_t value)
{
    bt_sw_cmd_t *cmd = (bt_sw_cmd_t *)malloc(sizeof(bt_sw_cmd_t));
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cmd->item_id = item_id;
    cmd->value = value;

    if (!gui_task_post_lvgl(bt_sw_apply_lvgl, cmd)) {
        free(cmd);
        ESP_LOGW(TAG, "gui_task_post_lvgl failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "queued SET_STATE item=%u value=%u", (unsigned)item_id, (unsigned)value);
    return ESP_OK;
}


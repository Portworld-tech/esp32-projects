#include "ui_runtime.h"

#include "wifi_ui_font.h"
#include "ui.h"
#include "ui_helpers.h"
#include "lvgl.h"
#include "wifi_management.h"

#include <stdio.h>

/* Screen11 网络信息缓存。 */
static char s_net_mac[18] = {0};
static char s_net_ip[16] = {0};

/* 更新网络信息并同步刷新 UI（IP/MAC/Wi-Fi 图标）。 */
void ui_runtime_set_network_info(const char *mac, const char *ip)
{
    if (mac && mac[0]) snprintf(s_net_mac, sizeof(s_net_mac), "%s", mac);
    else s_net_mac[0] = '\0';

    if (ip && ip[0]) snprintf(s_net_ip, sizeof(s_net_ip), "%s", ip);
    else s_net_ip[0] = '\0';

    if (ui_Label79) {
        lv_label_set_text_fmt(ui_Label79, "WIFI address:%s%s",
                              (s_net_mac[0] ? " " : ""),
                              (s_net_mac[0] ? s_net_mac : "-"));
    }
    if (ui_Label80) {
        lv_label_set_text_fmt(ui_Label80, "IP address:%s%s",
                              (s_net_ip[0] ? " " : ""),
                              (s_net_ip[0] ? s_net_ip : "-"));
    }

    if (ui_Image39) {
        if (s_net_ip[0]) lv_img_set_src(ui_Image39, UI_SRC_WIFI);
        else lv_img_set_src(ui_Image39, UI_SRC_WIFICLOSE);
    }
}

/* Re-apply cached MAC/IP to Screen11 labels (e.g. after managed screen switch). */
void ui_runtime_network_refresh_labels(void)
{
    ui_runtime_set_network_info(s_net_mac[0] ? s_net_mac : NULL, s_net_ip[0] ? s_net_ip : NULL);
}

/* Screen7 加载后触发一次 Wi-Fi 扫描刷新（已连接时跳过，避免断线）。 */
void ui_extra_screen7_wifi_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) return;
    if (wifi_management_is_connected()) {
        return;
    }
    wifi_management_request_scan_refresh();
}

/* Wi-Fi 下拉框事件：切换选项时清密码；列表关闭后刷新热点并应用延迟的扫描结果。 */
void ui_extra_event_dropdown1(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED && ui_Dropdown1) {
        lv_dropdown_set_text(ui_Dropdown1, NULL);
        if (!wifi_management_is_dropdown_refresh() && ui_TextArea1) {
            lv_textarea_set_text(ui_TextArea1, "");
        }
    } else if (code == LV_EVENT_CANCEL && ui_Dropdown1) {
        /* LVGL binds the open list label to dropdown->options; never call
         * lv_dropdown_set_options while the list is visible. */
        wifi_management_dropdown_list_closed();
        if (!wifi_management_is_connected()) {
            wifi_management_request_scan_refresh();
        }
    }
}

/* Wi-Fi 密码输入框事件：聚焦时展开键盘区域，失焦时恢复布局。 */
void ui_extra_event_textarea1(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        _ui_keyboard_set_target(ui_Keyboard2, ui_TextArea1);
        _ui_flag_modify(ui_Container74, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Container71, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_basic_set_property(ui_Container72, _UI_BASIC_PROPERTY_HEIGHT, 432);
        _ui_basic_set_property(ui_Container73, _UI_BASIC_PROPERTY_HEIGHT, 238);
        _ui_basic_set_property(ui_Container74, _UI_BASIC_PROPERTY_HEIGHT, 194);
        ui_runtime_rgb_commit_active_screen();
    } else if (code == LV_EVENT_DEFOCUSED) {
        _ui_flag_modify(ui_Container71, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Container74, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_basic_set_property(ui_Container74, _UI_BASIC_PROPERTY_HEIGHT, 0);
        _ui_basic_set_property(ui_Container72, _UI_BASIC_PROPERTY_HEIGHT, 336);
        _ui_basic_set_property(ui_Container73, _UI_BASIC_PROPERTY_HEIGHT, 235);
    }
}

/* 屏幕键盘事件：确认时发起连接，取消/完成后收起键盘区。 */
void ui_extra_event_keyboard2(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (code == LV_EVENT_READY && ui_Dropdown1 && ui_TextArea1) {
            char ssid[64];
            lv_dropdown_get_selected_str(ui_Dropdown1, ssid, sizeof(ssid));
            (void)wifi_management_connect(ssid, lv_textarea_get_text(ui_TextArea1));
        }
        _ui_flag_modify(ui_Container71, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Container74, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_basic_set_property(ui_Container74, _UI_BASIC_PROPERTY_HEIGHT, 0);
        _ui_basic_set_property(ui_Container72, _UI_BASIC_PROPERTY_HEIGHT, 336);
        _ui_basic_set_property(ui_Container73, _UI_BASIC_PROPERTY_HEIGHT, 235);
        if (ui_TextArea1) lv_obj_clear_state(ui_TextArea1, LV_STATE_FOCUSED);
        ui_runtime_rgb_commit_active_screen();
    }
}

/* 应用 Wi-Fi 文本字体与布局（含中文 SSID 显示）。 */
void ui_extra_apply_wifi_ssid_runtime(void)
{
    wifi_ui_font_init();
    if (ui_Label24) {
        lv_obj_set_style_text_font(ui_Label24, wifi_ui_font_ssid(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(ui_Label24, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_Label24, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    if (ui_Label43) {
        lv_obj_set_style_text_font(ui_Label43, wifi_ui_font_ssid(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(ui_Label43, 210, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_Label43, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
    if (ui_Dropdown1) {
        const lv_font_t *fssid = wifi_ui_font_ssid();
        lv_obj_set_style_text_font(ui_Dropdown1, &lv_font_montserrat_14, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_Dropdown1, fssid, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_Dropdown1, fssid, LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_t *dd_list = lv_dropdown_get_list(ui_Dropdown1);
        if (dd_list) {
            lv_obj_set_style_text_font(dd_list, fssid, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(dd_list, fssid, LV_PART_SELECTED | LV_STATE_DEFAULT);
        }
    }
}


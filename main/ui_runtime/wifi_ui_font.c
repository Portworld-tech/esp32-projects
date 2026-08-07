#include "wifi_ui_font.h"

#include "lv_font_source_han_14_cjk3500.h"

static lv_font_t s_font_wifi_ssid;
static bool s_wifi_font_inited;

void wifi_ui_font_init(void)
{
    if (s_wifi_font_inited) {
        return;
    }
    s_wifi_font_inited = true;
    s_font_wifi_ssid = lv_font_source_han_14_cjk3500;
#if LV_FONT_SIMSUN_16_CJK
    /* 补全 3500 字表外或未收录的汉字；ASCII 仍由主字体绘制。 */
    s_font_wifi_ssid.fallback = &lv_font_simsun_16_cjk;
#endif
}

const lv_font_t *wifi_ui_font_ssid(void)
{
    if (!s_wifi_font_inited) {
        wifi_ui_font_init();
    }
    return &s_font_wifi_ssid;
}

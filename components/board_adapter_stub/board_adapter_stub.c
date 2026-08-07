#include "withthewind_board_lvgl_init.h"
#include "esp_log.h"

static const char *TAG = "board_stub";

esp_err_t board_display_start_with_lvgl_cfg(const lvgl_port_cfg_t *lvgl_cfg)
{
    (void)lvgl_cfg;
    ESP_LOGE(TAG, "board BSP not included in public share 鈥?provide your adapter");
    return ESP_ERR_NOT_SUPPORTED;
}
i2c_master_bus_handle_t board_i2c_get_handle(void) { return NULL; }
esp_err_t board_backlight_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t board_backlight_set(int brightness_percent) { (void)brightness_percent; return ESP_ERR_NOT_SUPPORTED; }
int board_backlight_quantize_percent(int brightness_percent) { return brightness_percent; }
int board_backlight_clamp_percent(int brightness_percent) { return brightness_percent; }
esp_err_t board_backlight_on(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t board_backlight_off(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t board_beep_set(int on) { (void)on; return ESP_OK; }
void board_display_present_fb(const void *rendered_fb) { (void)rendered_fb; }
void board_display_present_frame(void) {}

idf_component_register(
    SRCS "board_adapter_stub.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd espressif__esp_lvgl_port
)

# --- Public share notes ---
# Board pin BSP removed. Linked component: board_adapter_stub (API only).
# Dropped from this export: board/board_ethernet_ch390.c, board/aht20.c
# Panel/touch REQUIRES below may remain for reference; replace with your BSP deps.
# Compile-time UI theme pack 鈥?selected in app_ui_theme_select.h (not menuconfig)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/resolve_ui_theme.cmake")

set(UI_THEMES_ROOT "${CMAKE_CURRENT_LIST_DIR}/../ui/themes")
set(UI_DIR "${UI_THEMES_ROOT}/${UI_THEME_ID}")
if(NOT EXISTS "${UI_DIR}")
    message(FATAL_ERROR "UI theme directory missing: ${UI_DIR}")
endif()
message(STATUS "UI theme dir: ${UI_DIR}")

set(MAIN_REQUIRES
        esp_wifi
        esp_netif
        esp_eth
        esp_event
        esp_pm
        esp_http_client
        esp_timer
        esp_driver_spi
        esp_driver_gpio
        esp_driver_i2c
        espressif__ch390
        espressif__esp_lvgl_port
        espressif__esp_lcd_panel_io_additions
        espressif__esp_lcd_st7701
        espressif__esp_lcd_touch_gt911
        board_adapter_stub
        lvgl__lvgl
        mqtt
        nvs_flash
        bt
        spiffs
        bt_management
        gui_task
        app_health
)

set(MAIN_SRCS
        main.c
        app/app_low_power.c
        app/ui_assets_warm.c
        gui/ui_bg_task.c
        "fonts/lv_font_source_han_14_cjk3500.c"
        "fonts/lv_font_source_han_ambient_time_150.c"
        "fonts/lv_font_source_han_screen1_time_100.c"
        "../wifi_management/wifi_management.c"
        "../wifi_management/wifi_bemfa_client.c"
)

set(MAIN_INCLUDE_DIRS
        .
        board
        app
        gui
        fonts
        "../wifi_management"
)

if(UI_THEME_IS_HUB)
    # Hub themes: thin runtime + per-theme multi-file pack (clear roles)
    list(APPEND MAIN_SRCS
        hub_ui/hub_ui.c
        hub_ui/hub_shell.c
        hub_ui/hub_nav.c
        hub_ui/hub_font.c
        hub_ui/hub_model.c
        hub_ui/hub_icons.c
        hub_ui/hub_room_ui.c
        hub_ui/hub_device_ui.c
        hub_ui/hub_pages_common.c
        hub_ui/hub_pages_life.c
        hub_ui/hub_i18n.c
        hub_ui/hub_standby.c
        hub_ui/ui_compat_stubs.c
        hub_ui/ui_runtime_stubs.c
        hub_ui/wifi_ui_font_stub.c
        "fonts/lv_font_hub_cjk_extra.c"
        "${UI_DIR}/palette.c"
        "${UI_DIR}/theme_local.c"
        "${UI_DIR}/home.c"
        "${UI_DIR}/pages_room.c"
        "${UI_DIR}/pages_scenes.c"
        "${UI_DIR}/pages_ops.c"
        "${UI_DIR}/pages_life.c"
        "${UI_DIR}/boot.c"
    )
    if(CONFIG_APP_USE_SPIFFS_UI_ASSETS)
        list(APPEND MAIN_SRCS "app/app_spiffs.c")
    endif()
    list(APPEND MAIN_INCLUDE_DIRS
        hub_ui
        ui_runtime
        "${UI_DIR}"
    )
else()
    # Default SquareLine product UI
    execute_process(
        COMMAND ${CMAKE_COMMAND}
                -DUI_DIR=${UI_DIR}
                -P "${CMAKE_CURRENT_LIST_DIR}/cmake/fix_ui_helpers_signature.cmake"
    )

    file(GLOB _ui_screen_glob "${UI_DIR}/screens/ui_Screen*.c")
    set(UI_SCREEN_SRCS "")
    foreach(_s IN LISTS _ui_screen_glob)
        get_filename_component(_bn "${_s}" NAME)
        if(NOT _bn STREQUAL "ui_Screen14.c" AND NOT _bn STREQUAL "ui_Screen15.c" AND NOT _bn STREQUAL "ui_Screen16.c")
            list(APPEND UI_SCREEN_SRCS "${_s}")
        endif()
    endforeach()
    list(APPEND UI_SCREEN_SRCS
        "${UI_DIR}/screens/ui_Screen14.c"
        "${UI_DIR}/screens/ui_Screen15.c"
        "${UI_DIR}/screens/ui_Screen16.c")

    if(CONFIG_APP_USE_SPIFFS_UI_ASSETS)
        set(UI_IMAGE_SRCS "")
        set(APP_SPIFFS_SRCS "app/app_spiffs.c")
    else()
        file(GLOB UI_IMAGE_SRCS "${UI_DIR}/images/ui_img_*.c")
        list(APPEND UI_IMAGE_SRCS "${UI_DIR}/images/ui_img_icon_png.c")
        list(APPEND UI_IMAGE_SRCS "${UI_DIR}/images/ui_temporary_image.c")
        list(REMOVE_DUPLICATES UI_IMAGE_SRCS)
        set(APP_SPIFFS_SRCS "")
    endif()

    list(APPEND MAIN_SRCS
        ui_runtime/ui_runtime.c
        ui_runtime/ui_runtime_screen1.c
        ui_runtime/ui_runtime_timers.c
        ui_runtime/ui_runtime_network.c
        ui_runtime/ui_runtime_utils.c
        ui_runtime/ui_runtime_cloud.c
        ui_runtime/ui_runtime_audio_brightness.c
        ui_runtime/ui_runtime_storage.c
        ui_runtime/ui_runtime_screen345.c
        ui_runtime/ui_runtime_ambient.c
        ui_runtime/bt_switch_control.c
        ui_runtime/wifi_ui_font.c
        ${APP_SPIFFS_SRCS}
        ${UI_SCREEN_SRCS}
        "${UI_DIR}/ui.c"
        "${UI_DIR}/components/ui_comp_hook.c"
        "${UI_DIR}/ui_helpers.c"
        "${UI_DIR}/app_ui_start.c"
        ${UI_IMAGE_SRCS}
    )
    list(APPEND MAIN_INCLUDE_DIRS
        ui_runtime
        "${UI_DIR}"
        "${UI_DIR}/components"
        "${UI_DIR}/screens"
        "${UI_DIR}/images"
    )
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS ${MAIN_INCLUDE_DIRS}
    REQUIRES ${MAIN_REQUIRES}
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
if(UI_THEME_IS_HUB)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_UI_THEME_HUB=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_UI_THEME_DEFAULT=1)
endif()

idf_component_get_property(LVGL_DIR lvgl__lvgl COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${LVGL_DIR}")

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
idf_component_get_property(WTW_BOARD_DIR board_adapter_stub COMPONENT_DIR)
target_include_directories(${LVGL_LIB} PRIVATE "${WTW_BOARD_DIR}/include")
target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_MEM_CUSTOM=1
        -DLV_MEM_CUSTOM_INCLUDE=\"lv_mem_psram_custom.h\"
        -DLV_MEM_CUSTOM_ALLOC=lv_mem_psram_malloc
        -DLV_MEM_CUSTOM_FREE=lv_mem_psram_free
        -DLV_MEM_CUSTOM_REALLOC=lv_mem_psram_realloc
)

target_compile_definitions(lvgl_port_lib PRIVATE LV_MEM_CUSTOM=1)


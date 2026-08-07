# LVGL Hub 鈥?public software share (no board pins)

This tree is an **analysis / portability** export of the hub application software.

## Included
- Hub UI (main/hub_ui), theme packs (ui/themes/*), SPIFFS icons
- Wi-Fi + Bemfa MQTT client sources
- Thin components: gui_task, pp_health, BT app API (not full t/ stack)
- Front-end prototype (lvgl-front), tools, non-pin docs

## Excluded on purpose
- GPIO / pin maps, sdkconfig.board.*.defaults, expander pin headers
- Product BSP withthewind_board_lvgl_init (replaced by oard_adapter_stub)
- Legacy Smart-LVGL-main*, firmware bins, partner .a
- uild/, managed_components/

## Build expectation
Do **not** expect a flashable binary for our hardware from this tree alone.
Implement components/board_adapter_stub (or your BSP) and supply board sdkconfig defaults.

See docs/GITHUB_SHARE.md.

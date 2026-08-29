# ESP32 Projects Showcase

Organization: **Portworld-tech**  
Contact: xjunsoftware@ycxytech.com

Public collection of **ESP32** software samples for analysis and learning. Board pin maps and proprietary BSP are **not** included.

## Projects

| Folder / tree | Description |
|---------------|-------------|
| *(repository root)* | LVGL8 hub UI firmware software layer (themes, Wi‑Fi/MQTT, SPIFFS icons). Replace `board_adapter_stub` with your BSP to run on hardware. |
| **[43p-esp32-neutral-software](https://github.com/Portworld-tech/43p-esp32-neutral-software)** | 43P customer / OEM layered SDK (closed `.a` + open Hub themes). Browser UI kit under `lvgl-front/`. |

### 43P UI theme preview (below firmware package)

Interactive ten-theme Hub demo (480×480):

- Repo: https://github.com/Portworld-tech/43p-esp32-neutral-software  
- Live: https://portworld-tech.github.io/43p-esp32-neutral-software/  
- Local: `cd lvgl-front && npx serve .`

More ESP32 demos will be added as separate directories over time.

## Notes

- See `docs/GITHUB_SHARE.md` for what is shared vs excluded.
- `components/board_adapter_stub` is an API-only stub (no GPIO / pin tables).
# Export portable software tree for GitHub analysis (no pins / BSP).
# Usage: powershell -File tools/export_github_share.ps1 [-OutDir path]

param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $OutDir) {
    $OutDir = Join-Path $Root ".github_share_export"
}

function Ensure-Dir([string]$p) {
    if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

function Copy-Tree([string]$rel, [string[]]$ExcludeNames = @()) {
    $src = Join-Path $Root $rel
    if (-not (Test-Path $src)) {
        Write-Host "skip missing: $rel"
        return
    }
    $dst = Join-Path $OutDir $rel
    Ensure-Dir (Split-Path $dst -Parent)
    if (Test-Path $src -PathType Container) {
        Ensure-Dir $dst
        Get-ChildItem -Path $src -Force | ForEach-Object {
            if ($ExcludeNames -contains $_.Name) { return }
            $target = Join-Path $dst $_.Name
            if ($_.PSIsContainer) {
                Copy-Item -Path $_.FullName -Destination $target -Recurse -Force
            } else {
                Copy-Item -Path $_.FullName -Destination $target -Force
            }
        }
    } else {
        Ensure-Dir (Split-Path $dst -Parent)
        Copy-Item -Path $src -Destination $dst -Force
    }
    Write-Host "ok  $rel"
}

if (Test-Path $OutDir) {
    Remove-Item -Path $OutDir -Recurse -Force
}
Ensure-Dir $OutDir

Write-Host "Export root: $OutDir"
Write-Host "Source:      $Root"

# ── Core app (no main/board) ─────────────────────────────────────
Ensure-Dir (Join-Path $OutDir "main")
$mainKeep = @(
    "hub_ui", "app", "gui", "ui_runtime", "fonts", "cmake",
    "app_ui.h", "app_ui_theme_select.h", "main.c",
    "CMakeLists.txt", "idf_component.yml", "Kconfig.projbuild"
)
foreach ($n in $mainKeep) {
    $src = Join-Path $Root "main\$n"
    if (-not (Test-Path $src)) { Write-Host "skip missing: main/$n"; continue }
    $dst = Join-Path $OutDir "main\$n"
    if ((Get-Item $src).PSIsContainer) {
        # fonts: skip OTF / node_modules
        if ($n -eq "fonts") {
            Ensure-Dir $dst
            Get-ChildItem $src -File | Where-Object {
                $_.Extension -in ".c", ".h", ".md", ".py", ".txt" -and $_.Name -notlike "_*"
            } | Copy-Item -Destination $dst -Force
            Write-Host "ok  main/fonts (generated only)"
        } else {
            Copy-Item -Path $src -Destination $dst -Recurse -Force
            Write-Host "ok  main/$n"
        }
    } else {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "ok  main/$n"
    }
}

# Strip pin-heavy Kconfig from export: keep a stub note instead of real pin map
$kcfg = Join-Path $OutDir "main\Kconfig.projbuild"
if (Test-Path $kcfg) {
    @"
# Pin / board GPIO menus intentionally omitted from the public share.
# Local product builds use the private main/Kconfig.projbuild + board defaults.
# Application code talks to board_* APIs only (see components/board_adapter_stub).
"@ | Set-Content -Path $kcfg -Encoding UTF8
    Write-Host "ok  main/Kconfig.projbuild -> stub"
}

# ── Themes (hub packs only; skip default SquareLine megabitmaps if huge) ─
$themesRoot = Join-Path $Root "ui\themes"
$themesOut = Join-Path $OutDir "ui\themes"
Ensure-Dir $themesOut
$hubThemes = @(
    "bloom", "slate", "sand", "ink", "forest", "dusk", "ocean", "zen", "pulse", "metro", "_template"
)
foreach ($t in $hubThemes) {
    $src = Join-Path $themesRoot $t
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $themesOut $t) -Recurse -Force
        Write-Host "ok  ui/themes/$t"
    }
}
$uiReadme = Join-Path $Root "ui\README.md"
if (Test-Path $uiReadme) {
    Ensure-Dir (Join-Path $OutDir "ui")
    Copy-Item $uiReadme (Join-Path $OutDir "ui\README.md") -Force
}

# ── Wi-Fi (no wifi_example) ──────────────────────────────────────
Ensure-Dir (Join-Path $OutDir "wifi_management")
foreach ($f in @("wifi_management.c", "wifi_management.h", "wifi_bemfa_client.c", "wifi_bemfa_client.h")) {
    $src = Join-Path $Root "wifi_management\$f"
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $OutDir "wifi_management\$f") -Force
        Write-Host "ok  wifi_management/$f"
    }
}

# ── BT app sources only ──────────────────────────────────────────
Ensure-Dir (Join-Path $OutDir "bt_management")
Get-ChildItem (Join-Path $Root "bt_management") -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in ".c", ".h", ".md", ".txt" } |
    ForEach-Object {
        Copy-Item $_.FullName (Join-Path $OutDir "bt_management\$($_.Name)") -Force
        Write-Host "ok  bt_management/$($_.Name)"
    }

# ── Thin components ──────────────────────────────────────────────
foreach ($c in @("gui_task", "app_health", "bt_management")) {
    Copy-Tree "components\$c"
}
$compReadme = Join-Path $Root "components\README.md"
if (Test-Path $compReadme) {
    Ensure-Dir (Join-Path $OutDir "components")
    Copy-Item $compReadme (Join-Path $OutDir "components\README.md") -Force
}

# ── Board adapter STUB (API surface only, no pins) ───────────────
$stubDir = Join-Path $OutDir "components\board_adapter_stub"
Ensure-Dir (Join-Path $stubDir "include")
@"
#pragma once
/* Public share stub — replace with your board BSP. No GPIO / pin maps here. */
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_display_start_with_lvgl_cfg(const lvgl_port_cfg_t *lvgl_cfg);
i2c_master_bus_handle_t board_i2c_get_handle(void);
esp_err_t board_backlight_init(void);
esp_err_t board_backlight_set(int brightness_percent);
int board_backlight_quantize_percent(int brightness_percent);
int board_backlight_clamp_percent(int brightness_percent);
esp_err_t board_backlight_on(void);
esp_err_t board_backlight_off(void);
esp_err_t board_beep_set(int on);
void board_display_present_fb(const void *rendered_fb);
void board_display_present_frame(void);

#ifdef __cplusplus
}
#endif
"@ | Set-Content -Path (Join-Path $stubDir "include\withthewind_board_lvgl_init.h") -Encoding UTF8

@"
idf_component_register(
    SRCS "board_adapter_stub.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd espressif__esp_lvgl_port
)
"@ | Set-Content -Path (Join-Path $stubDir "CMakeLists.txt") -Encoding UTF8

@"
#include "withthewind_board_lvgl_init.h"
#include "esp_log.h"

static const char *TAG = "board_stub";

esp_err_t board_display_start_with_lvgl_cfg(const lvgl_port_cfg_t *lvgl_cfg)
{
    (void)lvgl_cfg;
    ESP_LOGE(TAG, "board BSP not included in public share — provide your adapter");
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
"@ | Set-Content -Path (Join-Path $stubDir "board_adapter_stub.c") -Encoding UTF8
Write-Host "ok  components/board_adapter_stub (API only)"

# ── Assets / tools / docs / front ────────────────────────────────
Copy-Tree "spiffs_image"
Copy-Tree "tools"
Copy-Tree "cmake"
Copy-Tree "lvgl-front"
Copy-Tree "wx"

# docs with exclusions (match by name pattern — avoid locale encoding issues)
$docsSrc = Join-Path $Root "docs"
$docsDst = Join-Path $OutDir "docs"
if (Test-Path $docsSrc) {
    Ensure-Dir $docsDst
    Get-ChildItem $docsSrc -Recurse -File | ForEach-Object {
        $n = $_.Name
        if ($n -eq "BOARD_PORTING_JOURNAL.md") { return }
        if ($n -like "02_*") { return }   # board display/touch
        if ($n -like "14_*") { return }   # CH390 eth wiring
        if ($n -like "15_*") { return }   # NCA9555 expander
        if ($n -match "CH390|NCA9555|pinconfig|BOARD_PORTING") { return }
        $rel = $_.FullName.Substring($docsSrc.Length).TrimStart("\", "/")
        $target = Join-Path $docsDst $rel
        Ensure-Dir (Split-Path $target -Parent)
        Copy-Item $_.FullName $target -Force
    }
    Copy-Item (Join-Path $Root "docs\GITHUB_SHARE.md") (Join-Path $docsDst "GITHUB_SHARE.md") -Force
    Write-Host "ok  docs (pin/board docs excluded)"
}

# config without board pin profile
Ensure-Dir (Join-Path $OutDir "config")
foreach ($f in @("sdkconfig.common.defaults", "sdkconfig.lvgl.defaults")) {
    $src = Join-Path $Root "config\$f"
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $OutDir "config\$f") -Force
        Write-Host "ok  config/$f"
    }
}

# Root files
foreach ($f in @(
    "partitions.csv",
    "PROJECT_TECH_STACK.md",
    "COMPONENT_PUBLISH_GUIDE.md",
    "LICENSE",
    "LICENSE.txt"
)) {
    $src = Join-Path $Root $f
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $OutDir $f) -Force
        Write-Host "ok  $f"
    }
}

# Root CMake without board.395hsd defaults; point REQUIRES note
@"
cmake_minimum_required(VERSION 3.19)

# Public share: no board pin profile. Provide your own sdkconfig board defaults locally.
set(SDKCONFIG_DEFAULTS
    "`${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig.common.defaults"
    "`${CMAKE_CURRENT_LIST_DIR}/config/sdkconfig.lvgl.defaults"
    "`${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
)

include(`$ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)
project(lvgl_demo_v8)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "`${CMAKE_CURRENT_LIST_DIR}/main/app_ui_theme_select.h")

include("`${CMAKE_CURRENT_LIST_DIR}/cmake/lvgl_psram_heap.cmake")
lvgl_psram_heap_force_link()

spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)
"@ | Set-Content -Path (Join-Path $OutDir "CMakeLists.txt") -Encoding UTF8
Write-Host "ok  CMakeLists.txt (no board.395hsd defaults)"

# Patch main/CMakeLists.txt in export: withthewind -> board_adapter_stub, drop board srcs & panel REQUIRES note
$mainCmake = Join-Path $OutDir "main\CMakeLists.txt"
if (Test-Path $mainCmake) {
    $txt = Get-Content $mainCmake -Raw
    $txt = $txt -replace "withthewind_board_lvgl_init", "board_adapter_stub"
    $txt = $txt -replace "(?m)^\s*board/board_ethernet_ch390\.c\r?\n", ""
    $txt = $txt -replace "(?m)^\s*board/aht20\.c\r?\n", ""
    # Soften hard panel deps for analysis tree (keep listed but commented guidance)
    $banner = @"
# --- Public share notes ---
# Board pin BSP removed. Linked component: board_adapter_stub (API only).
# Dropped from this export: board/board_ethernet_ch390.c, board/aht20.c
# Panel/touch REQUIRES below may remain for reference; replace with your BSP deps.

"@
    $txt = $banner + $txt
    Set-Content -Path $mainCmake -Value $txt -Encoding UTF8
    Write-Host "ok  main/CMakeLists.txt patched for stub board"
}

@"
# LVGL Hub — public software share (no board pins)

This tree is an **analysis / portability** export of the hub application software.

## Included
- Hub UI (`main/hub_ui`), theme packs (`ui/themes/*`), SPIFFS icons
- Wi-Fi + Bemfa MQTT client sources
- Thin components: `gui_task`, `app_health`, BT app API (not full `bt/` stack)
- Front-end prototype (`lvgl-front`), tools, non-pin docs

## Excluded on purpose
- GPIO / pin maps, `sdkconfig.board.*.defaults`, expander pin headers
- Product BSP `withthewind_board_lvgl_init` (replaced by `board_adapter_stub`)
- Legacy `Smart-LVGL-main*`, firmware bins, partner `.a`
- `build/`, `managed_components/`

## Build expectation
Do **not** expect a flashable binary for our hardware from this tree alone.
Implement `components/board_adapter_stub` (or your BSP) and supply board `sdkconfig` defaults.

See `docs/GITHUB_SHARE.md`.
"@ | Set-Content -Path (Join-Path $OutDir "README.md") -Encoding UTF8

@"
build/
managed_components/
dependencies.lock
sdkconfig
sdkconfig.old
*.elf
*.map
*.bin
!spiffs_image/
.vscode/
.idea/
__pycache__/
"@ | Set-Content -Path (Join-Path $OutDir ".gitignore") -Encoding UTF8

Write-Host ""
Write-Host "Done. Review $OutDir then init a new git repo there for GitHub upload."
Write-Host "Guide: docs/GITHUB_SHARE.md"

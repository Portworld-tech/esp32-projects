# components/ — 本地 ESP-IDF 组件

本目录存放**可独立发布、可复用**的 ESP-IDF 组件。根工程 `lvgl_demo_v8` 与骨架工程 `esp32_s3_frame` 均通过 ESP-IDF 组件搜索路径自动发现此处（骨架工程另加 `EXTRA_COMPONENT_DIRS` 指向 `esp32_s3_frame/components/`）。

---

## 组件索引

| 组件 | 版本 | 职责 | 主要 API |
|------|------|------|----------|
| [withthewind_board_lvgl_init](./withthewind_board_lvgl_init/) | 0.1.1 | ST7701 RGB LCD、GT911 触摸、NCA9555 IO 扩展、LVGL PSRAM 分配器 | `board_display_start_with_lvgl_cfg()`、`board_backlight_*()` |
| [bt_management](./bt_management/) | — | BLE GATT / SPP / Mesh 栈封装 | `bt_management_init()`、`bt_management_start()` |
| [gui_task](./gui_task/) | — | LVGL 异步任务队列 | `gui_task_init()`、`gui_task_post_lvgl()` |
| [app_health](./app_health/) | — | 长稳运行堆内存监控 | `app_health_monitor_start()` |

---

## 标准目录布局

每个可发布组件应遵循 ESP-IDF 约定：

```text
<component_name>/
├── CMakeLists.txt          # idf_component_register
├── idf_component.yml       # Component Manager 清单（版本、依赖）
├── Kconfig                 # 可选：menuconfig 开关
├── README.md               # 组件说明
├── include/                # 对外头文件
│   └── *.h
└── src/                    # 实现
    └── *.c
```

`bt_management`、`gui_task`、`app_health` 的源码仍位于仓库根目录 `bt_management/`、`main/gui/`、`main/app/`；组件 `CMakeLists.txt` 通过相对路径引用，避免重复拷贝。

---

## 与 managed_components 的区别

| 目录 | 来源 | 是否手改 |
|------|------|----------|
| `components/` | 仓库自研、可发布 | ✅ 维护源码 |
| `managed_components/` | `main/idf_component.yml` 由 IDF Component Manager 拉取 | ❌ 勿手改，改依赖后 `idf.py reconfigure` |

---

## 与 main/ 内联模块的区别

`wifi_management/` 与 `ui_runtime/` 存在双向依赖（Wi‑Fi 扫描 UI 与 runtime 网络逻辑），暂保留在 `main/` 中以相对路径编入；骨架工程通过 `frame_wifi` / `ui_shell` 引用同一源码树。

---

## 共享配置

两工程共用 [`config/sdkconfig.common.defaults`](../config/sdkconfig.common.defaults)（PSRAM、CPU、LVGL、RGB 三缓冲等），各工程 `CMakeLists.txt` 通过 `SDKCONFIG_DEFAULTS` 叠加本工程 `sdkconfig.defaults`。

共享 CMake：[`cmake/lvgl_psram_heap.cmake`](../cmake/lvgl_psram_heap.cmake)（强制链接 PSRAM 分配器）。

---

## 发布

板级组件发布流程见 [COMPONENT_PUBLISH_GUIDE.md](../COMPONENT_PUBLISH_GUIDE.md)。

---

## 骨架工程中的 components

`esp32_s3_frame/components/` 是框架层组件树（`app_core` / `frame_*` / vendor BSP），通过 `frame_*` 薄封装调用本目录 monorepo 组件。详见 [esp32_s3_frame/components/README.md](../esp32_s3_frame/components/README.md)。

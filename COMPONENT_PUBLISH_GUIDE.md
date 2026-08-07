# ESP-IDF 组件上传发布指南（withthewind2724）

本文档记录将本地组件发布到 GitHub 和 ESP Component Registry 的完整步骤，适用于本仓库当前组件：

- 组件目录：`components/withthewind_board_lvgl_init`
- GitHub 仓库：`https://github.com/withthewind2724/portworld`
- Registry 命名空间：`withthewind2724`

---

## 1. 前置条件

请确认以下工具可用：

- `git`
- `compote`（IDF Component Manager CLI）
- 已安装并可用的 ESP-IDF 环境

检查命令：

```powershell
git --version
compote --version
```

---

## 2. 组件目录结构要求

一个可发布的组件至少包含：

- `CMakeLists.txt`
- `idf_component.yml`
- `include/` 与 `src/`（或你的源码目录）

当前组件示例：

```text
components/withthewind_board_lvgl_init/
  ├─ CMakeLists.txt
  ├─ idf_component.yml
  ├─ include/withthewind_board_lvgl_init.h
  └─ src/withthewind_board_lvgl_init.c
```

---

## 3. 配置组件清单（idf_component.yml）

关键字段建议：

```yaml
version: "0.1.0"
description: "Board support: LVGL + ST7701 + GT911 without IO-expander BSP"
url: "https://github.com/withthewind2724/portworld"
repository: "https://github.com/withthewind2724/portworld"
license: "Apache-2.0"

dependencies:
  idf: ">=5.3"
  espressif/esp_lvgl_port:
    public: true
    version: ^2
  esp_lcd_panel_io_additions:
    version: ^1
  esp_lcd_st7701:
    version: "*"
  esp_lcd_touch_gt911:
    version: "*"
  lvgl/lvgl:
    version: ">=8,<10"
```

说明：

- `version` 每次发布新版本都要递增（例如 `0.1.0 -> 0.1.1`）。
- `public: true` 表示该依赖会透传给使用你组件的上层组件。
- `compote pack` 生成的 `dist/` 目录勿提交 Git（已加入 `.gitignore`）；上传直接用 `compote component upload`。

---

## 4. 推送代码到 GitHub

> 若目录还不是 git 仓库，先初始化。

### 4.1 初始化并提交

```powershell
cd E:\show\lvglframe
git init
git config user.name "withthewind2724"
git config user.email "2724601568@qq.com"

git add .
git commit -m "Add withthewind_board_lvgl_init component"
```

### 4.2 绑定远程并推送

```powershell
git branch -M main
git remote add origin https://github.com/withthewind2724/portworld.git
git push -u origin main
```

如果远程已存在：

```powershell
git remote set-url origin https://github.com/withthewind2724/portworld.git
```

如果提示 `main -> main (fetch first)`：

```powershell
git fetch origin
git pull --rebase origin main --allow-unrelated-histories
git push -u origin main
```

---

## 5. 登录 Registry 并上传组件

进入组件目录：

```powershell
cd E:\show\lvglframe\components\withthewind_board_lvgl_init
```

### 5.1 浏览器登录（推荐）

```powershell
compote registry login --profile withthewind --registry-url https://components.espressif.com --default-namespace withthewind2724
```

### 5.2 上传

```powershell
compote component upload --profile withthewind --name withthewind_board_lvgl_init --namespace withthewind2724 --version 0.1.0
```

上传成功后会输出 archive 上传信息和版本创建结果。

---

## 6. 使用 Token 上传（CI/自动化场景）

如果希望不走浏览器登录，可用环境变量：

```powershell
$env:IDF_COMPONENT_PROFILE="withthewind"
$env:IDF_COMPONENT_API_TOKEN="你的token"

compote component upload --profile withthewind --name withthewind_board_lvgl_init --namespace withthewind2724 --version 0.1.0
```

上传完成后建议清理环境变量：

```powershell
Remove-Item Env:IDF_COMPONENT_API_TOKEN -ErrorAction SilentlyContinue
Remove-Item Env:IDF_COMPONENT_PROFILE -ErrorAction SilentlyContinue
```

---

## 7. 常见问题与处理

### 7.1 `fatal: not a git repository`

当前目录未初始化 git：

```powershell
git init
```

### 7.2 `src refspec main does not match any`

本地还没有 commit。先 `git add` + `git commit` 再 push。

### 7.3 `gh` 命令不存在

`gh`（GitHub CLI）不是必需工具。可以直接用 `git` + `compote` 完成发布。

### 7.4 `HTTP 502` 推送失败

网络/网关临时问题，稍后重试；或切换 SSH remote。

### 7.5 `403 Forbidden`（Registry 上传）

token 无对应 namespace 上传权限，或登录账号不匹配。  
确认 token 对应账号是 `withthewind2724`，且 namespace 正确。

### 7.6 `Token not found`

profile 中没有可用 token。重新登录或通过 `IDF_COMPONENT_API_TOKEN` 注入。

### 7.7 `Profile "... not found"`

环境变量 `IDF_COMPONENT_PROFILE` 与命令参数冲突。先清理：

```powershell
Remove-Item Env:IDF_COMPONENT_PROFILE -ErrorAction SilentlyContinue
```

---

## 8. 发布新版本流程（推荐）

每次迭代建议按以下顺序：

1. 修改组件代码
2. 更新 `idf_component.yml` 的 `version`（例如 `0.1.0 -> 0.1.1`）
3. `git add/commit/push`
4. 执行 `compote component upload ... --version 0.1.1`
5. 在使用方项目中升级依赖版本

---

## 9. 使用方工程依赖写法

在使用方 `idf_component.yml` 中：

```yaml
dependencies:
  withthewind2724/withthewind_board_lvgl_init:
    version: "^0.1.0"
```

> 注意：CMake `REQUIRES` 的实际组件 target 名可能是带 namespace 前缀的形式（例如双下划线形式），请以本地解析结果为准。


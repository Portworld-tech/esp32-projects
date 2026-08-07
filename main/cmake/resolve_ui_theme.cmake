# Resolve UI_THEME_ID / UI_THEME_IS_HUB from main/app_ui_theme_select.h
#
# This file lives in main/cmake/, so CMAKE_CURRENT_LIST_DIR here is .../main/cmake
# (NOT .../main). Header is one level up.

get_filename_component(_UI_THEME_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_UI_THEME_SELECT_H "${_UI_THEME_MAIN_DIR}/app_ui_theme_select.h")

if(NOT EXISTS "${_UI_THEME_SELECT_H}")
    message(FATAL_ERROR "Missing UI theme select header: ${_UI_THEME_SELECT_H}")
endif()

# Do NOT use set_property(DIRECTORY ... CMAKE_CONFIGURE_DEPENDS) here:
# ESP-IDF runs this file during early component_get_requirements, before any
# directory is registered. Changing the header still needs: idf.py reconfigure

set(UI_THEME_ID "")
file(STRINGS "${_UI_THEME_SELECT_H}" _ui_theme_lines)
foreach(_line IN LISTS _ui_theme_lines)
    # Active line: #define APP_UI_THEME_ID  APP_UI_THEME_SLATE
    # Ignore enum-style #define APP_UI_THEME_SLATE 1
    if(_line MATCHES "^[ \t]*#define[ \t]+APP_UI_THEME_ID[ \t]+APP_UI_THEME_([A-Za-z0-9_]+)")
        string(TOLOWER "${CMAKE_MATCH_1}" UI_THEME_ID)
        break()
    endif()
endforeach()

if(UI_THEME_ID STREQUAL "")
    message(WARNING "APP_UI_THEME_ID not found in app_ui_theme_select.h — using default")
    set(UI_THEME_ID "default")
endif()

set(_UI_THEME_VALID
    default slate sand ink forest dusk ocean zen pulse bloom metro)
list(FIND _UI_THEME_VALID "${UI_THEME_ID}" _ui_theme_idx)
if(_ui_theme_idx LESS 0)
    message(FATAL_ERROR "Unknown APP_UI_THEME_ID → '${UI_THEME_ID}'. Valid: ${_UI_THEME_VALID}")
endif()

if(UI_THEME_ID STREQUAL "default")
    set(UI_THEME_IS_HUB 0)
else()
    set(UI_THEME_IS_HUB 1)
endif()

# Export for parent CMakeLists (main/)
set(UI_THEME_MAIN_DIR "${_UI_THEME_MAIN_DIR}")

message(STATUS "UI theme pack (from app_ui_theme_select.h): ${UI_THEME_ID}")

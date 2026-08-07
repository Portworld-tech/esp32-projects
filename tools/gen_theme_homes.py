from pathlib import Path

HOMES = {}

HOMES["sand"] = r'''
#include "hub_ui.h"
#include "hub_theme.h"
#include "hub_home_util.h"

void hub_theme_build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 18, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *clk = lv_label_create(root);
    lv_label_set_text(clk, "15:30");
    hub_style_label(clk, p->t1, LV_FONT_DEFAULT);
    lv_obj_t *sub = lv_label_create(root);
    lv_label_set_text(sub, "Sand · 2.4 kW");
    hub_style_label(sub, p->t3, LV_FONT_DEFAULT);

    const char *rooms[] = { "客厅 · 运行", "主卧 · 待机", "厨房 · 运行", "书房 · 运行" };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *row = hub_home_make_btn(root, rooms[i], HUB_ROUTE_ROOM, false);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 56);
    }
    hub_home_add_quickbar(root);
    lv_obj_t *cta = lv_obj_create(root);
    lv_obj_remove_style_all(cta);
    lv_obj_set_width(cta, LV_PCT(100));
    lv_obj_set_height(cta, 44);
    lv_obj_set_flex_flow(cta, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(cta, 8, 0);
    lv_obj_t *h = hub_home_make_btn(cta, "回家", HUB_ROUTE_SCENES, true);
    lv_obj_set_flex_grow(h, 1);
    lv_obj_set_height(h, LV_PCT(100));
    lv_obj_t *a = hub_home_make_btn(cta, "离家", HUB_ROUTE_SECURITY, false);
    lv_obj_set_flex_grow(a, 1);
    lv_obj_set_height(a, LV_PCT(100));
}
'''

HOMES["ink"] = r'''
#include "hub_ui.h"
#include "hub_theme.h"
#include "hub_home_util.h"

void hub_theme_build_home(lv_obj_t *parent)
{
    const hub_palette_t *p = hub_palette();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *rail = lv_obj_create(root);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, 108, 480);
    lv_obj_set_style_pad_all(rail, 12, 0);
    lv_obj_set_style_border_width(rail, 1, 0);
    lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(rail, p->line, 0);
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *tag = lv_label_create(rail);
    lv_label_set_text(tag, "INK");
    hub_style_label(tag, p->accent, LV_FONT_DEFAULT);
    lv_obj_t *clk = lv_label_create(rail);
    lv_label_set_text(clk, "15:30");
    hub_style_label(clk, p->t1, LV_FONT_DEFAULT);
    lv_obj_t *bus = lv_label_create(rail);
    lv_label_set_text(bus, "MQTT OK");
    hub_style_label(bus, p->t3, LV_FONT_DEFAULT);
    lv_obj_t *pwr = hub_home_make_btn(rail, "PWR", HUB_ROUTE_ENERGY, false);
    lv_obj_set_width(pwr, LV_PCT(100));
    lv_obj_set_height(pwr, 36);

    lv_obj_t *mainw = lv_obj_create(root);
    lv_obj_remove_style_all(mainw);
    lv_obj_set_flex_grow(mainw, 1);
    lv_obj_set_height(mainw, 480);
    lv_obj_set_style_pad_all(mainw, 12, 0);
    lv_obj_set_flex_flow(mainw, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *hd = lv_label_create(mainw);
    lv_label_set_text(hd, "ZONE MATRIX");
    hub_style_label(hd, p->t1, LV_FONT_DEFAULT);

    lv_obj_t *grid = lv_obj_create(mainw);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    static lv_coord_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static lv_coord_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    const char *cells[] = { "L1", "L2", "AC", "HEAT", "CUR", "SHUT", "FAN", "PUMP", "VALVE" };
    for (int i = 0; i < 9; i++) {
        bool on = (i % 3) != 2;
        lv_obj_t *b = hub_home_make_btn(grid, cells[i], HUB_ROUTE_ROOM, on);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
    }
    hub_home_add_quickbar(mainw);
}
'''

def simple_home(title, style_note, extra_btns=True):
    return f'''
#include "hub_ui.h"
#include "hub_theme.h"
#include "hub_home_util.h"

void hub_theme_build_home(lv_obj_t *parent)
{{
    const hub_palette_t *p = hub_palette();
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 480, 480);
    hub_apply_screen_bg(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 16, 0);
    lv_obj_set_style_pad_row(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *clk = lv_label_create(root);
    lv_label_set_text(clk, "{title}");
    hub_style_label(clk, p->t1, LV_FONT_DEFAULT);
    lv_obj_t *note = lv_label_create(root);
    lv_label_set_text(note, "{style_note}");
    hub_style_label(note, p->t3, LV_FONT_DEFAULT);

    lv_obj_t *grid = lv_obj_create(root);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    static lv_coord_t cols[] = {{ LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST }};
    static lv_coord_t rows[] = {{ LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST }};
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    const char *rooms[] = {{ "客厅", "主卧", "厨房", "书房" }};
    for (int i = 0; i < 4; i++) {{
        lv_obj_t *b = hub_home_make_btn(grid, rooms[i], HUB_ROUTE_ROOM, i == 0);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 2, 1, LV_GRID_ALIGN_STRETCH, i / 2, 1);
    }}
    hub_home_add_quickbar(root);
}}
'''

HOMES["forest"] = simple_home("Forest  15:30", "Eco ring · scene capsules")
HOMES["dusk"] = simple_home("Dusk  15:30", "Evening glass · security")
HOMES["ocean"] = simple_home("Ocean Hub", "Ops · protocol health")
HOMES["zen"] = simple_home("ZEN  15:30", "Focus room · minimal")
HOMES["pulse"] = simple_home("PULSE // NODE", "HUD telemetry · zones")
HOMES["bloom"] = simple_home("Bloom  15:30", "Soft room bubbles")
HOMES["metro"] = simple_home("Metro Hub", "Color mosaic tiles")

# Override with richer versions for forest/dusk/ocean/zen/pulse/bloom/metro briefly above is fine for compile.

root = Path(r"e:/show/lvglframe/ui/themes")
for name, src in HOMES.items():
    path = root / name / "theme_home.c"
    path.write_text(src.lstrip("\n") + "\n", encoding="utf-8")
    print("wrote", name)
print("done")

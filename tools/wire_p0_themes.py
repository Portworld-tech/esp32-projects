# -*- coding: utf-8 -*-
from pathlib import Path

root = Path(r"e:/show/lvglframe/ui/themes")
themes = ["slate", "sand", "ink", "forest", "dusk", "ocean", "zen", "pulse", "bloom", "metro"]

room_src = """/** {name} - room page (AdaptiveGrid via hub_room_ui) */
#include \"theme_local.h\"
#include \"hub_room_ui.h\"

void build_room(lv_obj_t *parent)
{{
    hub_build_room_page(parent);
}}
"""

boot_tpl = """/** {name} - router + app_ui_start */
#include \"theme_local.h\"
#include \"hub_ui.h\"
#include \"hub_room_ui.h\"
#include \"app_ui.h\"

void hub_theme_build(lv_obj_t *parent, hub_route_t route)
{{
    switch (route) {{
    case HUB_ROUTE_HOME: build_home(parent); break;
    case HUB_ROUTE_ROOM: hub_build_room_page(parent); break;
    case HUB_ROUTE_ROOM_EDIT: hub_build_room_edit(parent); break;
    case HUB_ROUTE_APPLIANCES: hub_build_appliances(parent); break;
    case HUB_ROUTE_SCENES: build_scenes(parent); break;
    case HUB_ROUTE_ENERGY: build_energy(parent); break;
    case HUB_ROUTE_GATEWAY: build_gateway(parent); break;
    case HUB_ROUTE_SETTINGS: build_settings(parent); break;
    case HUB_ROUTE_NETWORK: build_network(parent); break;
    case HUB_ROUTE_SECURITY: hub_build_security(parent); break;
    case HUB_ROUTE_SCHEDULE: build_schedule(parent); break;
    case HUB_ROUTE_HVAC: build_hvac(parent); break;
    case HUB_ROUTE_POINTS: build_points(parent); break;
    default: build_home(parent); break;
    }}
}}

void app_ui_start(void)
{{
    hub_ui_init();
}}
"""

zone_old = """void zone_cb(lv_event_t *e)
{
    int cell = (int)(uintptr_t)lv_event_get_user_data(e);
    hub_model_toggle_zone(cell);
    hub_ui_refresh();
}"""

zone_new = """void zone_cb(lv_event_t *e)
{
    int cell = (int)(uintptr_t)lv_event_get_user_data(e);
    /* Ink PUMP/VALVE cells open appliances sheet (FE parity) */
    if (cell == 7 || cell == 8) {
        hub_ui_go(HUB_ROUTE_APPLIANCES);
        return;
    }
    hub_model_toggle_zone(cell);
    hub_ui_refresh();
}"""

for t in themes:
    d = root / t
    name = t.capitalize()
    (d / "pages_room.c").write_text(room_src.format(name=name), encoding="utf-8")
    (d / "boot.c").write_text(boot_tpl.format(name=name), encoding="utf-8")
    tl = d / "theme_local.c"
    txt = tl.read_text(encoding="utf-8")
    if zone_old in txt:
        tl.write_text(txt.replace(zone_old, zone_new), encoding="utf-8")
        print(t, "zone_cb updated")
    elif "HUB_ROUTE_APPLIANCES" in txt:
        print(t, "zone_cb already wired")
    else:
        print(t, "WARN zone_cb pattern missing")

    life = d / "pages_life.c"
    lt = life.read_text(encoding="utf-8")
    if "HUB_ROUTE_APPLIANCES" not in lt:
        needle = '{ HUB_ICO_HOME, "房间", "控件布局", HUB_ROUTE_ROOM },'
        insert = (
            needle
            + '\n        { HUB_ICO_POWER, "设备", "水泵 / 水阀 / 插座", HUB_ROUTE_APPLIANCES },'
        )
        if needle in lt:
            life.write_text(lt.replace(needle, insert, 1), encoding="utf-8")
            print(t, "settings appliances link")
        else:
            print(t, "WARN settings row missing")
    print(t, "pages_room+boot ok")

print("done")

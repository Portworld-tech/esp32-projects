# -*- coding: utf-8 -*-
"""Wire all hub themes to shared scenes/energy/gateway/points; thin local wrappers."""
from pathlib import Path

root = Path(r"e:/show/lvglframe/ui/themes")
themes = ["slate", "sand", "ink", "forest", "dusk", "ocean", "zen", "pulse", "bloom", "metro"]

boot_tpl = """/** {name} - router + app_ui_start */
#include \"theme_local.h\"
#include \"hub_ui.h\"
#include \"hub_room_ui.h\"
#include \"hub_device_ui.h\"
#include \"hub_pages_common.h\"
#include \"app_ui.h\"

void hub_theme_build(lv_obj_t *parent, hub_route_t route)
{{
    switch (route) {{
    case HUB_ROUTE_HOME: build_home(parent); break;
    case HUB_ROUTE_ROOM: hub_build_room_page(parent); break;
    case HUB_ROUTE_ROOM_EDIT: hub_build_room_edit(parent); break;
    case HUB_ROUTE_APPLIANCES: hub_build_appliances(parent); break;
    case HUB_ROUTE_SCENES: hub_build_scenes(parent); break;
    case HUB_ROUTE_ENERGY: hub_build_energy(parent); break;
    case HUB_ROUTE_GATEWAY: hub_build_gateway(parent); break;
    case HUB_ROUTE_SETTINGS: build_settings(parent); break;
    case HUB_ROUTE_NETWORK: hub_build_network(parent); break;
    case HUB_ROUTE_SECURITY: hub_build_security(parent); break;
    case HUB_ROUTE_SCHEDULE: build_schedule(parent); break;
    case HUB_ROUTE_HVAC: build_hvac(parent); break;
    case HUB_ROUTE_POINTS: hub_build_points(parent); break;
    default: build_home(parent); break;
    }}
}}

void app_ui_start(void)
{{
    hub_ui_init();
}}
"""

scenes_tpl = """/** {name} - scenes (shared HubScenesPage) */
#include \"theme_local.h\"
#include \"hub_pages_common.h\"

void build_scenes(lv_obj_t *parent)
{{
    hub_build_scenes(parent);
}}
"""

ops_tpl = """/** {name} - energy/gateway/network/points via shared FE pages */
#include \"theme_local.h\"
#include \"hub_pages_common.h\"
#include \"hub_device_ui.h\"

void build_energy(lv_obj_t *parent)
{{
    hub_build_energy(parent);
}}

void build_gateway(lv_obj_t *parent)
{{
    hub_build_gateway(parent);
}}

void build_network(lv_obj_t *parent)
{{
    hub_build_network(parent);
}}

void build_points(lv_obj_t *parent)
{{
    hub_build_points(parent);
}}
"""

for t in themes:
    d = root / t
    name = t.capitalize()
    (d / "boot.c").write_text(boot_tpl.format(name=name), encoding="utf-8")
    (d / "pages_scenes.c").write_text(scenes_tpl.format(name=name), encoding="utf-8")
    (d / "pages_ops.c").write_text(ops_tpl.format(name=name), encoding="utf-8")
    print(t, "boot+scenes+ops wired")

print("done")

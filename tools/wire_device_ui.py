# -*- coding: utf-8 -*-
"""Wire network+brightness into all hub themes; fix settings loop."""
from pathlib import Path
import re

root = Path(r"e:/show/lvglframe/ui/themes")
themes = ["slate", "sand", "ink", "forest", "dusk", "ocean", "zen", "pulse", "bloom", "metro"]

boot_tpl = """/** {name} - router + app_ui_start */
#include \"theme_local.h\"
#include \"hub_ui.h\"
#include \"hub_room_ui.h\"
#include \"hub_device_ui.h\"
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
    case HUB_ROUTE_NETWORK: hub_build_network(parent); break;
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

for t in themes:
    d = root / t
    name = t.capitalize()
    (d / "boot.c").write_text(boot_tpl.format(name=name), encoding="utf-8")

    life = d / "pages_life.c"
    txt = life.read_text(encoding="utf-8")

    # Fix settings loop: use sizeof
    txt2 = re.sub(
        r"for \(int i = 0; i < 7; i\+\+\)",
        "for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++)",
        txt,
        count=1,
    )
    if txt2 == txt:
        txt2 = re.sub(
            r"for \(int i = 0; i < 8; i\+\+\)",
            "for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++)",
            txt,
            count=1,
        )

    # Ensure include hub_device_ui.h
    if '#include "hub_device_ui.h"' not in txt2:
        txt2 = txt2.replace(
            '#include "app_ui.h"',
            '#include "app_ui.h"\n#include "hub_device_ui.h"',
            1,
        )

    # Append brightness after settings rows loop closing brace of build_settings
    if "hub_build_brightness_block" not in txt2:
        # Find end of build_settings: after the for-loop's closing before next function
        m = re.search(
            r"(void build_settings\(lv_obj_t \*parent\)\s*\{.*?"
            r"for \(int i = 0; i < \(int\)\(sizeof\(rows\) / sizeof\(rows\[0\]\)\); i\+\+\) \{.*?\n    \})",
            txt2,
            flags=re.S,
        )
        if m:
            insert_at = m.end()
            txt2 = (
                txt2[:insert_at]
                + "\n    hub_build_brightness_block(body);\n"
                + txt2[insert_at:]
            )
            print(t, "brightness appended")
        else:
            print(t, "WARN could not find settings loop end")

    # Use themed cards in settings if only hub_apply_card
    # already has theme_card_chrome typically

    life.write_text(txt2, encoding="utf-8")
    print(t, "boot+settings ok")

print("done")

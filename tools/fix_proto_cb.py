# -*- coding: utf-8 -*-
from pathlib import Path

old = """void proto_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    hub_model_toggle_proto(id);
    hub_ui_refresh();
}"""

new = """void proto_cb(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    if (id && strcmp(id, \"wifi\") == 0) {
        hub_ui_go(HUB_ROUTE_NETWORK);
        return;
    }
    hub_model_toggle_proto(id);
    hub_ui_refresh();
}"""

root = Path(r"e:/show/lvglframe/ui/themes")
for t in ["slate", "sand", "ink", "forest", "dusk", "ocean", "zen", "pulse", "bloom", "metro"]:
    p = root / t / "theme_local.c"
    txt = p.read_text(encoding="utf-8")
    if old in txt:
        p.write_text(txt.replace(old, new), encoding="utf-8")
        print(t, "proto_cb ok")
    else:
        print(t, "WARN / skip")

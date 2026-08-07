get_filename_component(BT_MGMT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../bt_management" ABSOLUTE)

set(_bt_mgmt_srcs
    "${BT_MGMT_ROOT}/bt_management.c"
    "${BT_MGMT_ROOT}/bt_proto.c"
    "${BT_MGMT_ROOT}/bt_gatt_star.c"
    "${BT_MGMT_ROOT}/bt_transport_ble.c"
    "${BT_MGMT_ROOT}/bt_transport_spp.c"
    "${BT_MGMT_ROOT}/bt_mesh_node.c"
)

idf_component_register(
    SRCS ${_bt_mgmt_srcs}
    INCLUDE_DIRS "${BT_MGMT_ROOT}"
    REQUIRES bt nvs_flash
)

if(CONFIG_BLE_MESH OR CONFIG_FRAME_ENABLE_MESH)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${IDF_PATH}/components/bt/esp_ble_mesh/api/models/include"
    )
endif()

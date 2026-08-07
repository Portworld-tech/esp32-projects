/**
 * 设备 UI 状态本地缓存（解决退出小程序或切换页面后开关/温度被重置、
 * 以及 setData 引用复用导致的多个开关互相覆盖问题）
 */

const REMOTE_KEY = 'remote_device_state_v1';
const BLE_UI_KEY = 'ble_near_mesh_ui_v1';

function now() {
  return Date.now();
}

/** 远程页：与 uid + statusTopic 绑定，避免换主题串数据 */
function saveRemoteSnapshot(payload) {
  try {
    wx.setStorageSync(REMOTE_KEY, { ...payload, savedAt: now() });
  } catch (e) {
    console.warn('saveRemoteSnapshot', e);
  }
}

function loadRemoteSnapshot(expectedUid, expectedStatusTopic) {
  try {
    const s = wx.getStorageSync(REMOTE_KEY);
    if (!s || s.uid !== expectedUid || s.statusTopic !== expectedStatusTopic) {
      return null;
    }
    return s;
  } catch (e) {
    return null;
  }
}

/** BLE：按 deviceId 存近场开关 + 配网开关，合并写入不冲掉未传的字段 */
function saveBleUiSnapshot(deviceId, patch) {
  if (!deviceId) return;
  try {
    const all = wx.getStorageSync(BLE_UI_KEY) || {};
    const prev = all[deviceId] || {};
    const switchPrev = prev.switchStates || { 1: false, 2: false, 3: false };
    const switchPatch = patch.switchStates || {};
    const next = {
      ...prev,
      ...patch,
      switchStates: {
        1: switchPatch[1] !== undefined ? !!switchPatch[1] : !!switchPrev[1],
        2: switchPatch[2] !== undefined ? !!switchPatch[2] : !!switchPrev[2],
        3: switchPatch[3] !== undefined ? !!switchPatch[3] : !!switchPrev[3],
      },
      savedAt: now(),
    };
    all[deviceId] = next;
    wx.setStorageSync(BLE_UI_KEY, all);
  } catch (e) {
    console.warn('saveBleUiSnapshot', e);
  }
}

function loadBleUiSnapshot(deviceId) {
  if (!deviceId) return null;
  try {
    const all = wx.getStorageSync(BLE_UI_KEY) || {};
    return all[deviceId] || null;
  } catch (e) {
    return null;
  }
}

function clearRemoteSnapshot() {
  try {
    wx.removeStorageSync(REMOTE_KEY);
  } catch (e) { /* ignore */ }
}

module.exports = {
  saveRemoteSnapshot,
  loadRemoteSnapshot,
  clearRemoteSnapshot,
  saveBleUiSnapshot,
  loadBleUiSnapshot,
};

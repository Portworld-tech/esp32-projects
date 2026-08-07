const btctrl = require('../../utils/btctrl.js');
const stateStore = require('../../utils/device_state_store.js');
const homePrefs = require('../../utils/home_prefs.js');

Page({
  data: {
    gatewayName: '',
    connected: false,
    syncHint: '',
    switchStates: {
      1: false,
      2: false,
      3: false,
    },
    lastAck: '',
    meshProvEnabled: false,
    _seq: 1,
    _gateway: null,
    alias1: '客厅空调',
    alias2: '客厅采暖',
    alias3: '卧室地暖',
  },

  persistBleUiFromData() {
    const gw = this.data._gateway;
    if (!gw || !gw.deviceId) return;
    stateStore.saveBleUiSnapshot(gw.deviceId, {
      switchStates: {
        1: !!this.data.switchStates[1],
        2: !!this.data.switchStates[2],
        3: !!this.data.switchStates[3],
      },
      meshProvEnabled: !!this.data.meshProvEnabled,
    });
  },

  hydrateBleFromCache() {
    const gw = this.data._gateway;
    if (!gw || !gw.deviceId) return;
    const snap = stateStore.loadBleUiSnapshot(gw.deviceId);
    if (!snap || !snap.switchStates) return;
    this.setData({
      switchStates: {
        1: !!snap.switchStates[1],
        2: !!snap.switchStates[2],
        3: !!snap.switchStates[3],
      },
      meshProvEnabled: snap.meshProvEnabled !== undefined ? !!snap.meshProvEnabled : this.data.meshProvEnabled,
      syncHint: snap.savedAt ? `已加载本地缓存（${new Date(snap.savedAt).toLocaleString()}）` : '',
    });
  },

  onLoad() {
    const p = homePrefs.loadPrefs();
    this.setData({
      alias1: p.aliases[1],
      alias2: p.aliases[2],
      alias3: p.aliases[3],
    });
    const gw = wx.getStorageSync('ble_gateway');
    if (gw) {
      this.setData({
        gatewayName: gw.name || gw.deviceId || '未知设备',
        _gateway: gw,
      });
      this.hydrateBleFromCache();
    } else {
      wx.showToast({ title: '请先在设备发现页连接网关', icon: 'none' });
    }
  },

  onShow() {
    const gw = this.data._gateway;
    if (!gw) return;

    this.hydrateBleFromCache();

    if (!this._bleListenersRegistered) {
      this._bleListenersRegistered = true;

      this._onBLEConn = (res) => {
        if (!res || res.deviceId !== gw.deviceId) return;
        this.setData({ connected: res.connected });
      };

      this._onBLEChar = (res) => {
        if (!res || res.deviceId !== gw.deviceId) return;
        if (res.characteristicId !== gw.notifyCharId) return;

        const value = res.value;
        if (!value) return;
        const bytes = new Uint8Array(value);
        const ack = btctrl.parseAckStatus(bytes);
        if (!ack) return;

        const ok = ack.status === 0;
        const msg = ok ? `ACK ok (seq=${ack.seq})` : `ACK failed (seq=${ack.seq})`;
        this.setData({ lastAck: msg, syncHint: ok ? '已与设备对齐（ACK）' : '' });

        if (!ok && this.data._pending) {
          const { itemId, desiredOn } = this.data._pending;
          const next = {
            1: !!this.data.switchStates[1],
            2: !!this.data.switchStates[2],
            3: !!this.data.switchStates[3],
            [itemId]: !desiredOn,
          };
          this.setData({ switchStates: next });
          this.persistBleUiFromData();
        }
        if (this.data._pending) this.data._pending = null;

        if (ok) {
          this.persistBleUiFromData();
        }
      };

      wx.onBLEConnectionStateChange(this._onBLEConn);
      wx.onBLECharacteristicValueChange(this._onBLEChar);
    }

    wx.notifyBLECharacteristicValueChange({
      state: true,
      deviceId: gw.deviceId,
      serviceId: gw.serviceId,
      characteristicId: gw.notifyCharId,
      success: () => {},
      fail: (err) => {
        console.warn('notify failed', err);
      },
    });

    wx.createBLEConnection({
      deviceId: gw.deviceId,
      success: () => {
        this.setData({ connected: true });
      },
      fail: () => {},
    });
  },

  onHide() {
    this.persistBleUiFromData();
  },

  onUnload() {
    this.persistBleUiFromData();
    if (this._bleListenersRegistered) {
      if (this._onBLEConn) wx.offBLEConnectionStateChange(this._onBLEConn);
      if (this._onBLEChar) wx.offBLECharacteristicValueChange(this._onBLEChar);
      this._bleListenersRegistered = false;
      this._onBLEConn = null;
      this._onBLEChar = null;
    }
  },

  toggleSwitch(e) {
    const itemId = Number(e.currentTarget.dataset.item);
    const curOn = !!this.data.switchStates[itemId];
    const desiredOn = !curOn;

    this.data._pending = { itemId, desiredOn };
    const next = {
      1: !!this.data.switchStates[1],
      2: !!this.data.switchStates[2],
      3: !!this.data.switchStates[3],
      [itemId]: desiredOn,
    };
    this.setData({ switchStates: next });
    this.persistBleUiFromData();

    const gw = this.data._gateway;
    if (!gw) return;

    const value = desiredOn ? 1 : 0;
    const bytes = new Uint8Array([itemId & 0xff, value & 0xff]);
    const buf = bytes.buffer;

    wx.writeBLECharacteristicValue({
      deviceId: gw.deviceId,
      serviceId: gw.serviceId,
      characteristicId: gw.writeCharId,
      value: buf,
      success: () => {
        wx.showToast({ title: `已发送 item=${itemId}`, icon: 'none' });
      },
      fail: (err) => {
        console.error(err);
        const rollback = {
          1: !!this.data.switchStates[1],
          2: !!this.data.switchStates[2],
          3: !!this.data.switchStates[3],
          [itemId]: !desiredOn,
        };
        this.data._pending = null;
        this.setData({ switchStates: rollback });
        this.persistBleUiFromData();
        wx.showToast({ title: '写入失败（可能未配对/未加密）', icon: 'none', duration: 2000 });
      },
    });
  },

  toggleMeshProv() {
    const gw = this.data._gateway;
    if (!gw) {
      wx.showToast({ title: '请先在设备发现页连接网关', icon: 'none' });
      return;
    }

    const enable = !this.data.meshProvEnabled;
    const prevMesh = !!this.data.meshProvEnabled;
    this.setData({ meshProvEnabled: enable, lastAck: '发送中...' });
    this.persistBleUiFromData();

    const seq = (this.data._seq = (this.data._seq + 1) & 0xffff);
    const frame = btctrl.buildSetStateFrame(10, enable ? 1 : 0, seq);
    const buf = frame.buffer.slice(frame.byteOffset, frame.byteOffset + frame.byteLength);

    wx.writeBLECharacteristicValue({
      deviceId: gw.deviceId,
      serviceId: gw.serviceId,
      characteristicId: gw.writeCharId,
      value: buf,
      success: () => {
        wx.showToast({ title: enable ? '开始配网广播' : '停止配网广播', icon: 'none' });
      },
      fail: (err) => {
        console.error(err);
        this.setData({ meshProvEnabled: prevMesh });
        this.persistBleUiFromData();
        wx.showToast({ title: '写入失败（可能未配对/未加密）', icon: 'none', duration: 2000 });
      },
    });
  },
});

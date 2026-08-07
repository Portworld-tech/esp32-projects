const btctrl = require('../../utils/btctrl.js');
const stateStore = require('../../utils/device_state_store.js');

Page({
  data: {
    gatewayName: '',
    meshProvEnabled: false,
    lastAck: '',
    syncHint: '',
    _gateway: null,
    _seq: 1,
    bleReady: false,
  },

  persistMeshUiFromData() {
    const gw = this.data._gateway;
    if (!gw || !gw.deviceId) return;
    stateStore.saveBleUiSnapshot(gw.deviceId, {
      meshProvEnabled: !!this.data.meshProvEnabled,
    });
  },

  hydrateMeshFromCache() {
    const gw = this.data._gateway;
    if (!gw || !gw.deviceId) return;
    const snap = stateStore.loadBleUiSnapshot(gw.deviceId);
    if (!snap || snap.meshProvEnabled === undefined) return;
    this.setData({
      meshProvEnabled: !!snap.meshProvEnabled,
      syncHint: snap.savedAt ? `已加载本地缓存` : '',
    });
  },

  onLoad() {
    const gw = wx.getStorageSync('ble_gateway');
    if (!gw) {
      wx.showToast({ title: '请先在设备发现页连接网关', icon: 'none' });
      return;
    }
    this.setData({
      gatewayName: gw.name || gw.deviceId || '未知设备',
      _gateway: gw,
    });
    this.hydrateMeshFromCache();
  },

  onShow() {
    const gw = this.data._gateway;
    if (!gw) return;

    this.hydrateMeshFromCache();

    if (!this._meshBleListenerRegistered) {
      this._meshBleListenerRegistered = true;
      this._onMeshChar = (res) => {
        if (!res || res.deviceId !== gw.deviceId) return;
        if (res.characteristicId !== gw.notifyCharId) return;
        const bytes = new Uint8Array(res.value);
        const ack = btctrl.parseAckStatus(bytes);
        if (!ack) return;
        const ok = ack.status === 0;
        this.setData({
          lastAck: ok ? `ACK ok (seq=${ack.seq})` : `ACK fail (seq=${ack.seq})`,
          syncHint: ok ? '网关已应答' : '',
        });
        if (ok) {
          this.persistMeshUiFromData();
        }
      };
      wx.onBLECharacteristicValueChange(this._onMeshChar);
    }

    this.ensureBleReady(gw);
  },

  onHide() {
    this.persistMeshUiFromData();
  },

  onUnload() {
    this.persistMeshUiFromData();
    if (this._meshBleListenerRegistered && this._onMeshChar) {
      wx.offBLECharacteristicValueChange(this._onMeshChar);
      this._meshBleListenerRegistered = false;
      this._onMeshChar = null;
    }
  },

  ensureBleReady(gw) {
    wx.openBluetoothAdapter({
      success: () => {
        wx.createBLEConnection({
          deviceId: gw.deviceId,
          timeout: 15000,
          success: () => {
            wx.notifyBLECharacteristicValueChange({
              state: true,
              deviceId: gw.deviceId,
              serviceId: gw.serviceId,
              characteristicId: gw.notifyCharId,
              success: () => {
                this.setData({ bleReady: true, lastAck: 'BLE已就绪，等待ACK...' });
              },
              fail: (err) => {
                this.setData({ bleReady: false, lastAck: `notify失败: ${err && (err.errMsg || err.errCode)}` });
                console.warn('notify failed', err);
              },
            });
          },
          fail: (err) => {
            this.setData({ bleReady: false, lastAck: `连接失败: ${err && (err.errMsg || err.errCode)}` });
          },
        });
      },
      fail: (err) => {
        this.setData({ bleReady: false, lastAck: `蓝牙未初始化: ${err && (err.errMsg || err.errCode)}` });
      },
    });
  },

  toggleMeshProv() {
    const gw = this.data._gateway;
    if (!gw) {
      wx.showToast({ title: '请先在设备发现页连接网关', icon: 'none' });
      return;
    }
    if (!this.data.bleReady) {
      wx.showToast({ title: 'BLE未就绪，请稍后再试', icon: 'none' });
      this.ensureBleReady(gw);
      return;
    }

    const enable = !this.data.meshProvEnabled;
    const prev = !!this.data.meshProvEnabled;
    const seq = (this.data._seq = (this.data._seq + 1) & 0xffff);
    const frame = btctrl.buildSetStateFrame(10, enable ? 1 : 0, seq);
    const buf = frame.buffer.slice(frame.byteOffset, frame.byteOffset + frame.byteLength);

    this.setData({ meshProvEnabled: enable, lastAck: '发送中...' });
    this.persistMeshUiFromData();

    wx.showLoading({ title: '发送中...', mask: true });
    wx.writeBLECharacteristicValue({
      deviceId: gw.deviceId,
      serviceId: gw.serviceId,
      characteristicId: gw.writeCharId,
      value: buf,
      success: () => {
        wx.hideLoading();
        this.setData({ lastAck: '发送完成，等待 ACK...' });
        wx.showToast({ title: enable ? '开始配网广播' : '停止配网广播', icon: 'success', duration: 1500 });
      },
      fail: (err) => {
        wx.hideLoading();
        console.error(err);
        this.setData({ meshProvEnabled: prev });
        this.persistMeshUiFromData();
        wx.showToast({ title: '写入失败（可能未配对/未加密）', icon: 'none', duration: 2000 });
      },
    });
  },
});

const stateStore = require('../../utils/device_state_store.js');

Page({
  data: {
    hasGateway: false,
    gatewayName: '',
    remoteReady: false,
    homeSummary: '等待设备同步',
    switchOnCount: 0,
    tempScreen3: '--',
    tempScreen5: '--',
    lastSyncText: '',
  },

  onLoad() {
    this.refreshDashboard();
  },

  onShow() {
    this.refreshDashboard();
  },

  refreshDashboard() {
    const gw = wx.getStorageSync('ble_gateway');
    const cfg = wx.getStorageSync('cloud_cfg_override') || {};
    const uid = cfg.uid || '1ab39688601b47beb814e4c1bf001173';
    const statusTopic = cfg.statusTopic || '4BE3K6ebb005';
    const snap = stateStore.loadRemoteSnapshot(uid, statusTopic);

    const hasGateway = !!(gw && gw.deviceId);
    const remoteReady = !!snap;
    let switchOnCount = 0;
    let homeSummary = '等待设备同步';
    let tempScreen3 = '--';
    let tempScreen5 = '--';
    let lastSyncText = '';

    if (snap && snap.switchStates) {
      switchOnCount =
        (snap.switchStates[1] ? 1 : 0) +
        (snap.switchStates[2] ? 1 : 0) +
        (snap.switchStates[3] ? 1 : 0);
      tempScreen3 = `${snap.tempScreen3}`;
      tempScreen5 = `${snap.tempScreen5}`;
      homeSummary = `开关已开启 ${switchOnCount}/3`;
      if (snap.savedAt) {
        const d = new Date(snap.savedAt);
        const hh = `${d.getHours()}`.padStart(2, '0');
        const mm = `${d.getMinutes()}`.padStart(2, '0');
        const ss = `${d.getSeconds()}`.padStart(2, '0');
        lastSyncText = `${hh}:${mm}:${ss}`;
      }
    }

    this.setData({
      hasGateway,
      gatewayName: hasGateway ? (gw.name || gw.deviceId) : '',
      remoteReady,
      homeSummary,
      switchOnCount,
      tempScreen3,
      tempScreen5,
      lastSyncText,
    });
  },

  goPage(e) {
    const url = e.currentTarget.dataset.url;
    if (!url) return;
    wx.navigateTo({ url });
  },
});

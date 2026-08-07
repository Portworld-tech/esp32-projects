const BLE_CTRL_UUID_SVC = '9e6b0001-5c3a-4e2b-a1e5-1234567890ab';
const BLE_CTRL_UUID_RX = '9e6b0002-5c3a-4e2b-a1e5-1234567890ab';
const BLE_CTRL_UUID_TX = '9e6b0003-5c3a-4e2b-a1e5-1234567890ab';

function normalizeUuid(u) {
  return (u || '').toLowerCase();
}

Page({
  data: {
    scanning: false,
    devices: [],
    btReady: false,
    onlyShowGateway: true,
    scanHint: '未开始扫描',
    currentGateway: null,
  },

  onLoad: function () {
    this.ensureBluetoothAdapter();
    this.refreshCurrentGateway();
  },

  onShow: function () {
    this.refreshCurrentGateway();
  },

  onUnload: function () {
    this.stopScan();
    if (wx.offBluetoothDeviceFound) {
      wx.offBluetoothDeviceFound();
    }
  },

  ensureBluetoothAdapter: function () {
    return new Promise((resolve) => {
      wx.openBluetoothAdapter({
        success: () => {
          this.setData({ btReady: true, scanHint: '蓝牙可用，点击开始扫描' });
          resolve(true);
        },
        fail: (err) => {
          this.setData({ btReady: false, scanHint: '蓝牙不可用，请开启权限/开关' });
          wx.showModal({
            title: '蓝牙未可用',
            content: (err && err.errMsg) ? err.errMsg : '请检查蓝牙权限/开关',
            showCancel: false
          });
          resolve(false);
        }
      });
    });
  },

  parseAdvName: function (device) {
    if (!device) return '';
    if (device.localName) return device.localName;
    if (device.name) return device.name;
    // Manufacturer/local name may not always be exposed in mini-program; keep fallback.
    return '';
  },

  isGatewayLikeDevice: function (device) {
    if (!device) return false;

    const uuids = (device.advertisServiceUUIDs || device.serviceUUIDs || []).map(normalizeUuid);
    if (uuids.includes(normalizeUuid(BLE_CTRL_UUID_SVC))) return true;

    const name = (this.parseAdvName(device) || '').toLowerCase();
    // Fallback name matching for some phones that don't expose UUID during scan.
    if (name.includes('lvglframe') || name.includes('esp') || name.includes('gateway')) return true;
    return false;
  },

  mergeDiscoveredDevice: function (incoming) {
    if (!incoming || !incoming.deviceId) return;
    const onlyGateway = this.data.onlyShowGateway;
    const isGateway = this.isGatewayLikeDevice(incoming);
    if (onlyGateway && !isGateway) return;

    const list = this.data.devices.slice();
    const idx = list.findIndex(d => d.deviceId === incoming.deviceId);
    const merged = {
      deviceId: incoming.deviceId,
      name: this.parseAdvName(incoming) || (idx >= 0 ? list[idx].name : ''),
      rssi: (typeof incoming.RSSI === 'number') ? incoming.RSSI : (idx >= 0 ? list[idx].rssi : -999),
      isGateway: isGateway,
      advertisServiceUUIDs: (incoming.advertisServiceUUIDs || incoming.serviceUUIDs || []),
    };

    if (idx >= 0) {
      list[idx] = Object.assign({}, list[idx], merged);
    } else {
      list.push(merged);
    }
    // Keep strongest signal first for convenience.
    list.sort((a, b) => (b.rssi || -999) - (a.rssi || -999));
    this.setData({ devices: list });
  },

  toggleGatewayFilter: function () {
    this.setData({ onlyShowGateway: !this.data.onlyShowGateway, devices: [] });
    if (this.data.scanning) {
      this.startScan();
    }
  },

  startScan: function () {
    const that = this;
    // restart scan for fresh list
    if (that.data.scanning) {
      that.stopScan();
    }

    that.setData({ devices: [], scanning: false, scanHint: '正在启动扫描...' });

    that.ensureBluetoothAdapter().then((ok) => {
      if (!ok) return;

      if (wx.offBluetoothDeviceFound) {
        wx.offBluetoothDeviceFound();
      }

      wx.onBluetoothDeviceFound(function (res) {
        const found = (res && res.devices) ? res.devices : (res ? [res] : []);
        found.forEach((d) => that.mergeDiscoveredDevice(d));
      });

      wx.startBluetoothDevicesDiscovery({
        allowDuplicatesKey: true,
        interval: 0,
        powerLevel: 'high',
        success: function () {
          that.setData({ scanning: true, scanHint: '扫描中...' });
          // Some devices are only returned by getBluetoothDevices()
          wx.getBluetoothDevices({
            success: function (resp) {
              (resp.devices || []).forEach((d) => that.mergeDiscoveredDevice(d));
            }
          });
        },
        fail: function (err) {
          that.setData({ scanning: false, scanHint: '开始扫描失败' });
          wx.showToast({ title: '开始扫描失败', icon: 'none' });
          console.error(err);
        }
      });
    });
  },

  stopScan: function () {
    if (!this.data.scanning) {
      this.setData({ scanHint: '已停止扫描' });
      return;
    }
    wx.stopBluetoothDevicesDiscovery({
      success: () => {
        this.setData({ scanning: false, scanHint: '已停止扫描' });
      },
      fail: () => {
        this.setData({ scanning: false, scanHint: '已停止扫描' });
      }
    });
  },

  connectDevice: async function (e) {
    const deviceId = e.currentTarget.dataset.deviceId;
    if (!deviceId) return;
    const dev = (this.data.devices || []).find(d => d.deviceId === deviceId);
    const name = dev ? (dev.name || '') : '';

    wx.showLoading({ title: '连接中...', mask: true });

    const svcIdAndChars = await this.connectAndFindCtrlChars(deviceId);
    wx.hideLoading();

    if (!svcIdAndChars) {
      wx.showToast({ title: '连接或查找特征失败', icon: 'none' });
      return;
    }

    wx.setStorageSync('ble_gateway', {
      deviceId,
      name,
      serviceId: svcIdAndChars.serviceId,
      writeCharId: svcIdAndChars.writeCharId,
      notifyCharId: svcIdAndChars.notifyCharId
    });
    this.refreshCurrentGateway();

    wx.showToast({ title: '已连接并保存特征', icon: 'success' });
    wx.navigateTo({ url: '/pages/near/near' });
  },

  refreshCurrentGateway: function () {
    const gw = wx.getStorageSync('ble_gateway');
    this.setData({ currentGateway: gw && gw.deviceId ? gw : null });
  },

  clearCurrentGateway: function () {
    wx.removeStorageSync('ble_gateway');
    this.setData({ currentGateway: null });
    wx.showToast({ title: '已清除网关绑定', icon: 'none' });
  },

  goNear: function () {
    wx.navigateTo({ url: '/pages/near/near' });
  },

  goMesh: function () {
    wx.navigateTo({ url: '/pages/mesh/mesh' });
  },

  goRemote: function () {
    wx.navigateTo({ url: '/pages/remote/remote' });
  },

  connectAndFindCtrlChars: function (deviceId) {
    return new Promise((resolve) => {
      const afterConnected = () => {
        // 获取服务
        wx.getBLEDeviceServices({
          deviceId,
          success: function (services) {
            let targetService = null;
            if (services && services.services) {
              for (const s of services.services) {
                if (normalizeUuid(s.uuid) === normalizeUuid(BLE_CTRL_UUID_SVC)) {
                  targetService = s;
                  break;
                }
              }
            }
            if (!targetService) {
              resolve(null);
              return;
            }

            wx.getBLEDeviceCharacteristics({
              deviceId,
              serviceId: targetService.uuid,
              success: function (charsRes) {
                let writeCharId = null;
                let notifyCharId = null;
                if (charsRes && charsRes.characteristics) {
                  for (const c of charsRes.characteristics) {
                    const cu = normalizeUuid(c.uuid);
                    if (cu === normalizeUuid(BLE_CTRL_UUID_RX)) writeCharId = c.uuid;
                    if (cu === normalizeUuid(BLE_CTRL_UUID_TX)) notifyCharId = c.uuid;
                  }
                }

                if (!writeCharId || !notifyCharId) {
                  resolve(null);
                  return;
                }

                // For mini program: write/notify APIs need serviceId and characteristicId (uuid string).
                resolve({
                  serviceId: targetService.uuid,
                  writeCharId,
                  notifyCharId
                });
              },
              fail: function () {
                resolve(null);
              }
            });
          },
          fail: function () {
            resolve(null);
          }
        });
      };

      wx.createBLEConnection({
        deviceId,
        timeout: 20000,
        success: afterConnected,
        fail: function (err) {
          // 已连接时重复连接会报错；直接复用现有连接继续查服务/特征即可。
          const msg = err && (err.errMsg || '');
          if (String(msg).indexOf('already connect') !== -1) {
            afterConnected();
            return;
          }
          console.error(err);
          resolve(null);
        }
      });
    });
  },
});


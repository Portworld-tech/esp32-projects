const cloudCfg = require('../../utils/cloud_config.js');
const { MiniMqttWsClient } = require('../../utils/mqtt_ws_client.js');
const stateStore = require('../../utils/device_state_store.js');
const homePrefs = require('../../utils/home_prefs.js');

const ITEM_LABELS = {
  0: '系统',
  1: 'Switch 4',
  2: 'Switch 3',
  3: 'Switch 1',
  4: '屏3 温度 −1',
  5: '屏3 温度 +1',
  6: '屏5 温度 −1',
  7: '屏5 温度 +1',
  10: 'Mesh 配网',
};

const FEED_MAX = 28;

function bemfaGetStatus(uid, topic) {
  return new Promise((resolve, reject) => {
    wx.request({
      url: 'https://api.bemfa.com/api/device/v1/status/',
      data: { uid, topic },
      header: {
        'content-type': 'application/x-www-form-urlencoded'
      },
      success: (res) => resolve(res),
      fail: (err) => reject(err)
    });
  });
}

function isSocketDomainBlocked(err) {
  const msg = (err && (err.errMsg || err.message)) ? String(err.errMsg || err.message) : '';
  return msg.indexOf('url not in domain list') !== -1;
}

function showSocketDomainHintOnce(page) {
  if (page._mqttDomainHintOnce) return;
  page._mqttDomainHintOnce = true;
  wx.showModal({
    title: '无法连接 MQTT（域名未放行）',
    content:
      '请在微信公众平台 → 开发 → 开发管理 → 服务器域名 → socket 合法域名中添加：bemfa.com（或完整 wss 域名）。\n' +
      '本地调试可在开发者工具中勾选「不校验合法域名、web-view、TLS 版本」。',
    showCancel: false,
  });
}

function parseStatusMsg(s) {
  if (!s) return null;
  const parts = String(s).trim().split(/[\s,;|]+/).filter(Boolean);
  if (parts.length < 3) return null;
  const item = Number(parts[0]);
  const value = Number(parts[1]);
  const rc = Number(parts[2]);
  if (!Number.isFinite(item) || !Number.isFinite(value) || !Number.isFinite(rc)) return null;
  const tag = parts.length > 3 ? parts.slice(3).join(' ') : '';
  return { item, value, rc, tag };
}

function formatTimeStr(d) {
  const pad = (n) => (n < 10 ? `0${n}` : `${n}`);
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

function topicShortName(topic) {
  if (!topic) return '-';
  const t = String(topic);
  const i = t.lastIndexOf('/');
  return i >= 0 ? t.slice(i + 1) || t : t;
}

function formatUplinkSummary(payload) {
  const raw = String(payload || '').trim();
  if (raw.charAt(0) === '{') {
    try {
      const o = JSON.parse(raw);
      if (o.v === 1) {
        const s4 = o.i1 ? '开' : '关';
        const s3 = o.i2 ? '开' : '关';
        const s1 = o.i3 ? '开' : '关';
        return `设备同步 · 屏3 ${o.t3}°C（${o.e3 ? '可调' : '关机'}）· 屏5 ${o.t5}°C（${o.e5 ? '可调' : '关机'}）· Sw4/3/1 ${s4}/${s3}/${s1}`;
      }
    } catch (e) { /* ignore */ }
  }
  const p = parseStatusMsg(payload);
  if (p) {
    const name = ITEM_LABELS[p.item] || `项 ${p.item}`;
    let action = '';
    if (p.item === 1 || p.item === 2 || p.item === 3 || p.item === 10) {
      action = p.value ? '开' : '关';
    } else if (p.item >= 4 && p.item <= 7) {
      action = '步进';
    } else {
      action = `值 ${p.value}`;
    }
    const ok = p.rc === 0;
    const tail = p.tag ? ` (${p.tag})` : '';
    return `${name} · ${action} · ${ok ? '成功' : '失败'}${tail}`;
  }
  const pl = String(payload || '').trim();
  if (pl === 'gw_online' || pl === 'online' || pl === 'sys online') {
    return '设备网关已连接 MQTT';
  }
  if (pl.indexOf('err') === 0) {
    return `设备提示：${pl}`;
  }
  return pl || '（空负载）';
}

function isStatusTopic(page, topic) {
  if (!topic) return false;
  const base = page.data.statusTopic;
  return (
    topic === base ||
    topic === `${base}/set` ||
    topic === `${base}/up`
  );
}

Page({
  data: {
    uid: cloudCfg.uid,
    cmdTopic: cloudCfg.cmdTopic,
    statusTopic: cloudCfg.statusTopic,
    cloudHost: cloudCfg.host,
    cloudPort: cloudCfg.mqttPort,
    wsUrl: cloudCfg.wsUrl,
    wsUrlInput: cloudCfg.wsUrl,

    deviceStatus: '离线',
    mqttConnected: false,
    switchStates: {
      1: false,
      2: false,
      3: false
    },
    meshProvEnabled: false,
    tempScreen3: 16,
    tempScreen5: 16,
    allowTemp3: true,
    allowTemp5: true,
    syncStale: true,

    lastStatus: '',
    lastStatusRaw: '',
    lastStatusSummary: '',
    lastRxTimeStr: '',
    statusFeed: [],

    mqttStateText: 'MQTT未连接',
    topicCheckText: '',
    cmdTopicInput: '',
    statusTopicInput: '',

    showAdvanced: false,
    expertMode: false,
    alias1: '客厅空调',
    alias2: '客厅采暖',
    alias3: '卧室地暖',

    _timer: null,
    _mqtt: null,
    _reconnectTimer: null,
    _connOpenTick: 0,
    _fastCloseCount: 0,
    _fastCloseHintShown: false,
  },

  onLoad: function () {
    this._mqttDomainHintOnce = false;
    const cfg = wx.getStorageSync('cloud_cfg_override') || {};
    if (cfg.wsUrl) {
      let changed = false;
      if (cfg.wsUrl.includes('/mqtt')) {
        cfg.wsUrl = cfg.wsUrl.replace('/mqtt', '/wss');
        changed = true;
      }
      if (cfg.wsUrl.includes('bemfa.com:9501')) {
        cfg.wsUrl = cfg.wsUrl.replace('bemfa.com:9501', 'bemfa.com:9504');
        changed = true;
      }
      if (changed) {
        wx.setStorageSync('cloud_cfg_override', cfg);
      }
    }
    if (cfg.uid || cfg.cmdTopic || cfg.statusTopic) {
      this.setData({
        uid: cfg.uid || this.data.uid,
        cmdTopic: cfg.cmdTopic || this.data.cmdTopic,
        statusTopic: cfg.statusTopic || this.data.statusTopic,
        wsUrl: cfg.wsUrl || this.data.wsUrl,
      });
    }
    this.setData({
      cmdTopicInput: this.data.cmdTopic,
      statusTopicInput: this.data.statusTopic,
      wsUrlInput: this.data.wsUrl,
    });
    this.loadUiPrefs();
    this.hydrateRemoteFromCache();
    this.startMqtt();
    this.fetchAll();
    this.data._timer = setInterval(() => {
      this.fetchAll();
    }, 5000);
  },

  onShow() {
    /* 从缓存恢复：仅在未连接 MQTT 时用本地快照铺满，避免退出后全是默认「关」 */
    const live = this.data._mqtt && this.data._mqtt.connected;
    if (!live) {
      this.hydrateRemoteFromCache();
      this.setData({ syncStale: true });
    }
  },

  onHide() {
    this.persistRemoteStateFromData();
  },

  onUnload: function () {
    this.persistRemoteStateFromData();
    this.stopMqtt();
    if (this.data._reconnectTimer) {
      clearTimeout(this.data._reconnectTimer);
      this.data._reconnectTimer = null;
    }
    if (this.data._timer) {
      clearInterval(this.data._timer);
      this.data._timer = null;
    }
  },

  toggleAdvanced() {
    this.setData({ showAdvanced: !this.data.showAdvanced });
  },

  loadUiPrefs() {
    const p = homePrefs.loadPrefs();
    this.setData({
      expertMode: !!p.expertMode,
      alias1: p.aliases[1],
      alias2: p.aliases[2],
      alias3: p.aliases[3],
    });
  },

  toggleExpertMode() {
    const next = !this.data.expertMode;
    this.setData({ expertMode: next, showAdvanced: next ? this.data.showAdvanced : false });
    homePrefs.savePrefs({
      expertMode: next,
      aliases: {
        1: this.data.alias1,
        2: this.data.alias2,
        3: this.data.alias3,
      },
    });
    wx.showToast({ title: next ? '已切换专家模式' : '已切换简洁模式', icon: 'none' });
  },

  /** 将当前 data 中的设备相关字段写入本地（勿直接改 this.data.switchStates 引用再 setData） */
  persistRemoteStateFromData() {
    stateStore.saveRemoteSnapshot({
      uid: this.data.uid,
      statusTopic: this.data.statusTopic,
      switchStates: {
        1: !!this.data.switchStates[1],
        2: !!this.data.switchStates[2],
        3: !!this.data.switchStates[3],
      },
      tempScreen3: this.data.tempScreen3,
      tempScreen5: this.data.tempScreen5,
      allowTemp3: !!this.data.allowTemp3,
      allowTemp5: !!this.data.allowTemp5,
      meshProvEnabled: !!this.data.meshProvEnabled,
    });
  },

  hydrateRemoteFromCache() {
    const snap = stateStore.loadRemoteSnapshot(this.data.uid, this.data.statusTopic);
    if (!snap) return;
    this.setData({
      switchStates: {
        1: !!snap.switchStates[1],
        2: !!snap.switchStates[2],
        3: !!snap.switchStates[3],
      },
      tempScreen3: Number.isFinite(Number(snap.tempScreen3)) ? snap.tempScreen3 : this.data.tempScreen3,
      tempScreen5: Number.isFinite(Number(snap.tempScreen5)) ? snap.tempScreen5 : this.data.tempScreen5,
      allowTemp3: !!snap.allowTemp3,
      allowTemp5: !!snap.allowTemp5,
      meshProvEnabled: !!snap.meshProvEnabled,
    });
  },

  applyDeviceSync(o) {
    if (!o || o.v !== 1) return;
    const t3 = Number(o.t3);
    const t5 = Number(o.t5);
    this.setData({
      switchStates: {
        1: !!o.i1,
        2: !!o.i2,
        3: !!o.i3,
      },
      tempScreen3: Number.isFinite(t3) ? t3 : this.data.tempScreen3,
      tempScreen5: Number.isFinite(t5) ? t5 : this.data.tempScreen5,
      allowTemp3: !!o.e3,
      allowTemp5: !!o.e5,
      meshProvEnabled: !!o.m,
      syncStale: false,
    });
    this.persistRemoteStateFromData();
  },

  pushStatusFeed(topic, payload) {
    const now = new Date();
    const timeStr = formatTimeStr(now);
    const summary = isStatusTopic(this, topic)
      ? formatUplinkSummary(payload)
      : `[其他主题] ${formatUplinkSummary(payload)}`;
    const entry = {
      ts: now.getTime(),
      timeStr,
      summary,
      raw: String(payload || ''),
      topicShort: topicShortName(topic),
    };
    const feed = [entry, ...this.data.statusFeed].slice(0, FEED_MAX);
    const rawLine = `${topic}: ${payload}`;
    this.setData({
      statusFeed: feed,
      lastRxTimeStr: timeStr,
      lastStatusSummary: summary,
      lastStatus: rawLine,
      lastStatusRaw: rawLine,
    });
  },

  startMqtt() {
    this.stopMqtt();
    this.setData({ mqttConnected: false });
    this.data._connOpenTick = 0;
    const c = new MiniMqttWsClient({
      wsUrl: this.data.wsUrl,
      clientId: this.data.uid,
      username: this.data.uid,
      protocols: ['mqtt'],
      keepAliveSec: 60,
    });
    c.on('debug', (d) => {
      if (d && d.msg) console.log('[mqtt-debug]', d.msg);
      if (d && d.msg && String(d.msg).indexOf('ws open:') === 0) {
        this.data._connOpenTick = Date.now();
      }
    });
    c.on('connect', () => {
      this.setData({
        mqttStateText: `已连接 ${this.data.wsUrl}`,
        deviceStatus: '在线',
        mqttConnected: true,
      });
      c.subscribe(this.data.statusTopic);
      c.subscribe(`${this.data.statusTopic}/set`);
      c.subscribe(`${this.data.statusTopic}/up`);
      c.subscribe(this.data.cmdTopic);
      c.subscribe(`${this.data.cmdTopic}/set`);
    });
    c.on('message', ({ topic, payload }) => {
      const raw = String(payload || '').trim();
      if (raw.charAt(0) === '{') {
        try {
          const o = JSON.parse(raw);
          if (o.v === 1) {
            this.applyDeviceSync(o);
          }
        } catch (e) { /* ignore */ }
      }
      const parsed = parseStatusMsg(payload);
      this.pushStatusFeed(topic, payload);
      if (!parsed) return;
      if (parsed.rc === 0) {
        if (parsed.item === 1 || parsed.item === 2 || parsed.item === 3) {
          const next = {
            1: !!this.data.switchStates[1],
            2: !!this.data.switchStates[2],
            3: !!this.data.switchStates[3],
            [parsed.item]: parsed.value !== 0,
          };
          this.setData({ switchStates: next });
          this.persistRemoteStateFromData();
        } else if (parsed.item === 10) {
          this.setData({ meshProvEnabled: parsed.value !== 0 });
          this.persistRemoteStateFromData();
        } else if (parsed.item >= 4 && parsed.item <= 7 && parsed.value !== 0) {
          /* 步进成功：设备稍后仍会 JSON 同步，此处可按方向粗更新显示 */
          let delta = 0;
          if (parsed.item === 4 || parsed.item === 6) delta = -1;
          if (parsed.item === 5 || parsed.item === 7) delta = 1;
          if (parsed.item === 4 || parsed.item === 5) {
            this.setData({ tempScreen3: this.data.tempScreen3 + delta });
          } else {
            this.setData({ tempScreen5: this.data.tempScreen5 + delta });
          }
          this.persistRemoteStateFromData();
        }
      }
    });
    c.on('error', (e) => {
      if (isSocketDomainBlocked(e)) showSocketDomainHintOnce(this);
      this.setData({
        mqttStateText: `错误: ${(e && e.errMsg) || 'unknown'}`,
        deviceStatus: '离线',
        mqttConnected: false,
      });
    });
    c.on('close', () => {
      // 如果反复 ws open 后立即 close(1000) 且没收到 CONNACK，通常是 broker 侧主动断开：
      // 最常见原因：设备与小程序使用同一个 MQTT clientId（Bemfa 的 uid），互相挤下线导致抖动。
      if (!c.connected && this.data._connOpenTick) {
        const dt = Date.now() - this.data._connOpenTick;
        if (dt < 1200) {
          this.data._fastCloseCount = (this.data._fastCloseCount || 0) + 1;
        } else {
          this.data._fastCloseCount = 0;
        }
        if (this.data._fastCloseCount >= 3 && !this.data._fastCloseHintShown) {
          this.data._fastCloseHintShown = true;
          wx.showModal({
            title: '云端连接被断开',
            content:
              '检测到连接后立即断开，可能是 MQTT clientId 冲突（设备端与小程序都用同一个 uid 作为 clientId，互相挤下线）。\n' +
              '解决方式：\n' +
              '1) 临时：关闭设备端 MQTT 或让设备不自动重连后，再用小程序控制；\n' +
              '2) 推荐：按巴法文档使用 appID/secretKey 鉴权，为小程序设置独立 clientId。\n',
            showCancel: false,
          });
        }
      }
      this.setData({
        mqttStateText: '连接已断开',
        deviceStatus: '离线',
        mqttConnected: false,
        syncStale: true,
      });
      if (this.data._reconnectTimer) clearTimeout(this.data._reconnectTimer);
      this.data._reconnectTimer = setTimeout(() => {
        this.startMqtt();
      }, 2500);
    });
    c.connect().catch((e) => {
      if (isSocketDomainBlocked(e)) showSocketDomainHintOnce(this);
      this.setData({
        mqttStateText: `连接失败: ${(e && e.errMsg) || 'unknown'}`,
        mqttConnected: false,
      });
    });
    this.data._mqtt = c;
  },

  stopMqtt() {
    if (this.data._mqtt) {
      this.data._mqtt.disconnect();
      this.data._mqtt = null;
    }
  },

  async fetchAll() {
    try {
      const uid = this.data.uid;
      const cmdTopic = this.data.cmdTopic;
      const st = await bemfaGetStatus(uid, cmdTopic);
      const code = st && st.data ? String(st.data.code || '0') : 'x';
      if (code === '40012') {
        this.setData({ topicCheckText: `CMD 主题无效: ${cmdTopic}` });
      } else if (code === '30011') {
        this.setData({ topicCheckText: 'CMD 主题存在，订阅状态以 MQTT 为准' });
      }
      const connected = !!(this.data._mqtt && this.data._mqtt.connected);
      this.setData({
        deviceStatus: connected ? '在线' : '离线',
        mqttConnected: connected,
      });
    } catch (e) {
      console.warn(e);
    }
  },

  async sendCmdItemValue(itemId, value, silent) {
    try {
      if ((itemId === 4 || itemId === 5) && !this.data.allowTemp3) {
        wx.showToast({ title: '空调已关机，请先打开 Switch4', icon: 'none', duration: 2000 });
        return;
      }
      if ((itemId === 6 || itemId === 7) && !this.data.allowTemp5) {
        wx.showToast({ title: '地暖已关机，请先打开 Switch1', icon: 'none', duration: 2000 });
        return;
      }
      const msg = `${itemId} ${value}`;
      if (!this.data._mqtt || !this.data._mqtt.connected) {
        wx.showToast({ title: 'MQTT 未连接', icon: 'none', duration: 1800 });
        return;
      }
      const pubTopic = `${this.data.cmdTopic}/set`;
      const ok = this.data._mqtt.publish(pubTopic, msg);
      if (!ok) {
        wx.showToast({ title: '发布失败', icon: 'none', duration: 1800 });
        return;
      }
      this.setData({ topicCheckText: `已发 ${pubTopic} → ${msg}` });
      /* 先乐观更新 UI 并落盘，避免未收到 ACK 就退出导致状态丢失；设备 JSON 随后覆盖为准 */
      if (itemId === 1 || itemId === 2 || itemId === 3) {
        const next = {
          1: !!this.data.switchStates[1],
          2: !!this.data.switchStates[2],
          3: !!this.data.switchStates[3],
          [itemId]: value !== 0,
        };
        this.setData({ switchStates: next });
      } else if (itemId === 10) {
        this.setData({ meshProvEnabled: value !== 0 });
      } else if (itemId >= 4 && itemId <= 7) {
        let d = 0;
        if (itemId === 4 || itemId === 6) d = -1;
        if (itemId === 5 || itemId === 7) d = 1;
        if (itemId === 4 || itemId === 5) {
          this.setData({ tempScreen3: this.data.tempScreen3 + d });
        } else {
          this.setData({ tempScreen5: this.data.tempScreen5 + d });
        }
      }
      this.persistRemoteStateFromData();
      if (!silent) {
        wx.showToast({ title: '已下发', icon: 'success', duration: 800 });
      }
      return true;
    } catch (e) {
      console.error(e);
      wx.showToast({ title: '下发失败', icon: 'none', duration: 1500 });
      return false;
    }
  },

  toggleSw4: function () {
    const desiredOn = !this.data.switchStates[1];
    this.sendCmdItemValue(1, desiredOn ? 1 : 0);
  },
  toggleSw3: function () {
    const desiredOn = !this.data.switchStates[2];
    this.sendCmdItemValue(2, desiredOn ? 1 : 0);
  },
  toggleSw1: function () {
    const desiredOn = !this.data.switchStates[3];
    this.sendCmdItemValue(3, desiredOn ? 1 : 0);
  },

  async sceneGoHome() {
    if (!this.data._mqtt || !this.data._mqtt.connected) {
      wx.showToast({ title: 'MQTT 未连接', icon: 'none' });
      return;
    }
    wx.showLoading({ title: '执行中...', mask: true });
    try {
      await this.sendCmdItemValue(1, 1, true);
      await this.sendCmdItemValue(2, 1, true);
      await this.sendCmdItemValue(3, 1, true);
      wx.showToast({ title: '已切换回家模式', icon: 'success' });
    } finally {
      wx.hideLoading();
    }
  },

  async sceneGoAway() {
    if (!this.data._mqtt || !this.data._mqtt.connected) {
      wx.showToast({ title: 'MQTT 未连接', icon: 'none' });
      return;
    }
    wx.showLoading({ title: '执行中...', mask: true });
    try {
      await this.sendCmdItemValue(1, 0, true);
      await this.sendCmdItemValue(2, 0, true);
      await this.sendCmdItemValue(3, 0, true);
      wx.showToast({ title: '已切换离家模式', icon: 'success' });
    } finally {
      wx.hideLoading();
    }
  },

  stepTemp3Up: function () {
    this.sendCmdItemValue(5, 1);
  },
  stepTemp3Down: function () {
    this.sendCmdItemValue(4, 1);
  },
  stepTemp5Up: function () {
    this.sendCmdItemValue(7, 1);
  },
  stepTemp5Down: function () {
    this.sendCmdItemValue(6, 1);
  },

  async toggleMeshProv() {
    const enable = !this.data.meshProvEnabled;
    this.sendCmdItemValue(10, enable ? 1 : 0);
  },

  onCmdTopicInput(e) {
    this.setData({ cmdTopicInput: e.detail.value || '' });
  },

  onStatusTopicInput(e) {
    this.setData({ statusTopicInput: e.detail.value || '' });
  },

  onWsUrlInput(e) {
    this.setData({ wsUrlInput: e.detail.value || '' });
  },

  applyTopicConfig() {
    const cmdTopic = (this.data.cmdTopicInput || '').trim();
    const statusTopic = (this.data.statusTopicInput || '').trim();
    const wsUrl = (this.data.wsUrlInput || '').trim();
    if (!cmdTopic || !statusTopic || !wsUrl) {
      wx.showToast({ title: '请填写完整', icon: 'none' });
      return;
    }
    this.setData({ cmdTopic, statusTopic, wsUrl });
    wx.setStorageSync('cloud_cfg_override', {
      uid: this.data.uid,
      cmdTopic,
      statusTopic,
      wsUrl,
    });
    /* 主题变更后旧缓存不可再用，避免串题显示错误开关 */
    stateStore.clearRemoteSnapshot();
    wx.showToast({ title: '已保存', icon: 'success' });
    this.startMqtt();
    this.checkTopics();
  },

  async checkTopics() {
    const uid = this.data.uid;
    const cmdTopic = this.data.cmdTopic;
    const statusTopic = this.data.statusTopic;
    try {
      const c = await bemfaGetStatus(uid, cmdTopic);
      const s = await bemfaGetStatus(uid, statusTopic);
      const cCode = c && c.data ? String(c.data.code || '0') : 'x';
      const sCode = s && s.data ? String(s.data.code || '0') : 'x';
      let tip = `CMD(${cmdTopic}) code=${cCode} · STATUS(${statusTopic}) code=${sCode}`;
      if (cCode === '40012' || sCode === '40012') {
        tip += ' · 主题与 UID 不匹配？请到巴法控制台核对';
      } else if (cCode === '30011' && sCode === '30011') {
        tip += ' · 主题有效';
      }
      this.setData({ topicCheckText: tip });
    } catch (e) {
      this.setData({ topicCheckText: '主题检查失败，请检查网络' });
    }
  }
});

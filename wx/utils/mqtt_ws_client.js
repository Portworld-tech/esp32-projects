function strToUtf8Bytes(str) {
  const out = [];
  for (let i = 0; i < str.length; i++) {
    let c = str.charCodeAt(i);
    if (c < 0x80) {
      out.push(c);
    } else if (c < 0x800) {
      out.push(0xc0 | (c >> 6));
      out.push(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      out.push(0xe0 | (c >> 12));
      out.push(0x80 | ((c >> 6) & 0x3f));
      out.push(0x80 | (c & 0x3f));
    } else {
      out.push(0xf0 | (c >> 18));
      out.push(0x80 | ((c >> 12) & 0x3f));
      out.push(0x80 | ((c >> 6) & 0x3f));
      out.push(0x80 | (c & 0x3f));
    }
  }
  return out;
}

function utf8BytesToStr(bytes) {
  let out = '';
  let i = 0;
  while (i < bytes.length) {
    const c = bytes[i++];
    if (c < 0x80) out += String.fromCharCode(c);
    else if ((c & 0xe0) === 0xc0) {
      const c2 = bytes[i++] & 0x3f;
      out += String.fromCharCode(((c & 0x1f) << 6) | c2);
    } else {
      const c2 = bytes[i++] & 0x3f;
      const c3 = bytes[i++] & 0x3f;
      out += String.fromCharCode(((c & 0x0f) << 12) | (c2 << 6) | c3);
    }
  }
  return out;
}

function pushUint16(arr, n) {
  arr.push((n >> 8) & 0xff, n & 0xff);
}

function pushStr(arr, s) {
  const b = strToUtf8Bytes(s || '');
  pushUint16(arr, b.length);
  for (let i = 0; i < b.length; i++) arr.push(b[i]);
}

function encodeRemainingLength(len) {
  const out = [];
  do {
    let d = len % 128;
    len = Math.floor(len / 128);
    if (len > 0) d |= 0x80;
    out.push(d);
  } while (len > 0);
  return out;
}

function toArrayBuffer(byteArray) {
  const u = new Uint8Array(byteArray);
  return u.buffer;
}

class MiniMqttWsClient {
  constructor(opts) {
    this.opts = opts || {};
    this.socketTask = null;
    this.connected = false;
    this.packetId = 1;
    this.rxBuf = new Uint8Array(0);
    this.pingTimer = null;
    this.connackTimer = null;
    this.handlers = {
      connect: null,
      message: null,
      error: null,
      close: null,
      debug: null,
    };
    this._manualClosed = false;
  }

  on(event, fn) {
    this.handlers[event] = fn;
  }

  emit(event, payload) {
    const fn = this.handlers[event];
    if (typeof fn === 'function') fn(payload);
  }

  nextPacketId() {
    this.packetId++;
    if (this.packetId > 0xffff) this.packetId = 1;
    return this.packetId;
  }

  connect() {
    return new Promise((resolve, reject) => {
      this._manualClosed = false;
      this.socketTask = wx.connectSocket({
        url: this.opts.wsUrl,
        timeout: 15000,
        protocols: this.opts.protocols || ['mqtt'],
      });

      this.socketTask.onOpen(() => {
        this.emit('debug', { msg: `ws open: ${this.opts.wsUrl}` });
        this.sendConnect();
        if (this.connackTimer) clearTimeout(this.connackTimer);
        this.connackTimer = setTimeout(() => {
          if (!this.connected) {
            const err = { errMsg: 'MQTT CONNACK timeout' };
            this.emit('error', err);
          }
        }, 8000);
        resolve();
      });

      this.socketTask.onError((err) => {
        this.emit('debug', { msg: `ws error: ${JSON.stringify(err || {})}` });
        this.emit('error', err);
        reject(err);
      });

      this.socketTask.onClose((e) => {
        this.connected = false;
        if (this.connackTimer) {
          clearTimeout(this.connackTimer);
          this.connackTimer = null;
        }
        if (this.pingTimer) {
          clearInterval(this.pingTimer);
          this.pingTimer = null;
        }
        this.emit('debug', { msg: `ws close: ${JSON.stringify(e || {})}` });
        this.emit('close', e);
      });

      this.socketTask.onMessage((res) => {
        const data = res.data;
        let chunk = null;
        if (typeof data === 'string') {
          chunk = new Uint8Array(strToUtf8Bytes(data));
        } else {
          chunk = new Uint8Array(data);
        }
        this.appendAndParse(chunk);
      });
    });
  }

  appendAndParse(chunk) {
    const merged = new Uint8Array(this.rxBuf.length + chunk.length);
    merged.set(this.rxBuf, 0);
    merged.set(chunk, this.rxBuf.length);
    this.rxBuf = merged;

    while (this.rxBuf.length >= 2) {
      const typeFlags = this.rxBuf[0];
      let mul = 1;
      let remLen = 0;
      let idx = 1;
      let encBytes = 0;
      while (true) {
        if (idx >= this.rxBuf.length) return;
        const b = this.rxBuf[idx++];
        encBytes++;
        remLen += (b & 127) * mul;
        mul *= 128;
        if ((b & 128) === 0) break;
        if (encBytes > 4) return;
      }
      const headerLen = 1 + encBytes;
      const frameLen = headerLen + remLen;
      if (this.rxBuf.length < frameLen) return;

      const packetType = (typeFlags >> 4) & 0x0f;
      const payload = this.rxBuf.slice(headerLen, frameLen);
      this.handlePacket(packetType, payload);

      this.rxBuf = this.rxBuf.slice(frameLen);
    }
  }

  handlePacket(packetType, payload) {
    if (packetType === 2) {
      // CONNACK
      if (payload.length >= 2 && payload[1] === 0) {
        this.connected = true;
        if (this.connackTimer) {
          clearTimeout(this.connackTimer);
          this.connackTimer = null;
        }
        this.emit('connect', {});
        if (!this.pingTimer) {
          this.pingTimer = setInterval(() => this.sendPingReq(), 30000);
        }
      } else {
        const code = payload.length >= 2 ? payload[1] : -1;
        this.emit('error', { errMsg: `MQTT CONNACK failed rc=${code}` });
      }
    } else if (packetType === 3) {
      // PUBLISH (QoS0 expected)
      if (payload.length < 2) return;
      const tlen = (payload[0] << 8) | payload[1];
      if (payload.length < 2 + tlen) return;
      const topic = utf8BytesToStr(payload.slice(2, 2 + tlen));
      const data = utf8BytesToStr(payload.slice(2 + tlen));
      this.emit('message', { topic, payload: data });
    } else if (packetType === 13) {
      // PINGRESP
    }
  }

  sendRaw(bytes) {
    if (!this.socketTask) return;
    this.socketTask.send({ data: toArrayBuffer(bytes) });
  }

  sendConnect() {
    const vh = [];
    pushStr(vh, 'MQTT');
    vh.push(0x04); // 3.1.1
    const hasUser = !!this.opts.username;
    const hasPass = !!this.opts.password;
    // bit7 username, bit6 password, bit1 clean session
    let flags = 0x02;
    if (hasUser) flags |= 0x80;
    if (hasPass) flags |= 0x40;
    vh.push(flags);
    pushUint16(vh, this.opts.keepAliveSec || 60);

    const pl = [];
    pushStr(pl, this.opts.clientId || 'wx_client');
    if (hasUser) pushStr(pl, this.opts.username || '');
    if (hasPass) pushStr(pl, this.opts.password || '');

    const rem = vh.length + pl.length;
    const pkt = [0x10, ...encodeRemainingLength(rem), ...vh, ...pl];
    this.sendRaw(pkt);
  }

  sendPingReq() {
    this.sendRaw([0xc0, 0x00]);
  }

  subscribe(topic) {
    if (!this.connected) return;
    const pid = this.nextPacketId();
    const pl = [];
    pushStr(pl, topic);
    pl.push(0x00); // qos0
    const vh = [];
    pushUint16(vh, pid);
    const rem = vh.length + pl.length;
    const pkt = [0x82, ...encodeRemainingLength(rem), ...vh, ...pl];
    this.sendRaw(pkt);
  }

  publish(topic, payload) {
    if (!this.connected) return false;
    const pl = [];
    pushStr(pl, topic);
    const pb = strToUtf8Bytes(payload || '');
    for (let i = 0; i < pb.length; i++) pl.push(pb[i]);
    const pkt = [0x30, ...encodeRemainingLength(pl.length), ...pl];
    this.sendRaw(pkt);
    return true;
  }

  disconnect() {
    this._manualClosed = true;
    try {
      this.sendRaw([0xe0, 0x00]); // DISCONNECT
    } catch (e) {}
    if (this.connackTimer) {
      clearTimeout(this.connackTimer);
      this.connackTimer = null;
    }
    if (this.socketTask) {
      this.socketTask.close({});
      this.socketTask = null;
    }
    if (this.pingTimer) {
      clearInterval(this.pingTimer);
      this.pingTimer = null;
    }
    this.connected = false;
  }
}

module.exports = {
  MiniMqttWsClient,
};


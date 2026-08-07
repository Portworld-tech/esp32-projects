// BT-CTRL protocol helpers (used by BLE near control).
// Matches esp-idf implementation: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF).

function crc16CcittFalse(bytes) {
  let crc = 0xFFFF;
  for (let i = 0; i < bytes.length; i++) {
    crc ^= (bytes[i] << 8);
    for (let b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
      else crc = (crc << 1) & 0xFFFF;
    }
  }
  return crc & 0xFFFF;
}

function buildBtCtrlFrame(msgType, seq, flags, bodyBytes) {
  const BTCTRL_MAGIC0 = 0xB7;
  const BTCTRL_MAGIC1 = 0xC1;
  const BTCTRL_VERSION = 1;

  const bodyLen = bodyBytes ? bodyBytes.length : 0;
  const out = new Uint8Array(16 + bodyLen);

  // Header (packed, little-endian for uint16/uint32 fields).
  out[0] = BTCTRL_MAGIC0;
  out[1] = BTCTRL_MAGIC1;
  out[2] = BTCTRL_VERSION;
  out[3] = msgType & 0xFF;
  out[4] = seq & 0xFF;
  out[5] = (seq >> 8) & 0xFF;
  out[6] = flags & 0xFF;
  out[7] = (flags >> 8) & 0xFF;
  out[8] = bodyLen & 0xFF;
  out[9] = (bodyLen >> 8) & 0xFF;
  // crc16 (set later)
  out[10] = 0;
  out[11] = 0;
  // reserved u32=0
  out[12] = 0;
  out[13] = 0;
  out[14] = 0;
  out[15] = 0;

  if (bodyLen > 0) {
    out.set(bodyBytes, 16);
  }

  // CRC computed over (header with crc16=0) + body.
  const crc = crc16CcittFalse(out.slice(0, 16 + bodyLen));
  out[10] = crc & 0xFF;
  out[11] = (crc >> 8) & 0xFF;
  return out;
}

function buildSetStateFrame(itemId, value, seq) {
  // BTCTRL_MSG_SET_STATE = 0x11
  const BTCTRL_MSG_SET_STATE = 0x11;
  const flags = 0;
  const body = new Uint8Array([itemId & 0xFF, value & 0xFF]);
  return buildBtCtrlFrame(BTCTRL_MSG_SET_STATE, seq & 0xFFFF, flags, body);
}

function parseBtCtrlHeader(bytes) {
  if (!bytes || bytes.length < 16) return null;
  const magic0 = bytes[0], magic1 = bytes[1];
  if (magic0 !== 0xB7 || magic1 !== 0xC1) return null;
  const ver = bytes[2];
  const msgType = bytes[3];
  const seq = bytes[4] | (bytes[5] << 8);
  const flags = bytes[6] | (bytes[7] << 8);
  const len = bytes[8] | (bytes[9] << 8);
  const crc16 = bytes[10] | (bytes[11] << 8);
  return { ver, msgType, seq, flags, len, crc16 };
}

function parseAckStatus(bytes) {
  const hdr = parseBtCtrlHeader(bytes);
  if (!hdr) return null;
  // BTCTRL_MSG_ACK = 0x82
  if (hdr.msgType !== 0x82) return null;
  // body is 1 byte status
  if (bytes.length < 17) return null;
  const status = bytes[16];
  return { status, seq: hdr.seq };
}

module.exports = {
  buildSetStateFrame,
  parseAckStatus,
};


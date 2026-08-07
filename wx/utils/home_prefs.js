const KEY = 'home_ui_prefs_v1';

const DEFAULTS = {
  expertMode: false,
  aliases: {
    1: '客厅空调',
    2: '客厅采暖',
    3: '卧室地暖',
  },
};

function loadPrefs() {
  try {
    const raw = wx.getStorageSync(KEY) || {};
    return {
      expertMode: !!raw.expertMode,
      aliases: {
        1: (raw.aliases && raw.aliases[1]) || DEFAULTS.aliases[1],
        2: (raw.aliases && raw.aliases[2]) || DEFAULTS.aliases[2],
        3: (raw.aliases && raw.aliases[3]) || DEFAULTS.aliases[3],
      },
    };
  } catch (e) {
    return { ...DEFAULTS };
  }
}

function savePrefs(prefs) {
  const p = prefs || {};
  try {
    wx.setStorageSync(KEY, {
      expertMode: !!p.expertMode,
      aliases: {
        1: (p.aliases && p.aliases[1]) || DEFAULTS.aliases[1],
        2: (p.aliases && p.aliases[2]) || DEFAULTS.aliases[2],
        3: (p.aliases && p.aliases[3]) || DEFAULTS.aliases[3],
      },
    });
  } catch (e) {}
}

module.exports = {
  loadPrefs,
  savePrefs,
  DEFAULTS,
};


// Cloud config unified with device-side wifi_bemfa_client.c
module.exports = {
  host: 'bemfa.com',
  mqttPort: 9501,
  // MQTT over WebSocket URL for mini-program side.
  // If your platform uses another WS endpoint, edit this value at runtime in remote page.
  wsUrl: 'wss://bemfa.com:9504/wss',
  uid: '1ab39688601b47beb814e4c1bf001173',
  cmdTopic: 'JeStBHKBQ006',
  statusTopic: '4BE3K6ebb005',
};


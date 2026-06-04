/**
 * HeadTracker Web UI - app.js
 * WebSocket client using the HeadTracker JSON protocol.
 *
 * Protocol commands (same as serial):
 *   Send:    {"Cmd":"Get"}  {"Cmd":"Set",...}  {"Cmd":"FW"}
 *            {"Cmd":"RstCnt"}  {"Cmd":"Flash"}  {"Cmd":"Reboot"}
 *            {"Cmd":"RD","panout":true,...}  {"Cmd":"D--"}
 *   Receive: {"Cmd":"Set",...}  {"Cmd":"FW",...}
 *            {"Cmd":"Data",...}  {"Cmd":"FE",...}
 */

'use strict';

// ─── WebSocket ────────────────────────────────────────────────────────────────
const WS_URL = `ws://${location.hostname}/ws`;
let ws = null;
let reconnectTimer = null;
let streaming = false;
let settings = {};

function connect() {
  if (ws && ws.readyState < 2) return;

  ws = new WebSocket(WS_URL);

  ws.onopen = () => {
    setStatus(true);
    clearTimeout(reconnectTimer);
    send({ Cmd: 'FW' });
    send({ Cmd: 'Get' });
    // Start data stream
    send({ Cmd: 'RD', panout: true, tiltout: true, rollout: true });
    streaming = true;
    document.getElementById('btnStream').textContent = '⏸ Stop Stream';
  };

  ws.onmessage = (evt) => {
    try {
      const msg = JSON.parse(evt.data);
      handleMessage(msg);
    } catch (e) {
      console.warn('JSON parse error:', e);
    }
  };

  ws.onclose = () => {
    setStatus(false);
    streaming = false;
    document.getElementById('btnStream').textContent = '▶ Start Stream';
    reconnectTimer = setTimeout(connect, 3000);
  };

  ws.onerror = () => {
    ws.close();
  };
}

function send(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

function setStatus(connected) {
  const el = document.getElementById('wsStatus');
  el.textContent = connected ? 'Connected' : 'Disconnected';
  el.className = 'badge ' + (connected ? 'connected' : 'disconnected');
}

// ─── Message handler ──────────────────────────────────────────────────────────
function handleMessage(msg) {
  switch (msg.Cmd) {
    case 'Set':
      settings = { ...settings, ...msg };
      applySettingsToUI(msg);
      break;

    case 'FW':
      document.getElementById('fwVersion').textContent = 'v' + (msg.Vers || '?');
      updateSysInfo(msg);
      break;

    case 'Data':
      updateLiveData(msg);
      break;

    case 'Flash':
      showToast(msg.OK ? '✓ Saved to flash' : '✗ Flash failed');
      break;

    case 'RstCnt':
      showToast('✓ Center set');
      break;

    case 'Reboot':
      showToast('Rebooting…');
      break;

    default:
      console.log('Unknown cmd:', msg.Cmd, msg);
  }
}

// ─── Live data update ─────────────────────────────────────────────────────────
function updateLiveData(d) {
  if (d.panoff  !== undefined) { setAngle('pan',  d.panoff,  d.panout); }
  if (d.tiltoff !== undefined) { setAngle('tilt', d.tiltoff, d.tiltout); }
  if (d.rolloff !== undefined) { setAngle('roll', d.rolloff, d.rollout); }

  if (d.calSys !== undefined) updateCal('cal-sys', 'Sys',  d.calSys);
  if (d.calGyr !== undefined) updateCal('cal-gyr', 'Gyro', d.calGyr);
  if (d.calAcc !== undefined) updateCal('cal-acc', 'Acc',  d.calAcc);
  if (d.calMag !== undefined) updateCal('cal-mag', 'Mag',  d.calMag);

  // Big calibration tab
  if (d.calSys !== undefined) updateCalBig('b-cal-sys', d.calSys);
  if (d.calGyr !== undefined) updateCalBig('b-cal-gyr', d.calGyr);
  if (d.calAcc !== undefined) updateCalBig('b-cal-acc', d.calAcc);
  if (d.calMag !== undefined) updateCalBig('b-cal-mag', d.calMag);
}

function setAngle(axis, angleDeg, us) {
  const valEl  = document.getElementById('lv-' + axis);
  const usEl   = document.getElementById('lv-' + axis + 'Us');
  const barEl  = document.getElementById('bar-' + axis);

  if (valEl && angleDeg !== undefined) {
    valEl.textContent = (angleDeg >= 0 ? '+' : '') + angleDeg.toFixed(1) + '°';
  }
  if (usEl && us !== undefined) {
    usEl.textContent = us + ' µs';
  }
  if (barEl && us !== undefined) {
    // Bar: 1000µs = 0%, 1500µs = 50%, 2000µs = 100%
    const pct = Math.min(100, Math.max(0, (us - 1000) / 10));
    barEl.style.width = pct + '%';
  }
}

function updateCal(id, label, val) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = label + ' ' + val;
  el.className = 'cal-dot ' + (val >= 3 ? 'ok' : val >= 2 ? 'mid' : '');
}

function updateCalBig(id, val) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = val + '/3';
  el.className = 'cal-val ' + (val >= 3 ? 'ok' : val >= 2 ? 'mid' : '');
}

// ─── Apply settings to Channel / Output UI ───────────────────────────────────
function applySettingsToUI(s) {
  const fields = [
    'PanMin','PanMax','PanCnt','PanGain','PanCh',
    'TltMin','TltMax','TltCnt','TltGain','TltCh',
    'RllMin','RllMax','RllCnt','RllGain','RllCh',
  ];
  fields.forEach(id => {
    const el = document.getElementById(id);
    if (el && s[id] !== undefined) el.value = s[id];
  });

  const revFields = ['PanRev','TltRev','RllRev'];
  revFields.forEach(id => {
    const el = document.getElementById(id);
    if (el && s[id] !== undefined) el.checked = !!s[id];
  });

  // Output mode flags
  if (s.OutMode !== undefined) {
    document.getElementById('chk-ppm').checked = !!(s.OutMode & 1);
    document.getElementById('chk-ble').checked = !!(s.OutMode & 2);
  }

  // WiFi AP SSID in system info
  if (s.APSSID) {
    const sysInfo = document.getElementById('sysInfo');
    if (sysInfo) {
      const vers = document.getElementById('fwVersion').textContent;
      sysInfo.innerHTML = `Firmware: ${vers}<br>Hardware: XIAO ESP32C6<br>WiFi AP: ${s.APSSID}<br>IP: 192.168.4.1`;
    }
  }
}

function updateSysInfo(fw) {
  const sysInfo = document.getElementById('sysInfo');
  if (!sysInfo) return;
  const ap = settings.APSSID || '—';
  sysInfo.innerHTML = `Firmware: v${fw.Vers || '?'}<br>Hardware: ${fw.Hard || '?'}<br>WiFi AP: ${ap}<br>IP: 192.168.4.1`;
}

// ─── Settings change handlers ─────────────────────────────────────────────────
// Number inputs – send on change
['PanMin','PanMax','PanCnt','PanGain','PanCh',
 'TltMin','TltMax','TltCnt','TltGain','TltCh',
 'RllMin','RllMax','RllCnt','RllGain','RllCh',
].forEach(id => {
  const el = document.getElementById(id);
  if (!el) return;
  el.addEventListener('change', () => {
    const val = parseFloat(el.value);
    send({ Cmd: 'Set', [id]: val });
  });
});

// Checkboxes
['PanRev','TltRev','RllRev'].forEach(id => {
  const el = document.getElementById(id);
  if (!el) return;
  el.addEventListener('change', () => {
    send({ Cmd: 'Set', [id]: el.checked });
  });
});

// Output mode checkboxes
function updateOutputMode() {
  let mode = 0;
  if (document.getElementById('chk-ppm').checked) mode |= 1;
  if (document.getElementById('chk-ble').checked) mode |= 2;
  send({ Cmd: 'Set', OutMode: mode });
}
document.getElementById('chk-ppm').addEventListener('change', updateOutputMode);
document.getElementById('chk-ble').addEventListener('change', updateOutputMode);

// ─── Buttons ──────────────────────────────────────────────────────────────────
document.getElementById('btnSetCenter').addEventListener('click',  () => send({ Cmd: 'RstCnt' }));
document.getElementById('btnSetCenter2').addEventListener('click', () => send({ Cmd: 'RstCnt' }));
document.getElementById('btnSaveFlash').addEventListener('click',  () => send({ Cmd: 'Flash' }));
document.getElementById('btnGetSettings').addEventListener('click', () => send({ Cmd: 'Get' }));
document.getElementById('btnReboot').addEventListener('click', () => {
  if (confirm('Reboot the HeadTracker?')) send({ Cmd: 'Reboot' });
});

document.getElementById('btnStream').addEventListener('click', () => {
  streaming = !streaming;
  if (streaming) {
    send({ Cmd: 'RD', panout: true, tiltout: true, rollout: true });
    document.getElementById('btnStream').textContent = '⏸ Stop Stream';
  } else {
    send({ Cmd: 'D--' });
    document.getElementById('btnStream').textContent = '▶ Start Stream';
  }
});

// ─── Tab navigation ───────────────────────────────────────────────────────────
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const target = btn.dataset.tab;
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + target).classList.add('active');
  });
});

// ─── Toast ────────────────────────────────────────────────────────────────────
let toastTimer = null;
function showToast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2500);
}

// ─── Start ────────────────────────────────────────────────────────────────────
connect();

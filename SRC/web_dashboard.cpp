/*
 * Chamber Master - Web Dashboard Module Implementation
 */

#include "web_dashboard.h"
#include "actuators.h"
#include "cooldown.h"

void handleStartCooldown() {
  if (server.method() == HTTP_POST || server.method() == HTTP_GET) {
    startAdaptiveCooldown();
    server.send(200, "application/json", "{\"status\":\"COOLDOWN_STARTED\"}");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleRoot() {
  String html = R"=====(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ENCLOSURE DASH</title>
<style>
    :root {
        --bg: #121212;
        --card: #1e1e24;
        --glass: rgba(255, 255, 255, 0.03);
        --accent: #9c27b0;
        --accent-glow: #9c27b033;
        --text: #f8fafc;
        --muted: #94a3b8;
        --danger: #ef4444;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
        background: var(--bg);
        color: var(--text);
        font-family: system-ui, -apple-system, sans-serif;
        min-height: 100vh;
        padding: 1rem;
        background-image: radial-gradient(circle at 20% 80%, rgba(156, 39, 176, 0.1) 0%, transparent 50%),
                          radial-gradient(circle at 80% 20%, rgba(233, 30, 99, 0.1) 0%, transparent 50%);
        line-height: 1.5;
    }
    .container { max-width: 1200px; margin: auto; }
    h1 {
        text-align: center;
        font-size: clamp(1.8rem, 5vw, 2.5rem);
        margin-bottom: 1.5rem;
        background: linear-gradient(90deg, #9c27b0, #e91e63);
        -webkit-background-clip: text;
        background-clip: text;
        color: transparent;
        font-weight: 800;
        letter-spacing: 1px;
    }
    .card {
        background: var(--card);
        backdrop-filter: blur(12px);
        border-radius: 1.5rem;
        padding: 1.5rem;
        margin-bottom: 1.5rem;
        border: 1px solid rgba(255, 255, 255, 0.1);
        box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3), 0 0 20px var(--accent-glow);
        transition: 0.3s;
    }
    .card:hover {
        transform: translateY(-4px);
        box-shadow: 0 12px 40px rgba(0, 0, 0, 0.4), 0 0 30px var(--accent-glow);
    }
    .grid {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(160px, 1fr));
        gap: 1rem;
    }
    .stat {
        text-align: center;
        padding: 0.8rem;
        background: var(--glass);
        border-radius: 1rem;
        transition: 0.3s;
        min-width: 0;
    }
    .stat:hover {
        background: rgba(156, 39, 176, 0.1);
        transform: scale(1.05);
    }
    .label {
        font-size: 0.8rem;
        color: var(--muted);
        text-transform: uppercase;
        letter-spacing: 1px;
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 6px;
        margin-bottom: 0.3rem;
    }
    .label svg { width: 20px; height: 20px; fill: var(--accent); }
    .value {
        font-size: 1.4rem;
        font-weight: 700;
        color: var(--accent);
        text-shadow: 0 0 10px var(--accent-glow);
    }
    .fan-wrapper {
        width: 50px;
        height: 50px;
        margin: 0 auto 0.5rem;
        position: relative;
    }
    .fan-core {
        position: absolute;
        top: 50%;
        left: 50%;
        width: 10px;
        height: 10px;
        background: var(--accent);
        border-radius: 50%;
        transform: translate(-50%, -50%);
        box-shadow: 0 0 10px var(--accent-glow);
        z-index: 5;
    }
    .fan-blade {
        position: absolute;
        width: 8px;
        height: 22px;
        background: var(--accent);
        border-radius: 4px;
        top: 50%;
        left: 50%;
        transform-origin: 50% 0%;
        box-shadow: 0 0 10px var(--accent-glow);
    }
    .fan-blade:nth-child(1) { transform: translate(-50%, 0%) rotate(0deg); }
    .fan-blade:nth-child(2) { transform: translate(-50%, 0%) rotate(72deg); }
    .fan-blade:nth-child(3) { transform: translate(-50%, 0%) rotate(144deg); }
    .fan-blade:nth-child(4) { transform: translate(-50%, 0%) rotate(216deg); }
    .fan-blade:nth-child(5) { transform: translate(-50%, 0%) rotate(288deg); }
    @keyframes spin { to { transform: rotate(360deg); } }
    .spinning { animation: spin linear infinite; }
    #cooldownCard {
        background: linear-gradient(135deg, rgba(255, 59, 92, 0.2), rgba(156, 39, 176, 0.1));
        border: 1px solid var(--danger);
    }
    .progress-ring { width: 100px; height: 100px; margin: 1rem auto; }
    .progress-ring circle {
        cx: 50; cy: 50; r: 40;
        fill: none; stroke-width: 8; stroke-linecap: round;
    }
    .progress-ring .bg { stroke: rgba(255, 255, 255, 0.1); }
    .progress-ring .fg {
        stroke: url(#gradient);
        transform: rotate(-90deg);
        transform-origin: 50% 50%;
        transition: stroke-dashoffset 0.5s ease;
    }
    .time { text-align: center; font-size: 1.2rem; font-weight: 700; color: var(--accent); }
    .cam-wrapper {
        position: relative;
        width: 100%;
        padding-top: 56.25%;
        border-radius: 1rem;
        overflow: hidden;
        background: #000;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);
    }
    #camFrame { position: absolute; top: 0; left: 0; width: 100%; height: 100%; border: none; }
    .btn {
        background: linear-gradient(90deg, #9c27b0, #e91e63);
        color: #fff;
        border: none;
        border-radius: 1rem;
        padding: 1rem 2rem;
        font-size: 1.1rem;
        font-weight: 700;
        cursor: pointer;
        width: 100%;
        margin-top: 1rem;
        box-shadow: 0 0 25px #9c27b033;
        transition: 0.3s;
        text-transform: uppercase;
        letter-spacing: 1px;
    }
    .btn:hover {
        background: linear-gradient(90deg, #e91e63, #9c27b0);
        box-shadow: 0 0 40px #9c27b066;
        transform: translateY(-2px);
    }
    .btn:active { transform: scale(0.96); }
    #faultBanner {
        position: fixed;
        top: 10px;
        left: 50%;
        transform: translateX(-50%);
        background: #ff0000;
        color: white;
        padding: 15px 30px;
        border-radius: 10px;
        font-weight: bold;
        font-size: 1.2rem;
        z-index: 1000;
        box-shadow: 0 0 20px #ff0000aa;
        text-transform: uppercase;
        letter-spacing: 1px;
        animation: blink 1s infinite alternate;
        display: none;
    }
    @keyframes blink { from { opacity: 0.8; } to { opacity: 1; } }
</style>
</head><body>
<div class="container">
<h1>ENCLOSURE DASH</h1>
<div id="faultBanner">INTAKE FAULT! EMERGENCY COOLING ACTIVE</div>
<div class="card"><div class="grid">
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M20 7h-4V5l-2-2h-4L8 5v2H4c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V9c0-1.1-.9-2-2-2zm-8-2h4v2h-4V5zm8 14H4V9h16v10z"/></svg>Chamber</div><div class="value" id="chamber">--.-°C</div></div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M12 2c5.52 0 10 4.48 10 10s-4.48 10-10 10S2 17.52 2 12 6.48 2 12 2zm0 18c4.42 0 8-3.58 8-8s-3.58-8-8-8-8 3.58-8 8 3.58 8 8 8zm1-5h-2v-2h2v2zm0-4h-2V7h2v4z"/></svg>Intake</div><div class="value" id="intake">--.-°C</div></div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M15 13V5c0-1.66-1.34-3-3-3S9 3.34 9 5v8c-1.21.91-2 2.37-2 4 0 2.76 2.24 5 5 5s5-2.24 5-5c0-1.63-.79-3.09-2-4zm-3-8c.55 0 1 .45 1 1v4h-2V6c0-.55.45-1 1-1z"/></svg>Ambient</div><div class="value" id="ambient">--.-°C</div></div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/></svg>Humidity</div><div class="value" id="humidity">--%</div></div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M19.43 12.98c.04-.32.07-.64.07-.98 0-.34-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.09.98 0 .33.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"/></svg>Mode</div><div class="value" id="mode">---</div></div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M12 8c-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4-1.79-4-4-4zm8.94 3c-.46-4.17-3.77-7.48-7.94-7.94V2h-2v1.06C6.83 3.52 3.52 6.83 3.06 11H2v2h1.06c.46 4.17 3.77 7.48 7.94 7.94V22h2v-1.06c4.17-.46 7.48-3.77 7.94-7.94H22v-2h-1.06zM12 19c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg>Target</div><div class="value" id="target">---</div></div>
<div class="stat" id="fanStat">
  <div class="fan-wrapper" id="fanIcon">
    <div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div>
    <div class="fan-core"></div>
  </div>
  <div class="label">Fan</div>
  <div class="value" id="fanSpeed">0%</div>
</div>
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M3 13h2v-2H3v2zm4 8h2v-2H7v2zm8-12h-2V7h2v2zm-4 4h2v-2h-2v2zm8 4h-2v-2h2v2zM3 9h2V7H3v2zm8 8h2v-2h-2v2zm-4-4H9v-2h2v2zm8-8h-2V5h2v2z"/></svg>Vent</div><div class="value" id="vent">CLOSED</div></div>
</div></div>
<div class="card" id="cooldownCard" style="display:none"><h2 style="text-align:center;margin-bottom:1rem;color:var(--danger);text-shadow:0 0 10px #ff3b5c66">COOLING DOWN</h2>
<div class="progress-ring"><svg width="100" height="100"><defs><linearGradient id="gradient"><stop offset="0%" stop-color="#ff3b5c"/><stop offset="100%" stop-color="#9c27b0"/></linearGradient></defs><circle class="bg" r="40" cx="50" cy="50"/><circle class="fg" r="40" cx="50" cy="50" stroke-dasharray="251.3" stroke-dashoffset="251.3"/></svg></div>
<div class="time" id="timeLeft">Calculating...</div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-top:1rem"><div class="stat"><div class="label">Fan Speed</div><div class="value" id="fanInfo">---% (---- RPM)</div></div><div class="stat"><div class="label">Ambient</div><div class="value" id="ambInfo">--.-°C --%</div></div></div></div>
<div class="card"><h3 style="margin-bottom:.8rem;color:var(--accent);display:flex;align-items:center;gap:8px">LIVE CAMERA</h3><div class="cam-wrapper" id="camWrapper"><iframe id="camFrame" src="http://3d-print-live.local/" title="Live Feed"></iframe></div></div>
<button class="btn" id="cooldownBtn">START COOLDOWN</button>
</div>
<script>
const camBase="http://3d-print-live.local";
async function update(){
  try{
    const r=await fetch('/status'),d=await r.json();
    document.getElementById('chamber').textContent = (d.chamberTemp !== null && d.chamberTemp !== undefined) ? d.chamberTemp.toFixed(1) + '°C' : '--.-°C';
    document.getElementById('intake').textContent = (d.intakeTemp !== null && d.intakeTemp !== undefined) ? d.intakeTemp.toFixed(1) + '°C' : '--.-°C';
    document.getElementById('ambient').textContent = (d.ambientTemp !== null && d.ambientTemp !== undefined) ? d.ambientTemp.toFixed(1) + '°C' : '--.-°C';
    document.getElementById('humidity').textContent = (d.ambientHum !== null && d.ambientHum !== undefined) ? d.ambientHum + '%' : '--%';
    document.getElementById('mode').textContent = d.activeMode || '---';
    document.getElementById('target').textContent = d.targetTemp ? (d.targetTemp + '°C') : '---';
    document.getElementById('fanSpeed').textContent = (d.fanDuty !== undefined) ? d.fanDuty + '%' : '0%';
    document.getElementById('vent').textContent = d.ventState || 'CLOSED';
    const newFan = d.fanDuty || 0;
    const fanIcon = document.getElementById('fanIcon');
    if (newFan > 5) {
      const speed = (100 - newFan) * 0.02 + 0.2;
      fanIcon.style.animationDuration = speed + 's';
      fanIcon.classList.add('spinning');
    } else {
      fanIcon.classList.remove('spinning');
    }
    const faultBanner = document.getElementById('faultBanner');
    if (d.fault && d.fault !== "NONE") {
      faultBanner.style.display = 'block';
    } else {
      faultBanner.style.display = 'none';
    }
    const cooling = (d.activeMode === 'COOLDOWN');
    document.getElementById('cooldownCard').style.display = cooling ? 'block' : 'none';
    document.getElementById('cooldownBtn').style.display = cooling ? 'none' : 'block';
    if (cooling) {
      const prog = Math.min(Math.max(d.progress || 0, 0), 1);
      const circ = 251.3;
      document.querySelector('.fg').style.strokeDashoffset = circ * (1 - prog);
      document.getElementById('fanInfo').textContent = `${d.fanDuty}% (${d.fanRpm || 0} RPM)`;
      document.getElementById('ambInfo').textContent = `${d.ambientTemp ? d.ambientTemp.toFixed(1) : '--.-'}°C ${d.ambientHum || 0}%`;
      const s = d.estSeconds;
      let timeText;
      if (s === 0) {
        timeText = 'Cooled';
      } else if (s === undefined || s === null || s < 0) {
        timeText = 'Calculating...';
      } else if (s >= 3600) {
        timeText = `${Math.floor(s/3600)}h ${Math.floor((s%3600)/60)}m`;
      } else if (s >= 120) {
        timeText = `${Math.floor(s/60)}m`;
      } else {
        timeText = `${Math.floor(s/60)}m${String(s%60).padStart(2,'0')}s`;
      }
      document.getElementById('timeLeft').textContent = timeText;
    }
  } catch(e) {
    console.error(e);
  }
}
document.getElementById('cooldownBtn').onclick = async() => {
  await fetch('/start_cooldown', {method:'POST'});
  update();
};
const camWrapper = document.getElementById('camWrapper');
async function initCam(){
  try{
    const r = await fetch(`${camBase}/status`);
    if(!r.ok) throw 0;
    const d = await r.json();
    const w = d.framesize_width || 1280, h = d.framesize_height || 1024;
    camWrapper.style.paddingTop = (h/w*100) + '%';
  } catch {
    camWrapper.style.paddingTop = '56.25%';
  }
}
window.addEventListener('load', () => {
  update();
  initCam();
  setInterval(update, 1000);
});
</script>
</body></html>)=====";
  server.send(200, "text/html", html);
}

void handleStatus() {
  String r = "{";
  r += "\"chamberTemp\":"; r += isnan(chamberTemp) ? "null" : String(chamberTemp, 1);
  r += ",\"intakeTemp\":"; r += isnan(intakeTemp) ? "null" : String(intakeTemp, 1);
  r += ",\"ambientTemp\":"; r += isnan(ambientTemp) ? "null" : String(ambientTemp, 1);
  r += ",\"ambientHum\":"; r += isnan(ambientHum) ? "null" : String((int)ambientHum);
  r += ",\"fanRpm\":"; r += (int)currentFanRPM;
  r += ",\"fanDuty\":"; r += (fanDutyCycle * 100) / 255;
  r += ",\"activeMode\":\""; r += (activeMode >= 0 && activeMode < MENU_LEN) ? menuItems[activeMode] : "UNKNOWN";
  r += "\",\"targetTemp\":\""; r += (activeMode == 6) ? "COOLDOWN" : String(activeMode == 5 ? customTarget : menuTargets[activeMode], 1);
  r += "\",\"ventState\":\""; switch(ventState){case VENT_CLOSED:r+="CLOSED";break;case VENT_HALF_OPEN:r+="HALF OPEN";break;case VENT_OPEN:r+="OPEN";break;default:r+="MOVING";}
  r += "\",\"fault\":\""; r += intakeFault ? "INTAKE HIGH" : "NONE";
  r += "\",\"estSeconds\":"; r += (activeMode == 6) ? String(cooldownEstSeconds) : "-1";
  r += ",\"progress\":"; r += String(cooldownProgress, 3);
  r += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", r);
}

void setupWiFiAndServer() {
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    if (!MDNS.begin(MDNS_HOSTNAME)) {
      #ifdef DEBUG
      Serial.println("Error setting up MDNS responder!");
      #endif
    }
    MDNS.addService("http", "tcp", 80);
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/start_cooldown", HTTP_POST, handleStartCooldown);
    server.on("/start_cooldown", HTTP_GET, handleStartCooldown);
    server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });
    server.begin();
    #ifdef DEBUG
    Serial.println("HTTP server started at http://enclosure-monitor.local");
    #endif
  }
}

void serviceWebClient() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}

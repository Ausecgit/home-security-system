#!/usr/bin/env python3
import os, json, time, threading
from flask import Flask, request, jsonify, send_from_directory, render_template_string
import paho.mqtt.client as mqtt

STILL_DIR = "/opt/statusboard/stills"
os.makedirs(STILL_DIR, exist_ok=True)

app = Flask(__name__)
events = []
events_lock = threading.Lock()
statuses = {}
statuses_lock = threading.Lock()
MAX_EVENTS = 100
MAX_STILLS = 200

mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

def on_mqtt_connect(c, u, f, rc, props=None):
    c.subscribe("home/cam/+/motion")
    c.subscribe("home/cam/+/status")
def on_mqtt_message(c, u, msg):
    try:
        data = json.loads(msg.payload.decode())
        data["received"] = time.time()
        if msg.topic.endswith("/status"):
            cam = data.get("cam", "?")
            with statuses_lock:
                statuses[cam] = data
        else:
            with events_lock:
                events.insert(0, data)
                del events[MAX_EVENTS:]
    except Exception as e:
        print(f"mqtt err: {e}")

mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

def find_still_for_event(cam, frame_id):
    prefix = f"{cam}_{frame_id}_"
    for f in os.listdir(STILL_DIR):
        if f.startswith(prefix):
            return f
    return None

DASHBOARD = '''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="5">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Security Operations Center</title>
<style>
:root {
  --bg: #0a0e14;
  --panel: #11161f;
  --panel-hi: #161c28;
  --border: #1e2738;
  --text: #c5cdd9;
  --text-dim: #5c6778;
  --accent: #3b82f6;
  --green: #22c55e;
  --yellow: #eab308;
  --red: #ef4444;
  --orange: #f97316;
}
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, "Segoe UI", system-ui, sans-serif;
  background: var(--bg);
  color: var(--text);
  min-height: 100vh;
  padding: 16px;
}
.header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 20px; background: var(--panel);
  border: 1px solid var(--border); border-radius: 10px; margin-bottom: 16px;
}
.header-left { display: flex; align-items: center; gap: 14px; }
.logo {
  width: 36px; height: 36px;
  background: linear-gradient(135deg, var(--accent), #6366f1);
  border-radius: 8px; display: flex; align-items: center;
  justify-content: center; font-size: 18px; font-weight: 800; color: #fff;
}
.header h1 { font-size: 18px; font-weight: 700; letter-spacing: 0.5px; color: #fff; }
.header h1 .sub { font-size: 12px; font-weight: 400; color: var(--text-dim); display: block; }
.header-right { display: flex; gap: 20px; align-items: center; }
.stat-pill { display: flex; align-items: center; gap: 8px; font-size: 13px; color: var(--text-dim); }
.stat-pill .val { color: #fff; font-weight: 600; font-variant-numeric: tabular-nums; }
.dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.dot.green { background: var(--green); box-shadow: 0 0 8px var(--green); animation: pulse 2s infinite; }
.dot.red { background: var(--red); box-shadow: 0 0 8px var(--red); }
.dot.gray { background: var(--text-dim); }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.4; } }

.cam-bar { display: flex; gap: 12px; margin-bottom: 16px; flex-wrap: wrap; }
.cam-card {
  background: var(--panel); border: 1px solid var(--border);
  border-radius: 10px; padding: 10px 14px; display: flex;
  align-items: center; gap: 12px; flex: 1; min-width: 200px;
}
.cam-icon {
  width: 32px; height: 32px; border-radius: 6px; background: var(--border);
  display: flex; align-items: center; justify-content: center; font-size: 16px;
}
.cam-info { display: flex; flex-direction: column; }
.cam-name { font-size: 14px; font-weight: 600; color: #fff; }
.cam-meta { font-size: 11px; color: var(--text-dim); }
.cam-tag {
  font-size: 10px; font-weight: 600; text-transform: uppercase;
  letter-spacing: 0.5px; padding: 3px 8px; border-radius: 4px; margin-left: auto;
}
.tag-online { background: rgba(34,197,94,0.15); color: var(--green); }

.panel {
  background: var(--panel); border: 1px solid var(--border);
  border-radius: 10px; overflow: hidden;
}
.panel-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 16px; border-bottom: 1px solid var(--border);
  background: var(--panel-hi);
}
.panel-header h2 {
  font-size: 13px; font-weight: 600; text-transform: uppercase;
  letter-spacing: 1px; color: var(--text-dim);
}
.panel-header .badge {
  font-size: 11px; padding: 2px 10px; border-radius: 20px;
  background: var(--border); color: var(--text); font-weight: 600;
}

.event-table { width: 100%; border-collapse: collapse; }
.event-table th {
  text-align: left; padding: 8px 12px;
  font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px;
  color: var(--text-dim); font-weight: 600;
  border-bottom: 1px solid var(--border);
}
.event-table td {
  padding: 6px 12px; font-size: 13px;
  border-bottom: 1px solid rgba(30,39,56,0.5);
  font-variant-numeric: tabular-nums;
  vertical-align: middle;
}
.event-table tr:hover { background: var(--panel-hi); }
.event-table .time { color: var(--text-dim); }
.sev {
  display: inline-block; padding: 2px 10px; border-radius: 4px;
  font-size: 11px; font-weight: 600; text-transform: uppercase;
  letter-spacing: 0.5px;
}
.sev-small { background: rgba(34,197,94,0.12); color: var(--green); }
.sev-medium { background: rgba(234,179,8,0.12); color: var(--yellow); }
.sev-large { background: rgba(239,68,68,0.12); color: var(--red); }
.empty-row { text-align: center; padding: 30px; color: var(--text-dim); font-size: 13px; }

.thumb {
  width: 80px; height: 60px; border-radius: 4px; overflow: hidden;
  border: 1px solid var(--border); cursor: pointer; background: var(--panel-hi);
}
.thumb img { width: 100%; height: 100%; object-fit: cover; transition: transform 0.2s; }
.thumb:hover img { transform: scale(1.1); }
.thumb-empty { display: flex; align-items: center; justify-content: center; color: var(--text-dim); font-size: 10px; }

.modal {
  display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%;
  background: rgba(0,0,0,0.85); z-index: 1000;
  align-items: center; justify-content: center;
  flex-direction: column; gap: 12px;
}
.modal.show { display: flex; }
.modal img { max-width: 90%; max-height: 80vh; border-radius: 8px; border: 1px solid var(--border); }
.modal-info { color: var(--text); font-size: 13px; font-variant-numeric: tabular-nums; }
.modal-close {
  position: absolute; top: 20px; right: 30px;
  font-size: 28px; color: #fff; cursor: pointer;
}

.clock { font-size: 22px; font-weight: 700; color: #fff; font-variant-numeric: tabular-nums; }
.clock-date { font-size: 12px; color: var(--text-dim); text-align: right; }
</style>
</head>
<body>

<div class="header">
  <div class="header-left">
    <div class="logo">S</div>
    <h1>Security Operations Center<span class="sub">Home Security System &middot; DonsTest Network</span></h1>
  </div>
  <div class="header-right">
    <div class="stat-pill"><span class="dot {{ 'green' if mqtt_status == 'connected' else 'red' }}"></span> MQTT <span class="val">{{mqtt_status}}</span></div>
    <div class="stat-pill">Uptime <span class="val">{{uptime}}</span></div>
    <div class="stat-pill">Events <span class="val">{{event_count}}</span></div>
    <div>
      <div class="clock">{{clock}}</div>
      <div class="clock-date">{{date}}</div>
    </div>
  </div>
</div>

<div class="cam-bar">
  {% for cam, s in statuses.items() %}
  <div class="cam-card">
    <div class="cam-icon">&#128247;</div>
    <div class="cam-info">
      <div class="cam-name">{{cam}}</div>
      <div class="cam-meta">Frame {{s.frames}} &middot; {{s.heap}}B free</div>
    </div>
    <div class="cam-tag tag-online">ONLINE</div>
  </div>
  {% endfor %}
  {% if not statuses %}
  <div class="cam-card"><div class="cam-info"><div class="cam-name">No cameras</div><div class="cam-meta">Waiting for connections...</div></div></div>
  {% endif %}
</div>

<div class="panel">
  <div class="panel-header">
    <h2>Event Log</h2>
    <span class="badge">{{event_count}} events &middot; {{still_count}} stills</span>
  </div>
  <table class="event-table">
    <thead>
      <tr>
        <th>Thumbnail</th>
        <th>Time</th>
        <th>Camera</th>
        <th>Severity</th>
        <th>Area</th>
        <th>Position</th>
        <th>Frame</th>
      </tr>
    </thead>
    <tbody>
      {% for e in events %}
      <tr>
        <td>
          {% if e.still %}
          <div class="thumb" onclick="openModal('{{e.still}}', '{{e.cam}} frame {{e.frame}}')">
            <img src="/stills/{{e.still}}" loading="lazy">
          </div>
          {% else %}
          <div class="thumb thumb-empty">no image</div>
          {% endif %}
        </td>
        <td class="time">{{e.received_t}}</td>
        <td>{{e.cam}}</td>
        <td><span class="sev sev-{{e.class}}">{{e.class}}</span></td>
        <td>{{"{:,}".format(e.area)}}</td>
        <td>{{e.cx}},{{e.cy}}</td>
        <td>{{e.frame}}</td>
      </tr>
      {% endfor %}
      {% if not events %}
      <tr><td colspan="7" class="empty-row">No motion events detected</td></tr>
      {% endif %}
    </tbody>
  </table>
</div>

<div class="modal" id="modal" onclick="closeModal()">
  <span class="modal-close" onclick="closeModal()">&times;</span>
  <img id="modalImg" src="">
  <div class="modal-info" id="modalInfo"></div>
</div>

<script>
function openModal(src, info) {
  document.getElementById('modalImg').src = '/stills/' + src;
  document.getElementById('modalInfo').textContent = info;
  document.getElementById('modal').classList.add('show');
}
function closeModal() {
  document.getElementById('modal').classList.remove('show');
}
document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') closeModal();
});
</script>

</body>
</html>'''

@app.route("/")
def dashboard():
    with events_lock:
        evs = list(events)
    for e in evs:
        e["received_t"] = time.strftime("%H:%M:%S", time.localtime(e.get("received", 0)))
        e["still"] = find_still_for_event(e.get("cam", ""), e.get("frame", 0))
    with statuses_lock:
        sts = dict(statuses)
    still_count = len(os.listdir(STILL_DIR))
    uptime_s = int(time.time() - START_TIME)
    h, rem = divmod(uptime_s, 3600)
    m, s = divmod(rem, 60)
    now = time.localtime()
    return render_template_string(DASHBOARD, events=evs,
        event_count=len(evs), still_count=still_count,
        statuses=sts, cam_count=len(sts),
        uptime=f"{h}h {m}m {s}s",
        clock=time.strftime("%H:%M:%S", now),
        date=time.strftime("%a %b %d", now),
        mqtt_status="connected" if mqtt_client.is_connected() else "disconnected")

@app.route("/still", methods=["POST"])
def receive_still():
    cam = request.headers.get("X-Cam-Id", "unknown")
    fid = request.headers.get("X-Frame-Id", "0")
    data = request.get_data()
    if not data:
        return "no data", 400
    fname = f"{cam}_{fid}_{int(time.time())}.jpg"
    path = os.path.join(STILL_DIR, fname)
    with open(path, "wb") as f:
        f.write(data)
    files = os.listdir(STILL_DIR)
    if len(files) > MAX_STILLS:
        old = sorted(files, key=lambda f: os.path.getmtime(os.path.join(STILL_DIR, f)))
        for f in old[:len(old)-MAX_STILLS]:
            try: os.remove(os.path.join(STILL_DIR, f))
            except: pass
    return jsonify({"saved": fname}), 200

@app.route("/stills/<filename>")
def serve_still(filename):
    return send_from_directory(STILL_DIR, filename)

START_TIME = time.time()
mqtt_thread_started = False
def start_mqtt():
    global mqtt_thread_started
    if mqtt_thread_started: return
    mqtt_thread_started = True
    mqtt_client.connect("127.0.0.1", 1883, 60)
    mqtt_client.loop_forever(retry_first_connection=True)

threading.Thread(target=start_mqtt, daemon=True).start()

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)

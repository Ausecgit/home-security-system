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
/* Header */
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 20px;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 10px;
  margin-bottom: 16px;
}
.header-left { display: flex; align-items: center; gap: 14px; }
.logo {
  width: 36px; height: 36px;
  background: linear-gradient(135deg, var(--accent), #6366f1);
  border-radius: 8px;
  display: flex; align-items: center; justify-content: center;
  font-size: 18px; font-weight: 800; color: #fff;
}
.header h1 {
  font-size: 18px; font-weight: 700; letter-spacing: 0.5px;
  color: #fff;
}
.header h1 .sub { font-size: 12px; font-weight: 400; color: var(--text-dim); display: block; }
.header-right { display: flex; gap: 20px; align-items: center; }
.stat-pill {
  display: flex; align-items: center; gap: 8px;
  font-size: 13px; color: var(--text-dim);
}
.stat-pill .val { color: #fff; font-weight: 600; font-variant-numeric: tabular-nums; }
.dot {
  width: 8px; height: 8px; border-radius: 50%;
  display: inline-block;
}
.dot.green { background: var(--green); box-shadow: 0 0 8px var(--green); animation: pulse 2s infinite; }
.dot.red { background: var(--red); box-shadow: 0 0 8px var(--red); }
.dot.gray { background: var(--text-dim); }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.4; } }

/* Grid layout */
.grid {
  display: grid;
  grid-template-columns: 1fr 1.5fr;
  gap: 16px;
  margin-bottom: 16px;
}
@media (max-width: 900px) { .grid { grid-template-columns: 1fr; } }

/* Panels */
.panel {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 10px;
  overflow: hidden;
}
.panel-header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 16px;
  border-bottom: 1px solid var(--border);
  background: var(--panel-hi);
}
.panel-header h2 {
  font-size: 13px; font-weight: 600; text-transform: uppercase;
  letter-spacing: 1px; color: var(--text-dim);
}
.panel-header .badge {
  font-size: 11px; padding: 2px 10px; border-radius: 20px;
  background: var(--border); color: var(--text);
  font-weight: 600;
}
.panel-body { padding: 14px; }

/* Camera cards */
.cam-grid { display: flex; flex-direction: column; gap: 10px; }
.cam-card {
  background: var(--panel-hi);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 12px 14px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.cam-card-left { display: flex; align-items: center; gap: 12px; }
.cam-icon {
  width: 32px; height: 32px; border-radius: 6px;
  background: var(--border);
  display: flex; align-items: center; justify-content: center;
  font-size: 16px;
}
.cam-info { display: flex; flex-direction: column; }
.cam-name { font-size: 14px; font-weight: 600; color: #fff; }
.cam-meta { font-size: 11px; color: var(--text-dim); }
.cam-status-tag {
  font-size: 11px; font-weight: 600; text-transform: uppercase;
  letter-spacing: 0.5px; padding: 3px 10px; border-radius: 4px;
}
.tag-online { background: rgba(34,197,94,0.15); color: var(--green); }
.tag-offline { background: rgba(92,103,120,0.15); color: var(--text-dim); }

/* Event table */
.event-table { width: 100%; border-collapse: collapse; }
.event-table th {
  text-align: left; padding: 8px 12px;
  font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px;
  color: var(--text-dim); font-weight: 600;
  border-bottom: 1px solid var(--border);
}
.event-table td {
  padding: 8px 12px; font-size: 13px;
  border-bottom: 1px solid rgba(30,39,56,0.5);
  font-variant-numeric: tabular-nums;
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

/* Stills gallery */
.stills-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 10px;
}
.still-item {
  position: relative;
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid var(--border);
  aspect-ratio: 4/3;
  background: var(--panel-hi);
}
.still-item img {
  width: 100%; height: 100%; object-fit: cover;
  transition: transform 0.2s;
}
.still-item:hover img { transform: scale(1.05); }
.still-item .still-label {
  position: absolute; bottom: 0; left: 0; right: 0;
  padding: 4px 8px;
  background: linear-gradient(transparent, rgba(0,0,0,0.8));
  font-size: 10px; color: #fff;
  font-variant-numeric: tabular-nums;
}

/* Clock */
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

<div class="grid">
  <!-- Camera Status Panel -->
  <div class="panel">
    <div class="panel-header">
      <h2>Cameras</h2>
      <span class="badge">{{cam_count}} active</span>
    </div>
    <div class="panel-body">
      <div class="cam-grid">
        {% for cam, s in statuses.items() %}
        <div class="cam-card">
          <div class="cam-card-left">
            <div class="cam-icon">&#128247;</div>
            <div class="cam-info">
              <div class="cam-name">{{cam}}</div>
              <div class="cam-meta">Frame {{s.frames}} &middot; {{s.heap}} bytes free</div>
            </div>
          </div>
          <div class="cam-status-tag tag-online">ONLINE</div>
        </div>
        {% endfor %}
        {% if not statuses %}
        <div style="color:var(--text-dim);font-size:13px;padding:10px;">No cameras reporting</div>
        {% endif %}
      </div>
    </div>
  </div>

  <!-- Event Log Panel -->
  <div class="panel">
    <div class="panel-header">
      <h2>Event Log</h2>
      <span class="badge">{{event_count}} events</span>
    </div>
    <div class="panel-body" style="padding:0;">
      <table class="event-table">
        <thead>
          <tr>
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
            <td class="time">{{e.received_t}}</td>
            <td>{{e.cam}}</td>
            <td><span class="sev sev-{{e.class}}">{{e.class}}</span></td>
            <td>{{"{:,}".format(e.area)}}</td>
            <td>{{e.cx}},{{e.cy}}</td>
            <td>{{e.frame}}</td>
          </tr>
          {% endfor %}
          {% if not events %}
          <tr><td colspan="6" class="empty-row">No motion events detected</td></tr>
          {% endif %}
        </tbody>
      </table>
    </div>
  </div>
</div>

<!-- Stills Gallery -->
<div class="panel">
  <div class="panel-header">
    <h2>Captured Stills</h2>
    <span class="badge">{{still_count}} stored</span>
  </div>
  <div class="panel-body">
    <div class="stills-grid">
      {% for s in stills %}
      <div class="still-item">
        <img src="/stills/{{s}}" loading="lazy" title="{{s}}">
        <div class="still-label">{{s}}</div>
      </div>
      {% endfor %}
      {% if not stills %}
      <div style="color:var(--text-dim);font-size:13px;padding:10px;">No stills captured</div>
      {% endif %}
    </div>
  </div>
</div>

</body>
</html>'''

@app.route("/")
def dashboard():
    with events_lock:
        evs = list(events)
    for e in evs:
        e["received_t"] = time.strftime("%H:%M:%S", time.localtime(e.get("received", 0)))
    with statuses_lock:
        sts = dict(statuses)
    stills = sorted(os.listdir(STILL_DIR), reverse=True)[:12]
    still_count = len(os.listdir(STILL_DIR))
    uptime_s = int(time.time() - START_TIME)
    h, rem = divmod(uptime_s, 3600)
    m, s = divmod(rem, 60)
    now = time.localtime()
    return render_template_string(DASHBOARD, events=evs,
        event_count=len(evs), stills=stills, still_count=still_count,
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
    old = sorted(os.listdir(STILL_DIR))
    if len(old) > 200:
        for f in old[:len(old)-200]:
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

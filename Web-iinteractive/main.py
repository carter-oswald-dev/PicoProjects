"""
main.py  –  Pico 2 W AM Radio — standalone web controller.

On boot:
  1. Opens a password-free WiFi access point  "PicoAMRadio"
  2. Serves a web UI at http://192.168.4.1
  3. UI lets you pick a MIDI file, set carrier frequency, start/stop.

No PC needed — works from any USB power source (power bank, phone charger, etc.).
Connect to the "PicoAMRadio" WiFi network, then open a browser to 192.168.4.1.
"""

import network
import socket
import os
import time
import radio  # radio.py on the Pico filesystem

# ---------------------------------------------------------------------------
# Access point config
# ---------------------------------------------------------------------------
AP_SSID     = "PicoAMRadio"
AP_PASSWORD = ""              # no password — open network
AP_IP       = "192.168.4.1"

# ---------------------------------------------------------------------------
# Start the WiFi access point
# ---------------------------------------------------------------------------
def start_ap():
    ap = network.WLAN(network.AP_IF)
    ap.active(True)
    ap.config(ssid=AP_SSID, password=AP_PASSWORD, security=0)  # security=0 → open
    # Give it a moment to come up
    for _ in range(20):
        if ap.active():
            break
        time.sleep(0.1)
    print(f"AP started: {AP_SSID}  —  connect and open http://{AP_IP}")
    return ap


# ---------------------------------------------------------------------------
# List MIDI files on the filesystem
# ---------------------------------------------------------------------------
def list_midi_files():
    try:
        return sorted(f for f in os.listdir("/") if f.lower().endswith(".mid"))
    except:
        return []


# ---------------------------------------------------------------------------
# HTML page
# ---------------------------------------------------------------------------
def build_page(status_msg="", error_msg=""):
    midi_files = list_midi_files()
    freq_hz    = radio.get_carrier_freq()
    freq_khz   = freq_hz / 1000
    playing    = radio.is_playing()

    # Build file selector options
    options_html = ""
    if midi_files:
        for f in midi_files:
            options_html += f'<option value="{f}">{f}</option>\n'
    else:
        options_html = '<option disabled>No .mid files found on Pico</option>'

    # Playing state indicator
    state_text  = "TRANSMITTING" if playing else "STOPPED"
    state_color = "#22c55e" if playing else "#94a3b8"

    start_disabled = "disabled" if playing or not midi_files else ""
    stop_disabled  = "disabled" if not playing else ""
    freq_disabled  = "disabled" if playing else ""

    status_block = f'<p class="status-msg">{status_msg}</p>' if status_msg else ""
    error_block  = f'<p class="error-msg">{error_msg}</p>'  if error_msg  else ""

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Pico AM Radio</title>
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    background: #0f172a; color: #e2e8f0;
    font-family: system-ui, sans-serif;
    display: flex; justify-content: center; align-items: flex-start;
    min-height: 100vh; padding: 2rem 1rem;
  }}
  .card {{
    background: #1e293b; border-radius: 1rem;
    padding: 2rem; width: 100%; max-width: 480px;
    box-shadow: 0 8px 32px rgba(0,0,0,0.4);
  }}
  h1 {{ font-size: 1.4rem; font-weight: 700; margin-bottom: 0.25rem; }}
  .subtitle {{ color: #64748b; font-size: 0.85rem; margin-bottom: 1.5rem; }}
  .state-badge {{
    display: inline-block; padding: 0.3rem 0.9rem;
    border-radius: 999px; font-weight: 700; font-size: 0.8rem;
    background: #0f172a; color: {state_color};
    border: 2px solid {state_color}; margin-bottom: 1.5rem;
    letter-spacing: 0.05em;
  }}
  .section {{ margin-bottom: 1.5rem; }}
  label {{ display: block; font-size: 0.8rem; color: #94a3b8; margin-bottom: 0.4rem; }}
  select, input[type=number] {{
    width: 100%; padding: 0.6rem 0.8rem;
    background: #0f172a; color: #e2e8f0;
    border: 1px solid #334155; border-radius: 0.5rem;
    font-size: 1rem;
  }}
  select:disabled, input:disabled {{ opacity: 0.45; cursor: not-allowed; }}
  .btn-row {{ display: flex; gap: 0.75rem; margin-top: 0.5rem; }}
  button {{
    flex: 1; padding: 0.7rem;
    border: none; border-radius: 0.5rem;
    font-size: 1rem; font-weight: 600; cursor: pointer;
  }}
  .btn-start {{ background: #22c55e; color: #052e16; }}
  .btn-start:disabled {{ background: #166534; color: #4ade80; opacity: 0.5; cursor: not-allowed; }}
  .btn-stop  {{ background: #ef4444; color: #fff; }}
  .btn-stop:disabled  {{ background: #7f1d1d; color: #fca5a5; opacity: 0.5; cursor: not-allowed; }}
  .freq-row {{ display: flex; gap: 0.5rem; align-items: flex-end; }}
  .freq-row input {{ flex: 1; }}
  .btn-set {{
    padding: 0.6rem 1rem; background: #3b82f6; color: #fff;
    border-radius: 0.5rem; border: none; font-weight: 600; cursor: pointer;
  }}
  .btn-set:disabled {{ background: #1e3a5f; color: #93c5fd; opacity: 0.5; cursor: not-allowed; }}
  .freq-hint {{ font-size: 0.75rem; color: #64748b; margin-top: 0.35rem; }}
  .warn-box {{
    background: #451a03; border: 1px solid #92400e;
    border-radius: 0.5rem; padding: 0.75rem 1rem;
    font-size: 0.8rem; color: #fcd34d; margin-bottom: 1.5rem;
    line-height: 1.5;
  }}
  .status-msg {{ color: #4ade80; font-size: 0.85rem; margin-top: 0.5rem; }}
  .error-msg  {{ color: #f87171; font-size: 0.85rem; margin-top: 0.5rem; }}
  .divider {{ border: none; border-top: 1px solid #1e3a5f; margin: 1.25rem 0; }}
</style>
</head>
<body>
<div class="card">
  <h1>📻 Pico AM Radio</h1>
  <p class="subtitle">Standalone MIDI transmitter &mdash; GP0 antenna</p>
  <div class="state-badge">{state_text}</div>

  {'<div class="warn-box">⚠ <strong>Stop transmission before changing frequency.</strong> The carrier frequency cannot be changed mid-transmission — stop first, then adjust, then start again.</div>' if playing else ''}

  <form method="POST" action="/play">
    <div class="section">
      <label for="midifile">MIDI File</label>
      <select name="filename" id="midifile" {start_disabled}>
        {options_html}
      </select>
    </div>
    <div class="btn-row">
      <button class="btn-start" type="submit" {start_disabled}>▶ Start</button>
    </div>
    {status_block}{error_block}
  </form>

  <hr class="divider">

  <form method="POST" action="/stop">
    <div class="btn-row">
      <button class="btn-stop" type="submit" {stop_disabled}>■ Stop</button>
    </div>
  </form>

  <hr class="divider">

  <form method="POST" action="/setfreq">
    <div class="section">
      <label for="freqinput">Carrier Frequency (kHz)</label>
      <div class="freq-row">
        <input type="number" id="freqinput" name="freq_khz"
               value="{freq_khz:.1f}"
               min="530" max="1700" step="1"
               {freq_disabled}>
        <button class="btn-set" type="submit" {freq_disabled}>Set</button>
      </div>
      <p class="freq-hint">
        Range: 530 kHz – 1700 kHz (AM broadcast band) &nbsp;|&nbsp; Current: <strong>{freq_khz:.1f} kHz</strong>
        &nbsp;({freq_hz/1_000_000:.3f} MHz)
        {'&nbsp;|&nbsp; <span style="color:#f87171">Stop transmission to change</span>' if playing else ''}
      </p>
    </div>
  </form>
</div>
</body>
</html>"""


# ---------------------------------------------------------------------------
# Minimal HTTP server (no framework, fits in RAM)
# ---------------------------------------------------------------------------
def parse_post_body(body_bytes):
    """Parse application/x-www-form-urlencoded body into a dict."""
    result = {}
    try:
        for pair in body_bytes.decode().split("&"):
            if "=" in pair:
                k, v = pair.split("=", 1)
                result[k] = v.replace("+", " ").replace("%2F", "/")
    except:
        pass
    return result

def send_response(conn, body, status="200 OK"):
    header = (
        f"HTTP/1.1 {status}\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        f"Content-Length: {len(body)}\r\n"
        "Connection: close\r\n\r\n"
    )
    conn.send(header.encode() + body.encode())

def send_redirect(conn, location="/"):
    header = (
        f"HTTP/1.1 302 Found\r\nLocation: {location}\r\nConnection: close\r\n\r\n"
    )
    conn.send(header.encode())


def serve():
    addr = socket.getaddrinfo("0.0.0.0", 80)[0][-1]
    srv  = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(addr)
    srv.listen(2)
    srv.settimeout(0.5)   # non-blocking poll so we don't hang on stop
    print(f"HTTP server listening on {AP_IP}:80")

    status_msg = ""
    error_msg  = ""

    while True:
        try:
            conn, addr = srv.accept()
        except OSError:
            continue  # timeout — loop again

        try:
            conn.settimeout(3)
            request = b""
            while b"\r\n\r\n" not in request:
                chunk = conn.recv(256)
                if not chunk:
                    break
                request += chunk

            # Read POST body if Content-Length is set
            body = b""
            if b"Content-Length:" in request:
                try:
                    cl_line = [l for l in request.split(b"\r\n") if b"Content-Length:" in l][0]
                    cl = int(cl_line.split(b":")[1].strip())
                    # body may have been partially included in the header read
                    header_end = request.index(b"\r\n\r\n") + 4
                    body = request[header_end:]
                    while len(body) < cl:
                        body += conn.recv(cl - len(body))
                except:
                    pass

            # Parse request line
            first_line = request.split(b"\r\n")[0].decode(errors="replace")
            parts = first_line.split(" ")
            method = parts[0] if parts else "GET"
            path   = parts[1] if len(parts) > 1 else "/"

            status_msg = ""
            error_msg  = ""

            # ---- Route: GET / ----
            if method == "GET" and path in ("/", "/index.html"):
                send_response(conn, build_page())

            # ---- Route: POST /play ----
            elif method == "POST" and path == "/play":
                params   = parse_post_body(body)
                filename = params.get("filename", "").strip()
                if radio.is_playing():
                    error_msg = "Already transmitting — stop first."
                    send_response(conn, build_page(error_msg=error_msg))
                elif not filename:
                    error_msg = "No file selected."
                    send_response(conn, build_page(error_msg=error_msg))
                else:
                    ok = radio.play(filename)
                    if ok:
                        time.sleep(0.15)  # let the thread start
                        send_redirect(conn)
                    else:
                        error_msg = "Could not start playback."
                        send_response(conn, build_page(error_msg=error_msg))

            # ---- Route: POST /stop ----
            elif method == "POST" and path == "/stop":
                radio.stop()
                time.sleep(0.3)   # let the thread wind down
                send_redirect(conn)

            # ---- Route: POST /setfreq ----
            elif method == "POST" and path == "/setfreq":
                if radio.is_playing():
                    error_msg = "Stop transmission before changing frequency."
                    send_response(conn, build_page(error_msg=error_msg))
                else:
                    params = parse_post_body(body)
                    try:
                        khz = float(params.get("freq_khz", "1000"))
                        hz  = int(khz * 1000)
                        radio.set_carrier_freq(hz)
                        send_redirect(conn)
                    except:
                        error_msg = "Invalid frequency value."
                        send_response(conn, build_page(error_msg=error_msg))

            # ---- 404 ----
            else:
                send_response(conn, "<h1>404</h1>", status="404 Not Found")

        except Exception as e:
            print("Request error:", e)
        finally:
            try:
                conn.close()
            except:
                pass


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
start_ap()
serve()

import asyncio
import json
import os
import sys
import time
from typing import Set
import serial
import serial.tools.list_ports
from starlette.applications import Starlette
from starlette.responses import JSONResponse, FileResponse
from starlette.routing import Route, WebSocketRoute
from starlette.staticfiles import StaticFiles
from starlette.websockets import WebSocket
import uvicorn

DEFAULT_PORT = "COM3"
DEFAULT_BAUD = 115200
HTTP_PORT = 8080

connected_clients: Set[WebSocket] = set()
ser_instance = None
active_port = DEFAULT_PORT
is_replaying = False

latest_state = {
    "status": "DISCONNECTED",
    "maneuver": "NONE",
    "dist_m": 0,
    "road": "--",
    "instr": "Waiting for connection...",
    "speed_kmh": 0,
    "rem_dist_m": 0,
    "rem_time_s": 0,
    "progress_pct": 0,
    "is_stale": False,
    "seq": 0,
    "lcd_w": 240,
    "lcd_h": 240
}

raw_logs = []

async def broadcast(message: dict):
    if not connected_clients:
        return
    text = json.dumps(message)
    dead = set()
    for ws in connected_clients:
        try:
            await ws.send_text(text)
        except Exception:
            dead.add(ws)
    connected_clients.difference_update(dead)

def parse_line(line: str):
    global latest_state
    line = line.strip()
    if not line:
        return None
    
    timestamp = int(time.time() * 1000)
    event_type = "RAW"
    payload = line

    if line.startswith("[RPMLOPP_EVT] "):
        event_type = "EVENT"
        try:
            data = json.loads(line[14:])
            payload = data
            if data.get("event") == "ACCEPTED_STATE" and "data" in data:
                latest_state = data["data"]
        except Exception as e:
            pass
    elif line.startswith("[RPMLOPP_ACK] "):
        event_type = "ACK"
        try:
            payload = json.loads(line[14:])
        except Exception:
            pass
    elif line.startswith("[RPMLOPP_LOG] "):
        event_type = "LOG"
        try:
            payload = json.loads(line[14:])
        except Exception:
            pass
    elif line.startswith("[RPMLOPP_ERR] "):
        event_type = "ERROR"
        try:
            payload = json.loads(line[14:])
        except Exception:
            pass

    record = {
        "ts": timestamp,
        "type": event_type,
        "raw": line,
        "payload": payload
    }
    raw_logs.append(record)
    if len(raw_logs) > 500:
        raw_logs.pop(0)
    return record

async def serial_reader_task():
    global ser_instance, active_port
    while True:
        try:
            if ser_instance is None or not ser_instance.is_open:
                try:
                    ser_instance = serial.Serial(active_port, DEFAULT_BAUD, timeout=0.1)
                    print(f"[*] Serial connected on {active_port} @ {DEFAULT_BAUD}")
                    await broadcast({
                        "type": "SERIAL_STATUS",
                        "connected": True,
                        "port": active_port
                    })
                except Exception as e:
                    await broadcast({
                        "type": "SERIAL_STATUS",
                        "connected": False,
                        "error": str(e),
                        "port": active_port
                    })
                    await asyncio.sleep(2)
                    continue

            if ser_instance and ser_instance.is_open:
                if ser_instance.in_waiting > 0:
                    raw_bytes = ser_instance.readline()
                    try:
                        line = raw_bytes.decode('utf-8', errors='replace')
                        parsed = parse_line(line)
                        if parsed:
                            await broadcast(parsed)
                    except Exception as err:
                        pass
                else:
                    await asyncio.sleep(0.02)
        except Exception as e:
            if ser_instance:
                try:
                    ser_instance.close()
                except Exception:
                    pass
                ser_instance = None
            await asyncio.sleep(1)

async def replay_file_task(filepath: str):
    global is_replaying
    if not os.path.exists(filepath):
        print(f"Replay file not found: {filepath}")
        return
    is_replaying = True
    print(f"[*] Starting replay of {filepath}")
    await broadcast({"type": "MODE_CHANGE", "mode": "RECORDED_REPLAY"})
    
    with open(filepath, "r", encoding="utf-8") as f:
        for raw_line in f:
            if not is_replaying:
                break
            try:
                item = json.loads(raw_line.strip())
                parsed = parse_line(item.get("line", ""))
                if parsed:
                    await broadcast(parsed)
                await asyncio.sleep(0.7)
            except Exception as e:
                pass
    is_replaying = False
    print("[*] Replay completed.")

async def websocket_endpoint(websocket: WebSocket):
    global active_port, ser_instance
    await websocket.accept()
    connected_clients.add(websocket)
    
    # Send initial snapshot
    await websocket.send_text(json.dumps({
        "type": "INIT_STATE",
        "latest_state": latest_state,
        "logs": raw_logs[-50:],
        "port": active_port,
        "serial_connected": ser_instance is not None and ser_instance.is_open
    }))

    try:
        while True:
            data_text = await websocket.receive_text()
            data = json.loads(data_text)
            cmd = data.get("action")
            if cmd == "SEND_SERIAL":
                text = data.get("text", "") + "\n"
                if ser_instance and ser_instance.is_open:
                    ser_instance.write(text.encode('utf-8'))
            elif cmd == "REPLAY":
                fixture_path = os.path.join(os.path.dirname(__file__), "..", "protocol", "fixtures", "replay_session.jsonl")
                asyncio.create_task(replay_file_task(fixture_path))
            elif cmd == "SET_PORT":
                active_port = data.get("port", DEFAULT_PORT)
                if ser_instance:
                    ser_instance.close()
                    ser_instance = None
            elif cmd == "SIMULATE":
                sim_state = data.get("state", {})
                parsed = parse_line(f"[RPMLOPP_EVT] {json.dumps({'event':'ACCEPTED_STATE', 'data': sim_state})}")
                if parsed:
                    await broadcast(parsed)
    except Exception as e:
        print(f"WS client error/disconnect: {e}")
    finally:
        connected_clients.discard(websocket)

async def api_ports(request):
    ports = [p.device for p in serial.tools.list_ports.comports()]
    return JSONResponse({"ports": ports, "current": active_port})

async def index_handler(request):
    public_index = os.path.join(os.path.dirname(__file__), "..", "dashboard", "public", "index.html")
    dist_index = os.path.join(os.path.dirname(__file__), "..", "dashboard", "dist", "index.html")
    if os.path.exists(dist_index):
        return FileResponse(dist_index)
    if os.path.exists(public_index):
        return FileResponse(public_index)
    return JSONResponse({"message": "RPMLopp Nav Dashboard HTML not found yet."})

async def download_apk(request):
    apk_path = os.path.join(os.path.dirname(__file__), "..", "android", "RPMLoopNav.apk")
    if os.path.exists(apk_path):
        return FileResponse(apk_path, media_type="application/vnd.android.package-archive", filename="RPMLoopNav.apk")
    return JSONResponse({"error": "APK not found. Build it with build.sh"}, status_code=404)

async def android_web_handler(request):
    android_html = os.path.join(os.path.dirname(__file__), "..", "android", "assets", "index.html")
    if os.path.exists(android_html):
        return FileResponse(android_html)
    return JSONResponse({"error": "Android Web companion not found."}, status_code=404)

routes = [
    Route("/", endpoint=index_handler),
    Route("/RPMLoopNav.apk", endpoint=download_apk),
    Route("/download/RPMLoopNav.apk", endpoint=download_apk),
    Route("/android", endpoint=android_web_handler),
    Route("/api/ports", endpoint=api_ports),
    WebSocketRoute("/ws", endpoint=websocket_endpoint)
]

app = Starlette(routes=routes)

async def run_server():
    config = uvicorn.Config(app, host="0.0.0.0", port=HTTP_PORT, log_level="warning")
    server = uvicorn.Server(config)
    asyncio.create_task(serial_reader_task())
    print(f"============================================================")
    print(f" RPMLopp Nav Desktop Bridge running on http://0.0.0.0:{HTTP_PORT}")
    print(f" Serial Port: {active_port} (auto-connecting)")
    print(f" WebSocket: ws://0.0.0.0:{HTTP_PORT}/ws")
    print(f"============================================================")
    await server.serve()

if __name__ == "__main__":
    try:
        asyncio.run(run_server())
    except KeyboardInterrupt:
        print("\nShutdown complete.")

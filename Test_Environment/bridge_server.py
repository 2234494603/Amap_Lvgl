"""Phone-navigation simulator bridge for the GaoDe_Map LVGL display.

Serves the Test_Environment web app and accepts compact route updates from the
browser. Every accepted update is atomically written in a small binary format
that the Windows LVGL simulator polls without requiring an HTTP dependency.
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import tempfile
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent
BRIDGE_DIR = ROOT / "bridge"
ROUTE_BIN = BRIDGE_DIR / "latest_route.bin"
ROUTE_JSON = BRIDGE_DIR / "latest_route.json"
MAX_BODY_BYTES = 128 * 1024
MAX_POINTS = 64
WRITE_LOCK = threading.Lock()


def _integer(value: Any, name: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{name} 不能是布尔值")
    try:
        result = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} 必须是整数") from exc
    if result < minimum or result > maximum:
        raise ValueError(f"{name} 必须在 {minimum}..{maximum} 范围内")
    return result


def validate_route(payload: Any) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise ValueError("请求正文必须是 JSON 对象")

    raw_points = payload.get("points")
    if not isinstance(raw_points, list) or not 2 <= len(raw_points) <= MAX_POINTS:
        raise ValueError(f"points 数量必须在 2..{MAX_POINTS} 之间")

    points: list[list[int]] = []
    for index, point in enumerate(raw_points):
        if not isinstance(point, list) or len(point) != 2:
            raise ValueError(f"points[{index}] 必须是 [x, y]")
        points.append([
            _integer(point[0], f"points[{index}].x", 0, 455),
            _integer(point[1], f"points[{index}].y", 0, 247),
        ])

    maneuver = payload.get("maneuver")
    if not isinstance(maneuver, dict):
        maneuver = {}

    return {
        "route_revision": _integer(payload.get("route_revision", 1), "route_revision", 0, 0xFFFFFFFF),
        "sequence": _integer(payload.get("sequence", 0), "sequence", 0, 0xFFFFFFFF),
        "distance_m": _integer(payload.get("distance_m", 0), "distance_m", 0, 0xFFFFFFFF),
        "duration_s": _integer(payload.get("duration_s", 0), "duration_s", 0, 0xFFFFFFFF),
        "progress_per_mille": _integer(
            payload.get("progress_per_mille", 0), "progress_per_mille", 0, 1000
        ),
        "points": points,
        "maneuver": {
            "type": str(maneuver.get("type", ""))[:32],
            "road": str(maneuver.get("road", ""))[:96],
            "distance_m": _integer(maneuver.get("distance_m", 0), "maneuver.distance_m", 0, 0xFFFFFFFF),
        },
    }


def encode_packet(route: dict[str, Any]) -> bytes:
    points = route["points"]
    header = struct.pack(
        "<4sBBHIIIIHH",
        b"GDRT",
        1,
        1,
        len(points),
        route["route_revision"],
        route["sequence"],
        route["distance_m"],
        route["duration_s"],
        route["progress_per_mille"],
        0,
    )
    return header + b"".join(struct.pack("<HH", x, y) for x, y in points)


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except Exception:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


class BridgeHandler(SimpleHTTPRequestHandler):
    server_version = "GaoDePhoneBridge/1.0"

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def send_json(self, status: int, body: dict[str, Any]) -> None:
        encoded = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self) -> None:
        if self.path.split("?", 1)[0] == "/api/status":
            if ROUTE_JSON.exists():
                try:
                    route = json.loads(ROUTE_JSON.read_text(encoding="utf-8"))
                    self.send_json(200, {"ok": True, "route": route})
                    return
                except (OSError, json.JSONDecodeError):
                    pass
            self.send_json(200, {"ok": True, "route": None})
            return
        super().do_GET()

    def do_POST(self) -> None:
        if self.path.split("?", 1)[0] != "/api/route":
            self.send_json(404, {"ok": False, "error": "接口不存在"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY_BYTES:
                raise ValueError("请求正文大小无效")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            route = validate_route(payload)
            packet = encode_packet(route)
            with WRITE_LOCK:
                atomic_write(ROUTE_BIN, packet)
                atomic_write(
                    ROUTE_JSON,
                    json.dumps(route, ensure_ascii=False, indent=2).encode("utf-8"),
                )
            self.send_json(
                200,
                {
                    "ok": True,
                    "point_count": len(route["points"]),
                    "packet_bytes": len(packet),
                    "sequence": route["sequence"],
                },
            )
        except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
            self.send_json(400, {"ok": False, "error": str(exc)})
        except OSError as exc:
            self.send_json(500, {"ok": False, "error": f"写入桥接文件失败：{exc}"})


def main() -> None:
    parser = argparse.ArgumentParser(description="GaoDe phone navigation bridge")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    BRIDGE_DIR.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer((args.host, args.port), BridgeHandler)
    print(f"Phone navigation simulator: http://{args.host}:{args.port}")
    print(f"LVGL bridge packet: {ROUTE_BIN}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()

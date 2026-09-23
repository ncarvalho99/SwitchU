#!/usr/bin/env python3
"""
SwitchU YouTube-DL Backend Service
Provides a lightweight HTTP REST API for Nintendo Switch SwitchU client:
- GET /health
- GET /api/search?q=<query>&limit=20
- GET /api/stream?id=<video_id>
- GET /api/download?id=<video_id>

Supports yt-dlp, youtube-dl, and native InnerTube fallback.
"""

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import urllib.parse
import urllib.request
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn

logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(levelname)s: %(message)s")
logger = logging.getLogger("switchu-ytdl")

# Try to import yt_dlp or youtube_dl
YTDL_MODULE = None
try:
    import yt_dlp as ytdl_lib
    YTDL_MODULE = "yt_dlp"
    logger.info("Using yt-dlp Python module")
except ImportError:
    try:
        import youtube_dl as ytdl_lib
        YTDL_MODULE = "youtube_dl"
        logger.info("Using youtube-dl Python module")
    except ImportError:
        ytdl_lib = None
        logger.info("No ytdl Python module found. Will use CLI or native InnerTube fallback.")


def format_duration(seconds):
    if not seconds:
        return "0:00"
    try:
        s = int(seconds)
        m, s = divmod(s, 60)
        h, m = divmod(m, 60)
        if h > 0:
            return f"{h}:{m:02d}:{s:02d}"
        return f"{m}:{s:02d}"
    except Exception:
        return "0:00"


def clean_title(title):
    if not title:
        return "Unknown"
    # Remove problematic characters for FAT32 / SD card
    return re.sub(r'[\\/*?:"<>|]', "", title).strip()


def search_innertube(query, limit=20):
    """Direct search using YouTube InnerTube API (fast, zero key required)."""
    url = "https://www.youtube.com/youtubei/v1/search"
    headers = {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Content-Type": "application/json"
    }
    payload = {
        "context": {
            "client": {
                "clientName": "WEB",
                "clientVersion": "2.20240101.00.00",
                "hl": "en",
                "gl": "US"
            }
        },
        "query": query
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            content = resp.read().decode("utf-8", errors="replace")
            res_json = json.loads(content)
    except Exception as e:
        logger.error("InnerTube search failed: %s", e)
        return []

    results = []
    try:
        sections = res_json.get("contents", {}).get("twoColumnSearchResultsRenderer", {}) \
            .get("primaryContents", {}).get("sectionListRenderer", {}).get("contents", [])
        for section in sections:
            item_section = section.get("itemSectionRenderer", {}).get("contents", [])
            for item in item_section:
                vr = item.get("videoRenderer")
                if not vr:
                    continue
                vid = vr.get("videoId")
                if not vid:
                    continue

                title = ""
                runs = vr.get("title", {}).get("runs", [])
                if runs:
                    title = "".join(r.get("text", "") for r in runs)
                else:
                    title = vr.get("title", {}).get("simpleText", "")

                author = ""
                byline_runs = vr.get("ownerText", {}).get("runs") or vr.get("shortBylineText", {}).get("runs", [])
                if byline_runs:
                    author = "".join(r.get("text", "") for r in byline_runs)

                duration = vr.get("lengthText", {}).get("simpleText", "0:00")

                thumbs = vr.get("thumbnail", {}).get("thumbnails", [])
                thumb_url = f"https://i.ytimg.com/vi/{vid}/hqdefault.jpg"
                if thumbs:
                    thumb_url = thumbs[-1].get("url", thumb_url)

                results.append({
                    "id": vid,
                    "title": clean_title(title),
                    "author": author or "YouTube",
                    "duration": duration,
                    "thumbnail": thumb_url
                })
                if len(results) >= limit:
                    break
            if len(results) >= limit:
                break
    except Exception as e:
        logger.error("Error parsing InnerTube search response: %s", e)

    return results


def resolve_stream_info(video_id):
    """Resolve direct audio stream URL using ytdl/yt-dlp or CLI."""
    url = f"https://www.youtube.com/watch?v={video_id}"
    logger.info("Resolving stream info for %s...", video_id)

    # 1. Try python module if available
    if ytdl_lib:
        ydl_opts = {
            "format": "bestaudio/best",
            "quiet": True,
            "no_warnings": True,
            "skip_download": True,
            "noplaylist": True,
        }
        try:
            with ytdl_lib.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=False)
                if info:
                    stream_url = info.get("url")
                    title = info.get("title", "track")
                    ext = info.get("ext", "mp3")
                    filesize = info.get("filesize") or info.get("filesize_approx") or 0
                    return {
                        "status": "ok",
                        "id": video_id,
                        "title": clean_title(title),
                        "stream_url": stream_url,
                        "ext": ext,
                        "filesize": filesize
                    }
        except Exception as e:
            logger.warning("Python ytdl extract failed for %s: %s", video_id, e)

    # 2. Try CLI yt-dlp or youtube-dl
    for cli_cmd in ["yt-dlp", "youtube-dl"]:
        try:
            cmd = [cli_cmd, "-g", "-f", "bestaudio/best", "--get-title", url]
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            if proc.returncode == 0:
                lines = proc.stdout.strip().split("\n")
                if len(lines) >= 2:
                    title = lines[0].strip()
                    stream_url = lines[1].strip()
                    return {
                        "status": "ok",
                        "id": video_id,
                        "title": clean_title(title),
                        "stream_url": stream_url,
                        "ext": "mp3",
                        "filesize": 0
                    }
                elif len(lines) == 1 and lines[0].startswith("http"):
                    return {
                        "status": "ok",
                        "id": video_id,
                        "title": "track_" + video_id,
                        "stream_url": lines[0].strip(),
                        "ext": "mp3",
                        "filesize": 0
                    }
        except Exception as e:
            logger.debug("CLI %s not available or failed: %s", cli_cmd, e)

    return {"status": "error", "error": f"Failed to extract stream for {video_id}"}


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class YtdlRequestHandler(BaseHTTPRequestHandler):
    server_version = "SwitchU-YTDL/1.0"

    def _send_json(self, status_code, data):
        body = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-SwitchU-Key")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-SwitchU-Key")
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        params = urllib.parse.parse_qs(parsed.query)

        # Health / Status
        if path in ["/", "/health", "/v1/health", "/api/health"]:
            self._send_json(200, {
                "status": "ok",
                "service": "switchu-ytdl",
                "version": "1.0.0",
                "ytdl_backend": YTDL_MODULE or "cli/innertube"
            })
            return

        # Search endpoint: /api/search?q=<query>&limit=20
        if path in ["/api/search", "/v1/search"]:
            query = params.get("q", [""])[0].strip()
            if not query:
                self._send_json(400, {"error": "Missing query 'q'"})
                return
            limit = int(params.get("limit", [20])[0])
            items = search_innertube(query, limit)
            self._send_json(200, {
                "query": query,
                "count": len(items),
                "items": items
            })
            return

        # Stream resolution endpoint: /api/stream?id=<video_id>
        if path in ["/api/stream", "/v1/stream"]:
            vid = params.get("id", [""])[0].strip()
            if not vid:
                url_param = params.get("url", [""])[0].strip()
                if "v=" in url_param:
                    vid = url_param.split("v=")[1].split("&")[0]
                elif "youtu.be/" in url_param:
                    vid = url_param.split("youtu.be/")[1].split("?")[0]

            if not vid:
                self._send_json(400, {"error": "Missing 'id' or 'url' parameter"})
                return

            info = resolve_stream_info(vid)
            status_code = 200 if info.get("status") == "ok" else 500
            self._send_json(status_code, info)
            return

        # Download redirect endpoint: /api/download?id=<video_id>
        if path in ["/api/download", "/v1/download"]:
            vid = params.get("id", [""])[0].strip()
            if not vid:
                self._send_json(400, {"error": "Missing 'id' parameter"})
                return

            info = resolve_stream_info(vid)
            if info.get("status") == "ok" and info.get("stream_url"):
                # Redirect to direct stream
                self.send_response(302)
                self.send_header("Location", info["stream_url"])
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                return
            else:
                self._send_json(500, {"error": info.get("error", "Stream resolution failed")})
                return

        self._send_json(404, {"error": "Not found", "path": path})


def main():
    parser = argparse.ArgumentParser(description="SwitchU YouTube-DL Backend Service")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default: 8080)")
    args = parser.parse_args()

    server_address = (args.host, args.port)
    httpd = ThreadedHTTPServer(server_address, YtdlRequestHandler)
    logger.info("SwitchU YTDL Service running at http://%s:%d", args.host, args.port)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logger.info("Stopping server...")
        httpd.server_close()


if __name__ == "__main__":
    main()

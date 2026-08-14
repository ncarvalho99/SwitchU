"""Small SteamGridDB proxy used by SwitchU's game-art gallery.

The Switch client never receives the SteamGridDB API key.  This service exposes
only search and read-only artwork metadata, caches upstream responses and is
intended to be reached through the Cloudflare Tunnel at gallery.nclabs.dev.
"""

from __future__ import annotations

import ipaddress
import json
import os
import threading
import time
from collections import defaultdict, deque
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

from fastapi import FastAPI, HTTPException, Request as FastApiRequest


APP_NAME = "switchu-gallery"
STEAMGRIDDB_API = "https://www.steamgriddb.com/api/v2"
CACHE_TTL_SECONDS = 15 * 60
RATE_LIMIT_REQUESTS = 40
RATE_LIMIT_WINDOW_SECONDS = 60
GRID_DIMENSIONS = frozenset({"600x900", "342x482", "660x930", "512x512", "1024x1024", "460x215", "920x430"})
HERO_DIMENSIONS = frozenset({"1920x620", "3840x1240", "1600x650"})


def _trusted_networks() -> tuple[ipaddress.IPv4Network | ipaddress.IPv6Network, ...]:
    raw = os.environ.get("SWITCHU_TRUSTED_NETWORKS", "172.26.128.0/26,10.0.0.0/26,127.0.0.0/8,::1/128")
    try:
        return tuple(ipaddress.ip_network(value.strip()) for value in raw.split(",") if value.strip())
    except ValueError as exc:
        raise RuntimeError("SWITCHU_TRUSTED_NETWORKS contains an invalid network") from exc


TRUSTED_NETWORKS = _trusted_networks()
_cache: dict[str, tuple[float, Any]] = {}
_cache_lock = threading.Lock()
_rate_windows: dict[str, deque[float]] = defaultdict(deque)
_rate_lock = threading.Lock()

app = FastAPI(docs_url=None, redoc_url=None, openapi_url=None, title=APP_NAME)


def _is_trusted_proxy(host: str | None) -> bool:
    if not host:
        return False
    try:
        address = ipaddress.ip_address(host)
    except ValueError:
        return False
    return any(address in network for network in TRUSTED_NETWORKS)


def _client_identity(request: FastApiRequest) -> str:
    # This header is set by the Cloudflare connector. It is read only after the
    # source network gate, so a public client cannot forge a rate-limit identity.
    forwarded = request.headers.get("cf-connecting-ip", "").strip()
    if forwarded:
        return forwarded
    return request.client.host if request.client else "unknown"


def _allow_request(identity: str) -> bool:
    now = time.monotonic()
    with _rate_lock:
        window = _rate_windows[identity]
        while window and now - window[0] >= RATE_LIMIT_WINDOW_SECONDS:
            window.popleft()
        if len(window) >= RATE_LIMIT_REQUESTS:
            return False
        window.append(now)
        return True


def _cached(key: str) -> Any | None:
    now = time.monotonic()
    with _cache_lock:
        value = _cache.get(key)
        if value is None or value[0] <= now:
            _cache.pop(key, None)
            return None
        return value[1]


def _store_cache(key: str, value: Any) -> Any:
    with _cache_lock:
        _cache[key] = (time.monotonic() + CACHE_TTL_SECONDS, value)
    return value


def _steamgriddb_get(path: str) -> list[dict[str, Any]]:
    cache_key = path
    cached = _cached(cache_key)
    if cached is not None:
        return cached

    api_key = os.environ.get("STEAMGRIDDB_API_KEY", "").strip()
    if not api_key:
        raise HTTPException(status_code=503, detail="SteamGridDB key is not configured")

    request = Request(
        f"{STEAMGRIDDB_API}/{path}",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Accept": "application/json",
            "User-Agent": "SwitchU/1.1 (+https://gallery.nclabs.dev)",
        },
        method="GET",
    )
    try:
        with urlopen(request, timeout=12) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except (HTTPError, URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise HTTPException(status_code=502, detail="SteamGridDB is unavailable") from exc

    if not payload.get("success") or not isinstance(payload.get("data"), list):
        raise HTTPException(status_code=502, detail="SteamGridDB returned an invalid response")
    return _store_cache(cache_key, payload["data"])


def _asset(item: dict[str, Any]) -> dict[str, Any]:
    return {
        "id": item.get("id"),
        "url": item.get("url"),
        "thumb": item.get("thumb") or item.get("url"),
        "width": item.get("width"),
        "height": item.get("height"),
        "mime": item.get("mime"),
        "style": item.get("style"),
        "score": item.get("score"),
        "author": item.get("author", {}).get("name") if isinstance(item.get("author"), dict) else None,
    }


@app.middleware("http")
async def restrict_to_tunnel(request: FastApiRequest, call_next: Any) -> Any:
    client_host = request.client.host if request.client else None
    if not _is_trusted_proxy(client_host):
        raise HTTPException(status_code=403, detail="Tunnel access required")
    if request.url.path != "/health" and not _allow_request(_client_identity(request)):
        raise HTTPException(status_code=429, detail="Too many requests")
    return await call_next(request)


@app.get("/health")
def health() -> dict[str, object]:
    return {
        "service": APP_NAME,
        "status": "ok",
        "steamgriddbConfigured": bool(os.environ.get("STEAMGRIDDB_API_KEY", "").strip()),
    }


@app.get("/v1/search")
def search(query: str) -> dict[str, list[dict[str, Any]]]:
    query = " ".join(query.split())
    if not 2 <= len(query) <= 120:
        raise HTTPException(status_code=422, detail="query must contain 2 to 120 characters")

    games = _steamgriddb_get(f"search/autocomplete/{quote(query, safe='')}")
    return {
        "games": [
            {"id": game.get("id"), "name": game.get("name"), "types": game.get("types", [])}
            for game in games[:12]
        ]
    }


def _game_assets(game_id: int, kind: str, dimensions: str) -> dict[str, list[dict[str, Any]]]:
    if game_id <= 0:
        raise HTTPException(status_code=422, detail="game id must be positive")
    allowed_dimensions = GRID_DIMENSIONS if kind == "grids" else HERO_DIMENSIONS
    if dimensions not in allowed_dimensions:
        raise HTTPException(status_code=422, detail="unsupported artwork dimensions")
    items = _steamgriddb_get(f"{kind}/game/{game_id}?dimensions={quote(dimensions, safe=',x')}")
    # nxui decodes JPEG and PNG. Do not hand WebP/animated formats to the
    # Switch menu only for texture decoding to fail after the network cost.
    compatible = (item for item in items if item.get("mime") in {"image/jpeg", "image/png"})
    return {kind: [_asset(item) for item in list(compatible)[:48]]}


@app.get("/v1/games/{game_id}/grids")
def grids(game_id: int, dimensions: str = "600x900") -> dict[str, list[dict[str, Any]]]:
    return _game_assets(game_id, "grids", dimensions)


@app.get("/v1/games/{game_id}/heroes")
def heroes(game_id: int, dimensions: str = "1920x620") -> dict[str, list[dict[str, Any]]]:
    return _game_assets(game_id, "heroes", dimensions)

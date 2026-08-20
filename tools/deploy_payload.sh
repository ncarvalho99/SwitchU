#!/usr/bin/env bash
# Safely publish a reviewed batch of theme packages on the catalogue host.
# Example: sudo tools/deploy_payload.sh --archive /srv/incoming/batch.zip --expected-count 61
set -Eeuo pipefail
IFS=$'\n\t'
umask 022

BASE_DIR="${SWITCHU_THEMES_ROOT:-/srv/themes}"
THEMES_DIR="$BASE_DIR/themes"
REINDEX_BIN="${SWITCHU_REINDEX_BIN:-/opt/switchu-themes/reindex.py}"
# Shared with publish.sh on the catalogue host. A manual package deployment and
# timer-driven ingestion must serialize against one another.
LOCK_FILE="${SWITCHU_DEPLOY_LOCK:-/run/switchu-publish.lock}"
archive=""
expected_count=""

usage() {
    cat <<'EOF'
Usage: deploy_payload.sh --archive <batch.zip> --expected-count <number>

The archive must contain exactly the expected number of root-level
<theme-id>.zip files. Existing theme.zip aliases are atomically replaced and
restored if reindexing fails. Immutable hash-named packages are never deleted.
EOF
}

while (($#)); do
    case "$1" in
        --archive) archive="${2:-}"; shift 2 ;;
        --expected-count) expected_count="${2:-}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if (( EUID != 0 )); then
    echo "Run this deployment helper as root." >&2
    exit 1
fi
if [[ -z "$archive" || ! -r "$archive" ]]; then
    echo "--archive must name a readable ZIP file." >&2
    exit 2
fi
if [[ ! "$expected_count" =~ ^[1-9][0-9]*$ ]]; then
    echo "--expected-count must be a positive integer." >&2
    exit 2
fi
if [[ ! -d "$THEMES_DIR" || ! -x "$REINDEX_BIN" ]]; then
    echo "Catalogue root or reindexer is unavailable; refusing deployment." >&2
    exit 1
fi
command -v flock >/dev/null || { echo "flock is required." >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 is required." >&2; exit 1; }
command -v unzip >/dev/null || { echo "unzip is required." >&2; exit 1; }

exec 9>"$LOCK_FILE"
flock -n 9 || { echo "Another theme deployment or ingestion is active." >&2; exit 1; }

# Staging under BASE_DIR keeps replacement on the same filesystem, so mv is an
# atomic rename rather than a copy.
stage="$(mktemp -d "$BASE_DIR/.theme-deploy.XXXXXX")"
packages_dir="$stage/packages"
backup_dir="$stage/rollback"
declare -a installed=()

cleanup() {
    local status=$?
    if (( status != 0 && ${#installed[@]} > 0 )); then
        echo "Deployment failed; restoring ${#installed[@]} aliases..." >&2
        local base
        for base in "${installed[@]}"; do
            mv -f "$backup_dir/$base.zip" "$THEMES_DIR/$base/theme.zip" || true
        done
    fi
    rm -rf -- "$stage"
    exit "$status"
}
trap cleanup EXIT

python3 - "$archive" "$expected_count" <<'PY'
import re
import stat
import sys
import zipfile

archive, expected = sys.argv[1], int(sys.argv[2])
safe_name = re.compile(r'^[a-z0-9][a-z0-9-]*\.zip$')
with zipfile.ZipFile(archive) as bundle:
    entries = [entry for entry in bundle.infolist() if not entry.is_dir()]
    names = [entry.filename for entry in entries]
    if len(entries) != expected:
        raise SystemExit(f'archive contains {len(entries)} files; expected {expected}')
    if len(set(names)) != len(names):
        raise SystemExit('archive contains duplicate names')
    if any('/' in name or not safe_name.fullmatch(name) for name in names):
        raise SystemExit('archive must contain only safe root-level <theme-id>.zip files')
    if any(stat.S_ISLNK(entry.external_attr >> 16) for entry in entries):
        raise SystemExit('archive must not contain symbolic links')
    if sum(entry.file_size for entry in entries) > 8 * 1024 * 1024 * 1024:
        raise SystemExit('archive expands beyond the 8 GiB safety limit')
print(f'validated {len(entries)} package files')
PY

mkdir -p "$packages_dir" "$backup_dir"
unzip -q "$archive" -d "$packages_dir"
mapfile -t packages < <(find "$packages_dir" -mindepth 1 -maxdepth 1 -type f -name '*.zip' -printf '%f\n' | LC_ALL=C sort)
if (( ${#packages[@]} != expected_count )); then
    echo "Extracted package count changed unexpectedly; refusing deployment." >&2
    exit 1
fi

for package in "${packages[@]}"; do
    base="${package%.zip}"
    source="$packages_dir/$package"
    target_dir="$THEMES_DIR/$base"
    [[ -d "$target_dir" && -f "$target_dir/theme.zip" ]] || {
        echo "Theme target is missing or invalid: $base" >&2
        exit 1
    }
    python3 - "$source" <<'PY'
import sys
import zipfile

path = sys.argv[1]
if not zipfile.is_zipfile(path):
    raise SystemExit(f'not a ZIP package: {path}')
with zipfile.ZipFile(path) as package:
    names = package.namelist()
    if 'theme.json' not in names or not any(name.lower().endswith('.dds') for name in names):
        raise SystemExit(f'incomplete theme package: {path}')
PY
done

echo "Publishing ${#packages[@]} validated packages..."
for package in "${packages[@]}"; do
    base="${package%.zip}"
    source="$packages_dir/$package"
    target="$THEMES_DIR/$base/theme.zip"
    # Keep an old hard link for rollback, then atomically replace only the
    # visible alias. Historical hash-named package files remain untouched.
    ln "$target" "$backup_dir/$package"
    mv -f -- "$source" "$target"
    chown root:root "$target"
    chmod 0644 "$target"
    installed+=("$base")
done

"$REINDEX_BIN"
echo "Published ${#installed[@]} packages and rebuilt $BASE_DIR/index.json."

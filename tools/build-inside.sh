#!/usr/bin/env bash
# Runs inside switchu-build with the repository bind-mounted at /src.

set -o pipefail
exec 2>&1

MODE="${1:-release}"
VARIANT="${2:-sysmodule}"
cd /src || exit 1

case "$MODE" in release|debug) ;; *) echo "modo invalido: $MODE"; exit 2;; esac
case "$VARIANT" in sysmodule|homebrew) ;; *) echo "variante invalida: $VARIANT"; exit 2;; esac

# CMake records the source directory. A cache produced outside Docker cannot
# safely be reused under /src, but an already-valid Docker cache is retained.
CACHE=build/espeak-ng-native/CMakeCache.txt
if [ -f "$CACHE" ] && ! grep -q '^CMAKE_HOME_DIRECTORY:INTERNAL=/src/' "$CACHE"; then
    echo "cache CMake gerado para outro diretorio de origem, descartando"
    rm -rf build/espeak-ng-native
fi

# Windows dependency files have paths such as C:/..., which GNU make parses as
# a target separator. Remove only the affected Atmosphere build state.
STRAT=lib/Atmosphere-libs/libstratosphere
if [ -d "$STRAT/build" ] && grep -rlsE '(^|[^A-Za-z])[A-Za-z]:[/\\]' --include='*.d' "$STRAT/build" | head -1 | grep -q .; then
    echo "descartando objetos libstratosphere com paths Windows"
    rm -rf "$STRAT/build" "$STRAT/lib"
fi

if [ "$VARIANT" = homebrew ]; then
    HOMEBREW=--homebrew=y
else
    HOMEBREW=--homebrew=n
fi

xmake f --yes -p cross -m "$MODE" -a aarch64 --toolchain=devkita64 "$HOMEBREW" --root || exit $?
xmake --root -j"$(nproc)" || exit $?

DIST="dist/$VARIANT/$MODE"
rm -rf "$DIST"
xmake install -o "$DIST" --root || exit $?

if [ "$VARIANT" = sysmodule ]; then
    [ -d "$DIST/atmosphere" ] || { echo "faltou $DIST/atmosphere"; exit 1; }
    [ -d "$DIST/switch" ] || { echo "faltou $DIST/switch"; exit 1; }
    mkdir -p artifacts
    ZIP="$PWD/artifacts/SwitchU-sysmodule-$MODE.zip"
    rm -f "$ZIP"
    ( cd "$DIST" && zip -qr "$ZIP" atmosphere switch ) || exit $?
    echo "pronto: artifacts/SwitchU-sysmodule-$MODE.zip ($(du -h "$ZIP" | cut -f1))"
    echo "copie atmosphere/ e switch/ do zip para a raiz do cartao SD"
else
    echo "pronto: build/cross/aarch64/$MODE/SwitchU.nro"
fi

echo "simbolos para ns_debug.sh: build/cross/aarch64/$MODE/{SwitchU,switchu-daemon}"

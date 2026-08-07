#!/usr/bin/env bash
# Runs HblOverride on a PC, against a fake SD tree, with no console involved.
#
#   docker run --rm -v "<repo>:/src" switchu-build bash /src/tests/hbl_override/run.sh
#
# Its paths are "sdmc:/..." with no leading slash, so they are relative. On
# Linux "sdmc:" is a legal directory name, which means the production .cpp runs
# unmodified from a scratch directory -- only <switch.h> and DebugLog are stubs.
#
# This is the one piece of SwitchU that writes to somebody's SD card, and two
# defects that compiling could never surface were caught here first.
set -e
cd "$(dirname "$0")/../.."

g++ -std=c++20 -Wall -Wextra -O0 -g \
    -I tests/hbl_override/stubs \
    -I projects/menu/src \
    -I projects/menu/src/launcher \
    tests/hbl_override/test_hbl_override.cpp \
    projects/menu/src/launcher/HblOverride.cpp \
    -o /tmp/hbl_override_test

/tmp/hbl_override_test

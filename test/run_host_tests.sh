#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
workdir=$(mktemp -d "${TMPDIR:-/tmp}/mecsveglia-tests.XXXXXX")
trap 'rm -f "$workdir/regression" "$workdir/handlers" "$workdir/handlers.inc"; rmdir "$workdir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itest/host/stubs -Isrc \
  -I".pio/libdeps/esp32-s3-fh4r2/ArduinoJson/src" \
  test/host/regression.cpp src/rss.cpp src/meteo.cpp src/eyes.cpp \
  -o "$workdir/regression"
"$workdir/regression"
python3 test/host/extract_handlers.py > "$workdir/handlers.inc"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itest/host/stubs -Isrc -I"$workdir" test/host/handlers.cpp \
  -o "$workdir/handlers"
"$workdir/handlers"

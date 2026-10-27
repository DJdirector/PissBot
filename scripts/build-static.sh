#!/bin/sh
# Builds a fully static pissbot (no shared libraries needed at runtime) inside
# an Alpine container, so it links against musl instead of glibc.
# Needs Docker. Result: dist/pissbot
set -eu
cd "$(dirname "$0")/.."
mkdir -p dist

docker run --rm \
  -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  -v "$PWD":/src -w /src alpine:3.22 sh -eu -c '
    apk add --no-cache build-base cmake ninja git linux-headers pkgconf \
      openssl-dev openssl-libs-static zlib-dev zlib-static \
      sqlite-dev sqlite-static
    cmake -S . -B build-static -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DPISSBOT_STATIC=ON
    cmake --build build-static -j"$(nproc)"
    ctest --test-dir build-static --output-on-failure

    # A static executable has no interpreter (INTERP) header.
    if readelf -lW build-static/pissbot | grep -q INTERP; then
      echo "ERROR: pissbot is dynamically linked" >&2
      exit 1
    fi
    # Smoke test: without a token it must start and exit with an error.
    set +e
    env -u BOT_TOKEN build-static/pissbot >/dev/null 2>&1
    rc=$?
    set -e
    [ "$rc" -eq 1 ] || { echo "ERROR: smoke test exited with $rc" >&2; exit 1; }

    strip -o dist/pissbot build-static/pissbot
    chown -R "$HOST_UID:$HOST_GID" dist build-static
  '
echo "Built dist/pissbot"

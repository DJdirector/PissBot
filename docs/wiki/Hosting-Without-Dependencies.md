# Hosting Without Dependencies

Some hosts, such as bot-hosting.net, only run an uploaded executable and have no shared libraries installed. PissBot can be built as one self-contained file that bundles D++, OpenSSL, zlib and SQLite.

## Build it

Using Docker (Alpine/musl, matches what CI does):

```
./scripts/build-static.sh
```

The result is `dist/pissbot`. The script also runs the tests and checks that the file is truly static.

Without Docker, on an Alpine system with `build-base cmake ninja git openssl-dev openssl-libs-static zlib-dev zlib-static sqlite-dev sqlite-static`:

```
cmake -S . -B build-static -G Ninja -DCMAKE_BUILD_TYPE=Release -DPISSBOT_STATIC=ON
cmake --build build-static
```

Do not build this way on Ubuntu/Debian: a static glibc binary breaks DNS lookups. Use Alpine.

Pushing a tag such as `v1.0.0` runs the "Static build" workflow, which attaches `pissbot-linux-x86_64` to a GitHub release.

## Deploy

1. Upload the binary and set it as the startup file in the panel.
2. Add `BOT_TOKEN` as an environment variable (mark it secret).
3. Start it. The database `bot_data.db` is created next to the binary and is included in the host's backups.

Voice is disabled in this build. The bot only listens to voice-state events (voice XP), which still work.

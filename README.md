# PissBot

A modular Discord bot written in C++ ([D++](https://github.com/brainboxdotcc/DPP) and SQLite). Each server turns on only the features it wants, and the bot registers slash commands only for those features.

![CI](https://github.com/OWNER/REPO/actions/workflows/ci.yml/badge.svg)

## Features

| Module | What it does |
| --- | --- |
| Levels and XP | XP from chat and voice, ranks, live leaderboard, role rewards |
| Social media posts | YouTube upload and live notifications |
| Custom commands | Your own `!commands` with Nightbot-style `$(variables)` |
| Answers | Auto-replies to plain messages like "hi" |
| Moderation | Warn, timeout, kick, ban, purge, mod log |
| Welcome and autorole | Greetings, goodbyes, role on join |
| Role menus | Self-assign roles with buttons |

A guided `/setup` wizard picks the modules, and `/permissions` lets the server owner choose which roles can use what.

## Quick start

```bash
# Debian/Ubuntu
sudo apt install build-essential cmake git pkg-config libssl-dev zlib1g-dev libsqlite3-dev libopus-dev

cmake -S . -B build
cmake --build build -j2          # first build compiles D++ and is slow

export BOT_TOKEN="your-token"    # or: cp .env.example .env, fill it in, then: set -a; . ./.env; set +a
./build/pissbot
```

Run it from the same folder every time: data lives in `bot_data.db` in the working directory. In the Discord Developer Portal, enable the **Message Content** and **Server Members** intents. See [Getting Started](docs/wiki/Getting-Started.md) for the full walkthrough.

## Documentation

The wiki pages are in [`docs/wiki`](docs/wiki). Start with [Home](docs/wiki/Home.md).

## Tests

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests labelled `unit` (template engine, database, answer matching) do not need D++ and build in seconds:

```bash
cmake --build build --target template_test database_test answers_test
ctest --test-dir build -L unit
```

CI runs both on every push and pull request. See [Architecture](docs/wiki/Architecture.md).

## Contributing

Never commit `.env` or `bot_data.db`; both are in `.gitignore`. To add a feature, create a module in `src/modules/` (a worked example is in [Architecture](docs/wiki/Architecture.md)).

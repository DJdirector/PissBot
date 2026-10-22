# Getting Started

## 1. Create the Discord application

1. Open the [Discord Developer Portal](https://discord.com/developers/applications) and create a new application.
2. In the **Bot** tab, create the bot and copy its **token**. Treat it like a password.
3. On the same tab, under **Privileged Gateway Intents**, turn on:
   - **Message Content Intent** (custom commands, answers, chat XP)
   - **Server Members Intent** (welcome messages and autorole)

   If either is off, the bot fails to connect with a "Disallowed intents" error.
4. In **OAuth2 > URL Generator**, tick the scopes `bot` and `applications.commands`, then the permissions in the table below, and open the generated link to invite the bot.

### Permissions to give the bot

Give only what your enabled modules need.

| Permission | Needed by |
| --- | --- |
| View Channels, Send Messages, Embed Links, Read Message History | everything |
| Manage Roles | Levels (role rewards), Welcome (autorole), Role Menus |
| Moderate Members | `/timeout`, `/untimeout` |
| Kick Members | `/kick` |
| Ban Members | `/ban`, `/unban` |
| Manage Messages | `/purge` |

The bot's own role must sit **above** any role it hands out or any member it moderates. Discord enforces this and the bot cannot override it.

## 2. Build

Requirements: Linux, a C++17 compiler, CMake 3.15 or newer, git, and the development packages for OpenSSL, zlib and SQLite.

On Debian or Ubuntu:

```bash
sudo apt install build-essential cmake git pkg-config libssl-dev zlib1g-dev libsqlite3-dev libopus-dev
```

Then, from the project folder:

```bash
cmake -S . -B build
cmake --build build -j2
```

CMake downloads the D++ library automatically. The first build compiles all of D++ and is slow (roughly 15 minutes on a single core). Later builds only recompile what changed. If the build runs out of memory, use `-j1`.

> **Pinned D++ version:** `CMakeLists.txt` pins D++ to an exact commit, so builds are reproducible. Following D++'s `master` branch previously broke modal dialogs with no change on our side. To upgrade D++, change the `GIT_TAG` hash, rebuild and run the tests (see [Architecture](Architecture)).

New source files are picked up automatically (the build globs `src/*.cpp` and `src/modules/*.cpp`), but run `cmake -S . -B build` again after adding a file.

## 3. Run

The bot reads its token from the `BOT_TOKEN` environment variable. It does **not** read `.env` by itself.

```bash
export BOT_TOKEN="your-token-here"
./build/pissbot
```

To load a `.env` file with `KEY=value` lines:

```bash
set -a; . ./.env; set +a
./build/pissbot
```

Data is stored in `bot_data.db` in the **current working directory**, so always start the bot from the same folder or you will get a fresh, empty database.

### Running as a service (systemd example)

```ini
[Unit]
Description=PissBot
After=network-online.target

[Service]
WorkingDirectory=/opt/pissbot
Environment=BOT_TOKEN=your-token-here
ExecStart=/opt/pissbot/build/pissbot
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

### Backups

Everything lives in `bot_data.db`. Copy that file (with the bot stopped, or using `sqlite3 bot_data.db ".backup backup.db"`) to back it up.

## 4. First steps in your server

1. Run `/setup` and pick your modules. See [Setup and Modules](Setup-and-Modules).
2. Run `/permissions` to let moderators use features. See [Permissions](Permissions).
3. Try the module dashboards: `/youtube`, `/answers`, `/welcome`.

Slash commands appear within seconds. Until a module is enabled, only `/setup`, `/permissions`, `/ping` and `/bot_status` exist.

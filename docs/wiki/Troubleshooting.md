# Troubleshooting

## The bot will not start

| Symptom | Cause and fix |
| --- | --- |
| `BOT_TOKEN environment variable is not set` | The token is read from the environment, not from `.env`. Export it or load `.env` with `set -a; . ./.env; set +a`. See [Getting Started](Getting-Started) |
| "Disallowed intents" or close code 4014 | Turn on **Message Content Intent** and **Server Members Intent** in the Developer Portal, Bot tab |
| `Failed to initialize database tables!` | The folder is not writable, or `bot_data.db` is corrupt. The bot creates `bot_data.db` in its working directory |
| Everything looks empty after a restart | You started the bot from a different folder, so it created a new `bot_data.db`. Always run it from the same working directory |

## Commands are missing

- Only `/setup`, `/permissions`, `/ping` and `/bot_status` exist until a module is enabled. Run `/setup`.
- If a command replies "That feature is turned off", its module is off. Run `/setup`.
- The bot must be invited with the `applications.commands` scope. Re-invite it with that scope if no slash commands appear at all.
- After changing modules, commands update within seconds. If they look stale, restart your Discord client.

## "You don't have permission to use this"

The member's role has not been granted that feature. An administrator can grant it in `/permissions`. Remember a feature with no roles assigned is administrators only.

## Roles are not being given

Applies to level rewards, autorole and role menus.

- The bot needs **Manage Roles**.
- The bot's highest role must be **above** the role it is giving. Drag the bot's role up in Server Settings, Roles.
- Level rewards are applied on level-up only. Members already past a level get a newly added reward on their next level-up.

## YouTube posts do not appear

- Make sure the Social Media Posts module is on and a notification channel is set (`/setup`).
- The channel ID must be the 24-character one starting with `UC`, not the `@handle`.
- The bot checks every 5 minutes and ignores videos older than 30 minutes. A video published an hour ago will not be posted.
- Only the newest upload is considered each cycle, and each video is announced once.
- Shorts are skipped when the Shorts filter is on. Streams that already ended are skipped.
- Posts say `@everyone` when no ping role is set. That is intended behaviour, not a bug.

## Answers or custom commands do not react

- The module must be enabled and **Message Content Intent** must be on.
- Answers set to **Whole message** only match when the whole message is the trigger. Switch to **Anywhere** for matches inside sentences.
- Each answer has a 15 second cooldown.
- A message that triggers a `!command` does not also trigger an answer.
- If a template is rejected on saving, read the message: it lists the unknown `$(variables)`.

## Welcome messages do not appear

- **Server Members Intent** must be on.
- A channel must be chosen in `/welcome`. The autorole works without one, messages do not.
- The bot needs permission to send messages in that channel.

## Moderation commands fail

The bot replies with the reason. The usual causes are the bot's role being below the target's, or a missing permission (Moderate Members, Kick Members, Ban Members, Manage Messages). Administrators and the server owner cannot be moderated by a bot with a lower role.

## Voice XP did not appear

Voice XP is credited when a member **leaves** the voice channel. If the bot restarts during a session, that session is lost.

## Build problems

- Missing packages: install `libssl-dev`, `zlib1g-dev` and `libsqlite3-dev`.
- Out of memory while compiling D++: build with `cmake --build build -j1`.
- The build downloads D++ from GitHub, so it needs internet access the first time. The version is pinned in `CMakeLists.txt`.
- Tests fail after a D++ upgrade: see "Upgrading D++" in [Architecture](Architecture).
- New source files are not compiled: run `cmake -S . -B build` again so the file list refreshes.

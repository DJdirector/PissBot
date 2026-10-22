# Architecture and Development

For anyone changing the code or adding features.

## Source layout

| File | Purpose |
| --- | --- |
| `src/main.cpp` | Startup, intents, event wiring, the setup wizard, the permissions dashboard, background timers, and the handlers of the four original modules |
| `src/module.h`, `src/module.cpp` | The `Module` interface, the module registry, per-server enable state, per-server command sync |
| `src/modules/legacy.cpp` | Command definitions for the core commands and the four original modules |
| `src/modules/moderation.cpp` | Moderation module |
| `src/modules/welcome.cpp` | Welcome and autorole module |
| `src/modules/roles.cpp` | Role menus module |
| `src/template.h`, `src/template.cpp` | The `$(...)` template engine. No Discord dependencies |
| `src/common.h`, `src/common.cpp` | Shared helpers: permission checks, modal field reader, reply helpers |
| `src/answers.h`, `src/answers.cpp` | Answers matching and cooldown logic. No Discord dependency |
| `src/database.h`, `src/database.cpp` | SQLite wrapper and migrations |
| `tests/` | Unit tests, run with `ctest` |
| `.github/workflows/ci.yml` | GitHub Actions: unit tests and a full build on every push and pull request |
| `CMakeLists.txt` | Build. Defines `pissbot_util` (no D++), `pissbot_modules` (D++) and the `pissbot` executable. Globs `src/modules/*.cpp` |

The four original modules (levels, social, commands, answers) still have their handlers in `main.cpp`, gated by module. Moving them into `src/modules/` is mechanical future work. New modules should be separate files.

## How a module works

`Module` (in `module.h`) has:

- `id()`, `name()`, `description()`: shown in `/setup`
- `commands(app_id)`: the slash commands to register where the module is on
- Optional handlers, each returning `true` if it consumed the event: `on_slash`, `on_select`, `on_button`, `on_form`, `on_message`, plus `on_member_join` and `on_member_leave`

The registry (`all_modules()`) lists modules in the order shown in `/setup`.

### Lifecycle

1. A server enables modules in `/setup`. The choice is stored in `guild_modules`.
2. `sync_guild_commands` registers the core commands plus the commands of each enabled module for that server. It runs after `/setup` changes and when the bot joins or starts in a server. At startup the old global commands are cleared.
3. Events are offered to the enabled modules of the server where they happened. The first handler that returns `true` wins.
4. Interactions no module handles fall through to the legacy handlers in `main.cpp`. Those are gated by the `kCommandModule` table, which maps each legacy command to its module.

### Event flow for messages

`on_message` of enabled modules, then custom commands (`!name`), then answers, then chat XP.

## Adding a module

1. Create `src/modules/yourmodule.cpp`:

```cpp
#include "module.h"

namespace {
class PollsModule : public Module {
public:
  std::string id() const override { return "polls"; }
  std::string name() const override { return "Polls"; }
  std::string description() const override { return "Quick yes/no polls"; }

  std::vector<dpp::slashcommand> commands(dpp::snowflake app) const override {
    return {dpp::slashcommand("poll", "Start a poll", app)};
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    if (event.command.get_command_name() != "poll")
      return false;                       // not ours
    if (!has_access(ctx.db, event.command, "polls")) {
      event.reply(no_permission_message());
      return true;
    }
    event.reply("Poll started");
    return true;
  }
};
} // namespace

std::unique_ptr<Module> make_polls_module() {
  return std::make_unique<PollsModule>();
}
```

2. Declare `make_polls_module()` in `module.h` and add it to the list in `module.cpp`.
3. Add a permission feature in the `kFeatures` table in `main.cpp` (key `polls`) so owners can grant it to roles.
4. Run `cmake -S . -B build` and rebuild.

Guidelines:

- Return `false` for anything that is not yours. Every enabled module sees every interaction.
- Prefix component IDs (`poll_...`) so they cannot clash.
- Store small settings with `db.kv_get` and `db.kv_set` (per server, empty string means unset). Add a table and `Database` methods only when you need real structure.
- Use the template engine for user-facing text that owners may customise, and call `validate_template` when saving.
- Modal text inputs are added directly to the modal. D++ wraps each one in a Label itself, so do not wrap them in action rows. Read submitted values with `form_field(event, "custom_id")`.
- Handlers run on D++ worker threads. Protect any shared in-memory state with your own mutex.
- Discord limits: 100 slash commands per server, 25 options per command, descriptions up to 100 characters, required options before optional ones.

## Template engine

`render_template(template, context)` expands `$(...)`. The `TemplateContext` carries the user, channel, server, query and optional `vars` that override built-ins (YouTube sets `channel`, `url`, `role`). `validate_template` rejects unknown names. See [Template Syntax](Template-Syntax) for behaviour and limits.

To add a variable, handle its name in `call_function` and add it to `builtin_names()` in `template.cpp`.

## Database

SQLite file `bot_data.db`, created in the working directory.

| Table | Contents |
| --- | --- |
| `guild_settings` | Notification and leaderboard channels, leaderboard message |
| `youtube_subs` | Tracked YouTube channels and their settings |
| `youtube_cache` | Last posted video per channel |
| `user_xp` | XP per member per server |
| `level_roles` | Level to role rewards |
| `custom_commands` | `!commands` |
| `answers` | Auto-replies |
| `command_permissions` | Roles allowed per feature |
| `guild_modules` | Enabled modules per server (a row means enabled) |
| `guild_kv` | Generic per-server settings for modules |
| `counters` | `$(count)` counters |
| `warnings` | Moderation warnings |

All queries use prepared statements with bound parameters.

## Testing

Tests are small programs that exit non-zero on failure (`CHECK(...)` in `tests/check.h`). They are registered in `tests/CMakeLists.txt`.

| Test | Label | Covers |
| --- | --- | --- |
| `template_test` | unit | The `$(...)` engine, maths, escapes, limits, validation |
| `database_test` | unit | Every database operation, SQL injection strings, and the migration of old `{placeholder}` messages |
| `answers_test` | unit | Answer matching rules and cooldowns |
| `commands_test` | full | Every slash command definition against Discord's rules (name format, length limits, required options first, no duplicates, 100 command cap) |

```bash
cmake --build build
ctest --test-dir build --output-on-failure        # everything
ctest --test-dir build -L unit                    # only tests without D++
```

`unit` tests build without compiling D++:

```bash
cmake --build build --target template_test database_test answers_test
```

When you add a feature, add or extend a test in `tests/`. Put logic in a file with no Discord dependency (like `answers.cpp` and `template.cpp`) so it can be tested under the `unit` label.

### Continuous integration

`.github/workflows/ci.yml` runs on every push to `main` and every pull request:

1. **Unit tests**: builds and runs the `unit` tests. Fast.
2. **Full build and tests**: builds the whole bot including D++ (with a compiler cache) and runs every test.

### Upgrading D++

D++ is pinned to an exact commit in `CMakeLists.txt`. To upgrade, change the `GIT_TAG` hash, rebuild, run `ctest`, and try the dashboards and modals in a test server, since Discord-facing behaviour (components, modals) is the part most likely to change.

## Known limitations

- Voice session timers, chat XP cooldowns and answer cooldowns are in memory and reset on restart.
- YouTube checking is polling every 5 minutes. There is no push.
- Only YouTube is implemented for social posts.
- One SQLite connection is shared across threads.
- Handlers that talk to Discord (as opposed to the logic in `template.cpp`, `answers.cpp` and `database.cpp`) are not covered by automated tests. Try them in a test server.

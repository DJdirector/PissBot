# Permissions

The server owner decides which roles can use which parts of the bot with `/permissions`.

## The rules

- **Administrators always have access to everything.**
- For each feature below, you can allow any number of roles (up to 25).
- A feature with **no roles assigned is administrators only**. Nothing is open to other roles until you assign some.
- `/permissions` itself is administrators only, so the owner cannot lock themselves out.

## Using the dashboard

1. Run `/permissions`. The message lists every feature and who can currently use it.
2. Pick a feature from the dropdown.
3. Select the roles that should have access. Your selection **replaces** the current list.
4. Press **Administrators only** to reset a feature, or **Back** to return.

## Features

| Feature                | Commands it controls                                                                                    |
| ---------------------- | ------------------------------------------------------------------------------------------------------- |
| Setup wizard           | `/setup`                                                                                                |
| YouTube notifications  | `/youtube`                                                                                              |
| Level rewards          | `/levelrole` `/levelrole_remove`                                                                        |
| Custom commands        | `/command_add` `/command_remove`                                                                        |
| XP adjustments         | `/add_xp` `/remove_xp`                                                                                  |
| Token adjustments      | `/tokens_add` `/tokens_remove` `/tokens_set`                                                            |
| Answers (auto-replies) | `/answers`                                                                                              |
| Moderation             | `/warn` `/warnings` `/clearwarnings` `/timeout` `/untimeout` `/kick` `/ban` `/unban` `/purge` `/modlog` |
| Welcome and autorole   | `/welcome`                                                                                              |
| Role menus             | `/rolemenu`                                                                                             |

A role you allow can use the matching dashboards fully (add, edit and remove). Only grant a feature to roles you trust with it. For example, Custom commands and Answers let that role make the bot say things publicly.

## Open to everyone

These are not restricted by `/permissions`:

`/ping` `/bot_status` `/xp` `/rank` `/xp_top` `/inspect_xp` `/levelrole_list` `/tokens` `/tokens_top` `/command_list` `/command_help` `/youtube_preview`, and using `!commands`, Answers and role menu buttons.

## Permissions are not Discord permissions

`/permissions` controls who may **ask the bot** to do something. The bot still needs the real Discord permission to carry it out (for example Ban Members for `/ban`). See [Getting Started](Getting-Started).

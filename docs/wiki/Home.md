# PissBot Wiki

PissBot is a modular Discord bot written in C++ (D++ and SQLite). Each server picks the features it wants, and the bot only registers commands for those features.

## Start here

- [Getting Started](Getting-Started): create the Discord app, build, run, invite
- [Setup and Modules](Setup-and-Modules): the `/setup` wizard and what each module does
- [Permissions](Permissions): who can use what

## Modules

| Module | What it does |
| --- | --- |
| [Levels and XP](Levels-and-XP) | XP from chat and voice, ranks, leaderboard, role rewards |
| [Social Media Posts](Social-Media-Posts) | YouTube upload and live notifications |
| [Custom Commands](Custom-Commands) | Your own `!commands` |
| [Answers](Answers) | Auto-replies to plain messages like "hi" |
| [Moderation](Moderation) | Warn, timeout, kick, ban, purge, mod log |
| [Welcome and Autorole](Welcome-and-Autorole) | Greetings, goodbyes, role on join |
| [Role Menus](Role-Menus) | Self-assign roles with buttons |

## Reference

- [Template Syntax](Template-Syntax): the `$(user)`, `$(choose a|b)` variable language
- [Command Reference](Command-Reference): every slash command in one table
- [Troubleshooting](Troubleshooting)
- [Architecture and Development](Architecture): how the code is organised and how to add a module

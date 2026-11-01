# Command Reference

Commands exist in a server only when their module is enabled (core commands are always there). "Permission" refers to the feature in [Permissions](Permissions); administrators can always use everything.

## Core (always available)

| Command        | Who                     | Description                                                                                         |
| -------------- | ----------------------- | --------------------------------------------------------------------------------------------------- |
| `/ping`        | everyone                | Check the bot responds                                                                              |
| `/bot_status`  | everyone                | Uptime, tracked YouTube channels, and whether the bot can post in each configured channel (private) |
| `/setup`       | Setup wizard permission | Choose modules and set channels. See [Setup and Modules](Setup-and-Modules)                         |
| `/permissions` | administrators only     | Choose which roles can use each feature. See [Permissions](Permissions)                             |

## Levels and XP

| Command                   | Who            | Description                                    |
| ------------------------- | -------------- | ---------------------------------------------- |
| `/xp`                     | everyone       | Your XP (private)                              |
| `/rank`                   | everyone       | Level, XP to next level, server rank (private) |
| `/xp_top`                 | everyone       | Top 10 by XP                                   |
| `/inspect_xp user`        | everyone       | Another member's XP                            |
| `/add_xp user amount`     | XP adjustments | Add XP                                         |
| `/remove_xp user amount`  | XP adjustments | Remove XP                                      |
| `/levelrole level role`   | Level rewards  | Give a role at a level                         |
| `/levelrole_remove level` | Level rewards  | Remove a reward                                |
| `/levelrole_list`         | everyone       | List rewards                                   |

## Social Media Posts

| Command                     | Who                   | Description                                       |
| --------------------------- | --------------------- | ------------------------------------------------- |
| `/youtube`                  | YouTube notifications | Open the YouTube dashboard                        |
| `/youtube_preview template` | everyone              | Preview a notification message with sample values |

## Custom Commands

| Command                      | Who             | Description                   |
| ---------------------------- | --------------- | ----------------------------- |
| `/command_add name response` | Custom commands | Create or update a `!command` |
| `/command_remove name`       | Custom commands | Delete a `!command`           |
| `/command_list`              | everyone        | List commands                 |
| `/command_help`              | everyone        | Show the variable list        |

## Answers

| Command    | Who     | Description                |
| ---------- | ------- | -------------------------- |
| `/answers` | Answers | Open the answers dashboard |

## Moderation

All need the Moderation permission.

| Command          | Options                                                       |
| ---------------- | ------------------------------------------------------------- |
| `/warn`          | `user`, `reason` (optional)                                   |
| `/warnings`      | `user`                                                        |
| `/clearwarnings` | `user`                                                        |
| `/timeout`       | `user`, `minutes` (1 to 40320), `reason` (optional)           |
| `/untimeout`     | `user`                                                        |
| `/kick`          | `user`, `reason` (optional)                                   |
| `/ban`           | `user`, `reason` (optional), `delete_days` (0 to 7, optional) |
| `/unban`         | `user_id`                                                     |
| `/purge`         | `amount` (1 to 100)                                           |
| `/modlog`        | `channel`                                                     |

## Welcome and Autorole

| Command    | Who                  | Description                |
| ---------- | -------------------- | -------------------------- |
| `/welcome` | Welcome and autorole | Open the welcome dashboard |

## Role Menus

| Command                                | Who        | Description                                 |
| -------------------------------------- | ---------- | ------------------------------------------- |
| `/rolemenu title role1 [role2..role5]` | Role menus | Post a button menu for self-assigning roles |

## Casino

See [Casino](Casino) for how it works.

| Command                         | Who               | Description                                                              |
| ------------------------------- | ----------------- | ------------------------------------------------------------------------ |
| `/casino opt-in` `opt-out`      | everyone          | Join or leave the casino (reacting to the sign-up message does the same) |
| `/casino daily`                 | Gambling role     | Claim 100 piss tokens every 24 hours                                     |
| `/casino balance` `rules`       | everyone          | Your balance, the payout tables (private)                                |
| `/casino role role`             | administrators    | Use an existing role instead of "Gambling"                               |
| `/slots bet`                    | Gambling role     | Spin the slot machine                                                    |
| `/roulette bet choice [number]` | Gambling role     | Bet on the wheel                                                         |
| `/blackjack bet`                | Gambling role     | Play against the dealer                                                  |
| `/tokens [user]`                | everyone          | Balance, rank and next daily claim                                       |
| `/tokens_top`                   | everyone          | Top 10 by piss tokens                                                    |
| `/tokens_add user amount`       | Token adjustments | Give tokens                                                              |
| `/tokens_remove user amount`    | Token adjustments | Take tokens (never below 0)                                              |
| `/tokens_set user amount`       | Token adjustments | Set a balance                                                            |

## Chat triggers (no slash command)

| Trigger                     | Module          | Description                      |
| --------------------------- | --------------- | -------------------------------- |
| `!name`                     | Custom Commands | Runs a custom command            |
| Plain messages such as `hi` | Answers         | Bot replies if a trigger matches |

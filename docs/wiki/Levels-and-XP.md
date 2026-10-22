# Levels and XP

Module id: `levels`. Members earn XP for chatting and for time in voice channels, level up, and can receive roles as rewards.

## How XP is earned

| Source | XP | Rule |
| --- | --- | --- |
| Chat message | 15 | At most once per minute per member per server |
| Voice channel | 10 per minute | Credited when the member **leaves** voice, so it appears after they disconnect |

Messages that trigger a [custom command](Custom-Commands) do not earn XP. Bots never earn XP.

Voice time is tracked in memory. If the bot restarts while someone is in voice, that session's XP is lost.

## Levels

```
level = floor( sqrt( xp / 100 ) )        XP needed for level N = 100 x N x N
```

| Level | Total XP |
| --- | --- |
| 1 | 100 |
| 2 | 400 |
| 3 | 900 |
| 5 | 2,500 |
| 10 | 10,000 |
| 20 | 40,000 |

When someone levels up from chatting, the bot announces it in that channel. Voice level-ups are not announced, but role rewards still apply.

## Role rewards

Reward a level with a role:

- `/levelrole level:5 role:@Regular` gives `@Regular` at level 5. Running it again for the same level replaces the role.
- `/levelrole_list` shows all rewards.
- `/levelrole_remove level:5` deletes one.

Notes:

- Roles **stack**. Members keep earlier rewards.
- Rewards are applied on level-up (every reward at or below the new level is given). If you add a reward later, members already past that level receive it on their next level-up.
- XP removal (`/remove_xp`) does not take away roles that were already given.
- The bot's role must be above the reward roles, with Manage Roles.

These commands need the **Level rewards** [permission](Permissions) (administrators by default).

## Leaderboard channel

Set during [setup](Setup-and-Modules). The bot keeps a single leaderboard message in that channel and refreshes it every 5 minutes. It can take a few minutes to appear the first time.

## Commands

| Command | Who | What it does |
| --- | --- | --- |
| `/xp` | everyone | Your XP (private reply) |
| `/rank` | everyone | Your level, XP toward the next level and server rank (private reply) |
| `/xp_top` | everyone | Top 10 members (public reply) |
| `/inspect_xp user` | everyone | Another member's XP |
| `/add_xp user amount` | XP adjustments permission | Adds XP (also triggers level-ups and roles) |
| `/remove_xp user amount` | XP adjustments permission | Removes XP |
| `/levelrole`, `/levelrole_remove` | Level rewards permission | Manage role rewards |
| `/levelrole_list` | everyone | Lists role rewards |

# Moderation

Module id: `moderation`. Warn, time out, kick, ban and clean up, with every action logged to a channel.

All commands need the **Moderation** [permission](Permissions) (administrators by default). The bot also needs the matching Discord permission and a role **above** the target member.

## Commands

| Command | Options | What it does |
| --- | --- | --- |
| `/warn` | `user` `reason` (optional) | Saves a warning, DMs the member, logs it |
| `/warnings` | `user` | Shows a member's warnings (the latest 10) |
| `/clearwarnings` | `user` | Deletes all of a member's warnings |
| `/timeout` | `user` `minutes` (1 to 40320) `reason` (optional) | Times a member out, up to 28 days |
| `/untimeout` | `user` | Removes a timeout |
| `/kick` | `user` `reason` (optional) | Kicks a member |
| `/ban` | `user` `reason` (optional) `delete_days` (0 to 7, optional) | Bans a member, optionally deleting recent messages |
| `/unban` | `user_id` | Unbans by numeric user ID |
| `/purge` | `amount` (1 to 100) | Deletes the latest messages in the current channel |
| `/modlog` | `channel` | Sets where actions are logged |

Replies to moderators are private. You cannot target yourself or the bot.

## The mod log

Run `/modlog channel:#mod-log`. Each action then posts an embed with the action, the member, the moderator and the reason. Without a log channel, actions still work but are not logged.

## Details

- **DMs:** warn, timeout, kick and ban send the member a direct message with the server name and reason. If their DMs are closed, the action still goes through.
- **Warnings** are saved per server with the moderator, reason and date. There are no automatic punishments yet.
- **Purge** only removes messages younger than about 13 days, because Discord cannot bulk-delete older ones.
- If the bot cannot act (role too low, missing permission) it tells you instead of failing silently.

## Required Discord permissions

| Command | Bot needs |
| --- | --- |
| `/timeout` `/untimeout` | Moderate Members |
| `/kick` | Kick Members |
| `/ban` `/unban` | Ban Members |
| `/purge` | Manage Messages |

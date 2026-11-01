# Setup and Modules

## The modular design

Every feature is a **module**. Each server turns modules on or off for itself.

- Slash commands are registered **per server**, and only for enabled modules. A server that never enables Moderation never sees `/ban`.
- Event handling (messages, joins, timers) only runs for modules that are on in that server.
- Turning a module off **keeps its data** (XP, answers, warnings and so on). Turn it back on and everything is still there.

Always available, whatever is enabled: `/setup`, `/permissions`, `/ping`, `/bot_status`.

## The `/setup` wizard

Run `/setup` (administrators, or roles you allowed under the `setup` permission). The reply is a private message that edits itself as you go.

1. **Choose modules.** A multi-select lists every module, with the currently enabled ones pre-selected. Pick the ones you want and close the dropdown. The server's commands are updated immediately. "Keep current" leaves things unchanged.
2. **Notification channel** (only if Social Media Posts is on). Pick where YouTube alerts go. The bot posts a short how-to guide in that channel.
3. **XP leaderboard channel** (only if Levels and XP is on). Pick where the live leaderboard lives. The bot posts an explanation of how XP works there.
4. **Casino channels** (only if Casino is on). Pick the casino channel (where games are played), the casino leaderboard channel, and the channel for the sign-up message. See [Casino](Casino).
5. **Done.** A summary lists the enabled modules and their commands.

Each channel step has a **Skip this step** button. If the bot cannot post in the channel you pick (a permission override is blocking it), the wizard warns you straight away and says what to allow. Run `/setup` again at any time to change modules or channels.

## Modules

| Module                                       | Commands                                                                                                         | Channel needed in setup                             |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | --------------------------------------------------- |
| [Levels and XP](Levels-and-XP)               | `/xp` `/rank` `/xp_top` `/inspect_xp` `/add_xp` `/remove_xp` `/levelrole` `/levelrole_remove` `/levelrole_list`  | Leaderboard channel                                 |
| [Social Media Posts](Social-Media-Posts)     | `/youtube` `/youtube_preview`                                                                                    | Notification channel                                |
| [Custom Commands](Custom-Commands)           | `/command_add` `/command_remove` `/command_list` `/command_help`                                                 | none                                                |
| [Answers](Answers)                           | `/answers`                                                                                                       | none                                                |
| [Moderation](Moderation)                     | `/warn` `/warnings` `/clearwarnings` `/timeout` `/untimeout` `/kick` `/ban` `/unban` `/purge` `/modlog`          | none (mod log via `/modlog`)                        |
| [Welcome and Autorole](Welcome-and-Autorole) | `/welcome`                                                                                                       | set in the `/welcome` dashboard                     |
| [Role Menus](Role-Menus)                     | `/rolemenu`                                                                                                      | none                                                |
| [Casino](Casino)                             | `/casino` `/slots` `/roulette` `/blackjack` `/tokens` `/tokens_top` `/tokens_add` `/tokens_remove` `/tokens_set` | Casino channel, casino leaderboard, sign-up message |

## Servers that used an older version

Servers that used the bot before modules existed start with **every module off**. Their feature commands disappear until an administrator runs `/setup` and enables them. Existing data (XP, YouTube channels, answers, custom commands) is untouched.

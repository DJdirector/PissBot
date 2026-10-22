# Custom Commands

Module id: `commands`. Create your own `!commands` that reply with text. Replies can use the [template syntax](Template-Syntax) for names, arguments, random choices, counters and more.

## Creating commands

Needs the **Custom commands** [permission](Permissions) (administrators by default).

- `/command_add name:rules response:Be kind and read #info` creates `!rules`.
- Running `/command_add` with an existing name updates it.
- `/command_remove name:rules` deletes it.
- `/command_list` lists all commands (everyone can use this).
- `/command_help` shows the variable list (everyone can use this).

Rules:

- Names use letters, numbers, `-` and `_`, up to 32 characters. A leading `!` is optional when creating. Names are case-insensitive.
- Members trigger a command by typing `!name`. Anything after the name is the **query**: `!8ball will it rain` has the query `will it rain`.
- Replies are limited to 2,000 characters after variables are filled in.
- Only user mentions in a reply ping anyone, so a member cannot use `$(query)` to ping a role or `@everyone`.
- A message that triggers a custom command does not also trigger an [Answer](Answers) or earn [XP](Levels-and-XP). Unknown `!words` are ignored.
- On the slash command, `\n` in the response becomes a new line.

## Examples

**8-ball**

```
$(user) asked: "$(query)" 🎱 The 8-Ball says: $(choose Yes, definitely!|Most likely.|Ask again later.|Don’t count on it.|Outlook not so good.|Signs point to yes!)
```

**Hug**

```
$(user) gives $(touser) a big hug!
```

**Dice**

```
$(username) rolled a $(rand 1 6)
```

**Counter** (counts per command, saved across restarts)

```
That's the $(count) time someone asked for coffee.
```

**Countdown**

```
The event starts $(countdown 2026-12-25 18:00)
```

**Quick maths**

```
$(math 12 * 7) is the answer.
```

## JavaScript is not supported

Nightbot's `$(eval ...)` runs arbitrary JavaScript. PissBot does not, because running scripts written by members with command access would let them run code on the host. If a response contains `$(eval`, saving is rejected with a suggestion. Most uses are covered by:

- `$(choose a|b|c)` for random picks
- `$(rand 1 100)` for random numbers
- `$(math ...)` for calculations

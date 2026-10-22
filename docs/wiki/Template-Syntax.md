# Template Syntax

Custom commands, Answers, YouTube messages and welcome/goodbye messages all use the same variable language, modelled on Nightbot: `$(name arguments)`.

Variables can be nested: `$(upper $(username))`, `$(math $(rand 1 6) * 2)`.

## Variables

### About the person and place

| Variable | Result |
| --- | --- |
| `$(user)` | Mentions the member who sent the message |
| `$(username)` | Their plain name (no ping) |
| `$(userid)` | Their numeric ID |
| `$(channel)` | Links the current channel |
| `$(channelid)` | The channel's numeric ID |
| `$(server)` | The server name |

### Input (custom commands)

| Variable | Result |
| --- | --- |
| `$(query)` or `$(args)` | Everything after the command name |
| `$(querystring)` | The query, URL-encoded (useful in links) |
| `$(1)`, `$(2)`, ... | The first, second, ... word of the query (empty if missing) |
| `$(touser)` | The first word of the query, or the sender if there is none |

### Random and counting

| Variable | Result |
| --- | --- |
| `$(choose a\|b\|c)` | One option at random. `$(random ...)` is an alias |
| `$(rand 1 100)` | A random whole number between the two values, inclusive |
| `$(count)` | A counter that goes up by one each use. Counted per command or per answer and saved across restarts |

### Text and maths

| Variable | Result |
| --- | --- |
| `$(math 2+2)` | Calculates `+ - * / % ^` with parentheses and decimals. `$(math (2+3)*4)` gives `20` |
| `$(upper text)` | UPPERCASE |
| `$(lower TEXT)` | lowercase |

### Time

| Variable | Result |
| --- | --- |
| `$(time)` | The current time, shown in each viewer's own time zone |
| `$(date)` | The current date, in each viewer's own format |
| `$(countdown 2026-12-25)` | A live "in 2 months" style timestamp. Accepts `YYYY-MM-DD` or `YYYY-MM-DD HH:MM`, read as UTC. `$(countup ...)` is the same |

### Extra variables by feature

| Where | Extra variables |
| --- | --- |
| YouTube messages | `$(url)` `$(channel)` (the YouTube channel name, replacing the Discord channel) `$(role)` |
| Welcome and goodbye | `$(members)` (member count). In the goodbye message `$(user)` shows the name without a ping |

`$(count)`, `$(query)` and `$(1)` only make sense where they have a value. `$(count)` does not work in welcome or YouTube messages; the text is left as written.

## Writing a literal `$`, `(`, `)` or `|`

Put a backslash in front: `\$(user)` prints `$(user)`, `\)` prints `)`, `\|` prints `|`, `\\` prints `\`.

This matters when an option contains an unmatched parenthesis, for example a smiley:

```
$(choose Sure :\)|Nope)
```

## Old placeholders

Messages saved before this syntax existed use `{user}`, `{username}`, `{args}`, `{channel}`, `{url}`, `{role}`. These keep working, and are treated as `$(user)` and so on.

## Validation and failures

- When you save a message, unknown variables are rejected with a list of what was not recognised.
- If a variable cannot work at runtime (for example `$(math 1/0)` or `$(rand 1)`), it is left in the message as typed.
- Variables are expanded lazily inside `$(choose ...)`. Only the chosen option is evaluated, so a `$(count)` in an option you did not get does not increase.

## Limits

- Output is cut at 2,000 characters (Discord's limit).
- Nesting up to 8 levels and 100 variable calls per message.
- `$(math ...)` expressions up to 200 characters.
- Only user mentions ping anyone in a command or answer reply.

## Not supported

- `$(eval ...)` (JavaScript). Use `$(choose)`, `$(rand)` and `$(math)`.
- `$(urlfetch ...)` (fetching a web page). It is left out because it would let messages make the bot request arbitrary addresses.

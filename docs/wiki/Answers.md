# Answers

Module id: `answers`. Answers are auto-replies to plain messages, with no `!` prefix. When someone sends `hi`, the bot can reply publicly. This is the Nightbot-style "keyword" behaviour.

Answers are separate from [Custom Commands](Custom-Commands): commands need `!name`, answers react to normal chat.

## The `/answers` dashboard

Needs the **Answers** [permission](Permissions) (administrators by default).

- The main view lists your answers (slots used out of 25). Pick one from the dropdown to edit it, or press **Add answer**.
- **Add answer** asks for what someone sends and what the bot replies.
- The answer view has **Edit reply**, a match-mode toggle, **Remove** and **Back**.

## Match modes

| Mode | Replies when | Example trigger `why` |
| --- | --- | --- |
| **Whole message** (default) | The whole message is the trigger | `why`, `Why?`, `  WHY!! ` match; `why is that` does not |
| **Anywhere** | The message contains the trigger as a whole word or phrase | `but why is that` matches; `whyyy` does not |

Matching ignores capitalisation (for English letters), extra spaces and punctuation at the start or end. Triggers can be 1 to 50 characters.

If several answers match one message, the **longest trigger wins**. With answers for `why` and `why not`, the message `why not?` uses `why not`.

## Behaviour

- Replies are public, visible to the whole channel.
- Each answer has a **15 second cooldown** per server, so it cannot be spammed.
- A server can have up to **25 answers**.
- An answer reply does not stop [XP](Levels-and-XP) from being awarded for the message.
- If a message triggers a custom command, no answer fires.
- Replies use the [template syntax](Template-Syntax). Useful variables: `$(user)`, `$(username)`, `$(channel)`, `$(server)`, `$(choose a|b|c)`. There is no query for answers. `$(count)` counts per answer.

## Example

Trigger `gm` (whole message), reply:

```
$(choose Good morning|Morning|gm) $(user)!
```

# Social Media Posts

Module id: `social`. Posts a message in your notification channel when a tracked YouTube channel uploads a video or goes live. YouTube is the only platform supported so far.

## Prerequisites

Enable the module and pick a notification channel in [`/setup`](Setup-and-Modules). The bot posts a short guide in that channel.

## The `/youtube` dashboard

`/youtube` opens a private message that updates itself as you click. It needs the **YouTube notifications** [permission](Permissions) (administrators by default).

**Main view**

- Shows where notifications are posted and how many channels are tracked.
- A dropdown picks a tracked channel to configure.
- **Add channel** opens a popup. Paste a channel ID (24 characters, starting with `UC`) or a `youtube.com/channel/...` link.

**Channel view**

| Control | Effect |
| --- | --- |
| Role picker | Role to ping with each post |
| Clear ping role | Removes the ping role |
| Edit messages | Opens a popup for the video and live-stream message templates |
| Ignore Shorts / Allow Shorts | Skips YouTube Shorts |
| Use rich embeds / Use plain text | Posts an embed (title, thumbnail) instead of plain text |
| Remove channel | Stops tracking the channel |
| Back | Returns to the main view |

> **Ping role warning:** if no ping role is set, posts use `@everyone`. Set a role before adding a real channel, or clear the habit by always choosing one.

## Message templates

Messages use the [template syntax](Template-Syntax). Variables specific to YouTube:

| Variable | Value |
| --- | --- |
| `$(url)` | Link to the video or stream |
| `$(channel)` | The YouTube channel's name |
| `$(role)` | The ping role (or `@everyone`) |

Defaults:

- Video: `🔴 $(role) NEW VIDEO GO WATCH RN!!` then the link on a new line
- Live: `🔴 $(role) $(channel) IS LIVE ON YOUTUBE!!` then the link on a new line

Old messages written as `{url}`, `{channel}`, `{role}` still work. Saving a template with an unknown variable is rejected with an explanation.

Use `/youtube_preview template:...` to see how a template renders with sample values.

## How checking works

- The bot checks every tracked channel **every 5 minutes**.
- Only the **newest** upload on the channel's feed is considered, and each video is announced once.
- Videos older than **30 minutes** are skipped, so adding a channel does not post old videos.
- A stream that is live now uses the live message. A stream that has already ended is skipped.
- With the Shorts filter on, Shorts are skipped.

## Limits and next steps

- YouTube only. Twitch and other platforms are not implemented yet.
- Notifications appear up to 5 minutes after a video goes up.

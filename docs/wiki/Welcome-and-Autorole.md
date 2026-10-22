# Welcome and Autorole

Module id: `welcome`. Greets new members, says goodbye when they leave, and gives every new member a role.

Needs the **Server Members Intent** enabled in the Developer Portal (see [Getting Started](Getting-Started)).

## The `/welcome` dashboard

Needs the **Welcome and autorole** [permission](Permissions) (administrators by default). The private message shows the current settings and updates as you change them.

| Control | Effect |
| --- | --- |
| Channel picker | Where welcome and goodbye messages are posted |
| Role picker | The autorole given to every new member |
| Edit messages | Popup for the welcome and goodbye messages |
| Turn messages off | Clears the channel, which stops both messages |
| Clear autorole | Stops giving a role on join |

## Messages

Messages use the [template syntax](Template-Syntax).

- **Welcome** default: `Welcome to $(server), $(user)! You are member #$(members).`
- **Goodbye** is off until you write one. Leaving the goodbye box empty turns it off.
- Extra variable: `$(members)` is the server's member count.
- In the goodbye message `$(user)` prints the name without pinging, since they have left.

Saving a message with an unknown variable is rejected and nothing is saved.

## Autorole

Works even when no channel is set. The bot's role must be above the autorole, with Manage Roles.

## Notes

- With no channel set, no messages are sent, but the autorole still applies.
- Welcome messages need the member join event, so the Server Members Intent must be on.

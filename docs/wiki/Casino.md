# Casino

Module id: `casino`. Members join by reacting to a sign-up message, claim free **piss tokens** every day, and play slots, roulette and blackjack with them. Tokens are the casino's own currency and are separate from XP.

## Setting it up

Turn on **Casino** in `/setup`. The wizard then asks for three channels, each with a **Skip this step** button:

1. **Casino channel.** Where games are played. `/slots`, `/roulette` and `/blackjack` only work there, so they don't flood your other chats. With no casino channel set they work anywhere.
2. **Casino leaderboard.** A live top 10 of piss token holders that refreshes every 5 minutes. It is separate from the XP leaderboard.
3. **Sign-up message.** The channel where the bot posts the sign-up message and adds the 🎰 reaction itself. Picking a channel again posts a new message and deletes the old one.

The bot needs **Manage Roles** (and its own role must sit above the Gambling role) to give out the role. **Manage Messages** is optional: with it the bot removes any reaction other than 🎰 from the sign-up message.

## Joining

React to the sign-up message with 🎰. The bot gives you the **Gambling** role, and your first 100 piss tokens. Removing the reaction takes the role away; your tokens are kept for when you come back.

`/casino opt-in` and `/casino opt-out` do the same thing without the reaction. `/casino role` (administrators) picks an existing role to use instead of "Gambling".

Reactions made while the bot was offline are not picked up. Use `/casino opt-in` for those.

## Tokens

| Command                      | Who               | Description                                                                             |
| ---------------------------- | ----------------- | --------------------------------------------------------------------------------------- |
| `/casino daily`              | Gambling role     | Claim 100 tokens, once every 24 hours                                                   |
| `/casino balance`            | everyone          | Your balance (private)                                                                  |
| `/casino rules`              | everyone          | Payout tables (private)                                                                 |
| `/tokens [user]`             | everyone          | Your balance, rank and next daily claim (private), or another member's balance and rank |
| `/tokens_top`                | everyone          | Top 10 by piss tokens                                                                   |
| `/tokens_add user amount`    | Token adjustments | Give tokens                                                                             |
| `/tokens_remove user amount` | Token adjustments | Take tokens. Never goes below 0                                                         |
| `/tokens_set user amount`    | Token adjustments | Set an exact balance                                                                    |

"Token adjustments" is a feature in [Permissions](Permissions). Until you give it to a role, only administrators can use the three admin commands. Admin replies never ping the member.

## Games

Bets are 10 to 200 piss tokens, with one game every 5 minutes per player. `/casino rules` lists every payout. The rules and odds live in `src/casino.h`.

| Game      | Command                                          |
| --------- | ------------------------------------------------ |
| Slots     | `/slots bet`                                     |
| Roulette  | `/roulette bet choice [number]`                  |
| Blackjack | `/blackjack bet`, then Hit, Stand or Double down |

A blackjack hand left idle for 4 minutes stands on its own, and survives a bot restart.

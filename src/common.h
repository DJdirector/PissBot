#pragma once
#include "database.h"
#include <dpp/dpp.h>
#include <string>
#include <vector>

// Shared helpers used by core code and every module.
std::string replace_all(std::string str, const std::string &from,
                        const std::string &to);

// Administrators always pass. Everyone else needs a role the owner assigned
// to `feature` in /permissions (none assigned = administrators only).
bool is_admin(const dpp::interaction &cmd);
bool has_access(Database &db, const dpp::interaction &cmd,
                const std::string &feature);

// Reads a text input out of a submitted modal by custom_id ("" if missing).
std::string form_field(const dpp::form_submit_t &event, const std::string &id);

dpp::message ephemeral_text(const std::string &text);
dpp::message no_permission_message();

// ---- XP and levels ----

int level_for_xp(int xp);
int xp_for_level(int level);

// Adds XP and, when that raises the member's level, gives them the level
// reward roles and optionally announces it in `announce_channel`.
void grant_xp(dpp::cluster &bot, Database &db, uint64_t guild_id,
              uint64_t user_id, int amount, uint64_t announce_channel = 0);

// ---- Channel access (Discord error 50013 "Missing Permissions") ----

struct ChannelAccess {
  bool known = false;                 // false when the cache can't tell
  std::vector<std::string> missing;   // permission names the bot lacks
};

// What the bot can do in a channel: View Channel, Send Messages, Embed Links,
// Read Message History (the set every module needs to post).
ChannelAccess check_channel_access(dpp::cluster &bot, uint64_t guild_id,
                                   uint64_t channel_id);

// "" when fine or unknown, otherwise a ready-to-show warning with the fix.
std::string channel_warning(dpp::cluster &bot, uint64_t guild_id,
                            uint64_t channel_id);

// One line per configured channel (notifications, leaderboard, welcome, mod
// log, casino) saying whether the bot can post there. Used by /bot_status.
std::string channel_access_report(dpp::cluster &bot, Database &db,
                                  uint64_t guild_id);

// Logs a send failure with the channel, the reason and the fix. Repeats for
// the same channel are rate limited so a broken channel doesn't flood the log.
void log_send_failure(dpp::cluster &bot, uint64_t channel_id,
                      const std::string &what,
                      const dpp::confirmation_callback_t &cb);

// message_create that explains failures instead of logging a bare error code.
void send_message_logged(dpp::cluster &bot, const dpp::message &msg,
                         const std::string &what);
#include "common.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <variant>

std::string replace_all(std::string str, const std::string &from,
                        const std::string &to) {
  if (from.empty())
    return str;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

bool is_admin(const dpp::interaction &cmd) {
  return cmd.get_resolved_permission(cmd.usr.id).has(dpp::p_administrator);
}

bool has_access(Database &db, const dpp::interaction &cmd,
                const std::string &feature) {
  if (is_admin(cmd))
    return true;
  auto allowed = db.get_permission_roles(cmd.guild_id, feature);
  if (allowed.empty())
    return false;
  for (const auto &role : cmd.member.get_roles()) {
    if (std::find(allowed.begin(), allowed.end(),
                  static_cast<uint64_t>(role)) != allowed.end())
      return true;
  }
  return false;
}

std::string form_field(const dpp::form_submit_t &event, const std::string &id) {
  for (const auto &c : event.components) {
    if (c.custom_id == id && std::holds_alternative<std::string>(c.value))
      return std::get<std::string>(c.value);
  }
  return "";
}

dpp::message ephemeral_text(const std::string &text) {
  return dpp::message(text).set_flags(dpp::m_ephemeral);
}

dpp::message no_permission_message() {
  return ephemeral_text("You don't have permission to use this.");
}

ChannelAccess check_channel_access(dpp::cluster &bot, uint64_t guild_id,
                                   uint64_t channel_id) {
  ChannelAccess out;
  dpp::guild *g = dpp::find_guild(guild_id);
  dpp::channel *ch = dpp::find_channel(channel_id);
  if (!g || !ch)
    return out;
  auto me = g->members.find(bot.me.id);
  if (me == g->members.end())
    return out; // our own member isn't cached, so we can't tell

  dpp::permission perms = g->permission_overwrites(me->second, *ch);
  out.known = true;
  if (perms.has(dpp::p_administrator))
    return out;

  struct Need {
    dpp::permissions p;
    const char *name;
  };
  const Need needed[] = {{dpp::p_view_channel, "View Channel"},
                         {dpp::p_send_messages, "Send Messages"},
                         {dpp::p_embed_links, "Embed Links"},
                         {dpp::p_read_message_history, "Read Message History"}};
  for (const auto &n : needed) {
    if (!perms.has(n.p))
      out.missing.push_back(n.name);
  }
  return out;
}

static std::string join_names(const std::vector<std::string> &names) {
  std::string s;
  for (size_t i = 0; i < names.size(); ++i)
    s += (i ? ", " : "") + names[i];
  return s;
}

std::string channel_warning(dpp::cluster &bot, uint64_t guild_id,
                            uint64_t channel_id) {
  ChannelAccess a = check_channel_access(bot, guild_id, channel_id);
  if (!a.known || a.missing.empty())
    return "";
  return "⚠️ I can't fully use <#" + std::to_string(channel_id) +
         "> yet. Missing: " + join_names(a.missing) +
         ". Open that channel's settings, go to Permissions, add my role and "
         "allow those. It starts working right away, no need to pick the "
         "channel again.";
}

std::string channel_access_report(dpp::cluster &bot, Database &db,
                                  uint64_t guild_id) {
  struct Entry {
    std::string label;
    uint64_t id;
  };
  auto kv_id = [&](const std::string &key) -> uint64_t {
    std::string v = db.kv_get(guild_id, key);
    try {
      return v.empty() ? 0 : std::stoull(v);
    } catch (...) {
      return 0;
    }
  };
  std::vector<Entry> entries = {
      {"Notifications", db.get_notification_channel(guild_id)},
      {"XP leaderboard", db.get_xp_leaderboard_channel(guild_id)},
      {"Welcome messages", kv_id("welcome_channel")},
      {"Mod log", kv_id("modlog_channel")}};

  std::string out;
  for (const auto &e : entries) {
    if (e.id == 0)
      continue;
    ChannelAccess a = check_channel_access(bot, guild_id, e.id);
    out += e.label + " <#" + std::to_string(e.id) + ">: ";
    if (!a.known)
      out += "could not check\n";
    else if (a.missing.empty())
      out += "OK\n";
    else
      out += "missing " + join_names(a.missing) + "\n";
  }
  return out;
}

void log_send_failure(dpp::cluster &bot, uint64_t channel_id,
                      const std::string &what,
                      const dpp::confirmation_callback_t &cb) {
  static std::mutex m;
  static std::map<uint64_t, std::chrono::steady_clock::time_point> last;
  auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m);
    auto it = last.find(channel_id);
    if (it != last.end() && now - it->second < std::chrono::minutes(30))
      return;
    last[channel_id] = now;
  }

  auto err = cb.get_error();
  std::string text = "Could not send " + what + " in channel " +
                     std::to_string(channel_id) + ": " + err.message +
                     " (error " + std::to_string(err.code) + ").";
  if (err.code == 50013 || err.code == 50001) {
    text += " The bot lacks permission in that channel. Check the channel's "
            "permission overrides for the bot's role: it needs View Channel, "
            "Send Messages, Embed Links and Read Message History.";
    dpp::channel *ch = dpp::find_channel(channel_id);
    if (ch) {
      ChannelAccess a = check_channel_access(bot, ch->guild_id, channel_id);
      if (a.known && !a.missing.empty())
        text += " Missing right now: " + join_names(a.missing) + ".";
    }
    text += " (Further failures for this channel are hidden for 30 minutes.)";
  }
  bot.log(dpp::ll_error, text);
}

void send_message_logged(dpp::cluster &bot, const dpp::message &msg,
                         const std::string &what) {
  uint64_t channel_id = msg.channel_id;
  bot.message_create(msg, [&bot, channel_id, what](const dpp::confirmation_callback_t &cb) {
    if (cb.is_error())
      log_send_failure(bot, channel_id, what, cb);
  });
}

#include "module.h"
#include <ctime>
#include <variant>

namespace {

const char *kFeature = "moderation";

std::string guild_name(uint64_t guild_id) {
  dpp::guild *g = dpp::find_guild(guild_id);
  return g ? g->name : "the server";
}

std::string get_reason(const dpp::slashcommand_t &event) {
  auto p = event.get_parameter("reason");
  if (std::holds_alternative<std::string>(p) && !std::get<std::string>(p).empty()) {
    std::string r = std::get<std::string>(p);
    if (r.size() > 400)
      r.resize(400);
    return r;
  }
  return "No reason given";
}

void log_action(Ctx &ctx, uint64_t guild_id, const std::string &action,
                uint64_t target, uint64_t mod, const std::string &reason) {
  std::string ch = ctx.db.kv_get(guild_id, "modlog_channel");
  if (ch.empty())
    return;
  dpp::embed e = dpp::embed()
                     .set_color(0xE67E22)
                     .set_title(action)
                     .add_field("User", "<@" + std::to_string(target) + ">", true)
                     .add_field("Moderator", "<@" + std::to_string(mod) + ">", true)
                     .add_field("Reason", reason)
                     .set_timestamp(time(nullptr));
  send_message_logged(ctx.bot, dpp::message(std::stoull(ch), e), "a mod log entry");
}

// Best effort: users with closed DMs are simply skipped.
void dm_user(Ctx &ctx, uint64_t user, const std::string &text) {
  ctx.bot.direct_message_create(
      user, dpp::message(text), [](const dpp::confirmation_callback_t &) {});
}

class ModerationModule : public Module {
public:
  std::string id() const override { return "moderation"; }
  std::string name() const override { return "Moderation"; }
  std::string description() const override {
    return "Warn, timeout, kick, ban, purge, with a mod log channel";
  }

  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    auto user_opt = [] {
      return dpp::command_option(dpp::co_user, "user", "Target member", true);
    };
    auto reason_opt = [] {
      return dpp::command_option(dpp::co_string, "reason", "Reason", false);
    };

    dpp::slashcommand warn("warn", "Warn a member", a);
    warn.add_option(user_opt());
    warn.add_option(reason_opt());
    v.push_back(warn);

    dpp::slashcommand warnings("warnings", "Show a member's warnings", a);
    warnings.add_option(user_opt());
    v.push_back(warnings);

    dpp::slashcommand clear("clearwarnings", "Clear a member's warnings", a);
    clear.add_option(user_opt());
    v.push_back(clear);

    dpp::slashcommand to("timeout", "Time a member out", a);
    to.add_option(user_opt());
    to.add_option(dpp::command_option(dpp::co_integer, "minutes",
                                      "Length in minutes (max 40320)", true)
                      .set_min_value(1)
                      .set_max_value(40320));
    to.add_option(reason_opt());
    v.push_back(to);

    dpp::slashcommand uto("untimeout", "Remove a timeout", a);
    uto.add_option(user_opt());
    v.push_back(uto);

    dpp::slashcommand kick("kick", "Kick a member", a);
    kick.add_option(user_opt());
    kick.add_option(reason_opt());
    v.push_back(kick);

    dpp::slashcommand ban("ban", "Ban a member", a);
    ban.add_option(user_opt());
    ban.add_option(reason_opt());
    ban.add_option(dpp::command_option(dpp::co_integer, "delete_days",
                                       "Days of messages to delete (0-7)", false)
                       .set_min_value(0)
                       .set_max_value(7));
    v.push_back(ban);

    dpp::slashcommand unban("unban", "Unban a user by ID", a);
    unban.add_option(
        dpp::command_option(dpp::co_string, "user_id", "The user's ID", true));
    v.push_back(unban);

    dpp::slashcommand purge("purge", "Delete recent messages in this channel", a);
    purge.add_option(dpp::command_option(dpp::co_integer, "amount",
                                         "How many (1-100)", true)
                         .set_min_value(1)
                         .set_max_value(100));
    v.push_back(purge);

    dpp::slashcommand modlog("modlog", "Set the mod log channel", a);
    modlog.add_option(dpp::command_option(dpp::co_channel, "channel",
                                          "Where to log actions", true)
                          .add_channel_type(dpp::CHANNEL_TEXT));
    v.push_back(modlog);
    return v;
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    const std::string cmd = event.command.get_command_name();
    static const char *mine[] = {"warn",  "warnings", "clearwarnings", "timeout",
                                 "untimeout", "kick", "ban", "unban", "purge",
                                 "modlog"};
    bool ours = false;
    for (auto *m : mine)
      ours = ours || cmd == m;
    if (!ours)
      return false;

    if (!has_access(ctx.db, event.command, kFeature)) {
      event.reply(no_permission_message());
      return true;
    }
    uint64_t guild = event.command.guild_id;
    uint64_t mod = event.command.usr.id;
    Ctx *c = &ctx;

    if (cmd == "modlog") {
      auto ch = std::get<dpp::snowflake>(event.get_parameter("channel"));
      ctx.db.kv_set(guild, "modlog_channel", std::to_string(uint64_t(ch)));
      std::string warn = channel_warning(ctx.bot, guild, uint64_t(ch));
      event.reply(ephemeral_text("Mod actions will be logged in <#" +
                                 std::to_string(uint64_t(ch)) + ">." +
                                 (warn.empty() ? "" : "\n\n" + warn)));
      return true;
    }

    if (cmd == "purge") {
      int64_t amount = std::get<int64_t>(event.get_parameter("amount"));
      uint64_t channel = event.command.channel_id;
      event.thinking(true);
      ctx.bot.messages_get(
          channel, 0, 0, 0, static_cast<uint64_t>(amount),
          [c, event, channel, guild, mod](const dpp::confirmation_callback_t &cb) {
            if (cb.is_error()) {
              event.edit_original_response(
                  dpp::message("Couldn't read messages here. Check my "
                               "permissions."));
              return;
            }
            auto msgs = std::get<dpp::message_map>(cb.value);
            std::vector<dpp::snowflake> ids;
            time_t cutoff = time(nullptr) - 13 * 24 * 3600; // bulk delete limit
            for (const auto &[id, m] : msgs) {
              if (static_cast<time_t>(id.get_creation_time()) > cutoff)
                ids.push_back(id);
            }
            if (ids.empty()) {
              event.edit_original_response(
                  dpp::message("Nothing to delete (messages older than 14 "
                               "days can't be bulk deleted)."));
              return;
            }
            size_t n = ids.size();
            auto done = [c, event, n, guild, mod,
                         channel](const dpp::confirmation_callback_t &d) {
              if (d.is_error()) {
                event.edit_original_response(
                    dpp::message("I couldn't delete those. Check that I have "
                                 "Manage Messages."));
                return;
              }
              event.edit_original_response(
                  dpp::message("Deleted " + std::to_string(n) + " message(s)."));
              log_action(*c, guild, "Purge in <#" + std::to_string(channel) + ">",
                         mod, mod, std::to_string(n) + " messages deleted");
            };
            if (n == 1)
              c->bot.message_delete(ids[0], channel, done);
            else
              c->bot.message_delete_bulk(ids, channel, done);
          });
      return true;
    }

    if (cmd == "unban") {
      std::string raw = std::get<std::string>(event.get_parameter("user_id"));
      uint64_t uid = 0;
      try {
        uid = std::stoull(raw);
      } catch (...) {
      }
      if (uid == 0) {
        event.reply(ephemeral_text("That doesn't look like a user ID."));
        return true;
      }
      event.thinking(true);
      ctx.bot.guild_ban_delete(
          guild, uid, [c, event, guild, uid, mod](const dpp::confirmation_callback_t &cb) {
            if (cb.is_error()) {
              event.edit_original_response(
                  dpp::message("Couldn't unban that user. Are they banned?"));
              return;
            }
            event.edit_original_response(
                dpp::message("Unbanned <@" + std::to_string(uid) + ">."));
            log_action(*c, guild, "Unban", uid, mod, "-");
          });
      return true;
    }

    // Everything below targets a member.
    uint64_t target = std::get<dpp::snowflake>(event.get_parameter("user"));
    if (target == mod || target == ctx.bot.me.id) {
      event.reply(ephemeral_text("Pick someone else."));
      return true;
    }
    std::string reason = get_reason(event);
    std::string server = guild_name(guild);

    if (cmd == "warn") {
      int total = ctx.db.add_warning(guild, target, mod, reason);
      dm_user(ctx, target, "You were warned in " + server + ": " + reason);
      log_action(ctx, guild, "Warning (#" + std::to_string(total) + ")", target,
                 mod, reason);
      event.reply(ephemeral_text("Warned <@" + std::to_string(target) +
                                 ">. They now have " + std::to_string(total) +
                                 " warning(s)."));
    } else if (cmd == "warnings") {
      auto list = ctx.db.get_warnings(guild, target);
      if (list.empty()) {
        event.reply(ephemeral_text("<@" + std::to_string(target) +
                                   "> has no warnings."));
        return true;
      }
      std::string text = "**Warnings for <@" + std::to_string(target) + ">** (" +
                         std::to_string(list.size()) + ")\n";
      size_t start = list.size() > 10 ? list.size() - 10 : 0;
      for (size_t i = start; i < list.size(); ++i) {
        text += "#" + std::to_string(list[i].id) + " <t:" +
                std::to_string(list[i].created_at) + ":d> by <@" +
                std::to_string(list[i].mod_id) + ">: " + list[i].reason + "\n";
      }
      event.reply(ephemeral_text(text));
    } else if (cmd == "clearwarnings") {
      int n = ctx.db.clear_warnings(guild, target);
      log_action(ctx, guild, "Warnings cleared", target, mod,
                 std::to_string(n) + " removed");
      event.reply(ephemeral_text("Cleared " + std::to_string(n) +
                                 " warning(s) for <@" + std::to_string(target) +
                                 ">."));
    } else if (cmd == "timeout") {
      int64_t minutes = std::get<int64_t>(event.get_parameter("minutes"));
      event.thinking(true);
      dm_user(ctx, target, "You were timed out in " + server + " for " +
                               std::to_string(minutes) + " minute(s): " + reason);
      ctx.bot.guild_member_timeout(
          guild, target, time(nullptr) + minutes * 60,
          [c, event, guild, target, mod, reason,
           minutes](const dpp::confirmation_callback_t &cb) {
            if (cb.is_error()) {
              event.edit_original_response(dpp::message(
                  "Couldn't time them out. My role must be above theirs and I "
                  "need Moderate Members."));
              return;
            }
            event.edit_original_response(dpp::message(
                "Timed out <@" + std::to_string(target) + "> for " +
                std::to_string(minutes) + " minute(s)."));
            log_action(*c, guild, "Timeout (" + std::to_string(minutes) + " min)",
                       target, mod, reason);
          });
    } else if (cmd == "untimeout") {
      event.thinking(true);
      ctx.bot.guild_member_timeout_remove(
          guild, target,
          [c, event, guild, target, mod](const dpp::confirmation_callback_t &cb) {
            if (cb.is_error()) {
              event.edit_original_response(
                  dpp::message("Couldn't remove that timeout."));
              return;
            }
            event.edit_original_response(
                dpp::message("Timeout removed for <@" + std::to_string(target) + ">."));
            log_action(*c, guild, "Timeout removed", target, mod, "-");
          });
    } else if (cmd == "kick" || cmd == "ban") {
      bool is_ban = (cmd == "ban");
      uint32_t del = 0;
      if (is_ban) {
        auto p = event.get_parameter("delete_days");
        if (std::holds_alternative<int64_t>(p))
          del = static_cast<uint32_t>(std::get<int64_t>(p)) * 86400;
      }
      event.thinking(true);
      dm_user(ctx, target, std::string("You were ") + (is_ban ? "banned from " : "kicked from ") +
                               server + ": " + reason);
      auto done = [c, event, guild, target, mod, reason,
                   is_ban](const dpp::confirmation_callback_t &cb) {
        if (cb.is_error()) {
          event.edit_original_response(dpp::message(
              std::string("Couldn't ") + (is_ban ? "ban" : "kick") +
              " them. My role must be above theirs and I need the right "
              "permission."));
          return;
        }
        event.edit_original_response(
            dpp::message(std::string(is_ban ? "Banned <@" : "Kicked <@") +
                         std::to_string(target) + ">."));
        log_action(*c, guild, is_ban ? "Ban" : "Kick", target, mod, reason);
      };
      if (is_ban)
        ctx.bot.guild_ban_add(guild, target, del, done);
      else
        ctx.bot.guild_member_kick(guild, target, done);
    }
    return true;
  }
};

} // namespace

std::unique_ptr<Module> make_moderation_module() {
  return std::make_unique<ModerationModule>();
}

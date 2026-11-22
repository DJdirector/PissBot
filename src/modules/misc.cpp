// Misc: pointless fun for moderators to mess with people. All of it is gated by
// the "misc" feature in /permissions (administrators only until you give it to
// a role), and every command answers privately so nobody can tell who used it.
//   /say        send a message as the bot into any channel
//   /bother     react to everything one member sends, for a while
//   /unbother   stop bothering a member
//   /fakeban    announce a (very fake) ban
#include "misc.h"
#include "module.h"
#include <ctime>
#include <random>

namespace {

const char *kFeature = "misc";

constexpr int kDefaultBotherMinutes = 30;
constexpr int kMaxBotherMinutes = 24 * 60;

int64_t now_s() { return static_cast<int64_t>(std::time(nullptr)); }

std::mt19937 &rng() {
  thread_local std::mt19937 gen{std::random_device{}()};
  return gen;
}

// Slash command options are one line, so "\n" in the text becomes a new line.
std::string unescape_newlines(std::string text) {
  size_t pos = 0;
  while ((pos = text.find("\\n", pos)) != std::string::npos) {
    text.replace(pos, 2, "\n");
    ++pos;
  }
  return text;
}

bool is_message_id(const std::string &s) {
  return !s.empty() && s.size() <= 19 &&
         s.find_first_not_of("0123456789") == std::string::npos;
}

// How a reaction shows up in text: custom emoji need their <:name:id> form.
std::string emoji_display(const std::string &reaction) {
  size_t colon = reaction.find(':');
  return colon == std::string::npos ? reaction : "<:" + reaction + ">";
}

class MiscModule : public Module {
public:
  std::string id() const override { return "misc"; }
  std::string name() const override { return "Misc"; }
  std::string description() const override {
    return "Fun for mods: anonymous messages, bothering people, fake bans";
  }

  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;

    dpp::slashcommand say("say", "Send a message as the bot (nobody sees who)",
                          a);
    say.add_option(dpp::command_option(dpp::co_channel, "channel",
                                       "Where to send it", true)
                       .add_channel_type(dpp::CHANNEL_TEXT)
                       .add_channel_type(dpp::CHANNEL_ANNOUNCEMENT)
                       .add_channel_type(dpp::CHANNEL_PUBLIC_THREAD)
                       .add_channel_type(dpp::CHANNEL_PRIVATE_THREAD));
    say.add_option(dpp::command_option(dpp::co_string, "message",
                                       "What to say (write \\n for a new line)",
                                       true)
                       .set_max_length(2000));
    say.add_option(dpp::command_option(dpp::co_string, "reply_to",
                                       "ID of a message to reply to", false));
    v.push_back(say);

    dpp::slashcommand bother(
        "bother", "React to everything a member sends, for a while", a);
    bother.add_option(
        dpp::command_option(dpp::co_user, "user", "Who to bother", true));
    bother.add_option(dpp::command_option(dpp::co_string, "emoji",
                                          "The emoji to react with", true));
    bother.add_option(
        dpp::command_option(
            dpp::co_integer, "minutes",
            "How long, default " + std::to_string(kDefaultBotherMinutes), false)
            .set_min_value(int64_t(1))
            .set_max_value(int64_t(kMaxBotherMinutes)));
    v.push_back(bother);

    dpp::slashcommand unbother("unbother", "Stop bothering a member", a);
    unbother.add_option(
        dpp::command_option(dpp::co_user, "user", "Who to leave alone", true));
    v.push_back(unbother);

    dpp::slashcommand fakeban("fakeban", "Announce a fake ban for someone", a);
    fakeban.add_option(
        dpp::command_option(dpp::co_user, "user", "Who gets \"banned\"", true));
    fakeban.add_option(dpp::command_option(
        dpp::co_string, "reason", "Why (default: something absurd)", false));
    v.push_back(fakeban);
    return v;
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    const std::string cmd = event.command.get_command_name();
    if (cmd != "say" && cmd != "bother" && cmd != "unbother" &&
        cmd != "fakeban")
      return false;
    if (!has_access(ctx.db, event.command, kFeature)) {
      event.reply(no_permission_message());
      return true;
    }
    if (cmd == "say")
      say(ctx, event);
    else if (cmd == "bother")
      bother(ctx, event);
    else if (cmd == "unbother")
      unbother(ctx, event);
    else
      fakeban(ctx, event);
    return true;
  }

  // Reacts to a bothered member's messages. Never consumes the message, so XP,
  // answers and the rest still see it.
  bool on_message(Ctx &ctx, const dpp::message_create_t &event) override {
    const uint64_t guild = event.msg.guild_id;
    const uint64_t user = event.msg.author.id;
    const std::string emoji = ctx.db.bother_emoji(guild, user, now_s());
    if (emoji.empty())
      return false;
    Database &db = ctx.db;
    ctx.bot.message_add_reaction(
        event.msg, emoji,
        [&db, guild, user](const dpp::confirmation_callback_t &cb) {
          // 10014: Discord doesn't know that emoji, so stop trying.
          if (cb.is_error() && cb.get_error().code == 10014)
            db.bother_clear(guild, user);
        });
    return false;
  }

private:
  void say(Ctx &ctx, const dpp::slashcommand_t &event) {
    const uint64_t guild = event.command.guild_id;
    const uint64_t channel =
        std::get<dpp::snowflake>(event.get_parameter("channel"));
    const std::string text = unescape_newlines(
        std::get<std::string>(event.get_parameter("message")));

    dpp::message msg(channel, text);
    // Users and roles can be mentioned, @everyone and @here never.
    msg.set_allowed_mentions(true, true, false, false);

    auto reply = event.get_parameter("reply_to");
    if (std::holds_alternative<std::string>(reply)) {
      const std::string &id = std::get<std::string>(reply);
      if (!is_message_id(id)) {
        event.reply(ephemeral_text(
            "That isn't a message ID. Right-click a message and choose Copy "
            "Message ID (Developer Mode must be on)."));
        return;
      }
      msg.set_reference(std::stoull(id), guild, channel, false);
    }

    event.thinking(true);
    ctx.bot.message_create(
        msg, [event, channel](const dpp::confirmation_callback_t &cb) {
          if (!cb.is_error()) {
            event.edit_original_response(dpp::message("Sent."));
            return;
          }
          int code = cb.get_error().code;
          event.edit_original_response(dpp::message(
              (code == 50013 || code == 50001)
                  ? "I can't send messages in <#" + std::to_string(channel) +
                        ">. Give my role View Channel and Send Messages there."
                  : "Couldn't send it: " + cb.get_error().message));
        });
  }

  void bother(Ctx &ctx, const dpp::slashcommand_t &event) {
    const uint64_t guild = event.command.guild_id;
    const dpp::snowflake user =
        std::get<dpp::snowflake>(event.get_parameter("user"));
    const std::string raw = std::get<std::string>(event.get_parameter("emoji"));

    auto target = event.command.resolved.users.find(user);
    if (target != event.command.resolved.users.end() &&
        target->second.is_bot()) {
      event.reply(ephemeral_text("Bots don't get bothered."));
      return;
    }
    const std::string emoji = misc::parse_reaction_emoji(raw);
    if (emoji.empty()) {
      event.reply(ephemeral_text(
          "I need a single emoji, either a normal one or a custom one from a "
          "server I'm in."));
      return;
    }
    int64_t minutes = kDefaultBotherMinutes;
    auto m = event.get_parameter("minutes");
    if (std::holds_alternative<int64_t>(m))
      minutes = std::get<int64_t>(m);

    if (!ctx.db.bother_set(guild, user, emoji, now_s() + minutes * 60)) {
      event.reply(ephemeral_text("Couldn't save that."));
      return;
    }
    event.reply(ephemeral_text(
        "I'll react with " + emoji_display(emoji) + " to everything <@" +
        std::to_string(uint64_t(user)) + "> sends for " +
        std::to_string(minutes) + " minutes. Use /unbother to stop early."));
  }

  void unbother(Ctx &ctx, const dpp::slashcommand_t &event) {
    const dpp::snowflake user =
        std::get<dpp::snowflake>(event.get_parameter("user"));
    const bool was = ctx.db.bother_clear(event.command.guild_id, user);
    event.reply(ephemeral_text(
        was ? "Left <@" + std::to_string(uint64_t(user)) + "> alone."
            : "<@" + std::to_string(uint64_t(user)) +
                  "> wasn't being bothered."));
  }

  void fakeban(Ctx &ctx, const dpp::slashcommand_t &event) {
    const uint64_t channel = event.command.channel_id;
    const dpp::snowflake user =
        std::get<dpp::snowflake>(event.get_parameter("user"));
    std::string reason;
    auto r = event.get_parameter("reason");
    if (std::holds_alternative<std::string>(r))
      reason = std::get<std::string>(r);
    if (reason.empty())
      reason = misc::fake_ban_reason(rng());

    dpp::message msg(
        channel, "🔨 **<@" + std::to_string(uint64_t(user)) +
                     "> has been banned from the server.**\nReason: " + reason +
                     "\nSentence: " + misc::fake_ban_sentence(rng()) +
                     "\n\n*Just kidding. Probably.*");
    msg.set_allowed_mentions(true, false, false,
                             false); // ping them, no one else

    event.thinking(true);
    ctx.bot.message_create(
        msg, [event, channel](const dpp::confirmation_callback_t &cb) {
          event.edit_original_response(dpp::message(
              cb.is_error()
                  ? "I can't send messages in <#" + std::to_string(channel) +
                        ">. Give my role Send Messages there."
                  : "Done."));
        });
  }
};

} // namespace

std::unique_ptr<Module> make_misc_module() {
  return std::make_unique<MiscModule>();
}

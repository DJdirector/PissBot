// Verifies the channel permission check that explains Discord error 50013,
// using a fake guild placed in D++'s cache (no network involved).
#include "check.h"
#include "common.h"

int main() {
  const uint64_t BOT = 5000, GUILD = 1, EVERYONE = 1;
  dpp::cluster bot("dummy-token-not-used");
  bot.me.id = BOT;

  // @everyone can view, send, embed and read history.
  dpp::role everyone;
  everyone.id = EVERYONE;
  everyone.guild_id = GUILD;
  everyone.permissions = dpp::p_view_channel | dpp::p_send_messages |
                         dpp::p_embed_links | dpp::p_read_message_history;
  dpp::get_role_cache()->store(new dpp::role(everyone));

  dpp::guild *g = new dpp::guild();
  g->id = GUILD;
  g->owner_id = 9999;
  g->roles.push_back(EVERYONE);
  dpp::guild_member me;
  me.user_id = BOT;
  me.guild_id = GUILD;
  g->members[BOT] = me;
  dpp::get_guild_cache()->store(g);

  auto make_channel = [&](uint64_t id) {
    dpp::channel *c = new dpp::channel();
    c->id = id;
    c->guild_id = GUILD;
    return c;
  };

  // 10: no overrides -> everything allowed
  dpp::get_channel_cache()->store(make_channel(10));
  auto a = check_channel_access(bot, GUILD, 10);
  CHECK(a.known && a.missing.empty());
  CHECK(channel_warning(bot, GUILD, 10).empty());

  // 11: @everyone is denied Send Messages (a typical "channel made for the bot"
  // problem) -> the bot is missing exactly that
  dpp::channel *c11 = make_channel(11);
  c11->permission_overwrites.push_back(
      dpp::permission_overwrite(EVERYONE, 0, dpp::p_send_messages, dpp::ot_role));
  dpp::get_channel_cache()->store(c11);
  a = check_channel_access(bot, GUILD, 11);
  CHECK(a.known && a.missing.size() == 1 && a.missing[0] == "Send Messages");
  std::string warn = channel_warning(bot, GUILD, 11);
  CHECK(warn.find("Send Messages") != std::string::npos);
  CHECK(warn.find("<#11>") != std::string::npos);

  // 12: denied View Channel and Embed Links
  dpp::channel *c12 = make_channel(12);
  c12->permission_overwrites.push_back(dpp::permission_overwrite(
      EVERYONE, 0, dpp::p_view_channel | dpp::p_embed_links, dpp::ot_role));
  dpp::get_channel_cache()->store(c12);
  a = check_channel_access(bot, GUILD, 12);
  CHECK(a.known && a.missing.size() == 2);

  // 13: denied for @everyone, but the bot's own member override allows it
  dpp::channel *c13 = make_channel(13);
  c13->permission_overwrites.push_back(
      dpp::permission_overwrite(EVERYONE, 0, dpp::p_send_messages, dpp::ot_role));
  c13->permission_overwrites.push_back(dpp::permission_overwrite(
      BOT, dpp::p_send_messages, 0, dpp::ot_member));
  dpp::get_channel_cache()->store(c13);
  a = check_channel_access(bot, GUILD, 13);
  CHECK(a.known && a.missing.empty());

  // Unknown channel or guild: don't raise a false alarm
  a = check_channel_access(bot, GUILD, 999);
  CHECK(!a.known);
  CHECK(channel_warning(bot, GUILD, 999).empty());
  CHECK(!check_channel_access(bot, 777, 10).known);

  std::cout << "access_test passed" << std::endl;
}

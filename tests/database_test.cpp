#include "check.h"
#include "database.h"

static YouTubeSub make_sub(uint64_t guild, const std::string &id,
                           const std::string &video, const std::string &stream) {
  YouTubeSub s;
  s.guild_id = guild;
  s.yt_channel_id = id;
  s.video_message = video;
  s.stream_message = stream;
  s.ping_role_id = "";
  s.ignore_shorts = false;
  s.use_embed = false;
  return s;
}

int main() {
  const std::string path = temp_db_path("pissbot_database_test");
  {
    Database db(path);
    CHECK(db.init());

    // ---- XP, ranks, leaderboard guilds ----
    CHECK(db.add_xp(1, 10, 500));
    CHECK(db.add_xp(1, 11, 900));
    CHECK(db.add_xp(1, 12, 100));
    CHECK(db.get_xp(1, 10) == 500);
    CHECK(db.get_xp(2, 10) == 0);
    CHECK(db.get_rank(1, 11) == 1);
    CHECK(db.get_rank(1, 10) == 2);
    CHECK(db.get_rank(1, 12) == 3);
    CHECK(db.get_rank(1, 99) == 4);
    auto top = db.get_top_xp(1, 2);
    CHECK(top.size() == 2 && top[0].first == 11 && top[1].first == 10);
    CHECK(db.remove_xp(1, 10, 50));
    CHECK(db.get_xp(1, 10) == 450);
    // spend_xp is all or nothing
    CHECK(db.spend_xp(1, 10, 450));
    CHECK(db.get_xp(1, 10) == 0);
    CHECK(!db.spend_xp(1, 10, 1));
    CHECK(!db.spend_xp(1, 12, 101)); // has 100
    CHECK(db.get_xp(1, 12) == 100);
    CHECK(!db.spend_xp(1, 12, 0) && !db.spend_xp(1, 12, -5));
    CHECK(!db.spend_xp(1, 777, 1)); // unknown user
    CHECK(db.add_xp(1, 10, 450));

    // ---- piss tokens (separate from XP) ----
    CHECK(db.token_balance(1, 10) == 0);
    CHECK(db.token_add(1, 10, 100));
    CHECK(db.token_add(1, 10, 50));
    CHECK(db.token_balance(1, 10) == 150);
    CHECK(db.get_xp(1, 10) == 450); // XP untouched
    CHECK(db.token_spend(1, 10, 150));
    CHECK(db.token_balance(1, 10) == 0);
    CHECK(!db.token_spend(1, 10, 1) && !db.token_spend(1, 10, 0));
    CHECK(!db.token_spend(1, 555, 1) && db.token_balance(2, 10) == 0);
    // daily claim: pays once, then not until the cooldown has passed
    CHECK(db.token_last_daily(1, 20) == 0);
    CHECK(db.token_claim_daily(1, 20, 100, 1000000, 86400));
    CHECK(db.token_balance(1, 20) == 100 && db.token_last_daily(1, 20) == 1000000);
    CHECK(!db.token_claim_daily(1, 20, 100, 1000000 + 86399, 86400));
    CHECK(db.token_balance(1, 20) == 100);
    CHECK(db.token_claim_daily(1, 20, 100, 1000000 + 86400, 86400));
    CHECK(db.token_balance(1, 20) == 200);
    CHECK(db.token_add(1, 30, 5) && db.token_claim_daily(1, 30, 100, 2000000000, 86400));
    CHECK(db.token_balance(1, 30) == 105); // claim works on an existing row

    // ---- token admin helpers, leaderboard and casino members ----
    CHECK(db.token_set(1, 40, 500) && db.token_balance(1, 40) == 500);
    CHECK(db.token_set(1, 40, 0) && db.token_balance(1, 40) == 0);
    CHECK(!db.token_set(1, 40, -1));
    CHECK(db.token_set(1, 41, 70)); // set creates the account too
    CHECK(db.token_take(1, 41, 30) == 30 && db.token_balance(1, 41) == 40);
    CHECK(db.token_take(1, 41, 1000) == 40 && db.token_balance(1, 41) == 0);
    CHECK(db.token_take(1, 41, 5) == 0 && db.token_take(1, 41, 0) == 0);
    CHECK(db.token_take(1, 999, 5) == 0); // unknown user

    // Balances so far in guild 1: user 20 = 200, user 30 = 105, others 0.
    auto rich = db.token_top(1, 10);
    CHECK(rich.size() == 2); // zero balances are left out
    CHECK(rich[0].first == 20 && rich[0].second == 200);
    CHECK(rich[1].first == 30 && rich[1].second == 105);
    CHECK(db.token_top(1, 1).size() == 1);
    CHECK(db.token_top(2, 10).empty()); // other guilds are separate
    CHECK(db.token_rank(1, 20) == 1 && db.token_rank(1, 30) == 2);
    CHECK(db.token_rank(1, 41) == 0 && db.token_rank(2, 20) == 0);

    CHECK(db.casino_member_count(1) == 0);
    CHECK(db.casino_member_add(1, 20, 1000) && db.casino_member_add(1, 30, 1001));
    CHECK(db.casino_member_add(1, 20, 2000)); // joining twice is harmless
    CHECK(db.casino_member_count(1) == 2 && db.casino_member_count(2) == 0);
    CHECK(db.casino_member_remove(1, 20) && !db.casino_member_remove(1, 20));
    CHECK(db.casino_member_count(1) == 1);

    CHECK(db.kv_guilds("casino_lb_channel").empty());
    CHECK(db.kv_set(7, "casino_lb_channel", "123") &&
          db.kv_set(8, "casino_lb_channel", "456") &&
          db.kv_set(9, "other_key", "1"));
    CHECK(db.kv_guilds("casino_lb_channel").size() == 2);
    CHECK(db.kv_set(7, "casino_lb_channel", ""));
    CHECK(db.kv_guilds("casino_lb_channel").size() == 1 &&
          db.kv_guilds("casino_lb_channel")[0] == 8);

    CHECK(db.set_xp_leaderboard_channel(1, 42));
    CHECK(db.set_xp_leaderboard_channel(2, 0));
    auto lb = db.get_leaderboard_guilds();
    CHECK(lb.size() == 1 && lb[0] == 1);

    // ---- level roles ----
    CHECK(db.add_level_role(1, 5, 777));
    CHECK(db.add_level_role(1, 2, 555));
    auto roles = db.get_level_roles(1);
    CHECK(roles.size() == 2 && roles[0].level == 2 && roles[1].role_id == 777);
    CHECK(db.add_level_role(1, 5, 778)); // same level replaces
    CHECK(db.get_level_roles(1)[1].role_id == 778);
    CHECK(db.remove_level_role(1, 5));
    CHECK(!db.remove_level_role(1, 5));

    // ---- custom commands, including hostile names ----
    CHECK(db.set_custom_command(1, "rules", "be nice $(user)"));
    CHECK(db.get_custom_command(1, "rules") == "be nice $(user)");
    CHECK(db.get_custom_command(2, "rules").empty());
    CHECK(db.set_custom_command(1, "x'; DROP TABLE user_xp;--", "y"));
    CHECK(db.list_custom_commands(1).size() == 2);
    CHECK(db.get_xp(1, 10) == 450); // table still intact
    CHECK(db.remove_custom_command(1, "rules"));
    CHECK(!db.remove_custom_command(1, "rules"));

    // ---- permissions ----
    CHECK(db.get_permission_roles(1, "youtube").empty());
    CHECK(db.set_permission_roles(1, "youtube", {30, 10, 20, 10}));
    CHECK((db.get_permission_roles(1, "youtube") ==
           std::vector<uint64_t>{10, 20, 30}));
    CHECK(db.get_permission_roles(2, "youtube").empty());
    CHECK(db.set_permission_roles(1, "youtube", {40}));
    CHECK((db.get_permission_roles(1, "youtube") == std::vector<uint64_t>{40}));
    CHECK(db.set_permission_roles(1, "youtube", {}));
    CHECK(db.get_permission_roles(1, "youtube").empty());

    // ---- answers ----
    CHECK(db.set_answer(1, "hi", "hello", false));
    CHECK(db.set_answer(1, "why", "because", true));
    CHECK(db.set_answer(2, "hi", "other guild", false));
    auto answers = db.get_answers(1);
    CHECK(answers.size() == 2 && !answers[0].contains && answers[1].contains);
    CHECK(db.set_answer(1, "hi", "updated", true)); // upsert
    CHECK(db.get_answers(1)[0].response == "updated" && db.get_answers(1)[0].contains);
    CHECK(db.remove_answer(1, "hi") && !db.remove_answer(1, "hi"));
    CHECK(db.get_answers(2).size() == 1);

    // ---- modules ----
    CHECK(db.get_enabled_modules(1).empty());
    CHECK(db.set_enabled_modules(1, {"levels", "social", "levels"}));
    CHECK((db.get_enabled_modules(1) == std::set<std::string>{"levels", "social"}));
    CHECK(db.set_enabled_modules(1, {"answers"}));
    CHECK((db.get_enabled_modules(1) == std::set<std::string>{"answers"}));
    CHECK(db.get_enabled_modules(2).empty());
    CHECK(db.set_enabled_modules(1, {}) && db.get_enabled_modules(1).empty());
    CHECK(db.get_xp(1, 11) == 900); // disabling a module keeps its data

    // ---- key/value settings ----
    CHECK(db.kv_get(1, "a").empty());
    CHECK(db.kv_set(1, "a", "hello") && db.kv_get(1, "a") == "hello");
    CHECK(db.kv_set(1, "a", "bye") && db.kv_get(1, "a") == "bye");
    CHECK(db.kv_get(2, "a").empty());
    CHECK(db.kv_set(1, "a", "") && db.kv_get(1, "a").empty()); // empty = unset

    // ---- counters ----
    CHECK(db.incr_counter(1, "cmd:x") == 1 && db.incr_counter(1, "cmd:x") == 2);
    CHECK(db.incr_counter(1, "cmd:y") == 1);
    CHECK(db.incr_counter(2, "cmd:x") == 1);

    // ---- warnings ----
    CHECK(db.add_warning(1, 10, 99, "spam") == 1);
    CHECK(db.add_warning(1, 10, 99, "x'); DROP TABLE warnings;--") == 2);
    CHECK(db.add_warning(1, 11, 99, "other") == 1);
    auto w = db.get_warnings(1, 10);
    CHECK(w.size() == 2 && w[0].reason == "spam" && w[0].mod_id == 99);
    CHECK(w[0].created_at > 1700000000);
    CHECK(db.clear_warnings(1, 10) == 2 && db.get_warnings(1, 10).empty());
    CHECK(db.get_warnings(1, 11).size() == 1);

    // ---- YouTube subscriptions with old-style placeholders (pre-migration) ----
    CHECK(db.add_youtube_channel(make_sub(
        3, "UCaaaaaaaaaaaaaaaaaaaaaa", "{role} new video {url}",
        "{role} {channel} is live {url}")));
    CHECK(db.set_custom_command(3, "old", "{user} said {args} in {channel}"));
    CHECK(db.set_answer(3, "yo", "hey {user} ({username}) in {channel}", false));
  }

  // Re-open the same file: init() runs migrations that rewrite {x} to $(x).
  {
    Database db(path);
    CHECK(db.init());
    auto subs = db.get_guild_youtube_subscriptions(3);
    CHECK(subs.size() == 1);
    CHECK(subs[0].video_message == "$(role) new video $(url)");
    CHECK(subs[0].stream_message == "$(role) $(channel) is live $(url)");
    CHECK(db.get_custom_command(3, "old") ==
          "$(user) said $(args) in $(channel)");
    CHECK(db.get_answers(3)[0].response ==
          "hey $(user) ($(username)) in $(channel)");
    // Running again changes nothing.
    Database db2(path);
    CHECK(db2.init());
    CHECK(db2.get_guild_youtube_subscriptions(3)[0].video_message ==
          "$(role) new video $(url)");
    // Data from before is still there.
    CHECK(db.get_xp(1, 11) == 900);
  }

  std::filesystem::remove(path);
  std::cout << "database_test passed" << std::endl;
}
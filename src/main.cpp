#include "answers.h"
#include "common.h"
#include "database.h"
#include "module.h"
#include "template.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <dpp/dpp.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>

static std::mutex state_mutex;
static std::set<std::string> pending_video_checks;

static bool pending_try_add(const std::string &id) {
  std::lock_guard<std::mutex> lock(state_mutex);
  return pending_video_checks.insert(id).second;
}

static void pending_remove(const std::string &id) {
  std::lock_guard<std::mutex> lock(state_mutex);
  pending_video_checks.erase(id);
}
static auto start_time = std::chrono::steady_clock::now();

static std::unordered_map<
    uint64_t,
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point>>
    text_xp_cooldowns;
static std::unordered_map<
    uint64_t,
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point>>
    voice_state_timers;

std::string process_custom_message(std::string str) {
  size_t pos = 0;
  while ((pos = str.find("\\n", pos)) != std::string::npos) {
    str.replace(pos, 2, "\n");
    pos += 1;
  }
  return str;
}


bool is_recent_upload(const std::string &iso_time, int max_age_minutes = 30) {
  std::tm tm = {};
  std::istringstream ss(iso_time);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail())
    return true;

  auto pub_time = std::chrono::system_clock::from_time_t(timegm(&tm));
  auto now = std::chrono::system_clock::now();
  auto age =
      std::chrono::duration_cast<std::chrono::minutes>(now - pub_time).count();

  return age <= max_age_minutes;
}

int level_for_xp(int xp) {
  return static_cast<int>(std::sqrt(std::max(0, xp) / 100.0));
}

int xp_for_level(int level) { return 100 * level * level; }

void grant_xp(dpp::cluster &bot, Database &db, uint64_t guild_id,
              uint64_t user_id, int amount, uint64_t announce_channel = 0) {
  int old_level = level_for_xp(db.get_xp(guild_id, user_id));
  if (!db.add_xp(guild_id, user_id, amount))
    return;
  int new_level = level_for_xp(db.get_xp(guild_id, user_id));
  if (new_level <= old_level)
    return;

  for (const auto &lr : db.get_level_roles(guild_id)) {
    if (lr.level <= new_level)
      bot.guild_member_add_role(guild_id, user_id, lr.role_id);
  }
  if (announce_channel != 0) {
    dpp::message msg(announce_channel, "🎉 <@" + std::to_string(user_id) +
                                           "> reached **level " +
                                           std::to_string(new_level) + "**!");
    msg.set_allowed_mentions(true, false, false, false);
    send_message_logged(bot, msg, "a level-up announcement");
  }
}


struct Feature {
  const char *key;
  const char *name;
  const char *commands;
};

static const Feature kFeatures[] = {
    {"setup", "Setup wizard", "/setup"},
    {"youtube", "YouTube notifications", "/youtube"},
    {"levels", "Level rewards", "/levelrole, /levelrole_remove"},
    {"commands", "Custom commands", "/command_add, /command_remove"},
    {"xp", "XP adjustments", "/add_xp, /remove_xp"},
    {"answers", "Answers (auto-replies)", "/answers"},
    {"moderation", "Moderation", "/warn, /timeout, /kick, /ban, /purge, /modlog"},
    {"welcome", "Welcome and autorole", "/welcome"},
    {"roles", "Role menus", "/rolemenu"},
};

static const Feature *find_feature(const std::string &key) {
  for (const auto &f : kFeatures) {
    if (key == f.key)
      return &f;
  }
  return nullptr;
}


static std::string role_list_text(const std::vector<uint64_t> &roles) {
  if (roles.empty())
    return "Administrators only";
  std::string out;
  for (size_t i = 0; i < roles.size(); ++i)
    out += (i ? ", <@&" : "<@&") + std::to_string(roles[i]) + ">";
  return out;
}

dpp::message permissions_list_view(Database &db, uint64_t guild_id,
                                   const std::string &notice = "") {
  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**Permissions**\nChoose which roles can use each part of the bot. "
          "Administrators can always use everything.\n\n";
  for (const auto &f : kFeatures) {
    text += "**" + std::string(f.name) + "** (`" + f.commands + "`)\n" +
            role_list_text(db.get_permission_roles(guild_id, f.key)) + "\n";
  }
  msg.set_content(text);

  dpp::component pick;
  pick.set_type(dpp::cot_selectmenu);
  pick.set_id("perm_pick");
  pick.set_placeholder("Select a feature to configure...");
  for (const auto &f : kFeatures)
    pick.add_select_option(dpp::select_option(f.name, f.key, f.commands));
  msg.add_component(dpp::component().add_component(pick));
  return msg;
}

dpp::message permissions_detail_view(Database &db, uint64_t guild_id,
                                     const std::string &key,
                                     const std::string &notice = "") {
  const Feature *f = find_feature(key);
  if (!f)
    return permissions_list_view(db, guild_id);
  auto roles = db.get_permission_roles(guild_id, key);

  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**" + std::string(f->name) + "** (`" + f->commands + "`)\n" +
          "Who can use it: " + role_list_text(roles) +
          "\n\nPick the roles below. Your selection replaces the current "
          "list. Administrators always have access.";
  msg.set_content(text);

  dpp::component role_select;
  role_select.set_type(dpp::cot_role_selectmenu);
  role_select.set_id(std::string("perm_roles:") + key);
  role_select.set_placeholder("Select roles that can use this...");
  role_select.set_max_values(25);
  msg.add_component(dpp::component().add_component(role_select));

  dpp::component btn_clear, btn_back;
  btn_clear.set_type(dpp::cot_button)
      .set_label("Administrators only")
      .set_style(dpp::cos_danger)
      .set_id(std::string("perm_clear:") + key)
      .set_disabled(roles.empty());
  btn_back.set_type(dpp::cot_button)
      .set_label("Back")
      .set_style(dpp::cos_secondary)
      .set_id("perm_back");
  msg.add_component(
      dpp::component().add_component(btn_clear).add_component(btn_back));
  return msg;
}

bool valid_command_name(const std::string &name) {
  if (name.empty() || name.size() > 32)
    return false;
  return std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-';
  });
}

// --- Guided setup wizard ---
// Steps: modules -> notify (if social is on) -> leaderboard (if levels is on)
// -> done
std::string next_setup_step(Database &db, uint64_t guild_id,
                            const std::string &current) {
  static const std::vector<std::string> order = {"modules", "notify",
                                                 "leaderboard", "done"};
  auto enabled = db.get_enabled_modules(guild_id);
  size_t idx = std::find(order.begin(), order.end(), current) - order.begin();
  for (size_t k = idx + 1; k < order.size(); ++k) {
    if (order[k] == "notify" && !enabled.count("social"))
      continue;
    if (order[k] == "leaderboard" && !enabled.count("levels"))
      continue;
    return order[k];
  }
  return "done";
}

dpp::message setup_step_message(Database &db, uint64_t guild_id,
                                const std::string &step) {
  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);

  if (step == "modules") {
    auto enabled = db.get_enabled_modules(guild_id);
    msg.set_content(
        "**Setup: choose your modules**\nTurn on the features you want. "
        "Commands for modules you leave off are not registered, so they "
        "won't clutter your server. Run /setup again any time to change "
        "this.");
    dpp::component select;
    select.set_type(dpp::cot_selectmenu);
    select.set_id("setup_modules");
    select.set_placeholder("Select the modules to enable...");
    select.set_min_values(0);
    select.set_max_values(static_cast<int>(all_modules().size()));
    for (const auto &m : all_modules()) {
      dpp::select_option opt(m->name(), m->id(), m->description());
      opt.set_default(enabled.count(m->id()) > 0);
      select.add_select_option(opt);
    }
    msg.add_component(dpp::component().add_component(select));
  } else if (step == "notify" || step == "leaderboard") {
    bool notify = (step == "notify");
    msg.set_content(
        notify ? "**Setup: notification channel**\nPick the channel where I "
                 "post YouTube uploads and live alerts. I'll drop a short "
                 "guide there so everyone knows what to do next."
               : "**Setup: XP leaderboard**\nPick the channel for the "
                 "auto-updating XP leaderboard. I'll explain how XP works "
                 "there.");
    dpp::component select;
    select.set_type(dpp::cot_channel_selectmenu);
    select.set_id(notify ? "select_notification_channel"
                         : "select_xp_leaderboard_channel");
    select.set_placeholder("Choose a text channel...");
    select.add_channel_type(dpp::CHANNEL_TEXT);

    dpp::component skip;
    skip.set_type(dpp::cot_button)
        .set_label("Skip this step")
        .set_style(dpp::cos_secondary)
        .set_id("setup_skip:" + step);

    msg.add_component(dpp::component().add_component(select));
    msg.add_component(dpp::component().add_component(skip));
    return msg;
  } else {
    auto enabled = db.get_enabled_modules(guild_id);
    std::string text = "**Setup complete**\n";
    if (enabled.empty()) {
      text += "No modules are enabled yet. Run /setup again to turn some on.\n";
    } else {
      text += "Enabled modules and their commands:\n";
      for (const auto &m : all_modules()) {
        if (!enabled.count(m->id()))
          continue;
        text += "• " + m->name() + ":";
        for (const auto &c : m->commands(dpp::snowflake(0)))
          text += " /" + c.name;
        text += "\n";
      }
    }
    text += "Always available: /setup /permissions /ping /bot_status\n"
            "Use /permissions to choose which roles can use each feature. "
            "Run /setup again any time to change modules or channels.";
    msg.set_content(text);
    return msg;
  }

  dpp::component skip;
  skip.set_type(dpp::cot_button)
      .set_label("Keep current")
      .set_style(dpp::cos_secondary)
      .set_id("setup_skip:" + step);
  msg.add_component(dpp::component().add_component(skip));
  return msg;
}

// --- YouTube dashboard ---
// Decodes the XML entities YouTube's feed uses in titles (&amp; etc.).
static std::string xml_unescape(std::string text) {
  static const std::pair<const char *, const char *> kEntities[] = {
      {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
      {"&#39;", "'"}, {"&apos;", "'"}, {"&amp;", "&"}};
  for (const auto &[from, to] : kEntities) {
    size_t pos = 0;
    const std::string f = from;
    while ((pos = text.find(f, pos)) != std::string::npos) {
      text.replace(pos, f.size(), to);
      pos += std::string(to).size();
    }
  }
  return text;
}

static std::string clip(const std::string &text, size_t max_chars) {
  std::string out = replace_all(text, "```", "` ` `");
  if (dpp::utility::utf8len(out) > max_chars)
    out = dpp::utility::utf8substr(out, 0, max_chars - 3) + "...";
  return out;
}

// Accepts a bare channel ID or any text/URL containing one (UC + 22 chars).
std::string extract_channel_id(const std::string &input) {
  size_t pos = 0;
  while ((pos = input.find("UC", pos)) != std::string::npos) {
    if (pos + 24 <= input.size()) {
      std::string cand = input.substr(pos, 24);
      bool ok = std::all_of(cand.begin(), cand.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-';
      });
      if (ok)
        return cand;
    }
    pos += 2;
  }
  return "";
}

bool find_sub(Database &db, uint64_t guild_id, const std::string &yt_id,
              YouTubeSub &out) {
  for (const auto &sub : db.get_guild_youtube_subscriptions(guild_id)) {
    if (sub.yt_channel_id == yt_id) {
      out = sub;
      return true;
    }
  }
  return false;
}

dpp::message youtube_list_view(Database &db, uint64_t guild_id,
                               const std::string &notice = "") {
  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  auto subs = db.get_guild_youtube_subscriptions(guild_id);
  uint64_t notify = db.get_notification_channel(guild_id);

  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**YouTube Notifications**\n";
  text += notify != 0 ? "Posting to <#" + std::to_string(notify) + ">\n"
                      : "No notification channel yet. Run `/setup` first so I "
                        "know where to post.\n";
  text += subs.empty() ? "\nNo channels tracked yet. Press **Add channel** to "
                         "start."
                       : "\nTracked channels: **" + std::to_string(subs.size()) +
                             "**. Pick one below to configure it, or add a "
                             "new one.";
  msg.set_content(text);

  if (!subs.empty()) {
    dpp::component pick;
    pick.set_type(dpp::cot_selectmenu);
    pick.set_id("yt_pick");
    pick.set_placeholder("Select a channel to configure...");
    int count = 0;
    for (const auto &sub : subs) {
      if (count++ >= 25)
        break;
      std::string desc =
          std::string("Role: ") + (sub.ping_role_id.empty() ? "none" : "set") +
          " | Shorts filter: " + (sub.ignore_shorts ? "on" : "off") +
          " | Embeds: " + (sub.use_embed ? "on" : "off");
      pick.add_select_option(
          dpp::select_option(sub.yt_channel_id, sub.yt_channel_id, desc));
    }
    msg.add_component(dpp::component().add_component(pick));
  }

  dpp::component add_btn;
  add_btn.set_type(dpp::cot_button)
      .set_label("Add channel")
      .set_style(dpp::cos_success)
      .set_id("yt_add");
  msg.add_component(dpp::component().add_component(add_btn));
  return msg;
}

dpp::message youtube_detail_view(Database &db, uint64_t guild_id,
                                 const std::string &yt_id,
                                 const std::string &notice = "") {
  YouTubeSub sub;
  if (!find_sub(db, guild_id, yt_id, sub))
    return youtube_list_view(db, guild_id, "That channel is no longer tracked.");

  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);

  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**Configuring YouTube channel** `" + yt_id + "`\n";
  text += "Ping role: " +
          (sub.ping_role_id.empty()
               ? std::string("none (uses @everyone)")
               : "<@&" + sub.ping_role_id + ">") +
          "\n";
  text += std::string("Shorts filter: ") +
          (sub.ignore_shorts ? "**on** (Shorts are skipped)"
                             : "**off** (Shorts are posted)") +
          "\n";
  text += std::string("Rich embeds: ") +
          (sub.use_embed ? "**on**" : "**off** (plain text)") + "\n\n";
  text += "Video message:\n```\n" + clip(sub.video_message, 300) + "\n```\n";
  text += "Live stream message:\n```\n" + clip(sub.stream_message, 300) +
          "\n```\nVariables: `$(url)` `$(channel)` `$(role)` plus `$(choose a|b)` and more";
  msg.set_content(text);

  dpp::component role_select;
  role_select.set_type(dpp::cot_role_selectmenu);
  role_select.set_id("yt_role:" + yt_id);
  role_select.set_placeholder("Select a role to ping...");
  msg.add_component(dpp::component().add_component(role_select));

  dpp::component btn_msgs, btn_shorts, btn_embed;
  btn_msgs.set_type(dpp::cot_button)
      .set_label("Edit messages")
      .set_style(dpp::cos_primary)
      .set_id("yt_msgs:" + yt_id);
  btn_shorts.set_type(dpp::cot_button)
      .set_label(sub.ignore_shorts ? "Allow Shorts" : "Ignore Shorts")
      .set_style(dpp::cos_secondary)
      .set_id("yt_shorts:" + yt_id);
  btn_embed.set_type(dpp::cot_button)
      .set_label(sub.use_embed ? "Use plain text" : "Use rich embeds")
      .set_style(dpp::cos_secondary)
      .set_id("yt_embed:" + yt_id);
  msg.add_component(dpp::component()
                        .add_component(btn_msgs)
                        .add_component(btn_shorts)
                        .add_component(btn_embed));

  dpp::component btn_clear, btn_remove, btn_back;
  btn_clear.set_type(dpp::cot_button)
      .set_label("Clear ping role")
      .set_style(dpp::cos_secondary)
      .set_id("yt_clearrole:" + yt_id)
      .set_disabled(sub.ping_role_id.empty());
  btn_remove.set_type(dpp::cot_button)
      .set_label("Remove channel")
      .set_style(dpp::cos_danger)
      .set_id("yt_remove:" + yt_id);
  btn_back.set_type(dpp::cot_button)
      .set_label("Back")
      .set_style(dpp::cos_secondary)
      .set_id("yt_back");
  msg.add_component(dpp::component()
                        .add_component(btn_clear)
                        .add_component(btn_remove)
                        .add_component(btn_back));
  return msg;
}

static bool find_answer(Database &db, uint64_t guild_id,
                        const std::string &trigger, Answer &out) {
  for (const auto &a : db.get_answers(guild_id)) {
    if (a.trigger == trigger) {
      out = a;
      return true;
    }
  }
  return false;
}

dpp::message answers_list_view(Database &db, uint64_t guild_id,
                               const std::string &notice = "") {
  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  auto answers = db.get_answers(guild_id);

  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**Answers**\nThe bot replies in the channel, visible to everyone, "
          "when someone sends a matching message (like `hi`). Each answer has "
          "a 15 second cooldown.\n";
  text += "Slots used: **" + std::to_string(answers.size()) + "/" +
          std::to_string(kMaxAnswers) + "**\n";
  text += answers.empty() ? "\nNo answers yet. Press **Add answer** to start."
                          : "\nPick one below to edit it, or add a new one.";
  msg.set_content(text);

  if (!answers.empty()) {
    dpp::component pick;
    pick.set_type(dpp::cot_selectmenu);
    pick.set_id("ans_pick");
    pick.set_placeholder("Select an answer to edit...");
    for (const auto &a : answers) {
      std::string desc =
          std::string(a.contains ? "Anywhere" : "Whole message") + " | " +
          replace_all(clip(a.response, 50), "\n", " ");
      pick.add_select_option(dpp::select_option(a.trigger, a.trigger, desc));
    }
    msg.add_component(dpp::component().add_component(pick));
  }

  dpp::component add_btn;
  add_btn.set_type(dpp::cot_button)
      .set_label("Add answer")
      .set_style(dpp::cos_success)
      .set_id("ans_add")
      .set_disabled(answers.size() >= kMaxAnswers);
  msg.add_component(dpp::component().add_component(add_btn));
  return msg;
}

dpp::message answers_detail_view(Database &db, uint64_t guild_id,
                                 const std::string &trigger,
                                 const std::string &notice = "") {
  Answer a;
  if (!find_answer(db, guild_id, trigger, a))
    return answers_list_view(db, guild_id, "That answer no longer exists.");

  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**Answer for** `" + clip(a.trigger, 60) + "`\n";
  text += a.contains
              ? "Match: **anywhere** (replies when a message contains this as "
                "a word or phrase)\n\n"
              : "Match: **whole message** (replies only when the message is "
                "just this)\n\n";
  text += "Reply:\n```\n" + clip(a.response, 600) +
          "\n```\nVariables: `$(user)` `$(username)` `$(channel)` `$(choose a|b)` and more (see /command_help)";
  msg.set_content(text);

  dpp::component btn_edit, btn_mode, btn_remove, btn_back;
  btn_edit.set_type(dpp::cot_button)
      .set_label("Edit reply")
      .set_style(dpp::cos_primary)
      .set_id("ans_edit:" + a.trigger);
  btn_mode.set_type(dpp::cot_button)
      .set_label(a.contains ? "Match whole message only"
                            : "Match anywhere in message")
      .set_style(dpp::cos_secondary)
      .set_id("ans_mode:" + a.trigger);
  btn_remove.set_type(dpp::cot_button)
      .set_label("Remove")
      .set_style(dpp::cos_danger)
      .set_id("ans_remove:" + a.trigger);
  btn_back.set_type(dpp::cot_button)
      .set_label("Back")
      .set_style(dpp::cos_secondary)
      .set_id("ans_back");
  msg.add_component(dpp::component()
                        .add_component(btn_edit)
                        .add_component(btn_mode)
                        .add_component(btn_remove)
                        .add_component(btn_back));
  return msg;
}

static TemplateContext message_context(Database &db, uint64_t guild_id,
                                       const dpp::message &msg,
                                       const std::string &args,
                                       const std::string &counter_key) {
  TemplateContext tc;
  tc.user_id = std::to_string(uint64_t(msg.author.id));
  tc.user_name = msg.author.username;
  tc.channel_id = std::to_string(uint64_t(msg.channel_id));
  dpp::guild *g = dpp::find_guild(guild_id);
  tc.server_name = g ? g->name : "this server";
  tc.args = args;
  Database *dbp = &db;
  tc.next_count = [dbp, guild_id, counter_key]() {
    return dbp->incr_counter(guild_id, counter_key);
  };
  return tc;
}

// Which module owns each legacy command that is still handled in main().
static const std::map<std::string, std::string> kCommandModule = {
    {"xp", "levels"},          {"inspect_xp", "levels"},
    {"add_xp", "levels"},      {"remove_xp", "levels"},
    {"xp_top", "levels"},      {"rank", "levels"},
    {"levelrole", "levels"},   {"levelrole_remove", "levels"},
    {"levelrole_list", "levels"},
    {"youtube", "social"},     {"youtube_preview", "social"},
    {"command_add", "commands"}, {"command_remove", "commands"},
    {"command_list", "commands"}, {"command_help", "commands"},
    {"answers", "answers"}};

int main() {
  const char *token_env = std::getenv("BOT_TOKEN");
  if (!token_env) {
    std::cerr << "Error: BOT_TOKEN environment variable is not set!"
              << std::endl;
    return 1;
  }

  Database db("bot_data.db");
  if (!db.init()) {
    std::cerr << "Failed to initialize database tables!" << std::endl;
    return 1;
  }

  uint32_t intents = dpp::i_default_intents | dpp::i_guild_members |
                     dpp::i_message_content | dpp::i_guild_voice_states;
  dpp::cluster bot(token_env, intents);

  bot.on_log(dpp::utility::cout_logger());

  Ctx ctx{bot, db};

  // --- Message Listener: modules, custom commands, answers, chat XP ---
  bot.on_message_create([&ctx](const dpp::message_create_t &event) {
    if (event.msg.author.is_bot() || event.msg.guild_id == 0)
      return;
    Database &db = ctx.db;
    dpp::cluster &bot = ctx.bot;

    uint64_t guild_id = event.msg.guild_id;
    uint64_t user_id = event.msg.author.id;
    auto enabled = db.get_enabled_modules(guild_id);
    if (enabled.empty())
      return;

    for (const auto &m : all_modules()) {
      if (enabled.count(m->id()) && m->on_message(ctx, event))
        return;
    }

    const std::string &content = event.msg.content;
    if (enabled.count("commands") && content.size() > 1 && content[0] == '!') {
      size_t space = content.find(' ');
      std::string name = content.substr(
          1, space == std::string::npos ? std::string::npos : space - 1);
      std::string args =
          space == std::string::npos ? "" : content.substr(space + 1);
      std::transform(name.begin(), name.end(), name.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      std::string response = db.get_custom_command(guild_id, name);
      if (!response.empty()) {
        TemplateContext tc =
            message_context(db, guild_id, event.msg, args, "cmd:" + name);
        dpp::message reply(event.msg.channel_id, render_template(response, tc));
        // Only user mentions resolve, so $(query) can't ping roles/@everyone.
        reply.set_allowed_mentions(true, false, false, false);
        send_message_logged(bot, reply, "a custom command reply");
        return;
      }
    }

    // Answers: plain-message auto-replies (XP is still awarded below).
    if (enabled.count("answers")) {
      const auto answers = db.get_answers(guild_id);
      if (!answers.empty()) {
        const Answer *hit = match_answer(answers, content);
        if (hit && answer_ready(guild_id, hit->trigger)) {
          TemplateContext tc = message_context(db, guild_id, event.msg, "",
                                               "answer:" + hit->trigger);
          dpp::message reply(event.msg.channel_id,
                             render_template(hit->response, tc));
          reply.set_allowed_mentions(true, false, false, false);
          send_message_logged(bot, reply, "an answer");
        }
      }
    }

    if (!enabled.count("levels"))
      return;
    auto now = std::chrono::steady_clock::now();
    {
      std::lock_guard<std::mutex> lock(state_mutex);
      auto &user_map = text_xp_cooldowns[guild_id];
      auto it = user_map.find(user_id);
      if (it != user_map.end()) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - it->second)
                .count();
        if (elapsed < 60)
          return;
      }
      user_map[user_id] = now;
    }
    grant_xp(bot, db, guild_id, user_id, 15, event.msg.channel_id);
  });

  // --- Voice Channel Listener for Voice XP ---
  bot.on_voice_state_update([&ctx](const dpp::voice_state_update_t &event) {
    uint64_t guild_id = event.state.guild_id;
    uint64_t user_id = event.state.user_id;
    if (!module_enabled(ctx.db, guild_id, "levels"))
      return;
    auto now = std::chrono::steady_clock::now();
    long long minutes = 0;

    {
      std::lock_guard<std::mutex> lock(state_mutex);
      auto &timers = voice_state_timers[guild_id];
      if (event.state.channel_id != 0) {
        if (timers.find(user_id) == timers.end())
          timers[user_id] = now;
      } else {
        auto it = timers.find(user_id);
        if (it != timers.end()) {
          minutes = std::chrono::duration_cast<std::chrono::minutes>(
                        now - it->second)
                        .count();
          timers.erase(it);
        }
      }
    }
    if (minutes > 0)
      grant_xp(ctx.bot, ctx.db, guild_id, user_id,
               static_cast<int>(minutes * 10));
  });

  // --- Member join / leave: forwarded to enabled modules ---
  bot.on_guild_member_add([&ctx](const dpp::guild_member_add_t &event) {
    uint64_t guild_id = event.adding_guild.id;
    auto enabled = ctx.db.get_enabled_modules(guild_id);
    for (const auto &m : all_modules()) {
      if (enabled.count(m->id()))
        m->on_member_join(ctx, event);
    }
  });
  bot.on_guild_member_remove([&ctx](const dpp::guild_member_remove_t &event) {
    auto enabled = ctx.db.get_enabled_modules(event.guild_id);
    for (const auto &m : all_modules()) {
      if (enabled.count(m->id()))
        m->on_member_leave(ctx, event);
    }
  });

  // Commands are registered per guild, only for enabled modules.
  bot.on_guild_create([&ctx](const dpp::guild_create_t &event) {
    sync_guild_commands(ctx, event.created.id);
  });

  // --- Slash Commands ---
  bot.on_slashcommand([&bot, &db](const dpp::slashcommand_t &event) {
    std::string command = event.command.get_command_name();

    {
      Ctx ctx{bot, db};
      uint64_t gid = event.command.guild_id;
      auto enabled = db.get_enabled_modules(gid);
      for (const auto &m : all_modules()) {
        if (enabled.count(m->id()) && m->on_slash(ctx, event))
          return;
      }
      auto owner = kCommandModule.find(command);
      if (owner != kCommandModule.end() && !enabled.count(owner->second)) {
        event.reply(ephemeral_text(
            "That feature is turned off. An admin can enable it with /setup."));
        return;
      }
    }

    if (command == "ping") {
      event.reply("Pong! 🏓");
    } else if (command == "setup") {
      if (!has_access(db, event.command, "setup")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      event.reply(setup_step_message(db, event.command.guild_id, "modules"));
    } else if (command == "bot_status") {
      auto now = std::chrono::steady_clock::now();
      auto uptime_sec =
          std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
              .count();
      auto subs = db.get_guild_youtube_subscriptions(event.command.guild_id);

      dpp::embed embed =
          dpp::embed()
              .set_color(0xFF0000)
              .set_title("🤖 PissBot Status")
              .add_field("Uptime",
                         std::to_string(uptime_sec / 60) + "m " +
                             std::to_string(uptime_sec % 60) + "s",
                         true)
              .add_field("Tracked Channels", std::to_string(subs.size()), true)
              .set_footer(dpp::embed_footer().set_text(
                  "System Active • SQLite Normal"));

      std::string access =
          channel_access_report(bot, db, event.command.guild_id);
      if (!access.empty())
        embed.add_field("Channel access", access, false);

      event.reply(dpp::message(event.command.channel_id, embed)
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "youtube_preview") {
      std::string msg_template =
          std::get<std::string>(event.get_parameter("template"));
      msg_template = process_custom_message(msg_template);

      std::string tmpl_err = validate_template(msg_template, {"url", "channel", "role"});
      if (!tmpl_err.empty()) {
        event.reply(ephemeral_text("⚠️ " + tmpl_err));
        return;
      }
      TemplateContext tc;
      tc.vars["url"] = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
      tc.vars["channel"] = "Example Channel";
      tc.vars["role"] = "<@&123456789>";
      std::string rendered = render_template(msg_template, tc);

      event.reply(dpp::message("**Message Preview:**\n" + rendered)
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "youtube") {
      if (!has_access(db, event.command, "youtube")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      event.reply(youtube_list_view(db, event.command.guild_id));
    } else if (command == "xp") {
      int current_xp = db.get_xp(event.command.guild_id, event.command.usr.id);
      event.reply(dpp::message("✨ You currently have **" +
                               std::to_string(current_xp) + " XP**!")
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "inspect_xp") {
      dpp::snowflake target_id =
          std::get<dpp::snowflake>(event.get_parameter("user"));
      int user_xp = db.get_xp(event.command.guild_id, target_id);
      event.reply("<@" + std::to_string(target_id) + "> currently has **" +
                  std::to_string(user_xp) + " XP**.");
    } else if (command == "add_xp") {
      if (!has_access(db, event.command, "xp")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }

      dpp::snowflake target_id =
          std::get<dpp::snowflake>(event.get_parameter("user"));
      int amount =
          static_cast<int>(std::get<int64_t>(event.get_parameter("amount")));

      grant_xp(bot, db, event.command.guild_id, target_id, amount,
               event.command.channel_id);
      event.reply("Added **" + std::to_string(amount) + " XP** to <@" +
                  std::to_string(target_id) + ">!");
    } else if (command == "remove_xp") {
      if (!has_access(db, event.command, "xp")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }

      dpp::snowflake target_id =
          std::get<dpp::snowflake>(event.get_parameter("user"));
      int amount =
          static_cast<int>(std::get<int64_t>(event.get_parameter("amount")));

      if (db.remove_xp(event.command.guild_id, target_id, amount)) {
        event.reply("Removed **" + std::to_string(amount) + " XP** from <@" +
                    std::to_string(target_id) + ">!");
      } else {
        event.reply(
            dpp::message("Failed to remove XP.").set_flags(dpp::m_ephemeral));
      }
    } else if (command == "xp_top") {
      auto top_users = db.get_top_xp(event.command.guild_id, 10);
      if (top_users.empty()) {
        event.reply("No XP recorded for this server yet!");
        return;
      }

      dpp::embed embed =
          dpp::embed().set_color(0xF1C40F).set_title("🏆 XP Leaderboard");

      std::string text = "";
      int rank = 1;
      for (const auto &[user_id, xp_amount] : top_users) {
        text += "**#" + std::to_string(rank++) + "** <@" +
                std::to_string(user_id) + "> — " + std::to_string(xp_amount) +
                " XP\n";
      }

      embed.set_description(text);
      event.reply(dpp::message(event.command.channel_id, embed));
    } else if (command == "answers") {
      if (!has_access(db, event.command, "answers")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      event.reply(answers_list_view(db, event.command.guild_id));
    } else if (command == "permissions") {
      if (!is_admin(event.command)) {
        event.reply(dpp::message("Only administrators can change "
                                 "permissions.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      event.reply(permissions_list_view(db, event.command.guild_id));
    } else if (command == "rank") {
      uint64_t gid = event.command.guild_id;
      uint64_t uid = event.command.usr.id;
      int xp = db.get_xp(gid, uid);
      int level = level_for_xp(xp);
      int next_xp = xp_for_level(level + 1);
      event.reply(dpp::message("**Level " + std::to_string(level) + "** | " +
                               std::to_string(xp) + " / " +
                               std::to_string(next_xp) +
                               " XP to next level | Rank #" +
                               std::to_string(db.get_rank(gid, uid)))
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "levelrole") {
      if (!has_access(db, event.command, "levels")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      int level =
          static_cast<int>(std::get<int64_t>(event.get_parameter("level")));
      dpp::snowflake role_id =
          std::get<dpp::snowflake>(event.get_parameter("role"));
      if (db.add_level_role(event.command.guild_id, level, role_id)) {
        event.reply(dpp::message("Members reaching **level " +
                                 std::to_string(level) + "** will get <@&" +
                                 std::to_string(role_id) +
                                 ">. Make sure my role is above it in the "
                                 "role list.")
                        .set_flags(dpp::m_ephemeral));
      } else {
        event.reply(dpp::message("Failed to save level role.")
                        .set_flags(dpp::m_ephemeral));
      }
    } else if (command == "levelrole_remove") {
      if (!has_access(db, event.command, "levels")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      int level =
          static_cast<int>(std::get<int64_t>(event.get_parameter("level")));
      bool ok = db.remove_level_role(event.command.guild_id, level);
      event.reply(dpp::message(ok ? "Removed the level " +
                                        std::to_string(level) + " reward."
                                  : "No reward set for that level.")
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "levelrole_list") {
      auto roles = db.get_level_roles(event.command.guild_id);
      if (roles.empty()) {
        event.reply(dpp::message("No level rewards set. Use `/levelrole`.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      std::string text = "**Level rewards:**\n";
      for (const auto &lr : roles) {
        text += "Level " + std::to_string(lr.level) + " -> <@&" +
                std::to_string(lr.role_id) + ">\n";
      }
      event.reply(dpp::message(text).set_flags(dpp::m_ephemeral));
    } else if (command == "command_add") {
      if (!has_access(db, event.command, "commands")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      std::string name = std::get<std::string>(event.get_parameter("name"));
      std::string response =
          std::get<std::string>(event.get_parameter("response"));
      if (!name.empty() && name[0] == '!')
        name.erase(0, 1);
      std::transform(name.begin(), name.end(), name.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (!valid_command_name(name)) {
        event.reply(dpp::message("Names can only use letters, numbers, - and "
                                 "_ (max 32 characters).")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      response = process_custom_message(response);
      std::string tmpl_err = validate_template(response);
      if (!tmpl_err.empty()) {
        event.reply(ephemeral_text("⚠️ " + tmpl_err));
        return;
      }
      if (db.set_custom_command(event.command.guild_id, name, response)) {
        event.reply(dpp::message("Saved! Members can now type `!" + name +
                                 "`.")
                        .set_flags(dpp::m_ephemeral));
      } else {
        event.reply(dpp::message("Failed to save command.")
                        .set_flags(dpp::m_ephemeral));
      }
    } else if (command == "command_remove") {
      if (!has_access(db, event.command, "commands")) {
        event.reply(dpp::message("You don't have permission to use this.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      std::string name = std::get<std::string>(event.get_parameter("name"));
      if (!name.empty() && name[0] == '!')
        name.erase(0, 1);
      std::transform(name.begin(), name.end(), name.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      bool ok = db.remove_custom_command(event.command.guild_id, name);
      event.reply(dpp::message(ok ? "Removed `!" + name + "`."
                                  : "No custom command with that name.")
                      .set_flags(dpp::m_ephemeral));
    } else if (command == "command_help") {
      event.reply(ephemeral_text(
          "**Custom command variables**\n" + template_help() +
          "\n\nExample (8-ball):\n`$(user) asked: \"$(query)\" 🎱 "
          "$(choose Yes!|No.|Ask again later.|Most likely.)`\n"
          "Use `\\)` or `\\|` to write a literal ) or |. JavaScript "
          "$(eval) is not supported."));
    } else if (command == "command_list") {
      auto names = db.list_custom_commands(event.command.guild_id);
      if (names.empty()) {
        event.reply(dpp::message("No custom commands yet. Use `/command_add`.")
                        .set_flags(dpp::m_ephemeral));
        return;
      }
      std::string text = "**Custom commands:**\n";
      for (const auto &n : names)
        text += "`!" + n + "`\n";
      event.reply(dpp::message(text).set_flags(dpp::m_ephemeral));
    }
  });

  // --- Channel / Role Select Dropdown Listener ---
  bot.on_select_click([&bot, &db](const dpp::select_click_t &event) {
    Ctx ctx{bot, db};
    {
      auto enabled = db.get_enabled_modules(event.command.guild_id);
      for (const auto &m : all_modules()) {
        if (enabled.count(m->id()) && m->on_select(ctx, event))
          return;
      }
    }
    if (event.custom_id == "setup_modules") {
      uint64_t gid = event.command.guild_id;
      std::vector<std::string> picked;
      for (const auto &v : event.values) {
        if (find_module(v))
          picked.push_back(v);
      }
      db.set_enabled_modules(gid, picked);
      sync_guild_commands(ctx, gid);
      event.reply(dpp::ir_update_message,
                  setup_step_message(db, gid, next_setup_step(db, gid, "modules")));
    } else if (event.custom_id == "select_notification_channel") {
      if (event.values.empty())
        return;
      dpp::snowflake channel_id = std::stoull(event.values[0]);
      db.set_notification_channel(event.command.guild_id, channel_id);
      send_message_logged(bot, dpp::message(
          channel_id,
          "👋 **PissBot will post notifications here.**\n"
          "How to get started:\n"
          "1. `/youtube` opens the YouTube dashboard. Press **Add channel** "
          "and paste a channel ID (starts with `UC`).\n"
          "2. Then pick a ping role, edit the message text, and toggle "
          "Shorts/embeds right in the dashboard.\n"
          "3. `/youtube_preview` lets you test a message template."),
          "the setup guide");
      dpp::message next = setup_step_message(
          db, event.command.guild_id,
          next_setup_step(db, event.command.guild_id, "notify"));
      std::string warn = channel_warning(bot, event.command.guild_id, channel_id);
      if (!warn.empty())
        next.set_content(warn + "\n\n" + next.content);
      event.reply(dpp::ir_update_message, next);
    } else if (event.custom_id == "select_xp_leaderboard_channel") {
      if (event.values.empty())
        return;
      dpp::snowflake channel_id = std::stoull(event.values[0]);
      db.set_xp_leaderboard_channel(event.command.guild_id, channel_id);
      send_message_logged(bot, dpp::message(
          channel_id,
          "🏆 **Live XP leaderboard channel.**\n"
          "The leaderboard below refreshes every 5 minutes.\n"
          "• Chatting earns 15 XP (once per minute)\n"
          "• Time in voice earns 10 XP per minute\n"
          "• `/rank` shows your level and rank, `/xp_top` shows the top 10\n"
          "• Admins can reward levels with `/levelrole`"),
          "the leaderboard guide");
      dpp::message next = setup_step_message(
          db, event.command.guild_id,
          next_setup_step(db, event.command.guild_id, "leaderboard"));
      std::string warn = channel_warning(bot, event.command.guild_id, channel_id);
      if (!warn.empty())
        next.set_content(warn + "\n\n" + next.content);
      event.reply(dpp::ir_update_message, next);
    } else if (event.custom_id == "ans_pick") {
      if (event.values.empty())
        return;
      event.reply(dpp::ir_update_message,
                  answers_detail_view(db, event.command.guild_id,
                                      event.values[0]));
    } else if (event.custom_id == "perm_pick") {
      if (event.values.empty())
        return;
      event.reply(dpp::ir_update_message,
                  permissions_detail_view(db, event.command.guild_id,
                                          event.values[0]));
    } else if (event.custom_id.rfind("perm_roles:", 0) == 0) {
      std::string key = event.custom_id.substr(11);
      std::vector<uint64_t> roles;
      for (const auto &v : event.values)
        roles.push_back(std::stoull(v));
      bool ok = find_feature(key) &&
                db.set_permission_roles(event.command.guild_id, key, roles);
      event.reply(dpp::ir_update_message,
                  permissions_detail_view(
                      db, event.command.guild_id, key,
                      ok ? "✅ Permissions updated." : "⚠️ Could not save."));
    } else if (event.custom_id == "yt_pick") {
      if (event.values.empty())
        return;
      event.reply(dpp::ir_update_message,
                  youtube_detail_view(db, event.command.guild_id,
                                      event.values[0]));
    } else if (event.custom_id.rfind("yt_role:", 0) == 0) {
      std::string yt_id = event.custom_id.substr(8);
      YouTubeSub sub;
      if (!event.values.empty() &&
          find_sub(db, event.command.guild_id, yt_id, sub)) {
        sub.ping_role_id = event.values[0];
        db.add_youtube_channel(sub);
      }
      event.reply(dpp::ir_update_message,
                  youtube_detail_view(db, event.command.guild_id, yt_id,
                                      "✅ Ping role updated."));
    }
  });

  // --- Button Click Listener ---
  bot.on_button_click([&bot, &db](const dpp::button_click_t &event) {
    const std::string &id = event.custom_id;
    uint64_t guild_id = event.command.guild_id;

    {
      Ctx ctx{bot, db};
      auto enabled = db.get_enabled_modules(guild_id);
      for (const auto &m : all_modules()) {
        if (enabled.count(m->id()) && m->on_button(ctx, event))
          return;
      }
    }

    if (id.rfind("setup_skip:", 0) == 0) {
      std::string step = id.substr(11);
      event.reply(dpp::ir_update_message,
                  setup_step_message(db, guild_id,
                                     next_setup_step(db, guild_id, step)));
    } else if (id == "ans_back") {
      event.reply(dpp::ir_update_message, answers_list_view(db, guild_id));
    } else if (id == "ans_add") {
      dpp::interaction_modal_response modal("ans_modal_add", "Add Answer");
      dpp::component trig;
      trig.set_label("When someone sends this...")
          .set_id("ans_input_trigger")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_short)
          .set_placeholder("hi")
          .set_required(true);
      dpp::component resp;
      resp.set_label("...the bot replies with")
          .set_id("ans_input_response")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_placeholder("Hey $(user), welcome!")
          .set_required(true);
      modal.add_component(trig);
      modal.add_component(resp);
      event.dialog(modal);
    } else if (id.rfind("ans_edit:", 0) == 0) {
      std::string trigger = id.substr(9);
      Answer a;
      if (!find_answer(db, guild_id, trigger, a)) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "That answer no longer exists."));
        return;
      }
      dpp::interaction_modal_response modal("ans_modal_edit:" + trigger,
                                            "Edit Reply");
      dpp::component resp;
      resp.set_label("The bot replies with")
          .set_id("ans_input_response")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_default_value(a.response)
          .set_required(true);
      modal.add_component(resp);
      event.dialog(modal);
    } else if (id.rfind("ans_mode:", 0) == 0 ||
               id.rfind("ans_remove:", 0) == 0) {
      std::string trigger = id.substr(id.find(':') + 1);
      Answer a;
      if (!find_answer(db, guild_id, trigger, a)) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "That answer no longer exists."));
        return;
      }
      if (id.rfind("ans_remove:", 0) == 0) {
        db.remove_answer(guild_id, trigger);
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "🗑️ Removed the answer for `" +
                                          clip(trigger, 60) + "`."));
        return;
      }
      db.set_answer(guild_id, trigger, a.response, !a.contains);
      event.reply(dpp::ir_update_message,
                  answers_detail_view(
                      db, guild_id, trigger,
                      !a.contains ? "✅ Now matches anywhere in a message."
                                  : "✅ Now matches the whole message only."));
    } else if (id == "perm_back") {
      event.reply(dpp::ir_update_message, permissions_list_view(db, guild_id));
    } else if (id.rfind("perm_clear:", 0) == 0) {
      std::string key = id.substr(11);
      bool ok = find_feature(key) && db.set_permission_roles(guild_id, key, {});
      event.reply(dpp::ir_update_message,
                  permissions_detail_view(
                      db, guild_id, key,
                      ok ? "✅ Now administrators only." : "⚠️ Could not save."));
    } else if (id == "yt_add") {
      dpp::interaction_modal_response modal("yt_modal_add",
                                            "Add YouTube Channel");
      dpp::component input;
      input.set_label("Channel ID or channel URL")
          .set_id("yt_input_channel")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_short)
          .set_placeholder("UCxxxxxxxxxxxxxxxxxxxxxx")
          .set_required(true);
      modal.add_component(input);
      event.dialog(modal);
    } else if (id == "yt_back") {
      event.reply(dpp::ir_update_message, youtube_list_view(db, guild_id));
    } else if (id.rfind("yt_msgs:", 0) == 0) {
      std::string yt_id = id.substr(8);
      YouTubeSub sub;
      if (!find_sub(db, guild_id, yt_id, sub)) {
        event.reply(dpp::ir_update_message,
                    youtube_list_view(db, guild_id,
                                      "That channel is no longer tracked."));
        return;
      }

      dpp::interaction_modal_response modal("yt_modal_msg:" + yt_id,
                                            "Customize Messages");
      dpp::component input_vid;
      input_vid.set_label("Video message template")
          .set_id("input_video_msg")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_default_value(sub.video_message)
          .set_required(true);
      dpp::component input_stream;
      input_stream.set_label("Live stream message template")
          .set_id("input_stream_msg")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_default_value(sub.stream_message)
          .set_required(true);

      // D++ wraps each input in a Label itself; add inputs directly.
      modal.add_component(input_vid);
      modal.add_component(input_stream);
      event.dialog(modal);
    } else if (id.rfind("yt_shorts:", 0) == 0 ||
               id.rfind("yt_embed:", 0) == 0 ||
               id.rfind("yt_clearrole:", 0) == 0 ||
               id.rfind("yt_remove:", 0) == 0) {
      std::string yt_id = id.substr(id.find(':') + 1);
      YouTubeSub sub;
      if (!find_sub(db, guild_id, yt_id, sub)) {
        event.reply(dpp::ir_update_message,
                    youtube_list_view(db, guild_id,
                                      "That channel is no longer tracked."));
        return;
      }

      std::string notice;
      if (id.rfind("yt_shorts:", 0) == 0) {
        sub.ignore_shorts = !sub.ignore_shorts;
        db.add_youtube_channel(sub);
        notice = sub.ignore_shorts ? "✅ Shorts will now be skipped."
                                   : "✅ Shorts will now be posted.";
      } else if (id.rfind("yt_embed:", 0) == 0) {
        sub.use_embed = !sub.use_embed;
        db.add_youtube_channel(sub);
        notice = sub.use_embed ? "✅ Rich embeds enabled."
                               : "✅ Switched to plain text.";
      } else if (id.rfind("yt_clearrole:", 0) == 0) {
        sub.ping_role_id = "";
        db.add_youtube_channel(sub);
        notice = "✅ Ping role cleared.";
      } else {
        db.remove_youtube_channel(guild_id, yt_id);
        event.reply(dpp::ir_update_message,
                    youtube_list_view(db, guild_id,
                                      "🗑️ Removed `" + yt_id + "`."));
        return;
      }
      event.reply(dpp::ir_update_message,
                  youtube_detail_view(db, guild_id, yt_id, notice));
    }
  });

  // --- Modal Submission Listener ---
  bot.on_form_submit([&bot, &db](const dpp::form_submit_t &event) {
    uint64_t guild_id = event.command.guild_id;

    // Submitted inputs arrive as a flat list; read them by custom_id.
    auto field = [&event](const std::string &id) -> std::string {
      for (const auto &c : event.components) {
        if (c.custom_id == id && std::holds_alternative<std::string>(c.value))
          return std::get<std::string>(c.value);
      }
      return "";
    };

    {
      Ctx ctx{bot, db};
      auto enabled = db.get_enabled_modules(guild_id);
      for (const auto &m : all_modules()) {
        if (enabled.count(m->id()) && m->on_form(ctx, event))
          return;
      }
    }

    if (event.custom_id == "ans_modal_add") {
      std::string trigger = normalize_text(field("ans_input_trigger"));
      std::string response = process_custom_message(field("ans_input_response"));
      if (strip_punct(trigger).empty() || dpp::utility::utf8len(trigger) > 50 ||
          response.empty()) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(
                        db, guild_id,
                        "⚠️ The trigger needs 1 to 50 characters (not just "
                        "punctuation) and the reply can't be empty."));
        return;
      }
      std::string tmpl_err = validate_template(response);
      if (!tmpl_err.empty()) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id, "⚠️ " + tmpl_err));
        return;
      }
      Answer existing;
      bool exists = find_answer(db, guild_id, trigger, existing);
      if (!exists && db.get_answers(guild_id).size() >= kMaxAnswers) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "⚠️ You've used all " +
                                          std::to_string(kMaxAnswers) +
                                          " answer slots. Remove one first."));
        return;
      }
      bool contains = exists ? existing.contains : false;
      if (!db.set_answer(guild_id, trigger, response, contains)) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "⚠️ Failed to save that answer."));
        return;
      }
      event.reply(dpp::ir_update_message,
                  answers_detail_view(
                      db, guild_id, trigger,
                      exists ? "✅ Answer updated."
                             : "✅ Answer added. It replies when a message is "
                               "exactly this. Switch to anywhere below if you "
                               "want it to react inside longer messages."));
    } else if (event.custom_id.rfind("ans_modal_edit:", 0) == 0) {
      std::string trigger = event.custom_id.substr(15);
      std::string response = process_custom_message(field("ans_input_response"));
      Answer a;
      if (response.empty() || !find_answer(db, guild_id, trigger, a)) {
        event.reply(dpp::ir_update_message,
                    answers_list_view(db, guild_id,
                                      "⚠️ Could not update that answer."));
        return;
      }
      std::string tmpl_err = validate_template(response);
      if (!tmpl_err.empty()) {
        event.reply(dpp::ir_update_message,
                    answers_detail_view(db, guild_id, trigger, "⚠️ " + tmpl_err));
        return;
      }
      db.set_answer(guild_id, trigger, response, a.contains);
      event.reply(dpp::ir_update_message,
                  answers_detail_view(db, guild_id, trigger,
                                      "✅ Reply updated."));
    } else if (event.custom_id == "yt_modal_add") {
      std::string yt_id = extract_channel_id(field("yt_input_channel"));
      if (yt_id.empty()) {
        event.reply(dpp::ir_update_message,
                    youtube_list_view(
                        db, guild_id,
                        "⚠️ That doesn't look like a channel ID. It starts "
                        "with `UC` and is 24 characters long (you can also "
                        "paste a `youtube.com/channel/...` link)."));
        return;
      }

      YouTubeSub existing;
      std::string notice = "✅ Channel added. Set a ping role and adjust the "
                           "options below.";
      if (find_sub(db, guild_id, yt_id, existing)) {
        notice = "That channel was already tracked.";
      } else {
        YouTubeSub sub;
        sub.guild_id = guild_id;
        sub.yt_channel_id = yt_id;
        sub.video_message = "🔴 $(role) NEW VIDEO GO WATCH RN!!\n$(url)";
        sub.stream_message = "🔴 $(role) $(channel) IS LIVE ON YOUTUBE!!\n$(url)";
        sub.ping_role_id = "";
        sub.ignore_shorts = false;
        sub.use_embed = false;
        if (!db.add_youtube_channel(sub)) {
          event.reply(dpp::ir_update_message,
                      youtube_list_view(db, guild_id,
                                        "⚠️ Failed to save that channel."));
          return;
        }
      }
      event.reply(dpp::ir_update_message,
                  youtube_detail_view(db, guild_id, yt_id, notice));
    } else if (event.custom_id.rfind("yt_modal_msg:", 0) == 0) {
      std::string yt_id = event.custom_id.substr(13);
      std::string vid_msg = field("input_video_msg");
      std::string stream_msg = field("input_stream_msg");

      YouTubeSub sub;
      if (!find_sub(db, guild_id, yt_id, sub)) {
        event.reply(dpp::ir_update_message,
                    youtube_list_view(db, guild_id,
                                      "That channel is no longer tracked."));
        return;
      }
      std::string tmpl_err =
          validate_template(vid_msg, {"url", "channel", "role"});
      if (tmpl_err.empty())
        tmpl_err = validate_template(stream_msg, {"url", "channel", "role"});
      if (!tmpl_err.empty()) {
        event.reply(dpp::ir_update_message,
                    youtube_detail_view(db, guild_id, yt_id,
                                        "⚠️ " + tmpl_err + " Nothing was saved."));
        return;
      }
      if (!vid_msg.empty())
        sub.video_message = process_custom_message(vid_msg);
      if (!stream_msg.empty())
        sub.stream_message = process_custom_message(stream_msg);
      db.add_youtube_channel(sub);
      event.reply(dpp::ir_update_message,
                  youtube_detail_view(db, guild_id, yt_id,
                                      "✅ Messages saved."));
    }
  });

  // --- Command Registration & Timers ---
  bot.on_ready([&bot, &db](const dpp::ready_t &event) {
    // Commands are registered per guild for enabled modules only (see
    // on_guild_create). Remove the old global command set once.
    bot.global_bulk_command_create({});

    // 5-minute background RSS timer
    bot.start_timer(
        [&bot, &db](dpp::timer handle) {
          auto subscriptions = db.get_all_youtube_subscriptions();

          for (const auto &sub : subscriptions) {
            uint64_t notify_channel_id =
                db.get_notification_channel(sub.guild_id);
            if (notify_channel_id == 0 ||
                !module_enabled(db, sub.guild_id, "social"))
              continue;

            std::string feed_url =
                "https://www.youtube.com/feeds/videos.xml?channel_id=" +
                sub.yt_channel_id;

            bot.request(
                feed_url, dpp::m_get,
                [&bot, &db, notify_channel_id,
                 sub](const dpp::http_request_completion_t &response) {
                  if (response.status == 429 || response.status != 200)
                    return;

                  const std::string &xml = response.body;
                  size_t entry_pos = xml.find("<entry>");
                  if (entry_pos == std::string::npos)
                    return;
                  std::string entry_xml = xml.substr(entry_pos);

                  auto get_entry_tag =
                      [&entry_xml](const std::string &tag) -> std::string {
                    std::string start_tag = "<" + tag + ">";
                    std::string end_tag = "</" + tag + ">";
                    size_t start = entry_xml.find(start_tag);
                    if (start == std::string::npos)
                      return "";
                    start += start_tag.length();
                    size_t end = entry_xml.find(end_tag, start);
                    if (end == std::string::npos)
                      return "";
                    return entry_xml.substr(start, end - start);
                  };

                  std::string latest_video_id = get_entry_tag("yt:videoId");
                  if (latest_video_id.empty())
                    return;

                  std::string cached_video_id =
                      db.get_last_posted_video(sub.yt_channel_id);

                  if (latest_video_id != cached_video_id &&
                      pending_try_add(latest_video_id)) {

                    db.set_last_posted_video(sub.yt_channel_id,
                                             latest_video_id);

                    std::string published_str = get_entry_tag("published");
                    if (!published_str.empty() &&
                        !is_recent_upload(published_str, 30)) {
                      pending_remove(latest_video_id);
                      return;
                    }

                    std::string channel_name = "YouTube Channel";
                    size_t name_start = xml.find("<name>");
                    if (name_start != std::string::npos &&
                        name_start < entry_pos) {
                      name_start += 6;
                      size_t name_end = xml.find("</name>", name_start);
                      if (name_end != std::string::npos) {
                        channel_name =
                            xml.substr(name_start, name_end - name_start);
                      }
                    }

                    std::string watch_url =
                        "https://www.youtube.com/watch?v=" + latest_video_id;
                    std::string video_title =
                        xml_unescape(get_entry_tag("title"));

                    bot.request(
                        watch_url, dpp::m_get,
                        [&bot, notify_channel_id, sub, latest_video_id,
                         watch_url, channel_name, video_title](
                            const dpp::http_request_completion_t &page_resp) {
                          pending_remove(latest_video_id);
                          if (page_resp.status != 200)
                            return;

                          const std::string &html = page_resp.body;

                          if (sub.ignore_shorts &&
                              (html.find("\"isShort\":true") !=
                                   std::string::npos ||
                               html.find("/shorts/") != std::string::npos)) {
                            return;
                          }

                          bool is_currently_live =
                              (html.find("\"isLive\":true") !=
                                   std::string::npos ||
                               html.find("\"isLiveNow\":true") !=
                                   std::string::npos);
                          bool is_ended_stream =
                              (html.find("\"isLiveContent\":true") !=
                                   std::string::npos &&
                               !is_currently_live);

                          if (is_ended_stream)
                            return;

                          std::string selected_template =
                              is_currently_live ? sub.stream_message
                                                : sub.video_message;
                          std::string role_tag =
                              sub.ping_role_id.empty()
                                  ? "@everyone"
                                  : "<@&" + sub.ping_role_id + ">";

                          TemplateContext tc;
                          tc.vars["url"] = watch_url;
                          tc.vars["channel"] = channel_name;
                          tc.vars["role"] = role_tag;
                          std::string final_msg =
                              render_template(selected_template, tc);

                          if (sub.use_embed) {
                            std::string label = is_currently_live
                                                    ? "🔴 Live Stream Started!"
                                                    : "🎥 New Video Uploaded!";
                            dpp::embed_author author;
                            author.name = label;
                            dpp::embed embed =
                                dpp::embed()
                                    .set_color(0xFF0000)
                                    .set_author(author)
                                    .set_title(video_title.empty()
                                                   ? label
                                                   : clip(video_title, 256))
                                    .set_url(watch_url)
                                    .set_description("[" + channel_name +
                                                     " - Watch Here](" +
                                                     watch_url + ")")
                                    .set_image("https://img.youtube.com/vi/" +
                                               latest_video_id +
                                               "/maxresdefault.jpg");

                            dpp::message msg(notify_channel_id, final_msg);
                            msg.add_embed(embed);
                            send_message_logged(bot, msg,
                                                "a YouTube notification");
                          } else {
                            send_message_logged(
                                bot, dpp::message(notify_channel_id, final_msg),
                                "a YouTube notification");
                          }
                        });
                  }
                });
          }

          // Auto-update XP Leaderboard in designated channels
          for (uint64_t lb_guild_id : db.get_leaderboard_guilds()) {
            uint64_t lb_channel_id = db.get_xp_leaderboard_channel(lb_guild_id);
            if (lb_channel_id == 0 ||
                !module_enabled(db, lb_guild_id, "levels"))
              continue;

            auto top_users = db.get_top_xp(lb_guild_id, 10);

            dpp::embed embed = dpp::embed()
                                   .set_color(0xF1C40F)
                                   .set_title("🏆 Live Server XP Leaderboard")
                                   .set_footer(dpp::embed_footer().set_text(
                                       "Auto-updates every 5 minutes"));

            std::string text = "";
            int rank = 1;
            for (const auto &[user_id, xp] : top_users) {
              text += "**#" + std::to_string(rank++) + "** <@" +
                      std::to_string(user_id) + "> — **" + std::to_string(xp) +
                      " XP**\n";
            }
            embed.set_description(text.empty() ? "No XP earned yet!" : text);

            uint64_t last_msg_id =
                db.get_last_leaderboard_message_id(lb_guild_id);

            if (last_msg_id != 0) {
              dpp::message msg(lb_channel_id, embed);
              msg.id = last_msg_id;

              bot.message_edit(
                  msg, [&bot, &db, lb_guild_id, lb_channel_id,
                        embed](const dpp::confirmation_callback_t &cb) {
                    if (cb.is_error()) {
                      int code = cb.get_error().code;
                      if (code == 50013 || code == 50001) {
                        // A permissions problem: creating a new message
                        // would fail the same way, so just explain it.
                        log_send_failure(bot, lb_channel_id,
                                         "the XP leaderboard", cb);
                        return;
                      }
                      bot.message_create(
                          dpp::message(lb_channel_id, embed),
                          [&bot, &db, lb_guild_id,
                           lb_channel_id](const dpp::confirmation_callback_t &new_cb) {
                            if (new_cb.is_error()) {
                              log_send_failure(bot, lb_channel_id,
                                               "the XP leaderboard", new_cb);
                              return;
                            }
                            auto created_msg =
                                std::get<dpp::message>(new_cb.value);
                            db.set_last_leaderboard_message_id(lb_guild_id,
                                                               created_msg.id);
                          });
                    }
                  });
            } else {
              bot.message_create(
                  dpp::message(lb_channel_id, embed),
                  [&bot, &db, lb_guild_id,
                   lb_channel_id](const dpp::confirmation_callback_t &cb) {
                    if (cb.is_error()) {
                      log_send_failure(bot, lb_channel_id, "the XP leaderboard",
                                       cb);
                      return;
                    }
                    auto created_msg = std::get<dpp::message>(cb.value);
                    db.set_last_leaderboard_message_id(lb_guild_id,
                                                       created_msg.id);
                  });
            }
          }
        },
        300);
  });

  bot.start(dpp::st_wait);
  return 0;
}
// Command definitions for the original features. Their handlers still live in
// main.cpp and are gated there by module (see command_module in main.cpp).
#include "module.h"

std::vector<dpp::slashcommand> core_commands(dpp::snowflake app_id) {
  std::vector<dpp::slashcommand> v;
  v.emplace_back("ping", "Check bot responsiveness", app_id);
  v.emplace_back("setup", "Choose modules and set channels", app_id);
  v.emplace_back("bot_status", "Show bot health and stats", app_id);
  dpp::slashcommand perm("permissions",
                         "Choose which roles can use bot features (Admin)",
                         app_id);
  perm.set_default_permissions(dpp::p_administrator);
  v.push_back(perm);
  return v;
}

namespace {

class LevelsModule : public Module {
public:
  std::string id() const override { return "levels"; }
  std::string name() const override { return "Levels and XP"; }
  std::string description() const override {
    return "XP for chatting and voice, ranks, leaderboard, role rewards";
  }
  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    v.emplace_back("xp", "Check your current XP balance", a);
    v.emplace_back("rank", "Show your level, XP and rank", a);
    v.emplace_back("xp_top", "Show top 10 highest XP users", a);

    dpp::slashcommand inspect("inspect_xp", "Check another user's XP", a);
    inspect.add_option(
        dpp::command_option(dpp::co_user, "user", "Target user", true));
    v.push_back(inspect);

    dpp::slashcommand add("add_xp", "Add XP to a user (Admin)", a);
    add.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
    add.add_option(
        dpp::command_option(dpp::co_integer, "amount", "Amount of XP to add", true));
    v.push_back(add);

    dpp::slashcommand rem("remove_xp", "Remove XP from a user (Admin)", a);
    rem.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
    rem.add_option(dpp::command_option(dpp::co_integer, "amount",
                                       "Amount of XP to remove", true));
    v.push_back(rem);

    dpp::slashcommand lr("levelrole",
                         "Give a role when members reach a level (Admin)", a);
    lr.add_option(
        dpp::command_option(dpp::co_integer, "level", "Level required", true)
            .set_min_value(1));
    lr.add_option(dpp::command_option(dpp::co_role, "role", "Role to give", true));
    v.push_back(lr);

    dpp::slashcommand lrr("levelrole_remove", "Remove a level reward (Admin)", a);
    lrr.add_option(
        dpp::command_option(dpp::co_integer, "level", "Level to clear", true)
            .set_min_value(1));
    v.push_back(lrr);

    v.emplace_back("levelrole_list", "List level rewards", a);
    return v;
  }
};

class SocialModule : public Module {
public:
  std::string id() const override { return "social"; }
  std::string name() const override { return "Social media posts"; }
  std::string description() const override {
    return "Post notifications for new YouTube videos and live streams";
  }
  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    v.emplace_back("youtube", "Manage YouTube notifications (Admin)", a);
    dpp::slashcommand prev("youtube_preview", "Test a notification message", a);
    prev.add_option(dpp::command_option(dpp::co_string, "template",
                                        "Message template to test", true));
    v.push_back(prev);
    return v;
  }
};

class CommandsModule : public Module {
public:
  std::string id() const override { return "commands"; }
  std::string name() const override { return "Custom commands"; }
  std::string description() const override {
    return "Your own !commands with variables like $(user) and $(choose a|b)";
  }
  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    dpp::slashcommand add("command_add",
                          "Create or update a custom !command (Admin)", a);
    add.add_option(dpp::command_option(dpp::co_string, "name",
                                       "Command name, used as !name", true));
    add.add_option(dpp::command_option(
        dpp::co_string, "response",
        "Reply. Try $(user) $(query) $(choose a|b|c) $(rand 1 100)", true));
    v.push_back(add);

    dpp::slashcommand rem("command_remove", "Delete a custom command (Admin)", a);
    rem.add_option(dpp::command_option(dpp::co_string, "name", "Command name", true));
    v.push_back(rem);

    v.emplace_back("command_list", "List custom commands", a);
    v.emplace_back("command_help", "List the variables custom commands can use", a);
    return v;
  }
};

class AnswersModule : public Module {
public:
  std::string id() const override { return "answers"; }
  std::string name() const override { return "Answers"; }
  std::string description() const override {
    return "Auto-replies to plain messages like \"hi\"";
  }
  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    v.emplace_back("answers", "Make the bot reply to plain messages (Admin)", a);
    return v;
  }
};

} // namespace

std::unique_ptr<Module> make_levels_module() {
  return std::make_unique<LevelsModule>();
}
std::unique_ptr<Module> make_social_module() {
  return std::make_unique<SocialModule>();
}
std::unique_ptr<Module> make_commands_module() {
  return std::make_unique<CommandsModule>();
}
std::unique_ptr<Module> make_answers_module() {
  return std::make_unique<AnswersModule>();
}

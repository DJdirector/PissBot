#include "module.h"
#include <algorithm>

namespace {

const char *kFeature = "roles";

class RolesModule : public Module {
public:
  std::string id() const override { return "roles"; }
  std::string name() const override { return "Role menus"; }
  std::string description() const override {
    return "Self-assign roles by clicking buttons on a message";
  }

  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    dpp::slashcommand rm("rolemenu",
                         "Post a message where members click buttons for roles", a);
    rm.add_option(dpp::command_option(dpp::co_string, "title",
                                      "Text shown above the buttons", true));
    rm.add_option(dpp::command_option(dpp::co_role, "role1", "First role", true));
    for (int i = 2; i <= 5; ++i) {
      rm.add_option(dpp::command_option(dpp::co_role, "role" + std::to_string(i),
                                        "Another role", false));
    }
    v.push_back(rm);
    return v;
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    if (event.command.get_command_name() != "rolemenu")
      return false;
    if (!has_access(ctx.db, event.command, kFeature)) {
      event.reply(no_permission_message());
      return true;
    }

    std::string title = std::get<std::string>(event.get_parameter("title"));
    if (title.size() > 300)
      title.resize(300);

    dpp::component row;
    int count = 0;
    for (int i = 1; i <= 5; ++i) {
      auto p = event.get_parameter("role" + std::to_string(i));
      if (!std::holds_alternative<dpp::snowflake>(p))
        continue;
      dpp::snowflake role_id = std::get<dpp::snowflake>(p);
      dpp::role r = event.command.get_resolved_role(role_id);
      if (r.permissions.has(dpp::p_administrator)) {
        event.reply(ephemeral_text(
            "Roles with Administrator can't be put in a role menu."));
        return true;
      }
      std::string label = r.name.empty() ? "Role" : r.name;
      if (label.size() > 80)
        label.resize(80);
      dpp::component b;
      b.set_type(dpp::cot_button)
          .set_label(label)
          .set_style(dpp::cos_primary)
          .set_id("rr:" + std::to_string(uint64_t(role_id)));
      row.add_component(b);
      count++;
    }
    if (count == 0) {
      event.reply(ephemeral_text("Pick at least one role."));
      return true;
    }

    dpp::message msg("**" + title + "**\nClick a button to get or remove that role.");
    msg.add_component(row);
    msg.set_allowed_mentions(false, false, false, false);
    event.reply(msg);
    return true;
  }

  bool on_button(Ctx &ctx, const dpp::button_click_t &event) override {
    if (event.custom_id.rfind("rr:", 0) != 0)
      return false;
    uint64_t role = 0;
    try {
      role = std::stoull(event.custom_id.substr(3));
    } catch (...) {
      return true;
    }
    uint64_t guild = event.command.guild_id;
    uint64_t user = event.command.usr.id;

    const auto &mine = event.command.member.get_roles();
    bool has = std::find(mine.begin(), mine.end(), dpp::snowflake(role)) != mine.end();

    event.thinking(true);
    auto done = [event, role, has](const dpp::confirmation_callback_t &cb) {
      if (cb.is_error()) {
        event.edit_original_response(dpp::message(
            "I couldn't change that role. My role has to be above it in the "
            "role list."));
        return;
      }
      event.edit_original_response(dpp::message(
          std::string(has ? "Removed <@&" : "Added <@&") + std::to_string(role) + ">."));
    };
    if (has)
      ctx.bot.guild_member_remove_role(guild, user, role, done);
    else
      ctx.bot.guild_member_add_role(guild, user, role, done);
    return true;
  }
};

} // namespace

std::unique_ptr<Module> make_roles_module() {
  return std::make_unique<RolesModule>();
}

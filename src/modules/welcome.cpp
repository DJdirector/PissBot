#include "module.h"
#include "template.h"
#include <variant>

namespace {

const char *kFeature = "welcome";
const char *kDefaultWelcome = "Welcome to $(server), $(user)! You are member #$(members).";

uint64_t kv_id(Database &db, uint64_t guild, const std::string &key) {
  std::string v = db.kv_get(guild, key);
  if (v.empty())
    return 0;
  try {
    return std::stoull(v);
  } catch (...) {
    return 0;
  }
}

std::string code_block(const std::string &s) {
  std::string t = replace_all(s, "```", "` ` `");
  if (t.size() > 300)
    t.resize(300);
  return "```\n" + t + "\n```";
}

dpp::message welcome_view(Database &db, uint64_t guild,
                          const std::string &notice = "") {
  dpp::message msg;
  msg.set_flags(dpp::m_ephemeral);
  uint64_t channel = kv_id(db, guild, "welcome_channel");
  uint64_t role = kv_id(db, guild, "welcome_autorole");
  std::string welcome = db.kv_get(guild, "welcome_msg");
  std::string goodbye = db.kv_get(guild, "goodbye_msg");

  std::string text;
  if (!notice.empty())
    text += notice + "\n\n";
  text += "**Welcome and autorole**\n";
  text += channel ? "Channel: <#" + std::to_string(channel) + ">\n"
                  : "Channel: not set (welcome and goodbye messages are off)\n";
  text += role ? "Autorole: <@&" + std::to_string(role) + ">\n"
               : "Autorole: none\n";
  text += "\nWelcome message:\n" +
          code_block(welcome.empty() ? kDefaultWelcome : welcome);
  text += "Goodbye message:\n" +
          (goodbye.empty() ? std::string("off\n") : code_block(goodbye));
  text += "Extra variable here: `$(members)`. Others: `$(user)` `$(username)` "
          "`$(server)` `$(choose a|b)`";
  msg.set_content(text);

  dpp::component ch;
  ch.set_type(dpp::cot_channel_selectmenu);
  ch.set_id("wel_channel");
  ch.set_placeholder("Channel for welcome and goodbye messages...");
  ch.add_channel_type(dpp::CHANNEL_TEXT);
  msg.add_component(dpp::component().add_component(ch));

  dpp::component rl;
  rl.set_type(dpp::cot_role_selectmenu);
  rl.set_id("wel_autorole");
  rl.set_placeholder("Role to give every new member...");
  msg.add_component(dpp::component().add_component(rl));

  dpp::component edit, off, clear;
  edit.set_type(dpp::cot_button)
      .set_label("Edit messages")
      .set_style(dpp::cos_primary)
      .set_id("wel_edit");
  off.set_type(dpp::cot_button)
      .set_label("Turn messages off")
      .set_style(dpp::cos_secondary)
      .set_id("wel_off")
      .set_disabled(channel == 0);
  clear.set_type(dpp::cot_button)
      .set_label("Clear autorole")
      .set_style(dpp::cos_secondary)
      .set_id("wel_clearrole")
      .set_disabled(role == 0);
  msg.add_component(
      dpp::component().add_component(edit).add_component(off).add_component(clear));
  return msg;
}

TemplateContext member_context(const std::string &user_id,
                               const std::string &user_name,
                               const dpp::guild &g) {
  TemplateContext ctx;
  ctx.user_id = user_id;
  ctx.user_name = user_name;
  ctx.server_name = g.name;
  ctx.vars["members"] = std::to_string(g.member_count);
  return ctx;
}

void send_to_channel(Ctx &ctx, uint64_t channel, const std::string &text) {
  if (channel == 0 || text.empty())
    return;
  dpp::message m(channel, text);
  m.set_allowed_mentions(true, false, false, false);
  ctx.bot.message_create(m);
}

class WelcomeModule : public Module {
public:
  std::string id() const override { return "welcome"; }
  std::string name() const override { return "Welcome and autorole"; }
  std::string description() const override {
    return "Greet new members, say goodbye, give a role on join";
  }
  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    std::vector<dpp::slashcommand> v;
    v.emplace_back("welcome", "Set up welcome messages and autorole (Admin)", a);
    return v;
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    if (event.command.get_command_name() != "welcome")
      return false;
    if (!has_access(ctx.db, event.command, kFeature)) {
      event.reply(no_permission_message());
      return true;
    }
    event.reply(welcome_view(ctx.db, event.command.guild_id));
    return true;
  }

  bool on_select(Ctx &ctx, const dpp::select_click_t &event) override {
    uint64_t guild = event.command.guild_id;
    if (event.custom_id == "wel_channel" && !event.values.empty()) {
      ctx.db.kv_set(guild, "welcome_channel", event.values[0]);
      event.reply(dpp::ir_update_message,
                  welcome_view(ctx.db, guild, "✅ Channel saved."));
      return true;
    }
    if (event.custom_id == "wel_autorole" && !event.values.empty()) {
      ctx.db.kv_set(guild, "welcome_autorole", event.values[0]);
      event.reply(dpp::ir_update_message,
                  welcome_view(ctx.db, guild,
                               "✅ Autorole saved. My role must sit above it "
                               "in the role list."));
      return true;
    }
    return false;
  }

  bool on_button(Ctx &ctx, const dpp::button_click_t &event) override {
    uint64_t guild = event.command.guild_id;
    const std::string &id = event.custom_id;
    if (id == "wel_off") {
      ctx.db.kv_set(guild, "welcome_channel", "");
      event.reply(dpp::ir_update_message,
                  welcome_view(ctx.db, guild, "✅ Welcome and goodbye messages are off."));
    } else if (id == "wel_clearrole") {
      ctx.db.kv_set(guild, "welcome_autorole", "");
      event.reply(dpp::ir_update_message,
                  welcome_view(ctx.db, guild, "✅ Autorole cleared."));
    } else if (id == "wel_edit") {
      dpp::interaction_modal_response modal("wel_modal", "Welcome Messages");
      std::string cur = ctx.db.kv_get(guild, "welcome_msg");
      dpp::component w;
      w.set_label("Welcome message")
          .set_id("wel_in_welcome")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_default_value(cur.empty() ? kDefaultWelcome : cur)
          .set_required(true);
      dpp::component g;
      g.set_label("Goodbye message (empty = off)")
          .set_id("wel_in_goodbye")
          .set_type(dpp::cot_text)
          .set_text_style(dpp::text_paragraph)
          .set_default_value(ctx.db.kv_get(guild, "goodbye_msg"))
          .set_required(false);
      modal.add_component(w);
      modal.add_component(g);
      event.dialog(modal);
    } else {
      return false;
    }
    return true;
  }

  bool on_form(Ctx &ctx, const dpp::form_submit_t &event) override {
    if (event.custom_id != "wel_modal")
      return false;
    uint64_t guild = event.command.guild_id;
    std::string welcome = process_text(form_field(event, "wel_in_welcome"));
    std::string goodbye = process_text(form_field(event, "wel_in_goodbye"));
    std::string err = validate_template(welcome, {"members"});
    if (err.empty())
      err = validate_template(goodbye, {"members"});
    if (!err.empty()) {
      event.reply(dpp::ir_update_message,
                  welcome_view(ctx.db, guild, "⚠️ " + err + " Nothing was saved."));
      return true;
    }
    ctx.db.kv_set(guild, "welcome_msg", welcome);
    ctx.db.kv_set(guild, "goodbye_msg", goodbye);
    event.reply(dpp::ir_update_message,
                welcome_view(ctx.db, guild, "✅ Messages saved."));
    return true;
  }

  void on_member_join(Ctx &ctx, const dpp::guild_member_add_t &event) override {
    uint64_t guild = event.adding_guild.id;
    uint64_t user = event.added.user_id;

    uint64_t role = kv_id(ctx.db, guild, "welcome_autorole");
    if (role != 0)
      ctx.bot.guild_member_add_role(guild, user, role);

    uint64_t channel = kv_id(ctx.db, guild, "welcome_channel");
    if (channel == 0)
      return;
    dpp::user *u = dpp::find_user(user);
    TemplateContext tc = member_context(std::to_string(user),
                                        u ? u->username : "new member",
                                        event.adding_guild);
    std::string tmpl = ctx.db.kv_get(guild, "welcome_msg");
    send_to_channel(ctx, channel,
                    render_template(tmpl.empty() ? kDefaultWelcome : tmpl, tc));
  }

  void on_member_leave(Ctx &ctx, const dpp::guild_member_remove_t &event) override {
    uint64_t guild = event.guild_id;
    uint64_t channel = kv_id(ctx.db, guild, "welcome_channel");
    std::string tmpl = ctx.db.kv_get(guild, "goodbye_msg");
    if (channel == 0 || tmpl.empty())
      return;
    TemplateContext tc = member_context(std::to_string(uint64_t(event.removed.id)),
                                        event.removed.username,
                                        event.removing_guild);
    // The member is gone, so don't ping them.
    tc.user_id.clear();
    send_to_channel(ctx, channel, render_template(tmpl, tc));
  }

private:
  static std::string process_text(std::string s) {
    size_t pos = 0;
    while ((pos = s.find("\\n", pos)) != std::string::npos) {
      s.replace(pos, 2, "\n");
      pos += 1;
    }
    if (s.size() > 1000)
      s.resize(1000);
    return s;
  }
};

} // namespace

std::unique_ptr<Module> make_welcome_module() {
  return std::make_unique<WelcomeModule>();
}

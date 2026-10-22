#pragma once
#include "common.h"
#include "database.h"
#include <dpp/dpp.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct Ctx {
  dpp::cluster &bot;
  Database &db;
};

// A switchable feature. Each guild enables modules in /setup. Slash commands
// are only registered in guilds where the module is on, and event handlers
// only run there. Handlers return true when they consumed the event.
class Module {
public:
  virtual ~Module() = default;
  virtual std::string id() const = 0;
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual std::vector<dpp::slashcommand>
  commands(dpp::snowflake app_id) const = 0;

  virtual bool on_slash(Ctx &, const dpp::slashcommand_t &) { return false; }
  virtual bool on_select(Ctx &, const dpp::select_click_t &) { return false; }
  virtual bool on_button(Ctx &, const dpp::button_click_t &) { return false; }
  virtual bool on_form(Ctx &, const dpp::form_submit_t &) { return false; }
  virtual bool on_message(Ctx &, const dpp::message_create_t &) { return false; }
  virtual void on_member_join(Ctx &, const dpp::guild_member_add_t &) {}
  virtual void on_member_leave(Ctx &, const dpp::guild_member_remove_t &) {}
};

// Registry of all toggleable modules (order = order shown in /setup).
const std::vector<std::unique_ptr<Module>> &all_modules();
Module *find_module(const std::string &id);
bool module_enabled(Database &db, uint64_t guild_id, const std::string &id);

// Always-on commands: /ping /setup /bot_status /permissions
std::vector<dpp::slashcommand> core_commands(dpp::snowflake app_id);

// Registers core commands plus those of enabled modules in one guild.
void sync_guild_commands(Ctx &ctx, uint64_t guild_id);

// Factories (one per module file).
std::unique_ptr<Module> make_levels_module();
std::unique_ptr<Module> make_social_module();
std::unique_ptr<Module> make_commands_module();
std::unique_ptr<Module> make_answers_module();
std::unique_ptr<Module> make_moderation_module();
std::unique_ptr<Module> make_welcome_module();
std::unique_ptr<Module> make_roles_module();

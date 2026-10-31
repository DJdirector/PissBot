#include "module.h"

const std::vector<std::unique_ptr<Module>> &all_modules() {
  static const std::vector<std::unique_ptr<Module>> modules = [] {
    std::vector<std::unique_ptr<Module>> v;
    v.push_back(make_levels_module());
    v.push_back(make_social_module());
    v.push_back(make_commands_module());
    v.push_back(make_answers_module());
    v.push_back(make_moderation_module());
    v.push_back(make_welcome_module());
    v.push_back(make_roles_module());
    v.push_back(make_casino_module());
    return v;
  }();
  return modules;
}

Module *find_module(const std::string &id) {
  for (const auto &m : all_modules()) {
    if (m->id() == id)
      return m.get();
  }
  return nullptr;
}

bool module_enabled(Database &db, uint64_t guild_id, const std::string &id) {
  return db.get_enabled_modules(guild_id).count(id) > 0;
}

void sync_guild_commands(Ctx &ctx, uint64_t guild_id) {
  auto enabled = ctx.db.get_enabled_modules(guild_id);
  std::vector<dpp::slashcommand> cmds = core_commands(ctx.bot.me.id);
  for (const auto &m : all_modules()) {
    if (!enabled.count(m->id()))
      continue;
    for (auto &c : m->commands(ctx.bot.me.id))
      cmds.push_back(c);
  }
  ctx.bot.guild_bulk_command_create(cmds, guild_id);
}
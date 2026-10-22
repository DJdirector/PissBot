#include "common.h"
#include <algorithm>
#include <variant>

std::string replace_all(std::string str, const std::string &from,
                        const std::string &to) {
  if (from.empty())
    return str;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

bool is_admin(const dpp::interaction &cmd) {
  return cmd.get_resolved_permission(cmd.usr.id).has(dpp::p_administrator);
}

bool has_access(Database &db, const dpp::interaction &cmd,
                const std::string &feature) {
  if (is_admin(cmd))
    return true;
  auto allowed = db.get_permission_roles(cmd.guild_id, feature);
  if (allowed.empty())
    return false;
  for (const auto &role : cmd.member.get_roles()) {
    if (std::find(allowed.begin(), allowed.end(),
                  static_cast<uint64_t>(role)) != allowed.end())
      return true;
  }
  return false;
}

std::string form_field(const dpp::form_submit_t &event, const std::string &id) {
  for (const auto &c : event.components) {
    if (c.custom_id == id && std::holds_alternative<std::string>(c.value))
      return std::get<std::string>(c.value);
  }
  return "";
}

dpp::message ephemeral_text(const std::string &text) {
  return dpp::message(text).set_flags(dpp::m_ephemeral);
}

dpp::message no_permission_message() {
  return ephemeral_text("You don't have permission to use this.");
}

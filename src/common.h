#pragma once
#include "database.h"
#include <dpp/dpp.h>
#include <string>

// Shared helpers used by core code and every module.
std::string replace_all(std::string str, const std::string &from,
                        const std::string &to);

// Administrators always pass. Everyone else needs a role the owner assigned
// to `feature` in /permissions (none assigned = administrators only).
bool is_admin(const dpp::interaction &cmd);
bool has_access(Database &db, const dpp::interaction &cmd,
                const std::string &feature);

// Reads a text input out of a submitted modal by custom_id ("" if missing).
std::string form_field(const dpp::form_submit_t &event, const std::string &id);

dpp::message ephemeral_text(const std::string &text);
dpp::message no_permission_message();

#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>

// Nightbot-style templates: $(user), $(query), $(choose a|b|c), $(math 2+2)...
// Legacy {user}-style placeholders are still understood.
struct TemplateContext {
  // Explicit variables. They win over built-ins (e.g. YouTube sets "channel").
  std::map<std::string, std::string> vars;
  std::string user_id;
  std::string user_name;
  std::string channel_id;
  std::string server_name;
  std::string args; // text after the command, for $(query) / $(1)
  std::function<long long()> next_count; // backs $(count); optional
};

std::string render_template(const std::string &tmpl,
                            const TemplateContext &ctx,
                            size_t max_len = 2000);

// Evaluates + - * / % ^ and parentheses. ok=false on any error.
double eval_math(const std::string &expr, bool &ok);

// Returns an error message when a template uses $(names) that don't exist
// (e.g. $(eval ...)); empty string when fine. extra_known are context vars.
std::string validate_template(const std::string &tmpl,
                              const std::set<std::string> &extra_known = {});

// One-line help listing every variable, for dashboards and /command_add.
std::string template_help();

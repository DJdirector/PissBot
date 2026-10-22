// Checks every slash command definition against Discord's rules, so a bad
// definition is caught here instead of failing when the bot registers commands.
#include "check.h"
#include "module.h"
#include <regex>
#include <set>

int main() {
  const std::regex name_re("^[a-z0-9_-]{1,32}$");
  std::set<std::string> names;
  size_t total = 0;

  auto check = [&](const dpp::slashcommand &c) {
    CHECK(std::regex_match(c.name, name_re));
    CHECK(!c.description.empty() && c.description.size() <= 100);
    CHECK(names.insert(c.name).second); // no duplicate command names
    CHECK(c.options.size() <= 25);
    bool optional_seen = false;
    for (const auto &o : c.options) {
      CHECK(std::regex_match(o.name, name_re));
      CHECK(!o.description.empty() && o.description.size() <= 100);
      if (!o.required)
        optional_seen = true;
      else
        CHECK(!optional_seen); // required options must come first
    }
    total++;
  };

  for (const auto &c : core_commands(1))
    check(c);

  std::set<std::string> ids;
  for (const auto &m : all_modules()) {
    CHECK(ids.insert(m->id()).second); // unique module ids
    CHECK(!m->name().empty() && m->name().size() <= 100);
    CHECK(!m->description().empty() && m->description().size() <= 100);
    CHECK(!m->commands(1).empty());
    for (const auto &c : m->commands(1))
      check(c);
  }

  CHECK(total <= 100);              // Discord's per-guild slash command limit
  CHECK(all_modules().size() <= 25); // must fit in one select menu
  CHECK(find_module("moderation") != nullptr);
  CHECK(find_module("does_not_exist") == nullptr);

  std::cout << "commands_test passed (" << total << " commands, "
            << all_modules().size() << " modules)" << std::endl;
}

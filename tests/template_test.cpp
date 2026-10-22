#include "check.h"
#include "template.h"

int main() {
  TemplateContext c;
  c.user_id = "42";
  c.user_name = "Seb";
  c.channel_id = "7";
  c.server_name = "Cool Server";
  c.args = "will it rain?";
  long long n = 0;
  c.next_count = [&] { return ++n; };

  // The 8-ball example from the project brief, with $(choose) instead of eval.
  std::string t =
      "$(user) asked: \"$(query)\" 🎱 The 8-Ball says: $(choose Yes, "
      "definitely!|Most likely.|Ask again later.|Don’t count on it.|Outlook not "
      "so good.|Signs point to yes!)";
  CHECK(render_template(t, c).rfind(
            "<@42> asked: \"will it rain?\" 🎱 The 8-Ball says: ", 0) == 0);
  std::set<std::string> seen;
  for (int i = 0; i < 400; i++) {
    std::string x = render_template(t, c);
    seen.insert(x.substr(x.find("says: ")));
  }
  CHECK(seen.size() == 6);

  // Identity and input variables
  CHECK(render_template("$(username)|$(userid)|$(channelid)|$(server)|$(channel)",
                        c) == "Seb|42|7|Cool Server|<#7>");
  CHECK(render_template("$(1)-$(2)-$(9)|", c) == "will-it-|");
  CHECK(render_template("$(touser)", c) == "will");
  CHECK(render_template("$(querystring)", c) == "will%20it%20rain%3F");
  TemplateContext noargs;
  noargs.user_id = "42";
  noargs.user_name = "Seb";
  CHECK(render_template("$(touser)", noargs) == "<@42>");

  // Legacy {placeholders} are upgraded
  CHECK(render_template("hi {user} / {username} / {args}", c) ==
        "hi <@42> / Seb / will it rain?");

  // Maths
  CHECK(render_template("$(math (2+3)*4 - 6/3)", c) == "18");
  CHECK(render_template("$(math 7 % 4 + 2^3)", c) == "11");
  CHECK(render_template("$(math 1.5 * 2)", c) == "3");
  CHECK(render_template("$(math 1/0)", c) == "$(math 1/0)");
  CHECK(render_template("$(math 2+)", c) == "$(math 2+)");
  CHECK(render_template("$(math $(rand 5 5) * 2)", c) == "10");
  bool ok;
  CHECK(eval_math("2*(3+4)", ok) == 14 && ok);
  eval_math("2+", ok);
  CHECK(!ok);

  // Random numbers stay in range
  for (int i = 0; i < 200; i++) {
    int v = std::stoi(render_template("$(rand 3 6)", c));
    CHECK(v >= 3 && v <= 6);
  }
  CHECK(render_template("$(rand 9 9)", c) == "9");
  CHECK(render_template("$(rand 1)", c) == "$(rand 1)");

  // Counters and lazy evaluation inside $(choose)
  CHECK(render_template("$(count) $(count)", c) == "1 2");
  n = 0;
  for (int i = 0; i < 60; i++)
    render_template("$(choose a|$(count))", c);
  CHECK(n < 60);

  // Text helpers and time
  CHECK(render_template("$(upper abc) $(lower ABC)", c) == "ABC abc");
  CHECK(render_template("$(countdown 2026-12-25)", c).find(":R>") !=
        std::string::npos);
  CHECK(render_template("$(countdown nonsense)", c) == "$(countdown nonsense)");
  CHECK(render_template("$(time)", c).rfind("<t:", 0) == 0);

  // Escapes and malformed input
  CHECK(render_template("\\$(user) costs \\(5\\)", c) == "$(user) costs (5)");
  CHECK(render_template("$(choose Sure :\\)|Sure :\\))", c) == "Sure :)");
  CHECK(render_template("$(unknown x) and $(user", c) ==
        "$(unknown x) and $(user");

  // Context variables win over built-ins (YouTube's channel name)
  TemplateContext y;
  y.vars = {{"url", "https://y/1"}, {"channel", "Chan"}, {"role", "<@&9>"}};
  CHECK(render_template("{role} {channel} {url} $(url)", y) ==
        "<@&9> Chan https://y/1 https://y/1");

  // Limits
  CHECK(render_template(std::string(5000, 'x'), c).size() == 2000);
  CHECK(render_template(std::string(1999, 'a') + "é", c).size() == 1999);
  std::string deep;
  for (int i = 0; i < 40; i++)
    deep += "$(upper ";
  deep += "x";
  for (int i = 0; i < 40; i++)
    deep += ")";
  render_template(deep, c); // must terminate without crashing
  std::string many;
  for (int i = 0; i < 300; i++)
    many += "$(rand 1 2)";
  CHECK(!render_template(many, c).empty());

  // Validation on save
  CHECK(validate_template("$(user) $(choose a|b) $(3)").empty());
  CHECK(validate_template("$(eval 1+1)").find("not supported") !=
        std::string::npos);
  CHECK(validate_template("$(url)", {"url"}).empty());
  CHECK(!validate_template("$(url)").empty());
  CHECK(!validate_template("$(nope)").empty());
  CHECK(validate_template("\\$(nope)").empty());

  std::cout << "template_test passed" << std::endl;
}

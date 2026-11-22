#include "check.h"
#include "misc.h"
#include <set>

int main() {
  using misc::parse_reaction_emoji;

  // Normal emoji pass through, with surrounding spaces trimmed
  CHECK(parse_reaction_emoji("😀") == "😀");
  CHECK(parse_reaction_emoji("  💩 ") == "💩");
  CHECK(parse_reaction_emoji("❤️") == "❤️");       // with variation selector
  CHECK(parse_reaction_emoji("👍🏽") == "👍🏽"); // skin tone
  CHECK(parse_reaction_emoji("🇨🇴") == "🇨🇴");     // flag
  CHECK(parse_reaction_emoji("©️") == "©️");
  CHECK(parse_reaction_emoji("1️⃣") == "1️⃣"); // keycap

  // Custom emoji become name:id
  CHECK(parse_reaction_emoji("<:pog:123456789012345678>") ==
        "pog:123456789012345678");
  CHECK(parse_reaction_emoji("<a:dance:98765>") == "dance:98765");

  // Things that are not an emoji
  CHECK(parse_reaction_emoji("").empty());
  CHECK(parse_reaction_emoji("   ").empty());
  CHECK(parse_reaction_emoji("abc").empty());
  CHECK(parse_reaction_emoji("a😀").empty());
  CHECK(parse_reaction_emoji("😀 😀").empty());
  CHECK(parse_reaction_emoji("é").empty());
  CHECK(parse_reaction_emoji("1").empty());
  CHECK(parse_reaction_emoji("<:x:123>").empty());     // name too short
  CHECK(parse_reaction_emoji("<:pog:abc>").empty());   // id not a number
  CHECK(parse_reaction_emoji("<pog:123456>").empty()); // missing colon
  CHECK(parse_reaction_emoji("<:pog:123456").empty()); // missing >
  CHECK(parse_reaction_emoji(std::string(40, '\xF0')).empty());

  // The silly texts are never empty and have some variety
  std::mt19937 rng(1);
  std::set<std::string> reasons, sentences;
  for (int i = 0; i < 200; ++i) {
    std::string r = misc::fake_ban_reason(rng);
    std::string s = misc::fake_ban_sentence(rng);
    CHECK(!r.empty() && !s.empty());
    reasons.insert(r);
    sentences.insert(s);
  }
  CHECK(reasons.size() > 3 && sentences.size() > 3);

  std::cout << "misc_test passed" << std::endl;
}

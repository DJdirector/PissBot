#include "answers.h"
#include "check.h"

int main() {
  std::vector<Answer> a = {
      {"hi", "hello", false},
      {"why", "because", true},
      {"why not", "no reason", true},
      {"caf\xc3\xa9", "coffee", true},
  };
  auto m = [&](const char *msg) -> std::string {
    const Answer *h = match_answer(a, msg);
    return h ? h->trigger : "";
  };

  // Whole-message mode
  CHECK(m("hi") == "hi");
  CHECK(m("  HI!!  ") == "hi");
  CHECK(m("hi everyone") == "");
  CHECK(m("history") == "");

  // Anywhere mode respects word boundaries
  CHECK(m("but why is that") == "why");
  CHECK(m("WHY") == "why");
  CHECK(m("whyyy") == "");
  CHECK(m("a caf\xc3\xa9 nearby") == "caf\xc3\xa9");
  CHECK(m("caf\xc3\xa9s") == "");

  // Longest trigger wins
  CHECK(m("why not?") == "why not");

  // Nothing to match
  CHECK(m("") == "");
  CHECK(m("???") == "");
  CHECK(match_answer({}, "hi") == nullptr);

  // Normalising helpers
  CHECK(normalize_text("  Hello   WORLD \n") == "hello world");
  CHECK(strip_punct("?!hi..") == "hi");
  CHECK(strip_punct("...") == "");

  // Cooldown: 15 seconds per answer per server
  auto t0 = std::chrono::steady_clock::now();
  CHECK(answer_ready(1, "hi", t0));
  CHECK(!answer_ready(1, "hi", t0 + std::chrono::seconds(5)));
  CHECK(answer_ready(1, "why", t0 + std::chrono::seconds(5)));
  CHECK(answer_ready(2, "hi", t0 + std::chrono::seconds(5)));
  CHECK(answer_ready(1, "hi", t0 + std::chrono::seconds(16)));

  std::cout << "answers_test passed" << std::endl;
}

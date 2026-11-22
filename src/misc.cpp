#include "misc.h"
#include <cstdint>
#include <vector>

namespace misc {
namespace {

// First UTF-8 code point of `s`, or 0 when it isn't valid UTF-8.
uint32_t first_codepoint(const std::string &s) {
  if (s.empty())
    return 0;
  const unsigned char c = static_cast<unsigned char>(s[0]);
  size_t len = c < 0x80           ? 1
               : (c >> 5) == 0x6  ? 2
               : (c >> 4) == 0xE  ? 3
               : (c >> 3) == 0x1E ? 4
                                  : 0;
  if (len == 0 || s.size() < len)
    return 0;
  uint32_t cp = len == 1 ? c : c & (0xFF >> (len + 1));
  for (size_t i = 1; i < len; ++i) {
    const unsigned char cc = static_cast<unsigned char>(s[i]);
    if ((cc & 0xC0) != 0x80)
      return 0;
    cp = (cp << 6) | (cc & 0x3F);
  }
  return cp;
}

bool all_digits(const std::string &s) {
  if (s.empty())
    return false;
  for (char c : s) {
    if (c < '0' || c > '9')
      return false;
  }
  return true;
}

bool is_emoji_name(const std::string &s) {
  if (s.size() < 2 || s.size() > 32)
    return false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_';
    if (!ok)
      return false;
  }
  return true;
}

template <size_t N>
std::string pick(const char *const (&list)[N], std::mt19937 &rng) {
  return list[std::uniform_int_distribution<size_t>(0, N - 1)(rng)];
}

} // namespace

std::string parse_reaction_emoji(const std::string &input) {
  size_t start = input.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = input.find_last_not_of(" \t\r\n");
  const std::string text = input.substr(start, end - start + 1);

  // Custom emoji: <:name:id> or <a:name:id>
  if (text.front() == '<') {
    if (text.size() < 6 || text.back() != '>')
      return "";
    std::string inner = text.substr(1, text.size() - 2);
    size_t pos = (!inner.empty() && inner[0] == 'a') ? 1 : 0;
    if (pos >= inner.size() || inner[pos] != ':')
      return "";
    ++pos;
    size_t colon = inner.find(':', pos);
    if (colon == std::string::npos)
      return "";
    std::string name = inner.substr(pos, colon - pos);
    std::string id = inner.substr(colon + 1);
    if (!is_emoji_name(name) || !all_digits(id) || id.size() > 20)
      return "";
    return name + ":" + id;
  }

  if (text.size() > 32)
    return "";

  // Keycap emoji such as 1 + U+FE0F + U+20E3
  const unsigned char c0 = static_cast<unsigned char>(text[0]);
  if (c0 < 0x80) {
    if (!((c0 >= '0' && c0 <= '9') || c0 == '#' || c0 == '*'))
      return "";
    for (size_t i = 1; i < text.size(); ++i) {
      if (static_cast<unsigned char>(text[i]) < 0x80)
        return "";
    }
    return text.find("\xE2\x83\xA3") == std::string::npos ? "" : text;
  }

  // Everything else must be non-ASCII and start where the emoji start
  // (the copyright and registered signs are the only ones below that).
  for (char c : text) {
    if (static_cast<unsigned char>(c) < 0x80)
      return "";
  }
  uint32_t cp = first_codepoint(text);
  if (cp != 0xA9 && cp != 0xAE && cp < 0x203C)
    return "";
  return text;
}

std::string fake_ban_reason(std::mt19937 &rng) {
  static const char *const reasons[] = {
      "Crimes against piss",
      "Excessive vibes",
      "Sending messages that were technically words",
      "Unauthorized thinking",
      "Failing to pay the piss tax",
      "Looking at the bot funny",
      "Being caught lacking",
      "Suspicious levels of Tuesday",
      "Having the audacity",
      "Reasons classified by the piss council",
  };
  return pick(reasons, rng);
}

std::string fake_ban_sentence(std::mt19937 &rng) {
  static const char *const sentences[] = {
      "5 minutes of staring at the wall",
      "Must apologize to the nearest houseplant",
      "Banished until the next snack break",
      "Community service: reading the terms and conditions",
      "Life without parole (parole is a type of rice)",
      "Exiled to piss hell for a very dramatic 30 seconds",
      "One strongly worded letter, to be ignored",
      "Cannot say the word \"moist\" for the rest of the day",
  };
  return pick(sentences, rng);
}

} // namespace misc

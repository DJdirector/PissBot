#include "answers.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>

static std::mutex cooldown_mutex;
static std::map<uint64_t, std::map<std::string, std::chrono::steady_clock::time_point>>
    answer_cooldowns;

std::string normalize_text(const std::string &in) {
  std::string out;
  bool prev_space = true;
  for (unsigned char c : in) {
    if (std::isspace(c)) {
      if (!prev_space)
        out += ' ';
      prev_space = true;
    } else {
      out += static_cast<char>(c < 128 ? std::tolower(c) : c);
      prev_space = false;
    }
  }
  if (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out;
}

std::string strip_punct(const std::string &in) {
  auto is_p = [](char c) {
    return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' ||
           c == ':' || c == '~' || c == ' ';
  };
  size_t a = 0, b = in.size();
  while (a < b && is_p(in[a]))
    a++;
  while (b > a && is_p(in[b - 1]))
    b--;
  return in.substr(a, b - a);
}

static bool is_word_char(unsigned char c) {
  return std::isalnum(c) || c >= 128 || c == '_';
}

static bool contains_word(const std::string &text, const std::string &needle) {
  if (needle.empty())
    return false;
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    bool left = pos == 0 || !is_word_char(text[pos - 1]);
    size_t end = pos + needle.size();
    bool right = end >= text.size() || !is_word_char(text[end]);
    if (left && right)
      return true;
    pos += 1;
  }
  return false;
}

// Most specific (longest) matching trigger wins.
const Answer *match_answer(const std::vector<Answer> &answers,
                                  const std::string &message) {
  std::string norm = normalize_text(message);
  std::string stripped = strip_punct(norm);
  if (stripped.empty())
    return nullptr;
  const Answer *best = nullptr;
  for (const auto &a : answers) {
    bool hit = a.contains ? contains_word(norm, a.trigger)
                          : (stripped == strip_punct(a.trigger));
    if (hit && (!best || a.trigger.size() > best->trigger.size()))
      best = &a;
  }
  return best;
}

// Per-trigger cooldown so one answer can't be spammed.
bool answer_ready(uint64_t guild_id, const std::string &trigger,
                  std::chrono::steady_clock::time_point now) {
  std::lock_guard<std::mutex> lock(cooldown_mutex);
  auto &per_guild = answer_cooldowns[guild_id];
  auto it = per_guild.find(trigger);
  if (it != per_guild.end() && now - it->second < std::chrono::seconds(15))
    return false;
  per_guild[trigger] = now;
  return true;
}


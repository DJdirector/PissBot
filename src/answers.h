#pragma once
#include "database.h"
#include <chrono>
#include <string>
#include <vector>

// Answers: plain-message auto-replies ("hi" -> "hello"). Pure logic, no
// Discord dependency, so it can be unit tested.

constexpr size_t kMaxAnswers = 25;

// Lowercases ASCII, trims and collapses whitespace.
std::string normalize_text(const std::string &in);

// Trims . , ! ? ; : ~ and spaces from both ends.
std::string strip_punct(const std::string &in);

// Most specific (longest) matching trigger wins; nullptr if none match.
const Answer *match_answer(const std::vector<Answer> &answers,
                           const std::string &message);

// True (and starts the 15 second cooldown) if the answer may fire now.
bool answer_ready(uint64_t guild_id, const std::string &trigger,
                  std::chrono::steady_clock::time_point now =
                      std::chrono::steady_clock::now());

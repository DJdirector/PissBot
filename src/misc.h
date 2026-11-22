#pragma once
// Pure helpers for the misc module (no Discord dependency, so they are unit
// tested): reading an emoji the way people type it, and the silly texts.
#include <random>
#include <string>

namespace misc {
// Turns what someone typed into what Discord's reaction API wants: a unicode
// emoji as is, or "name:id" for a custom emoji (<:name:id> or <a:name:id>).
// Returns "" when the text isnt a single emoji.
std::string parse_reaction_emoji(const std::string &text);

// Absurd reasons and sentences for the fake ban.
std::string fake_ban_reason(std::mt19937 &rng);
std::string fake_ban_sentence(std::mt19937 &rng);

} // namespace misc

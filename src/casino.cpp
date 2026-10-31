#include "casino.h"
#include <algorithm>
#include <array>
#include <sstream>

namespace casino {

// ---- Slots ----------------------------------------------------------------

namespace {
const SlotSymbol kSymbols[kSlotSymbolCount] = {
    {"🍒", 8, 5},   {"🍋", 6, 8},    {"🍇", 4, 15},
    {"🔔", 3, 30},  {"💎", 2, 75},   {"7️⃣", 1, 100},
};
constexpr int kCherry = 0;

int total_weight() {
  int sum = 0;
  for (const auto &s : kSymbols)
    sum += s.weight;
  return sum;
}
} // namespace

const SlotSymbol &slot_symbol(int index) {
  return kSymbols[std::clamp(index, 0, kSlotSymbolCount - 1)];
}

int spin_reel(std::mt19937 &rng) {
  int roll = std::uniform_int_distribution<int>(0, total_weight() - 1)(rng);
  for (int i = 0; i < kSlotSymbolCount; ++i) {
    if (roll < kSymbols[i].weight)
      return i;
    roll -= kSymbols[i].weight;
  }
  return 0;
}

int slot_multiplier(int a, int b, int c) {
  if (a == b && b == c)
    return slot_symbol(a).triple_multiplier;
  int cherries = (a == kCherry) + (b == kCherry) + (c == kCherry);
  return cherries == 2 ? kCherryPairMultiplier : 0;
}

double slots_expected_return() {
  double total = 0;
  const double w = total_weight();
  for (int a = 0; a < kSlotSymbolCount; ++a)
    for (int b = 0; b < kSlotSymbolCount; ++b)
      for (int c = 0; c < kSlotSymbolCount; ++c) {
        double p = (kSymbols[a].weight / w) * (kSymbols[b].weight / w) *
                   (kSymbols[c].weight / w);
        total += p * slot_multiplier(a, b, c);
      }
  return total;
}

// ---- Roulette -----------------------------------------------------------------

bool parse_roulette_kind(const std::string &t, RouletteKind &out) {
  static const std::pair<const char *, RouletteKind> kKinds[] = {
      {"red", RouletteKind::Red},       {"black", RouletteKind::Black},
      {"even", RouletteKind::Even},     {"odd", RouletteKind::Odd},
      {"low", RouletteKind::Low},       {"high", RouletteKind::High},
      {"dozen1", RouletteKind::Dozen1}, {"dozen2", RouletteKind::Dozen2},
      {"dozen3", RouletteKind::Dozen3}, {"number", RouletteKind::Number},
  };
  for (const auto &[name, kind] : kKinds) {
    if (t == name) {
      out = kind;
      return true;
    }
  }
  return false;
}

bool roulette_is_red(int pocket) {
  static const bool kRed[37] = {
      false, true,  false, true,  false, true,  false, true,  false, true,
      false, false, true,  false, true,  false, true,  false, true,  true,
      false, true,  false, true,  false, true,  false, true,  false, false,
      true,  false, true,  false, true,  false, true};
  return pocket >= 0 && pocket <= 36 && kRed[pocket];
}

int roulette_multiplier(const RouletteBet &bet, int p) {
  if (p < 0 || p > 36)
    return 0;
  switch (bet.kind) {
  case RouletteKind::Number:
    return p == bet.number ? kNumberMultiplier : 0;
  case RouletteKind::Red:
    return p != 0 && roulette_is_red(p) ? 2 : 0;
  case RouletteKind::Black:
    return p != 0 && !roulette_is_red(p) ? 2 : 0;
  case RouletteKind::Even:
    return p != 0 && p % 2 == 0 ? 2 : 0;
  case RouletteKind::Odd:
    return p % 2 == 1 ? 2 : 0;
  case RouletteKind::Low:
    return p >= 1 && p <= 18 ? 2 : 0;
  case RouletteKind::High:
    return p >= 19 ? 2 : 0;
  case RouletteKind::Dozen1:
    return p >= 1 && p <= 12 ? 3 : 0;
  case RouletteKind::Dozen2:
    return p >= 13 && p <= 24 ? 3 : 0;
  case RouletteKind::Dozen3:
    return p >= 25 ? 3 : 0;
  }
  return 0;
}

double roulette_expected_return(const RouletteBet &bet) {
  double sum = 0;
  for (int p = 0; p <= 36; ++p)
    sum += roulette_multiplier(bet, p);
  return sum / 37.0;
}

std::string roulette_label(const RouletteBet &bet) {
  switch (bet.kind) {
  case RouletteKind::Red: return "Red";
  case RouletteKind::Black: return "Black";
  case RouletteKind::Even: return "Even";
  case RouletteKind::Odd: return "Odd";
  case RouletteKind::Low: return "Low (1-18)";
  case RouletteKind::High: return "High (19-36)";
  case RouletteKind::Dozen1: return "1st dozen (1-12)";
  case RouletteKind::Dozen2: return "2nd dozen (13-24)";
  case RouletteKind::Dozen3: return "3rd dozen (25-36)";
  case RouletteKind::Number: return "Number " + std::to_string(bet.number);
  }
  return "";
}

// ---- Blackjack ----------------------------------------------------------------

std::string card_text(int card) {
  static const char *kRanks[13] = {"A", "2", "3", "4",  "5", "6", "7",
                                   "8", "9", "10", "J", "Q", "K"};
  static const char *kSuits[4] = {"♠", "♥", "♦", "♣"};
  if (card < 0 || card > 51)
    return "?";
  return std::string(kRanks[card % 13]) + kSuits[card / 13];
}

std::string cards_text(const std::vector<int> &cards) {
  std::string out;
  for (int c : cards) {
    if (!out.empty())
      out += " ";
    out += "`" + card_text(c) + "`";
  }
  return out;
}

HandValue hand_value(const std::vector<int> &cards) {
  int total = 0, aces = 0;
  for (int c : cards) {
    int rank = c % 13;
    if (rank == 0) {
      total += 11;
      ++aces;
    } else {
      total += std::min(rank + 1, 10);
    }
  }
  while (total > 21 && aces > 0) {
    total -= 10;
    --aces;
  }
  return {total, aces > 0};
}

bool is_blackjack(const std::vector<int> &cards) {
  return cards.size() == 2 && hand_value(cards).total == 21;
}

std::vector<int> shuffled_deck(std::mt19937 &rng) {
  std::vector<int> deck(52);
  for (int i = 0; i < 52; ++i)
    deck[i] = i;
  std::shuffle(deck.begin(), deck.end(), rng);
  return deck;
}

namespace {
int draw(BlackjackGame &g) {
  int card = g.deck.back();
  g.deck.pop_back();
  return card;
}
} // namespace

BlackjackGame blackjack_deal(int bet, int64_t now, std::mt19937 &rng) {
  BlackjackGame g;
  g.bet = bet;
  g.started = now;
  g.deck = shuffled_deck(rng);
  g.player.push_back(draw(g));
  g.dealer.push_back(draw(g));
  g.player.push_back(draw(g));
  g.dealer.push_back(draw(g));
  return g;
}

void blackjack_hit(BlackjackGame &g) { g.player.push_back(draw(g)); }

void blackjack_double(BlackjackGame &g) {
  g.doubled = true;
  g.player.push_back(draw(g));
}

void blackjack_dealer_play(BlackjackGame &g) {
  while (hand_value(g.dealer).total < 17 && !g.deck.empty())
    g.dealer.push_back(draw(g));
}

bool blackjack_player_bust(const BlackjackGame &g) {
  return hand_value(g.player).total > 21;
}

bool blackjack_finished_on_deal(const BlackjackGame &g) {
  return is_blackjack(g.player) || is_blackjack(g.dealer);
}

BlackjackOutcome blackjack_outcome(const BlackjackGame &g) {
  const bool player_bj = is_blackjack(g.player) && !g.doubled;
  const bool dealer_bj = is_blackjack(g.dealer);
  if (player_bj && dealer_bj)
    return BlackjackOutcome::Push;
  if (player_bj)
    return BlackjackOutcome::Blackjack;
  if (dealer_bj)
    return BlackjackOutcome::Loss;

  const int p = hand_value(g.player).total;
  const int d = hand_value(g.dealer).total;
  if (p > 21)
    return BlackjackOutcome::Loss;
  if (d > 21 || p > d)
    return BlackjackOutcome::Win;
  return p == d ? BlackjackOutcome::Push : BlackjackOutcome::Loss;
}

int blackjack_return(const BlackjackGame &g, BlackjackOutcome outcome) {
  switch (outcome) {
  case BlackjackOutcome::Blackjack:
    return g.stake() * 5 / 2; // pays 3 to 2
  case BlackjackOutcome::Win:
    return g.stake() * 2;
  case BlackjackOutcome::Push:
    return g.stake();
  case BlackjackOutcome::Loss:
    return 0;
  }
  return 0;
}

// Format: "bet,started,doubled|player cards|dealer cards|deck cards"
std::string blackjack_serialize(const BlackjackGame &g) {
  auto list = [](const std::vector<int> &v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i)
      s += (i ? " " : "") + std::to_string(v[i]);
    return s;
  };
  return std::to_string(g.bet) + "," + std::to_string(g.started) + "," +
         (g.doubled ? "1" : "0") + "|" + list(g.player) + "|" +
         list(g.dealer) + "|" + list(g.deck);
}

bool blackjack_parse(const std::string &text, BlackjackGame &out) {
  std::vector<std::string> parts;
  std::stringstream ss(text);
  std::string part;
  while (std::getline(ss, part, '|'))
    parts.push_back(part);
  if (text.empty() || text.back() == '|')
    parts.push_back(""); // trailing empty section (empty deck)
  if (parts.size() != 4)
    return false;

  BlackjackGame g;
  char c1 = 0, c2 = 0;
  int doubled = 0;
  std::stringstream head(parts[0]);
  if (!(head >> g.bet >> c1 >> g.started >> c2 >> doubled) || c1 != ',' ||
      c2 != ',' || (doubled != 0 && doubled != 1) || g.bet < kMinBet ||
      g.bet > kMaxBet)
    return false;
  g.doubled = doubled == 1;

  std::array<bool, 52> seen{};
  auto read_cards = [&](const std::string &s, std::vector<int> &dest) {
    std::stringstream cs(s);
    int card;
    while (cs >> card) {
      if (card < 0 || card > 51 || seen[card])
        return false;
      seen[card] = true;
      dest.push_back(card);
    }
    return cs.eof();
  };
  if (!read_cards(parts[1], g.player) || !read_cards(parts[2], g.dealer) ||
      !read_cards(parts[3], g.deck) || g.player.size() < 2 ||
      g.dealer.size() < 2)
    return false;
  out = g;
  return true;
}

} // namespace casino
#pragma once
// Pure casino logic (no Discord, no database): payout tables, card handling and
// hand serialization. The Discord side lives in modules/casino.cpp.
//
// Games are played with piss tokens, the casino's own currency (not XP). Every
// game works the same way: the bet is taken up front, and a win pays back
// `bet * multiplier` (the bet included). A multiplier of 0 means the bet is lost.
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace casino {

constexpr int kMinBet = 10;
constexpr int kMaxBet = 200;
constexpr int kCooldownSeconds = 5 * 60;   // between games, per player
constexpr int kHandTimeoutSeconds = 4 * 60; // idle blackjack hand stands

constexpr int kDailyTokens = 100;           // free tokens per claim
constexpr int kDailySeconds = 24 * 60 * 60; // time between claims
constexpr const char *kRoleName = "Gambler"; // created on /casino opt-in

// ---- Slots ----------------------------------------------------------------

constexpr int kSlotSymbolCount = 6;

struct SlotSymbol {
  const char *emoji;
  int weight;            // how common the symbol is on a reel
  int triple_multiplier; // payout for three in a row
};

constexpr int kCherryPairMultiplier = 2; // exactly two cherries

const SlotSymbol &slot_symbol(int index);
int spin_reel(std::mt19937 &rng); // returns a symbol index
int slot_multiplier(int a, int b, int c);
double slots_expected_return(); // exact return per 1 bet (1.0 = break even)

// ---- Roulette (single zero, 0-36) -----------------------------------------

enum class RouletteKind {
  Red, Black, Even, Odd, Low, High, Dozen1, Dozen2, Dozen3, Number
};

constexpr int kNumberMultiplier = 30; // real casinos pay 36

struct RouletteBet {
  RouletteKind kind = RouletteKind::Red;
  int number = 0; // only for RouletteKind::Number
};

bool parse_roulette_kind(const std::string &text, RouletteKind &out);
bool roulette_is_red(int pocket);
int roulette_multiplier(const RouletteBet &bet, int pocket);
double roulette_expected_return(const RouletteBet &bet);
std::string roulette_label(const RouletteBet &bet);

// ---- Blackjack --------------------------------------------------------------
// One 52-card deck per hand, dealer stands on every 17. Actions: hit, stand,
// double down. No splitting or insurance. Cards are 0-51: rank = card % 13
// (0 = Ace, 10-12 = J Q K), suit = card / 13.

struct HandValue {
  int total = 0;
  bool soft = false; // an ace is currently counted as 11
};

std::string card_text(int card);
std::string cards_text(const std::vector<int> &cards);
HandValue hand_value(const std::vector<int> &cards);
bool is_blackjack(const std::vector<int> &cards); // two cards totalling 21
std::vector<int> shuffled_deck(std::mt19937 &rng);

struct BlackjackGame {
  int bet = 0;           // the original bet
  int64_t started = 0;   // unix seconds, used for the idle timeout
  bool doubled = false;
  std::vector<int> player, dealer, deck; // deck draws from the back

  int stake() const { return doubled ? bet * 2 : bet; } // tokens put in so far
};

enum class BlackjackOutcome { Win, Loss, Push, Blackjack };

BlackjackGame blackjack_deal(int bet, int64_t now, std::mt19937 &rng);
void blackjack_hit(BlackjackGame &game);          // player draws a card
void blackjack_double(BlackjackGame &game);       // doubles the stake, one card
void blackjack_dealer_play(BlackjackGame &game);  // dealer draws to 17
bool blackjack_player_bust(const BlackjackGame &game);
bool blackjack_finished_on_deal(const BlackjackGame &game); // a natural 21
BlackjackOutcome blackjack_outcome(const BlackjackGame &game);
int blackjack_return(const BlackjackGame &game, BlackjackOutcome outcome);

std::string blackjack_serialize(const BlackjackGame &game);
bool blackjack_parse(const std::string &text, BlackjackGame &out);

} // namespace casino
// Checks the casino rules: payout tables, house edge, hand values and saving
// a blackjack hand. All logic here is pure (no Discord, no database).
#include "casino.h"
#include "check.h"
#include <set>

using namespace casino;

static bool near(double a, double b, double eps = 1e-9) {
  return a > b - eps && a < b + eps;
}

static void test_slots() {
  const int C = 0, L = 1, G = 2, B = 3, D = 4, S = 5;
  CHECK(slot_multiplier(C, C, C) == 5);
  CHECK(slot_multiplier(L, L, L) == 8);
  CHECK(slot_multiplier(G, G, G) == 15);
  CHECK(slot_multiplier(B, B, B) == 30);
  CHECK(slot_multiplier(D, D, D) == 75);
  CHECK(slot_multiplier(S, S, S) == 100);
  // Exactly two cherries, in any position, pay back x2.
  CHECK(slot_multiplier(C, C, L) == 2);
  CHECK(slot_multiplier(C, L, C) == 2);
  CHECK(slot_multiplier(L, C, C) == 2);
  // One cherry or a pair of anything else pays nothing.
  CHECK(slot_multiplier(C, L, G) == 0);
  CHECK(slot_multiplier(L, L, G) == 0);
  CHECK(slot_multiplier(S, S, C) == 0);

  // Generous but not a money printer: the house keeps a few percent.
  double rtp = slots_expected_return();
  CHECK(rtp > 0.85 && rtp < 0.98);

  // The reel only produces valid symbols and every symbol can appear.
  std::mt19937 rng(1);
  std::set<int> seen;
  for (int i = 0; i < 20000; ++i) {
    int s = spin_reel(rng);
    CHECK(s >= 0 && s < kSlotSymbolCount);
    seen.insert(s);
  }
  CHECK(seen.size() == static_cast<size_t>(kSlotSymbolCount));
}

static void test_roulette() {
  CHECK(roulette_is_red(1) && roulette_is_red(36) && roulette_is_red(19));
  CHECK(!roulette_is_red(2) && !roulette_is_red(0) && !roulette_is_red(35));
  int reds = 0;
  for (int p = 1; p <= 36; ++p)
    reds += roulette_is_red(p);
  CHECK(reds == 18);

  RouletteBet red{RouletteKind::Red}, black{RouletteKind::Black},
      even{RouletteKind::Even}, odd{RouletteKind::Odd},
      low{RouletteKind::Low}, high{RouletteKind::High},
      d1{RouletteKind::Dozen1}, d2{RouletteKind::Dozen2},
      d3{RouletteKind::Dozen3};
  CHECK(roulette_multiplier(red, 1) == 2 && roulette_multiplier(red, 2) == 0);
  CHECK(roulette_multiplier(black, 2) == 2 && roulette_multiplier(black, 1) == 0);
  CHECK(roulette_multiplier(even, 4) == 2 && roulette_multiplier(odd, 5) == 2);
  CHECK(roulette_multiplier(low, 18) == 2 && roulette_multiplier(low, 19) == 0);
  CHECK(roulette_multiplier(high, 19) == 2 && roulette_multiplier(high, 18) == 0);
  CHECK(roulette_multiplier(d1, 12) == 3 && roulette_multiplier(d1, 13) == 0);
  CHECK(roulette_multiplier(d2, 13) == 3 && roulette_multiplier(d2, 24) == 3);
  CHECK(roulette_multiplier(d3, 25) == 3 && roulette_multiplier(d3, 36) == 3);
  // Zero beats every outside bet.
  for (const auto &b : {red, black, even, odd, low, high, d1, d2, d3})
    CHECK(roulette_multiplier(b, 0) == 0);
  RouletteBet n17{RouletteKind::Number, 17}, n0{RouletteKind::Number, 0};
  CHECK(roulette_multiplier(n17, 17) == kNumberMultiplier);
  CHECK(roulette_multiplier(n17, 18) == 0);
  CHECK(roulette_multiplier(n0, 0) == kNumberMultiplier);

  // Every bet type has a house edge, and none is a bad deal beyond reason.
  for (const auto &b : {red, black, even, odd, low, high, d1, d2, d3}) {
    double r = roulette_expected_return(b);
    CHECK(r < 1.0 && r > 0.95);
  }
  CHECK(roulette_expected_return(n17) < 1.0 &&
        roulette_expected_return(n17) > 0.7);

  RouletteKind k;
  CHECK(parse_roulette_kind("dozen2", k) && k == RouletteKind::Dozen2);
  CHECK(parse_roulette_kind("number", k) && k == RouletteKind::Number);
  CHECK(!parse_roulette_kind("purple", k));
}

static std::vector<int> cards(std::initializer_list<int> ranks) {
  // Ranks 0 (ace) to 12 (king), all spades.
  return std::vector<int>(ranks);
}

static void test_hand_values() {
  CHECK(hand_value(cards({0, 12})).total == 21 && hand_value(cards({0, 12})).soft);
  CHECK(hand_value(cards({0, 0})).total == 12);         // A A = 12, not 22
  CHECK(hand_value(cards({0, 0, 8})).total == 21);      // A A 9
  CHECK(hand_value(cards({9, 5, 8})).total == 25);      // 10 6 9 busts
  CHECK(hand_value(cards({0, 5})).total == 17 && hand_value(cards({0, 5})).soft);
  CHECK(!hand_value(cards({0, 5, 9})).soft);            // A 6 10 = 17 hard
  CHECK(hand_value(cards({10, 11, 12})).total == 30);   // faces count 10
  CHECK(is_blackjack(cards({0, 9})) && is_blackjack(cards({12, 0})));
  CHECK(!is_blackjack(cards({6, 6, 6})));               // three cards, 21
  CHECK(card_text(0) == "A♠" && card_text(13 + 9) == "10♥" && card_text(51) == "K♣");
}

static BlackjackGame game(int bet, std::vector<int> p, std::vector<int> d,
                          std::vector<int> deck = {}) {
  BlackjackGame g;
  g.bet = bet;
  g.player = p;
  g.dealer = d;
  g.deck = deck;
  return g;
}

static void test_blackjack_payouts() {
  using O = BlackjackOutcome;
  // Win pays x2, push returns the bet, loss pays nothing.
  auto win = game(50, cards({9, 8}), cards({9, 6}));      // 19 v 17
  CHECK(blackjack_outcome(win) == O::Win && blackjack_return(win, O::Win) == 100);
  auto push = game(50, cards({9, 7}), cards({9, 7}));
  CHECK(blackjack_outcome(push) == O::Push && blackjack_return(push, O::Push) == 50);
  auto loss = game(50, cards({9, 6}), cards({9, 8}));
  CHECK(blackjack_outcome(loss) == O::Loss && blackjack_return(loss, O::Loss) == 0);
  auto bust = game(50, cards({9, 5, 8}), cards({9, 6}));
  CHECK(blackjack_outcome(bust) == O::Loss);
  auto dbust = game(50, cards({9, 5}), cards({9, 5, 8}));
  CHECK(blackjack_outcome(dbust) == O::Win);

  // Naturals: pays 3 to 2 (rounded down), push if the dealer has one too.
  auto nat = game(15, cards({0, 12}), cards({9, 6}));
  CHECK(blackjack_finished_on_deal(nat));
  CHECK(blackjack_outcome(nat) == O::Blackjack && blackjack_return(nat, O::Blackjack) == 37);
  auto both = game(50, cards({0, 12}), cards({0, 11}));
  CHECK(blackjack_outcome(both) == O::Push);
  auto dealer_nat = game(50, cards({9, 8}), cards({0, 12}));
  CHECK(blackjack_outcome(dealer_nat) == O::Loss);

  // Doubling doubles what is at stake, and the payout.
  auto dbl = game(50, cards({4, 5, 8}), cards({9, 6}));
  dbl.doubled = true;
  CHECK(dbl.stake() == 100);
  CHECK(blackjack_outcome(dbl) == O::Win && blackjack_return(dbl, O::Win) == 200);

  // The dealer draws to 17 and stops, standing on soft 17.
  // Dealer 5 + ace = soft 16 draws a 2 (cards draw from the back of the deck).
  auto dealer = game(10, cards({9, 8}), cards({4, 0}), cards({12, 1}));
  blackjack_dealer_play(dealer);
  CHECK(dealer.dealer.size() == 3 && hand_value(dealer.dealer).total == 18);
  auto soft = game(10, cards({9, 8}), cards({0, 5}), cards({12}));
  blackjack_dealer_play(soft);
  CHECK(soft.dealer.size() == 2); // soft 17 stands
}

static void test_blackjack_play_and_storage() {
  std::mt19937 rng(42);

  // A deck has 52 different cards.
  auto deck = shuffled_deck(rng);
  CHECK(deck.size() == 52 && std::set<int>(deck.begin(), deck.end()).size() == 52);

  // Dealing takes four cards and keeps them all distinct.
  BlackjackGame g = blackjack_deal(30, 1000, rng);
  CHECK(g.player.size() == 2 && g.dealer.size() == 2 && g.deck.size() == 48);

  // Saving and loading a hand gives the same hand back.
  BlackjackGame back;
  CHECK(blackjack_parse(blackjack_serialize(g), back));
  CHECK(back.bet == 30 && back.started == 1000 && !back.doubled);
  CHECK(back.player == g.player && back.dealer == g.dealer && back.deck == g.deck);
  blackjack_hit(g);
  g.doubled = true;
  CHECK(blackjack_parse(blackjack_serialize(g), back) && back.doubled &&
        back.player.size() == 3);

  // An empty deck still round-trips; damaged data is refused.
  BlackjackGame empty = game(20, cards({1, 2}), cards({3, 4}), {});
  empty.started = 5;
  CHECK(blackjack_parse(blackjack_serialize(empty), back) && back.deck.empty());
  CHECK(!blackjack_parse("", back));
  CHECK(!blackjack_parse("garbage", back));
  CHECK(!blackjack_parse("30,1000,0|1 2|3 4", back));            // missing deck
  CHECK(!blackjack_parse("30,1000,0|1 1|3 4|5 6", back));        // duplicate card
  CHECK(!blackjack_parse("30,1000,0|1 2|3 99|5 6", back));       // not a card
  CHECK(!blackjack_parse("5000,1000,0|1 2|3 4|5 6", back));      // bet too big
  CHECK(!blackjack_parse("30,1000,0|1|3 4|5 6", back));          // one card

  // Playing like the dealer (hit to 17) loses a little over time: the game is
  // fair but not generous.
  double staked = 0, returned = 0;
  for (int i = 0; i < 100000; ++i) {
    BlackjackGame h = blackjack_deal(100, 0, rng);
    if (!blackjack_finished_on_deal(h)) {
      while (hand_value(h.player).total < 17)
        blackjack_hit(h);
      if (!blackjack_player_bust(h))
        blackjack_dealer_play(h);
    }
    staked += h.stake();
    returned += blackjack_return(h, blackjack_outcome(h));
  }
  double rtp = returned / staked;
  CHECK(rtp > 0.88 && rtp < 1.0);
}

int main() {
  test_slots();
  test_roulette();
  test_hand_values();
  test_blackjack_payouts();
  test_blackjack_play_and_storage();
  CHECK(kMinBet > 0 && kMaxBet >= kMinBet && kCooldownSeconds == 300);
  std::cout << "casino_test passed" << std::endl;
}
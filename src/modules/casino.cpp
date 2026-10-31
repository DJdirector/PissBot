// Casino: slots, roulette and blackjack played with piss tokens. Members opt in
// with /casino opt-in (which gives them the Gambler role) and claim 100 tokens
// a day. The game rules and payouts are in casino.h; this file is the Discord
// side (commands, buttons, cooldowns, and saving a blackjack hand so it
// survives a bot restart).
#include "casino.h"
#include "module.h"
#include <ctime>
#include <mutex>
#include <random>

namespace {

using namespace casino;

const uint32_t kGreen = 0x57F287, kRed = 0xED4245, kGrey = 0x99AAB5,
               kBlue = 0x5865F2;

// Every casino action runs under this lock, so balance checks, payouts and
// the saved blackjack hand can't interleave between two clicks.
std::mutex g_mutex;

std::mt19937 &rng() {
  static std::mt19937 gen = [] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    return std::mt19937(seq);
  }();
  return gen;
}

int64_t now_s() { return static_cast<int64_t>(std::time(nullptr)); }
std::string str(uint64_t v) { return std::to_string(v); }

std::string cooldown_key(uint64_t user) { return "casino_cd_" + str(user); }
std::string hand_key(uint64_t user) { return "casino_bj_" + str(user); }

dpp::message embed_message(const std::string &title, const std::string &body,
                           uint32_t color) {
  dpp::embed e;
  e.set_title(title).set_description(body).set_color(color);
  dpp::message m;
  m.add_embed(e);
  return m;
}

std::string tokens(int64_t n) {
  return "**" + std::to_string(n) + "** piss tokens";
}

void credit(Ctx &ctx, uint64_t guild, uint64_t user, int amount) {
  if (amount > 0)
    ctx.db.token_add(guild, user, amount);
}

// "You won +X" / "Push" / "You lost X" plus the new balance.
std::string result_text(Ctx &ctx, uint64_t guild, uint64_t user, int stake,
                        int paid_back) {
  std::string line;
  if (paid_back > stake)
    line = "🎉 You won **+" + std::to_string(paid_back - stake) +
           "** piss tokens";
  else if (paid_back == stake)
    line = "🤝 Your bet of " + tokens(stake) + " is returned";
  else
    line = "❌ You lost " + tokens(stake);
  return line + "\nBalance: " + tokens(ctx.db.token_balance(guild, user));
}

// ---- Blackjack hand storage and display -----------------------------------

bool load_hand(Ctx &ctx, uint64_t guild, uint64_t user, BlackjackGame &g) {
  std::string raw = ctx.db.kv_get(guild, hand_key(user));
  if (raw.empty())
    return false;
  if (blackjack_parse(raw, g))
    return true;
  ctx.db.kv_set(guild, hand_key(user), ""); // unreadable: drop it
  return false;
}

void save_hand(Ctx &ctx, uint64_t guild, uint64_t user, const BlackjackGame &g) {
  ctx.db.kv_set(guild, hand_key(user), blackjack_serialize(g));
}

std::string value_text(const std::vector<int> &cards) {
  HandValue v = hand_value(cards);
  return std::string(v.soft && v.total <= 21 ? "soft " : "") +
         std::to_string(v.total);
}

dpp::message hand_view(const BlackjackGame &g, uint64_t user, bool reveal,
                       const std::string &headline, uint32_t color,
                       bool can_double) {
  std::string body = "<@" + str(user) + ">, bet " + tokens(g.stake()) + "\n\n";
  body += "Dealer: " +
          (reveal ? cards_text(g.dealer) + " (" + value_text(g.dealer) + ")"
                  : "`" + card_text(g.dealer[0]) + "` `?`") +
          "\n";
  body += "You: " + cards_text(g.player) + " (" + value_text(g.player) + ")\n";
  if (!headline.empty())
    body += "\n" + headline;
  dpp::message msg = embed_message("🃏 Blackjack", body, color);

  if (!reveal) {
    auto button = [&](const char *action, const char *label,
                      dpp::component_style style) {
      dpp::component b;
      b.set_type(dpp::cot_button)
          .set_label(label)
          .set_style(style)
          .set_id(std::string("bj_") + action + ":" + str(user));
      return b;
    };
    dpp::component row;
    row.add_component(button("hit", "Hit", dpp::cos_primary));
    row.add_component(button("stand", "Stand", dpp::cos_secondary));
    if (can_double)
      row.add_component(button("double", "Double down", dpp::cos_success));
    msg.add_component(row);
  }
  return msg;
}

// Ends a hand: the dealer plays (unless the player busted or someone has a
// natural), the hand is removed from storage, winnings are paid out.
dpp::message finish_hand(Ctx &ctx, uint64_t guild, uint64_t user,
                         BlackjackGame &g, const std::string &note) {
  ctx.db.kv_set(guild, hand_key(user), "");
  if (!blackjack_player_bust(g) && !blackjack_finished_on_deal(g))
    blackjack_dealer_play(g);

  BlackjackOutcome outcome = blackjack_outcome(g);
  int paid = blackjack_return(g, outcome);
  credit(ctx, guild, user, paid);

  std::string headline;
  uint32_t color = kRed;
  switch (outcome) {
  case BlackjackOutcome::Blackjack:
    headline = "🃏 **Blackjack!** Pays 3 to 2.";
    color = kGreen;
    break;
  case BlackjackOutcome::Win:
    headline = blackjack_player_bust(g) ? "" : "✅ You beat the dealer.";
    color = kGreen;
    break;
  case BlackjackOutcome::Push:
    headline = "Push.";
    color = kGrey;
    break;
  case BlackjackOutcome::Loss:
    headline = blackjack_player_bust(g) ? "💥 Bust!" : "The dealer wins.";
    break;
  }
  if (!note.empty())
    headline = note + "\n" + headline;
  headline += "\n" + result_text(ctx, guild, user, g.stake(), paid);
  return hand_view(g, user, true, headline, color, false);
}

// A hand left idle past the timeout stands on its own. Called before the
// player's next game so an abandoned hand still gets paid or lost.
void settle_stale_hand(Ctx &ctx, uint64_t guild, uint64_t user) {
  BlackjackGame g;
  if (!load_hand(ctx, guild, user, g))
    return;
  if (now_s() - g.started < kHandTimeoutSeconds)
    return;
  ctx.db.kv_set(guild, hand_key(user), "");
  blackjack_dealer_play(g);
  credit(ctx, guild, user, blackjack_return(g, blackjack_outcome(g)));
}

// ---- Starting a game ---------------------------------------------------------

// "" = the bet is taken and the cooldown has started; otherwise the reason the
// player can't play right now.
std::string try_start(Ctx &ctx, uint64_t guild, uint64_t user, int64_t bet) {
  settle_stale_hand(ctx, guild, user);

  BlackjackGame open;
  if (load_hand(ctx, guild, user, open))
    return "You still have a blackjack hand in progress. Finish it first.";

  std::string last = ctx.db.kv_get(guild, cooldown_key(user));
  if (!last.empty()) {
    try {
      int64_t ends = std::stoll(last) + kCooldownSeconds;
      if (ends > now_s())
        return "⏳ The casino needs a break. You can play again <t:" +
               std::to_string(ends) + ":R>.";
    } catch (...) {
    }
  }

  if (bet < kMinBet || bet > kMaxBet)
    return "Bets must be between " + std::to_string(kMinBet) + " and " +
           std::to_string(kMaxBet) + " piss tokens.";
  if (!ctx.db.token_spend(guild, user, bet))
    return "You don't have enough piss tokens for that bet. You have " +
           tokens(ctx.db.token_balance(guild, user)) +
           ". Claim your daily tokens with /casino daily.";

  ctx.db.kv_set(guild, cooldown_key(user), std::to_string(now_s()));
  return "";
}

std::string rules_text() {
  std::string t = "**Casino rules**\n";
  t += "Opt in with `/casino opt-in` to get the " + std::string(kRoleName) +
       " role, then claim **" + std::to_string(kDailyTokens) +
       "** piss tokens every day with `/casino daily`. Games are played with "
       "piss tokens, not XP.\n";
  t += "Bets are " + std::to_string(kMinBet) + " to " + std::to_string(kMaxBet) +
       " piss tokens. One game every " + std::to_string(kCooldownSeconds / 60) +
       " minutes, whichever game you pick.\n\n";
  t += "**Slots**: two 🍒 pays x" + std::to_string(kCherryPairMultiplier) +
       ". Three in a row pays ";
  for (int i = 0; i < kSlotSymbolCount; ++i)
    t += std::string(slot_symbol(i).emoji) + " x" +
         std::to_string(slot_symbol(i).triple_multiplier) +
         (i + 1 < kSlotSymbolCount ? ", " : ".\n\n");
  t += "**Roulette**: red, black, even, odd, low or high pay x2. A dozen pays "
       "x3. A single number pays x" +
       std::to_string(kNumberMultiplier) + ". Zero loses everything but a bet "
       "on 0.\n\n";
  t += "**Blackjack**: win pays x2, blackjack pays x2.5, a push returns your "
       "bet. Dealer stands on 17. Hit, stand or double down. A hand left "
       "idle for " +
       std::to_string(kHandTimeoutSeconds / 60) + " minutes stands.\n\n";
  t += "A payout of xN gives back N times your bet, bet included.";
  return t;
}

// ---- Opt-in role -----------------------------------------------------------------

uint64_t configured_role(Database &db, uint64_t guild) {
  std::string v = db.kv_get(guild, "casino_role");
  if (v.empty())
    return 0;
  try {
    return std::stoull(v);
  } catch (...) {
    return 0;
  }
}

bool has_gambler_role(Database &db, const dpp::interaction &cmd) {
  uint64_t role = configured_role(db, cmd.guild_id);
  if (role == 0)
    return false;
  for (const auto &r : cmd.member.get_roles())
    if (static_cast<uint64_t>(r) == role)
      return true;
  return false;
}

const char *kNeedOptIn =
    "You need to opt in first. Use `/casino opt-in` to join the casino.";

std::string next_daily_text(int64_t last) {
  return "You already claimed today. Come back <t:" +
         std::to_string(last + kDailySeconds) + ":R>.";
}

// Claims the daily tokens; returns "" on success, otherwise why not.
std::string claim_daily(Database &db, uint64_t guild, uint64_t user) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (db.token_claim_daily(guild, user, kDailyTokens, now_s(), kDailySeconds))
    return "";
  return next_daily_text(db.token_last_daily(guild, user));
}

// Gives the member the Gambler role (creating it the first time), then
// replies. This runs asynchronously, so it only captures things that outlive
// the command (the bot and the database), never the Ctx.
void opt_in(Ctx &ctx, const dpp::slashcommand_t &event) {
  dpp::cluster &bot = ctx.bot;
  Database &db = ctx.db;
  const uint64_t guild = event.command.guild_id;
  const uint64_t user = event.command.usr.id;

  if (has_gambler_role(db, event.command)) {
    event.reply(ephemeral_text("You're already in the casino. Balance: " +
                               tokens(db.token_balance(guild, user))));
    return;
  }
  event.thinking(true);

  const std::string role_problem =
      "I couldn't set up the " + std::string(kRoleName) +
      " role. An admin needs to give me the Manage Roles permission, or pick "
      "an existing role with `/casino role`.";

  auto give_role = [&bot, &db, event, guild, user,
                    role_problem](uint64_t role_id) {
    bot.guild_member_add_role(
        guild, user, role_id,
        [&db, event, guild, user,
         role_problem](const dpp::confirmation_callback_t &cb) {
          if (cb.is_error()) {
            event.edit_original_response(dpp::message(role_problem));
            return;
          }
          std::string text = "🎰 Welcome to the casino! You now have the " +
                             std::string(kRoleName) + " role.";
          if (claim_daily(db, guild, user).empty())
            text += "\nHere are your first " + std::to_string(kDailyTokens) +
                    " piss tokens.";
          text += "\nBalance: " + tokens(db.token_balance(guild, user)) +
                  "\nUse `/casino rules` to see how the games pay.";
          event.edit_original_response(dpp::message(text));
        });
  };

  uint64_t role = configured_role(db, guild);
  if (role != 0 && dpp::find_role(role)) {
    give_role(role);
    return;
  }

  dpp::role fresh;
  fresh.set_guild_id(guild).set_name(kRoleName);
  bot.role_create(fresh, [&db, event, guild, give_role,
                          role_problem](const dpp::confirmation_callback_t &cb) {
    if (cb.is_error()) {
      event.edit_original_response(dpp::message(role_problem));
      return;
    }
    uint64_t role_id = std::get<dpp::role>(cb.value).id;
    db.kv_set(guild, "casino_role", str(role_id));
    give_role(role_id);
  });
}

void opt_out(Ctx &ctx, const dpp::slashcommand_t &event) {
  if (!has_gambler_role(ctx.db, event.command)) {
    event.reply(ephemeral_text("You aren't opted in."));
    return;
  }
  event.thinking(true);
  ctx.bot.guild_member_remove_role(
      event.command.guild_id, event.command.usr.id,
      configured_role(ctx.db, event.command.guild_id),
      [event](const dpp::confirmation_callback_t &cb) {
        event.edit_original_response(dpp::message(
            cb.is_error()
                ? "I couldn't remove the role. An admin needs to check that my "
                  "role sits above it."
                : "You left the casino. Your piss tokens are kept if you come "
                  "back with `/casino opt-in`."));
      });
}

class CasinoModule : public Module {
public:
  std::string id() const override { return "casino"; }
  std::string name() const override { return "Casino"; }
  std::string description() const override {
    return "Opt-in blackjack, roulette and slots played with piss tokens";
  }

  std::vector<dpp::slashcommand> commands(dpp::snowflake a) const override {
    auto bet_option = [] {
      return dpp::command_option(dpp::co_integer, "bet",
                                 "How many piss tokens to bet", true)
          .set_min_value(kMinBet)
          .set_max_value(kMaxBet);
    };
    std::vector<dpp::slashcommand> v;

    dpp::slashcommand bj("blackjack", "Play blackjack against the dealer", a);
    bj.add_option(bet_option());
    v.push_back(bj);

    dpp::slashcommand rl("roulette", "Bet on the roulette wheel", a);
    rl.add_option(bet_option());
    dpp::command_option choice(dpp::co_string, "choice", "What to bet on", true);
    const std::pair<const char *, const char *> kChoices[] = {
        {"Red (x2)", "red"},        {"Black (x2)", "black"},
        {"Even (x2)", "even"},      {"Odd (x2)", "odd"},
        {"Low 1-18 (x2)", "low"},   {"High 19-36 (x2)", "high"},
        {"1st dozen 1-12 (x3)", "dozen1"},
        {"2nd dozen 13-24 (x3)", "dozen2"},
        {"3rd dozen 25-36 (x3)", "dozen3"},
        {"A single number (x30)", "number"}};
    for (const auto &[label, value] : kChoices)
      choice.add_choice(dpp::command_option_choice(label, std::string(value)));
    rl.add_option(choice);
    rl.add_option(dpp::command_option(dpp::co_integer, "number",
                                      "Your number, only for a single number bet",
                                      false)
                      .set_min_value(0)
                      .set_max_value(36));
    v.push_back(rl);

    dpp::slashcommand sl("slots", "Spin the slot machine", a);
    sl.add_option(bet_option());
    v.push_back(sl);

    dpp::slashcommand cs("casino",
                         "Join the casino, claim daily tokens, see the rules", a);
    cs.add_option(dpp::command_option(dpp::co_sub_command, "opt-in",
                                      "Join the casino and get the Gambler role"));
    cs.add_option(dpp::command_option(dpp::co_sub_command, "opt-out",
                                      "Leave the casino and drop the Gambler role"));
    cs.add_option(dpp::command_option(dpp::co_sub_command, "daily",
                                      "Claim your daily piss tokens"));
    cs.add_option(dpp::command_option(dpp::co_sub_command, "balance",
                                      "Show how many piss tokens you have"));
    cs.add_option(dpp::command_option(dpp::co_sub_command, "rules",
                                      "Show the casino rules and payouts"));
    dpp::command_option role(dpp::co_sub_command, "role",
                             "Choose the role members get when they opt in (Admin)");
    role.add_option(dpp::command_option(
        dpp::co_role, "role", "The role to use for the casino", true));
    cs.add_option(role);
    v.push_back(cs);
    return v;
  }

  bool on_slash(Ctx &ctx, const dpp::slashcommand_t &event) override {
    const std::string cmd = event.command.get_command_name();
    if (cmd == "casino") {
      handle_casino(ctx, event);
      return true;
    }
    if (cmd != "blackjack" && cmd != "roulette" && cmd != "slots")
      return false;

    if (!has_gambler_role(ctx.db, event.command)) {
      event.reply(ephemeral_text(kNeedOptIn));
      return true;
    }

    const uint64_t guild = event.command.guild_id;
    const uint64_t user = event.command.usr.id;
    int64_t bet = std::get<int64_t>(event.get_parameter("bet"));

    RouletteBet rbet;
    if (cmd == "roulette") {
      if (!parse_roulette_kind(
              std::get<std::string>(event.get_parameter("choice")), rbet.kind)) {
        event.reply(ephemeral_text("Pick something to bet on."));
        return true;
      }
      if (rbet.kind == RouletteKind::Number) {
        auto n = event.get_parameter("number");
        if (!std::holds_alternative<int64_t>(n)) {
          event.reply(ephemeral_text(
              "For a single number bet, fill in the number option (0-36)."));
          return true;
        }
        rbet.number = static_cast<int>(std::get<int64_t>(n));
      }
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    std::string problem = try_start(ctx, guild, user, bet);
    if (!problem.empty()) {
      event.reply(ephemeral_text(problem));
      return true;
    }
    const int stake = static_cast<int>(bet);

    if (cmd == "slots") {
      int a = spin_reel(rng()), b = spin_reel(rng()), c = spin_reel(rng());
      int paid = stake * slot_multiplier(a, b, c);
      credit(ctx, guild, user, paid);
      std::string body = "<@" + str(user) + ">\n\n**[ " +
                         slot_symbol(a).emoji + " | " + slot_symbol(b).emoji +
                         " | " + slot_symbol(c).emoji + " ]**\n\n" +
                         result_text(ctx, guild, user, stake, paid);
      event.reply(embed_message("🎰 Slots", body, paid > stake ? kGreen
                                                  : paid == stake ? kGrey
                                                                  : kRed));
    } else if (cmd == "roulette") {
      int pocket = std::uniform_int_distribution<int>(0, 36)(rng());
      int paid = stake * roulette_multiplier(rbet, pocket);
      credit(ctx, guild, user, paid);
      const char *dot = pocket == 0 ? "🟢" : roulette_is_red(pocket) ? "🔴" : "⚫";
      std::string body = "<@" + str(user) + "> bet on **" +
                         roulette_label(rbet) + "**\n\nThe ball lands on " +
                         dot + " **" + std::to_string(pocket) + "**\n\n" +
                         result_text(ctx, guild, user, stake, paid);
      event.reply(embed_message("🎡 Roulette", body, paid > 0 ? kGreen : kRed));
    } else {
      BlackjackGame g = blackjack_deal(stake, now_s(), rng());
      if (blackjack_finished_on_deal(g)) {
        event.reply(finish_hand(ctx, guild, user, g, ""));
      } else {
        save_hand(ctx, guild, user, g);
        event.reply(hand_view(g, user, false, "Hit, stand or double down?",
                              kBlue, ctx.db.token_balance(guild, user) >= stake));
      }
    }
    return true;
  }

  bool on_button(Ctx &ctx, const dpp::button_click_t &event) override {
    const std::string &id = event.custom_id;
    if (id.rfind("bj_", 0) != 0)
      return false;
    size_t colon = id.find(':');
    if (colon == std::string::npos)
      return false;
    const std::string action = id.substr(3, colon - 3);
    uint64_t owner = 0;
    try {
      owner = std::stoull(id.substr(colon + 1));
    } catch (...) {
      return false;
    }

    if (event.command.usr.id != owner) {
      event.reply(ephemeral_text("This isn't your hand. Start one with /blackjack."));
      return true;
    }

    const uint64_t guild = event.command.guild_id;
    std::lock_guard<std::mutex> lock(g_mutex);

    BlackjackGame g;
    if (!load_hand(ctx, guild, owner, g)) {
      event.reply(ephemeral_text("This hand is already over."));
      return true;
    }
    if (now_s() - g.started >= kHandTimeoutSeconds) {
      event.reply(dpp::ir_update_message,
                  finish_hand(ctx, guild, owner, g,
                              "⏱️ The hand sat idle, so you stood."));
      return true;
    }

    if (action == "stand") {
      event.reply(dpp::ir_update_message, finish_hand(ctx, guild, owner, g, ""));
    } else if (action == "hit") {
      blackjack_hit(g);
      if (blackjack_player_bust(g) || hand_value(g.player).total == 21) {
        event.reply(dpp::ir_update_message,
                    finish_hand(ctx, guild, owner, g, ""));
      } else {
        save_hand(ctx, guild, owner, g);
        event.reply(dpp::ir_update_message,
                    hand_view(g, owner, false, "Hit or stand?", kBlue, false));
      }
    } else if (action == "double") {
      if (g.player.size() != 2 || g.doubled) {
        event.reply(ephemeral_text("You can only double down on your first move."));
        return true;
      }
      if (!ctx.db.token_spend(guild, owner, g.bet)) {
        event.reply(ephemeral_text(
            "You don't have enough piss tokens to double down. You need " +
            std::to_string(g.bet) + " more."));
        return true;
      }
      blackjack_double(g);
      event.reply(dpp::ir_update_message, finish_hand(ctx, guild, owner, g, ""));
    } else {
      return false;
    }
    return true;
  }

private:
  void handle_casino(Ctx &ctx, const dpp::slashcommand_t &event) {
    const auto &data = event.command.get_command_interaction();
    const std::string sub = data.options.empty() ? "" : data.options[0].name;
    const uint64_t guild = event.command.guild_id;
    const uint64_t user = event.command.usr.id;

    if (sub == "opt-in") {
      opt_in(ctx, event);
    } else if (sub == "opt-out") {
      opt_out(ctx, event);
    } else if (sub == "daily") {
      if (!has_gambler_role(ctx.db, event.command)) {
        event.reply(ephemeral_text(kNeedOptIn));
        return;
      }
      std::string problem = claim_daily(ctx.db, guild, user);
      event.reply(ephemeral_text(
          problem.empty()
              ? "🪙 You claimed " + std::to_string(kDailyTokens) +
                    " piss tokens. Balance: " +
                    tokens(ctx.db.token_balance(guild, user))
              : problem));
    } else if (sub == "balance") {
      event.reply(ephemeral_text("Balance: " +
                                 tokens(ctx.db.token_balance(guild, user))));
    } else if (sub == "rules") {
      event.reply(ephemeral_text(rules_text()));
    } else if (sub == "role") {
      if (!is_admin(event.command)) {
        event.reply(no_permission_message());
        return;
      }
      uint64_t role_id = 0;
      for (const auto &o : data.options[0].options)
        if (o.name == "role")
          role_id = std::get<dpp::snowflake>(o.value);
      if (role_id == 0) {
        event.reply(ephemeral_text("Pick a role."));
        return;
      }
      ctx.db.kv_set(guild, "casino_role", str(role_id));
      event.reply(ephemeral_text("✅ Members who opt in will get <@&" +
                                 str(role_id) +
                                 ">. My role must sit above it in the role list."));
    } else {
      event.reply(ephemeral_text("Unknown casino command."));
    }
  }
};

} // namespace

std::unique_ptr<Module> make_casino_module() {
  return std::make_unique<CasinoModule>();
}
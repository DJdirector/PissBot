#include "template.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <chrono>
#include <random>
#include <sstream>

namespace {

struct State {
  int calls = 0;
};

const int kMaxDepth = 8;
const int kMaxCalls = 100;

const std::set<std::string> &builtin_names() {
  static const std::set<std::string> names = {
      "user",   "username", "userid",   "channel",   "channelid", "server",
      "query",  "args",     "querystring", "touser", "count",     "choose",
      "random", "rand",     "math",     "upper",     "lower",     "time",
      "date",   "countdown", "countup"};
  return names;
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool is_number_name(const std::string &s) {
  return !s.empty() && s.size() <= 3 &&
         std::all_of(s.begin(), s.end(),
                     [](unsigned char c) { return std::isdigit(c); });
}

std::string url_encode(const std::string &in) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : in) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

std::vector<std::string> split_words(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream ss(s);
  std::string w;
  while (ss >> w)
    out.push_back(w);
  return out;
}

std::mt19937_64 &rng() {
  thread_local std::mt19937_64 gen{std::random_device{}()};
  return gen;
}

// ---- math ----
struct MathParser {
  const std::string &s;
  size_t i = 0;
  bool ok = true;
  int depth = 0;
  explicit MathParser(const std::string &str) : s(str) {}

  void ws() {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
      i++;
  }
  double expr() {
    double v = term();
    for (;;) {
      ws();
      if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        char op = s[i++];
        double r = term();
        v = op == '+' ? v + r : v - r;
      } else
        return v;
    }
  }
  double term() {
    double v = power();
    for (;;) {
      ws();
      if (i < s.size() && (s[i] == '*' || s[i] == '/' || s[i] == '%')) {
        char op = s[i++];
        double r = power();
        if (op == '*')
          v *= r;
        else if (r == 0) {
          ok = false;
          return 0;
        } else if (op == '/')
          v /= r;
        else
          v = std::fmod(v, r);
      } else
        return v;
    }
  }
  double power() {
    double base = unary();
    ws();
    if (i < s.size() && s[i] == '^') {
      i++;
      double e = power();
      double r = std::pow(base, e);
      if (!std::isfinite(r))
        ok = false;
      return r;
    }
    return base;
  }
  double unary() {
    ws();
    if (i < s.size() && s[i] == '-') {
      i++;
      return -unary();
    }
    if (i < s.size() && s[i] == '+') {
      i++;
      return unary();
    }
    return primary();
  }
  double primary() {
    ws();
    if (++depth > 50) {
      ok = false;
      return 0;
    }
    double v = 0;
    if (i < s.size() && s[i] == '(') {
      i++;
      v = expr();
      ws();
      if (i < s.size() && s[i] == ')')
        i++;
      else
        ok = false;
    } else {
      size_t start = i;
      while (i < s.size() &&
             (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.'))
        i++;
      if (start == i) {
        ok = false;
        return 0;
      }
      try {
        v = std::stod(s.substr(start, i - start));
      } catch (...) {
        ok = false;
      }
    }
    depth--;
    return v;
  }
};

std::string format_number(double v) {
  if (std::fabs(v - std::round(v)) < 1e-9 && std::fabs(v) < 1e15) {
    return std::to_string(static_cast<long long>(std::llround(v)));
  }
  std::ostringstream ss;
  ss.precision(6);
  ss << std::fixed << v;
  std::string r = ss.str();
  while (!r.empty() && r.back() == '0')
    r.pop_back();
  if (!r.empty() && r.back() == '.')
    r.pop_back();
  return r;
}

bool parse_date(const std::string &in, time_t &out) {
  int y, mo, d, h = 0, mi = 0;
  std::string t = in;
  std::replace(t.begin(), t.end(), 'T', ' ');
  int n = std::sscanf(t.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi);
  if (n < 3 || y < 1970 || y > 2200 || mo < 1 || mo > 12 || d < 1 || d > 31)
    return false;
  std::tm tm = {};
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  out = timegm(&tm);
  return true;
}

std::string expand(const std::string &s, const TemplateContext &ctx,
                   State &st, int depth);

// Splits on '|' that is not escaped and not inside nested $( ).
std::vector<std::string> split_choices(const std::string &s) {
  std::vector<std::string> parts;
  std::string cur;
  int d = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '\\' && i + 1 < s.size()) {
      cur += c;
      cur += s[++i];
      continue;
    }
    if (c == '(')
      d++;
    else if (c == ')' && d > 0)
      d--;
    if (c == '|' && d == 0) {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  parts.push_back(cur);
  return parts;
}

bool call_function(const std::string &inner, const TemplateContext &ctx,
                   State &st, int depth, std::string &out) {
  size_t sp = inner.find_first_of(" \t\n");
  std::string name = lower(inner.substr(0, sp));
  std::string rest = sp == std::string::npos ? "" : inner.substr(sp + 1);

  auto var = ctx.vars.find(name);
  if (var != ctx.vars.end()) {
    out = var->second;
    return true;
  }

  if (++st.calls > kMaxCalls)
    return false;

  if (name == "choose" || name == "random") {
    auto parts = split_choices(rest);
    if (parts.empty())
      return false;
    std::uniform_int_distribution<size_t> pick(0, parts.size() - 1);
    std::string chosen = parts[pick(rng())];
    // trim
    size_t a = chosen.find_first_not_of(" \t\n");
    size_t b = chosen.find_last_not_of(" \t\n");
    chosen = a == std::string::npos ? "" : chosen.substr(a, b - a + 1);
    out = expand(chosen, ctx, st, depth + 1);
    return true;
  }

  std::string args = expand(rest, ctx, st, depth + 1);

  if (name == "user") {
    out = ctx.user_id.empty() ? ctx.user_name : "<@" + ctx.user_id + ">";
  } else if (name == "username") {
    out = ctx.user_name;
  } else if (name == "userid") {
    out = ctx.user_id;
  } else if (name == "channel") {
    out = ctx.channel_id.empty() ? "" : "<#" + ctx.channel_id + ">";
  } else if (name == "channelid") {
    out = ctx.channel_id;
  } else if (name == "server") {
    out = ctx.server_name;
  } else if (name == "query" || name == "args") {
    out = ctx.args;
  } else if (name == "querystring") {
    out = url_encode(ctx.args);
  } else if (name == "touser") {
    auto words = split_words(ctx.args);
    out = words.empty() ? (ctx.user_id.empty() ? ctx.user_name
                                                : "<@" + ctx.user_id + ">")
                        : words[0];
  } else if (is_number_name(name)) {
    auto words = split_words(ctx.args);
    size_t n = std::stoul(name);
    out = (n >= 1 && n <= words.size()) ? words[n - 1] : "";
  } else if (name == "count") {
    if (!ctx.next_count)
      return false;
    out = std::to_string(ctx.next_count());
  } else if (name == "rand") {
    auto w = split_words(args);
    if (w.size() != 2)
      return false;
    try {
      long long lo = std::stoll(w[0]), hi = std::stoll(w[1]);
      if (lo > hi)
        std::swap(lo, hi);
      if (hi - lo > 1000000000LL)
        return false;
      std::uniform_int_distribution<long long> d(lo, hi);
      out = std::to_string(d(rng()));
    } catch (...) {
      return false;
    }
  } else if (name == "math") {
    if (args.size() > 200)
      return false;
    MathParser p(args);
    double v = p.expr();
    p.ws();
    if (!p.ok || p.i != args.size() || !std::isfinite(v))
      return false;
    out = format_number(v);
  } else if (name == "upper") {
    out = args;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return c < 128 ? std::toupper(c) : c; });
  } else if (name == "lower") {
    out = lower(args);
  } else if (name == "time") {
    out = "<t:" + std::to_string(static_cast<long long>(std::time(nullptr))) +
          ":t>";
  } else if (name == "date") {
    out = "<t:" + std::to_string(static_cast<long long>(std::time(nullptr))) +
          ":D>";
  } else if (name == "countdown" || name == "countup") {
    time_t t;
    if (!parse_date(args, t))
      return false;
    out = "<t:" + std::to_string(static_cast<long long>(t)) + ":R>";
  } else {
    return false;
  }
  return true;
}

std::string expand(const std::string &s, const TemplateContext &ctx,
                   State &st, int depth) {
  if (depth > kMaxDepth)
    return s;
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    char c = s[i];
    if (c == '\\' && i + 1 < s.size() &&
        (s[i + 1] == '$' || s[i + 1] == '(' || s[i + 1] == ')' ||
         s[i + 1] == '|' || s[i + 1] == '\\')) {
      out += s[i + 1];
      i += 2;
      continue;
    }
    if (c == '$' && i + 1 < s.size() && s[i + 1] == '(') {
      int d = 0;
      size_t close = std::string::npos;
      for (size_t j = i + 1; j < s.size(); ++j) {
        if (s[j] == '\\') {
          j++;
          continue;
        }
        if (s[j] == '(')
          d++;
        else if (s[j] == ')' && --d == 0) {
          close = j;
          break;
        }
      }
      if (close == std::string::npos) {
        out += s.substr(i);
        break;
      }
      std::string inner = s.substr(i + 2, close - (i + 2));
      std::string repl;
      if (call_function(inner, ctx, st, depth, repl))
        out += repl;
      else
        out += s.substr(i, close - i + 1);
      i = close + 1;
      continue;
    }
    out += c;
    i++;
  }
  return out;
}

// {user} -> $(user) for names we know, so old saved messages keep working.
std::string upgrade_legacy(const std::string &s, const TemplateContext &ctx) {
  std::set<std::string> names = builtin_names();
  for (const auto &kv : ctx.vars)
    names.insert(kv.first);
  std::string out;
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '{') {
      size_t close = s.find('}', i + 1);
      if (close != std::string::npos && close - i <= 24) {
        std::string name = lower(s.substr(i + 1, close - i - 1));
        if (names.count(name)) {
          out += "$(" + name + ")";
          i = close + 1;
          continue;
        }
      }
    }
    out += s[i++];
  }
  return out;
}

} // namespace

double eval_math(const std::string &expr, bool &ok) {
  MathParser p(expr);
  double v = p.expr();
  p.ws();
  ok = p.ok && p.i == expr.size() && std::isfinite(v);
  return ok ? v : 0;
}

std::string render_template(const std::string &tmpl,
                            const TemplateContext &ctx, size_t max_len) {
  State st;
  std::string out = expand(upgrade_legacy(tmpl, ctx), ctx, st, 0);
  if (out.size() > max_len) {
    size_t cut = max_len;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80)
      cut--;
    out.resize(cut);
  }
  return out;
}

std::string validate_template(const std::string &tmpl,
                              const std::set<std::string> &extra_known) {
  std::set<std::string> bad;
  size_t i = 0;
  while ((i = tmpl.find("$(", i)) != std::string::npos) {
    if (i > 0 && tmpl[i - 1] == '\\') {
      i += 2;
      continue;
    }
    size_t j = i + 2;
    while (j < tmpl.size() &&
           (std::isalnum(static_cast<unsigned char>(tmpl[j])) || tmpl[j] == '_'))
      j++;
    std::string name = lower(tmpl.substr(i + 2, j - (i + 2)));
    if (!name.empty() && !builtin_names().count(name) &&
        !extra_known.count(name) && !is_number_name(name))
      bad.insert(name);
    if (name.empty())
      bad.insert("");
    i = j;
  }
  if (bad.empty())
    return "";
  std::string list;
  for (const auto &b : bad)
    list += (list.empty() ? "" : ", ") + std::string("$(") + b + ")";
  std::string msg = "Unknown variable(s): " + list + ".";
  if (bad.count("eval"))
    msg += " $(eval) (JavaScript) is not supported. Use $(choose a|b|c), "
           "$(rand 1 100) or $(math 2+2) instead.";
  return msg;
}

std::string template_help() {
  return "$(user) $(username) $(channel) $(server) $(query) $(1) $(touser) "
         "$(count) $(choose a|b|c) $(rand 1 100) $(math 2+2) $(upper x) "
         "$(lower x) $(time) $(date) $(countdown 2026-12-25)";
}

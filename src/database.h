#ifndef DATABASE_H
#define DATABASE_H

#include <cstdint>
#include <sqlite3.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct YouTubeSub {
  uint64_t guild_id;
  std::string yt_channel_id;
  std::string video_message;
  std::string stream_message;
  std::string ping_role_id;
  bool ignore_shorts;
  bool use_embed;
};

struct WarningEntry {
  int64_t id;
  uint64_t mod_id;
  std::string reason;
  int64_t created_at;
};

struct Answer {
  std::string trigger;
  std::string response;
  bool contains; // false = whole message only, true = anywhere in message
};

struct LevelRole {
  int level;
  uint64_t role_id;
};

class Database {
public:
  explicit Database(const std::string &db_path);
  ~Database();

  bool init();

  // Guild Configuration
  bool set_notification_channel(uint64_t guild_id, uint64_t channel_id);
  uint64_t get_notification_channel(uint64_t guild_id);

  // YouTube Subscriptions
  bool add_youtube_channel(const YouTubeSub &sub);
  bool remove_youtube_channel(uint64_t guild_id,
                              const std::string &yt_channel_id);
  std::vector<YouTubeSub> get_guild_youtube_subscriptions(uint64_t guild_id);
  std::vector<YouTubeSub> get_all_youtube_subscriptions();

  // YouTube Video Cache
  std::string get_last_posted_video(const std::string &yt_channel_id);
  bool set_last_posted_video(const std::string &yt_channel_id,
                             const std::string &video_id);

  // XP System
  bool add_xp(uint64_t guild_id, uint64_t user_id, int amount);
  bool remove_xp(uint64_t guild_id, uint64_t user_id, int amount);
  // Takes `amount` XP only if the user has at least that much (all or
  // nothing, checked atomically). Returns false when they can't afford it.
  bool spend_xp(uint64_t guild_id, uint64_t user_id, int amount);
  int get_xp(uint64_t guild_id, uint64_t user_id);
  std::vector<std::pair<uint64_t, int>> get_top_xp(uint64_t guild_id,
                                                   int limit = 10);

  bool set_xp_leaderboard_channel(uint64_t guild_id, uint64_t channel_id);
  uint64_t get_xp_leaderboard_channel(uint64_t guild_id);

  bool set_last_leaderboard_message_id(uint64_t guild_id, uint64_t message_id);
  uint64_t get_last_leaderboard_message_id(uint64_t guild_id);

  int get_rank(uint64_t guild_id, uint64_t user_id);
  std::vector<uint64_t> get_leaderboard_guilds();

  // Level Role Rewards
  bool add_level_role(uint64_t guild_id, int level, uint64_t role_id);
  bool remove_level_role(uint64_t guild_id, int level);
  std::vector<LevelRole> get_level_roles(uint64_t guild_id);

  // Custom Commands
  bool set_custom_command(uint64_t guild_id, const std::string &name,
                          const std::string &response);
  bool remove_custom_command(uint64_t guild_id, const std::string &name);
  std::string get_custom_command(uint64_t guild_id, const std::string &name);
  std::vector<std::string> list_custom_commands(uint64_t guild_id);

  // Role-based command permissions
  bool set_permission_roles(uint64_t guild_id, const std::string &feature,
                            const std::vector<uint64_t> &roles);
  std::vector<uint64_t> get_permission_roles(uint64_t guild_id,
                                             const std::string &feature);

  // Answers (auto-replies to plain messages)
  bool set_answer(uint64_t guild_id, const std::string &trigger,
                  const std::string &response, bool contains);
  bool remove_answer(uint64_t guild_id, const std::string &trigger);
  std::vector<Answer> get_answers(uint64_t guild_id);

  // Modules enabled per guild (presence of a row = enabled)
  std::set<std::string> get_enabled_modules(uint64_t guild_id);
  bool set_enabled_modules(uint64_t guild_id,
                           const std::vector<std::string> &modules);

  // Small per-guild settings store for modules ("" = unset)
  std::string kv_get(uint64_t guild_id, const std::string &key);
  bool kv_set(uint64_t guild_id, const std::string &key,
              const std::string &value);

  // Auto-incrementing counters ($(count))
  long long incr_counter(uint64_t guild_id, const std::string &key);

  // Moderation warnings
  int add_warning(uint64_t guild_id, uint64_t user_id, uint64_t mod_id,
                  const std::string &reason); // returns the new total
  std::vector<WarningEntry> get_warnings(uint64_t guild_id, uint64_t user_id);
  int clear_warnings(uint64_t guild_id, uint64_t user_id);

private:
  sqlite3 *db;
  std::string path;

  bool execute_query(const std::string &query);
  void run_migrations();
};

#endif // DATABASE_H
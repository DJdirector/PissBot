#include "database.h"
#include <iostream>

Database::Database(const std::string &db_path) : db(nullptr), path(db_path) {}

Database::~Database() {
  if (db) {
    sqlite3_close(db);
  }
}

bool Database::execute_query(const std::string &query) {
  char *err_msg = nullptr;
  int rc = sqlite3_exec(db, query.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQLite Error: " << (err_msg ? err_msg : "Unknown error")
              << std::endl;
    sqlite3_free(err_msg);
    return false;
  }
  return true;
}

bool Database::init() {
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  std::string create_guilds = "CREATE TABLE IF NOT EXISTS guild_settings ("
                              "guild_id INTEGER PRIMARY KEY, "
                              "notification_channel_id INTEGER DEFAULT 0, "
                              "xp_leaderboard_channel_id INTEGER DEFAULT 0, "
                              "xp_last_message_id INTEGER DEFAULT 0"
                              ");";

  std::string create_youtube = "CREATE TABLE IF NOT EXISTS youtube_subs ("
                               "guild_id INTEGER, "
                               "yt_channel_id TEXT, "
                               "video_message TEXT, "
                               "stream_message TEXT, "
                               "ping_role_id TEXT, "
                               "ignore_shorts INTEGER DEFAULT 0, "
                               "use_embed INTEGER DEFAULT 0, "
                               "PRIMARY KEY (guild_id, yt_channel_id)"
                               ");";

  std::string create_cache = "CREATE TABLE IF NOT EXISTS youtube_cache ("
                             "yt_channel_id TEXT PRIMARY KEY, "
                             "last_video_id TEXT"
                             ");";

  std::string create_xp = "CREATE TABLE IF NOT EXISTS user_xp ("
                          "guild_id INTEGER, "
                          "user_id INTEGER, "
                          "xp INTEGER DEFAULT 0, "
                          "PRIMARY KEY (guild_id, user_id)"
                          ");";

  std::string create_level_roles =
      "CREATE TABLE IF NOT EXISTS level_roles ("
      "guild_id INTEGER, "
      "level INTEGER, "
      "role_id INTEGER, "
      "PRIMARY KEY (guild_id, level)"
      ");";

  std::string create_custom_cmds =
      "CREATE TABLE IF NOT EXISTS custom_commands ("
      "guild_id INTEGER, "
      "name TEXT, "
      "response TEXT, "
      "PRIMARY KEY (guild_id, name)"
      ");";

  std::string create_perms =
      "CREATE TABLE IF NOT EXISTS command_permissions ("
      "guild_id INTEGER, "
      "feature TEXT, "
      "role_id INTEGER, "
      "PRIMARY KEY (guild_id, feature, role_id)"
      ");";

  std::string create_answers =
      "CREATE TABLE IF NOT EXISTS answers ("
      "guild_id INTEGER, "
      "trigger_text TEXT, "
      "response TEXT, "
      "match_contains INTEGER DEFAULT 0, "
      "PRIMARY KEY (guild_id, trigger_text)"
      ");";

  std::string create_modules =
      "CREATE TABLE IF NOT EXISTS guild_modules ("
      "guild_id INTEGER, module_id TEXT, PRIMARY KEY (guild_id, module_id));";
  std::string create_kv =
      "CREATE TABLE IF NOT EXISTS guild_kv ("
      "guild_id INTEGER, key TEXT, value TEXT, PRIMARY KEY (guild_id, key));";
  std::string create_counters =
      "CREATE TABLE IF NOT EXISTS counters ("
      "guild_id INTEGER, key TEXT, value INTEGER DEFAULT 0, "
      "PRIMARY KEY (guild_id, key));";
  std::string create_warnings =
      "CREATE TABLE IF NOT EXISTS warnings ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, guild_id INTEGER, "
      "user_id INTEGER, mod_id INTEGER, reason TEXT, "
      "created_at INTEGER DEFAULT (strftime('%s','now')));";

  if (!execute_query(create_guilds) || !execute_query(create_youtube) ||
      !execute_query(create_cache) || !execute_query(create_xp) ||
      !execute_query(create_level_roles) ||
      !execute_query(create_custom_cmds) || !execute_query(create_perms) ||
      !execute_query(create_answers) || !execute_query(create_modules) ||
      !execute_query(create_kv) || !execute_query(create_counters) ||
      !execute_query(create_warnings)) {
    return false;
  }

  run_migrations();
  return true;
}

bool table_has_column(sqlite3 *db, const std::string &table,
                      const std::string &column) {
  std::string sql = "PRAGMA table_info(" + table + ");";
  sqlite3_stmt *stmt;
  bool exists = false;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string col_name =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      if (col_name == column) {
        exists = true;
        break;
      }
    }
  }
  sqlite3_finalize(stmt);
  return exists;
}

void Database::run_migrations() {
  if (!table_has_column(db, "youtube_subs", "ignore_shorts")) {
    execute_query(
        "ALTER TABLE youtube_subs ADD COLUMN ignore_shorts INTEGER DEFAULT 0;");
  }
  if (!table_has_column(db, "youtube_subs", "use_embed")) {
    execute_query(
        "ALTER TABLE youtube_subs ADD COLUMN use_embed INTEGER DEFAULT 0;");
  }
  if (!table_has_column(db, "guild_settings", "xp_leaderboard_channel_id")) {
    execute_query("ALTER TABLE guild_settings ADD COLUMN "
                  "xp_leaderboard_channel_id INTEGER DEFAULT 0;");
  }
  if (!table_has_column(db, "guild_settings", "xp_last_message_id")) {
    execute_query("ALTER TABLE guild_settings ADD COLUMN xp_last_message_id "
                  "INTEGER DEFAULT 0;");
  }

  // Messages saved before the $(...) syntax used {user}-style placeholders.
  // They still render, but rewrite them so dashboards show the new syntax.
  // Safe to repeat: once converted there is nothing left to replace.
  execute_query(
      "UPDATE youtube_subs SET "
      "video_message = replace(replace(replace(video_message, '{role}', "
      "'$(role)'), '{url}', '$(url)'), '{channel}', '$(channel)'), "
      "stream_message = replace(replace(replace(stream_message, '{role}', "
      "'$(role)'), '{url}', '$(url)'), '{channel}', '$(channel)');");
  execute_query(
      "UPDATE custom_commands SET response = replace(replace(replace("
      "replace(response, '{user}', '$(user)'), '{username}', '$(username)'), "
      "'{args}', '$(args)'), '{channel}', '$(channel)');");
  execute_query(
      "UPDATE answers SET response = replace(replace(replace(response, "
      "'{user}', '$(user)'), '{username}', '$(username)'), '{channel}', "
      "'$(channel)');");
}

bool Database::set_notification_channel(uint64_t guild_id,
                                        uint64_t channel_id) {
  std::string sql =
      "INSERT INTO guild_settings (guild_id, notification_channel_id) VALUES "
      "(" +
      std::to_string(guild_id) + ", " + std::to_string(channel_id) + ") " +
      "ON CONFLICT(guild_id) DO UPDATE SET notification_channel_id = " +
      std::to_string(channel_id) + ";";
  return execute_query(sql);
}

uint64_t Database::get_notification_channel(uint64_t guild_id) {
  std::string sql =
      "SELECT notification_channel_id FROM guild_settings WHERE guild_id = " +
      std::to_string(guild_id) + ";";
  sqlite3_stmt *stmt;
  uint64_t channel_id = 0;

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      channel_id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
  }
  sqlite3_finalize(stmt);
  return channel_id;
}

bool Database::set_xp_leaderboard_channel(uint64_t guild_id,
                                          uint64_t channel_id) {
  std::string sql =
      "INSERT INTO guild_settings (guild_id, xp_leaderboard_channel_id) VALUES "
      "(" +
      std::to_string(guild_id) + ", " + std::to_string(channel_id) + ") " +
      "ON CONFLICT(guild_id) DO UPDATE SET xp_leaderboard_channel_id = " +
      std::to_string(channel_id) + ";";
  return execute_query(sql);
}

uint64_t Database::get_xp_leaderboard_channel(uint64_t guild_id) {
  std::string sql =
      "SELECT xp_leaderboard_channel_id FROM guild_settings WHERE guild_id = " +
      std::to_string(guild_id) + ";";
  sqlite3_stmt *stmt;
  uint64_t channel_id = 0;

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      channel_id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
  }
  sqlite3_finalize(stmt);
  return channel_id;
}

bool Database::set_last_leaderboard_message_id(uint64_t guild_id,
                                               uint64_t message_id) {
  std::string sql = "UPDATE guild_settings SET xp_last_message_id = " +
                    std::to_string(message_id) +
                    " WHERE guild_id = " + std::to_string(guild_id) + ";";
  return execute_query(sql);
}

uint64_t Database::get_last_leaderboard_message_id(uint64_t guild_id) {
  std::string sql =
      "SELECT xp_last_message_id FROM guild_settings WHERE guild_id = " +
      std::to_string(guild_id) + ";";
  sqlite3_stmt *stmt;
  uint64_t message_id = 0;

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      message_id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
  }
  sqlite3_finalize(stmt);
  return message_id;
}

bool Database::add_youtube_channel(const YouTubeSub &sub) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO youtube_subs (guild_id, yt_channel_id, video_message, "
      "stream_message, ping_role_id, ignore_shorts, use_embed) "
      "VALUES (?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(guild_id, yt_channel_id) DO UPDATE SET "
      "video_message = excluded.video_message, stream_message = "
      "excluded.stream_message, "
      "ping_role_id = excluded.ping_role_id, ignore_shorts = "
      "excluded.ignore_shorts, use_embed = excluded.use_embed;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int64(stmt, 1, sub.guild_id);
  sqlite3_bind_text(stmt, 2, sub.yt_channel_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, sub.video_message.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, sub.stream_message.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, sub.ping_role_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, sub.ignore_shorts ? 1 : 0);
  sqlite3_bind_int(stmt, 7, sub.use_embed ? 1 : 0);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::remove_youtube_channel(uint64_t guild_id,
                                      const std::string &yt_channel_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "DELETE FROM youtube_subs WHERE guild_id = ? AND yt_channel_id = ?;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, yt_channel_id.c_str(), -1, SQLITE_TRANSIENT);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

std::vector<YouTubeSub>
Database::get_guild_youtube_subscriptions(uint64_t guild_id) {
  std::vector<YouTubeSub> list;
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT yt_channel_id, video_message, stream_message, ping_role_id, "
      "ignore_shorts, use_embed FROM youtube_subs WHERE guild_id = ?;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      YouTubeSub sub;
      sub.guild_id = guild_id;
      sub.yt_channel_id =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      sub.video_message =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      sub.stream_message =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));

      const unsigned char *role = sqlite3_column_text(stmt, 3);
      sub.ping_role_id = role ? reinterpret_cast<const char *>(role) : "";

      sub.ignore_shorts = sqlite3_column_int(stmt, 4) != 0;
      sub.use_embed = sqlite3_column_int(stmt, 5) != 0;
      list.push_back(sub);
    }
  }
  sqlite3_finalize(stmt);
  return list;
}

std::vector<YouTubeSub> Database::get_all_youtube_subscriptions() {
  std::vector<YouTubeSub> list;
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT guild_id, yt_channel_id, video_message, stream_message, "
      "ping_role_id, ignore_shorts, use_embed FROM youtube_subs;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      YouTubeSub sub;
      sub.guild_id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
      sub.yt_channel_id =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      sub.video_message =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      sub.stream_message =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

      const unsigned char *role = sqlite3_column_text(stmt, 4);
      sub.ping_role_id = role ? reinterpret_cast<const char *>(role) : "";

      sub.ignore_shorts = sqlite3_column_int(stmt, 5) != 0;
      sub.use_embed = sqlite3_column_int(stmt, 6) != 0;
      list.push_back(sub);
    }
  }
  sqlite3_finalize(stmt);
  return list;
}

std::string Database::get_last_posted_video(const std::string &yt_channel_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT last_video_id FROM youtube_cache WHERE yt_channel_id = ?;";
  std::string last_id = "";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, yt_channel_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      last_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    }
  }
  sqlite3_finalize(stmt);
  return last_id;
}

bool Database::set_last_posted_video(const std::string &yt_channel_id,
                                     const std::string &video_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO youtube_cache (yt_channel_id, last_video_id) VALUES (?, ?) "
      "ON CONFLICT(yt_channel_id) DO UPDATE SET last_video_id = "
      "excluded.last_video_id;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, yt_channel_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, video_id.c_str(), -1, SQLITE_TRANSIENT);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::add_xp(uint64_t guild_id, uint64_t user_id, int amount) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO user_xp (guild_id, user_id, xp) VALUES (?, ?, ?) "
      "ON CONFLICT(guild_id, user_id) DO UPDATE SET xp = xp + excluded.xp;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_int(stmt, 3, amount);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::remove_xp(uint64_t guild_id, uint64_t user_id, int amount) {
  sqlite3_stmt *stmt;
  std::string sql = "UPDATE user_xp SET xp = MAX(0, xp - ?) WHERE guild_id = ? "
                    "AND user_id = ?;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int(stmt, 1, amount);
  sqlite3_bind_int64(stmt, 2, guild_id);
  sqlite3_bind_int64(stmt, 3, user_id);

  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

int Database::get_xp(uint64_t guild_id, uint64_t user_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT xp FROM user_xp WHERE guild_id = ? AND user_id = ?;";
  int current_xp = 0;

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      current_xp = sqlite3_column_int(stmt, 0);
    }
  }
  sqlite3_finalize(stmt);
  return current_xp;
}

std::vector<std::pair<uint64_t, int>> Database::get_top_xp(uint64_t guild_id,
                                                           int limit) {
  std::vector<std::pair<uint64_t, int>> leaderboard;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT user_id, xp FROM user_xp WHERE guild_id = ? ORDER "
                    "BY xp DESC LIMIT ?;";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      uint64_t user_id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
      int xp = sqlite3_column_int(stmt, 1);
      leaderboard.emplace_back(user_id, xp);
    }
  }
  sqlite3_finalize(stmt);
  return leaderboard;
}

int Database::get_rank(uint64_t guild_id, uint64_t user_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT COUNT(*) + 1 FROM user_xp WHERE guild_id = ? AND xp > "
      "(SELECT COALESCE((SELECT xp FROM user_xp WHERE guild_id = ? AND "
      "user_id = ?), 0));";
  int rank = 0;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, guild_id);
    sqlite3_bind_int64(stmt, 3, user_id);
    if (sqlite3_step(stmt) == SQLITE_ROW)
      rank = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return rank;
}

std::vector<uint64_t> Database::get_leaderboard_guilds() {
  std::vector<uint64_t> guilds;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT guild_id FROM guild_settings WHERE "
                    "xp_leaderboard_channel_id != 0;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW)
      guilds.push_back(static_cast<uint64_t>(sqlite3_column_int64(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return guilds;
}

bool Database::add_level_role(uint64_t guild_id, int level, uint64_t role_id) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO level_roles (guild_id, level, role_id) VALUES (?, ?, ?) "
      "ON CONFLICT(guild_id, level) DO UPDATE SET role_id = excluded.role_id;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_int(stmt, 2, level);
  sqlite3_bind_int64(stmt, 3, role_id);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::remove_level_role(uint64_t guild_id, int level) {
  sqlite3_stmt *stmt;
  std::string sql = "DELETE FROM level_roles WHERE guild_id = ? AND level = ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_int(stmt, 2, level);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  int changed = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return success && changed > 0;
}

std::vector<LevelRole> Database::get_level_roles(uint64_t guild_id) {
  std::vector<LevelRole> roles;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT level, role_id FROM level_roles WHERE guild_id = ? "
                    "ORDER BY level ASC;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      roles.push_back({sqlite3_column_int(stmt, 0),
                       static_cast<uint64_t>(sqlite3_column_int64(stmt, 1))});
    }
  }
  sqlite3_finalize(stmt);
  return roles;
}

bool Database::set_custom_command(uint64_t guild_id, const std::string &name,
                                  const std::string &response) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO custom_commands (guild_id, name, response) VALUES (?, ?, ?) "
      "ON CONFLICT(guild_id, name) DO UPDATE SET response = excluded.response;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, response.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::remove_custom_command(uint64_t guild_id,
                                     const std::string &name) {
  sqlite3_stmt *stmt;
  std::string sql =
      "DELETE FROM custom_commands WHERE guild_id = ? AND name = ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  int changed = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return success && changed > 0;
}

std::string Database::get_custom_command(uint64_t guild_id,
                                         const std::string &name) {
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT response FROM custom_commands WHERE guild_id = ? AND name = ?;";
  std::string response;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *txt = sqlite3_column_text(stmt, 0);
      response = txt ? reinterpret_cast<const char *>(txt) : "";
    }
  }
  sqlite3_finalize(stmt);
  return response;
}

std::vector<std::string> Database::list_custom_commands(uint64_t guild_id) {
  std::vector<std::string> names;
  sqlite3_stmt *stmt;
  std::string sql =
      "SELECT name FROM custom_commands WHERE guild_id = ? ORDER BY name;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *txt = sqlite3_column_text(stmt, 0);
      if (txt)
        names.emplace_back(reinterpret_cast<const char *>(txt));
    }
  }
  sqlite3_finalize(stmt);
  return names;
}

bool Database::set_permission_roles(uint64_t guild_id,
                                    const std::string &feature,
                                    const std::vector<uint64_t> &roles) {
  if (!execute_query("BEGIN;"))
    return false;

  bool ok = true;
  sqlite3_stmt *stmt;
  std::string del =
      "DELETE FROM command_permissions WHERE guild_id = ? AND feature = ?;";
  if (sqlite3_prepare_v2(db, del.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, feature.c_str(), -1, SQLITE_TRANSIENT);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
  } else {
    ok = false;
  }
  sqlite3_finalize(stmt);

  for (size_t i = 0; ok && i < roles.size(); ++i) {
    std::string ins = "INSERT OR IGNORE INTO command_permissions (guild_id, "
                      "feature, role_id) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, ins.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      ok = false;
      break;
    }
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, feature.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, roles[i]);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
  }

  if (!execute_query(ok ? "COMMIT;" : "ROLLBACK;"))
    return false;
  return ok;
}

std::vector<uint64_t> Database::get_permission_roles(uint64_t guild_id,
                                                     const std::string &feature) {
  std::vector<uint64_t> roles;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT role_id FROM command_permissions WHERE guild_id = ? "
                    "AND feature = ? ORDER BY role_id;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, feature.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW)
      roles.push_back(static_cast<uint64_t>(sqlite3_column_int64(stmt, 0)));
  }
  sqlite3_finalize(stmt);
  return roles;
}

bool Database::set_answer(uint64_t guild_id, const std::string &trigger,
                          const std::string &response, bool contains) {
  sqlite3_stmt *stmt;
  std::string sql =
      "INSERT INTO answers (guild_id, trigger_text, response, match_contains) "
      "VALUES (?, ?, ?, ?) ON CONFLICT(guild_id, trigger_text) DO UPDATE SET "
      "response = excluded.response, match_contains = "
      "excluded.match_contains;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, trigger.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, response.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, contains ? 1 : 0);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return success;
}

bool Database::remove_answer(uint64_t guild_id, const std::string &trigger) {
  sqlite3_stmt *stmt;
  std::string sql =
      "DELETE FROM answers WHERE guild_id = ? AND trigger_text = ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, trigger.c_str(), -1, SQLITE_TRANSIENT);
  bool success = (sqlite3_step(stmt) == SQLITE_DONE);
  int changed = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return success && changed > 0;
}

std::vector<Answer> Database::get_answers(uint64_t guild_id) {
  std::vector<Answer> answers;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT trigger_text, response, match_contains FROM answers "
                    "WHERE guild_id = ? ORDER BY trigger_text;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *t = sqlite3_column_text(stmt, 0);
      const unsigned char *r = sqlite3_column_text(stmt, 1);
      answers.push_back({t ? reinterpret_cast<const char *>(t) : "",
                         r ? reinterpret_cast<const char *>(r) : "",
                         sqlite3_column_int(stmt, 2) != 0});
    }
  }
  sqlite3_finalize(stmt);
  return answers;
}

std::set<std::string> Database::get_enabled_modules(uint64_t guild_id) {
  std::set<std::string> out;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT module_id FROM guild_modules WHERE guild_id = ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *t = sqlite3_column_text(stmt, 0);
      if (t)
        out.insert(reinterpret_cast<const char *>(t));
    }
  }
  sqlite3_finalize(stmt);
  return out;
}

bool Database::set_enabled_modules(uint64_t guild_id,
                                   const std::vector<std::string> &modules) {
  if (!execute_query("BEGIN;"))
    return false;
  bool ok = true;
  sqlite3_stmt *stmt;
  std::string del = "DELETE FROM guild_modules WHERE guild_id = ?;";
  if (sqlite3_prepare_v2(db, del.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
  } else {
    ok = false;
  }
  sqlite3_finalize(stmt);
  for (size_t i = 0; ok && i < modules.size(); ++i) {
    std::string ins = "INSERT OR IGNORE INTO guild_modules (guild_id, "
                      "module_id) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, ins.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      ok = false;
      break;
    }
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, modules[i].c_str(), -1, SQLITE_TRANSIENT);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
  }
  return execute_query(ok ? "COMMIT;" : "ROLLBACK;") && ok;
}

std::string Database::kv_get(uint64_t guild_id, const std::string &key) {
  sqlite3_stmt *stmt;
  std::string sql = "SELECT value FROM guild_kv WHERE guild_id = ? AND key = ?;";
  std::string value;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *t = sqlite3_column_text(stmt, 0);
      value = t ? reinterpret_cast<const char *>(t) : "";
    }
  }
  sqlite3_finalize(stmt);
  return value;
}

bool Database::kv_set(uint64_t guild_id, const std::string &key,
                      const std::string &value) {
  sqlite3_stmt *stmt;
  std::string sql;
  if (value.empty())
    sql = "DELETE FROM guild_kv WHERE guild_id = ? AND key = ?;";
  else
    sql = "INSERT INTO guild_kv (guild_id, key, value) VALUES (?, ?, ?) ON "
          "CONFLICT(guild_id, key) DO UPDATE SET value = excluded.value;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  if (!value.empty())
    sqlite3_bind_text(stmt, 3, value.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

long long Database::incr_counter(uint64_t guild_id, const std::string &key) {
  sqlite3_stmt *stmt;
  std::string up =
      "INSERT INTO counters (guild_id, key, value) VALUES (?, ?, 1) ON "
      "CONFLICT(guild_id, key) DO UPDATE SET value = value + 1;";
  if (sqlite3_prepare_v2(db, up.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  long long v = 0;
  std::string sel = "SELECT value FROM counters WHERE guild_id = ? AND key = ?;";
  if (sqlite3_prepare_v2(db, sel.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW)
      v = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return v;
}

int Database::add_warning(uint64_t guild_id, uint64_t user_id, uint64_t mod_id,
                          const std::string &reason) {
  sqlite3_stmt *stmt;
  std::string ins = "INSERT INTO warnings (guild_id, user_id, mod_id, reason) "
                    "VALUES (?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db, ins.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_int64(stmt, 3, mod_id);
  sqlite3_bind_text(stmt, 4, reason.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  if (!ok)
    return 0;
  return static_cast<int>(get_warnings(guild_id, user_id).size());
}

std::vector<WarningEntry> Database::get_warnings(uint64_t guild_id,
                                                 uint64_t user_id) {
  std::vector<WarningEntry> out;
  sqlite3_stmt *stmt;
  std::string sql = "SELECT id, mod_id, reason, created_at FROM warnings WHERE "
                    "guild_id = ? AND user_id = ? ORDER BY id ASC;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *r = sqlite3_column_text(stmt, 2);
      out.push_back({sqlite3_column_int64(stmt, 0),
                     static_cast<uint64_t>(sqlite3_column_int64(stmt, 1)),
                     r ? reinterpret_cast<const char *>(r) : "",
                     sqlite3_column_int64(stmt, 3)});
    }
  }
  sqlite3_finalize(stmt);
  return out;
}

int Database::clear_warnings(uint64_t guild_id, uint64_t user_id) {
  sqlite3_stmt *stmt;
  std::string sql = "DELETE FROM warnings WHERE guild_id = ? AND user_id = ?;";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    return 0;
  sqlite3_bind_int64(stmt, 1, guild_id);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_step(stmt);
  int n = sqlite3_changes(db);
  sqlite3_finalize(stmt);
  return n;
}

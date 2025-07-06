#pragma once

#include "sqlite3.h"
#include <string>
#include <vector>
#include <iostream>

struct PlayerData
{
  std::string id;
  std::string name;
  int met_count;
  double offense_ratio;
  double defense_ratio;
  std::string note;
};

struct PlaylistRecord
{
  int id;
  std::string player_id;
  int playlist_id;
  int with_wins;
  int with_losses;
  int against_wins;
  int against_losses;
  int goals;
  int assists;
  int shots;
  int saves;
  int epic_saves;
  int clears;
  int demos;
  int center_balls;
  int hat_tricks;
  int playmakers;
  int saviors;
};

struct PlayerWithRecords
{
  PlayerData player;
  std::vector<PlaylistRecord> records;
};

class DatabaseManager
{
public:
  DatabaseManager(const std::string &db_path) : db_path_(db_path), db_(nullptr) {}
  ~DatabaseManager()
  {
    disconnect();
  }

  bool connect()
  {
    if (sqlite3_open(db_path_.c_str(), &db_))
    {
      std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }
    return true;
  }

  void disconnect()
  {
    if (db_)
    {
      sqlite3_close(db_);
      db_ = nullptr;
    }
  }

  bool create_tables()
  {
    const char *create_players_table_sql =
        "CREATE TABLE IF NOT EXISTS players ("
        "id TEXT PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "met_count INTEGER DEFAULT 0,"
        "offense_ratio REAL DEFAULT 0.5,"
        "defense_ratio REAL DEFAULT 0.5,"
        "note TEXT"
        ");";

    const char *create_playlist_records_table_sql =
        "CREATE TABLE IF NOT EXISTS playlist_records ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "player_id TEXT,"
        "playlist_id INTEGER NOT NULL,"
        "with_wins INTEGER DEFAULT 0,"
        "with_losses INTEGER DEFAULT 0,"
        "against_wins INTEGER DEFAULT 0,"
        "against_losses INTEGER DEFAULT 0,"
        "goals INTEGER DEFAULT 0,"
        "assists INTEGER DEFAULT 0,"
        "shots INTEGER DEFAULT 0,"
        "saves INTEGER DEFAULT 0,"
        "epic_saves INTEGER DEFAULT 0,"
        "clears INTEGER DEFAULT 0,"
        "demos INTEGER DEFAULT 0,"
        "center_balls INTEGER DEFAULT 0,"
        "hat_tricks INTEGER DEFAULT 0,"
        "playmakers INTEGER DEFAULT 0,"
        "saviors INTEGER DEFAULT 0,"
        "FOREIGN KEY(player_id) REFERENCES players(id)"
        ");";

    char *err_msg = nullptr;
    if (sqlite3_exec(db_, create_players_table_sql, 0, 0, &err_msg) != SQLITE_OK)
    {
      std::cerr << "SQL error: " << err_msg << std::endl;
      sqlite3_free(err_msg);
      return false;
    }

    if (sqlite3_exec(db_, create_playlist_records_table_sql, 0, 0, &err_msg) != SQLITE_OK)
    {
      std::cerr << "SQL error: " << err_msg << std::endl;
      sqlite3_free(err_msg);
      return false;
    }

    return true;
  }

  // Player CRUD
  bool create_player(const PlayerData &player)
  {
    const char *sql = "INSERT INTO players (id, name, met_count, offense_ratio, defense_ratio, note) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, player.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, player.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, player.met_count);
    sqlite3_bind_double(stmt, 4, player.offense_ratio);
    sqlite3_bind_double(stmt, 5, player.defense_ratio);
    sqlite3_bind_text(stmt, 6, player.note.c_str(), -1, SQLITE_STATIC);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

  bool get_player(const std::string &player_id, PlayerData &player)
  {
    const char *sql = "SELECT * FROM players WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, player_id.c_str(), -1, SQLITE_STATIC);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      player.id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      player.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      player.met_count = sqlite3_column_int(stmt, 2);
      player.offense_ratio = sqlite3_column_double(stmt, 3);
      player.defense_ratio = sqlite3_column_double(stmt, 4);
      player.note = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      result = true;
    }

    sqlite3_finalize(stmt);
    return result;
  }

  bool update_player(const PlayerData &player)
  {
    const char *sql = "UPDATE players SET name = ?, met_count = ?, offense_ratio = ?, defense_ratio = ?, note = ? WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, player.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, player.met_count);
    sqlite3_bind_double(stmt, 3, player.offense_ratio);
    sqlite3_bind_double(stmt, 4, player.defense_ratio);
    sqlite3_bind_text(stmt, 5, player.note.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, player.id.c_str(), -1, SQLITE_STATIC);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

  bool delete_player(const std::string &player_id)
  {
    const char *sql = "DELETE FROM players WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, player_id.c_str(), -1, SQLITE_STATIC);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

  bool get_all_players(std::vector<PlayerData> &players)
  {
    const char *sql = "SELECT * FROM players;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      PlayerData player;
      player.id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      player.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      player.met_count = sqlite3_column_int(stmt, 2);
      player.offense_ratio = sqlite3_column_double(stmt, 3);
      player.defense_ratio = sqlite3_column_double(stmt, 4);
      player.note = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      players.push_back(player);
    }

    sqlite3_finalize(stmt);
    return true;
  }

  bool get_all_players_with_records(std::vector<PlayerWithRecords> &players_with_records)
  {
    std::vector<PlayerData> players;
    if (!get_all_players(players))
    {
      return false;
    }

    for (const auto &player : players)
    {
      PlayerWithRecords pwr;
      pwr.player = player;
      if (get_playlist_records_for_player(player.id, pwr.records))
      {
        players_with_records.push_back(pwr);
      }
    }

    return true;
  }

  // PlaylistRecord CRUD
  bool create_playlist_record(const PlaylistRecord &record)
  {
    const char *sql = "INSERT INTO playlist_records (player_id, playlist_id, with_wins, with_losses, against_wins, against_losses, goals, assists, shots, saves, epic_saves, clears, demos, center_balls, hat_tricks, playmakers, saviors) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, record.player_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, record.playlist_id);
    sqlite3_bind_int(stmt, 3, record.with_wins);
    sqlite3_bind_int(stmt, 4, record.with_losses);
    sqlite3_bind_int(stmt, 5, record.against_wins);
    sqlite3_bind_int(stmt, 6, record.against_losses);
    sqlite3_bind_int(stmt, 7, record.goals);
    sqlite3_bind_int(stmt, 8, record.assists);
    sqlite3_bind_int(stmt, 9, record.shots);
    sqlite3_bind_int(stmt, 10, record.saves);
    sqlite3_bind_int(stmt, 11, record.epic_saves);
    sqlite3_bind_int(stmt, 12, record.clears);
    sqlite3_bind_int(stmt, 13, record.demos);
    sqlite3_bind_int(stmt, 14, record.center_balls);
    sqlite3_bind_int(stmt, 15, record.hat_tricks);
    sqlite3_bind_int(stmt, 16, record.playmakers);
    sqlite3_bind_int(stmt, 17, record.saviors);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

  bool get_playlist_record(int record_id, PlaylistRecord &record)
  {
    const char *sql = "SELECT * FROM playlist_records WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_int(stmt, 1, record_id);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      record.id = sqlite3_column_int(stmt, 0);
      record.player_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      record.playlist_id = sqlite3_column_int(stmt, 2);
      record.with_wins = sqlite3_column_int(stmt, 3);
      record.with_losses = sqlite3_column_int(stmt, 4);
      record.against_wins = sqlite3_column_int(stmt, 5);
      record.against_losses = sqlite3_column_int(stmt, 6);
      record.goals = sqlite3_column_int(stmt, 7);
      record.assists = sqlite3_column_int(stmt, 8);
      record.shots = sqlite3_column_int(stmt, 9);
      record.saves = sqlite3_column_int(stmt, 10);
      record.epic_saves = sqlite3_column_int(stmt, 11);
      record.clears = sqlite3_column_int(stmt, 12);
      record.demos = sqlite3_column_int(stmt, 13);
      record.center_balls = sqlite3_column_int(stmt, 14);
      record.hat_tricks = sqlite3_column_int(stmt, 15);
      record.playmakers = sqlite3_column_int(stmt, 16);
      record.saviors = sqlite3_column_int(stmt, 17);
      result = true;
    }

    sqlite3_finalize(stmt);
    return result;
  }

  bool get_playlist_records_for_player(const std::string &player_id, std::vector<PlaylistRecord> &records)
  {
    const char *sql = "SELECT * FROM playlist_records WHERE player_id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_text(stmt, 1, player_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      PlaylistRecord record;
      record.id = sqlite3_column_int(stmt, 0);
      record.player_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      record.playlist_id = sqlite3_column_int(stmt, 2);
      record.with_wins = sqlite3_column_int(stmt, 3);
      record.with_losses = sqlite3_column_int(stmt, 4);
      record.against_wins = sqlite3_column_int(stmt, 5);
      record.against_losses = sqlite3_column_int(stmt, 6);
      record.goals = sqlite3_column_int(stmt, 7);
      record.assists = sqlite3_column_int(stmt, 8);
      record.shots = sqlite3_column_int(stmt, 9);
      record.saves = sqlite3_column_int(stmt, 10);
      record.epic_saves = sqlite3_column_int(stmt, 11);
      record.clears = sqlite3_column_int(stmt, 12);
      record.demos = sqlite3_column_int(stmt, 13);
      record.center_balls = sqlite3_column_int(stmt, 14);
      record.hat_tricks = sqlite3_column_int(stmt, 15);
      record.playmakers = sqlite3_column_int(stmt, 16);
      record.saviors = sqlite3_column_int(stmt, 17);
      records.push_back(record);
    }

    sqlite3_finalize(stmt);
    return true;
  }

  bool update_playlist_record(const PlaylistRecord &record)
  {
    const char *sql = "UPDATE playlist_records SET with_wins = ?, with_losses = ?, against_wins = ?, against_losses = ?, goals = ?, assists = ?, shots = ?, saves = ?, epic_saves = ?, clears = ?, demos = ?, center_balls = ?, hat_tricks = ?, playmakers = ?, saviors = ? WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_int(stmt, 1, record.with_wins);
    sqlite3_bind_int(stmt, 2, record.with_losses);
    sqlite3_bind_int(stmt, 3, record.against_wins);
    sqlite3_bind_int(stmt, 4, record.against_losses);
    sqlite3_bind_int(stmt, 5, record.goals);
    sqlite3_bind_int(stmt, 6, record.assists);
    sqlite3_bind_int(stmt, 7, record.shots);
    sqlite3_bind_int(stmt, 8, record.saves);
    sqlite3_bind_int(stmt, 9, record.epic_saves);
    sqlite3_bind_int(stmt, 10, record.clears);
    sqlite3_bind_int(stmt, 11, record.demos);
    sqlite3_bind_int(stmt, 12, record.center_balls);
    sqlite3_bind_int(stmt, 13, record.hat_tricks);
    sqlite3_bind_int(stmt, 14, record.playmakers);
    sqlite3_bind_int(stmt, 15, record.saviors);
    sqlite3_bind_int(stmt, 16, record.id);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

  bool delete_playlist_record(int record_id)
  {
    const char *sql = "DELETE FROM playlist_records WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK)
    {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
      return false;
    }

    sqlite3_bind_int(stmt, 1, record_id);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
  }

private:
  std::string db_path_;
  struct sqlite3 *db_;
};

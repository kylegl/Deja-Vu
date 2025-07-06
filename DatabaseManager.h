#pragma once

#include "sqlite3.h"
#include <string>
#include <vector>

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
  DatabaseManager(const std::string &db_path);
  ~DatabaseManager();

  bool connect();
  void disconnect();
  bool create_tables();

  // Player CRUD
  bool create_player(const PlayerData &player);
  bool get_player(const std::string &player_id, PlayerData &player);
  bool update_player(const PlayerData &player);
  bool delete_player(const std::string &player_id);
  bool get_all_players(std::vector<PlayerData> &players);
  bool get_all_players_with_records(std::vector<PlayerWithRecords> &players);

  // PlaylistRecord CRUD
  bool create_playlist_record(const PlaylistRecord &record);
  bool get_playlist_record(int record_id, PlaylistRecord &record);
  bool get_playlist_records_for_player(const std::string &player_id, std::vector<PlaylistRecord> &records);
  bool update_playlist_record(const PlaylistRecord &record);
  bool delete_playlist_record(int record_id);

private:
  std::string db_path_;
  struct sqlite3 *db_;
};

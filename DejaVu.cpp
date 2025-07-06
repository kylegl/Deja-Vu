#include "pch.h"
#include "DejaVu.h"
#include "bakkesmod/wrappers/GameObject/Stats/StatEventWrapper.h"
#include "imgui/imgui.h"
#include <fstream>
#include "nlohmann/json.hpp" // This file is not in the project, you need to add it. https://github.com/nlohmann/json

using json = nlohmann::json;

/**
 * TODO
 * - Add option to show total record across all playlists
 * - Create 2-way cvar binding
 * - IMGUI stuff
 */

INITIALIZE_EASYLOGGINGPP

BAKKESMOD_PLUGIN(DejaVu, "Deja Vu", PluginVersion, 0)

// to_string overloads for cvars
namespace std
{
  inline string to_string(const std::string &str)
  {
    return str;
  }
  string to_string(const LinearColor &color)
  {
    char buf[49];
    sprintf(buf, "(%f, %f, %f, %f)", color.R, color.G, color.B, color.A);
    return buf;
  }
  string to_string(const Record &record)
  {
    return to_string(record.wins) + ":" + to_string(record.losses);
  }
  string to_string(const PlaylistID &playlist)
  {
    return to_string(static_cast<int>(playlist));
  }
}

void SetupLogger(std::string logPath, bool enabled)
{
  el::Configurations defaultConf;
  defaultConf.setToDefault();
  defaultConf.setGlobally(el::ConfigurationType::Filename, logPath);
  defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime %level [%fbase:%line] %msg");
  defaultConf.setGlobally(el::ConfigurationType::Enabled, enabled ? "true" : "false");
  el::Loggers::reconfigureLogger("default", defaultConf);
}

void DejaVu::HookAndLogEvent(std::string eventName)
{
  this->gameWrapper->HookEvent(eventName, std::bind(&DejaVu::LogChatbox, this, std::placeholders::_1));
  this->gameWrapper->HookEvent(eventName, std::bind(&DejaVu::Log, this, std::placeholders::_1));
}

void DejaVu::onLoad()
{
  this->isAlreadyAddedToStats = false;

  // At end of match in unranked when people leave and get replaced by bots the event fires and for some reason IsInOnlineGame turns back on
  // Check 1v1. Player added event didn't fire after joining last
  // Add debug
  this->mmrWrapper = this->gameWrapper->GetMMRWrapper();

  this->dataDir = this->gameWrapper->GetDataFolder().append("dejavu");
  this->logPath = std::filesystem::path(dataDir).append(this->logFile);
  std::string dbPath = std::filesystem::path(dataDir).append("dejavu.db").string();
  this->dbManager = std::make_unique<DatabaseManager>(dbPath);
  this->dbManager->connect();
  this->dbManager->create_tables();

  this->cvarManager->registerNotifier("dejavu_track_current", [this](const std::vector<std::string> &commands)
                                      { HandlePlayerAdded("dejavu_track_current"); }, "Tracks current lobby", PERMISSION_ONLINE);

  this->cvarManager->registerNotifier("dejavu_export_json", [this](const std::vector<std::string> &commands)
                                      { ExportData(); }, "Exports all player data to a JSON file.", PERMISSION_ALL);

  this->cvarManager->registerNotifier("dejavu_launch_quick_note", [this](const std::vector<std::string> &commands)
                                      {
#if !DEV
  if (IsInRealGame())
#endif !DEV
  	LaunchQuickNoteModal(); }, "Launches the quick note modal", PERMISSION_ONLINE);

#if DEV
  this->cvarManager->registerNotifier("dejavu_test", [this](const std::vector<std::string> &commands)
                                      { this->gameWrapper->SetTimeout([this](GameWrapper *gameWrapper)
                                                                      { Log("test after 5"); }, 5); }, "test", PERMISSION_ALL);

  this->cvarManager->registerNotifier("dejavu_cleanup", [this](const std::vector<std::string> &commands)
                                      { CleanUpJson(); }, "Cleans up the database", PERMISSION_ALL);

  this->cvarManager->registerNotifier("dejavu_dump_list", [this](const std::vector<std::string> &commands)
                                      {
		if (this->matchesMetLists.size() == 0)
		{
			this->cvarManager->log("No entries in list");
			return;
		}

		for (auto& match : this->matchesMetLists)
		{
			std::string guid = match.first;
			this->cvarManager->log("For match GUID:" + guid);
			auto& set = match.second;
			for (auto& playerID : set)
			{
				this->cvarManager->log("    " + playerID);
			}
		} }, "Dumps met list", PERMISSION_ALL);
  this->cvarManager->registerNotifier("dejavu_test_cvar_binding", [this](const std::vector<std::string> &commands)
                                      { this->enabledDebug = !*this->enabledDebug; }, "Tests cvar binding", PERMISSION_ALL);
#endif DEV

#pragma region register cvars

  this->enabled.Register(this->cvarManager);

  this->trackOpponents.Register(this->cvarManager);
  this->trackTeammates.Register(this->cvarManager);
  this->trackGrouped.Register(this->cvarManager);
  this->showMetCount.Register(this->cvarManager);
  this->showRecord.Register(this->cvarManager);
  this->showAllPlaylistsRecord.Register(this->cvarManager);

  auto visualCVar = this->enabledVisuals.Register(this->cvarManager);
  visualCVar.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
                               {
		if (!cvar.getBoolValue())
			this->enabledDebug = false; });
  this->toggleWithScoreboard.Register(this->cvarManager);
  this->showNotes.Register(this->cvarManager);

  auto debugCVar = this->enabledDebug.Register(this->cvarManager);
  debugCVar.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
                              {
		bool val = cvar.getBoolValue();

		if (this->gameWrapper->IsInOnlineGame()) {
			if (val)
				cvar.setValue(false);
			return;
		}

		if (val) {
			// this->blueTeamRenderData.push_back({ "0", "Blue Player 1", 5, { 5, 5 }, { 5, 5 }, "This guy was a great team player" });
			// this->blueTeamRenderData.push_back({ "0", "Blue Player 2", 15, { 15, 15 }, { 15, 15 }, "Quick chat spammer" });
			// this->blueTeamRenderData.push_back({ "0", "Blue Player 3 with a loooonngggg name", 9999, { 999, 999 }, { 9999, 9999 } });
			// this->orangeTeamRenderData.push_back({ "0", "Orange Player 1", 5, { 5, 5 }, { 55, 55 } });
			// this->orangeTeamRenderData.push_back({ "0", "Orange Player 2", 15, { 15, 15 }, { 150, 150 }, "Left early" });
		} });
#if DEV
  this->enabledDebug = true;
#endif DEV

  this->scale.Register(this->cvarManager);

  this->alpha.Register(this->cvarManager);

  this->xPos.Register(this->cvarManager);
  this->yPos.Register(this->cvarManager);
  this->width.Register(this->cvarManager);

#pragma warning(suppress : 4996)
  this->textColorR.Register(this->cvarManager);
#pragma warning(suppress : 4996)
  this->textColorG.Register(this->cvarManager);
#pragma warning(suppress : 4996)
  this->textColorB.Register(this->cvarManager);
  this->textColor.Register(this->cvarManager);

  this->enabledBorders.Register(this->cvarManager);
  this->borderColor.Register(this->cvarManager);

  this->enabledBackground.Register(this->cvarManager);

#pragma warning(suppress : 4996)
  this->backgroundColorR.Register(this->cvarManager);
#pragma warning(suppress : 4996)
  this->backgroundColorG.Register(this->cvarManager);
#pragma warning(suppress : 4996)
  this->backgroundColorB.Register(this->cvarManager);
  this->backgroundColor.Register(this->cvarManager);

  this->hasUpgradedColors.Register(this->cvarManager);

  auto logCVar = this->enabledLog.Register(this->cvarManager);
  logCVar.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
                            {
		bool val = cvar.getBoolValue();

		SetupLogger(this->logPath.string(), val); });

  this->mainGUIKeybind.Register(this->cvarManager).addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
                                                                     {
		std::string newBind = cvar.getStringValue();
		if (!oldValue.empty() && oldValue != "None")
			this->cvarManager->executeCommand("unbind " + oldValue, false);
		if (newBind != "None")
			this->cvarManager->setBind(newBind, "togglemenu dejavu"); });
  this->quickNoteKeybind.Register(this->cvarManager).addOnValueChanged([this](std::string oldValue, CVarWrapper cvar)
                                                                       {
		std::string newBind = cvar.getStringValue();
		if (!oldValue.empty() && oldValue != "None")
			this->cvarManager->executeCommand("unbind " + oldValue, false);
		if (newBind != "None")
			this->cvarManager->setBind(newBind, "dejavu_launch_quick_note"); });

#pragma endregion register cvars

  // I guess this doesn't fire for "you"
  this->gameWrapper->HookEvent("Function TAGame.GameEvent_TA.EventPlayerAdded", std::bind(&DejaVu::HandlePlayerAdded, this, std::placeholders::_1));
  this->gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState", std::bind(&DejaVu::HandlePlayerAdded, this, std::placeholders::_1));
  this->gameWrapper->HookEvent("Function TAGame.Team_TA.EventScoreUpdated", std::bind(&DejaVu::HandlePlayerAdded, this, std::placeholders::_1));
  // TODO: Look for event like "spawning" so that when you join an in progress match it will gather data

  this->gameWrapper->HookEvent("Function TAGame.GameEvent_TA.EventPlayerRemoved", std::bind(&DejaVu::HandlePlayerRemoved, this, std::placeholders::_1));

  // Don't think this one ever actually works
  this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitGame", bind(&DejaVu::HandleGameStart, this, std::placeholders::_1));
  // Fires when joining a game
  this->gameWrapper->HookEvent("Function OnlineGameJoinGame_X.JoiningBase.IsJoiningGame", std::bind(&DejaVu::HandleGameStart, this, std::placeholders::_1));
  // Fires when the match is first initialized
  this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnAllTeamsCreated", std::bind(&DejaVu::HandleGameStart, this, std::placeholders::_1));

  this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet", std::bind(&DejaVu::HandleWinnerSet, this, std::placeholders::_1));
  this->gameWrapper->HookEvent("Function TAGame.GameEvent_TA.OnCanVoteForfeitChanged", std::bind(&DejaVu::HandleForfeitChanged, this, std::placeholders::_1));
  this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated", std::bind(&DejaVu::HandleGameTimeUpdate, this, std::placeholders::_1));

  this->gameWrapper->HookEvent("Function TAGame.GFxShell_TA.LeaveMatch", std::bind(&DejaVu::HandleGameLeave, this, std::placeholders::_1));
  // Function TAGame.GFxHUD_TA.HandlePenaltyChanged

  this->gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnOpenScoreboard", std::bind(&DejaVu::OpenScoreboard, this, std::placeholders::_1));
  this->gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnCloseScoreboard", std::bind(&DejaVu::CloseScoreboard, this, std::placeholders::_1));

  this->gameWrapper->UnregisterDrawables();
  this->gameWrapper->RegisterDrawable(bind(&DejaVu::RenderDrawable, this, std::placeholders::_1));

  gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatEvent",
                                                      [this](ServerWrapper caller, void *params, std::string eventName)
                                                      {
                                                        OnStatEvent(caller, params, eventName);
                                                      });

  /*
  HookEventWithCaller<ServerWrapper>("FUNCTION", bind(&CLASSNAME::FUNCTIONNAME, this, placeholders::_1, 2, 3);
  void CLASSNAME::FUNCTIONNAME(ServerWrapper caller, void* params, string funcName)
  {
    bool returnVal = (bool)params;
  }
  */

  // std::string eventsToLog[] = {
  //"Function TAGame.GameEvent_Soccar_TA.EndGame",
  //"Function TAGame.GameEvent_Soccar_TA.EndRound",
  //"Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
  //"Function TAGame.GameEvent_Soccar_TA.EventGameEnded",
  //"Function TAGame.GameEvent_Soccar_TA.EventGameWinnerSet",
  //"Function TAGame.GameEvent_Soccar_TA.EventMatchWinnerSet",
  //"Function TAGame.GameEvent_Soccar_TA.HasWinner",
  //"Function TAGame.GameEvent_Soccar_TA.EventEndGameCountDown",
  //"Function TAGame.GameEvent_Soccar_TA.EventGameTimeUpdated",
  //"Function TAGame.GameEvent_Soccar_TA.Finished.BeginState",
  //"Function TAGame.GameEvent_Soccar_TA.Finished.OnFinished",
  //"Function TAGame.GameEvent_Soccar_TA.FinishEvent",
  //"Function TAGame.GameEvent_Soccar_TA.HasWinner",
  //"Function TAGame.GameEvent_Soccar_TA.OnGameWinnerSet",
  //"Function TAGame.GameEvent_Soccar_TA.SetMatchWinner",
  //"Function TAGame.GameEvent_Soccar_TA.SubmitMatch",
  //"Function TAGame.GameEvent_Soccar_TA.SubmitMatchComplete",
  //"Function TAGame.GameEvent_Soccar_TA.WaitForEndRound",
  //"Function TAGame.GameEvent_Soccar_TA.EventActiveRoundChanged",
  //"Function TAGame.GameEvent_Soccar_TA.GetWinningTeam",
  //"Function TAGame.GameEvent_TA.EventGameStateChanged",
  //"Function TAGame.GameEvent_TA.Finished.EndState",
  //"Function TAGame.GameEvent_TA.Finished.BeginState",
  //"Function TAGame.GameEvent_Soccar_TA.Destroyed",
  //"Function TAGame.GameEvent_TA.IsFinished",
  //};

  // for (std::string eventName : eventsToLog)
  //{
  //	HookAndLogEvent(eventName);
  // }

  /*
    Goal Scored event: "Function TAGame.Team_TA.EventScoreUpdated"
    Game Start event: "Function TAGame.GameEvent_Soccar_TA.InitGame"
    Game End event: "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded"
    Function TAGame.GameEvent_Soccar_TA.Destroyed
    Function TAGame.GameEvent_Soccar_TA.EventMatchEnded
    Function GameEvent_TA.Countdown.BeginState
    Function TAGame.GameEvent_Soccar_TA.InitField

    Function TAGame.GameEvent_Soccar_TA.InitGame
    Function TAGame.GameEvent_Soccar_TA.InitGame

    Function TAGame.GameEvent_Soccar_TA.EventGameWinnerSet
    Function TAGame.GameEvent_Soccar_TA.EventMatchWinnerSet
  */

  GenerateSettingsFile();

  this->gameWrapper->SetTimeout([this](GameWrapper *gameWrapper)
                                {
		if (!*this->hasUpgradedColors)
		{
			LOG(INFO) << "Upgrading colors...";
#pragma warning(suppress : 4996)
			this->textColor = LinearColor{ (float)*this->textColorR, (float)*this->textColorG, (float)*this->textColorB, 0xff };
#pragma warning(suppress : 4996)
			this->backgroundColor = LinearColor{ (float)*this->backgroundColorR, (float)*this->backgroundColorG, (float)*this->backgroundColorB, 0xff };
			this->hasUpgradedColors = true;
			this->cvarManager->executeCommand("writeconfig");
		}

		LOG(INFO) << "---DEJAVU LOADED---"; }, 0.001f);

#if DEV
  this->cvarManager->executeCommand("exec tmp.cfg");
#endif
}

void DejaVu::onUnload()
{
  LOG(INFO) << "---DEJAVU UNLOADED---";

#if DEV
  this->cvarManager->backupCfg("./bakkesmod/cfg/tmp.cfg");
#endif

  if (this->isWindowOpen)
    this->cvarManager->executeCommand("togglemenu " + GetMenuName());
}

void DejaVu::OpenScoreboard(std::string eventName)
{
  this->isScoreboardOpen = true;
}

void DejaVu::CloseScoreboard(std::string eventName)
{
  this->isScoreboardOpen = false;
}

void DejaVu::Log(std::string msg)
{
  this->cvarManager->log(msg);
}

void DejaVu::LogError(std::string msg)
{
  this->cvarManager->log("ERROR: " + msg);
}

void DejaVu::LogChatbox(std::string msg)
{
  this->gameWrapper->LogToChatbox(msg);
  LOG(INFO) << msg;
}

std::optional<std::string> DejaVu::GetMatchGUID()
{
  ServerWrapper server = GetCurrentServer();
  if (server.IsNull())
    return std::nullopt;
  if (server.IsPlayingPrivate())
    return std::nullopt;
  const std::string &curMatchGUID = server.GetMatchGUID();
  if (curMatchGUID == "No worldInfo" || curMatchGUID.length() == 0)
    return std::nullopt;
  return curMatchGUID;
}

ServerWrapper DejaVu::GetCurrentServer()
{
  return this->gameWrapper->GetCurrentGameState();
}

PriWrapper DejaVu::GetLocalPlayerPRI()
{
  auto server = GetCurrentServer();
  if (server.IsNull())
    return NULL;
  auto player = server.GetLocalPrimaryPlayer();
  if (player.IsNull())
    return NULL;
  return player.GetPRI();
}

void DejaVu::HandlePlayerAdded(std::string eventName)
{
  if (!IsInRealGame())
    return;
  LOG(INFO) << "HandlePlayerAdded: " << eventName;
  if (this->gameIsOver)
    return;
  ServerWrapper server = GetCurrentServer();
  LOG(INFO) << "server is null: " << (server.IsNull() ? "true" : "false");
  if (server.IsNull())
    return;
  if (server.IsPlayingPrivate())
    return;
  auto matchGUID = GetMatchGUID();
  if (!matchGUID.has_value())
    return;
  LOG(INFO) << "Match GUID: " << matchGUID.value();
  if (!this->curMatchGUID.has_value())
    this->curMatchGUID = matchGUID;
  MMRWrapper mw = this->gameWrapper->GetMMRWrapper();
  ArrayWrapper<PriWrapper> pris = server.GetPRIs();

  int len = pris.Count();

  bool needsSave = false;

  for (int i = 0; i < len; i++)
  {
    PriWrapper player = pris.Get(i);
    bool isSpectator = player.GetbIsSpectator();
    if (isSpectator)
    {
      LOG(INFO) << "player is spectator. skipping";
      continue;
    }

    PriWrapper localPlayer = GetLocalPlayerPRI();
    if (!localPlayer.IsNull())
    {
      bool isTeammate = player.GetTeamNum() == localPlayer.GetTeamNum();
      std::string myLeaderID = localPlayer.GetPartyLeaderID().str();
      bool isInMyGroup = myLeaderID != "0" && player.GetPartyLeaderID().str() == myLeaderID;

      if (isTeammate && !*this->trackTeammates)
      {
        continue;
      }

      if (!isTeammate && !*this->trackOpponents)
      {
        continue;
      }

      if (isInMyGroup && !*this->trackGrouped)
      {
        continue;
      }

      UniqueIDWrapper uniqueID = player.GetUniqueIdWrapper();

      std::string uniqueIDStr = uniqueID.str();

      UniqueIDWrapper localUniqueID = localPlayer.GetUniqueIdWrapper();
      std::string localUniqueIDStr = localUniqueID.str();

      // Skip self
      if (uniqueIDStr == localUniqueIDStr)
      {
        continue;
        //} else
        //{
        // this->gameWrapper->LogToChatbox(uniqueID.str() + " != " + localUniqueID.str());
      }

      std::string playerName = player.GetPlayerName().ToString();
      LOG(INFO) << "uniqueID: " << uniqueIDStr << " name: " << playerName;

      // Bots
      if (uniqueIDStr == "0")
      {
        playerName = "[BOT]";
        continue;
      }

      int curPlaylist = mw.GetCurrentPlaylist();

      // GetAndSetMetMMR(localUniqueID, curPlaylist, uniqueID);

      // GetAndSetMetMMR(uniqueID, curPlaylist, uniqueID);

      // Only do met count logic if we haven't yet
      if (this->matchesMetLists[this->curMatchGUID.value()].count(uniqueIDStr) == 0)
      {
        LOG(INFO) << "Haven't processed yet: " << playerName;
        this->matchesMetLists[this->curMatchGUID.value()].emplace(uniqueIDStr);

        PlayerData playerData;
        if (!this->dbManager->get_player(uniqueIDStr, playerData))
        {
          LOG(INFO) << "Haven't met yet: " << playerName;
          playerData.id = uniqueIDStr;
          playerData.name = playerName;
          playerData.met_count = 1;
          this->dbManager->create_player(playerData);
        }
        else
        {
          LOG(INFO) << "Have met before: " << playerName;
          playerData.met_count++;
          playerData.name = playerName;
          this->dbManager->update_player(playerData);
        }
      }
      AddPlayerToRenderData(player);
    }
    else
    {
      LOG(INFO) << "localPlayer is null";
    }
  }
}

void DejaVu::AddPlayerToRenderData(PriWrapper player)
{
  auto uniqueIDStr = player.GetUniqueIdWrapper().str();
  // If we've already added him to the list, return
  if (this->currentMatchPRIs.count(uniqueIDStr) != 0)
    return;
  auto server = GetCurrentServer();
  auto myTeamNum = server.GetLocalPrimaryPlayer().GetPRI().GetTeamNum();
  auto theirTeamNum = player.GetTeamNum();
  if (myTeamNum == TEAM_NOT_SET || theirTeamNum == TEAM_NOT_SET)
  {
    LOG(INFO) << "No team set for " << player.GetPlayerName().ToString() << ", retrying in 5 seconds: " << (int)myTeamNum << ":" << (int)theirTeamNum;
    // No team set. Try again in a couple seconds
    this->gameWrapper->SetTimeout([this](GameWrapper *gameWrapper)
                                  { HandlePlayerAdded("NoTeamSetRetry"); }, 5);
    return;
  }
  // Team set, so we all good
  this->currentMatchPRIs.emplace(uniqueIDStr, player);

  LOG(INFO) << "adding player: " << player.GetPlayerName().ToString();

  PlayerData playerData;
  this->dbManager->get_player(uniqueIDStr, playerData);
  int metCount = playerData.met_count;
  std::string playerNote = playerData.note;

  bool sameTeam = theirTeamNum == myTeamNum;
  Record record = GetRecord(uniqueIDStr, static_cast<PlaylistID>(server.GetPlaylist().GetPlaylistId()), sameTeam ? Side::Same : Side::Other);
  Record allRecord = GetRecord(uniqueIDStr, PlaylistID::ANY, sameTeam ? Side::Same : Side::Other);
  LOG(INFO) << "player team num: " << std::to_string(theirTeamNum);
  std::string playerName = player.GetPlayerName().ToString();
}

void DejaVu::RemovePlayerFromRenderData(PriWrapper player)
{
  if (player.IsNull())
    return;
  if (!player.GetPlayerName().IsNull())
    LOG(INFO) << "Removing player: " << player.GetPlayerName().ToString();
  std::string steamID = player.GetUniqueIdWrapper().str();
  LOG(INFO) << "Player SteamID: " << steamID;
}

void DejaVu::HandlePlayerRemoved(std::string eventName)
{
  if (!IsInRealGame())
    return;
  LOG(INFO) << eventName;

  auto server = GetCurrentServer();

  auto pris = server.GetPRIs();
  for (auto it = this->currentMatchPRIs.begin(); it != this->currentMatchPRIs.end();)
  {
    std::string playerID = it->first;
    auto player = it->second;

    bool isInGame = false;
    for (int i = 0; i < pris.Count(); i++)
    {
      auto playerInGame = pris.Get(i);
      std::string playerInGameID = playerInGame.GetUniqueIdWrapper().str();
      if (playerID == playerInGameID)
        isInGame = true;
    }
    if (!isInGame)
    {
      if (!player.IsNull() && !player.GetPlayerName().IsNull())
        LOG(INFO) << "Player is no longer in game: " << player.GetPlayerName().ToString();
      it = this->currentMatchPRIs.erase(it);
      RemovePlayerFromRenderData(player);
    }
    else
      ++it;
  }
}

void DejaVu::HandleGameStart(std::string eventName)
{
  LOG(INFO) << eventName;
  Reset();
  this->cvarManager->getCvar("cl_dejavu_debug").setValue(false);
  this->curMatchGUID = GetMatchGUID();
  this->isAlreadyAddedToStats = false;
}

void DejaVu::HandleWinnerSet(std::string eventName)
{
  LOG(INFO) << eventName;

  GameOver();
}

void DejaVu::HandleForfeitChanged(std::string eventName)
{
  LOG(INFO) << eventName;

  ServerWrapper server = this->gameWrapper->GetCurrentGameState();

  if (server.IsNull())
    return;

  if (server.GetbCanVoteToForfeit())
    return;

  // that means some team forfeited the game

  GameOver();
}

void DejaVu::HandleGameTimeUpdate(std::string eventName)
{
  LOG(INFO) << eventName;

  ServerWrapper server = this->gameWrapper->GetCurrentGameState();

  if (server.IsNull())
    return;

  if (!server.IsFinished())
    return;

  float time = server.GetGameTimeRemaining();

  // Overtime is over or 0 goal is scored meaning game is finished

  if (time <= 0)
  {
    GameOver();
  }
}

void DejaVu::GameOver()
{
  if (!this->isAlreadyAddedToStats)
  {
    SetRecord();
    Reset();
    this->gameIsOver = true;
    this->isAlreadyAddedToStats = true;
  }
}

void DejaVu::HandleGameLeave(std::string eventName)
{
  LOG(INFO) << eventName;
  Reset();
  this->curMatchGUID = std::nullopt;
  this->isAlreadyAddedToStats = false;
}

void DejaVu::OnStatEvent(ServerWrapper caller, void *params, std::string eventName)
{
  if (params == nullptr)
  {
    return;
  }
  auto server = GetCurrentServer();
  if (server.IsNull())
    return;

  StatEventParams *pStruct = (StatEventParams *)params;
  PriWrapper playerPRI = PriWrapper(pStruct->PRI);
  StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);

  if (playerPRI.IsNull() || statEvent.IsNull())
  {
    return;
  }
  std::string uniqueIDStr = playerPRI.GetUniqueIdWrapper().str();
  if (uniqueIDStr == "0")
    return;

  PlaylistID playlist = static_cast<PlaylistID>(server.GetPlaylist().GetPlaylistId());
  PlaylistRecord playlistRecord;
  // This is not ideal, but we'll need a way to get a specific record for a player and playlist.
  // For now, let's assume a new record is created if one doesn't exist.
  std::vector<PlaylistRecord> records;
  dbManager->get_playlist_records_for_player(uniqueIDStr, records);

  auto it = std::find_if(records.begin(), records.end(), [playlist](const PlaylistRecord &r)
                         { return r.playlist_id == static_cast<int>(playlist); });

  if (it != records.end())
  {
    playlistRecord = *it;
  }
  else
  {
    playlistRecord.player_id = uniqueIDStr;
    playlistRecord.playlist_id = static_cast<int>(playlist);
  }

  std::string statName = statEvent.GetStatName();

  if (statName == "Goal")
    playlistRecord.goals++;
  else if (statName == "Assist")
    playlistRecord.assists++;
  else if (statName == "Shot")
    playlistRecord.shots++;
  else if (statName == "Save")
    playlistRecord.saves++;
  else if (statName == "EpicSave")
    playlistRecord.epic_saves++;
  else if (statName == "Clear")
    playlistRecord.clears++;
  else if (statName == "Hat Trick")
    playlistRecord.hat_tricks++;
  else if (statName == "Playmaker")
    playlistRecord.playmakers++;
  else if (statName == "Savior")
    playlistRecord.saviors++;
  else if (statName == "Center")
    playlistRecord.center_balls++;
  else if (statName == "Demolition")
    playlistRecord.demos++;

  if (playlistRecord.id == 0)
  {
    dbManager->create_playlist_record(playlistRecord);
  }
  else
  {
    dbManager->update_playlist_record(playlistRecord);
  }

  Log(playerPRI.GetPlayerName().ToString() + " got stat: " + statEvent.GetStatName());
}

void DejaVu::Reset()
{
  this->gameIsOver = false;
  this->currentMatchPRIs.clear();
  // Maybe move this to HandleGameLeave but probably don't need to worry about it
  // this->matchesMetLists.clear();
}

void DejaVu::GetAndSetMetMMR(SteamID steamID, int playlist, SteamID idToSet)
{
  this->gameWrapper->SetTimeout([this, steamID, playlist, idToSet](GameWrapper *gameWrapper)
                                {
                                  float mmrValue = this->mmrWrapper.GetPlayerMMR(steamID, playlist);
                                  // For some reason getting a lot of these values 100.01998901367188
                                  if (mmrValue < 0 && !this->mmrWrapper.IsSynced(steamID, playlist))
                                  {
                                    Log("Not synced yet: " + std::to_string(mmrValue) + "|" + std::to_string(this->mmrWrapper.IsSyncing(steamID)));
                                    GetAndSetMetMMR(steamID, playlist, idToSet);
                                    return;
                                  }

                                  // TODO: Add this back in
                                  // json& player = this->allPlayersData[std::to_string(idToSet.ID)];
                                  // std::string keyToSet = (steamID.ID == idToSet.ID) ? "otherMetMMR" : "playerMetMMR";
                                  // if (!player.contains(keyToSet))
                                  //{
                                  //	player[keyToSet] = json::object();
                                  // }
                                  // player[keyToSet][std::to_string(playlist)] = mmrValue;
                                  // WriteData();
                                },
                                5);
}

Record DejaVu::GetRecord(UniqueIDWrapper uniqueID, PlaylistID playlist, Side side)
{
  return GetRecord(uniqueID.str(), playlist, side);
}

Record DejaVu::GetRecord(std::string uniqueID, PlaylistID playlist, Side side)
{
  PlayerData playerData;
  if (!this->dbManager->get_player(uniqueID, playerData))
    return {0, 0};

  std::vector<PlaylistRecord> records;
  this->dbManager->get_playlist_records_for_player(uniqueID, records);

  if (playlist == PlaylistID::ANY)
  {
    Record combinedRecord{};
    for (const auto &record : records)
    {
      if (side == Side::Same)
      {
        combinedRecord.wins += record.with_wins;
        combinedRecord.losses += record.with_losses;
      }
      else
      {
        combinedRecord.wins += record.against_wins;
        combinedRecord.losses += record.against_losses;
      }
    }
    return combinedRecord;
  }

  for (const auto &record : records)
  {
    if (record.playlist_id == static_cast<int>(playlist))
    {
      if (side == Side::Same)
        return {record.with_wins, record.with_losses};
      else
        return {record.against_wins, record.against_losses};
    }
  }

  return {0, 0};
}

void DejaVu::SetRecord()
{
  if (!IsInRealGame())
    return;

  auto server = this->gameWrapper->GetOnlineGame();
  if (server.IsNull())
    return;

  auto winningTeam = server.GetWinningTeam();
  if (winningTeam.IsNull())
    return;

  auto localPlayer = server.GetLocalPrimaryPlayer();
  if (localPlayer.IsNull())
    return;
  auto localPRI = localPlayer.GetPRI();
  if (localPRI.IsNull())
    return;

  PlaylistID playlist = static_cast<PlaylistID>(server.GetPlaylist().GetPlaylistId());
  bool myTeamWon = winningTeam.GetTeamNum() == localPRI.GetTeamNum();
  Log(myTeamWon ? "YOU WON!" : "YOU LOST!");
  UniqueIDWrapper myID = localPRI.GetUniqueIdWrapper();
  auto players = server.GetPRIs();
  for (int i = 0; i < players.Count(); i++)
  {
    auto player = players.Get(i);
    std::string uniqueIDStr = player.GetUniqueIdWrapper().str();
    if (uniqueIDStr == myID.str() || uniqueIDStr == "0")
      continue;

    bool sameTeam = player.GetTeamNum() == localPRI.GetTeamNum();

    std::vector<PlaylistRecord> records;
    this->dbManager->get_playlist_records_for_player(uniqueIDStr, records);

    PlaylistRecord *playlistRecord = nullptr;
    for (auto &rec : records)
    {
      if (rec.playlist_id == static_cast<int>(playlist))
      {
        playlistRecord = &rec;
        break;
      }
    }

    if (!playlistRecord)
    {
      PlaylistRecord newRecord;
      newRecord.player_id = uniqueIDStr;
      newRecord.playlist_id = static_cast<int>(playlist);
      this->dbManager->create_playlist_record(newRecord);
      // a bit inefficient to fetch again, but it's the easiest way to get the ID
      this->dbManager->get_playlist_records_for_player(uniqueIDStr, records);
      for (auto &rec : records)
      {
        if (rec.playlist_id == static_cast<int>(playlist))
        {
          playlistRecord = &rec;
          break;
        }
      }
    }

    if (sameTeam)
    {
      if (myTeamWon)
        playlistRecord->with_wins++;
      else
        playlistRecord->with_losses++;
    }
    else
    {
      if (myTeamWon)
        playlistRecord->against_wins++;
      else
        playlistRecord->against_losses++;
    }

    this->dbManager->update_playlist_record(*playlistRecord);

    PlayerData playerData;
    this->dbManager->get_player(uniqueIDStr, playerData);
    CalculatePlayerRatios(playerData);
    this->dbManager->update_player(playerData);
  }
}

bool DejaVu::IsInRealGame()
{
  return this->gameWrapper->IsInOnlineGame() && !this->gameWrapper->IsInReplay() && !this->gameWrapper->IsInFreeplay();
}

static float MetCountColumnWidth;
static float RecordColumnWidth;
void DejaVu::CleanUpJson()
{
  // This function is now dangerous as it would delete players with only one game.
  // I will disable it for now.
  this->cvarManager->log("CleanUpJson is disabled for now.");
}

void DejaVu::CalculatePlayerRatios(PlayerData &player)
{
  std::vector<PlaylistRecord> records;
  this->dbManager->get_playlist_records_for_player(player.id, records);

  float totalOffensivePoints = 0;
  float totalDefensivePoints = 0;

  for (const auto &record : records)
  {
    totalOffensivePoints += record.goals * 10;
    totalOffensivePoints += record.assists * 5;
    totalOffensivePoints += record.shots * 1;
    totalOffensivePoints += record.center_balls * 1;
    totalOffensivePoints += record.hat_tricks * 2.5;
    totalOffensivePoints += record.playmakers * 2.5;

    totalDefensivePoints += record.saves * 5;
    totalDefensivePoints += record.epic_saves * 7.5;
    totalDefensivePoints += record.clears * 2;
    totalDefensivePoints += record.saviors * 2.5;
  }

  if (player.met_count > 0)
  {
    float avgOffensivePoints = totalOffensivePoints / player.met_count;
    float avgDefensivePoints = totalDefensivePoints / player.met_count;
    float totalPoints = avgOffensivePoints + avgDefensivePoints;

    if (totalPoints > 0)
    {
      player.offense_ratio = avgOffensivePoints / totalPoints;
      player.defense_ratio = avgDefensivePoints / totalPoints;
    }
  }
}

void DejaVu::GenerateSettingsFile()
{
  this->cvarManager->executeCommand("writeconfig");
}
void DejaVu::Render()
{
  ImGui::Text("Deja Vu Plugin Settings");

  if (ImGui::Button("Export All Data"))
  {
    ExportData();
  }
}

void DejaVu::ExportData()
{
  std::vector<PlayerWithRecords> players_with_records;
  if (!this->dbManager->get_all_players_with_records(players_with_records))
  {
    this->cvarManager->log("Failed to get players from database.");
    return;
  }

  json exportJson = json::object();

  for (const auto &pwr : players_with_records)
  {
    json playerJson;
    playerJson["name"] = pwr.player.name;
    playerJson["met"] = pwr.player.met_count;
    playerJson["note"] = pwr.player.note;
    playerJson["offense_ratio"] = pwr.player.offense_ratio;
    playerJson["defense_ratio"] = pwr.player.defense_ratio;

    json recordsJson = json::object();
    for (const auto &record : pwr.records)
    {
      json recordJson;
      recordJson["with"]["wins"] = record.with_wins;
      recordJson["with"]["losses"] = record.with_losses;
      recordJson["against"]["wins"] = record.against_wins;
      recordJson["against"]["losses"] = record.against_losses;
      recordJson["goals"] = record.goals;
      recordJson["assists"] = record.assists;
      recordJson["shots"] = record.shots;
      recordJson["saves"] = record.saves;
      recordJson["epic_saves"] = record.epic_saves;
      recordJson["clears"] = record.clears;
      recordJson["demos"] = record.demos;
      recordJson["center_balls"] = record.center_balls;
      recordJson["hat_tricks"] = record.hat_tricks;
      recordJson["playmakers"] = record.playmakers;
      recordJson["saviors"] = record.saviors;
      recordsJson[std::to_string(record.playlist_id)] = recordJson;
    }
    playerJson["records"] = recordsJson;
    exportJson[pwr.player.id] = playerJson;
  }

  std::filesystem::path export_path = dataDir / "dejavu_export.json";
  std::ofstream file(export_path);
  if (file.is_open())
  {
    file << exportJson.dump(4); // pretty print with 4 spaces
    file.close();
    this->cvarManager->log("Data exported successfully to dejavu_export.json");
  }
  else
  {
    this->cvarManager->log("Failed to open dejavu_export.json for writing.");
  }
}

void DejaVu::RenderPlaystyleBar(CanvasWrapper canvas, float offenseRatio, Vector2 pos, Vector2 size)
{
  LinearColor offenseColor = {255, 165, 0, 255};
  LinearColor defenseColor = {0, 0, 255, 255};

  float offenseWidth = size.X * offenseRatio;
  float defenseWidth = size.X * (1.0 - offenseRatio);

  // Draw Offense part
  canvas.SetColor(offenseColor);
  canvas.SetPosition(pos);
  canvas.FillBox(Vector2{(int)offenseWidth, size.Y});

  // Draw Defense part
  canvas.SetColor(defenseColor);
  canvas.SetPosition(Vector2{pos.X + (int)offenseWidth, pos.Y});
  canvas.FillBox(Vector2{(int)defenseWidth, size.Y});

  // Draw Text
  canvas.SetColor(*this->textColor);
  std::string offensePercent = std::to_string((int)(offenseRatio * 100));
  std::string defensePercent = std::to_string(100 - (int)(offenseRatio * 100));
  std::string text = offensePercent + "% / " + defensePercent + "%";
  Vector2 textSize = canvas.GetStringSize(text);
  Vector2 textPos = {pos.X + (size.X / 2) - (textSize.X / 2), pos.Y + (size.Y / 2) - (textSize.Y / 2)};
  canvas.SetPosition(textPos);
  canvas.DrawString(text);
}

void DejaVu::RenderDrawable(CanvasWrapper canvas)
{
  if (!Canvas::IsContextSet())
  {
    Canvas::SetContext(canvas);
    MetCountColumnWidth = Canvas::GetStringWidth("9999") + 11;
    RecordColumnWidth = Canvas::GetStringWidth("9999:9999") + 11;
  }
  Canvas::SetGlobalAlpha((char)(*this->alpha * 255));
  Canvas::SetScale(*this->scale);
  bool inGame = IsInRealGame();
  bool noData = this->currentMatchPRIs.size() == 0;
  bool scoreboardIsClosedAndToggleIsOn = *this->toggleWithScoreboard && !this->isScoreboardOpen;
  if ((!*this->enabled || !*this->enabledVisuals || !inGame || noData || scoreboardIsClosedAndToggleIsOn) && !*this->enabledDebug)
    return;
  auto localPlayer = GetLocalPlayerPRI();
  if (localPlayer.IsNull())
    return;

  ServerWrapper server = GetCurrentServer();
  if (server.IsNull())
    return;
  PlaylistID currentPlaylist = static_cast<PlaylistID>(server.GetPlaylist().GetPlaylistId());

  std::vector<std::string> teammates;
  std::vector<std::string> opponents;
  int localPlayerTeam = localPlayer.GetTeamNum();
  for (auto const &[playerID, playerPRI] : this->currentMatchPRIs)
  {
    if (playerPRI.GetTeamNum() == localPlayerTeam)
    {
      teammates.push_back(playerID);
    }
    else
    {
      opponents.push_back(playerID);
    }
  }

  float y_pos = 100;
  const int PLAYER_COL_X = 100;
  const int MATCHES_COL_X = 300;
  const int RECORD_COL_X = 400;
  const int PLAYSTYLE_COL_X = 600;

  auto render_player_row = [&](const std::string &playerID, PlaylistID playlist)
  {
    PlayerData playerData;
    if (!this->dbManager->get_player(playerID, playerData))
    {
      auto pri_it = this->currentMatchPRIs.find(playerID);
      if (pri_it != this->currentMatchPRIs.end() && !pri_it->second.IsNull())
      {
        playerData.name = pri_it->second.GetPlayerName().ToString();
      }
      else
      {
        playerData.name = "Unknown Player";
      }
    }

    int matches = playerData.met_count;
    Record withRecord = GetRecord(playerID, playlist, Side::Same);
    Record againstRecord = GetRecord(playerID, playlist, Side::Other);

    std::string name = playerData.name;
    std::string matchesStr = std::to_string(matches);
    std::string recordStr = std::to_string(withRecord.wins) + "-" + std::to_string(withRecord.losses) + " / " + std::to_string(againstRecord.wins) + "-" + std::to_string(againstRecord.losses);

    Canvas::SetPosition(Vector2{PLAYER_COL_X, (int)y_pos});
    Canvas::DrawString(name);
    Canvas::SetPosition(Vector2{MATCHES_COL_X, (int)y_pos});
    Canvas::DrawString(matchesStr);
    Canvas::SetPosition(Vector2{RECORD_COL_X, (int)y_pos});
    Canvas::DrawString(recordStr);

    RenderPlaystyleBar(canvas, playerData.offense_ratio, Vector2{PLAYSTYLE_COL_X, (int)y_pos}, Vector2{150, 18});

    y_pos += 20;
  };

  auto render_header = [&](float &y_pos_ref)
  {
    Canvas::SetPosition(Vector2{PLAYER_COL_X, (int)y_pos_ref});
    Canvas::DrawString("Player");
    Canvas::SetPosition(Vector2{MATCHES_COL_X, (int)y_pos_ref});
    Canvas::DrawString("Matches");
    Canvas::SetPosition(Vector2{RECORD_COL_X, (int)y_pos_ref});
    Canvas::DrawString("Record (With/Against)");
    Canvas::SetPosition(Vector2{PLAYSTYLE_COL_X, (int)y_pos_ref});
    Canvas::DrawString("Playstyle (Offense/Defense)");
    y_pos_ref += 20;
  };

  // Teammates
  Canvas::SetPosition(Vector2{PLAYER_COL_X, (int)y_pos});
  Canvas::DrawString("My Team");
  y_pos += 20;
  render_header(y_pos);
  for (const auto &playerID : teammates)
  {
    render_player_row(playerID, currentPlaylist);
  }

  y_pos += 20; // spacing

  // Opponents
  Canvas::SetPosition(Vector2{PLAYER_COL_X, (int)y_pos});
  Canvas::DrawString("Opponents");
  y_pos += 20;
  render_header(y_pos);
  for (const auto &playerID : opponents)
  {
    render_player_row(playerID, currentPlaylist);
  }
}

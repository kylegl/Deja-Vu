#include "pch.h"
#include "MatchTracker.h"
#include "vendor/imgui/imgui.h"
#include "vendor/imgui/imgui_stdlib.h"
#undef max

void MatchTracker::Render()
{

  if (!this->isWindowOpen)
  {
    cvarManager->executeCommand("togglemenu " + GetMenuName());

    return;
  }

  if (this->openQuickNote)
  {
#if DEV
    std::set<std::string> matchMetList = {"0"};
    if (this->curMatchGUID.has_value())
      matchMetList = this->matchesMetLists[this->curMatchGUID.value()];
#else
    if (!this->curMatchGUID.has_value())
    {
      LOG(INFO) << "Closing quick note because there is no match guid";
      this->openQuickNote = false;
      return;
    }

    const std::set<std::string> &matchMetList = this->matchesMetLists[this->curMatchGUID.value()];
#endif DEV

    ImGui::OpenPopup("Quick Note");
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 4), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Quick Note"))
    {
      float reserveHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2;
      ImGui::BeginChild("#dejavu_quick_note", ImVec2(0, -reserveHeight));
      ImGui::Columns(2, "Quick Note Edit");
      ImGui::SetColumnWidth(0, std::max(ImGui::GetColumnWidth(0), 200.0f));

      ImGui::Separator();
      ImGui::Text("Name");
      ImGui::NextColumn();
      ImGui::Text("Player Note");
      ImGui::NextColumn();
      ImGui::Separator();

      for (const auto &uniqueID : matchMetList)
      {
        auto &playerData = this->data["players"][uniqueID];
        ImGui::Text(playerData["name"].get<std::string>().c_str());
        ImGui::NextColumn();

        float buttonPos = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x;
        if (!playerData.contains("note"))
          playerData["note"] = "";
        float buttonWidth = 2 * ImGui::GetStyle().FramePadding.x + ImGui::CalcTextSize("Edit").x;
        ImGui::BeginChild((std::string("#note") + uniqueID).c_str(), ImVec2(ImGui::GetColumnWidth() - buttonWidth - 2 * ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextWrapped(playerData["note"].get<std::string>().c_str());
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::SetCursorPosX(buttonPos - buttonWidth);
        if (ImGui::Button((std::string("Edit##") + uniqueID).c_str(), ImVec2(0, ImGui::GetFrameHeightWithSpacing())))
        {
          ImGui::OpenPopup("Edit note");
          this->playersNoteToEdit = uniqueID;
        }

        ImGui::NextColumn();
      }

      RenderEditNoteModal();
      ImGui::EndChild();

      int escIdx = ImGui::GetIO().KeyMap[ImGuiKey_Escape];
      if (ImGui::Button("Close", ImVec2(120, 0)) || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && escIdx >= 0 && ImGui::IsKeyPressed(escIdx)))
      {
        this->openQuickNote = false;
        ImGui::CloseCurrentPopup();
        CloseMenu();
      }

      ImGui::EndPopup();
    }

    this->shouldBlockInput = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    return;
  }

  // ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
  // ImGui::SetNextWindowSize(ImVec2(256, 384), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(55 + 250 + 55 + 250 + 80 + 100, 600), ImVec2(FLT_MAX, FLT_MAX));

  ImGuiWindowFlags windowFlags = 0 | ImGuiWindowFlags_MenuBar
      //| ImGuiWindowFlags_NoCollapse
      ;

  if (!ImGui::Begin(GetMenuTitle().c_str(), &this->isWindowOpen, windowFlags))
  {
    // Early out if the window is collapsed, as an optimization.
    this->shouldBlockInput = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    ImGui::End();
    return;
  }

  // ImGui::SetNextWindowSizeConstraints(ImVec2(55 + 250 + 55 + 250 + 80 + 100, 600), ImVec2(FLT_MAX, FLT_MAX));
  // ImGui::Begin(menuTitle.c_str(), &isWindowOpen, ImGuiWindowFlags_NoCollapse);
  // const char* playlists[] = {"asdf", "fff"};

  static const char *items[]{"Apple", "Banana", "Orange"};

  static PlaylistID selectedPlaylist = PlaylistID::ANY;

  // ImGui::ListBox("Fruit", &this->gui_selectedPlaylist, this->playlists, 2);
  // ImGui::ListBox("Playlist Filter", &this->gui_selectedPlaylist, this->playlists, IM_ARRAYSIZE(this->playlists));
  // ImGui::Combo("Playlist Filter", &this->gui_selectedPlaylist, this->playlists, IM_ARRAYSIZE(this->playlists));

  const char *preview = (selectedPlaylist != PlaylistID::ANY) ? PlaylistNames[selectedPlaylist].c_str() : "Select...";

  if (ImGui::BeginCombo("Playlist Filter", preview))
  {
    for (auto it = PlaylistNames.begin(); it != PlaylistNames.end(); ++it)
    {
      bool isSelected = selectedPlaylist == it->first;
      if (ImGui::Selectable(it->second.c_str(), isSelected))
        selectedPlaylist = it->first;
      if (isSelected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  static char nameFilter[65] = "";
  ImGui::InputText("Name", nameFilter, IM_ARRAYSIZE(nameFilter), ImGuiInputTextFlags_AutoSelectAll);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild("#MatchTrackerDataDisplay", ImVec2(0, ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2 + ImGui::GetStyle().FramePadding.y), false, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding);

  // ImGui::ListBoxHeader()
  ImGui::Columns(5, "dejavu_stats");
  // ImGui::SetColumnWidth(0, 55);
  // ImGui::SetColumnWidth(1, 250);
  // ImGui::SetColumnWidth(2, 55);
  // ImGui::SetColumnWidth(3, 250);
  // ImGui::SetColumnWidth(4, 55);
  ImGui::Separator();
  ImGui::Text("Name");
  ImGui::NextColumn();
  ImGui::Text("Met Count");
  ImGui::NextColumn();
  ImGui::Text("Total Record With");
  ImGui::NextColumn();
  ImGui::Text("Total Record Against");
  ImGui::NextColumn();
  ImGui::Text("Player Note");
  ImGui::NextColumn();
  ImGui::Separator();
  ImGui::EndChild();

  // ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y);
  ImGui::BeginChild("#MatchTrackerDataDisplayBody", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding);
  ImGui::Columns(5, "dejavu_stats_body");
  ImGui::Separator();

  std::string selectedPlaylistIDStr = std::to_string(static_cast<int>(selectedPlaylist));

  auto nameFilterView = std::string_view(nameFilter);

  int i = 0;
  this->playerIDsToDisplay.resize(this->data["players"].size());
  for (auto &player : this->data["players"].items())
  {
    if (selectedPlaylist != PlaylistID::ANY && (!player.value()["playlistData"].contains(selectedPlaylistIDStr) || player.value()["playlistData"][selectedPlaylistIDStr]["records"].is_null()))
      continue;
    // Default to player id
    std::string name = player.key();
    try
    {
      if (!player.value().count("name") || player.value()["name"].is_null())
        player.value()["name"] = player.key();
      name = player.value()["name"].get<std::string>();
    }
    catch (const std::exception &e)
    {
      this->gameWrapper->Toast("MatchTracker Error", "Check console/log for details");
      this->cvarManager->log(e.what());
      LOG(ERROR) << e.what();
      continue;
    }
    bool nameFound = std::search(name.begin(), name.end(), nameFilterView.begin(), nameFilterView.end(), [](char ch1, char ch2)
                                 { return std::toupper(ch1) == std::toupper(ch2); }) == name.end();
    if (nameFilterView.size() > 0 && nameFound)
      continue;
    this->playerIDsToDisplay[i++] = player.key();
  }
  this->playerIDsToDisplay.resize(i);
  ImGuiListClipper clipper((int)this->playerIDsToDisplay.size());
  while (clipper.Step())
  {
    try
    {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
      {
        std::string uniqueID = this->playerIDsToDisplay[i];
        auto &playerData = this->data["players"][uniqueID];

        int metCount = 0;
        if (!playerData.count("metCount") || playerData["metCount"].is_null())
          playerData["metCount"] = 1;
        metCount = playerData["metCount"].get<int>();
        std::string name = uniqueID;
        if (!playerData.count("name") || playerData["name"].is_null())
          playerData["name"] = uniqueID;
        name = playerData["name"].get<std::string>();

        auto sameRecord = GetRecord(uniqueID, selectedPlaylist, Side::Same);
        auto otherRecord = GetRecord(uniqueID, selectedPlaylist, Side::Other);

        ImGui::Text(name.c_str());
        ImGui::NextColumn();
        ImGui::Text(std::to_string(metCount).c_str());
        ImGui::NextColumn();
        std::ostringstream sameRecordFormatted;
        sameRecordFormatted << sameRecord.wins << ":" << sameRecord.losses;
        ImGui::Text(sameRecordFormatted.str().c_str());
        ImGui::NextColumn();
        std::ostringstream otherRecordFormatted;
        otherRecordFormatted << otherRecord.wins << ":" << otherRecord.losses;
        ImGui::Text(otherRecordFormatted.str().c_str());
        ImGui::NextColumn();

        float buttonPos = ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x;
        if (!playerData.contains("note"))
          playerData["note"] = "";
        float buttonWidth = 2 * ImGui::GetStyle().FramePadding.x + ImGui::CalcTextSize("Edit").x;
        ImGui::BeginChild((std::string("#note") + uniqueID).c_str(), ImVec2(ImGui::GetColumnWidth() - buttonWidth - 2 * ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextWrapped(playerData["note"].get<std::string>().c_str());
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::SetCursorPosX(buttonPos - buttonWidth);
        if (ImGui::Button((std::string("Edit##") + uniqueID).c_str(), ImVec2(0, ImGui::GetFrameHeightWithSpacing())))
        {
          ImGui::OpenPopup("Edit note");
          this->playersNoteToEdit = uniqueID;
        }
        ImGui::NextColumn();
        ImGui::Separator();
      }
    }
    catch (const std::exception &e)
    {
      this->gameWrapper->Toast("MatchTracker Error", "Check console/log for details");
      this->cvarManager->log(e.what());
      LOG(ERROR) << e.what();
      continue;
    }
  }
  clipper.End();

  RenderEditNoteModal();

  // if (ImGui::BeginMenuBar())
  //{
  //	if (ImGui::BeginMenu("File"))
  //	{
  //		if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
  //		if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
  //		if (ImGui::MenuItem("Close", "Ctrl+W")) { this->isWindowOpen = false; }
  //		ImGui::EndMenu();
  //	}
  //	ImGui::EndMenuBar();
  // }
  ImGui::PopStyleVar();
  ImGui::EndChild();

  this->shouldBlockInput = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
  ImGui::End();
}

void MatchTracker::RenderEditNoteModal()
{
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 3, ImGui::GetIO().DisplaySize.y / 3), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Edit note"))
  {
    if (this->playersNoteToEdit.empty())
    {
      ImGui::CloseCurrentPopup();
    }
    else
    {
      json::value_type playerData = this->data["players"][this->playersNoteToEdit];
      if (!playerData.contains("note"))
        playerData["note"] = "";
      auto playerNote = playerData["note"].get_ptr<std::string *>();

      ImVec2 textSize(
          ImGui::GetWindowWidth() - 2 * ImGui::GetStyle().WindowPadding.x,
          ImGui::GetWindowHeight() - 2 * ImGui::GetStyle().WindowPadding.y - ImGui::GetTextLineHeightWithSpacing() - 4 * ImGui::GetStyle().FramePadding.y - 4 * ImGui::GetStyle().ItemSpacing.y);
      if (ImGui::InputTextMultiline("##note", playerNote, textSize))
      {
        this->data["players"][this->playersNoteToEdit]["note"] = *playerNote;
        auto blueMember = std::find_if(this->blueTeamRenderData.begin(), this->blueTeamRenderData.end(), [this](const RenderData &renderData)
                                       { return renderData.id == this->playersNoteToEdit; });
        if (blueMember != this->blueTeamRenderData.end())
          (*blueMember).note = *playerNote;
        else
        {
          auto orangeMember = std::find_if(this->orangeTeamRenderData.begin(), this->orangeTeamRenderData.end(), [this](const RenderData &renderData)
                                           { return renderData.id == this->playersNoteToEdit; });

          if (orangeMember != this->orangeTeamRenderData.end())
            (*orangeMember).note = *playerNote;
        }
      }
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

      int escIdx = ImGui::GetIO().KeyMap[ImGuiKey_Escape];
      if (ImGui::Button("OK", ImVec2(120, 0)) || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && escIdx >= 0 && ImGui::IsKeyPressed(escIdx)))
      {
        ImGui::CloseCurrentPopup();
        WriteData();
      }
    }

    ImGui::EndPopup();
  }
}

std::string MatchTracker::GetMenuName()
{
  return "dejavu";
}

std::string MatchTracker::GetMenuTitle()
{
  return this->menuTitle;
}

void MatchTracker::SetImGuiContext(uintptr_t ctx)
{
  ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
  ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
}

bool MatchTracker::ShouldBlockInput()
{
  return this->shouldBlockInput;
}

bool MatchTracker::IsActiveOverlay()
{
  return true;
}

void MatchTracker::LaunchQuickNoteModal()
{
  this->openQuickNote = true;
  if (!this->isWindowOpen)
    cvarManager->executeCommand("togglemenu " + GetMenuName());
}

void MatchTracker::OnOpen()
{
  this->isWindowOpen = true;
  this->cvarManager->getCvar("cl_dejavu_log").setValue(true);
  this->playerIDsToDisplay.resize(this->data["players"].size());
}

void MatchTracker::OnClose()
{
  this->isWindowOpen = false;
  WriteData();
}

void MatchTracker::OpenMenu()
{
  if (!this->isWindowOpen)
    ToggleMenu();
}

void MatchTracker::CloseMenu()
{
  if (this->isWindowOpen)
    ToggleMenu();
}

void MatchTracker::ToggleMenu()
{
  cvarManager->executeCommand("togglemenu " + GetMenuName());
}

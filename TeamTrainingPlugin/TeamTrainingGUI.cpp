#include "TeamTrainingPlugin.h"

#include "imgui/imgui_custom_widgets.h"

#include <algorithm>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include <boost/locale.hpp>

// WARNING: It gets really ugly below...

// TODO: Add commenting and ratings
// TODO: Specify 0 = none.
// TODO: Sorting methods.

namespace fs = std::filesystem;
using namespace std;

// TODO: Use this for later update to convert all training pack related data
std::string iso_8859_1_to_utf8(std::string str)
{
	string strOut;
	for (std::string::iterator it = str.begin(); it != str.end(); ++it)
	{
		uint8_t ch = *it;
		if (ch < 0x80) {
			strOut.push_back(ch);
		}
		else {
			strOut.push_back(0xc0 | ch >> 6);
			strOut.push_back(0x80 | (ch & 0x3f));
		}
	}
	return strOut;
}

int filenameFilter(ImGuiTextEditCallbackData* data) {
	switch (data->EventChar) {
	case ' ': case '"': case '*': case '<': case'>': case '?': case '\\': case '|': case '/': case ':':
		return 1;
	default:
		return 0;
	}
}

void copyToClipboard(string data)
{
	ImGui::SetClipboardText(data.c_str());
}

bool isValidPackCode(string code)
{
	if (code.length() != 19) return false;
	vector<string> sections;
	boost::split(sections, code, boost::is_any_of("-"));
	if (sections.size() != 4) return false;
	for (auto& section : sections) {
		if (section.size() != 4) return false;
	}
	return true;
}

void TeamTrainingPlugin::Render()
{
	if (!this->isWindowOpen) {
		cvarManager->executeCommand("togglemenu " + GetMenuName());
		return;
	}

	ImGuiWindowFlags windowFlags = 0; //| ImGuiWindowFlags_MenuBar;

	ImGui::SetNextWindowSizeConstraints(ImVec2(55 + 250 + 55 + 250 + 80 + 100, 600), ImVec2(FLT_MAX, FLT_MAX));
	if (!ImGui::Begin(GetMenuTitle().c_str(), &this->isWindowOpen, windowFlags)) {
		// Early out if the window is collapsed, as an optimization
		this->shouldBlockInput = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
		ImGui::End();
		return;
	}
	
	if (ImGui::BeginTabBar("Team Training", ImGuiTabBarFlags_None)) {
		if (ImGui::BeginTabItem("Selection")) {
			ImGui::TextWrapped("For multiplayer training packs, be sure to start a multiplayer freeplay session via Rocket Plugin before loading a pack.");

			// This must be done before calling AddSearchFilters
			if (packs.size() == 0) {
				packs = getTrainingPacks();
				filterLocalPacks(localFilterState.filters);
			}

			AddSearchFilters(localFilterState.filters, "Selection", false,
				std::bind(&TeamTrainingPlugin::loadLocalTagsThread, this, std::placeholders::_1),
				std::bind(&TeamTrainingPlugin::filterLocalPacks, this, std::placeholders::_1));

			if (errorMsgs["Selection"].size() > 0) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				for (auto err : errorMsgs["Selection"]) {
					ImGui::TextWrapped(err.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::Separator();
			}

			if (ImGui::Button("Reload Packs")) {
				packs.clear();
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Favorited Training Packs")) {
				ImGui::OpenPopup("Add Favorited Packs");
				cvarManager->log("opening favorited packs");
			}
			ImGui::SameLine();
			ImGui::PushItemWidth(200);
			ImGui::InputText("Code##AddByCode", addByCode, IM_ARRAYSIZE(addByCode));
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("Add By Code")) {
				AddPackByCode(addByCode);
			}
			ShowFavoritedPacksWindow();
			ShowAddByCodeModal();

			if (packs.size() == 0) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				if (packDataPath.length() == 0) {
					packDataPath = getPackDataPath("").string();
				}
				ImGui::TextWrapped("No training packs exist in %s", packDataPath.c_str());
				ImGui::TextWrapped("Plugin comes packaged with 3 training packs.");
				ImGui::TextWrapped("Try reinstalling or unzipping the training packs from the Team Training plugin download on bakkesplugins.com.");
				ImGui::PopStyleColor();
			}
			else if (filteredPacks.size() == 0) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				ImGui::TextWrapped("No training packs found with the given filter options");
				ImGui::PopStyleColor();
			}
			else {
				// Left Selection
				ImGui::BeginChild("left pane", ImVec2(300, 0), true);
				int i = 0;
				for (auto pack : filteredPacks) {
					if (pack.errorMsg != "") {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
					}
					if (ImGui::Selectable((pack.description + "##Selection" + to_string(i)).c_str(), selectedPackIdx == i)) {
						selectedPackIdx = i;
					}
					if (pack.errorMsg != "") {
						ImGui::PopStyleColor();
					}
					if (selectedPackIdx == i) {
						ImGui::SetItemDefaultFocus();
					}
					i++;
				}
				ImGui::EndChild();
				ImGui::SameLine();

				// Right Details
				TrainingPack pack = filteredPacks[selectedPackIdx];
				ImGui::BeginGroup();

				if (pack.errorMsg == "") {
					if (pack.drills.size() > 0) {
						if (ImGui::Button("Load Team Training Pack")) {
							errorMsgs["Selection"].clear();

							auto server = gameWrapper->GetGameEventAsServer();
							if (!isValidServer(server)) {
								errorMsgs["Selection"].push_back("You must be the host in a freeplay session or LAN match to load a training pack. Use Rocket plugin to launch a multiplayer session with non-local players.");
							}
							else {
								auto cars = server.GetCars();
								if (cars.Count() < pack.offense) {
									errorMsgs["Selection"].push_back("Pack requires " + std::to_string(pack.offense) + " players but there are only " + std::to_string(cars.Count()) + " in the lobby.");
								}
							}

							if (errorMsgs["Selection"].size() == 0) {
								cvarManager->executeCommand("sleep 1; team_train_load " + pack.filepath.filename().string());
							}
						}
					}

					if (pack.code != "") {
						if (pack.drills.size() > 0) {
							ImGui::SameLine();
						}
						if (ImGui::Button("Load Custom Training Pack")) {
							cvarManager->executeCommand("sleep 1; load_training " + pack.code);
						}
					}

					ImGui::BeginChild("Training Pack Details", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

					ImGui::Text(pack.description.c_str());
					ImGui::Separator();
					ImGui::Text("Creator: %s", pack.creator.c_str());
					ImGui::Text("Description: %s", pack.description.c_str());
					ImGui::Text("Offensive Players: %d", pack.offense);
					ImGui::Text("Defensive Players: %d", pack.defense);
					ImGui::Text("Drills: %d", pack.numDrills);
					ImGui::TextWrapped("Code: %s", pack.code.c_str());
					if (!pack.notes.empty()) ImGui::TextWrapped("Notes: %s", pack.notes.c_str());
					if (!pack.youtube.empty()) {
						ImGui::TextWrapped("Youtube: %s", pack.youtube.c_str());
						ImGui::SameLine();
						if (ImGui::Button("Copy to Clipboard")) {
							copyToClipboard(pack.youtube);
						}
					}
					ImGui::TextWrapped("Filepath: %s", pack.filepath.string().c_str());
					if (ImGui::Button("Edit Tags##Selection")) {
						if (!localEditTagsState.has_downloaded) {
							localEditTagsState.enableTagsPending(pack.tags);
							boost::thread t{ &TeamTrainingPlugin::loadAllTagsThread, this, boost::ref(localEditTagsState) };
						}
						else {
							localEditTagsState.unmarkTags();
							for (auto& tag : pack.tags) {
								localEditTagsState.tags[tag] = true;
							}
						}

						tagEditingPack = pack;
						ImGui::OpenPopup("Edit Tags");
					}
					ImGui::SameLine();
					ImGui::TextWrapped("Tags: %s", boost::algorithm::join(pack.tags, ", ").c_str());

					ShowTagsWindow(localEditTagsState, true, std::bind(&TeamTrainingPlugin::loadAllTagsThread, this, placeholders::_1),
						[this](TagsState& state) {
							vector<string> newTags = state.GetEnabledTags();
							bool tagsChanged = tagEditingPack.setTags(newTags);
							if (tagsChanged) {
								tagEditingPack.save();
								localFilterState.filters.tagsState.addTags(newTags);
								packs.clear();
							}
						});

					ImGui::Separator();

					if (pack.uploadID == NO_UPLOAD_ID && pack.drills.size() > 0) { // Don't allow uploading of custom training metadata this way
						if (ImGui::Button("Upload")) {
							UploadPack(pack);
						}
						ShowUploadingModal();
						ImGui::SameLine();
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
						ImGui::TextWrapped("Warning: Double check your tags, description, etc as you cannot currently upload updated versions of training packs.");
						ImGui::PopStyleColor();
					}

					ImGui::EndChild();
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
					ImGui::Text(pack.description.c_str());
					ImGui::Separator();
					ImGui::TextWrapped("Failed to load pack: %s", pack.errorMsg.c_str());
					ImGui::TextWrapped("If you converted this pack yourself, please try again.");
					ImGui::TextWrapped("If that does not work, please report this bug to daftpenguinrl@gmail.com, @PenguinDaft on Twitter, DaftPenguin#5103 on Discord, or report issue on http://github.com/daftpenguin/teamtrainingplugin.");
					ImGui::TextWrapped("Please include the custom training pack code with your report and the json file at the location: %s.", pack.filepath.c_str());
					ImGui::PopStyleColor();
				}

				ImGui::SameLine();
				ImGui::EndGroup();
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Download")) {
			AddSearchFilters(searchState.filters, "Downloads", true,
				std::bind(&TeamTrainingPlugin::downloadTagsThread, this, std::placeholders::_1),
				std::bind(&TeamTrainingPlugin::SearchPacks, this, std::placeholders::_1));

			if (searchState.is_searching) {
				ImGui::SetCursorPos((ImGui::GetWindowSize() - ImVec2(30, 30)) * 0.5f);
				ImGui::Spinner("Searching...", 30, 5);
			}
			else if (searchState.failed) {
				ImGui::TextWrapped("Search failed");
				if (searchState.error != "") {
					ImGui::TextWrapped(searchState.error.c_str());
				}
			}
			else if (searchState.packs.size() == 0) {
				ImGui::TextWrapped("No packs found");
			}
			else {
				// Left Selection
				ImGui::BeginChild("left pane", ImVec2(300, 0), true);
				int i = 0;
				for (auto pack : searchState.packs) {
					if (ImGui::Selectable((pack.description + "##" + to_string(i)).c_str(), downloadSelectedPackIdx == i)) {
						downloadSelectedPackIdx = i;
					}
					if (downloadSelectedPackIdx == i) {
						ImGui::SetItemDefaultFocus();
					}
					i++;
				}
				ImGui::EndChild();
				ImGui::SameLine();

				// Right Details
				TrainingPackDBMetaData pack = searchState.packs[downloadSelectedPackIdx];
				ImGui::BeginGroup();

				if (ImGui::Button("Download##BySearch")) {
					cvarManager->log("Downloading pack with ID: " + pack.id);
					DownloadPack(pack);
				}
				if (pack.code != "") {
					ImGui::SameLine();
					if (ImGui::Button("Load Custom Training Pack")) {
						cvarManager->executeCommand("sleep 1; load_training " + string(pack.code));
					}
				}
				ShowDownloadingModal();

				ImGui::BeginChild("Training Pack Details", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

				ImGui::Text(pack.description.c_str());
				ImGui::Separator();
				ImGui::Text("Creator: %s", pack.creator.c_str());
				ImGui::Text("Offensive Players: %d", pack.offense);
				ImGui::Text("Defensive Players: %d", pack.defense);
				ImGui::Text("Drills: %d", pack.num_drills);
				ImGui::Text("Code: %s", pack.code.c_str());
				ImGui::TextWrapped("Tags: %s", boost::algorithm::join(pack.tags, ", ").c_str());
				ImGui::Text("Downloads: %d", pack.downloads);

				ImGui::EndChild();

				ImGui::SameLine();
				ImGui::EndGroup();
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Player Roles")) {
			if (!this->pack) {
				ImGui::Text("No pack loaded");
			} else if (!isValidServer()) {
				ImGui::Text("Must be host of a freeplay session or LAN match");
			} else {
				ImGui::TextWrapped("Drag a player's name onto another player's name to swap their roles.");
				ImGui::TextWrapped("Player role assignments are based on their order as they are \"seen\" by the server.");
				ImGui::TextWrapped("Different events may change their ordering, like players getting demolished or disconnecting then reconnecting to the server.");
				ImGui::TextWrapped("I may or may not fix this in a future update.");

				if (errorMsgs["Roles"].size() > 0) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
					for (auto err : errorMsgs["Roles"]) {
						ImGui::TextWrapped(err.c_str());
					}
					ImGui::PopStyleColor();
					ImGui::Separator();
				}

				auto server = gameWrapper->GetGameEventAsServer();
				auto cars = server.GetCars();

				ImGuiDragDropFlags src_drag_flags = 0;// | ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers | ImGuiDragDropFlags_SourceNoPreviewTooltip;
				ImGuiDragDropFlags tgt_drag_flags = 0;// | ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

				for (int i = 0; i < player_order.size(); i++) {
					ImGui::Button((cars.Get(player_order[i]).GetPRI().GetPlayerName().ToString()).c_str());

					if (ImGui::BeginDragDropSource(src_drag_flags)) {
						ImGui::SetDragDropPayload("PLAYER_ROLE_POSITION", &i, sizeof(int), ImGuiCond_Once);
						ImGui::Button(cars.Get(player_order[i]).GetPRI().GetPlayerName().ToString().c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLAYER_ROLE_POSITION", tgt_drag_flags)) {
							swap(player_order[i], player_order[*(const int*)payload->Data]);
						}
						ImGui::EndDragDropTarget();
					}

					ImGui::SameLine();
					if (this->pack->offense > 0 && i == 0) {
						ImGui::Text("Shooter");
					} else if (i < this->pack->offense) {
						ImGui::Text("Passer");
					} else if (i < (this->pack->offense + this->pack->defense)) {
						ImGui::Text("Defender");
					} else {
						ImGui::Text("Specatator");
					}
				}

				ImGui::Separator();
				ImGui::Text("Reset shot to use new order");
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Creation")) {

			if (errorMsgs["Creation"].size() > 0) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				for (auto err : errorMsgs["Creation"]) {
					ImGui::TextWrapped(err.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::Separator();
			}

			if (ImGui::BeginTabBar("Creation Options", ImGuiTabBarFlags_None)) {
				if (ImGui::BeginTabItem("From Training Pack")) {

					ImGui::TextWrapped("Team training packs can be generated from the single player custom training packs by separating each player's position into individual drills.");
					ImGui::TextWrapped("This will NOT work out for any random custom training pack, you must design the pack in a specific way for this to conversion to work.");
					ImGui::TextWrapped("Enter the number of offensive and defensive players below, and the ordering of each drill to the corresponding player position will show.");
					ImGui::TextWrapped("Using the custom training pack creator within the game, setup each drill using this pattern.");
					ImGui::TextWrapped("After creating the custom training pack, load the pack up in game, fill in all of the details below, then click the convert button.");
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
					ImGui::TextWrapped("Turn off BakkesMod's playlist shuffling, mirroring, and custom training variance before doing conversion.");
					ImGui::TextWrapped("Number of drills is the total number of drills that will be in the final team training pack.");
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::TextWrapped("For example, convert WayProtein's training pack by loading it with the button below, enter 2 offensive players, 0 defensive players, and 8 drills, then click convert.");
					if (ImGui::Button("Load WayProtein's Passing (downfield left)")) {
						cvarManager->executeCommand("sleep 1; load_training C833-6A35-A46A-7191");
					}
					ImGui::Separator();

					if (gameWrapper->IsInCustomTraining()) {
						if (!inGameTrainingPackData.failed && ::strcmp(inGameTrainingPackData.code, code) != 0) {
							::strncpy(filename, inGameTrainingPackData.code, sizeof(code));
							::strncpy(code, inGameTrainingPackData.code, sizeof(code));
							::strncpy(creatorID, inGameTrainingPackData.creatorID, sizeof(creatorID));
							gui_num_drills = inGameTrainingPackData.numDrills;
							offensive_players = 1;
							defensive_players = 0;
							enabledTags.clear();

							::strncpy(creator, iso_8859_1_to_utf8(inGameTrainingPackData.creator).c_str(), sizeof(creator));
							::strncpy(description, iso_8859_1_to_utf8(inGameTrainingPackData.description).c_str(), sizeof(description));
						}
					}

					if (ImGui::InputInt("Offensive players", &offensive_players, ImGuiInputTextFlags_CharsDecimal)) {
						offensive_players = (offensive_players < 0) ? 0 : offensive_players;
						gui_num_drills = inGameTrainingPackData.fixGUIDrills(offensive_players, defensive_players, gui_num_drills);
					}
					if (ImGui::InputInt("Defensive players", &defensive_players, ImGuiInputTextFlags_CharsDecimal)) {
						defensive_players = (defensive_players < 0) ? 0 : defensive_players;
						gui_num_drills = inGameTrainingPackData.fixGUIDrills(offensive_players, defensive_players, gui_num_drills);
					}
					if (ImGui::InputInt("Number of drills", &gui_num_drills, ImGuiInputTextFlags_CharsDecimal)) {
						gui_num_drills = (gui_num_drills < 1) ? 1 : gui_num_drills;
					}

					ImGui::InputText("Filename (no spaces or extensions)", filename, IM_ARRAYSIZE(filename), ImGuiInputTextFlags_CallbackCharFilter, filenameFilter);
					ImGui::InputText("Creator", creator, IM_ARRAYSIZE(creator), ImGuiInputTextFlags_None);
					ImGui::InputText("Description", description, IM_ARRAYSIZE(description), ImGuiInputTextFlags_None);
					ImGui::InputTextMultiline("Creator Notes", creatorNotes, IM_ARRAYSIZE(creatorNotes));
					ImGui::InputText("Youtube Link", youtubeLink, IM_ARRAYSIZE(youtubeLink), ImGuiInputTextFlags_None);

					if (ImGui::Button("Edit Tags##Creation")) {
						if (!localEditTagsState.has_downloaded) {
							localEditTagsState.enableTagsPending(enabledTags);
							boost::thread t{ &TeamTrainingPlugin::loadAllTagsThread, this, boost::ref(localEditTagsState) };
						}
						else {
							localEditTagsState.unmarkTags();
							for (auto& tag : enabledTags) {
								localEditTagsState.tags[tag] = true;
							}
						}

						ImGui::OpenPopup("Edit Tags");
					}
					ImGui::SameLine();
					ImGui::TextWrapped("Tags: %s", boost::join(enabledTags, ", ").c_str());

					ShowTagsWindow(localEditTagsState, true, std::bind(&TeamTrainingPlugin::loadAllTagsThread, this, placeholders::_1),
						[this](TagsState& state) {
							enabledTags.clear();
							enabledTags = state.GetEnabledTags();
						});

					ImGui::Text("Code: %s", code);
					ImGui::Text("CreatorID: %s", creatorID);

					if (ImGui::Button("Convert")) {
						errorMsgs["Creation"].clear();
						if (!gameWrapper->IsInCustomTraining()) {
							errorMsgs["Creation"].push_back("Must be in custom training to do conversion.");
						}
						if (offensive_players + defensive_players == 0) {
							errorMsgs["Creation"].push_back("Must have at least one player.");
						}
						else if (offensive_players == 0) {
							errorMsgs["Creation"].push_back("Must have at least on offensive player. Defense only packs are not supported at this time.");
						}
						if (filename[0] == '\0') {
							errorMsgs["Creation"].push_back("Must specify a filename.");
						}
						if (!inGameTrainingPackData.failed && (offensive_players + defensive_players) * gui_num_drills > inGameTrainingPackData.numDrills) {
							errorMsgs["Creation"].push_back("Not enough shots in the custom training pack to support that many team training drills: \
								(offense + defense) * number of drills must be less than or equal to " + to_string(inGameTrainingPackData.numDrills) + ".");
						}

						OnClose();

						gameWrapper->Execute([this](GameWrapper* gw) {
							if (!gameWrapper->IsInCustomTraining()) {
								cvarManager->log("Not in custom training");
								return;
							}

							unordered_set<string> tags;
							for (auto it = enabledTags.begin(); it != enabledTags.end(); ++it) {
								tags.insert(*it);
							}

							conversion_pack = TrainingPack(getPackDataPath(filename), offensive_players,
								defensive_players, gui_num_drills, code, creator, creatorID,
								description, creatorNotes, youtubeLink, &tags);
							conversion_pack.startNewDrill();
							getNextShotCalled = 0;
							getNextShot();
							});
					}

					ImGui::Separator();

					ImGui::Text(("Drill order (repeat " + to_string(gui_num_drills) + " times):").c_str());
					if (offensive_players > 0) {
						ImGui::Text("Shooter");
					}
					for (int i = 0; i < offensive_players - 1; i++) {
						if (i == offensive_players - 1) {
							ImGui::Text("Passer (and starting ball position/trajectory)");
						}
						else {
							ImGui::Text("Passer");
						}
					}
					for (int i = 0; i < defensive_players; i++) {
						ImGui::Text("Defender");
					}

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("From Replay")) {

					if (packs.size() == 0) {
						packs = getTrainingPacks();
						filterLocalPacks(localFilterState.filters);
					}

					// TODO: Record frame by frame data for x seconds up to and including current frame.
					//		 Determine some way to serialize this data and record it into json file. Just do it as json? Pack data as binary and base64?

					ImGui::TextWrapped("Load a replay and pause playback of the replay in the spot you want to create a drill from");

					ImGui::Separator();

					ImGui::Text("Player Role Assignments");

					ImGui::Text("Offensive Team:");
					ImGui::SameLine();
					ImGui::RadioButton("Blue", &offensiveTeam, 0);
					ImGui::SameLine();
					ImGui::RadioButton("Orange", &offensiveTeam, 1);

					if (gameWrapper->IsInReplay() && replayData.dataSet && replayData.errorMsg.empty()) {
						auto offense = offensiveTeam == 0 ? replayData.team0 : replayData.team1;
						auto defense = offensiveTeam == 0 ? replayData.team1 : replayData.team0;
						ImGui::Text("Offensive Players:");
						ImGui::Indent(20);
						for (auto player : offense) {
							if (unassignedPlayers.find(player.id) != unassignedPlayers.end())
								continue;
							ImGui::Text("Player: %s", player.name.c_str());
							ImGui::SameLine();
							if (ImGui::Button(string("Unassign##" + player.id).c_str())) {
								unassignedPlayers[player.id] = player;
							}
						}
						ImGui::Unindent();
						ImGui::Text("Defensive Players:");
						ImGui::Indent(20);
						for (auto player : defense) {
							if (unassignedPlayers.find(player.id) != unassignedPlayers.end())
								continue;
							ImGui::Text("Player: %s", player.name.c_str());
							ImGui::SameLine();
							if (ImGui::Button(string("Unassign##Defence" + player.id).c_str())) {
								unassignedPlayers[player.id] = player;
							}
						}
						ImGui::Unindent();
						if (unassignedPlayers.size() > 0) {
							ImGui::Text("Unassigned:");
							ImGui::Indent(20);
							auto it = unassignedPlayers.begin();
							while (it != unassignedPlayers.end()) {
								ImGui::Text("Player: %s", it->second.name);
								ImGui::SameLine();
								if (ImGui::Button(string("Reassign##" + it->second.id).c_str())) {
									it = unassignedPlayers.erase(it);
								}
								else {
									++it;
								}
							}
							ImGui::Unindent();
						}
					}
					else {
						ImGui::TextWrapped("Either no replay is currently loaded, or the plugin failed to retrieve the replay data.");
						if (!replayData.errorMsg.empty()) {
							ImGui::TextWrapped("An error occurred while trying to load replay data: %s", replayData.errorMsg.c_str());
						}
					}

					if (ImGui::Button("Force Reload Replay Data")) {
						gameWrapper->Execute([this](GameWrapper* gw) { onReplayLoaded("forced"); });
					}

					ImGui::Separator();

					ImGui::Checkbox("Append to existing training pack", &appendToExistingPack);
					if (appendToExistingPack) {
						ImGui::TextWrapped("In order to append to an existing training pack, the training pack's number of offensive players must be equal to the number of offensive players you're recording from this replay through the role assignments below.");
						ImGui::TextWrapped("You can only append to training packs that have already been converted to the plugin's training pack format (only those packs are listed below). These packs will show the \"Load Team Training pack\" button when selected in the selection tab. If you'd like to append this to a pack that isn't listed below, you will need to convert it to the plugin's training pack format first using the \"From Training Pack\" tab.");
						ImGui::Separator();

						ImGui::BeginChild("left pane", ImVec2(300, 200), true);

						int i = 0;
						for (auto pack : packsForAppend) {
							if (ImGui::Selectable((pack->description + "##" + to_string(i) + "Append").c_str(), createFromReplaySelectedPackIdx == i)) {
								createFromReplaySelectedPackIdx = i;
							}
							if (createFromReplaySelectedPackIdx == i) {
								ImGui::SetItemDefaultFocus();
							}
							i++;
						}
						ImGui::EndChild();
						ImGui::SameLine();

						/*if (createFromReplaySelectedPackIdx >= cachedPackNamesForAppend.size()) createFromReplaySelectedPackIdx = 0;
						ImGui::ListBox("Pack to append to", &createFromReplaySelectedPackIdx, &cachedPackNamesForAppend[0], cachedPackNamesForAppend.size());

						ImGui::EndChild();
						ImGui::SameLine();*/

						ImGui::BeginGroup();
						TrainingPack* selectedPack = packsForAppend[createFromReplaySelectedPackIdx];
						ImGui::Text("Pack Details:");
						ImGui::TextWrapped(("Creator: " + selectedPack->creator).c_str());
						ImGui::TextWrapped(("Offense: " + to_string(selectedPack->offense)).c_str());
						ImGui::TextWrapped(("Defense: " + to_string(selectedPack->defense)).c_str());
						ImGui::TextWrapped(("Drills: " + to_string(selectedPack->drills.size())).c_str());

						ImGui::SameLine();
						ImGui::EndGroup();

						ImGui::Separator();
					} else {
						// TODO: CreatorID
						ImGui::InputText("Filename (no spaces or extensions)", filename, IM_ARRAYSIZE(filename), ImGuiInputTextFlags_CallbackCharFilter, filenameFilter);
						ImGui::InputText("Creator", creator, IM_ARRAYSIZE(creator), ImGuiInputTextFlags_None);
						ImGui::InputText("Description", description, IM_ARRAYSIZE(description), ImGuiInputTextFlags_None);
						ImGui::InputTextMultiline("Creator Notes", creatorNotes, IM_ARRAYSIZE(creatorNotes));
						ImGui::InputText("Youtube Link", youtubeLink, IM_ARRAYSIZE(youtubeLink), ImGuiInputTextFlags_None);
					}

					if (ImGui::Button("Create Drill##CreateFromReplay")) {
						errorMsgs["Creation"].clear();
						gameWrapper->Execute([this](GameWrapper* gw) {
							if (filename[0] == '\0') {
								errorMsgs["Creation"].push_back("Must specify a filename.");
								return;
							}

							if (unassignedPlayers.size() == (replayData.team0.size() + replayData.team1.size())) {
								errorMsgs["Creation"].push_back("Must assign at least one player");
								return;
							}

							if (!gameWrapper->IsInReplay()) {
								errorMsgs["Creation"].push_back("Must have a replay loaded");
								return;
							}
							auto replay = gameWrapper->GetGameEventAsReplay();
							if (replay.IsNull()) {
								errorMsgs["Creation"].push_back("Replay is null? Try again.");
								return;
							}

							TrainingPackDrill drill;
							auto cars = replay.GetCars();
							if (cars.IsNull()) {
								errorMsgs["Creation"].push_back("Cars array is null. Cannot proceed. Try again.");
								return;
							}

							auto ball = replay.GetBall();
							if (ball.IsNull()) {
								errorMsgs["Creation"].push_back("Ball is null? Cannot proceed. Try again.");
								return;
							}
							drill.ball.fromBall(ball);

							auto offense = offensiveTeam == 0 ? replayData.team0 : replayData.team1;
							auto defense = offensiveTeam == 0 ? replayData.team1 : replayData.team0;
							bool shooterSet = false;

							// Find shooter in offense, since some offensive players may have been unassigned
							string shooterId = "";
							for (auto player : offense) {
								if (unassignedPlayers.find(player.id) == unassignedPlayers.end()) {
									shooterId = player.id;
									break;
								}
							}

							for (int i = 0; i < cars.Count(); i++) {
								auto car = cars.Get(i);
								if (car.IsNull()) {
									errorMsgs["Creation"].push_back("One of the cars are null? Cannot proceed. Try again.");
									return;
								}

								auto pri = car.GetPRI();
								if (pri.IsNull()) {
									errorMsgs["Creation"].push_back("One of the players PRIs are null? Cannot proceed. Try again.");
									return;
								}

								auto uid = pri.GetUniqueIdWrapper().GetIdString();
								if (unassignedPlayers.find(uid) != unassignedPlayers.end()) {
									continue;
								}

								if (car.GetTeamNum2() == offensiveTeam) {
									if (!shooterId.empty() || shooterId.compare(uid) == 0) {
										shooterSet = true;
										drill.shooter.fromCar(car);
									}
									else {
										drill.passers.push_back(TrainingPackPlayer(car));
									}
								}
								else {
									drill.defenders.push_back(TrainingPackPlayer(car));
								}
							}

							if ((!shooterSet && !shooterId.empty()) ||
								(drill.passers.size() + drill.defenders.size()) != (offense.size() - 1 + defense.size() - unassignedPlayers.size())) {
								errorMsgs["Creation"].push_back("Failed to identify one of the players. Sometimes this happens if a player was just demoed. If this is the case, unassign that player. Otherwise, try to force replay data reload.");
								onReplayLoaded("forced");
								return;
							}

							cvarManager->log(getPackDataPath(filename));
							if (appendToExistingPack) {
								auto pack = TrainingPack(getPackDataPath(cachedPackNamesForAppend[createFromReplaySelectedPackIdx]));
								if (pack.offense != drill.passers.size() + 1) {
									errorMsgs["Creation"].push_back("Cannot append drill to selected training pack. The number of offensive players in the replay (" +
										to_string(drill.passers.size() + 1) + ") must match the training pack's offensive players (" + to_string(pack.offense) + ").");
									return;
								}

								if (drill.passers.size() == 0 && pack.offense == 0 && pack.defense != drill.defenders.size()) {
									errorMsgs["Creation"].push_back("Cannot append drill to selected training pack. Since the pack's offensive players is 0, the number of defensive players in the replay (" +
										to_string(drill.defenders.size()) + ") must match the training pack's defensive players (" + to_string(pack.defense) + ").");
									return;
								}

								pack.addDrill(drill);
								pack.save();
								packs.clear();
								gameWrapper->Toast("Team Training", "Drill added to existing training pack " + string(filename), "teamtraining1");
							} else {
								auto fpath = getPackDataPath(filename);
								if (fs::exists(fpath)) {
									errorMsgs["Creation"].push_back("Cannot create new training pack. A training pack with that filename already exists.");
									return;
								}
								auto pack = TrainingPack(fpath, drill.defenders.size(),
									drill.defenders.size(), 0, "", creator, creatorID, description,
									creatorNotes, youtubeLink, nullptr);
								pack.addDrill(drill);
								pack.save();
								packs.clear();
								gameWrapper->Toast("Team Training", "Drill added to new training pack " + string(filename), "teamtraining1");
							}
							});

						ImGui::EndTabItem();
					}
				}
			}
			ImGui::EndTabBar();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Settings")) {
			ImGui::TextWrapped("This plugin now utilizes the automatic shuffling and shot variance settings set in BakkesMod's Custom Training settings (F2 -> Custom Training tab).");
			ImGui::Separator();
			ImGui::InputText("Countdown after reset (in seconds)", countdown, IM_ARRAYSIZE(countdown), ImGuiInputTextFlags_CharsScientific);
			/*ImGui::Checkbox("Freeze players during countdown", &netcodeEnabled);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
			ImGui::TextWrapped("This feature requires ALL players in the lobby to have the latest version of the Team Training plugin installed and running. For easy install/update, open the plugin manager UI through plugins section of F2 menu, and install by ID 99.");
			ImGui::TextWrapped("Consider this feature as experimental as I have not yet determined all unusual events that may occur.");
			ImGui::TextWrapped("If you have any issues with players not becoming unfrozen, any player in the lobby should be able to press the unstuck button to unfreeze everyone.");
			ImGui::PopStyleColor();*/

			if (ImGui::Button("Unstuck")) {
				cvarManager->executeCommand("sleep 1; team_train_unstuck");
			}

			if (ImGui::Button("Save")) {
				cvarManager->getCvar(CVAR_PREFIX + "countdown").setValue(countdown);
				//cvarManager->getCvar(CVAR_PREFIX + "netcode_enabled").setValue(netcodeEnabled);
			}

			ImGui::Separator();
			ImGui::TextWrapped("Some of the \"Quick Settings\" buttons in BakkesMod's F2 screen will reset the bindings.");
			ImGui::TextWrapped("Use this button to reload the Team Training bindings.");
			
			if (ImGui::Button("Load Team Training Bindings")) {
				cvarManager->loadCfg(CFG_FILE);
			}

			ImGui::EndTabItem();
		}
		
		int whatsNewFlags = (cvarManager->getCvar(CVAR_PREFIX + "last_version_loaded").getStringValue().compare(PLUGIN_VERSION) != 0) ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("What's new", NULL, whatsNewFlags)) {
			if (whatsNewFlags & ImGuiTabItemFlags_SetSelected) {
				cvarManager->getCvar(CVAR_PREFIX + "last_version_loaded").setValue(PLUGIN_VERSION);
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
			ImGui::TextWrapped("The v0.3.0 update introduces many features that I have been working on for the past 4-5 months. If you notice any issues at all, please let me know as there are definitely some bugs that need to be worked out.");
			ImGui::PopStyleColor();

			ImGui::TextWrapped("v0.3.3 (Apr 19 2021");
			ImGui::TextWrapped("Changelog:");
			ImGui::TextWrapped("Added ability to create drills from repalys");

			ImGui::TextWrapped("v0.3.2 (Feb 19 2021)");
			ImGui::TextWrapped("Changelog:");
			ImGui::BulletText("Fixed crash on null tags");
			ImGui::BulletText("Added handling of invalid pack IDs returned when adding favorited packs");
			ImGui::BulletText("Added workaround for when Epic UID returns as 0");
			ImGui::BulletText("Fixed bug where only packs with filenames the same as the training pack codes could be loaded");
			ImGui::BulletText("Resolved an issue that would have prevented future plugin updates from being automatically downloaded");
			ImGui::BulletText("Added support for automatic shot mirroring and fixed ball spin and drill variance");

			ImGui::TextWrapped("v0.3.1 (Jan 3 2021)");
			ImGui::TextWrapped("Changelog:");
			ImGui::BulletText("Fixed bug preventing plugin from being loaded");

			ImGui::TextWrapped("v0.3.0 (Jan 2 2021)");
			ImGui::TextWrapped("Changelog:");
			ImGui::BulletText("Added the ability to search, download, and share any team trainingand custom training packs");
			ImGui::BulletText("Added support for tags to organize training packs");
			ImGui::BulletText("Added support for custom training metadata for organizingand downloading custom training packs");
			ImGui::BulletText("Added button to add custom training packs via codeand add all favorited packs");
			ImGui::BulletText("Fixed issue with non-ascii characters not showing in UI");

			ImGui::Separator();

			ImGui::TextWrapped("If you experience any issues using this plugin, please let me know.");
			ImGui::BulletText("Twitter: @PenguinDaft");
			ImGui::BulletText("Discord: DaftPenguin#5103");
			ImGui::BulletText("GitHub: github.com/daftpenguin/teamtrainingplugin");
			ImGui::TextWrapped("While this plugin is unverified on bakkesplugins.com, all plugin-side code is open source can be found on the GitHub link above along with the zip for every release.");

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();

	this->shouldBlockInput = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

std::string TeamTrainingPlugin::GetMenuName()
{
	return "teamtrainingplugin";
}

std::string TeamTrainingPlugin::GetMenuTitle()
{
	return menuTitle;
}

void TeamTrainingPlugin::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool TeamTrainingPlugin::ShouldBlockInput()
{
	return this->shouldBlockInput;
}

bool TeamTrainingPlugin::IsActiveOverlay()
{
	return true;
}

void TeamTrainingPlugin::OnOpen()
{
	stringstream ss;
	ss.precision(1);
	ss << std::fixed << cvarManager->getCvar(CVAR_PREFIX + "countdown").getFloatValue();
	::strncpy(countdown, ss.str().c_str(), IM_ARRAYSIZE(countdown));
	this->isWindowOpen = true;
}

void TeamTrainingPlugin::OnClose()
{
	this->isWindowOpen = false;
	packs.clear();
}

void TeamTrainingPlugin::filterLocalPacks(SearchFilterState& filters)
{
	fs::path selectedPath;
	if (filteredPacks.size() > selectedPackIdx) {
		selectedPath = filteredPacks[selectedPackIdx].filepath;
	}
	selectedPackIdx = 0;
	filteredPacks.clear();

	for (auto& pack : packs) {
		std::string packCode = boost::algorithm::to_lower_copy(pack.code);
		std::string filterCode = boost::algorithm::to_lower_copy(std::string(filters.code));
		if (!filterCode.empty() && packCode.compare(filterCode) != 0) continue;

		std::string packCreator = boost::algorithm::to_lower_copy(pack.creator);
		std::string filterCreator = boost::algorithm::to_lower_copy(std::string(filters.creator));
		if (!filterCreator.empty() && packCreator.compare(filterCreator) != 0) continue;

		if (filters.offense != 0 && pack.offense != filters.offense) {
			continue;
		}
		if (filters.defense != 0 && pack.defense != filters.defense) {
			continue;
		}

		auto enabledTags = filters.tagsState.GetEnabledTags();
		if (enabledTags.size() > 0) {
			bool matchingTag = false;
			for (auto& tag : enabledTags) {
				if (pack.tags.find(tag) != pack.tags.end()) {
					matchingTag = true;
					break;
				}
			}
			if (!matchingTag) continue;
		}

		std::string packDescription = boost::algorithm::to_lower_copy(pack.description);
		std::string filterDescription = boost::algorithm::to_lower_copy(std::string(filters.description));
		if (!filterDescription.empty() && packDescription.find(filterDescription) == std::string::npos) continue;

		filteredPacks.push_back(pack);

		if (pack.filepath == selectedPath) {
			selectedPackIdx = filteredPacks.size() - 1;
		}
	}
}

void TeamTrainingPlugin::searchPacksThread(SearchFilterState& filters)
{
	downloadSelectedPackIdx = 0;
	searchState.is_searching = true;

	std::string url = "/api/rocket-league/teamtraining/search";
	json j(filters);
	j["type"] = "packs";

	httplib::Client cli(SERVER_URL);
	if (auto res = cli.Post(url.c_str(), j.dump(), "application/json")) {
		if (res->status == 200) {
			json js;
			try {
				js = json::parse(res->body);
			}
			catch (...) {
				cvarManager->log(res->body);
				searchState.error = "Error parsing response from server";
				return;
			}

			for (json jsPack : js) {
				TrainingPackDBMetaData pack;
				pack.id = jsPack["id"].get<int>();
				pack.code = jsPack["code"].get<string>();
				pack.description = jsPack["description"].get<string>();
				pack.creator = jsPack["creator_name"].get<string>();
				pack.offense = jsPack["offense"].get<int>();
				pack.defense = jsPack["defense"].get<int>();
				pack.num_drills = jsPack["num_drills"].get<int>();
				pack.downloads = jsPack["downloads"].get<int>();
				for (json tag : jsPack["tags"]) {
					pack.tags.insert(tag.get<std::string>());
				}
				searchState.packs.push_back(pack); 
			}

			searchState.is_searching = false;
		}
		else {
			cvarManager->log(res->body);
			searchState.is_searching = false;
			searchState.failed = true;
			searchState.error = res->body;
		}
	}
	else {
		cvarManager->log("Failed to reach host");
		searchState.is_searching = false;
		searchState.failed = true;
		searchState.error = "Failed to reach host";
	}
}

void TeamTrainingPlugin::SearchPacks(SearchFilterState& filters)
{
	searchState.newSearch();
	searchState.packs.clear();
	boost::thread t{ &TeamTrainingPlugin::searchPacksThread, this, boost::ref(filters) };
}

void TeamTrainingPlugin::loadAllTagsThread(TagsState& state)
{
	// This should result in both local and server tags combined into given state. The state's cancellation should
	// work as cancellations in both loaders. After a successful load, both localFilterState and searchState tagsStates
	// should be updated with their relevant tags, too.

	TagsState& local = localFilterState.filters.tagsState;
	TagsState& server = searchState.filters.tagsState;

	// Only get sever and local tags if this is a forced refresh (state.has_downloaded), or they haven't been loaded yet.

	if (state.has_downloaded || !local.has_downloaded) {
		loadLocalTagsThread(state);
		if (state.cancelled || state.failed) {
			return;
		}
		state.is_downloading = true;
		local.setTags(state.tags);
	}

	state.tags.clear(); // We'll add local tags back into state later
	if (state.has_downloaded || server.has_downloaded) {
		downloadTagsThread(state);
		if (state.cancelled || state.failed) {
			return;
		}
		server.setTags(state.tags);
	}
	else {
		state.setTags(server.tags);
	}

	state.addTags(local.tags);
	state.restoreSelected();
	state.is_downloading = false;
	state.has_downloaded = true;

	for (auto& tag : state.pendingEnabledTags) {
		state.tags[tag] = true;
	}
}

void TeamTrainingPlugin::loadLocalTagsThread(TagsState& state)
{
	for (auto& pack : packs) {
		for (auto it = pack.tags.begin(); it != pack.tags.end(); ++it) {
			state.tags[*it] = false;
		}
	}
	state.restoreSelected();
	state.is_downloading = false;
	state.has_downloaded = true;
}

void TeamTrainingPlugin::downloadTagsThread(TagsState& state)
{
	std::string url = "/api/rocket-league/teamtraining/search";
	json j;
	j["type"] = "tags";

	httplib::Client cli(SERVER_URL);
	if (auto res = cli.Post(url.c_str(), j.dump(), "application/json")) {
		if (res->status == 200) {
			json js;
			try {
				js = json::parse(res->body);
			}
			catch (...) {
				cvarManager->log(res->body);
				state.error = "Error parsing response from server";
				return;
			}

			vector<string> tags = js.get<vector<string>>();
			for (auto it = tags.begin(); it != tags.end(); ++it) {
				state.tags[*it] = false;
			}
			state.restoreSelected();
			state.is_downloading = false;
			state.has_downloaded = true;
		}
		else {
			cvarManager->log(res->body);
			state.is_downloading = false;
			state.failed = true;
			state.error = res->body;
		}
	}
	else {
		cvarManager->log("Failed to reach host");
		state.is_downloading = false;
		state.failed = true;
		state.error = "Failed to reach host";
	}
}

void TeamTrainingPlugin::downloadPackThread(bool isRetry)
{
	string fname = "packid-" + to_string(downloadState.pack_id);
	if (!downloadState.pack_code.empty()) {
		fname = downloadState.pack_code;
	}
	fs::path fpath = getPackDataPath(fname);

	if (!isRetry && fs::exists(fpath)) {
		downloadState.failed = true;
		downloadState.error = "You may have already downloaded this training pack as the filename " + fpath.filename().string() + " already exists. Click retry if you would like to download the training pack anyways and overwrite this file.";
		return;
	}

	cvarManager->log("Downloading pack to: " + fpath.string());

	httplib::Client cli(SERVER_URL);

	stringstream dataSS;

	bool packExists;
	auto res = cli.Get(("/api/rocket-league/teamtraining/download?id=" + to_string(downloadState.pack_id)).c_str(), httplib::Headers(),
		[&](const httplib::Response& r) {
			packExists = r.status == 200; // Don't update progress if we're not downloading the pack
			return true;
		},
		[&](const char* data, size_t data_length) {
			dataSS << string(data, data_length);
			return downloadState.cancelled == false;
		},
		[&](uint64_t len, uint64_t total) {
			if (packExists) downloadState.progress = (float)len / (float)total;
			return downloadState.cancelled == false;
		});

	if (res) {
		if (res->status == 200) {
			ofstream outfile(fpath);
			outfile << dataSS.str();
			outfile.close();
			packs.clear();
		}
		else if (res->status != 404) {
			downloadState.error = dataSS.str();
			downloadState.failed = true;
			cvarManager->log(downloadState.error);
		}
	}
	else {
		downloadState.failed = true;
		downloadState.error = "Download failed. Could not reach server";
		cvarManager->log(downloadState.error);
	}
}

void TeamTrainingPlugin::DownloadPack(TrainingPackDBMetaData& pack)
{
	downloadState.newPack(pack.id, pack.code, pack.description);
	ImGui::OpenPopup("Downloading");
	boost::thread t{ &TeamTrainingPlugin::downloadPackThread, this, false };
}

void TeamTrainingPlugin::uploadPackThread()
{
	httplib::Client cli(SERVER_URL);

	cvarManager->log("Reading pack file: " + uploadState.pack_path.string());

	TrainingPack pack(uploadState.pack_path);
	pack.uploaderID = uploadState.uploaderID;
	pack.uploader = uploadState.uploader;

	cvarManager->log("Converting pack to json");
	json js(pack);
	if (pack.errorMsg != "") {
		uploadState.failed = true;
		uploadState.error = pack.errorMsg;
		return;
	}

	string fname = fs::path(uploadState.pack_path).filename().string();
	if (!uploadState.pack_code.empty()) {
		fname = uploadState.pack_code + ".json";
	}

	httplib::MultipartFormDataItems items = {
		{ "file", js.dump(2), fname, "application/json" }
	};

	auto res = cli.Post("/api/rocket-league/teamtraining/upload", items);
	if (res) {
		if (res->status == 200) {
			json js;
			try {
				js = json::parse(res->body);
			}
			catch (...) {
				cvarManager->log(res->body);
				uploadState.failed = true;
				uploadState.error = "Error parsing upload favorited pack response from server";
				return;
			}

			if (js.find("id") == js.end() || js.find("code") == js.end()) {
				uploadState.failed = true;
				uploadState.error = "Received bad response from server";
				cvarManager->log(res->body);
				return;
			}

			TrainingPack pack(uploadState.pack_path);
			pack.uploadID = js["id"].get<int>();
			pack.save();
			cvarManager->log("Upload successful");
			uploadState.progress = 1;
			packs.clear(); // Reload so pack has uploadID
		}
		else {
			uploadState.failed = true;
			cvarManager->log(res->body);
			uploadState.error = res->body;
		}
	}
	else {
		uploadState.failed = true;
		cvarManager->log("Upload failed. Could not reach server.");
		uploadState.error = "Upload failed. Could not reach server.";
	}
}

void TeamTrainingPlugin::UploadPack(const TrainingPack &pack)
{
	uploadState.newPack(pack);
	gameWrapper->Execute([=](GameWrapper *gw) {
		auto name = gw->GetPlayerName();
		uploadState.uploader = "";
		if (!name.IsNull()) {
			uploadState.uploader = name.ToString();
		}
		uploadState.uploaderID = "EPIC:" + gw->GetEpicID();
		boost::thread t{ &TeamTrainingPlugin::uploadPackThread, this };
		});
	ImGui::OpenPopup("Uploading");
}

void TeamTrainingPlugin::AddSearchFilters(
	SearchFilterState& filterState, string idPrefix, bool alwaysShowSearchButton,
	std::function<void(TagsState& tagsState)> tagLoader,
	std::function<void(SearchFilterState& filters)> searchCallback)
{
	if (ImGui::CollapsingHeader("Search Filters")) {
		if (ImGui::InputInt(("Offensive players##" + idPrefix + "Offense").c_str(), &filterState.offense, ImGuiInputTextFlags_CharsDecimal)) {
			filterState.offense = (filterState.offense < 0) ? 0 : filterState.offense;
		}
		if (ImGui::InputInt(("Defensive players##" + idPrefix + "Defense").c_str(), &filterState.defense, ImGuiInputTextFlags_CharsDecimal)) {
			filterState.defense = (filterState.defense < 0) ? 0 : filterState.defense;
		}
		ImGui::InputText(("Description##" + idPrefix + "Description").c_str(), filterState.description, IM_ARRAYSIZE(filterState.description));
		ImGui::InputText(("Creator##" + idPrefix + "Creator").c_str(), filterState.creator, IM_ARRAYSIZE(filterState.creator));
		ImGui::InputText(("Training Pack Code##" + idPrefix + "Code").c_str(), filterState.code, IM_ARRAYSIZE(filterState.code));

		if (ImGui::Button(("Edit Tags##" + idPrefix + "EditTags").c_str())) {
			if (!filterState.tagsState.has_downloaded) {
				filterState.tagsState.refresh();
				boost::thread t{ tagLoader, boost::ref(filterState.tagsState) };
			}
			filterState.tagsState.beforeEditEnabledTags = filterState.tagsState.GetEnabledTags();
			ImGui::OpenPopup("Edit Tags");
		}
		ImGui::SameLine();
		ImGui::TextWrapped("Tags: %s", boost::algorithm::join(filterState.tagsState.GetEnabledTags(), ", ").c_str());
		ShowTagsWindow(filterState.tagsState, false, tagLoader, nullptr);

		if (ImGui::Button(("Search##" + idPrefix + "SearchButton").c_str())) {
			searchCallback(filterState);
		}
		ImGui::SameLine();
		if (ImGui::Button(("Clear Filters##" + idPrefix + "ClearFilters").c_str())) {
			filterState.clear();
		}

		ImGui::Separator();
	}
	else if (alwaysShowSearchButton) {
		if (ImGui::Button(("Search##" + idPrefix + "SearchButton").c_str())) {
			searchCallback(filterState);
		}
	}
}

void TeamTrainingPlugin::ShowTagsWindow(TagsState& state, bool allowCustomTags,
	std::function<void(TagsState& state)> tagLoader,
	std::function<void(TagsState& state)> applyCallback)
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(790, 600), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::BeginPopupModal("Edit Tags", NULL)) {
		if (state.is_downloading) {
			ImGui::Text("Retrieving tags from the server");
			ImGui::SetCursorPos((ImGui::GetWindowSize() - ImVec2(30, 30)) * ImVec2(0.5f, 0.25f));
			ImGui::Spinner("Retrieving...", 30, 5);
		}
		else {
			if (state.error != "") {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				ImGui::TextWrapped(state.error.c_str());
				ImGui::PopStyleColor();
			}

			bool showRetry = state.failed;
			if (showRetry) {
				if (ImGui::Button("Retry", ImVec2(120, 0))) {
					state.retry();
					boost::thread t{ tagLoader, boost::ref(state) };
				}
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					state.cancel();
					if (state.tags.size() == 0) {
						state.undoEdits();
						ImGui::CloseCurrentPopup();
					}
				}
			}
			else {
				// Populate checkboxes
				ImGui::Columns(NUM_TAG_COLUMNS, NULL, false);
				size_t columnSize = 1 + ((state.tags.size() - 1) / (size_t)NUM_TAG_COLUMNS); // Fast ceiling of int div
				size_t i = 0;
				for (auto it = state.tags.begin(); it != state.tags.end(); ++it) {
					if (i != 0 && i % columnSize == 0) {
						ImGui::NextColumn();
					}
					++i;
					ImGui::Checkbox(it->first.c_str(), &it->second);
				}
				ImGui::EndColumns();

				if (allowCustomTags) {
					ImGui::InputText("", customTag, IM_ARRAYSIZE(customTag));
					ImGui::SameLine();
					if (ImGui::Button("Add New Tag")) {
						state.tags[customTag] = true;
						customTag[0] = 0;
					}
				}
			}
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		if (ImGui::Button("Apply")) {
			state.beforeEditEnabledTags.clear();
			ImGui::CloseCurrentPopup();
			if (applyCallback != nullptr) applyCallback(state);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			state.undoEdits();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh Tags")) {
			state.refresh();
			boost::thread t{ tagLoader, boost::ref(state) };
		}

		ImGui::EndPopup();
	}
}

void TeamTrainingPlugin::ShowFavoritedPacksWindow()
{
	UploadFavoritesState& state = uploadFavsState;
	ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::BeginPopupModal("Add Favorited Packs", NULL)) {
		if (state.was_started) {
			if (state.is_running) {
				size_t completed = state.packsCompleted.size();
				size_t inProgress = state.packsInProgress.size();
				ImGui::TextWrapped(("Processing: " + to_string(completed) + " of " + to_string(inProgress + completed) + " packs completed").c_str());
				ImGui::ProgressBar(state.progress, ImVec2(ImGui::GetWindowWidth() - 20, 0));
			}

			if (!state.error.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				ImGui::TextWrapped(state.error.c_str());
				ImGui::PopStyleColor();
			}

			bool showOk = state.packsInProgress.size() == 0;
			bool showRetry = state.failed;
			
			if (showOk) {
				if (ImGui::Button("Ok", ImVec2(120, 0))) {
					state.resetState();
					ImGui::CloseCurrentPopup();
					ImGui::SameLine();
				}
			}
			else if (showRetry) {
				if (ImGui::Button("Retry", ImVec2(120, 0))) {
					UploadFavoritedPacks();
				}
				ImGui::SameLine();
			}
			else {
				ImGui::Indent(120);
			}

			if (!showOk) {
				if (ImGui::Button("Stop", ImVec2(120, 0))) {
					state.resetState();
					ImGui::CloseCurrentPopup();
				}
			}
		}
		else {
			ImGui::TextWrapped("In order to add your favorited training packs, any packs the server does not have data about will need to uploaded to the server to be processed.");
			ImGui::TextWrapped("Clicking the Start button will begin the process of retrieving the data about known packs, and uploading any unknown packs.");
			ImGui::TextWrapped("This process may be cancelled and resumed later.");

			if (ImGui::Button("Start")) {
				UploadFavoritedPacks();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}

void TeamTrainingPlugin::UploadFavoritedPacks()
{
	// This was meant to be used to set the favorites director inside of uploadFavsState, but couldn't figure out how to get that directory...
	boost::thread t{ &TeamTrainingPlugin::FavoritedPacksUploadThread, this };
}

void TeamTrainingPlugin::FavoritedPacksUploadThread()
{
	UploadFavoritesState& state = uploadFavsState;
	state.resetState();
	state.is_running = true;
	state.was_started = true;

	// Try to update player's name in case it has changed since plugin was loaded
	gameWrapper->Execute([this](GameWrapper* gw) {
		auto pname = gw->GetPlayerName();
		if (!pname.IsNull()) uploader = pname.ToString();
		});

	// Can't figure out how to get full path to favorited packs directory, so we just get all favorited packs from every user directory inside TAGame/Training

	char* userprofile = getenv("USERPROFILE");
	if (!userprofile) {
		state.error = "Failed to get user directory. USERPROFILE does not seem to be set?";
		state.failed = true;
		return;
	}

	fs::path trainingDir = fs::path(userprofile) / "Documents" / "My Games" / "Rocket League" / "TAGame" / "Training";
	unordered_set<string> packFNames;

	cvarManager->log("Upload packs from: " + trainingDir.string());
	for (const auto& userTrainingDir : fs::directory_iterator(trainingDir)) {
		fs::path favDir = userTrainingDir.path() / "Favorities"; // LMAO
		if (fs::exists(favDir)) {
			for (const auto& pack : fs::directory_iterator(favDir)) {
				if (pack.path().has_extension() && pack.path().extension() == ".Tem") {
					if (packFNames.find(pack.path().stem().string()) == packFNames.end()) { // Do only unique .Tem files
						cvarManager->log("Uploading pack: " + pack.path().string());
						state.packsInProgress.insert(pack.path().string());
						packFNames.insert(pack.path().stem().string());
					}
				}
			}
		}
	}

	// Retrieve data about known packs
	std::string url = "/api/rocket-league/teamtraining/search";

	json j;
	j["type"] = "favorites";
	j["packFNames"] = packFNames;

	struct knownPack { int id = NO_UPLOAD_ID; string code; };
	map<string, struct knownPack> knownPacks;

	httplib::Client cli(SERVER_URL);
	if (auto res = cli.Post(url.c_str(), j.dump(), "application/json")) {
		if (res->status == 200) {
			json js;
			try {
				js = json::parse(res->body);
			}
			catch (...) {
				cvarManager->log(res->body);
				state.failed = true;
				state.error = "Error parsing favorites search response from server";
				return;
			}

			for (json jsPack : js) {
				string packFName = jsPack["packFName"].get<string>();
				knownPacks[packFName] = knownPack{ jsPack["id"].get<int>(), jsPack["code"].get<string>() };
			}
		}
		else {
			state.is_running = false;
			state.failed = true;
			state.error = res->body;
			cvarManager->log(state.error);
			return;
		}
	}
	else {
		state.is_running = false;
		state.failed = true;
		state.error = "Failed to reach host";
		cvarManager->log(state.error);
		return;
	}

	if (knownPacks.size() > 0) {
		stringstream ss;
		for (auto it = knownPacks.begin(); it != knownPacks.end(); ++it) {
			ss << it->second.id << ",";
		}
		cvarManager->log(to_string(knownPacks.size()) + " known pack(s) identified: " + ss.str());
	}

	// Collect fpaths into vector since we're modifying packsInProgress and it's cleaner this way
	vector<fs::path> packsToProcess;
	for (auto it = state.packsInProgress.begin(); it != state.packsInProgress.end(); ++it) {
		packsToProcess.push_back(fs::path(*it));
	}

	// Download all packs by either uploading unknown packs, or downloading known packs
	for (auto fpath : packsToProcess) {
		auto knownIt = knownPacks.find(fpath.stem().string());
		int pack_id = NO_UPLOAD_ID;
		string pack_code;

		// If known, set ID and move onto downloading. Otherwise, upload pack, receive ID, then download.
		if (knownIt != knownPacks.end()) {
			pack_id = knownIt->second.id;
			pack_code = knownIt->second.code;
		}
		else {
			cvarManager->log("Uploading pack: " + fpath.string());
			ifstream fin(fpath, ios::binary);
			vector<char> buf(istreambuf_iterator<char>(fin), {});

			httplib::MultipartFormDataItems items = {
				{ "file", string(buf.data(), buf.size()), fpath.filename().string(), "application/octet-stream" },
				{ "uploader", uploader, "", "" },
				{ "uploaderID", uploaderID, "", "" }
			};

			auto res = cli.Post("/api/rocket-league/teamtraining/upload", items);
			if (res) {
				if (res->status == 200) {
					json js;
					try {
						js = json::parse(res->body);
					}
					catch (...) {
						cvarManager->log(res->body);
						state.failed = true;
						state.error = "Error parsing upload favorited pack response from server";
						return;
					}

					if (js.find("id") == js.end() || js.find("code") == js.end()) {
						state.failed = true;
						state.error = "Received bad response from server";
						cvarManager->log(res->body);
						return;
					}
					pack_id = js["id"].get<int>();
					pack_code = js["code"].get<string>();
				}
				else {
					// Full failing here might be unnecessary. Should probably determine from error if it's pack related error, or server error.
					state.failed = true;
					state.error = res->body;
					cvarManager->log(state.error);
					return;
				}
			}
			else {
				state.failed = true;
				state.error = "Upload failed. Could not reach server.";
				cvarManager->log(state.error);
				return;
			}
		}

		if (pack_id == NO_UPLOAD_ID || pack_id == 0 || pack_code.empty()) {
			state.failed = true;
			state.error = "Received invalid pack ID (" + to_string(pack_id) + ") or pack code (" + pack_code + ")";
			cvarManager->log(state.error);
			return;
		}

		fs::path outpath = getPackDataPath(pack_code);
		if (fs::exists(outpath)) {
			cvarManager->log("Skipping pack " + pack_code + " since it already exists");
		}
		else {
			cvarManager->log("Downloading pack: " + to_string(pack_id));
			stringstream dataSS;
			bool packExists;

			auto res = cli.Get(("/api/rocket-league/teamtraining/download?id=" + to_string(pack_id)).c_str(), httplib::Headers(),
				[&](const httplib::Response& r) {
					packExists = r.status == 200; // Don't update progress if we're not downloading the pack
					return true;
				},
				[&](const char* data, size_t data_length) {
					dataSS << string(data, data_length);
					return state.cancelled == false;
				});

			if (res) {
				if (res->status == 200) {
					// This could be better but whatever...
					ofstream outfile(outpath);
					outfile << dataSS.str();
					outfile.close();
					TrainingPack pack(outpath);
					pack.addTag("Favorite");
					pack.save();
				}
				else {
					packs.clear();
					state.failed = true;
					state.error = dataSS.str();
					cvarManager->log(state.error);
					return;
				}
			}
			else {
				packs.clear();
				state.failed = true;
				state.error = "Download failed. Could not reach server";
				cvarManager->log(state.error);
				return;
			}
		}

		state.packsCompleted.insert(fpath.string());
		state.packsInProgress.erase(fpath.string());
		state.progress = state.packsCompleted.size() / (float) (state.packsCompleted.size() + state.packsInProgress.size());
	}

	packs.clear(); // Force reload for new packs
}

void TeamTrainingPlugin::AddPackByCode(string code)
{
	boost::to_upper(code);
	if (!isValidPackCode(code)) {
		errorMsgs["Selection"].push_back("Invalid pack code");
		return;
	}

	downloadState.newPack(NO_UPLOAD_ID, code, "");
	ImGui::OpenPopup("Add By Code");
	boost::thread t{ &TeamTrainingPlugin::addPackByCodeThread, this, false };
}

void TeamTrainingPlugin::addPackByCodeThread(bool isRetry)
{
	// Call download by code and save pack if it exists
	// TODO: We should really be generalizing some download pack function to use here, in download pack thread, and the add favs thread
	fs::path fpath = getPackDataPath(downloadState.pack_code);

	if (fs::exists(fpath) && !isRetry) {
		downloadState.error = "You have already downloaded this pack. Click retry if you would to redownload the pack and overwrite the existing file.";
		downloadState.failed = true;
		return;
	}

	downloadState.stage = "Downloading pack from server if it already has it";
	httplib::Client cli(SERVER_URL);
	stringstream dataSS;

	bool packExists;
	auto res = cli.Get(("/api/rocket-league/teamtraining/download?code=" + downloadState.pack_code).c_str(), httplib::Headers(),
		[&](const httplib::Response& r) {
			packExists = r.status == 200; // Don't update progress if we're not downloading the pack
			return true;
		},
		[&](const char* data, size_t data_length) {
			dataSS << string(data, data_length);
			return downloadState.cancelled == false;
		},
		[&](uint64_t len, uint64_t total) {
			if (packExists) downloadState.progress = (float)len / (float)total;
			return downloadState.cancelled == false;
		});

	if (res) {
		if (res->status == 200) {
			ofstream outfile(fpath);
			outfile << dataSS.str();
			outfile.close();
			downloadState.stage = "Pack added";
			packs.clear();
			return;
		}
		else if (res->status != 404) {
			downloadState.error = dataSS.str();
			downloadState.failed = true;
			cvarManager->log(downloadState.error);
			return;
		}
	}
	else {
		downloadState.failed = true;
		downloadState.error = "Download failed. Could not reach server";
		cvarManager->log(downloadState.error);
		return;
	}
	
	// Load training, then get Tem from game and upload to server like adding favs
	// TODO: Generalize the uploading Tem and downloading pack procedure for adding favs and adding by code
	downloadState.stage = "Server does not have pack data. Loading training pack in-game to upload it to the server. This will take a few seconds.";

	// TODO: Figure out why hooking on the game event stopped working.
	// Probably related to the other handler attached to the same event, even though unhooking and hooking this one didn't work.
	// Maybe we will need to add a single handler and determine if this needs to be called.
	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		cvarManager->log("Timeout triggered");
		if (!gw->IsInCustomTraining()) {
			downloadState.error = "Failed to load custom training pack. Either code was invalid or Rocket League failed to download the pack from their servers.";
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}

		auto te = TrainingEditorWrapper(gameWrapper->GetGameEventAsServer().memory_address);
		if (te.IsNull()) {
			downloadState.error = "Failed to get training pack data from game.";
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}

		auto tdd = te.GetTrainingData().GetTrainingData();
		auto code = tdd.GetCode();
		if (code.IsNull()) {
			downloadState.error = "Failed to get training pack code from game.";
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}

		if (code.ToString().compare(downloadState.pack_code) != 0) {
			downloadState.error = "Failed to load custom training pack. Either code was invalid or Rocket League failed to download the pack from their servers.";
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}

		auto fnameUnreal = te.GetTrainingFileName();
		if (fnameUnreal.IsNull()) {
			downloadState.error = "Failed to get training pack file from game.";
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}

		downloadState.temFName = fnameUnreal.ToString();

		if (downloadState.cancelled) {
			return;
		}

		boost::thread t{ &TeamTrainingPlugin::addPackByTemFNameThread, this };
		}, 5.0f);

	cvarManager->executeCommand("sleep 1; load_training " + downloadState.pack_code);
}

void TeamTrainingPlugin::ShowAddByCodeModal()
{
	// TODO: This is nearly an exact copy of the download modal...
	if (ImGui::BeginPopupModal("Add By Code", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (!downloadState.stage.empty()) {
			ImGui::TextWrapped(downloadState.stage.c_str());
		}

		ImGui::ProgressBar(downloadState.progress, ImVec2(300, 0));

		if (!downloadState.error.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
			ImGui::TextWrapped(downloadState.error.c_str());
			ImGui::PopStyleColor();
		}

		bool showOk = downloadState.progress >= 1;
		bool showRetry = downloadState.failed;
		if (showOk) {
			if (ImGui::Button("Ok", ImVec2(120, 0))) {
				downloadState.is_downloading = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		else if (showRetry) {
			if (ImGui::Button("Retry", ImVec2(120, 0))) {
				downloadState.resetState();
				boost::thread t{ &TeamTrainingPlugin::addPackByCodeThread, this, true };
			}
			ImGui::SameLine();
		}
		else {
			ImGui::Indent(120);
		}

		if (!showOk) {
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				downloadState.cancelled = true;
				downloadState.is_downloading = false;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

// TODO: Generalize this better and use it in adding favs thread?
void TeamTrainingPlugin::addPackByTemFNameThread()
{
	// If pack is favorited, fname will be "Favorities\something", otherwise it will be "Downloaded\something" (no .Tem)
	bool isFavorited = downloadState.temFName.find("Favorities") != string::npos;

	char* userprofile = getenv("USERPROFILE");
	if (!userprofile) {
		downloadState.error = "Failed to get user directory. USERPROFILE does not seem to be set?";
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}

	cvarManager->log("Searching for file: " + downloadState.temFName);
	downloadState.stage = "Searching for training pack file";

	fs::path trainingDir = fs::path(userprofile) / "Documents" / "My Games" / "Rocket League" / "TAGame" / "Training";
	fs::path fpath;
	bool fileFound = false;
	for (const auto& userTrainingDir : fs::directory_iterator(trainingDir)) {
		fpath = userTrainingDir.path() / (downloadState.temFName + ".Tem");
		if (fs::exists(fpath)) {
			if (isFavorited) {
				fileFound = true;
				break; // Found pack
			}
			// Check timestamp of file
			auto timestamp = fs::last_write_time(fpath).time_since_epoch();
			auto now = chrono::system_clock::now();
			auto secsSinceWrite = chrono::duration_cast<chrono::seconds>(now.time_since_epoch() - timestamp).count();
			if (secsSinceWrite < MAX_SECONDS_SINCE_TEM_FILE_CREATED) {
				fileFound = true;
				break;
			}
		}
	}

	if (!fileFound) {
		downloadState.error = "Failed to find training pack file in file system.";
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}

	// Upload the .Tem file
	cvarManager->log("Uploading pack: " + fpath.string());
	downloadState.stage = "Uploading training pack to server";
	ifstream fin(fpath, ios::binary);
	vector<char> buf(istreambuf_iterator<char>(fin), {});

	string uploadName = (isFavorited ? fpath.filename().string() : downloadState.pack_code + ".Tem");
	httplib::MultipartFormDataItems items = {
	{ "file", string(buf.data(), buf.size()), uploadName, "application/octet-stream" }
	};

	int pack_id = NO_UPLOAD_ID;
	string pack_code;

	httplib::Client cli(SERVER_URL);
	auto res = cli.Post("/api/rocket-league/teamtraining/upload", items);
	if (res) {
		if (res->status == 200) {
			json js;
			try {
				js = json::parse(res->body);
			}
			catch (...) {
				cvarManager->log(res->body);
				downloadState.error = "Error parsing upload response from server";
				downloadState.failed = true;
				return;
			}

			if (js.find("id") == js.end() || js.find("code") == js.end()) {
				downloadState.error = "Received bad response from server";
				downloadState.failed = true;
				cvarManager->log(res->body);
				return;
			}
			pack_id = js["id"].get<int>();
			pack_code = js["code"].get<string>();
		}
		else {
			downloadState.error = res->body;
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}
	}
	else {
		downloadState.error = "Upload failed. Could not reach server.";
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}

	if (pack_id == NO_UPLOAD_ID || pack_code.empty()) {
		downloadState.error = "Received invalid pack ID (" + to_string(pack_id) + ") or pack code (" + pack_code + ")";
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}

	if (pack_code.compare(downloadState.pack_code) != 0) {
		downloadState.error = "Oops, something went wrong. The wrong training pack file was uploaded and the pack codes don't match. Given code: " + downloadState.pack_code + ", Uploaded code: " + pack_code;;
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}

	// Now download the pack
	fs::path outpath = getPackDataPath(pack_code);
	cvarManager->log("Downloading pack: " + to_string(pack_id));
	downloadState.stage = "Downloading training pack data from server";
	stringstream dataSS;
	bool packExists;

	auto downRes = cli.Get(("/api/rocket-league/teamtraining/download?id=" + to_string(pack_id)).c_str(), httplib::Headers(),
		[&](const httplib::Response& r) {
			packExists = r.status == 200; // Don't update progress if we're not downloading the pack
			return true;
		},
		[&](const char* data, size_t data_length) {
			dataSS << string(data, data_length);
			return downloadState.cancelled == false;
		},
			[&](uint64_t len, uint64_t total) {
			if (packExists) downloadState.progress = (float)len / (float)total;
			return downloadState.cancelled == false;
		});

	if (downRes) {
		if (downRes->status == 200) {
			ofstream outfile(outpath);
			outfile << dataSS.str();
			outfile.close();
			downloadState.stage = "Pack added";
			packs.clear();
			return;
		}
		else {
			downloadState.error = dataSS.str();
			cvarManager->log(downloadState.error);
			downloadState.failed = true;
			return;
		}
	}
	else {
		downloadState.error = "Download failed. Could not reach server.";
		cvarManager->log(downloadState.error);
		downloadState.failed = true;
		return;
	}
}

void TeamTrainingPlugin::ShowUploadingModal()
{
	if (ImGui::BeginPopupModal("Uploading", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(("Uploading: " + uploadState.pack_description).c_str());
		ImGui::ProgressBar(uploadState.progress, ImVec2(300, 0));

		if (!uploadState.error.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
			ImGui::TextWrapped(uploadState.error.c_str());
			ImGui::PopStyleColor();
		}

		bool showOk = uploadState.progress >= 1;
		bool showRetry = uploadState.failed;
		if (showOk) {
			if (ImGui::Button("Ok", ImVec2(120, 0))) {
				uploadState.is_uploading = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		else if (showRetry) {
			if (ImGui::Button("Retry", ImVec2(120, 0))) {
				uploadState.resetState();
				cvarManager->log("launching uploadPackThread");
				boost::thread t{ &TeamTrainingPlugin::uploadPackThread, this };
			}
			ImGui::SameLine();
		}
		else {
			ImGui::Indent(120);
		}

		if (!showOk) {
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				uploadState.cancelled = true;
				uploadState.is_uploading = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}

void TeamTrainingPlugin::ShowDownloadingModal() {
	if (ImGui::BeginPopupModal("Downloading", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text(("Downloading: " + downloadState.pack_description).c_str());
		ImGui::ProgressBar(downloadState.progress, ImVec2(300, 0));

		if (!downloadState.error.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
			ImGui::TextWrapped(downloadState.error.c_str());
			ImGui::PopStyleColor();
		}

		bool showOk = downloadState.progress >= 1;
		bool showRetry = downloadState.failed;
		if (showOk) {
			if (ImGui::Button("Ok", ImVec2(120, 0))) {
				downloadState.is_downloading = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		else if (showRetry) {
			if (ImGui::Button("Retry", ImVec2(120, 0))) {
				downloadState.resetState();
				cvarManager->log("launching downloadPackThread");
				boost::thread t{ &TeamTrainingPlugin::downloadPackThread, this, true };
			}
		}
		else {
			ImGui::Indent(120);
		}

		if (!showOk) {
			if (showRetry) {
				ImGui::SameLine();
			}
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				downloadState.cancelled = true;
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

#include "TeamTrainingPlugin.h"

#include "imgui/imgui_custom_widgets.h"

#include <algorithm>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

// TODO: Prevent users from uploading too frequently (in server code, not here) - by IP, by steamID...
// TODO: Block uploads containing censored words (catch here, but also apply in server code)
// TODO: Add commenting and ratings
// TODO: Add tips/notes from multiple users
// TODO: Allow reporting of packs, comments, tips, etc
// TODO: Allow filtering on offense, defense, tags, drill name, creator, code
// TODO: Specify 0 = none. Search on description. Search on creator.
// TODO: Sorting methods. Add notes section in metadata.
// TODO: How to determine which packs were downloaded vs created? How to determine which packs can be uploaded?
// TODO: Track uploader from creator (we're going to upload packs we don't own).
// TODO: Notes and other data should be updateable by both creator and uploader?
// TODO: Method to force update packs (new censored words)
// TODO: Just download all meta data?

using namespace std;

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
	const size_t len = data.size();
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), data.c_str(), len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
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
			ImGui::TextWrapped("Be sure to start a multiplayer freeplay session via Rocket Plugin before loading a pack.");

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
			ShowFavoritedPacksWindow();

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

							if (!gameWrapper->IsInFreeplay()) {
								errorMsgs["Selection"].push_back("You must be in a freeplay session to load a training pack. Use Rocket plugin to launch a multiplayer freeplay session with non-local players.");
							}
							else {
								auto cars = gameWrapper->GetGameEventAsServer().GetCars();
								if (cars.Count() < pack.offense) {
									errorMsgs["Selection"].push_back("Pack requires " + std::to_string(pack.offense) + " players but there are only " + std::to_string(cars.Count()) + " in the lobby.");
								}
							}

							if (errorMsgs["Selection"].size() == 0) {
								cvarManager->executeCommand("sleep 1; team_train_load " + pack.code);
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
					ImGui::TextWrapped("Tags: %s", boost::algorithm::join(pack.tags, ", ").c_str());

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

		if (ImGui::BeginTabItem("Download Packs (beta)")) {
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
				ImGui::BeginChild("left pane", ImVec2(150, 0), true);
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
				ImGui::Text("Tags: %s", boost::algorithm::join(pack.tags, ", ").c_str());

				ImGui::EndChild();

				ImGui::SameLine();
				ImGui::EndGroup();
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Player Roles")) {
			if (!this->pack) {
				ImGui::Text("No pack loaded");
			} else if (!gameWrapper->IsInFreeplay()) {
				ImGui::Text("Not in freeplay");
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
			ImGui::TextWrapped("For example, convert WayProtein's training pack by loading it with the button below, enter 2 offensive players, 0 defensive players, and 8 drills. Then click convert.");
			if (ImGui::Button("Load WayProtein's Passing (downfield left)")) {
				cvarManager->executeCommand("sleep 1; load_training C833-6A35-A46A-7191");
			}
			ImGui::Separator();

			if (errorMsgs["Creation"].size() > 0) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				for (auto err : errorMsgs["Creation"]) {
					ImGui::TextWrapped(err.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::Separator();
			}

			// TODO: Hook game event when training pack loaded and store this data always (unload when training pack unloaded)
			if (gameWrapper->IsInCustomTraining()) {
				auto trainingEditor = TrainingEditorWrapper(gameWrapper->GetGameEventAsServer().memory_address);
				auto trainingSaveData = trainingEditor.GetTrainingData();

				if (trainingSaveData.GetbUnowned() == 1) {
					auto training = trainingSaveData.GetTrainingData();
					if (strcmp(training.GetCode().ToString().c_str(), code) != 0) {
						::strncpy(code, training.GetCode().ToString().c_str(), IM_ARRAYSIZE(code));
						::strncpy(filename, training.GetCode().ToString().c_str(), IM_ARRAYSIZE(filename));
						::strncpy(creator, training.GetCreatorName().ToString().c_str(), IM_ARRAYSIZE(creator));
						::strncpy(description, training.GetTM_Name().ToString().c_str(), IM_ARRAYSIZE(description));
					}
				}
			}

			if (ImGui::InputInt("Offensive players", &offensive_players, ImGuiInputTextFlags_CharsDecimal)) {
				offensive_players = (offensive_players < 0) ? 0 : offensive_players;
			}
			if (ImGui::InputInt("Defensive players", &defensive_players, ImGuiInputTextFlags_CharsDecimal)) {
				defensive_players = (defensive_players < 0) ? 0 : defensive_players;
			}
			if (ImGui::InputInt("Number of drills", &gui_num_drills, ImGuiInputTextFlags_CharsDecimal)) {
				gui_num_drills = (gui_num_drills < 1) ? 1 : gui_num_drills;
			}

			ImGui::InputText("Filename (no spaces or extensions)", filename, IM_ARRAYSIZE(filename), ImGuiInputTextFlags_CallbackCharFilter, filenameFilter);
			ImGui::InputText("Creator", creator, IM_ARRAYSIZE(creator), ImGuiInputTextFlags_None);
			ImGui::InputText("Description ", description, IM_ARRAYSIZE(description), ImGuiInputTextFlags_None);
			ImGui::Text("Code: %s", code);

			if (ImGui::Button("Convert")) {
				errorMsgs["Creation"].clear();
				if (!gameWrapper->IsInCustomTraining()) {
					errorMsgs["Creation"].push_back("Must be in custom training to do conversion.");
				}
				if (offensive_players + defensive_players == 0) {
					errorMsgs["Creation"].push_back("Must have at least one player.");
				} else if (offensive_players == 0) {
					errorMsgs["Creation"].push_back("Must have at least on offensive player. Defense only packs are not supported at this time.");
				}
				if (filename[0] == '\0') {
					errorMsgs["Creation"].push_back("Must specify a filename.");
				}

				if (errorMsgs["Creation"].size() == 0) {
					errorMsgs["Creation"].clear();
					std::string creatorEscaped(creator);
					std::string descriptionEscaped(description);
					boost::replace_all(descriptionEscaped, "\"", "\\\"");
					boost::replace_all(descriptionEscaped, "'", "\\'");
					boost::replace_all(creatorEscaped, "\"", "\\\"");
					boost::replace_all(creatorEscaped, "'", "\\'");
					std::stringstream cmd_ss;
					cmd_ss << "sleep 1; team_train_internal_convert " << offensive_players << " " << defensive_players << " " << gui_num_drills
						<< " \"" << filename << "\" \"" << creatorEscaped << "\" \"" << descriptionEscaped << "\" \"" << code << "\"";
					std::string cmd = cmd_ss.str();
					OnClose();
					cvarManager->executeCommand(cmd);
				}
			}

			ImGui::Separator();

			ImGui::Text("Drill order:");
			if (offensive_players > 0) {
				ImGui::Text("Shooter");
			}
			for (int i = 0; i < offensive_players - 1; i++) {
				if (i == offensive_players - 1) {
					ImGui::Text("Passer (and starting ball position/trajectory)");
				} else {
					ImGui::Text("Passer");
				}
			}
			for (int i = 0; i < defensive_players; i++) {
				ImGui::Text("Defender");
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Settings")) {
			ImGui::TextWrapped("This plugin now utilizes the automatic shuffling and shot variance settings set in BakkesMod's Custom Training settings (F2 -> Custom Training tab).");
			ImGui::Separator();
			ImGui::InputText("Countdown after reset (in seconds)", countdown, IM_ARRAYSIZE(countdown), ImGuiInputTextFlags_CharsScientific);

			if (ImGui::Button("Save")) {
				cvarManager->getCvar(CVAR_PREFIX + "countdown").setValue(countdown);
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

			ImGui::TextWrapped("v0.2.8 (Dec 7 2020)");
			ImGui::TextWrapped("Changelog:");
			ImGui::BulletText("Updated to work with upcoming Epic Games support");

			ImGui::Separator();

			ImGui::TextWrapped("v0.2.7 (Sep 4 2020)");
			ImGui::TextWrapped("Changelog:");
			ImGui::BulletText("Fixed bug causing crashes when variance is enabled");

			ImGui::Separator();
			
			ImGui::TextWrapped("v0.2.6 (Sep 1 2020) changelog:");
			ImGui::BulletText("Updated link from old bakkesmod.lib to new pluginsdk.lib");
			ImGui::BulletText("Added support for drill shuffling and variance in drills using BakkesMod's built-in custom training options");

			ImGui::Separator();

			ImGui::TextWrapped("If you experience any issues using this plugin, please let me know.");
			ImGui::BulletText("Twitter: @PenguinDaft");
			ImGui::BulletText("Discord: DaftPenguin#5103");
			ImGui::BulletText("GitHub: github.com/daftpenguin/teamtrainingplugin");

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
	filteredPacks.clear();
	selectedPackIdx = 0;

	for (auto& pack : packs) {
		std::string packCode = boost::algorithm::to_lower_copy(pack.code);
		std::string filterCode = boost::algorithm::to_lower_copy(std::string(filters.code));
		if (!filterCode.empty() && packCode.compare(filterCode) != 0) continue;

		std::string packCreator = boost::algorithm::to_lower_copy(pack.creator);
		std::string filterCreator = boost::algorithm::to_lower_copy(std::string(filters.creator));
		if (!filterCreator.empty() && packCreator.compare(filterCreator) != 0) continue;

		if (filters.offense != 0 && pack.offense != filters.offense) {
			cvarManager->log("Filtering on offense");
			continue;
		}
		if (filters.defense != 0 && pack.defense != filters.defense) {
			cvarManager->log("Filtering on defense");
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

void TeamTrainingPlugin::downloadTagsThread(SearchFilterState &state)
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
				searchState.error = "Error parsing response from server";
				return;
			}

			cvarManager->log(res->body);

			vector<string> tags = js.get<vector<string>>();
			for (auto it = tags.begin(); it != tags.end(); ++it) {
				state.tagsState.tags[*it] = false;
			}
			state.tagsState.restoreSelected();
			state.tagsState.is_downloading = false;
			state.tagsState.has_downloaded = true;
		}
		else {
			cvarManager->log(res->body);
			state.tagsState.is_downloading = false;
			state.tagsState.failed = true;
			state.tagsState.error = res->body;
		}
	}
	else {
		cvarManager->log("Failed to reach host");
		state.tagsState.is_downloading = false;
		state.tagsState.failed = true;
		state.tagsState.error = "Failed to reach host";
	}
}

void TeamTrainingPlugin::loadLocalTagsThread(SearchFilterState& state)
{
	for (auto& pack : packs) {
		for (auto it = pack.tags.begin(); it != pack.tags.end(); ++it) {
			state.tagsState.tags[*it] = false;
		}
	}
	state.tagsState.restoreSelected();

	state.tagsState.is_downloading = false;
	state.tagsState.has_downloaded = true;
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

	stringstream url;
	url << "/api/rocket-league/teamtraining/download?id=" << downloadState.pack_id;

	ofstream outfile(fpath);

	auto res = cli.Get(url.str().c_str(),
		[&](const char* data, size_t data_length) {
			outfile << string(data, data_length);
			cvarManager->log(string(data, data_length));
			return downloadState.cancelled == false;
		},
		[&](uint64_t len, uint64_t total) {
			downloadState.progress = (float) len / (float) total;
			return downloadState.cancelled == false;
		});

	outfile.close();
}

void TeamTrainingPlugin::DownloadPack(TrainingPackDBMetaData& pack)
{
	downloadState.newPack(pack.id, pack.code, pack.description);
	ImGui::OpenPopup("Downloading");
	boost::thread t{ &TeamTrainingPlugin::downloadPackThread, this, false };
	packs.clear(); // so we reload the packs next time selection tab is loaded
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

	httplib::MultipartFormDataItems items = {
		{ "file", js.dump(2), uploadState.pack_code + ".json", "application/json" }
	};

	auto res = cli.Post("/api/rocket-league/teamtraining/upload", items);
	if (res) {
		if (res->status == 200) {
			// TODO: Have server return new uploadID, add to the json, then rewrite the training pack file
			cvarManager->log("Upload successful");
			uploadState.progress = 1;
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
		uploadState.uploaderID = gw->GetEpicID();
		boost::thread t{ &TeamTrainingPlugin::uploadPackThread, this };
		});
	ImGui::OpenPopup("Uploading");
}

void TeamTrainingPlugin::AddSearchFilters(
	SearchFilterState& filterState, string idPrefix, bool alwaysShowSearchButton,
	std::function<void(SearchFilterState& filters)> tagLoader,
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

		ImGui::Text("Tags: %s", boost::algorithm::join(filterState.tagsState.GetEnabledTags(), ", ").c_str());

		if (ImGui::Button(("Edit Tags##" + idPrefix + "EditTags").c_str())) {
			if (!filterState.tagsState.has_downloaded) {
				filterState.tagsState.refresh();
				boost::thread t{ tagLoader, boost::ref(filterState) };
			}
			filterState.tagsState.beforeEditEnabledTags = filterState.tagsState.GetEnabledTags();
			ImGui::OpenPopup("Edit Tags");
		}
		ShowTagsWindow(filterState, tagLoader);

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

void TeamTrainingPlugin::ShowTagsWindow(SearchFilterState& state, std::function<void(SearchFilterState& filters)> tagLoader)
{
	ImGui::SetNextWindowSizeConstraints(ImVec2(790, 600), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::BeginPopupModal("Edit Tags", NULL)) {
		if (state.tagsState.is_downloading) {
			ImGui::Text("Retrieving tags from the server");
			ImGui::SetCursorPos((ImGui::GetWindowSize() - ImVec2(30, 30)) * ImVec2(0.5f, 0.25f));
			ImGui::Spinner("Retrieving...", 30, 5);
		}
		else {
			if (state.tagsState.error != "") {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				ImGui::TextWrapped(state.tagsState.error.c_str());
				ImGui::PopStyleColor();
			}

			bool showRetry = state.tagsState.failed;
			if (showRetry) {
				if (ImGui::Button("Retry", ImVec2(120, 0))) {
					state.tagsState.retry();
					boost::thread t{ tagLoader, boost::ref(state) };
				}
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					state.tagsState.cancel();
					if (state.tagsState.tags.size() == 0) {
						state.tagsState.undoEdits();
						ImGui::CloseCurrentPopup();
					}
				}
			}
			else {
				// Populate checkboxes
				ImGui::Columns(NUM_TAG_COLUMNS, NULL, false);
				size_t columnSize = 1 + ((state.tagsState.tags.size() - 1) / (size_t)NUM_TAG_COLUMNS); // Fast ceiling of int div
				size_t i = 0;
				for (auto it = state.tagsState.tags.begin(); it != state.tagsState.tags.end(); ++it) {
					if (i != 0 && i % columnSize == 0) {
						ImGui::NextColumn();
					}
					++i;
					ImGui::Checkbox(it->first.c_str(), &it->second);
				}
				ImGui::EndColumns();
			}
		}

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		if (ImGui::Button("Apply")) {
			state.tagsState.beforeEditEnabledTags.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			state.tagsState.undoEdits();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh Tags")) {
			state.tagsState.refresh();
			boost::thread t{ tagLoader, boost::ref(state) };
		}

		ImGui::EndPopup();
	}
}

void TeamTrainingPlugin::ShowFavoritedPacksWindow()
{
	UploadFavoritesState& state = uploadFavsState;
	ImGui::SetNextWindowSizeConstraints(ImVec2(600, 200), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::BeginPopupModal("Add Favorited Packs", NULL)) {
		if (state.was_started) {
			if (state.is_running) {
				size_t completed = state.packsCompleted.size();
				size_t inProgress = state.packsInProgress.size();
				ImGui::TextWrapped(("Processing: " + to_string(completed) + " of " + to_string(inProgress + completed) + " packs completed").c_str());
				ImGui::ProgressBar(uploadState.progress, ImVec2(300, 0));
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

	//C:\Users\monsi\Documents\My Games\Rocket League\TAGame\Training

	char* userprofile = getenv("USERPROFILE");
	if (!userprofile) {
		state.error = "Failed to get user directory. USERPROFILE does not seem to be set?";
		state.failed = true;
		return;
	}

	fs::path trainingDir = fs::path(userprofile) / "Documents" / "My Games" / "Rocket League" / "TAGame" / "Training";

	cvarManager->log("Upload packs from: " + trainingDir.string());
	for (const auto& userTrainingDir : fs::directory_iterator(trainingDir)) {
		fs::path favDir = userTrainingDir.path() / "Favorities"; // LMAO
		if (fs::exists(favDir)) {
			for (const auto& pack : fs::directory_iterator(favDir)) {
				if (pack.path().has_extension() && pack.path().extension() == ".Tem") {
					cvarManager->log("Uploading pack: " + pack.path().string());
					state.packsInProgress.insert(pack.path().string());
				}
			}
		}
	}

	// Retrieve data about known packs
	std::string url = "/api/rocket-league/teamtraining/search";

	std::vector<string> packFNames;
	packFNames.reserve(state.packsInProgress.size());
	for (auto it = state.packsInProgress.begin(); it != state.packsInProgress.end(); ++it) {
		packFNames.push_back(fs::path(*it).stem().string());
	}

	json j;
	j["type"] = "favorites";
	j["packFNames"] = packFNames;	

	cvarManager->log(j.dump());

	map<string, int> availablePacks;

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
				int id = jsPack["id"].get<int>();
				availablePacks[packFName] = id;
			}
		}
		else {
			cvarManager->log(res->body);
			state.is_running = false;
			state.failed = true;
			state.error = res->body;
		}
	}
	else {
		cvarManager->log("Failed to reach host");
		state.is_running = false;
		state.failed = true;
		state.error = "Failed to reach host";
	}

	if (state.failed) {
		return;
	}

	// Upload unknown packs
	// TODO: Finish implementation
	for (auto it = state.packsInProgress.begin(); it != state.packsInProgress.end(); ++it) {
		packFNames.push_back(fs::path(*it).stem().string());
	}

	// Download data for all packs given their IDs
}

void TeamTrainingPlugin::ShowUploadingModal()
{
	// TODO: Use OpenPopup properly
	if (uploadState.is_uploading) {
		if (ImGui::BeginPopupModal("Uploading", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(("Uploading: " + uploadState.pack_code).c_str());
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
				}
				ImGui::SameLine();
			}
			else if (showRetry) {
				if (ImGui::Button("Retry", ImVec2(120, 0))) {
					uploadState.resetState();
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
}

void TeamTrainingPlugin::ShowDownloadingModal() {
	//ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(FLT_MAX, FLT_MAX));
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

vector<string> TagsState::GetEnabledTags() const {
	vector<string> tags;
	for (auto it = this->tags.begin(); it != this->tags.end(); ++it) {
		if (it->second) {
			tags.push_back(it->first);
		}
	}
	return tags;
}

void to_json(json& j, const SearchFilterState& s)
{
	j = json{
		{"description", s.description},
		{"creator", s.creator},
		{"code", s.code},
		{"offense", s.offense},
		{"defense", s.defense},
		{"tags", s.tagsState.GetEnabledTags()}
	};
}
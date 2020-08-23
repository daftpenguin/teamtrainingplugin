#include "TeamTrainingPlugin.h"

#include <sstream>
#include <boost/algorithm/string/replace.hpp>

using namespace std;

int filenameFilter(ImGuiTextEditCallbackData* data) {
	switch (data->EventChar) {
	case ' ': case '"': case '*': case '<': case'>': case '?': case '\\': case '|': case '/': case ':':
		return 1;
	default:
		return 0;
	}
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

			if (errorMsgs["Selection"].size() > 0) {
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				for (auto err : errorMsgs["Selection"]) {
					ImGui::TextWrapped(err.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::Separator();
			}

			if (ImGui::Button("Refresh")) {
				packs.clear();
			}

			if (packs.size() == 0) {
				packs = getTrainingPacks();
			}

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
			else {
				// Left Selection
				static int selected = 0;
				ImGui::BeginChild("left pane", ImVec2(300, 0), true);
				int i = 0;
				for (auto pack : packs) {
					if (pack.errorMsg != "") {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
					}
					if (ImGui::Selectable(pack.description.c_str(), selected == i)) {
						selected = i;
					}
					if (pack.errorMsg != "") {
						ImGui::PopStyleColor();
					}
					if (selected == i) {
						ImGui::SetItemDefaultFocus();
					}
					i++;
				}
				ImGui::EndChild();
				ImGui::SameLine();

				// Right Details
				TrainingPack pack = packs[selected];
				ImGui::BeginGroup();

				if (pack.errorMsg == "") {
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
					if (pack.code != "") {
						ImGui::SameLine();
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
					ImGui::Text("Drills: %d", pack.drills.size());
					ImGui::Text("Code: %s", pack.code.c_str());
					ImGui::Text("Filepath: %s", pack.filepath.c_str());

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

		if (ImGui::BeginTabItem("Download Drills")) {
			// Allow filtering on offense, defense, tags, drill name, creator
			// Allow code to be used to download pack

			ImGui::TextWrapped("Download by code:");
			ImGui::InputText("", searchState.code, IM_ARRAYSIZE(searchState.code));
			
			if (ImGui::Button("Download##ByCode")) {
				DownloadPack(searchState.code);
			}

			ImGui::Separator();

			// TODO: Specify 0 = none. Search on description. Search on creator.
			// TODO: Sorting methods. Add notes section in metadata.
			// TODO: How to determine which packs were downloaded vs created? How to determine which packs can be uploaded?
			// TODO: Track uploader from creator (we're going to upload packs we don't own).
			// TODO: Notes and other data should be updateable by both creator and uploader?
			// TODO: Put training pack code in filename on server with timestamp.
			// TODO: Allow comments and rating?

			ImGui::TextWrapped("Search drills with filter options");
			if (ImGui::InputInt("Offensive players", &searchState.offense, ImGuiInputTextFlags_CharsDecimal)) {
				searchState.offense = (searchState.offense < 0) ? 0 : searchState.offense;
			}
			if (ImGui::InputInt("Defensive players", &searchState.defense, ImGuiInputTextFlags_CharsDecimal)) {
				searchState.defense = (searchState.defense < 0) ? 0 : searchState.defense;
			}

			if (ImGui::Button("Search")) {
				// TODO: RunPackSearch should set errors and we need to add display for it
				RunPackSearch();
			}

			ImGui::Separator();

			if (searchState.packs.size() == 0) {

			}
			else {
				// Left Selection
				static int selected = 0;
				ImGui::BeginChild("left pane", ImVec2(150, 0), true);
				int i = 0;
				for (auto pack : searchState.packs) {
					if (ImGui::Selectable(pack.description.c_str(), selected == i)) {
						selected = i;
					}
					if (selected == i) {
						ImGui::SetItemDefaultFocus();
					}
					i++;
				}
				ImGui::EndChild();
				ImGui::SameLine();

				// Right Details
				TrainingPackDBMetaData pack = searchState.packs[selected];
				ImGui::BeginGroup();

				if (ImGui::Button("Download##BySearch")) {
					// TODO: Why isn't this being called? Is it because there are two buttons with the text "Download"?? Yes, find way around this (can we assign IDs?). Also, refresh packs for selection after download.
					cvarManager->log("Downloading pack with code: " + pack.code);
					DownloadPack(pack.code);
				}
				if (pack.code != "") {
					ImGui::SameLine();
					if (ImGui::Button("Load Custom Training Pack")) {
						cvarManager->executeCommand("sleep 1; load_training " + string(pack.code));
					}
				}

				ImGui::BeginChild("Training Pack Details", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

				ImGui::Text(pack.description.c_str());
				ImGui::Separator();
				ImGui::Text("Creator: %s", pack.creator.c_str());
				ImGui::Text("Offensive Players: %d", pack.offense);
				ImGui::Text("Defensive Players: %d", pack.defense);
				ImGui::Text("Drills: %d", pack.num_drills);
				ImGui::Text("Code: %s", pack.code.c_str());

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
					ImGui::Button(cars.Get(player_order[i]).GetPRI().GetPlayerName().ToString().c_str());

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
				cvarManager->executeCommand(CVAR_PREFIX + "countdown " + countdown);
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
				cvarManager->executeCommand(CVAR_PREFIX + "last_version_loaded " + PLUGIN_VERSION + "; writeconfig");
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

void TeamTrainingPlugin::RunPackSearch()
{
	stringstream query;
	query << "/api/rocket-league/teamtraining/search?type=packs";
	if (searchState.offense > 0) {
		query << "&offense=" << searchState.offense;
	}
	if (searchState.defense > 0) {
		query << "&defense=" << searchState.defense;
	}

	searchState.packs.clear();

	httplib::Client cli(SERVER_URL);
	if (auto res = cli.Get(query.str().c_str())) {
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
				pack.code = jsPack["code"].get<string>();
				pack.description = jsPack["description"].get<string>();
				pack.creator = jsPack["creator"].get<string>();
				pack.offense = jsPack["offense"].get<int>();
				pack.defense = jsPack["defense"].get<int>();
				pack.num_drills = jsPack["num_drills"].get<int>();
				pack.downloads = jsPack["downloads"].get<int>();
				searchState.packs.push_back(pack);
			}
		}
		else {
			cvarManager->log(res->body);
			searchState.error = res->body;
		}
	}
	else {
		cvarManager->log("Failed to reach host");
		searchState.error = "Failed to reach host";
	}
}

void TeamTrainingPlugin::DownloadPack(string code)
{
	fs::path fpath = getPackDataPath(code);

	if (fs::exists(fpath)) {
		// TODO: Set error message
		return;
	}

	cvarManager->log("Downloading pack to: " + fpath.string());

	httplib::Client cli(SERVER_URL);

	stringstream url;
	url << "/api/rocket-league/teamtraining/download?code=" << code;

	ofstream outfile(fpath);

	auto res = cli.Get(url.str().c_str(),
		[&](const char* data, size_t data_length) {
			outfile << string(data, data_length);
			cvarManager->log(string(data, data_length));
			return true;
		});

	outfile.close();
}
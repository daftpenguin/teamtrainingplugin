#include "TeamTrainingPlugin.h"

#include <sstream>
#include <boost/algorithm/string/replace.hpp>

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
			ImGui::Text("Be sure to start a multiplayer freeplay sessions via Rocket Plugin before loading a pack.");

			if (errorMsgs["Selection"].size() > 0) {
				for (auto err : errorMsgs["Selection"]) {
					ImGui::Text(err.c_str());
				}
				ImGui::Separator();
			}

			if (ImGui::Button("Refresh")) {
				packs.clear();
			}

			if (packs.size() == 0) {
				packs = getTrainingPacks();
				pack_keys.clear();
				for (auto pack : packs) {
					pack_keys.push_back(pack.first);
				}
			}

			// Left Selection
			static int selected = 0;
			ImGui::BeginChild("left pane", ImVec2(150, 0), true);
			int i = 0;
			for (auto pack : packs) {
				pack_keys.push_back(pack.first);
				if (pack.second.errorMsg != "") {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
				}
				if (ImGui::Selectable(pack.first.c_str(), selected == i)) {
					selected = i;
				}
				if (pack.second.errorMsg != "") {
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
			std::string pack_key = pack_keys[selected];
			TrainingPack pack = packs.at(pack_key);
			ImGui::BeginGroup();
			
			if (pack.errorMsg == "") {
				if (ImGui::Button("Load Team Training Pack")) {
					OnClose();
					cvarManager->executeCommand("sleep 1; team_train_load " + pack_key);
				}
				if (pack.code != "") {
					ImGui::SameLine();
					if (ImGui::Button("Load Custom Training Pack")) {
						OnClose();
						cvarManager->executeCommand("sleep 1; load_training " + pack.code);
					}
				}

				ImGui::BeginChild("Training Pack Details", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

				ImGui::Text(pack_key.c_str());
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
				ImGui::Text(pack_key.c_str());
				ImGui::Separator();
				ImGui::TextWrapped("Failed to load pack: %s", pack.errorMsg.c_str());
				ImGui::TextWrapped("If you converted this pack yourself, please try again.");
				ImGui::TextWrapped("If that does not work, please report this bug to daftpenguinrl@gmail.com, @PenguinDaft on Twitter, DaftPenguin#5103 on Discord, or report issue on http://github.com/daftpenguin/teamtrainingplugin.");
				ImGui::TextWrapped("Please include the custom training pack code with your report and the json file at the location: %s.", pack.filepath.c_str());
				ImGui::PopStyleColor();
			}
			
			ImGui::SameLine();
			ImGui::EndGroup();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Player Roles")) {
			if (!this->pack) {
				ImGui::Text("No pack loaded");
			} else {
				ImGui::Text("Player role assignments are based on their order as they are \"seen\" by the server.");
				ImGui::Text("Different events may change their ordering, like players getting demolished or disconnecting then reconnecting to the server.");
				ImGui::Text("I may or may not fix this in a future update.");

				if (errorMsgs["Roles"].size() > 0) {
					for (auto err : errorMsgs["Creation"]) {
						ImGui::Text(err.c_str());
					}
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
			ImGui::TextWrapped("Enter the number of offensive and defensive players below, and the ordering of each drill to the corresponding player position will show.");
			ImGui::TextWrapped("Repeat this pattern for more than one drill in the team training pack.");
			ImGui::TextWrapped("After creating the custom training pack, load it in, fill in all of the details below, then click the convert button.");
			ImGui::TextWrapped("TURN OFF TRAINING VARIANCE BEFORE DOING CONVERSION");
			ImGui::Separator();

			if (errorMsgs["Creation"].size() > 0) {
				for (auto err : errorMsgs["Creation"]) {
					ImGui::TextColored(ImVec4(255, 0, 0, 255), err.c_str());
				}
				ImGui::Separator();
			}

			if (gameWrapper->IsInCustomTraining()) {
				TrainingEditorSaveDataWrapper training = TrainingEditorWrapper(gameWrapper->GetGameEventAsServer().memory_address).GetTrainingData().GetTrainingData();
				
				if (strcmp(training.GetCode().ToString().c_str(), code) != 0) {
					::strncpy(code, training.GetCode().ToString().c_str(), IM_ARRAYSIZE(code));
					::strncpy(filename, training.GetCode().ToString().c_str(), IM_ARRAYSIZE(filename));
					::strncpy(creator, training.GetCreatorName().ToString().c_str(), IM_ARRAYSIZE(creator));
					::strncpy(description, training.GetTM_Name().ToString().c_str(), IM_ARRAYSIZE(description));
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
				if (offensive_players + defensive_players == 0) {
					errorMsgs["Creation"].push_back("Must have at least one player.");
				}
				if (filename[0] == '\0') {
					errorMsgs["Creation"].push_back("Must specify a filename.");
				}

				if (errorMsgs["Creation"].size() == 0) {
					errorMsgs["Creation"].clear();
					std::string creatorEscaped(creator);
					std::string descriptionEscaped(description);
					boost::replace_all(descriptionEscaped, "\"", "\\\"");
					boost::replace_all(creatorEscaped, "\"", "\\\"");
					std::stringstream cmd_ss;
					cmd_ss << "sleep 1; team_train_internal_convert " << offensive_players << " " << defensive_players << " " << gui_num_drills
						<< " \"" << filename << "\" \"" << creator << "\" \"" << description << "\" \"" << code << "\"";
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
			ImGui::Checkbox("Randomize", &randomize);
			ImGui::InputText("Countdown after reset (in seconds)", countdown, IM_ARRAYSIZE(countdown), ImGuiInputTextFlags_CharsScientific);

			if (ImGui::Button("Save")) {
				cvarManager->executeCommand(CVAR_PREFIX + "randomize " + std::to_string(randomize));
				cvarManager->executeCommand(CVAR_PREFIX + "countdown " + countdown);
			}

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
	randomize = cvarManager->getCvar(CVAR_PREFIX + "randomize").getBoolValue();
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
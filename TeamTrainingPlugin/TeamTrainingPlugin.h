#pragma comment(lib, "BakkesMod.lib")

#include <fstream>

#include "bakkesmod\plugin\bakkesmodplugin.h"
#include "bakkesmod\plugin\pluginwindow.h"

#include "imgui/imgui.h"

#include "TrainingPack.h"

using namespace placeholders;

class TeamTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow
{
public:
	void onLoad();
	void onUnload();

private:
	// Console commands
	void onLoadTrainingPack(std::vector<std::string> params);
	void onResetShot(std::vector<std::string> params);
	void onNextShot(std::vector<std::string> params);
	void onPrevShot(std::vector<std::string> params);
	void randomizePlayers(std::vector<std::string> params);
	void cyclePlayers(std::vector<std::string> params);
	void listPacks(std::vector<std::string> params);

	// Hooks
	void onResetShotEvent(std::string eventName);
	void onFreeplayDestroyed(std::string eventName);

	// Helper functions
	void setShot(int shot);
	void resetShot();
	void setPlayerToCar(TrainingPackPlayer player, CarWrapper car);
	bool validatePlayers(ServerWrapper tutorial);
	std::map<std::string, TrainingPack> getTrainingPacks();

	std::vector<unsigned int> player_order;
	std::shared_ptr<TrainingPack> pack;
	unsigned int current_shot = 0;
	unsigned int last_shot_set = 0;

	// Properties for printing custom training
	void writeShotInfo(std::vector<std::string> params);
	void internalConvert(std::vector<std::string> params);
	void onNextRound(std::string eventName);
	void onBallTick(ServerWrapper server, void * params, std::string eventName);
	void writeDrillToFile();
	void getNextShot();
	ofstream custom_training_export_file;
	int offense;
	int defense;
	int num_drills;
	int drills_written = 0;
	TrainingPackBall custom_training_ball;
	std::vector<TrainingPackPlayer> custom_training_players;

	// Random extra
	void addNewBall(std::vector<std::string> params);
	void twoBallTraining(std::vector<std::string> params);
	Vector2 start;
	Vector2 end;

/*
 * GUI stuffs
 */
private:
	bool isWindowOpen = false;
	bool isMinimized = false;
	bool shouldBlockInput = false;
	std::string menuTitle = "Team Training";
	std::string errorMsg = "";
	int offensive_players = 0;
	int defensive_players = 0;
	int gui_num_drills = 1;
	char filename[128];
	char creator[128];
	char description[500];
	char code[20] = "";
	//ImGui::FileBrowser fileDialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_SingleClickDir | ImGuiFileBrowserFlags_SortIgnoreCase);
public:
	void Render();
	std::string GetMenuName();
	std::string GetMenuTitle();
	void SetImGuiContext(uintptr_t ctx);
	bool ShouldBlockInput();
	bool IsActiveOverlay();
	void OnOpen();
	void OnClose();
};


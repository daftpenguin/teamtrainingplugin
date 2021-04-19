// TODO: Fix my fucking coding style... camelCase or underscores. capitalize methods or don't. ffs pick one holy shit
#pragma once
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "CRYPT32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "pluginsdk.lib")

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define WIN32_LEAN_AND_MEAN
#include "cpp-httplib/httplib.h"

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "imgui/imgui.h"
#include "TrainingPack.h"
#include "GUIStates.h"
//#include "NetcodeManager/NetcodeManager.h"

#include <fstream>
#include <filesystem>
#include <chrono>


constexpr auto PLUGIN_VERSION = "0.3.3";
constexpr auto SERVER_URL = "https://www.daftpenguin.com";
//constexpr auto SERVER_URL = "http://localhost:8000";
constexpr int MAX_BALL_TICK_FAILURES = 3;
constexpr int MAX_BALL_VELOCITY_ZERO = 5;
constexpr int NUM_TAG_COLUMNS = 5;
constexpr int MAX_SECONDS_SINCE_TEM_FILE_CREATED = 15;
constexpr auto CUSTOM_TRAINING_LOADED_EVENT = "Function TAGame.GameEvent_TrainingEditor_TA.StartPlayTest";
constexpr auto REPLAY_LOADED_EVENT = "Function TAGame.GameInfo_Replay_TA.ImportReplay";


const std::string CVAR_PREFIX("cl_team_training_");

struct ReplayPlayerData {
	std::string name;
	std::string id;
};

class ReplayData {
public:
	ReplayData() : dataSet(false) {};
	void load(GameWrapper* gw);
	void clear();

	bool dataSet = false;
	std::vector<ReplayPlayerData> team0;
	std::vector<ReplayPlayerData> team1;
	std::string errorMsg;
};

class TeamTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow
{
public:
	void onLoad();
	void onUnload();

private:
	//std::shared_ptr<NetcodeManager> Netcode;
	//void OnMessageReceived(const std::string& Message, PriWrapper Sender);

	/*
	 * Training Pack Usage
	 */
	void onLoadTrainingPack(std::vector<std::string> params);
	void onResetShot(std::vector<std::string> params);
	void onNextShot(std::vector<std::string> params);
	void onPrevShot(std::vector<std::string> params);
	void randomizePlayers(std::vector<std::string> params);
	void cyclePlayers(std::vector<std::string> params);
	void listPacks(std::vector<std::string> params);
	//void test(std::vector<std::string> params);
	void loadRandomPack(std::vector<std::string> params);
	bool isValidServer();
	bool isValidServer(ServerWrapper& sw);

	// Player tracking
	//void onPlayerLeave(PlayerControllerWrapper pc, void* params, string eventName);
	//void onPlayerJoin(PlayerControllerWrapper pc, void* params, string eventName);

	// Hooks
	void onGoalScored(std::string eventName);
	void onResetShotEvent(std::string eventName);
	void onFreeplayDestroyed(std::string eventName);

	// Drill variance functions
	TrainingPackDrill createDrillVariant(TrainingPackDrill drill);
	TrainingPackPlayer addCarVariance(TrainingPackPlayer car);
	TrainingPackBall addBallVariance(TrainingPackBall location);

	// Helper functions
	void setShot(int shot);
	void resetShot();
	void setPlayerToCar(TrainingPackPlayer player, CarWrapper car);
	bool validatePlayers(ArrayWrapper<CarWrapper> cars);
	std::vector<TrainingPack> getTrainingPacks();

	std::vector<int> player_order;
	std::shared_ptr<TrainingPack> pack;
	unsigned int current_shot = 0;
	unsigned int last_shot_set = 0;
	bool goal_was_scored = false;
	//bool netcodeEnabled = false;
	std::chrono::milliseconds last_reset;

/*
 * Training Pack Conversion
 */
private:
	void onNextRound(std::string eventName);
	void onBallTick(std::string eventName);
	void getNextShot();
	std::filesystem::path getPackDataPath(std::string packName);

	void toggleBoost(int state);
	void savePack();
	void stopConversion(std::string message);

	TrainingPack conversion_pack;
	int ball_tick_failures;
	int ball_velocity_zero;
	int getNextShotCalled;

/*
 * GUI stuffs
 */
private:
	bool isWindowOpen = false;
	bool shouldBlockInput = true;
	std::string menuTitle = "Team Training (created by DaftPenguin)";
	std::map<std::string, std::vector<std::string>> errorMsgs = {
		{ "Selection", {}},
		{ "Roles", {}},
		{ "Creation", {}},
	};
	std::string packDataPath = "";
	std::string uploader;
	std::string uploaderID;
	// Selection
	char addByCode[20] = "";
	char customTag[128] = "";
	TrainingPack tagEditingPack;
	size_t selectedPackIdx = 0;
	std::vector<TrainingPack> packs;
	std::vector<TrainingPack> filteredPacks;
	// Downloads
	int downloadSelectedPackIdx = 0;
	SearchState searchState;
	std::vector<TrainingPack*> packsForAppend;
	std::vector<char*> cachedPackNamesForAppend;
	// Creation from training pack
	InGameTrainingPackData inGameTrainingPackData;
	int offensive_players = 0;
	int defensive_players = 0;
	int gui_num_drills = 1;
	char filename[MAX_FILENAME_LENGTH] = "";
	char code[20] = "";
	char creator[MAX_CREATOR_LENGTH] = "";
	char creatorID[MAX_CREATOR_ID_LENGTH] = "";
	char description[MAX_DESCRIPTION_LENGTH] = "";
	char creatorNotes[MAX_NOTES_LENGTH] = "";
	char youtubeLink[MAX_YOUTUBE_LENGTH] = "";
	std::vector<std::string> enabledTags;
	// Creation from replay
	bool appendToExistingPack;
	int offensiveTeam;
	int createFromReplaySelectedPackIdx;
	std::unordered_map<std::string, ReplayPlayerData> unassignedPlayers;
	ReplayData replayData;
	// Settings
	char countdown[10] = "1.0";

public:
	void Render();
	std::string GetMenuName();
	std::string GetMenuTitle();
	void SetImGuiContext(uintptr_t ctx);
	bool ShouldBlockInput();
	bool IsActiveOverlay();
	void OnOpen();
	void OnClose();

private:
	static constexpr char CFG_FILE[] = "team_training.cfg";
	static constexpr char RESET_CFG_FILE[] = "default_training.cfg";

	void AddSearchFilters(
		SearchFilterState& filterState, std::string idPrefix, bool alwaysShowSearchButton,
		std::function<void(TagsState& tagsState)> tagLoader,
		std::function<void(SearchFilterState& filters)> searchCallback);
	void filterLocalPacks(SearchFilterState& filters);
	
	void searchPacksThread(SearchFilterState& filters);
	void SearchPacks(SearchFilterState& filters);
	
	void downloadPackThread(bool isRetry);
	void DownloadPack(TrainingPackDBMetaData& pack);
	void ShowDownloadingModal();

	void uploadPackThread();
	void UploadPack(const TrainingPack& pack);
	void ShowUploadingModal();

	void loadLocalTagsThread(TagsState& state);
	void downloadTagsThread(TagsState& state);
	void loadAllTagsThread(TagsState& state);
	void ShowTagsWindow(TagsState& state, bool allowCustomTags,
		std::function<void(TagsState& state)> tagLoader,
		std::function<void(TagsState& state)> applyCallback);

	void ShowFavoritedPacksWindow();
	void UploadFavoritedPacks();
	void FavoritedPacksUploadThread();

	void AddPackByCode(std::string code);
	void addPackByCodeThread(bool isRetry);
	void ShowAddByCodeModal();

	void addPackByTemFNameThread();

	void onCustomTrainingLoaded(std::string event);
	void onReplayLoaded(std::string event);

	TagsState localEditTagsState;
	SearchState localFilterState;
	UploadFavoritesState uploadFavsState;
	UploadState uploadState;
	DownloadState downloadState;
};
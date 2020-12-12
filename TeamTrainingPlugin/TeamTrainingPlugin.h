#pragma comment(lib, "pluginsdk.lib")

#define WIN32_LEAN_AND_MEAN
#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "cpp-httplib/httplib.h"

#include <fstream>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"

#include "imgui/imgui.h"

#include "TrainingPack.h"

#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

using namespace std;

constexpr auto PLUGIN_VERSION = "0.2.8";
constexpr auto SERVER_URL = "http://localhost:8000";
constexpr int MAX_BALL_TICK_FAILURES = 3;
constexpr int MAX_BALL_VELOCITY_ZERO = 5;

const std::string CVAR_PREFIX("cl_team_training_");

/*
 * State stuff needed for GUI
 */

class SearchFilterState : private boost::noncopyable {
public:
	SearchFilterState() : descriptionQuery(""), code(""), offense(0), defense(0) {};

	string descriptionQuery;
	char code[20];
	int offense;
	int defense;
};

class SearchState : private boost::noncopyable
{
public:
	SearchState() : is_searching(false), failed(false), error(""), mutex() {};

	void newSearch() {
		resetState();
		is_searching = true;
	}

	void resetState() {
		is_searching = false;
		failed = false;
		error = "";
	}

	SearchFilterState filters;
	vector<TrainingPackDBMetaData> packs;

	bool is_searching;
	bool failed;
	string error;
	boost::mutex mutex;
};

class DownloadState : private boost::noncopyable
{
public:
	DownloadState() : is_downloading(false), failed(false), cancelled(false), progress(0), error(""), pack_code(""), mutex() {};

	void newPack(string code) {
		resetState();
		pack_code = code;
		is_downloading = true;
	}

	void resetState() {
		is_downloading = false;
		failed = false;
		cancelled = false;
		progress = 0;
		error = "";
	}

	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	string pack_code;
	boost::mutex mutex;
};

class UploadState : private boost::noncopyable
{
public:
	UploadState() : is_uploading(false), failed(false), cancelled(false), progress(0), error(""), pack_path(""), pack_code(""), mutex() {};

	void newPack(TrainingPack pack) {
		resetState();
		pack_path = pack.filepath;
		pack_code = pack.code;
		is_uploading = true;
	};

	void resetState() {
		is_uploading = false;
		failed = false;
		cancelled = false;
		progress = 0;
		error = "";
	}

	bool is_uploading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	string pack_path;
	string pack_code;
	boost::mutex mutex;
};

class TeamTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow
{
public:
	void onLoad();
	void onUnload();

private:
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

/*
 * Training Pack Conversion
 */
private:
	void internalConvert(std::vector<std::string> params);
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
	// Selection
	std::vector<TrainingPack> packs;
	// Downloads
	SearchState searchState;
	// Creation
	int offensive_players = 0;
	int defensive_players = 0;
	int gui_num_drills = 1;
	char filename[128] = "";
	char creator[128] = "";
	char description[500] = "";
	char code[20] = "";
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

	void AddSearchFilters(SearchFilterState& filterState, string idPrefix, std::function<void(SearchFilterState&)> searchCallback);
	void searchPacksThread(SearchFilterState& filters);
	void SearchPacks(SearchFilterState& filters);
	void downloadPackThread();
	void DownloadPack(std::string code);
	void uploadPackThread();
	void UploadPack(const TrainingPack& pack);
	void ShowLoadingModals();

	DownloadState downloadState;
	UploadState uploadState;
};
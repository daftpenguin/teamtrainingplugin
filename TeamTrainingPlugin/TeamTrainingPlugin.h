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
constexpr auto SERVER_URL = "http://localhost:8000"; // TODO: Make this a cvar?
constexpr int MAX_BALL_TICK_FAILURES = 3;
constexpr int MAX_BALL_VELOCITY_ZERO = 5;

constexpr int MAX_FILENAME_LENGTH = 128;
constexpr int MAX_DESCRIPTION_LENGTH = 500;
constexpr int MAX_CREATOR_LENGTH = 128;
constexpr int NUM_TAG_COLUMNS = 5;

const string CVAR_PREFIX("cl_team_training_");

/*
 * State stuff needed for GUI
 */

// TODO: This evolved into ugliness, fix it...
class TagsState : private boost::noncopyable
{
public:
	void refresh() {
		oldTags.clear();
		for (auto it = tags.begin(); it != tags.end(); ++it) {
			oldTags[it->first] = it->second;
		}
		resetState();
		is_downloading = true;
	}

	void retry() {
		resetState();
		is_downloading = true;
	}

	void cancel() {
		resetState();
		for (auto it = oldTags.begin(); it != oldTags.end(); ++it) {
			tags[it->first] = it->second;
		}
		oldTags.clear();
	}

	void clear() {
		for (auto it = tags.begin(); it != tags.end(); ++it) {
			it->second = false;
		}
	}

	void resetState() {
		has_downloaded = false;
		is_downloading = false;
		failed = false;
		cancelled = false;
		progress = 0;
		error = "";
		tags.clear();
	}

	void restoreSelected() {
		for (auto it = oldTags.begin(); it != oldTags.end(); ++it) {
			if (it->second) {
				auto newIt = tags.find(it->first);
				if (newIt != tags.end()) {
					tags[it->first] = true;
				}
			}
		}
		oldTags.clear();
	}

	void undoEdits() {
		// Cancelling the tag edits, though a refresh of the tags could cause changes (in the case where a previously selected tag was removed from the list of retrieved tags)
		for (auto it = tags.begin(); it != tags.end(); ++it) {
			tags[it->first] = false;
		}
		for (auto it = beforeEditEnabledTags.begin(); it != beforeEditEnabledTags.end(); ++it) {
			if (tags.find(*it) != tags.end()) {
				tags[*it] = true;
			}
		}
	}

	vector<string> GetEnabledTags() const;

	bool has_downloaded;
	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	vector<string> beforeEditEnabledTags; // If user cancels editing the tags, reset them
	map<string, bool> tags;               // Tag => is enabled?
	map<string, bool> oldTags;            // Old tag => is enabled? mapping so we can restore if user cancels refresh or it fails
	boost::mutex mutex;
};

class SearchFilterState : private boost::noncopyable {
public:
	SearchFilterState() : description(""), creator(""), code(""), offense(0), defense(0) {};

	void clear() {
		description[0] = 0;
		creator[0] = 0;
		code[0] = 0;
		offense = 0;
		defense = 0;
		tagsState.clear();
	}

	char description[MAX_DESCRIPTION_LENGTH];
	char creator[MAX_CREATOR_LENGTH];
	char code[20];
	int offense;
	int defense;

	TagsState tagsState;
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
	UploadState() : is_uploading(false), failed(false), cancelled(false), progress(0), error(""), pack_path(fs::path()), pack_code(""), mutex() {};

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
	fs::path pack_path;
	string pack_code;
	string uploaderID;
	string uploader;
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
	int selectedPackIdx = 0;
	std::vector<TrainingPack> packs;
	std::vector<TrainingPack> filteredPacks;
	SearchState localFilterState;
	// Downloads
	int downloadSelectedPackIdx = 0;
	SearchState searchState;
	// Creation
	int offensive_players = 0;
	int defensive_players = 0;
	int gui_num_drills = 1;
	char filename[MAX_FILENAME_LENGTH] = "";
	char creator[MAX_CREATOR_LENGTH] = "";
	char description[MAX_DESCRIPTION_LENGTH] = "";
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

	void AddSearchFilters(
		SearchFilterState& filterState, string idPrefix, bool alwaysShowSearchButton,
		std::function<void(SearchFilterState& filters)> tagLoader,
		std::function<void(SearchFilterState& filters)> searchCallback);
	void filterLocalPacks(SearchFilterState& filters);
	void searchPacksThread(SearchFilterState& filters);
	void SearchPacks(SearchFilterState& filters);
	void downloadTagsThread(SearchFilterState& filters);
	void loadLocalTagsThread(SearchFilterState& filters);
	void downloadPackThread();
	void DownloadPack(std::string code);
	void uploadPackThread();
	void UploadPack(const TrainingPack& pack);
	void ShowLoadingModals();
	void ShowTagsWindow(SearchFilterState& state, std::function<void(SearchFilterState& filters)> tagLoader);

	DownloadState downloadState;
	UploadState uploadState;
};

void to_json(json& j, const SearchFilterState& s);
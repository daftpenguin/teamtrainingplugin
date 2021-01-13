#pragma once

#include "bakkesmod/wrappers/gamewrapper.h"
#include "TrainingPack.h"
#include "nlohmann/json.hpp"

#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

using json = nlohmann::ordered_json;

constexpr int MAX_FILENAME_LENGTH = 128;
constexpr int MAX_DESCRIPTION_LENGTH = 500;
constexpr int MAX_CREATOR_LENGTH = 128;
constexpr int MAX_CREATOR_ID_LENGTH = 64; // Should be plenty, longest should be: Unknown:18446744073709551615
constexpr int MAX_NOTES_LENGTH = 2048;
constexpr int MAX_YOUTUBE_LENGTH = 1024;

/*
 * State stuff needed for GUI
 */

 // TODO: This evolved into ugliness, fix it...
class TagsState : private boost::noncopyable
{
public:
	TagsState() : has_downloaded(false), is_downloading(false), failed(false), cancelled(false), progress(0) {};
	void refresh();
	bool isRefreshing();
	void retry();
	void cancel();
	void setTags(std::map<std::string, bool> newTags);
	void addTags(std::map<std::string, bool> newTags);
	void addTags(std::vector<std::string> newTags);
	void unmarkTags();
	void resetState();
	void restoreSelected();
	void enableTagsPending(std::unordered_set<std::string> tags);
	void enableTagsPending(std::vector<std::string> tags);
	void undoEdits();
	std::vector<std::string> GetEnabledTags() const;

	bool has_downloaded;
	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	std::string error;
	std::vector<std::string> pendingEnabledTags;    // Tags to be enabled after refreshing or first time loading them
	std::vector<std::string> beforeEditEnabledTags; // If user cancels editing the tags, reset them
	std::map<std::string, bool> tags;               // Tag => is enabled?
	std::map<std::string, bool> oldTags;            // Old tag => is enabled? mapping so we can restore if user cancels refresh or it fails
	boost::mutex mutex;
};

class SearchFilterState : private boost::noncopyable {
public:
	SearchFilterState() : description(""), creator(""), code(""), offense(0), defense(0) {};
	void clear();

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
	void newSearch();
	void resetState();

	SearchFilterState filters;
	std::vector<TrainingPackDBMetaData> packs;

	bool is_searching;
	bool failed;
	std::string error;
	boost::mutex mutex;
};

class DownloadState : private boost::noncopyable
{
public:
	DownloadState() : stage(""), is_downloading(false), failed(false), cancelled(false), progress(0), error(""), pack_id(NO_UPLOAD_ID), pack_description(""), temFName(""), mutex() {};
	void newPack(int id, std::string code, std::string description);
	void resetState();

	std::string stage;
	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	std::string error;
	int pack_id;
	std::string pack_code;
	std::string pack_description;
	std::string temFName;
	boost::mutex mutex;
};

class UploadState : private boost::noncopyable
{
public:
	UploadState() : is_uploading(false), failed(false), cancelled(false), progress(0), error(""), pack_path(std::filesystem::path()), pack_code(""), pack_description(""), mutex() {};
	void newPack(TrainingPack pack);
	void resetState();

	bool is_uploading;
	bool failed;
	bool cancelled;
	float progress;
	std::string error;
	std::filesystem::path pack_path;
	std::string pack_code;
	std::string pack_description;
	std::string uploaderID;
	std::string uploader;
	boost::mutex mutex;
};

class UploadFavoritesState : private boost::noncopyable
{
public:
	UploadFavoritesState() : was_started(false), is_running(false), failed(false), cancelled(false), progress(0), error("") {};
	void resetState();

	bool was_started;
	bool is_running;
	bool failed;
	bool cancelled;
	float progress;
	std::string error;

	std::unordered_set<std::string> packsInProgress;
	std::unordered_set<std::string> packsCompleted;
};

class InGameTrainingPackData
{
public:
	InGameTrainingPackData() : code(""), description(""), creator(""), creatorID(""), numDrills(0), failed(false) {};
	void resetState();
	int fixGUIDrills(int off, int def, int guiDrills);

	char code[20] = "";
	char creator[MAX_CREATOR_LENGTH] = "";
	char creatorID[MAX_CREATOR_ID_LENGTH] = "";
	char description[MAX_DESCRIPTION_LENGTH] = "";
	int numDrills;
	//unordered_set<string> tags; // TODO: How do we get this through BakkesMod?

	bool failed = false;
};

std::string UIDToString(UniqueIDWrapper uid);

void to_json(json& j, const SearchFilterState& s);
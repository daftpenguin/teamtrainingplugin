#pragma once

#include "bakkesmod/wrappers/gamewrapper.h"
#include "TrainingPack.h"
#include "nlohmann/json.hpp"

#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>

using json = nlohmann::ordered_json;
using namespace std;

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
	void setTags(map<string, bool> newTags);
	void addTags(map<string, bool> newTags);
	void addTags(vector<string> newTags);
	void unmarkTags();
	void resetState();
	void restoreSelected();
	void enableTagsPending(unordered_set<string> tags);
	void enableTagsPending(vector<string> tags);
	void undoEdits();
	vector<string> GetEnabledTags() const;

	bool has_downloaded;
	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	vector<string> pendingEnabledTags;    // Tags to be enabled after refreshing or first time loading them
	vector<string> beforeEditEnabledTags; // If user cancels editing the tags, reset them
	map<string, bool> tags;               // Tag => is enabled?
	map<string, bool> oldTags;            // Old tag => is enabled? mapping so we can restore if user cancels refresh or it fails
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
	vector<TrainingPackDBMetaData> packs;

	bool is_searching;
	bool failed;
	string error;
	boost::mutex mutex;
};

class DownloadState : private boost::noncopyable
{
public:
	DownloadState() : stage(""), is_downloading(false), failed(false), cancelled(false), progress(0), error(""), pack_id(NO_UPLOAD_ID), pack_description(""), temFName(""), mutex() {};
	void newPack(int id, string code, string description);
	void resetState();

	string stage;
	bool is_downloading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	int pack_id;
	string pack_code;
	string pack_description;
	string temFName;
	boost::mutex mutex;
};

class UploadState : private boost::noncopyable
{
public:
	UploadState() : is_uploading(false), failed(false), cancelled(false), progress(0), error(""), pack_path(fs::path()), pack_code(""), pack_description(""), mutex() {};
	void newPack(TrainingPack pack);
	void resetState();

	bool is_uploading;
	bool failed;
	bool cancelled;
	float progress;
	string error;
	fs::path pack_path;
	string pack_code;
	string pack_description;
	string uploaderID;
	string uploader;
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
	string error;

	unordered_set<string> packsInProgress;
	unordered_set<string> packsCompleted;
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

string UIDToString(UniqueIDWrapper uid);

void to_json(json& j, const SearchFilterState& s);
#include "GUIStates.h"

using namespace std;

// Prepares tag state for being refreshed with tags and marks it as being refreshed
void TagsState::refresh() {
	oldTags.clear();
	for (auto it = tags.begin(); it != tags.end(); ++it) {
		oldTags[it->first] = it->second;
	}
	resetState();
	is_downloading = true;
}

bool TagsState::isRefreshing() {
	return is_downloading;
}

void TagsState::retry() {
	resetState();
	is_downloading = true;
}

void TagsState::cancel() {
	resetState();
	for (auto it = oldTags.begin(); it != oldTags.end(); ++it) {
		tags[it->first] = it->second;
	}
	oldTags.clear();
}

void TagsState::setTags(map<string, bool> newTags) {
	tags.clear();
	addTags(newTags);
}

void TagsState::addTags(map<string, bool> newTags) {
	for (auto it = newTags.begin(); it != newTags.end(); ++it) {
		if (tags.find(it->first) == tags.end()) {
			tags[it->first] = false;
		}
	}
}

void TagsState::addTags(vector<string> newTags) {
	for (auto& tag : newTags) {
		if (tags.find(tag) == tags.end()) {
			tags[tag] = false;
		}
	}
}

void TagsState::unmarkTags() {
	for (auto it = tags.begin(); it != tags.end(); ++it) {
		it->second = false;
	}
}

void TagsState::resetState() {
	has_downloaded = false;
	is_downloading = false;
	failed = false;
	cancelled = false;
	progress = 0;
	error = "";
	tags.clear();
}

void TagsState::restoreSelected() {
	for (auto it = oldTags.begin(); it != oldTags.end(); ++it) {
		if (it->second) {
			auto newIt = tags.find(it->first);
			if (newIt != tags.end()) {
				tags[it->first] = true;
			}
		}
	}
}

// Add tags to be enabled after refreshing or initial loading of tags
void TagsState::enableTagsPending(unordered_set<string> tags) {
	pendingEnabledTags.clear();
	for (auto& tag : tags) {
		pendingEnabledTags.push_back(tag);
	}
}

// Add tags to be enabled after refreshing or initial loading of tags
void TagsState::enableTagsPending(vector<string> tags) {
	pendingEnabledTags.clear();
	pendingEnabledTags = tags;
}

void TagsState::undoEdits() {
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

vector<string> TagsState::GetEnabledTags() const {
	vector<string> tags;
	for (auto it = this->tags.begin(); it != this->tags.end(); ++it) {
		if (it->second) {
			tags.push_back(it->first);
		}
	}
	return tags;
}

map<OnlinePlatform, string> PLATFORM_TO_STRING = {
	{ OnlinePlatform_Unknown, "Unknown" },
	{ OnlinePlatform_Steam, "Steam" },
	{ OnlinePlatform_PS4, "PS4" },
	{ OnlinePlatform_PS3, "PS3" },
	{ OnlinePlatform_Dingo, "XBOX" },
	{ OnlinePlatform_QQ, "QQ" },
	{ OnlinePlatform_OldNNX, "OldNNX" },
	{ OnlinePlatform_NNX, "NNX" },
	{ OnlinePlatform_PsyNet, "PsyNet" },
	{ OnlinePlatform_Deleted, "Deleted" },
	{ OnlinePlatform_WeGame, "WeGame" },
	{ OnlinePlatform_Epic, "Epic" }
};

string UIDToString(UniqueIDWrapper uidw)
{
	auto uidL = uidw.GetUID();
	string uid = (uidL == 0 ? uidw.GetEpicAccountID() : to_string(uidL));
	return PLATFORM_TO_STRING[uidw.GetPlatform()] + ":" + uid;
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

void SearchFilterState::clear()
{
	description[0] = 0;
	creator[0] = 0;
	code[0] = 0;
	offense = 0;
	defense = 0;
	tagsState.unmarkTags();
}

void SearchState::newSearch()
{
	resetState();
	is_searching = true;
}

void SearchState::resetState()
{
	is_searching = false;
	failed = false;
	error = "";
}

void DownloadState::newPack(int id, string code, string description)
{
	resetState();
	stage = "";
	pack_id = id;
	pack_code = code;
	pack_description = description;
	is_downloading = true;
}

void DownloadState::resetState()
{
	stage = "";
	is_downloading = false;
	failed = false;
	cancelled = false;
	progress = 0;
	error = "";
}

void UploadState::newPack(TrainingPack pack)
{
	resetState();
	pack_path = pack.filepath;
	pack_code = pack.code;
	pack_description = pack.description;
	is_uploading = true;
}

void UploadState::resetState()
{
	is_uploading = false;
	failed = false;
	cancelled = false;
	progress = 0;
	error = "";
}

void UploadFavoritesState::resetState()
{
	was_started = false;
	is_running = false;
	failed = false;
	cancelled = false;
	progress = 0;
	error = "";
	packsInProgress.clear();
	packsCompleted.clear();
}

void InGameTrainingPackData::resetState()
{
	code[0] = 0;
	description[0] = 0;
	creator[0] = 0;
	creatorID[0] = 0;
	numDrills = 0;
	failed = false;
}

int InGameTrainingPackData::fixGUIDrills(int off, int def, int guiDrills)
{
	if ((off + def) * guiDrills <= numDrills) return guiDrills; // Can't determine user's intentions, assume they're correct
	return numDrills / (off + def);
}

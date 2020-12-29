#pragma once

#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/GameEvent/ReplayDirectorWrapper.h"
#include "bakkesmod/wrappers/WrapperStructs.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <unordered_set>

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;
using namespace std;

static constexpr int NO_UPLOAD_ID = -1;
static constexpr int LATEST_TRAINING_PACK_VERSION = 4;

struct TrainingPackDBMetaData {
	int id;
	string code;
	string description;
	string creator;
	int offense;
	int defense;
	int num_drills;
	int downloads;
	unordered_set<string> tags;
};

struct TrainingPackBall {
	Vector location;
	Vector velocity;
	Vector torque;
	Rotator rotation;
	Vector angular;

	inline TrainingPackBall clone()
	{
		return TrainingPackBall{
			location.clone(),
			velocity.clone(),
			torque.clone(),
			Rotator{ rotation.Pitch, rotation.Yaw, rotation.Roll },
			angular.clone()
		};
	}
};

struct TrainingPackPlayer {
	float boost = 100;
	Vector location;
	Vector velocity;
	Rotator rotation;

	TrainingPackPlayer() : boost(100) {};
	TrainingPackPlayer(float boost, Vector location, Vector velocity, Rotator rotation)
		: boost(boost), location(location.clone()), velocity(velocity.clone()), rotation(Rotator(rotation)) {};

	inline TrainingPackPlayer clone() {
		return TrainingPackPlayer{
			boost,
			location.clone(),
			velocity.clone(),
			Rotator{ rotation.Pitch, rotation.Yaw, rotation.Roll }
		};
	}
};

struct TrainingPackDrill {
	TrainingPackBall ball;
	vector<TrainingPackPlayer> passers;
	TrainingPackPlayer shooter;
	vector<TrainingPackPlayer> defenders;
};

class TrainingPack
{
public:
	TrainingPack();
	TrainingPack(string filepath) : TrainingPack(fs::path(filepath)) {};
	TrainingPack(fs::path filepath);
	
	string errorMsg = ""; // Use this to display error messages in pack selection tab
	chrono::system_clock::time_point load_time;
	fs::path filepath;
	int version = 0;
	int offense = 0;
	int defense = 0;

	// Version 2 data
	string description;
	string creator;
	string code;

	// Version 3 data only added angular field to the ball

	// Version 4 data
	string creatorID;
	string uploader;
	string uploaderID;
	int uploadID;
	unordered_set<string> tags;
	int numDrills;
	string notes;
	string youtube;

	vector<TrainingPackDrill> drills;

	// Things needed for pack creation, properties should not be imported/exported
	TrainingPack(fs::path filepath, int offense, int defense, int numDrills, string code,
		string creator, string creatorID, string description, string notes,
		string youtube, unordered_set<string> tags);
	char* save();
	void addBallLocation(BallWrapper ball);
	void setBallMovement(BallWrapper ball);
	void addPlayer(CarWrapper car);
	void startNewDrill();
	bool lastPlayerAddedWasFirstPasser();
	bool allPlayersInDrillAdded();
	bool expectingMoreDrills();

	bool setTags(vector<string> tags);
	void addTag(string tag);
	void removeTag(string tag);

	int expected_drills;
	int players_added = 0;
	bool data_saved = false;
	bool ball_location_set = false;
	bool ball_velocity_set = false;
	bool shooter_added = false;
};

/* JSON Serialization/Deserialization stuff */

struct TrainingPackPlayerWithRole {
	TrainingPackPlayer player;
	string role;
};

void from_json(const json& j, TrainingPack& p);
void to_json(json& j, const TrainingPack& p);
void from_json(const json& j, TrainingPackDrill& d);
void to_json(json& j, const TrainingPackDrill& d);
void from_json(const json& j, TrainingPackBall& b);
void to_json(json& j, const TrainingPackBall& b);
void from_json(const json& j, TrainingPackPlayer& p);
void to_json(json& j, const TrainingPackPlayer& p);
void from_json(const json& j, TrainingPackPlayerWithRole& p);
void to_json(json& j, const TrainingPackPlayerWithRole& p);
void from_json(const json& j, Vector& v);
void to_json(json& j, const Vector& v);
void from_json(const json& j, Rotator& r);
void to_json(json& j, const Rotator& r);
#pragma once

#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct TrainingPackDBMetaData {
	std::string code;
	std::string description;
	std::string creator;
	int offense;
	int defense;
	int num_drills;
	int downloads;
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
	float boost;
	Vector location;
	Vector velocity;
	Rotator rotation;

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
	std::vector<TrainingPackPlayer> passers;
	TrainingPackPlayer shooter;
	std::vector<TrainingPackPlayer> defenders;
};

class TrainingPack
{
public:
	//TrainingPack(std::string filepath, int offense, int defense, std::string description, std::string creator, std::string code);
	TrainingPack(std::string filepath) : TrainingPack(fs::path(filepath)) {};
	TrainingPack(fs::path filepath);
	void load(std::string filepath);
	//void addDrill(TrainingPackDrill drill);
	//void save();

	std::string errorMsg = ""; // Use this to display error messages in pack selection tab
	std::chrono::system_clock::time_point load_time;
	std::string filepath;
	unsigned int version = 0;
	unsigned int offense = 0;
	unsigned int defense = 0;
	std::string description;
	std::string creator;
	std::string code;
	std::vector<TrainingPackDrill> drills;

private:
	TrainingPackDrill parseDrill(json js_drill);
	TrainingPackBall parseBall(json js_ball);
	TrainingPackPlayer parsePlayer(json js_player);
};


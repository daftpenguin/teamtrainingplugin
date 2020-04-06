#pragma once

#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

using json = nlohmann::json;

struct TrainingPackBall {
	Vector location;
	Vector velocity;
	Vector torque;
	Rotator rotation;
};

struct TrainingPackPlayer {
	float boost;
	Vector location;
	Vector velocity;
	Rotator rotation;
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
	TrainingPack(std::string filepath, std::shared_ptr<CVarManagerWrapper> cvarManager);
	void load(std::string filepath, std::shared_ptr<CVarManagerWrapper> cvarManager);

	std::string filepath;
	unsigned int version = 0;
	unsigned int offense = 0;
	unsigned int defense = 0;
	std::string description;
	std::string creator;
	std::string code;
	std::vector<TrainingPackDrill> drills;

private:
	TrainingPackDrill parseDrill(json js_drill, std::shared_ptr<CVarManagerWrapper> cvarManager);
	TrainingPackBall parseBall(json js_ball, std::shared_ptr<CVarManagerWrapper> cvarManager);
	TrainingPackPlayer parsePlayer(json js_player, std::shared_ptr<CVarManagerWrapper> cvarManager);
};


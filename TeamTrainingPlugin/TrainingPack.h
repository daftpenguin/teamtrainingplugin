#pragma once

#include "bakkesmod/wrappers/wrapperstructs.h"
#include "bakkesmod/wrappers/CVarManagerWrapper.h"
#include "nlohmann/json.h"
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
	TrainingPack(std::string file_path, std::shared_ptr<CVarManagerWrapper> cvarManager);
	void load(std::string file_path, std::shared_ptr<CVarManagerWrapper> cvarManager);

	std::string name;
	unsigned int offense;
	unsigned int defense;
	std::vector<TrainingPackDrill> drills;

private:
	TrainingPackDrill parseDrill(json js_drill, std::shared_ptr<CVarManagerWrapper> cvarManager);
	TrainingPackBall parseBall(json js_ball, std::shared_ptr<CVarManagerWrapper> cvarManager);
	TrainingPackPlayer parsePlayer(json js_player, std::shared_ptr<CVarManagerWrapper> cvarManager);
};


#include "TrainingPack.h"
#include <fstream>
#include <sstream>
#include <Windows.h>

static inline bool file_exists(const std::string &file_path)
{
	GetFileAttributes(file_path.c_str());
	return !(INVALID_FILE_ATTRIBUTES == GetFileAttributes(file_path.c_str()) && GetLastError() == ERROR_FILE_NOT_FOUND);
}

TrainingPack::TrainingPack(std::string file_path, std::shared_ptr<CVarManagerWrapper> cvarManager)
{
	if (file_exists(file_path)) {
		load(file_path, cvarManager);
	}
	else {
		file_path = ".\\bakkesmod\\data\\teamtraining\\" + file_path + ".json";
		if (file_exists(file_path)) {
			load(file_path, cvarManager);
		}
		else {
			cvarManager->log("Failed to find pack at " + file_path);
			return;
		}
	}
}

void TrainingPack::load(std::string file_path, std::shared_ptr<CVarManagerWrapper> cvarManager)
{
	std::ifstream inFile;
	inFile.open(file_path);

	std::stringstream ss;
	ss << inFile.rdbuf();
	json js = json::parse(ss.str());

	name = js["name"].get<std::string>();
	offense = js["offense"].get<unsigned int>();
	defense = js["defense"].get<unsigned int>();

	for (json js_drill : js["drills"]) {
		drills.push_back(parseDrill(js_drill, cvarManager));
	}
}

TrainingPackDrill TrainingPack::parseDrill(json js_drill, std::shared_ptr<CVarManagerWrapper> cvarManager)
{
	TrainingPackDrill drill;

	drill.ball = parseBall(js_drill["ball"], cvarManager);

	for (json js_player : js_drill["players"]) {
		std::string role = js_player["role"].get<std::string>();
		TrainingPackPlayer player = parsePlayer(js_player, cvarManager);
		if (role.compare("passer") == 0) {
			drill.passers.push_back(player);
		}
		else if (role.compare("shooter") == 0) {
			drill.shooter = player;
		}
		else if (role.compare("defender") == 0) {
			drill.defenders.push_back(player);
		}
	}

	return drill;
}

Vector parseVector(json js_vector)
{
	return Vector(
		js_vector["x"].get<float>(),
		js_vector["y"].get<float>(),
		js_vector["z"].get<float>()
	);
}

Rotator parseRotator(json js_rotator)
{
	return Rotator(
		js_rotator["pitch"].get<int>(),
		js_rotator["yaw"].get<int>(),
		js_rotator["roll"].get<int>()
	);
}

TrainingPackBall TrainingPack::parseBall(json js_ball, std::shared_ptr<CVarManagerWrapper> cvarManager)
{
	TrainingPackBall ball;
	ball.location = parseVector(js_ball["location"]);
	ball.velocity = parseVector(js_ball["velocity"]);
	ball.torque = parseVector(js_ball["torque"]);
	ball.rotation = parseRotator(js_ball["rotation"]);
	return ball;
}

TrainingPackPlayer TrainingPack::parsePlayer(json js_player, std::shared_ptr<CVarManagerWrapper> cvarManager)
{
	TrainingPackPlayer player;
	player.boost = js_player["boost"].get<float>();
	player.location = parseVector(js_player["location"]);
	player.velocity = parseVector(js_player["velocity"]);
	player.rotation = parseRotator(js_player["rotation"]);
	return player;
}
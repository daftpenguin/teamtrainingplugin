// TODO: Record all ball positions/trajectories for future updates

#include "TrainingPack.h"

#include <fstream>
#include <sstream>
#include <Windows.h>


TrainingPack::TrainingPack(fs::path filepath) : filepath(filepath.string())
{
	load_time = std::chrono::system_clock::now();
	if (fs::exists(filepath)) {
		load(filepath.string());
	}
	else {
		errorMsg = "Pack not found";
	}
}

void TrainingPack::load(std::string filepath)
{
	std::ifstream inFile;
	inFile.open(filepath);

	std::stringstream ss;
	ss << inFile.rdbuf();

	json js;
	try {
		js = json::parse(ss.str());
	}
	catch (...) {
		errorMsg = "Error parsing json";
		return;
	}

	if (js.find("version") == js.end()) {
		// If no version exists, first version which only contains name, offense, and defense
		description = js["name"].get<std::string>();
		version = 1;
	}
	else {
		version = js["version"].get<unsigned int>();
		if (version >= 2) {
			description = js["description"].get<std::string>();
			creator = js["creator"].get<std::string>();
			code = js["code"].get<std::string>();
		}
	}

	offense = js["offense"].get<unsigned int>();
	defense = js["defense"].get<unsigned int>();
	this->filepath = filepath;

	for (json js_drill : js["drills"]) {
		drills.push_back(parseDrill(js_drill));
	}

	inFile.close();
}

TrainingPackDrill TrainingPack::parseDrill(json js_drill)
{
	TrainingPackDrill drill;

	drill.ball = parseBall(js_drill["ball"]);

	for (json js_player : js_drill["players"]) {
		std::string role = js_player["role"].get<std::string>();
		TrainingPackPlayer player = parsePlayer(js_player);
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

TrainingPackBall TrainingPack::parseBall(json js_ball)
{
	TrainingPackBall ball;
	ball.location = parseVector(js_ball["location"]);
	ball.velocity = parseVector(js_ball["velocity"]);
	ball.torque = parseVector(js_ball["torque"]);
	ball.rotation = parseRotator(js_ball["rotation"]);
	if (js_ball.contains("angular")) {
		ball.angular = parseVector(js_ball["angular"]);
	} else {
		ball.angular = Vector{ 0, 0, 0 };
	}
	return ball;
}

TrainingPackPlayer TrainingPack::parsePlayer(json js_player)
{
	TrainingPackPlayer player;
	player.boost = js_player["boost"].get<float>();
	player.location = parseVector(js_player["location"]);
	player.velocity = parseVector(js_player["velocity"]);
	player.rotation = parseRotator(js_player["rotation"]);
	return player;
}
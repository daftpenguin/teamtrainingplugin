// TODO: Defenders only drills, allow for consecutive or even simultenous shots (multiple balls) to be taken on net that you have to save
// TODO: Add targets to passes and shots, and add single player or no shooter mode to practice these drills when you don't have enough offensive players
// TODO: Add target on ball that moves based on the player position and ball position in order to hit the ball perfectly at a target

#include "TeamTrainingPlugin.h"

#include "bakkesmod\wrappers\includes.h"
#include "bakkesmod\wrappers\GameEvent\ReplayDirectorWrapper.h"
#include "bakkesmod\wrappers\WrapperStructs.h"
#include "utils\parser.h"

#include <algorithm>
#include <random>
#include <experimental\filesystem>

namespace fs = std::experimental::filesystem;

#pragma comment (lib, "Ws2_32.lib")

BAKKESMOD_PLUGIN(TeamTrainingPlugin, "Team Training plugin", "0.1", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING )

const std::string CVAR_PREFIX("cl_team_training_");

std::string vectorString(Vector v) {
	std::stringstream ss;
	ss << "{ \"x\": " << v.X << ", \"y\": " << v.Y << ", \"z\": " << v.Z << " }";
	return ss.str();
}

std::string rotationString(Rotator r) {
	std::stringstream ss;
	ss << "{ \"pitch\": " << r.Pitch << ", \"yaw\": " << r.Yaw << ", \"roll\": " << r.Roll << " }";
	return ss.str();
}

std::string vectorToString(std::vector<unsigned int> v) {
	std::stringstream ss;
	for (size_t i = 0; i < v.size(); i++) {
		if (i != 0)
			ss << ",";
		ss << v[i];
	}
	return ss.str();
}

void TeamTrainingPlugin::onLoad()
{
	cvarManager->registerNotifier("team_train_load", std::bind(&TeamTrainingPlugin::onLoadTrainingPack, this, std::placeholders::_1), 
		"Launches given team training pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_reset", std::bind(&TeamTrainingPlugin::onResetShot, this, std::placeholders::_1),
		"Resets the current shot", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_next", std::bind(&TeamTrainingPlugin::onNextShot, this, std::placeholders::_1),
		"Loads the next shot in the pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_prev", std::bind(&TeamTrainingPlugin::onPrevShot, this, std::placeholders::_1),
		"Loads the previous shot in the pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_randomize_players", std::bind(&TeamTrainingPlugin::randomizePlayers, this, std::placeholders::_1),
		"Randomizes the assignments of players to roles", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_cycle_players", std::bind(&TeamTrainingPlugin::cyclePlayers, this, std::placeholders::_1),
		"Cycles the assignments of players to roles", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_list", std::bind(&TeamTrainingPlugin::listPacks, this, std::placeholders::_1),
		"Lists available packs", PERMISSION_ALL);

	cvarManager->registerNotifier("write_shot_info", std::bind(&TeamTrainingPlugin::writeShotInfo, this, std::placeholders::_1),
		"Print car and ball location of current training drill", PERMISSION_CUSTOM_TRAINING | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_internal_convert", std::bind(&TeamTrainingPlugin::internalConvert, this, std::placeholders::_1),
		"Converts custom training pack into team training pack (intended for internal use through GUI)", PERMISSION_CUSTOM_TRAINING | PERMISSION_PAUSEMENU_CLOSED);

	// Random commands that may be useful for team training?
	//cvarManager->registerNotifier("team_train_add_ball", std::bind(&TeamTrainingPlugin::addNewBall, this, std::placeholders::_1),
	//	"Spawns a new ball", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);

	/*
	cvarManager->registerNotifier("team_train_two_ball_drill", std::bind(&TeamTrainingPlugin::twoBallTraining, this, std::placeholders::_1),
		"Save the shot using the ball", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	*/
	
	// Variables
	cvarManager->registerCvar(CVAR_PREFIX + "randomize", "0", "Randomize the shots in a training pack", true, true, 0, true, 1, true);
	cvarManager->registerCvar(CVAR_PREFIX + "countdown", "1", "Time to wait until shot begins", true, true, 0, true, 10, true);
}

void TeamTrainingPlugin::onUnload()
{
	if (!pack) {
		cvarManager->loadCfg("team_training_bindings_backup.cfg");
		pack = NULL;
	}
}

void TeamTrainingPlugin::onLoadTrainingPack(std::vector<std::string> params)
{
	cvarManager->log("Team training now has a UI. F2 -> Plugins -> Team Training Plugin and click the button :)");

	if (!gameWrapper->IsInFreeplay()) {
		return;
	}
	ServerWrapper tutorial = gameWrapper->GetGameEventAsServer();

	if (!pack) {
		// First time loading, set bindings
		cvarManager->backupBinds("bakkesmod\\cfg\\team_training_bindings_backup.cfg");
		cvarManager->loadCfg("team_training.cfg");
		//gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.ResetPlayers", std::bind(&TeamTrainingPlugin::onResetShotEvent, this, std::placeholders::_1));
		gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", std::bind(&TeamTrainingPlugin::onFreeplayDestroyed, this, std::placeholders::_1));
	}

	if (params.size() < 2) {
		cvarManager->log("Must provide the name of an existing training pack");
		return;
	}

	std::string pack_name = params[1];
	if (pack_name.size() < 5 || pack_name.substr(pack_name.size() - 5).compare(".json") != 0) {
		pack_name += ".json";
	}
	
	this->pack = std::make_shared<TrainingPack>(params[1], cvarManager);
	if (this->pack->filepath == "") {
		this->pack = NULL;
		cvarManager->log("Given training pack is either invalid or does not exist");
		return;
	}

	cvarManager->log("Loaded training pack: " + this->pack->filepath);
	if (cvarManager->getCvar(CVAR_PREFIX + "randomize").getBoolValue()) {
		auto rng = std::default_random_engine{};
		std::shuffle(std::begin(pack->drills), std::end(pack->drills), rng);
	}

	if (!validatePlayers(tutorial)) {
		return;
	}

	ArrayWrapper<CarWrapper> cars = tutorial.GetCars();
	player_order.clear();
	for (int i = 0; i < cars.Count(); i++) {
		player_order.push_back(i);
	}

	setShot(0);
}

void TeamTrainingPlugin::onFreeplayDestroyed(std::string eventName)
{
	cvarManager->loadCfg("team_training_bindings_backup.cfg");
	pack = NULL;
}

void TeamTrainingPlugin::setShot(int shot)
{
	if (!gameWrapper->IsInFreeplay()) {
		cvarManager->log("Not in freeplay");
		return;
	}
	
	if (!pack) {
		cvarManager->log("No pack loaded");
		return;
	}

	ServerWrapper tutorial = gameWrapper->GetGameEventAsServer();

	unsigned int shot_set = last_shot_set + 1;
	last_shot_set = shot_set;

	if (!validatePlayers(tutorial)) {
		return;
	}

	current_shot = shot;

	TrainingPackDrill drill = pack->drills[shot];

	ArrayWrapper<BallWrapper> balls = tutorial.GetGameBalls();
	if (balls.Count() < 1) {
		cvarManager->log("Cannot retrieve ball");
	}
	BallWrapper ball = balls.Get(0);
	ArrayWrapper<CarWrapper> cars = tutorial.GetCars();

	// Stop all cars and ball
	for (int i = 0; i < cars.Count(); i++) {
		cars.Get(i).Stop();
	}
	ball.Stop();

	ball.SetLocation(drill.ball.location);

	int i = 0;
	for (auto player : drill.passers) {
		if (player_order[i] >= cars.Count()) {
			cvarManager->log("Not enough cars for passer positions");
			return;
		}
		CarWrapper car = cars.Get(player_order[i++]);
		setPlayerToCar(player, car);
	}

	if (i >= cars.Count()) {
		cvarManager->log("Cannot find shooter");
		return;
	}

	setPlayerToCar(drill.shooter, cars.Get(player_order[i++]));
	
	float countdown = cvarManager->getCvar(CVAR_PREFIX + "countdown").getFloatValue();
	gameWrapper->SetTimeout([&, &_cvarManager = cvarManager, shot_set](GameWrapper *gw) {
		if (!gameWrapper->IsInFreeplay()) {
			return;
		}

		// Don't do anything if a new shot was set before timeout is called
		if (last_shot_set != shot_set) {
			return;
		}

		ServerWrapper sw = gw->GetGameEventAsServer();
		ArrayWrapper<CarWrapper> cars = sw.GetCars();

		BallWrapper ball = sw.GetBall();
		TrainingPackDrill drill = pack->drills[current_shot];
		ball.SetRotation(drill.ball.rotation);
		ball.SetVelocity(drill.ball.velocity);
	}, max(0.0f, countdown));
}

void TeamTrainingPlugin::randomizePlayers(std::vector<std::string> params)
{
	auto rng = std::default_random_engine{};
	std::shuffle(std::begin(player_order), std::end(player_order), rng);
	cvarManager->log("Player order: " + vectorToString(player_order));
	resetShot();
}

void TeamTrainingPlugin::cyclePlayers(std::vector<std::string> params)
{
	unsigned int first = player_order.front();
	for (int i = 0; i < player_order.size() - 1; i++) {
		player_order[i] = player_order[i + 1];
	}
	player_order[player_order.size() - 1] = first;
	cvarManager->log("Player order: " + vectorToString(player_order));
	resetShot();
}

void TeamTrainingPlugin::listPacks(std::vector<std::string> params)
{
	auto const& packs = getTrainingPacks();

	if (packs.size() > 0) {
		cvarManager->log("Available team training packs:");
		for (auto const& pack : packs) {
			cvarManager->log(pack.first);
		}
	}
	else {
		cvarManager->log("No team training packs in data\\teamtraining directory");
	}
}

void TeamTrainingPlugin::onResetShotEvent(std::string eventName)
{
	resetShot();
}

void TeamTrainingPlugin::onResetShot(std::vector<std::string> params)
{
	resetShot();
}

void TeamTrainingPlugin::resetShot()
{
	if (!gameWrapper->IsInFreeplay()) {
		return;
	}

	setShot(current_shot);
}

void TeamTrainingPlugin::onNextShot(std::vector<std::string> params)
{
	if (!pack) {
		cvarManager->log("No pack loaded");
		return;
	}

	int shot = current_shot + 1;
	if (shot >= pack->drills.size()) {
		shot = 0;
	}

	setShot(shot);
}

void TeamTrainingPlugin::onPrevShot(std::vector<std::string> params)
{
	if (!pack) {
		cvarManager->log("No pack loaded");
		return;
	}

	int shot = current_shot - 1;
	if (shot < 0) {
		shot = pack->drills.size() - 1;
	}

	setShot(shot);
}

void TeamTrainingPlugin::setPlayerToCar(TrainingPackPlayer player, CarWrapper car)
{
	car.GetBoostComponent().SetBoostAmount(player.boost);
	car.SetLocation(player.location);
	car.SetRotation(player.rotation);
}

bool TeamTrainingPlugin::validatePlayers(ServerWrapper tutorial)
{
	// Validate the number of players
	int num_players = tutorial.GetCars().Count();
	if (num_players < pack->offense) {
		cvarManager->log("Pack requires at least " + std::to_string(pack->offense) + " players but there are only " + std::to_string(num_players));
		return false;
	}

	return true;
}


/* For extracting custom training pack to team training pack */

Rotator cloneRotation(Rotator r) {
	return Rotator(r.Pitch, r.Yaw, r.Roll);
}

void TeamTrainingPlugin::internalConvert(std::vector<std::string> params) {
	if (!gameWrapper->IsInCustomTraining()) {
		cvarManager->log("Not in custom training");
		return;
	}

	this->offense = std::atoi(params[1].c_str());
	this->defense = std::atoi(params[2].c_str());
	this->num_drills = std::atoi(params[3].c_str());
	this->drills_written = 0;

	std::string filename = params[4];
	std::string creator = params[5];
	std::string description = params[6];
	std::string code = params[7];

	cvarManager->log("Saving data for pack: " + filename);
	custom_training_export_file.open("bakkesmod\\data\\teamtraining\\" + filename + ".json");
	custom_training_export_file
		<< "{\n\"version\": 2,\n\t\"description\": \"" << description << "\",\n"
		<< "\t\"creator\": \"" << creator << "\",\n"
		<< "\t\"code\": \"" << code << "\",\n"
		<< "\t\"offense\": " << offense << ",\n\t\"defense\": " << defense << ",\n\t\"drills\": [\n";
	getNextShot();
}

void TeamTrainingPlugin::writeShotInfo(std::vector<std::string> params)
{
	cvarManager->log("Team training now has a UI. F2 -> Plugins -> Team Training Plugin and click the button :)");

	if (!gameWrapper->IsInCustomTraining()) {
		cvarManager->log("Not in custom training");
		return;
	}

	if (params.size() < 5) {
		cvarManager->log("Usage: " + params[0] + " <num_offensive> <num_defensive> <total_team_drills> <drill_name>");
		return;
	}

	if (params.size() > 5) {
		cvarManager->log("Spaces are not allowed in drill name");
		return;
	}

	offense = std::atoi(params[1].c_str());
	defense = std::atoi(params[2].c_str());
	num_drills = std::atoi(params[3].c_str());
	std::string drill_name = params[4].c_str();

	//TrainingEditorSaveDataWrapper training = TrainingEditorWrapper(gameWrapper->GetGameEventAsServer().memory_address).GetTrainingData().GetTrainingData();
	//TrainingEditorWrapper t = TrainingEditorWrapper(gameWrapper->GetGameEventAsServer().memory_address);
	/*
	// Getting the number of rounds seems to be broken?
	int rounds = training.GetNumRounds();
	cvarManager->log("Rounds: " + std::to_string(rounds));
	if (rounds % (offense + defense) != 0) {
		cvarManager->log("Incorrect number of rounds for " + std::to_string(offense) + " offensive players and " + std::to_string(defense) + " defensive players");
		return;
	}
	num_drills = rounds / (offense + defense);
	*/
	// drill_name = training.GetCode().ToString();

	cvarManager->log("Saving data for pack: " + drill_name);
	custom_training_export_file.open("bakkesmod\\data\\teamtraining\\" + drill_name + ".json");
	std::string creator("Unknown");
	std::string description("Unknown");
	custom_training_export_file
		<< "{\n\t\"name\": \"" << description << " by " << creator << "\",\n"
		<< "\t\"offense\": " << offense << ",\n\t\"defense\": " << defense << ",\n\t\"drills\": [\n";
	getNextShot();
}

void TeamTrainingPlugin::getNextShot()
{
	ServerWrapper server = gameWrapper->GetGameEventAsServer();

	ArrayWrapper<CarWrapper> cars = server.GetCars();
	CarWrapper car = cars.Get(0);
	TrainingPackPlayer p = TrainingPackPlayer{ 33.0f, car.GetLocation().clone(), Vector(0), cloneRotation(car.GetRotation()) };
	if (custom_training_players.size() < offense) { // Push to front first for passers
		custom_training_players.insert(custom_training_players.begin(), p);
	}
	else {
		custom_training_players.push_back(p);
	}

	if (custom_training_players.size() == offense) {
		// Last offensive shot in drill, get ball and setup to get velocity
		BallWrapper ball = server.GetBall();
		custom_training_ball.location = ball.GetLocation().clone();
		custom_training_ball.rotation = cloneRotation(ball.GetRotation());
		gameWrapper->HookEventWithCaller<ServerWrapper>("Function GameEvent_Soccar_TA.Active.Tick", std::bind(&TeamTrainingPlugin::onBallTick, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		gameWrapper->GetPlayerController().ToggleBoost(1);
	}
	else {
		// Not first shot in drill, write to file if drill is complete, then move to the next shot
		if (custom_training_players.size() == offense + defense) {
			writeDrillToFile();
		}
		gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm", std::bind(&TeamTrainingPlugin::onNextRound, this, std::placeholders::_1));
		cvarManager->executeCommand("workshop_playlist_next;sv_training_next");
	}
}

void TeamTrainingPlugin::onNextRound(std::string eventName)
{
	getNextShot();
}

void TeamTrainingPlugin::writeDrillToFile()
{

	// Sure I should have used the json library, but this got the job done... Don't judge me :(
	custom_training_export_file
		<< "\t\t{\n\t\t\t\"ball\": {\n\t\t\t\t\"location\": " << vectorString(custom_training_ball.location) << ",\n\t\t\t\t\"velocity\": " << vectorString(custom_training_ball.velocity) << ",\n"
		<< "\t\t\t\t\"torque\": { \"x\": 0, \"y\": 0, \"z\": 0 },\n\t\t\t\t\"rotation\": " << rotationString(custom_training_ball.rotation) << "\n\t\t\t},\n\t\t\t\"players\": [\n";

	int i = 0;
	for (auto player : custom_training_players) {
		std::string role = (i < offense - 1) ? "passer" : ((i == offense - 1) ? "shooter" : "defender");

		if (i != 0) {
			custom_training_export_file << ",\n";
		}
		custom_training_export_file
			<< "\t\t\t\t{\n\t\t\t\t\t\"role\": \"" << role << "\",\n\t\t\t\t\t\"boost\": " << std::to_string(player.boost) << ",\n\t\t\t\t\t\"location\": " << vectorString(player.location)
			<< ",\n\t\t\t\t\t\"velocity\": " << vectorString(player.velocity) << ",\n\t\t\t\t\t\"rotation\": " << rotationString(player.rotation) << "\n\t\t\t\t}";
		i++;
	}

	custom_training_export_file
		<< "\n\t\t\t]\n\t\t}";

	custom_training_players.clear();
	if (++drills_written < num_drills) {
		custom_training_export_file << ",\n";
		gameWrapper->SetTimeout([&, &_cvarManager = cvarManager](GameWrapper *gw) {
			cvarManager->executeCommand("workshop_playlist_next;sv_training_next");
		}, 1.0f);
	}
	else {
		custom_training_export_file << "\n\t]\n}";
		custom_training_export_file.close();
		cvarManager->log("Finished writing drills");
	}
}

void TeamTrainingPlugin::onBallTick(ServerWrapper server, void * params, std::string eventName)
{
	BallWrapper ball = server.GetBall();
	Vector v = ball.GetVelocity();
	if (v.X != 0 || v.Y != 0 || v.Z != 0) {
		gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.Tick");
		gameWrapper->GetPlayerController().ToggleBoost(0);
		custom_training_ball.velocity = ball.GetVelocity().clone();
		if (defense == 0) {
			writeDrillToFile();
		}
		else {
			cvarManager->executeCommand("workshop_playlist_next;sv_training_next");
		}
	}
}

/* Random commands */

void TeamTrainingPlugin::addNewBall(std::vector<std::string> params)
{
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	int balls = sw.GetTotalGameBalls();
	sw.SetTotalGameBalls2(balls + 1);
	sw.ResetBalls();
}

/* Experimental code below */

void TeamTrainingPlugin::twoBallTraining(std::vector<std::string> params)
{
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	sw.SetTotalGameBalls2(2);
	sw.ResetBalls();
	ArrayWrapper<BallWrapper> balls = sw.GetGameBalls();
	if (balls.Count() != 2) {
		cvarManager->log("Something went wrong. Total number of balls is " + std::to_string(balls.Count()) + ", expecting 2 balls.");
		return;
	}

	CarWrapper car = sw.GetCars().Get(0);
	car.SetLocation(Vector(3704.04, 3386.56, 17.01));
	car.SetRotation(Rotator(-100, 26220, 0));
	car.Stop();

	// TODO: Find better car and ball position
	gameWrapper->SetTimeout([&, &_cvarManager = cvarManager](GameWrapper *gw) {
		ServerWrapper sw = gameWrapper->GetGameEventAsServer();
		ArrayWrapper<BallWrapper> balls = sw.GetGameBalls();

		BallWrapper ball = balls.Get(0);
		BallWrapper ball2 = balls.Get(1);
		ball.SetLocation(Vector(1147.23, 4653.7, 100.48));
		ball.SetVelocity(Vector(-554.356, -2820.86, 1287.35));

		ball2.SetLocation(Vector(-644.42, 5077.22, 100.48));
		ball2.SetVelocity(Vector(3063.11, -860.333, -341.428));
	}, 2.0f);
}

std::map<std::string, TrainingPack> TeamTrainingPlugin::getTrainingPacks() {
	std::map<std::string, TrainingPack> packs;

	for (const auto & entry : fs::directory_iterator(".\\bakkesmod\\data\\teamtraining\\")) {
		if (entry.path().has_extension() && entry.path().extension() == ".json") {
			//packs[entry.path().filename().string()] = TrainingPack(entry.path().string(), cvarManager);
			packs.emplace(entry.path().filename().string(), TrainingPack(entry.path().string(), cvarManager));
		}
	}

	return packs;
}
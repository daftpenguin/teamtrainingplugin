#define _USE_MATH_DEFINES

#include "TeamTrainingPlugin.h"

#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/GameEvent/ReplayDirectorWrapper.h"
#include "bakkesmod/wrappers/WrapperStructs.h"
#include "utils/parser.h"

#include <algorithm>
#include <random>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <vector>

namespace fs = std::filesystem;
using namespace std;

#pragma comment (lib, "Ws2_32.lib")

BAKKESMOD_PLUGIN(TeamTrainingPlugin, "Team Training plugin", PLUGIN_VERSION, PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING)

mt19937 gen((unsigned int) chrono::system_clock::now().time_since_epoch().count());
uniform_real_distribution<float> dist(0.0, 1.0);

std::random_device rd;
std::mt19937 urbg(rd());

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

std::string vectorToString(std::vector<int> v) {
	std::stringstream ss;
	for (unsigned int i = 0; i < v.size(); i++) {
		if (i != 0)
			ss << ",";
		ss << v[i];
	}
	return ss.str();
}

void TeamTrainingPlugin::onLoad()
{
	// Usage
	cvarManager->registerNotifier("team_train_load", 
		std::bind(&TeamTrainingPlugin::onLoadTrainingPack, this, std::placeholders::_1), 
		"Launches given team training pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_reset", 
		std::bind(&TeamTrainingPlugin::onResetShot, this, std::placeholders::_1),
		"Resets the current shot", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_next", 
		std::bind(&TeamTrainingPlugin::onNextShot, this, std::placeholders::_1),
		"Loads the next shot in the pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_prev", 
		std::bind(&TeamTrainingPlugin::onPrevShot, this, std::placeholders::_1),
		"Loads the previous shot in the pack", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_list", 
		std::bind(&TeamTrainingPlugin::listPacks, this, std::placeholders::_1),
		"Lists available packs", PERMISSION_ALL);

	// Role assignment
	cvarManager->registerNotifier("team_train_randomize_players",
		std::bind(&TeamTrainingPlugin::randomizePlayers, this, std::placeholders::_1),
		"Randomizes the assignments of players to roles", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);
	cvarManager->registerNotifier("team_train_cycle_players",
		std::bind(&TeamTrainingPlugin::cyclePlayers, this, std::placeholders::_1),
		"Cycles the assignments of players to roles", PERMISSION_FREEPLAY | PERMISSION_PAUSEMENU_CLOSED);

	// Creation
	cvarManager->registerNotifier("team_train_internal_convert", 
		std::bind(&TeamTrainingPlugin::internalConvert, this, std::placeholders::_1),
		"Converts custom training pack into team training pack (intended for internal use through GUI)",
		PERMISSION_CUSTOM_TRAINING | PERMISSION_PAUSEMENU_CLOSED);
	
	// Variables
	cvarManager->registerCvar(CVAR_PREFIX + "countdown", "1", "Time to wait until shot begins", true, true, 0, true, 10, true);
	cvarManager->registerCvar(CVAR_PREFIX + "last_version_loaded", "", "The last version of the plugin that was loaded, used for displaying changelog first when plugin is updated.",
		false, false, 0, 0, 0, true);

	gameWrapper->LoadToastTexture("teamtraining1", gameWrapper->GetDataFolder() / "assets" / "teamtraining_logo.png");

	/*gameWrapper->HookEventWithCallerPost<PlayerControllerWrapper>(
		"Function TAGame.GameEvent_TA.AddCar",
		std::bind(&TeamTrainingPlugin::onPlayerLeave, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gameWrapper->HookEventWithCallerPost<PlayerControllerWrapper>(
		"Function TAGame.GameEvent_TA.RemoveCar",
		std::bind(&TeamTrainingPlugin::onPlayerJoin, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));*/

	//cvarManager->registerNotifier("team_train_test", std::bind(&TeamTrainingPlugin::test, this, std::placeholders::_1), "test", PERMISSION_ALL);
}

/*void TeamTrainingPlugin::test(std::vector<std::string> params)
{	
}*/

//void TeamTrainingPlugin::onPlayerLeave(PlayerControllerWrapper pc, void* params, string eventName)
//{
//	if (!gameWrapper->IsInFreeplay() || pc.IsNull()) {
//		return;
//	}
//
//	cvarManager->log("Player left: " + pc.GetPRI().GetPlayerName().ToString());
//}
//
//void TeamTrainingPlugin::onPlayerJoin(PlayerControllerWrapper pc, void* params, string eventName)
//{
//	if (!gameWrapper->IsInFreeplay() || pc.IsNull()) {
//		return;
//	}
//
//	cvarManager->log("Player joined: " + pc.GetPRI().GetPlayerName().ToString());
//}

void TeamTrainingPlugin::onUnload()
{
	if (!pack) {
		cvarManager->loadCfg(RESET_CFG_FILE);
		pack = NULL;
	}
}

void TeamTrainingPlugin::onLoadTrainingPack(std::vector<std::string> params)
{
	cvarManager->log("Team training now has a UI. F2 -> Plugins -> Team Training Plugin and click the button :)");

	if (!gameWrapper->IsInFreeplay()) {
		return;
	}
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (server.IsNull()) {
		cvarManager->log("Server is null, cannot load pack");
		return;
	}

	if (params.size() < 2) {
		cvarManager->log("Must provide the name of an existing training pack");
		return;
	}

	bool firstPack = pack == NULL;
	this->pack = std::make_shared<TrainingPack>(getPackDataPath(params[1]));
	if (this->pack->errorMsg != "") {
		this->pack = NULL;
		cvarManager->log("Error opening training pack: " + this->pack->errorMsg);
		return;
	}

	if (firstPack) {
		cvarManager->loadCfg(CFG_FILE);
		gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.PostGoalScored.BeginState", 
			std::bind(&TeamTrainingPlugin::onGoalScored, this, std::placeholders::_1));
		gameWrapper->HookEventPost("Function TAGame.GameEvent_TA.StartEvent", 
			std::bind(&TeamTrainingPlugin::onResetShotEvent, this, std::placeholders::_1));
		gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", 
			std::bind(&TeamTrainingPlugin::onFreeplayDestroyed, this, std::placeholders::_1));
	}

	cvarManager->log("Loaded training pack: " + this->pack->filepath.string());
	if (cvarManager->getCvar("sv_training_autoshuffle").getBoolValue()) {
		std::shuffle(pack->drills.begin(), pack->drills.end(), urbg);
	}

	auto cars = server.GetCars();
	if (cars.IsNull()) {
		cvarManager->log("Failed to get cars from server");
		return;
	}

	if (!validatePlayers(cars)) {
		return;
	}

	int carCount = server.GetCars().Count();
	if (player_order.size() < carCount) {
		player_order.clear();
		for (int i = 0; i < carCount; i++) {
			player_order.push_back(i);
		}
	}

	setShot(0);
}

void TeamTrainingPlugin::onFreeplayDestroyed(std::string eventName)
{
	cvarManager->loadCfg(RESET_CFG_FILE);
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

	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (server.IsNull()) {
		cvarManager->log("Server is null, cannot continue to set shot");
		return;
	}

	// Make sure we can get ball and cars first
	cvarManager->log("Checking cars and ball exist");
	BallWrapper ball = server.GetBall();
	if (ball.IsNull()) {
		cvarManager->log("Ball is null, cannot setup shot");
		return;
	}
	ArrayWrapper<CarWrapper> cars = server.GetCars();
	if (cars.IsNull()) {
		cvarManager->log("Cars array is null, cannot continue to set shot");
		return;
	}

	cvarManager->log("Validating players");
	if (!validatePlayers(cars)) {
		return;
	}

	cvarManager->log("Checking shot number");
	if (shot >= pack->drills.size()) {
		cvarManager->log("setShot called with shot that doesn't exist. Aborting...");
		return;
	}

	// Set some data for the timer, before modifying anything
	unsigned int shot_set = last_shot_set + 1;
	last_shot_set = shot_set;
	goal_was_scored = false;
	current_shot = shot;

	cvarManager->log("Retrieving shot and generating variant");
	TrainingPackDrill drill = pack->drills[shot];
	if (cvarManager->getCvar("sv_training_enabled").getBoolValue()) {
		drill = createDrillVariant(drill);
	}

	// Stop all cars and ball
	cvarManager->log("Stopping cars and ball");
	for (int i = 0; i < cars.Count(); i++) {
		CarWrapper car = cars.Get(i);
		if (car.IsNull()) {
			cvarManager->log("Car " + to_string(i) + " is null. Can't stop it.");
		}
		else {
			car.Stop();
		}
	}
	ball.Stop();

	cvarManager->log("Setting passers to cars");
	int i = 0;
	for (auto& player : drill.passers) {
		if (player_order[i] >= cars.Count()) {
			cvarManager->log("Not enough cars for passer positions");
			return;
		}
		cvarManager->log(to_string(i) + " index, " + to_string(player_order[i]) + " assigned car");
		CarWrapper car = cars.Get(player_order[i++]);
		if (car.IsNull()) {
			cvarManager->log("Car " + to_string(i-1) + " became null?");
			return;
		}
		setPlayerToCar(player, car);
	}

	cvarManager->log("Checking if can find shooter");
	if (i >= cars.Count() || player_order[i] >= cars.Count() || cars.Get(player_order[i]).IsNull()) {
		cvarManager->log("Cannot find shooter");
		return;
	}

	cvarManager->log("Assigning shooter");
	setPlayerToCar(drill.shooter, cars.Get(player_order[i++]));

	cvarManager->log("Assigning defenders to cars");
	if (cars.Count() > this->pack->offense) {
		for (auto &player : drill.defenders) {
			if (player_order[i] >= cars.Count()) {
				break;
			}
			cvarManager->log(to_string(i) + " index, " + to_string(player_order[i]) + " assigned car");
			CarWrapper car = cars.Get(player_order[i++]);
			if (car.IsNull()) {
				cvarManager->log("Failed to get defender with index " + to_string(i - 1));
			}
			setPlayerToCar(player, car);
		}
	}

	cvarManager->log("Setting ball location");
	ball.SetLocation(drill.ball.location);
	
	cvarManager->log("Attaching timer");
	float countdown = cvarManager->getCvar(CVAR_PREFIX + "countdown").getFloatValue();
	cvarManager->log("Countdown: " + to_string(countdown));
	auto pack_load_time = this->pack->load_time;
	gameWrapper->SetTimeout([&, &_cvarManager = cvarManager, shot_set, pack_load_time](GameWrapper *gw) {
		if (!gameWrapper->IsInFreeplay()) {
			return;
		}

		// Don't do anything if a new shot was set or pack was replaced before timeout is called
		if (!pack || last_shot_set != shot_set || pack_load_time != pack->load_time) {
			return;
		}

		ServerWrapper sw = gw->GetGameEventAsServer();
		if (sw.IsNull()) {
			return;
		}

		if (current_shot >= pack->drills.size()) {
			cvarManager->log("current_shot is more than number of drills? This shouldn't have happened...");
			return;
		}

		TrainingPackDrill drill = pack->drills[current_shot];
		BallWrapper ball = sw.GetBall();
		if (ball.IsNull()) {
			cvarManager->log("ball is null in timeout. Aborting...");
			return;
		}
		ball.SetRotation(drill.ball.rotation);
		ball.SetVelocity(drill.ball.velocity);
		ball.SetAngularVelocity(drill.ball.angular, false);

	}, max(0.0f, countdown));
}

TrainingPackDrill TeamTrainingPlugin::createDrillVariant(TrainingPackDrill drill)
{
	cvarManager->log("Passers: " + to_string(drill.passers.size()));

	TrainingPackDrill new_drill;
	new_drill.ball = addBallVariance(drill.ball);
	new_drill.shooter = addCarVariance(drill.shooter);

	for (int p = 0; p < drill.passers.size(); p++) {
		new_drill.passers.push_back(addCarVariance(drill.passers[p]));
	}

	for (int p = 0; p < drill.defenders.size(); p++) {
		new_drill.defenders.push_back(addCarVariance(drill.defenders[p]));
	}

	return new_drill;
}

TrainingPackPlayer TeamTrainingPlugin::addCarVariance(TrainingPackPlayer player)
{
	return player;
}

TrainingPackBall TeamTrainingPlugin::addBallVariance(TrainingPackBall ball)
{
	/*
sv_training_allowmirror "1" //Mirrors custom training shots randomly
sv_training_autoshuffle "1" //Automatically shuffles playlists
sv_training_enabled "1" //Enable custom training variance
sv_training_limitboost "-1" //Limits the boost you can use in custom training (-1 for unlimited)
sv_training_var_car_loc "0" //Randomly changes the car location by this amount (in unreal units)
sv_training_var_car_rot "0" //Randomly changes the car rotation by this amount (in %)
	*/

	// Location variance
	float theta = (float) (dist(gen) * 2 * M_PI);
	Vector loc_var = Vector{ cos(theta), sin(theta), 0 } * cvarManager->getCvar("sv_training_var_loc").getFloatValue();
	loc_var.Z = cvarManager->getCvar("sv_training_var_loc_z").getFloatValue();
	ball.location = ball.location + loc_var;

	// Rotation variance (a lot of packs seem to have no rotation)
	float rot_var = (float) 1.0 + cvarManager->getCvar("sv_training_var_rot").getFloatValue() / 100.0f;
	ball.rotation = ball.rotation * (int) rot_var;

	// Speed variance
	float magnitude = ball.velocity.magnitude();
	ball.velocity.normalize();
	ball.velocity = ball.velocity * (magnitude * (1 + cvarManager->getCvar("sv_training_var_speed").getFloatValue() / 100));

	// Random spin
	// This is probably incorrect, but choose a random point on the surface of a unit sphere with uniform distribution.
	// Then multiply with randomly chosen magnitude.
	float z = (float) (-1.0 + 2.0 * dist(gen));
	float longitude = (float) (2.0 * M_PI * dist(gen));
	float rh = sin(acos(z));
	ball.angular = ball.angular + (Vector{
		rh * cos(longitude),
		rh * sin(longitude),
		z } * (cvarManager->getCvar("sv_training_var_spin").getFloatValue()));
	cvarManager->log("Spin added: " + vectorString(ball.angular));

	return ball;
}

void TeamTrainingPlugin::randomizePlayers(std::vector<std::string> params)
{
	std::shuffle(std::begin(player_order), std::end(player_order), urbg);
	cvarManager->log("Player order: " + vectorToString(player_order));
	resetShot();
}

void TeamTrainingPlugin::cyclePlayers(std::vector<std::string> params)
{
	unsigned int first = player_order.front();
	for (size_t i = 0; i < player_order.size() - 1; i++) {
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
			cvarManager->log(pack.filepath.string());
		}
	}
	else {
		if (packDataPath.length() == 0) {
			packDataPath = getPackDataPath("").string();
		}
		cvarManager->log("No team training packs in " + packDataPath);
	}
}

void TeamTrainingPlugin::onGoalScored(std::string eventName)
{
	goal_was_scored = true;
}

void TeamTrainingPlugin::onResetShotEvent(std::string eventName)
{
	if (!gameWrapper->GetGameEventAsServer().IsNull()) {
		if (goal_was_scored) {
			onNextShot({});
		} else {
			resetShot();
		}
	}
}

void TeamTrainingPlugin::onResetShot(std::vector<std::string> params)
{
	resetShot();
}

void TeamTrainingPlugin::resetShot()
{
	if (!gameWrapper->IsInFreeplay() || !pack) {
		return;
	}

	setShot(current_shot);
}

void TeamTrainingPlugin::onNextShot(std::vector<std::string> params)
{
	if (!gameWrapper->IsInFreeplay() || !pack) {
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
	if (!gameWrapper->IsInFreeplay() || !pack) {
		return;
	}

	int shot = current_shot - 1;
	if (shot < 0) {
		shot = (int) pack->drills.size() - 1;
	}

	setShot(shot);
}

void TeamTrainingPlugin::setPlayerToCar(TrainingPackPlayer player, CarWrapper car)
{
	BoostWrapper boost = car.GetBoostComponent();
	if (!boost.IsNull()) {
		boost.SetBoostAmount(player.boost);
	}
	car.SetLocation(player.location);
	car.SetRotation(player.rotation);
}

bool TeamTrainingPlugin::validatePlayers(ArrayWrapper<CarWrapper> cars)
{
	// Validate the number of players
	int num_players = cars.Count();
	if (num_players < pack->offense) {
		cvarManager->log("Pack requires at least " + std::to_string(pack->offense) + " players but there are only " + \
			std::to_string(num_players));
		return false;
	}

	return true;
}


/* For extracting custom training pack to team training pack */

void TeamTrainingPlugin::internalConvert(std::vector<std::string> params)
{
	if (!gameWrapper->IsInCustomTraining()) {
		cvarManager->log("Not in custom training");
		return;
	}

	int offense = std::atoi(params[1].c_str());
	int defense = std::atoi(params[2].c_str());
	int num_drills = std::atoi(params[3].c_str());
	std::string filename = params[4];
	std::string creator = params[5];
	std::string description = params[6];
	std::string code = params[7];

	// TODO: Fill these in
	std::string creatorID = "";
	std::string uploader = "";
	std::string uploaderID = "";
	std::vector<std::string> tags;
	
	conversion_pack = TrainingPack(getPackDataPath(filename), offense, defense, description, creator, code, creatorID, tags, num_drills);
	conversion_pack.startNewDrill();
	getNextShotCalled = 0;
	getNextShot();
}

void TeamTrainingPlugin::getNextShot()
{
	if (conversion_pack.data_saved) {
		return;
	}

	if (getNextShotCalled++ > (conversion_pack.offense + conversion_pack.defense) * conversion_pack.expected_drills) {
		stopConversion("Detected bug in training pack creation, too many rounds have been checked. Aborting training pack creation. Please report this to DaftPenguin.");
		return;
	}

	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	ArrayWrapper<CarWrapper> cars = server.GetCars();
	CarWrapper car = cars.Get(0);

	cvarManager->log("Player added");
	conversion_pack.addPlayer(car);

	if (conversion_pack.lastPlayerAddedWasFirstPasser()) {
		// Drill is first passer, get ball and setup to get velocity
		cvarManager->log("Setting ball location, getting movement");
		BallWrapper ball = server.GetBall();
		if (!ball.IsNull()) {
			conversion_pack.addBallLocation(ball);
		}
		ball_tick_failures = 0;
		ball_velocity_zero = 0;
		gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Active.Tick", 
			std::bind(&TeamTrainingPlugin::onBallTick, this, std::placeholders::_1));
		toggleBoost(1);
	} else {
		// Hook for next round if drill isn't complete
		if (!conversion_pack.allPlayersInDrillAdded()) {
			gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm",
				std::bind(&TeamTrainingPlugin::onNextRound, this, std::placeholders::_1));
			cvarManager->executeCommand("sv_training_next");
		}
		else {
			if (conversion_pack.expectingMoreDrills()) {
				cvarManager->log("next shot creating new drill");
				conversion_pack.startNewDrill();
				gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm",
					std::bind(&TeamTrainingPlugin::onNextRound, this, std::placeholders::_1));
				gameWrapper->SetTimeout([&, &_cvarManager = cvarManager](GameWrapper* gw) {
					cvarManager->executeCommand("sv_training_next");
					}, 1.0f);
			}
			else {
				savePack();
			}
		}
	}
}

void TeamTrainingPlugin::savePack()
{
	conversion_pack.save();
	stopConversion("Team training pack conversion successfully completed.");
}

void TeamTrainingPlugin::stopConversion(string message)
{
	gameWrapper->UnhookEvent("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm");
	gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.Tick");
	cvarManager->log(message);
	gameWrapper->Toast("Team Training", message, "teamtraining1");
	toggleBoost(0);
}

void TeamTrainingPlugin::toggleBoost(int state)
{
	auto pc = gameWrapper->GetPlayerController();
	if (!pc.IsNull()) {
		pc.ToggleBoost(1);
	} else {
		gameWrapper->Toast("Team Training", "Failed to automatically apply boost to retrieve ball trajectory, please manually press boost or accelerate to continue.", "teamtraining1");
		cvarManager->log("Failed to automatically apply boost to retrieve ball trajectory, please manually press boost or accelerate to continue.");
	}
}

void TeamTrainingPlugin::onNextRound(std::string eventName)
{
	gameWrapper->UnhookEvent("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm");

	if (conversion_pack.data_saved) {
		return;
	}

	getNextShot();
}

void TeamTrainingPlugin::onBallTick(std::string eventName)
{
	if (conversion_pack.data_saved) {
		return;
	}

	if (conversion_pack.ball_velocity_set) {
		return;
	}

	auto server = gameWrapper->GetGameEventAsServer();
	if (server.IsNull()) {
		cvarManager->log("Server is null, skipping ball tick");
		if (++ball_tick_failures > MAX_BALL_TICK_FAILURES) {
			stopConversion("Maximum ball tick failures reach, aborting training pack creation. Please report this issue to DaftPenguin.");
			return;
		}
	}

	BallWrapper ball = server.GetBall();
	if (ball.IsNull()) {
		cvarManager->log("Ball is null, skipping ball tick");
		if (++ball_tick_failures > MAX_BALL_TICK_FAILURES) {
			stopConversion("Maximum ball tick failures reach, aborting training pack creation. Please report this issue to DaftPenguin.");
			return;
		}
	}

	Vector v = ball.GetVelocity();
	if (v.X != 0 || v.Y != 0 || v.Z != 0 || ++ball_velocity_zero > MAX_BALL_VELOCITY_ZERO) { // Ball might not have a velocity, so fail at some point and assume it's 0
		cvarManager->log("Setting ball movement");
		conversion_pack.setBallMovement(ball);
		gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.Tick");
		toggleBoost(0);

		if (conversion_pack.allPlayersInDrillAdded()) {
			if (conversion_pack.expectingMoreDrills()) {
				cvarManager->log("Ball tick creating new drill");
				conversion_pack.startNewDrill();
			}
			else {
				savePack();
				return; // Avoid hooking for next round
			}
		}
		gameWrapper->HookEventPost("Function TAGame.GameEvent_TrainingEditor_TA.OnResetRoundConfirm",
			std::bind(&TeamTrainingPlugin::onNextRound, this, std::placeholders::_1));
		cvarManager->executeCommand("sv_training_next");
	}
}

static inline bool dir_exists(const char *dirpath)
{
	DWORD ftyp = GetFileAttributes(dirpath);
	return (ftyp != INVALID_FILE_ATTRIBUTES && ftyp & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<TrainingPack> TeamTrainingPlugin::getTrainingPacks() {
	std::vector<TrainingPack> packs;

	fs::path dataPath = getPackDataPath("");
	if (!fs::exists(dataPath)) {
		return packs;
	}

	for (const auto & entry : fs::directory_iterator(dataPath)) {
		if (entry.path().has_extension() && entry.path().extension() == ".json") {
			packs.push_back(TrainingPack(entry.path().string()));
		}
	}

	return packs;
}

fs::path TeamTrainingPlugin::getPackDataPath(std::string packName)
{
	if (packName.length() > 0) {
		fs::path path = packName;
		if (!path.has_extension()) {
			path += ".json";
		}

		if (fs::exists(path)) {
			return path.make_preferred();
		}

		return (gameWrapper->GetDataFolder() / "teamtraining" / path).make_preferred();
	}

	return (gameWrapper->GetDataFolder() / "teamtraining").make_preferred();
}
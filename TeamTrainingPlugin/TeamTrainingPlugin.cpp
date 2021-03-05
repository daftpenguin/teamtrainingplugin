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
#include <string>
#include <openssl/crypto.h>
#include <boost/algorithm/string.hpp>

using namespace std;
namespace fs = std::filesystem;

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
	using namespace std::placeholders;
	//Netcode = std::make_shared<NetcodeManager>(cvarManager, gameWrapper, exports, std::bind(&TeamTrainingPlugin::OnMessageReceived, this, _1, _2));

	// Remove TeamTrainingPlugin.dll if it exists (we had to blacklist TeamTraining.dll because prebuilt openssl dll wasn't unloading)
	auto oldDllPath = gameWrapper->GetBakkesModPath() / "plugins" / "TeamTrainingPlugin.dll";
	if (fs::exists(oldDllPath)) {
		if (fs::remove(oldDllPath)) {
			cvarManager->log("Old dll successfully removed");
		}
		else {
			cvarManager->log("Old dll exists but deletion failed");
		}
	}

	// Usage
	cvarManager->registerNotifier("team_train_load", 
		std::bind(&TeamTrainingPlugin::onLoadTrainingPack, this, _1), 
		"Launches given team training pack", PERMISSION_ALL);
	cvarManager->registerNotifier("team_train_reset", 
		std::bind(&TeamTrainingPlugin::onResetShot, this, _1),
		"Resets the current shot", PERMISSION_ALL);
	cvarManager->registerNotifier("team_train_next", 
		std::bind(&TeamTrainingPlugin::onNextShot, this, _1),
		"Loads the next shot in the pack", PERMISSION_ALL);
	cvarManager->registerNotifier("team_train_prev", 
		std::bind(&TeamTrainingPlugin::onPrevShot, this, _1),
		"Loads the previous shot in the pack", PERMISSION_ALL);
	cvarManager->registerNotifier("team_train_list", 
		std::bind(&TeamTrainingPlugin::listPacks, this, _1),
		"Lists available packs", PERMISSION_ALL);
	/*cvarManager->registerNotifier("team_train_unstuck", [this](std::vector<std::string> params) {
		if (gameWrapper->IsInFreeplay()) {
			cvarManager->log("Unfreezing not supported in freeplay");
			return;
		}
		Netcode->SendMessageW("unstuck");
		}, "Attempts to unfreeze all cars and sends messages to all other clients to unfreeze", PERMISSION_ALL);
	*/

	// Role assignment
	cvarManager->registerNotifier("team_train_randomize_players",
		std::bind(&TeamTrainingPlugin::randomizePlayers, this, _1),
		"Randomizes the assignments of players to roles", PERMISSION_ALL);
	cvarManager->registerNotifier("team_train_cycle_players",
		std::bind(&TeamTrainingPlugin::cyclePlayers, this, _1),
		"Cycles the assignments of players to roles", PERMISSION_ALL);
	
	// Variables
	cvarManager->registerCvar(CVAR_PREFIX + "countdown", "1", "Time to wait until shot begins", true, true, 0, true, 10, true);
	//cvarManager->registerCvar(CVAR_PREFIX + "netcode_enabled", "0", "Enables netcode to communicate countdown on drill resets", true, false, 0, false, 0, true);
	cvarManager->registerCvar(CVAR_PREFIX + "last_version_loaded", "", "The last version of the plugin that was loaded, used for displaying changelog first when plugin is updated.",
		false, false, 0, 0, 0, true);

	gameWrapper->LoadToastTexture("teamtraining1", gameWrapper->GetDataFolder() / "assets" / "teamtraining_logo.png");

	cvarManager->registerNotifier("team_train_rand_pack",
		std::bind(&TeamTrainingPlugin::loadRandomPack, this, _1), "Loads a random training pack", PERMISSION_ALL);

	auto pname = gameWrapper->GetPlayerName();
	if (!pname.IsNull()) uploader = pname.ToString();
	uploaderID = UIDToString(gameWrapper->GetUniqueID());

	gameWrapper->HookEventPost(CUSTOM_TRAINING_LOADED_EVENT, bind(&TeamTrainingPlugin::onCustomTrainingLoaded, this, _1));

	/*gameWrapper->HookEventWithCallerPost<PlayerControllerWrapper>(
		"Function TAGame.GameEvent_TA.AddCar",
		std::bind(&TeamTrainingPlugin::onPlayerLeave, this, _1, _2, _3));
	gameWrapper->HookEventWithCallerPost<PlayerControllerWrapper>(
		"Function TAGame.GameEvent_TA.RemoveCar",
		std::bind(&TeamTrainingPlugin::onPlayerJoin, this, _1, _2, _3));*/

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

/*void TeamTrainingPlugin::OnMessageReceived(const string& Message, PriWrapper Sender)
{
	if (!isValidServer()) {
		cvarManager->log("Received messsage while not in valid session");
		return;
	}

	if (gameWrapper->IsInFreeplay()) {
		cvarManager->log("Message received in freeplay. Netcode not supported in freeplay.");
		return;
	}

	vector<string> fields;
	boost::split(fields, Message, boost::is_any_of("|"));

	chrono::milliseconds reset_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	last_reset = reset_time;

	if (fields[0].compare("unstuck") == 0)
	{
		auto server = gameWrapper->GetGameEventAsServer();
		if (server.IsNull()) {
			cvarManager->log("Server null during unstuck, will attempt to unstuck local car");
			auto car = gameWrapper->GetLocalCar();
			if (car.IsNull()) {
				cvarManager->log("Local car is null too during unstuck. This isn't good...");
				return;
			}
			car.SetFrozen(0);
		}
		else {
			auto cars = server.GetCars();
			for (auto car : cars) {
				if (car.IsNull()) continue;
				car.SetFrozen(0);
			}
		}
	}
	else if (fields[0].compare("reset") == 0)
	{
		if (fields.size() <= 1) cvarManager->log("Not enough args for reset: " + to_string(fields.size()));
		float timer = (float) ::atof(fields[1].c_str());

		auto server = gameWrapper->GetGameEventAsServer();
		if (server.IsNull()) {
			cvarManager->log("Server is null.");

			auto server = gameWrapper->GetGameEventAsReplay();
			if (server.IsNull()) {
				cvarManager->log("Replay is null too");
			}

			auto car = gameWrapper->GetLocalCar();
			if (car.IsNull()) {
				cvarManager->log("Local car is null too.");
				return;
			}

			car.SetFrozen(1);
			gameWrapper->SetTimeout([this, reset_time](GameWrapper* gw) {
				if (reset_time != last_reset) return;

				auto car = gameWrapper->GetLocalCar();
				if (car.IsNull()) {
					cvarManager->log("Local car is null during unfreeze timer.");
					return;
				}
				car.SetFrozen(0);
				}, timer);
		}
		else {
			auto cars = server.GetCars();
			for (auto car : cars) {
				if (car.IsNull()) continue;
				car.SetFrozen(1);
			}

			gameWrapper->SetTimeout([this, reset_time](GameWrapper* gw) {
				if (reset_time != last_reset) return;

				auto server = gameWrapper->GetGameEventAsServer();
				if (server.IsNull()) {
					cvarManager->log("Server is null but previously wasn't...");
					auto car = gameWrapper->GetLocalCar();
					if (car.IsNull()) {
						cvarManager->log("Car is null, too. Perhaps host left match?");
						return;
					}
					car.SetFrozen(0);
				}

				auto cars = server.GetCars();
				for (auto car : cars) {
					if (car.IsNull()) continue;
					car.SetFrozen(0);
				}
				}, timer);
		}
	}
}*/

void TeamTrainingPlugin::onLoadTrainingPack(std::vector<std::string> params)
{
	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!isValidServer(server)) {
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
	if (!pack) {
		cvarManager->log("No pack loaded");
		return;
	}

	ServerWrapper server = gameWrapper->GetGameEventAsServer();
	if (!isValidServer(server)) {
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
	cvarManager->log("Drill ball before variance: location: " + vectorString(drill.ball.location) + ", spin: " + vectorString(drill.ball.angular));
	if (cvarManager->getCvar("sv_training_enabled").getBoolValue()) {
		drill = createDrillVariant(drill);
	}
	cvarManager->log("Drill ball after variance: location: " + vectorString(drill.ball.location) + ", spin: " + vectorString(drill.ball.angular));

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
		car.SetFrozen(0); // Might currently be frozen
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

	/*if (cvarManager->getCvar(CVAR_PREFIX + "netcode_enabled").getBoolValue()) {
		// Message received on server seems to happen in same tick, and car location is never set if frozen then, so wait a tick
		if (gameWrapper->IsInFreeplay()) {
			cvarManager->log("Freezing on shot reset not supported in freeplay");
		}
		else {
			gameWrapper->SetTimeout([this, countdown](GameWrapper* gw) {
				Netcode->SendMessage("reset|" + to_string(countdown));
				}, 0.009f);
		}
	}*/

	auto pack_load_time = this->pack->load_time;
	gameWrapper->SetTimeout([&, &_cvarManager = cvarManager, drill, shot_set, pack_load_time](GameWrapper *gw) {
		// Don't do anything if a new shot was set or pack was replaced before timeout is called
		if (!pack || last_shot_set != shot_set || pack_load_time != pack->load_time) {
			return;
		}

		ServerWrapper sw = gw->GetGameEventAsServer();
		if (!isValidServer(sw)) {
			return;
		}

		if (current_shot >= pack->drills.size()) {
			cvarManager->log("current_shot is more than number of drills? This shouldn't have happened...");
			return;
		}

		//TrainingPackDrill drill = pack->drills[current_shot];
		BallWrapper ball = sw.GetBall();
		if (ball.IsNull()) {
			cvarManager->log("ball is null in timeout. Aborting...");
			return;
		}
		ball.SetRotation(drill.ball.rotation);
		ball.SetVelocity(drill.ball.velocity);
		cvarManager->log("Applying angular velocity: " + vectorString(drill.ball.angular));
		ball.SetAngularVelocity(drill.ball.angular, false);

		cvarManager->log("Drill ball when setting velocity: location: " + vectorString(drill.ball.location) + ", spin: " + vectorString(drill.ball.angular));

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

	if (cvarManager->getCvar("sv_training_allowmirror").getBoolValue() && dist(gen) * 1 > 0.5) {
		new_drill.mirror();
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
	float randSpin = cvarManager->getCvar("sv_training_var_spin").getFloatValue();
	ball.angular = ball.angular + (Vector{
		rh * cos(longitude),
		rh * sin(longitude),
		z } * randSpin);
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

void TeamTrainingPlugin::loadRandomPack(std::vector<std::string> params)
{
	if (packs.size() == 0) {
		packs = getTrainingPacks();
	}

	unordered_set<string> tags(params.begin() + 1, params.end());
	vector<TrainingPack> filteredPacks;
	for (TrainingPack& p : packs) {
		if (p.offense + p.defense != 1 || p.code.empty()) continue; // Only allow single player packs for now

		if (tags.size() == 0) {
			filteredPacks.push_back(p);
		}
		else {
			for (const string& tag : p.tags) {
				if (tags.find(tag) != tags.end()) {
					filteredPacks.push_back(p);
					break;
				}
			}
		}
	}

	if (filteredPacks.size() == 0) {
		cvarManager->log("Failed to load random training pack. There appear to be no training packs with at least one tag matching: " + boost::join(tags, ", "));
		return;
	}

	int r = rand() % filteredPacks.size();
	cvarManager->executeCommand("sleep 1; load_training " + filteredPacks[r].code);
}

bool TeamTrainingPlugin::isValidServer()
{
	if (gameWrapper->IsInFreeplay()) return true;

	/*if (gameWrapper->IsInOnlineGame() || gameWrapper->IsInReplay() || !gameWrapper->IsInGame()) {
		cvarManager->log("Must be host of a freeplay session or LAN match");
		return false;
	}*/

	return true;
}

bool TeamTrainingPlugin::isValidServer(ServerWrapper& server)
{
	if (server.IsNull()) {
		cvarManager->log("Server is null, cannot continue to set shot");
		return false;
	}

	return isValidServer();
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
	if (!isValidServer() || !pack) {
		return;
	}

	setShot(current_shot);
}

void TeamTrainingPlugin::onNextShot(std::vector<std::string> params)
{
	if (!isValidServer() || !pack) {
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
	if (!isValidServer() || !pack) {
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

void TeamTrainingPlugin::onCustomTrainingLoaded(string event)
{
	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		// It seems this data is more reliably available if we wait a second
		inGameTrainingPackData.resetState();
		inGameTrainingPackData.failed = true;

		if (!gw->IsInCustomTraining()) {
			return;
		}

		auto te = TrainingEditorWrapper(gw->GetGameEventAsServer().memory_address);
		if (te.IsNull()) {
			return;
		}

		auto td = te.GetTrainingData();
		/*if (td.GetbUnowned() != 1) {
			return;
		}*/

		auto tdd = td.GetTrainingData();

		auto code = tdd.GetCode();
		if (!code.IsNull()) ::strncpy(inGameTrainingPackData.code, code.ToString().c_str(), sizeof(inGameTrainingPackData.code));

		auto description = tdd.GetTM_Name();
		if (!description.IsNull()) ::strncpy(inGameTrainingPackData.description, description.ToString().c_str(), sizeof(inGameTrainingPackData.description));

		auto creator = tdd.GetCreatorName();
		if (!creator.IsNull()) ::strncpy(inGameTrainingPackData.creator, creator.ToString().c_str(), sizeof(inGameTrainingPackData.creator));

		auto creatorID = UIDToString(tdd.GetCreatorPlayerUniqueID());
		::strncpy(inGameTrainingPackData.creatorID, creatorID.c_str(), sizeof(inGameTrainingPackData.creatorID));

		inGameTrainingPackData.numDrills = te.GetTotalRounds();

		inGameTrainingPackData.failed = false;
		}, 1.0f);
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
#pragma once
#pragma comment(lib, "BakkesMod.lib")
#include "TrainingPack.h"
#include "bakkesmod\plugin\bakkesmodplugin.h"
#include <fstream>

class TeamTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
public:
	void onLoad();
	void onUnload();

private:
	// Console commands
	void onLoadTrainingPack(std::vector<std::string> params);
	void onResetShot(std::vector<std::string> params);
	void onNextShot(std::vector<std::string> params);
	void onPrevShot(std::vector<std::string> params);
	void randomizePlayers(std::vector<std::string> params);
	void cyclePlayers(std::vector<std::string> params);
	void listPacks(std::vector<std::string> params);

	// Hooks
	void onResetShotEvent(std::string eventName);
	void onFreeplayDestroyed(std::string eventName);

	// Helper functions
	void setShot(int shot);
	void resetShot();
	void setPlayerToCar(TrainingPackPlayer player, CarWrapper car);
	bool validatePlayers(ServerWrapper tutorial);

	std::vector<unsigned int> player_order;
	std::shared_ptr<TrainingPack> pack;
	unsigned int current_shot = 0;
	unsigned int last_shot_set = 0;

	// Properties for printing custom training
	void writeShotInfo(std::vector<std::string> params);
	void onNextRound(std::string eventName);
	void onBallTick(ServerWrapper server, void * params, std::string eventName);
	void writeDrillToFile();
	void getNextShot();
	ofstream custom_training_export_file;
	int offense;
	int defense;
	int num_drills;
	int drills_written = 0;
	TrainingPackBall custom_training_ball;
	std::vector<TrainingPackPlayer> custom_training_players;

	// Random extra
	void addNewBall(std::vector<std::string> params);
	void twoBallTraining(std::vector<std::string> params);
	Vector2 start;
	Vector2 end;
};


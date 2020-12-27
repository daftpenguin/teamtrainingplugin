#include "TrainingPack.h"

#include <fstream>
#include <sstream>

TrainingPack::TrainingPack() : TrainingPack(fs::path()) {};


TrainingPack::TrainingPack(fs::path filepath, int offense, int defense, std::string description, std::string creator, std::string code,
	std::string creatorID, std::vector<std::string> tags, int num_drills) :
	filepath(filepath), offense(offense), defense(defense), description(description), creator(creator), code(code),
	creatorID(creatorID), uploader(""), uploaderID(""), uploadID(NO_UPLOAD_ID), expected_drills(num_drills), players_added(0)
{
	for (auto tag : tags) {
		this->tags.insert(tag);
	}
}

TrainingPack::TrainingPack(fs::path filepath) : filepath(filepath), version(0), offense(0), defense(0), description(""), creator(""), code(""),
	creatorID(""), uploader(""), uploaderID(""), uploadID(NO_UPLOAD_ID), expected_drills(0)
{
	load_time = std::chrono::system_clock::now();

	if (filepath.empty()) {
		return;
	}

	if (!fs::exists(filepath)) {
		errorMsg = "Pack not found";
	}

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

	inFile.close();

	from_json(js, *this);
}

char *TrainingPack::save()
{
	if (data_saved) {
		return NULL;
	}

	data_saved = true; // Assume when we call save, data was complete (seems that sometimes events might be triggered before we can unhook)

	version = LATEST_TRAINING_PACK_VERSION;

	std::ofstream file;
	file.open(filepath);
	if (file.fail()) {
		return strerror(errno);
	}
	else {
		json j(*this);
		file << j.dump(2) << std::endl;
	}

	return NULL;
}

void TrainingPack::addBallLocation(BallWrapper ball)
{
	TrainingPackBall &b = drills.back().ball;
	b.location = ball.GetLocation();
	ball_location_set = true;
}

void TrainingPack::setBallMovement(BallWrapper ball)
{
	if (!ball_location_set) {
		addBallLocation(ball);
	}

	TrainingPackBall& b = drills.back().ball;
	b.velocity = ball.GetVelocity().clone();
	b.angular = ball.GetAngularVelocity().clone();
	b.rotation = Rotator(ball.GetRotation());
	b.torque = Vector{ 0, 0, 0 };
	ball_velocity_set = true;
}

void TrainingPack::addPlayer(CarWrapper car)
{
	float boost = 100.0;
	auto boost_comp = car.GetBoostComponent();
	if (!boost_comp.IsNull()) {
		boost = boost_comp.GetCurrentBoostAmount();
	}
	TrainingPackPlayer player = TrainingPackPlayer(boost, car.GetLocation(), car.GetVelocity(), car.GetRotation());

	TrainingPackDrill& drill = drills.back();
	if (players_added == 0 && offense > 0) {
		drill.shooter = player;
		shooter_added = true;
	} else if (players_added < offense) {
		drill.passers.insert(drill.passers.begin(), player);
	} else {
		drill.defenders.push_back(player);
	}

	players_added++;
}

void TrainingPack::startNewDrill()
{
	drills.push_back(TrainingPackDrill());
	players_added = 0;
	ball_location_set = false;
	ball_velocity_set = false;
	shooter_added = false;
}

bool TrainingPack::lastPlayerAddedWasFirstPasser()
{
	return players_added == offense;
}

bool TrainingPack::allPlayersInDrillAdded()
{
	TrainingPackDrill& b = drills.back();
	return (shooter_added && b.passers.size() == (size_t) offense - 1 && b.defenders.size() == defense);
}

bool TrainingPack::expectingMoreDrills()
{
	return (drills.size() < expected_drills);
}

// Sets tags to given tags, and returns true if they are different than the current tags
bool TrainingPack::setTags(std::vector<std::string> newTags)
{
	bool isDiff = newTags.size() != tags.size();
	if (!isDiff) {
		for (auto& tag : newTags) {
			if (tags.find(tag) == tags.end()) {
				isDiff = true;
				break;
			}
		}
	}

	if (!isDiff) {
		return false;
	}

	tags.clear();
	for (auto& tag : newTags) {
		tags.insert(tag);
	}

	return isDiff;
}

void TrainingPack::addTag(std::string tag)
{
	tags.insert(tag);
}

void TrainingPack::removeTag(std::string tag)
{
	tags.erase(tag);
}


/*
 *	Serialization & Deserialization Stuff
 */

void from_json(const json& j, TrainingPack& p)
{
	if (j.find("version") == j.end()) {
		p.version = 1;
	}
	else {
		j.at("version").get_to(p.version);
	}

	j.at("offense").get_to(p.offense);
	j.at("defense").get_to(p.defense);

	if (p.version >= 2) {

		j.at("code").get_to(p.code);
		j.at("description").get_to(p.description);
		j.at("creator").get_to(p.creator);
	}
	if (p.version >= 4) { // Version 3 was for ball's angular field
		j.at("creatorID").get_to(p.creatorID);

		if (j.find("uploader") != j.end()) {
			j.at("uploader").get_to(p.uploader);
		}

		if (j.find("uploaderID") != j.end()) {
			j.at("uploaderID").get_to(p.uploaderID);
		}

		if (j.find("uploadID") == j.end()) {
			p.uploadID = NO_UPLOAD_ID;
		}
		else {
			j.at("uploadID").get_to(p.uploadID);
		}

		if (j.find("notes") != j.end()) {
			j.at("notes").get_to(p.notes);
		}

		if (j.find("youtube") != j.end()) {
			j.at("youtube").get_to(p.youtube);
		}
	}

	// Tags can be added to older packs so don't assume version here
	p.tags = std::unordered_set<std::string>();
	if (j.find("tags") != j.end()) {
		for (json tag : j["tags"]) {
			p.tags.insert(tag.get<std::string>());
		}
	}

	p.drills = std::vector<TrainingPackDrill>();
	if (j.find("drills") != j.end() && j["drills"].size() > 0) { // Could just be custom training metadata
		for (json drill : j["drills"]) {
			p.drills.push_back(drill.get<TrainingPackDrill>());
		}
		p.numDrills = p.drills.size();
	}
	else {
		p.numDrills = j["numDrills"];
	}
}

void to_json(json& j, const TrainingPack& p)
{
	j = json{ {"version", p.version}, {"offense", p.offense}, {"defense", p.defense} };

	if (p.version >= 2) {
		j["description"] = p.description;
		j["creator"] = p.creator;
		j["code"] = p.code;
	}
	if (p.version >= 4) {
		j["creatorID"] = p.creatorID;
		if (!p.uploader.empty()) j["uploader"] = p.uploader;
		if (!p.uploaderID.empty()) j["uploaderID"] = p.uploaderID;
		if (p.uploadID != NO_UPLOAD_ID) j["uploadID"] = p.uploadID;
		j["numDrills"] = p.numDrills;
		if (!p.notes.empty()) j["notes"] = p.notes;
		if (!p.youtube.empty()) j["youtube"] = p.youtube;
		if (p.tags.size() > 0) j["tags"] = json(p.tags);
	}

	if (p.drills.size() > 0) {
		j["drills"] = json(p.drills);
	}
}

void from_json(const json& j, TrainingPackDrill& d)
{
	j.at("ball").get_to(d.ball);

	for (json p : j["players"]) {
		auto player = p.get<TrainingPackPlayerWithRole>();
		if (player.role.compare("passer") == 0) {
			d.passers.push_back(player.player);
		}
		else if (player.role.compare("shooter") == 0) {
			d.shooter = player.player;
		}
		else if (player.role.compare("defender") == 0) {
			d.defenders.push_back(player.player);
		}
	}
}

void to_json(json& j, const TrainingPackDrill& d)
{
	j = json{ {"ball", d.ball} };

	j["players"].push_back(TrainingPackPlayerWithRole{ d.shooter, "shooter" });

	if (!d.passers.empty()) {
		for (const TrainingPackPlayer& player : d.passers) {
			j["players"].push_back(TrainingPackPlayerWithRole{ player, "passer" });
		}
	}

	if (!d.defenders.empty()) {
		for (const TrainingPackPlayer& player : d.defenders) {
			j["players"].push_back(TrainingPackPlayerWithRole{ player, "defender" });
		}
	}
}

void from_json(const json& j, TrainingPackBall& b)
{
	j.at("location").get_to(b.location);
	j.at("torque").get_to(b.torque);
	j.at("velocity").get_to(b.velocity);
	j.at("rotation").get_to(b.rotation);

	if (j.find("angular") != j.end()) {
		j.at("angular").get_to(b.angular);
	}
	else {
		b.angular = Vector{ 0, 0, 0 };
	}
}

void to_json(json& j, const TrainingPackBall& b)
{
	j = json{ {"location", json(b.location)},
			  {"torque", json(b.torque)},
			  {"velocity", json(b.velocity)},
			  {"rotation", json(b.rotation)},
			  {"angular", json(b.angular)}
	};
}

void from_json(const json& j, TrainingPackPlayer& p)
{
	j.at("boost").get_to(p.boost);
	j.at("location").get_to(p.location);
	j.at("rotation").get_to(p.rotation);
	j.at("velocity").get_to(p.velocity);
}

void to_json(json& j, const TrainingPackPlayer& p)
{
	j = json{
			{"boost", json(p.boost)},
			{"location", json(p.location)},
			{"rotation", json(p.rotation)},
			{"velocity", json(p.velocity)}
	};
}

void from_json(const json& j, TrainingPackPlayerWithRole& p)
{
	from_json(j, p.player);
	j.at("role").get_to(p.role);
}

void to_json(json& j, const TrainingPackPlayerWithRole& p)
{
	j = json(p.player);
	j["role"] = p.role;
}

void from_json(const json& j, Vector& v)
{
	j.at("x").get_to(v.X);
	j.at("y").get_to(v.Y);
	j.at("z").get_to(v.Z);
}

void to_json(json& j, const Vector& v)
{
	j = json{ {"x", v.X}, {"y", v.Y}, {"z", v.Z} };
}

void from_json(const json& j, Rotator& r)
{
	j.at("pitch").get_to(r.Pitch);
	j.at("yaw").get_to(r.Yaw);
	j.at("roll").get_to(r.Roll);
}

void to_json(json& j, const Rotator& r)
{
	j = json{ {"pitch", r.Pitch}, {"yaw", r.Yaw}, {"roll", r.Roll} };
}
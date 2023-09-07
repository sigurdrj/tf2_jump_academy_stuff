#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>

#include "json.hpp"

#define API_URL "https://api.jumpacademy.tf/mapinfo_json"
#define S 30 // Waypoint box size (half)
#define C 30 // Controlpoint box size (half)

using std::string;
using std::vector;
using json = nlohmann::json;

void usage(){
	std::cout << "Usage: generate_tf2_script [map vmf file] [script output file]\n";
}

struct Coordinate{
	double x, y, z;

	Coordinate(){};

	Coordinate(const string fromString){
		std::stringstream stream(fromString);
		stream >> x >> y >> z;
	};
};

struct Entity{
	int id;
	Coordinate origin;
	string targetName;
	Coordinate angles; // Unused here
	string className;

	Entity(){
		id = -1;
//		origin = Coordinate("-1 -1 -1");
		origin.x=-1, origin.y=-1, origin.z=-1;
		targetName = "";
//		angles = Coordinate("-1 -1 -1");
		angles.x=-1, angles.y=-1, angles.z=-1;
		className = "";
	};
};

std::pair <string, string> get_key_value_from_vmf_line(const string line){
	string key="", value="";

	bool inString = false;
	bool inKey = true;

	for (char c : line){
		if (c == '"'){
			inString ^= 1;
			if (!inString)
				inKey = false;

			continue;
		}

		if (inString){
			if (inKey)
				key += c;
			else
				value += c;
		}
	}

	return std::make_pair(key, value);
}

// Sure, probably not perfect but the getline works for the bsp decompiler I used
vector <Entity> get_all_entities_from_vmf_file(const string filename){
	vector <Entity> ret;

	std::ifstream file(filename);

	Entity e;

	unsigned long long lineNum = 0;
	bool inEntity = false;
	for (string line; std::getline(file, line); ++lineNum){
		if (line.starts_with("entity"))
			inEntity = true;

		if (!inEntity)
			continue;

		auto keyAndValue = get_key_value_from_vmf_line(line);
		string key   = keyAndValue.first;
		string value = keyAndValue.second;

		if (key == "id")
			e.id = std::stoi(value); // Can throw exception
		else if (key == "origin")
			e.origin = Coordinate(value);
		else if (key == "targetname")
			e.targetName = value;
		else if (key == "angles")
			e.angles = Coordinate(value);
		else if (key == "classname")
			e.className = value;

		if (inEntity && line.starts_with("}")){
			inEntity = false;

			// We don't care about nameless entities
			if (e.targetName == "")
				continue;

			ret.push_back(e);
			e = Entity();
		}
	}

	return ret;
}

Coordinate get_entity_origin_by_name(vector <Entity> entities, const string targetName, const string classNamePrefix){
	for (Entity e : entities){
		if (!e.className.starts_with(classNamePrefix))
			continue;

		if (e.targetName == targetName){
			return e.origin;
		}
	}

	// Found no entity
	return Coordinate("-1 -1 -1");
}

vector <Entity> get_entities_by_names(vector <Entity> entities, const string targetsName, const string classNamePrefix, const bool targetsNameIsPrefix){
	vector <Entity> ret;

	for (Entity e : entities){
		if (!e.className.starts_with(classNamePrefix))
			continue;

		if (!targetsNameIsPrefix){
			if (e.targetName == targetsName)
				ret.push_back(e);
		} else{
			if (e.targetName.starts_with(targetsName))
				ret.push_back(e);
		}
	}

	return ret;
}

int main(int argc, char *argv[]){
	if (argc < 3){
		usage();
		return 1;
	}

	const string mapFilename = argv[1];
	const string scriptOutputFilename = argv[2];

	const string mapName = mapFilename.substr(0, mapFilename.rfind('_'));

	curlpp::Easy client;

	std::stringstream responseStream;

	client.setOpt(new curlpp::options::Url(string(API_URL) + string("/?map=") + mapName + string("&layout=1")));
	client.setOpt(new curlpp::options::UserAgent("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:99.0) Gecko/20100101 Firefox/99.0"));
	client.setOpt(new curlpp::options::WriteStream(&responseStream));
	client.setOpt(new curlpp::options::Timeout(5)); // Seconds

	try{
		client.perform();
	} catch(curlpp::LibcurlRuntimeError &e){
		std::cerr << "Failed to do API request\n";
		return 2;
	}

	const string responseStr = responseStream.str();

	auto response = json::parse(responseStr);
	auto layout = response[mapName]["layout"];

	std::ofstream scriptFile(scriptOutputFilename);

	auto mapEntities = get_all_entities_from_vmf_file(mapFilename);
	auto entities = get_entities_by_names(mapEntities, "ingot_", "prop_dynamic", true);

	for (Entity e : entities){
		auto c = e.origin;

		std::cout << c.x << ' ' << c.y << ' ' << c.z << " // " << e.targetName << '\n';
		scriptFile << "box " << c.x-S << ' ' << c.y-S << ' ' << c.z-S*5 << ' ' << c.x+S << ' ' << c.y+S << ' ' << c.z+S*15 << " // " << e.targetName << '\n';
	}

	scriptFile.close();
}

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib> 

namespace fs = std::filesystem;

int main() {
	try {
		fs::path configPath = fs::current_path() / "src" / "configs" / "msfs-host.yaml";
		if (!fs::exists(configPath)) {
			std::cerr << "Config file not found: " << configPath << std::endl;
			return 1;
		}

		YAML::Node config = YAML::LoadFile(configPath.string());
		YAML::Node simconnect = config["simconnect"];

		// Extract values with defaults
		std::string listen_ip = simconnect["listen_ip"] ? simconnect["listen_ip"].as<std::string>() : "0.0.0.0";
		int port = simconnect["port"] ? simconnect["port"].as<int>() : 500;
		std::string protocol = simconnect["protocol"] ? simconnect["protocol"].as<std::string>() : "IPv4";
		std::string scope = simconnect["scope"] ? simconnect["scope"].as<std::string>() : "global";
		int max_clients = simconnect["max_clients"] ? simconnect["max_clients"].as<int>() : 64;
		std::string server_name = simconnect["server_name"] ? simconnect["server_name"].as<std::string>() : "MSFS_Host";

        // Resolve output path
        const char* appdata = std::getenv("APPDATA");
        if (!appdata) {
            std::cerr << "APPDATA environment variable not found" << std::endl;
            return 1;
        }
        fs::path outDir = fs::path(appdata) / "Microsoft Flight Simulator";
        fs::create_directories(outDir);
        fs::path outFile = outDir / "SimConnect.xml";

        // Write XML file manually
        std::ofstream xml(outFile);
        if (!xml.is_open()) {
            std::cerr << "Failed to open file for writing: " << outFile << std::endl;
            return 1;
        }

        xml << R"(<?xml version="1.0" encoding="utf-8"?>)" << "\n";
        xml << "<SimBase.Document Type=\"SimConnect\" version=\"1,0\">" << "\n";
        xml << "  <Descr>SimConnect Server Configuration</Descr>\n";
        xml << "  <Filename>SimConnect.xml</Filename>\n";
        xml << "  <SimConnect.Comm>\n";
        xml << "    <Disabled>False</Disabled>\n";
        xml << "    <Protocol>" << protocol << "</Protocol>\n";
        xml << "    <Scope>" << scope << "</Scope>\n";
        xml << "    <Address>" << listen_ip << "</Address>\n";
        xml << "    <Port>" << port << "</Port>\n";
        xml << "    <MaxClients>" << max_clients << "</MaxClients>\n";
        xml << "    <ServerName>" << server_name << "</ServerName>\n";
        xml << "  </SimConnect.Comm>\n";
        xml << "</SimBase.Document>\n";

        xml.close();
        std::cout << "Generated " << outFile << std::endl;
	}
	catch (const std::exception& ex) {
		std::cerr << "Exception: " << ex.what() << std::endl;
		return 1;
	}
    return 0;
}
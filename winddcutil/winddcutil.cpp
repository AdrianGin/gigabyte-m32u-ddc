#include "stdafx.h"


#include <atlstr.h> // CW2A
#include <PhysicalMonitorEnumerationAPI.h>
#include <LowLevelMonitorConfigurationAPI.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

std::vector<PHYSICAL_MONITOR> physicalMonitors{};


BOOL CALLBACK monitorEnumProcCallback(HMONITOR hMonitor, HDC hDeviceContext, LPRECT rect, LPARAM data)
{
	DWORD numberOfPhysicalMonitors;
	BOOL success = GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numberOfPhysicalMonitors);
	if (success) {
		auto originalSize = physicalMonitors.size();
		physicalMonitors.resize(physicalMonitors.size() + numberOfPhysicalMonitors);
		success = GetPhysicalMonitorsFromHMONITOR(hMonitor, numberOfPhysicalMonitors, physicalMonitors.data() + originalSize);
	}
	return true;
}

std::string toUtf8(wchar_t *buffer)
{
	CW2A utf8(buffer, CP_UTF8);
	const char* data = utf8.m_psz;
	return std::string{ data };
}

int printUsage()
{
	std::cout << "Usage: windcutil command [<arg> ...]" << std::endl
		<< "Commands:" << std::endl
		<< "\tdetect                                         Detect monitors" << std::endl
		<< "\tcapabilities <display-id>                      Query monitor capabilities" << std::endl
		<< "\tsetvcp <display-id> <feature-code> <new-value> Set VCP feature value" << std::endl;
	return 1;
}

int detect(std::vector<std::string> args)
{
	int i = 0;
	for (auto &physicalMonitor : physicalMonitors) {
		std::cout << "Display " << i << ":\t" << toUtf8(physicalMonitor.szPhysicalMonitorDescription) << std::endl;
		i++;
	}
	return 0;
}

int capabilities(std::vector<std::string> args) {
	if (args.size() < 1) {
		std::cerr << "Display ID required" << std::endl;
		return printUsage();
	}
	
	int displayId = 0;
	try {
		displayId = std::stoi(args[0]);
	}
	catch (...) {
		std::cerr << "Failed to parse display ID" << std::endl;
		return 1;
	}

	if (displayId > physicalMonitors.size() - 1) {
		std::cerr << "Invalid display ID" << std::endl;
		return 1;
	}

	auto physicalMonitorHandle = physicalMonitors[displayId].hPhysicalMonitor;

	DWORD capabilitiesStringLengthInCharacters;
	auto success = GetCapabilitiesStringLength(physicalMonitorHandle, &capabilitiesStringLengthInCharacters);
	if (!success) {
		std::cerr << "Failed to get capabilities string length" << std::endl;
		return 1;
	}

	std::unique_ptr<char[]> capabilitiesString{ new char[capabilitiesStringLengthInCharacters] };
	success = CapabilitiesRequestAndCapabilitiesReply(physicalMonitorHandle, capabilitiesString.get(), capabilitiesStringLengthInCharacters);
	if (!success) {
		std::cerr << "Failed to get capabilities string" << std::endl;
		return 1;
	}

	std::cout << std::string(capabilitiesString.get()) << std::endl;

	return 0;
}

int setVcp(std::vector<std::string> args) {

	// hex to long
	std::string s = "60";
	DWORD x = std::stoul(s, nullptr, 16);
	std::cout << s << " = " << x << std::endl;


	size_t displayId = std::stoul("0");
	int vcpInt = std::stoi("96"); // Input Source 96 (0x60)
	if (vcpInt > 255)
		throw std::out_of_range{ "VCP code must be less than 255." };
	BYTE vcpCode = static_cast<BYTE>(vcpInt);
	DWORD newValue = std::stoul("17"); // 17 (0x11), 18 (0x12), 15 (0x0F)
	bool success = SetVCPFeature(physicalMonitors[displayId].hPhysicalMonitor, vcpCode, newValue);
	if (!success)
		std::cerr << "Failed to set vcp feature" << std::endl;
	return 0;
}

std::unordered_map<std::string, std::function<int(std::vector<std::string>)>> commands
{
	{ "detect", detect},
	{ "capabilities", capabilities },
	{ "setvcp", setVcp},
};


int main(int argc, char *argv[], char *envp[])
{
	if (argc < 2) {
		return printUsage();
	}

	std::vector<std::string> args;
	for (int i = 1; i < argc; i++) {
		std::string arg(argv[i]);
		args.emplace_back(arg);
	}

	auto command = commands.find(args[0]);
	if (command == commands.end()) {
		std::cerr << "Unkown command" << std::endl;
		return printUsage();
	}
	args.erase(args.begin());

	EnumDisplayMonitors(NULL, NULL, &monitorEnumProcCallback, 0);

	auto success = command->second(args);

	DestroyPhysicalMonitors(physicalMonitors.size(), physicalMonitors.data());

	return success;
}


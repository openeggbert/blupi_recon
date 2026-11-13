// Port settings file (see settings.h): a flat JSON object of booleans,
// numbers and strings, read leniently (unknown keys are ignored) and written
// whole.

#include "ui/settings.h"
#include "platform/host.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

// Host paths: the forced include maps fopen to the game file system.
#pragma push_macro("fopen")
#undef fopen
static FILE* host_fopen(const char* path, const char* mode)
{
	return fopen(path, mode);
}
#pragma pop_macro("fopen")

namespace
{
// key -> raw value text (true, false, a number or a quoted string)
bool ParseFlatObject(const std::string& text, std::map<std::string, std::string>& out)
{
	size_t i = text.find('{');
	if (i == std::string::npos) return false;
	i++;
	auto skip = [&]() { while (i < text.size() && isspace((unsigned char)text[i])) i++; };
	for (;;)
	{
		skip();
		if (i >= text.size()) return false;
		if (text[i] == '}') return true;
		if (text[i] == ',') { i++; continue; }
		if (text[i] != '"') return false;
		size_t end = text.find('"', i + 1);
		if (end == std::string::npos) return false;
		std::string key = text.substr(i + 1, end - i - 1);
		i = end + 1;
		skip();
		if (i >= text.size() || text[i] != ':') return false;
		i++;
		skip();
		size_t start = i;
		if (i < text.size() && text[i] == '"')
		{
			for (i++; i < text.size() && text[i] != '"'; i++)
			{
				if (text[i] == '\\') i++;
			}
			if (i >= text.size()) return false;
			i++;
		}
		else
		{
			while (i < text.size() && text[i] != ',' && text[i] != '}' && !isspace((unsigned char)text[i])) i++;
		}
		out[key] = text.substr(start, i - start);
	}
}

void ReadBool(const std::map<std::string, std::string>& values, const char* key, bool& value)
{
	auto it = values.find(key);
	if (it == values.end()) return;
	if (it->second == "true") value = true;
	else if (it->second == "false") value = false;
}

// A quoted value, with the escapes Quoted writes (\\ and \"). Any other
// backslash is kept, so a hand-written Windows path reads as written.
void ReadString(const std::map<std::string, std::string>& values, const char* key, std::string& value)
{
	auto it = values.find(key);
	if (it == values.end()) return;
	const std::string& raw = it->second;
	if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') return;
	std::string out;
	for (size_t i = 1; i + 1 < raw.size(); i++)
	{
		if (raw[i] == '\\' && i + 2 < raw.size() && (raw[i + 1] == '\\' || raw[i + 1] == '"')) i++;
		out += raw[i];
	}
	value = out;
}

std::string Quoted(const std::string& text)
{
	std::string out = "\"";
	for (char c : text)
	{
		if (c == '\\' || c == '"') out += '\\';
		out += c;
	}
	return out + "\"";
}

void ReadInt(const std::map<std::string, std::string>& values, const char* key, int& value, int lo, int hi)
{
	auto it = values.find(key);
	if (it == values.end()) return;
	char* end = NULL;
	long v = strtol(it->second.c_str(), &end, 10);
	if (end != it->second.c_str() && v >= lo && v <= hi) value = (int)v;
}
}  // namespace

bool PortSettingsLoad(const char* path, PortSettings& settings)
{
	FILE* file = host_fopen(path, "rb");
	if (file == NULL) return false;
	std::string text;
	char buffer[512];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, n);
	fclose(file);

	std::map<std::string, std::string> values;
	if (!ParseFlatObject(text, values)) return false;
	ReadBool(values, "widescreen", settings.widescreen);
	ReadBool(values, "interpolate", settings.interpolate);
	ReadBool(values, "displayRate", settings.displayRate);
	ReadBool(values, "fullscreen", settings.fullscreen);
	ReadBool(values, "integerScale", settings.integerScale);
	ReadInt(values, "scale", settings.scale, 1, 8);
	ReadString(values, "dataFolder", settings.dataFolder);
	ReadString(values, "language", settings.language);
	ReadString(values, "gamepadA", settings.gamepadA);
	ReadString(values, "gamepadB", settings.gamepadB);
	ReadString(values, "gamepadX", settings.gamepadX);
	ReadString(values, "gamepadY", settings.gamepadY);
	ReadString(values, "gamepadStart", settings.gamepadStart);
	ReadString(values, "gamepadBack", settings.gamepadBack);
	ReadString(values, "gamepadLeftShoulder", settings.gamepadLeftShoulder);
	ReadString(values, "gamepadRightShoulder", settings.gamepadRightShoulder);
	ReadBool(values, "gamepadArrows", settings.gamepadArrows);
	ReadString(values, "netAddress", settings.netAddress);
	ReadString(values, "netServer", settings.netServer);
	ReadString(values, "netRoom", settings.netRoom);
	ReadInt(values, "hdScale", settings.hdScale, 1, 4);
	ReadString(values, "hdPack", settings.hdPack);
	return true;
}

PortSettings PortSettingsMerge(const PortSettings& file, const PortSettings& given, const PortSettingsGiven& set)
{
	PortSettings s = file;
	if (set.widescreen) s.widescreen = given.widescreen;
	if (set.interpolate) s.interpolate = given.interpolate;
	if (set.displayRate) s.displayRate = given.displayRate;
	if (set.fullscreen) s.fullscreen = given.fullscreen;
	if (set.scale) s.scale = given.scale;
	if (set.integerScale) s.integerScale = given.integerScale;
	if (set.language) s.language = given.language;
	if (set.netServer) s.netServer = given.netServer;
	if (set.netRoom) s.netRoom = given.netRoom;
	if (set.hdScale) s.hdScale = given.hdScale;
	if (set.hdPack) s.hdPack = given.hdPack;
	return s;
}

// The game's own keys (Win32 virtual-key numbers, the vocabulary of CEvent::TreatEvent):
// Ctrl jumps, Space fires, Pause pauses, Escape leaves, Enter confirms.
static int GamepadAction(const std::string& name)
{
	static const struct { const char* name; int key; } actions[] =
	{
		{ "jump", 0x11 }, { "fire", 0x20 }, { "pause", 0x13 }, { "escape", 0x1B }, { "enter", 0x0D },
		{ "left", 0x25 }, { "up", 0x26 }, { "right", 0x27 }, { "down", 0x28 },
	};
	for (const auto& a : actions)
	{
		if (name == a.name) return a.key;
	}
	return 0;
}

void PortGamepadKeys(const PortSettings& settings, blupi::host::GamepadKeys& keys)
{
	keys.south = GamepadAction(settings.gamepadA);
	keys.east = GamepadAction(settings.gamepadB);
	keys.west = GamepadAction(settings.gamepadX);
	keys.north = GamepadAction(settings.gamepadY);
	keys.start = GamepadAction(settings.gamepadStart);
	keys.back = GamepadAction(settings.gamepadBack);
	keys.leftShoulder = GamepadAction(settings.gamepadLeftShoulder);
	keys.rightShoulder = GamepadAction(settings.gamepadRightShoulder);
	keys.arrows = settings.gamepadArrows;
}

bool PortSettingsSave(const char* path, const PortSettings& settings)
{
	FILE* file = host_fopen(path, "wb");
	if (file == NULL) return false;
	auto b = [](bool v) { return v ? "true" : "false"; };
	fprintf(file,
			"{\n"
			"  \"widescreen\": %s,\n"
			"  \"interpolate\": %s,\n"
			"  \"displayRate\": %s,\n"
			"  \"fullscreen\": %s,\n"
			"  \"integerScale\": %s,\n"
			"  \"scale\": %d",
			b(settings.widescreen), b(settings.interpolate), b(settings.displayRate),
			b(settings.fullscreen), b(settings.integerScale), settings.scale);
	if (!settings.dataFolder.empty())
	{
		fprintf(file, ",\n  \"dataFolder\": %s", Quoted(settings.dataFolder).c_str());
	}
	if (!settings.language.empty())
	{
		fprintf(file, ",\n  \"language\": %s", Quoted(settings.language).c_str());
	}
	fprintf(file,
			",\n  \"gamepadA\": %s,\n  \"gamepadB\": %s,\n  \"gamepadX\": %s,\n  \"gamepadY\": %s"
			",\n  \"gamepadStart\": %s,\n  \"gamepadBack\": %s"
			",\n  \"gamepadLeftShoulder\": %s,\n  \"gamepadRightShoulder\": %s,\n  \"gamepadArrows\": %s",
			Quoted(settings.gamepadA).c_str(), Quoted(settings.gamepadB).c_str(), Quoted(settings.gamepadX).c_str(),
			Quoted(settings.gamepadY).c_str(), Quoted(settings.gamepadStart).c_str(),
			Quoted(settings.gamepadBack).c_str(), Quoted(settings.gamepadLeftShoulder).c_str(),
			Quoted(settings.gamepadRightShoulder).c_str(), b(settings.gamepadArrows));
	if (!settings.netAddress.empty())
	{
		fprintf(file, ",\n  \"netAddress\": %s", Quoted(settings.netAddress).c_str());
	}
	if (!settings.netServer.empty())
	{
		fprintf(file, ",\n  \"netServer\": %s", Quoted(settings.netServer).c_str());
	}
	if (!settings.netRoom.empty())
	{
		fprintf(file, ",\n  \"netRoom\": %s", Quoted(settings.netRoom).c_str());
	}
	if (settings.hdScale > 1)
	{
		fprintf(file, ",\n  \"hdScale\": %d", settings.hdScale);
	}
	if (!settings.hdPack.empty())
	{
		fprintf(file, ",\n  \"hdPack\": %s", Quoted(settings.hdPack).c_str());
	}
	fprintf(file, "\n}\n");
	bool ok = ferror(file) == 0;
	fclose(file);
	return ok;
}

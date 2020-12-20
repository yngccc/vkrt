#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <stack>
#include <string>
using namespace std::literals;
#include <algorithm>
#include <fstream>
#include <filesystem>

#include <shellscalingapi.h>

#include <SDL.h>
#include <SDL_syswm.h>

typedef unsigned uint;
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

template<typename T, unsigned N>
constexpr unsigned countof(T const (&)[N]) {
	return N;
}

auto operator""_kb(uint64 n) -> uint64 {
	return 1024 * n;
}

auto operator""_mb(uint64 n) -> uint64 {
	return 1024 * 1024 * n;
}

auto operator""_gb(uint64 n) -> uint64 {
	return 1024 * 1024 * 1024 * n;
}

uint64 align(uint64 x, uint64 n) {
	uint64 remainder = x % n;
	if (remainder == 0) {
		return x;
	}
	else {
		return x + (n - remainder);
	}
}

void fatal(const std::string& msg) {
	SDL_MessageBoxData messageBoxData = {};
	messageBoxData.title = "Error";
	messageBoxData.message = msg.c_str();
	messageBoxData.numbuttons = 1;
	SDL_MessageBoxButtonData buttonData = {};
	buttonData.text = "OK";
	messageBoxData.buttons = &buttonData;
	SDL_ShowMessageBox(&messageBoxData, nullptr);
	std::exit(EXIT_FAILURE);
}

struct Exception {
	Exception() {}
	Exception(const char* msg) {}
	Exception(const std::string& msg) {}
};

void setCurrentDirToExeDir() {
	wchar_t buf[512];
	DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(countof(buf)));
	if (n >= countof(buf)) {
		throw Exception();
	}
	std::filesystem::path path(buf);
	std::filesystem::path parentPath = path.parent_path();
	BOOL success = SetCurrentDirectoryW(parentPath.c_str());
	if (!success) {
		throw Exception();
	}
}

std::vector<char> readFile(const std::filesystem::path& path) {
	std::fstream file(path, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		throw Exception();
	}
	std::vector<char> data(std::istreambuf_iterator<char>{file}, {});
	return data;
}

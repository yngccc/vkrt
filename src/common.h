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
#include <tuple>

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

auto operator""_deg(long double deg) -> long double {
	return deg * M_PI / 180.0;
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

uint8* align(uint8* ptr, uint64 n) {
	return (uint8*)align((intptr_t)ptr, n);
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

template <typename T>
struct Range {
	Range(T* first, T* last) : begin_{ first }, end_{ last } {}
	Range(T* first, size_t size) : Range{ first, first + size } {}

	T* begin() const noexcept { return begin_; }
	T* end() const noexcept { return end_; }

	T* begin_;
	T* end_;
};

template <typename T>
Range<T> makeRange(T* first, size_t size) noexcept {
	return Range(first, size);
}

template <typename T>
Range<T> makeRange(T* first, T* last) noexcept {
	return Range(first, last);
}

template <typename T, typename TIter = decltype(std::begin(std::declval<T>())), typename = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T&& iterable) {
	struct iterator {
		size_t i;
		TIter iter;
		bool operator!= (const iterator& other) const { return iter != other.iter; }
		void operator++ () { ++i; ++iter; }
		auto operator* () const { return std::tie(i, *iter); }
	};
	struct iterable_wrapper {
		T iterable;
		auto begin() { return iterator{ 0, std::begin(iterable) }; }
		auto end() { return iterator{ 0, std::end(iterable) }; }
	};
	return iterable_wrapper{ std::forward<T>(iterable) };
}

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

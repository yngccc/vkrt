#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <stack>
#include <string>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <tuple>
using namespace std::literals;

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
	return deg / 180.0 * M_PI;
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
	DWORD n = GetModuleFileNameW(nullptr, buf, countof(buf));
	if (n >= countof(buf)) {
		assert(false && "setCurrentDirToExeDir -> GetModuleFileNameW error");
	}
	std::filesystem::path path(buf);
	std::filesystem::path parentPath = path.parent_path();
	BOOL success = SetCurrentDirectoryW(parentPath.c_str());
	if (!success) {
		assert(false && "setCurrentDirToExeDir -> SetCurrentDirectoryW error");
	}
}

std::vector<char> readFile(const std::filesystem::path& path) {
	std::fstream file(path, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		assert(false && "readFile -> cannot open file");
	}
	std::vector<char> data(std::istreambuf_iterator<char>{file}, {});
	return data;
}

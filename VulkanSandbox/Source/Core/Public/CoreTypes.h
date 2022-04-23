#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Type aliases

//~ Unsigned base types
using uint8 = unsigned char;
using uint16 = unsigned short int;
using uint32 = unsigned int;
using uint64 = unsigned long long;

//~ Signed base types.
using int8 = signed char;
using int16 = signed short int;
using int32 = signed int;
using int64 = signed long long;

//~ Smart Pointers
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T, typename ... Args>
constexpr UniquePtr<T> MakeUnique(Args&& ... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename ... Args>
constexpr SharedPtr<T> MakeShared(Args&& ... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename T>
using WeakPtr = std::weak_ptr<T>;

//~ Data Containers
template<typename T>
using Array = std::vector<T>;

template<typename T, typename T2>
using Map = std::unordered_map<T, T2>;

template<typename T>
using Deque = std::deque<T>;

template<typename T>
using Queue = std::queue<T>;

//~ Misc
using String = std::string;

#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace logger = SKSE::log;
using namespace std::literals;

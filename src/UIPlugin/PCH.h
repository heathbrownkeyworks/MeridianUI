#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

using namespace std::literals;

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "Common/Logger.h"

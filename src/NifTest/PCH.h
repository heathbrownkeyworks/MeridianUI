#pragma once

#define DLLEXPORT __declspec(dllexport)
#define PLUGIN_NAME "MeridianNifTest"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

using namespace std::literals;
namespace logger = SKSE::log;


/*
 * LayoutLoader.cpp
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#include "LayoutLoader.h"
#include "Debug.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace UI::Layout
{
#if SIMULATION
#  define LAYOUTS_DIR "assets/layouts"
#else
#  define LAYOUTS_DIR "/etc/assets/layouts"
#endif

	std::optional<nlohmann::json> loadLayoutDocument(std::string_view relativePath)
	{
		ZoneScoped;
		const std::string path = fmt::format("{}/{}", LAYOUTS_DIR, relativePath);
		LOG_DBG("Loading layout document: {:s}", path);

		std::ifstream file(path);
		if (!file.is_open())
		{
			LOG_ERROR("Failed to open layout document: {:s}", path);
			return std::nullopt;
		}

		try
		{
			return nlohmann::json::parse(file, /* callback */ nullptr, /* allow_exceptions */ true,
										 /* ignore_comments */ true);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Failed to parse layout document '{:s}': {:s}", path, e.what());
			return std::nullopt;
		}
	}

	std::vector<LayoutInfo> getAvailableLayouts()
	{
		ZoneScoped;
		std::vector<LayoutInfo> layouts;

		std::error_code ec;
		std::filesystem::directory_iterator dirIter(LAYOUTS_DIR, ec);
		if (ec)
		{
			LOG_ERROR("Failed to list layouts directory '{:s}': {:s}", LAYOUTS_DIR, ec.message());
			return layouts;
		}

		for (const auto& entry : dirIter)
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json")
			{
				continue;
			}

			const std::string filename = entry.path().filename().string();
			std::optional<nlohmann::json> doc = loadLayoutDocument(filename);
			if (!doc)
			{
				continue; // Already logged by loadLayoutDocument().
			}

			layouts.push_back({.file = filename, .name = doc->value("name", filename)});
		}

		std::sort(layouts.begin(), layouts.end(), [](const LayoutInfo& a, const LayoutInfo& b) { return a.file < b.file; });
		return layouts;
	}
} // namespace UI::Layout

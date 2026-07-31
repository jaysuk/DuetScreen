/*
 * LayoutLoader.cpp
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#include "LayoutLoader.h"
#include "Debug.h"
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
} // namespace UI::Layout

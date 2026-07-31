/*
 * LayoutLoader.h
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

namespace UI::Layout
{
	/**
	 * @brief Reads and parses a layout document from the layouts asset folder
	 * (assets/layouts/ in Simulation, /etc/assets/layouts/ on-device - mirrors how i18n/icon assets
	 * are located).
	 *
	 * @param relativePath Path relative to the layouts folder, e.g. "default.json".
	 * @return The parsed document, or std::nullopt if the file couldn't be opened or parsed - the
	 * reason is logged either way.
	 */
	std::optional<nlohmann::json> loadLayoutDocument(std::string_view relativePath);
} // namespace UI::Layout

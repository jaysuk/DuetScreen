/*
 * LayoutLoader.h
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

	struct LayoutInfo
	{
		std::string file; // Filename, e.g. "default.json" - pass to loadLayoutDocument()/ID_LAYOUT_FILE.
		std::string name; // Display name, from the document's "name" field (falls back to `file`).
	};

	/**
	 * @brief Lists every layout document in the layouts asset folder, in a consistent (sorted by
	 * filename) order.
	 *
	 * @note Unlike loadLayoutDocument(), a document that fails to open or parse is silently skipped
	 * rather than reported here (still logged) - this is meant for populating a picker, where one
	 * bad file shouldn't stop the rest from being offered.
	 */
	std::vector<LayoutInfo> getAvailableLayouts();

	/// The reserved ID_LAYOUT_FILE value meaning "load/save via ID_CUSTOM_LAYOUT in Storage instead
	/// of a file in the layouts asset folder" - deliberately not ".json"-suffixed so it can never
	/// collide with a real shipped preset's filename.
	inline constexpr std::string_view CUSTOM_LAYOUT_SENTINEL = "__custom__";

	/// Parses the document currently stored under ID_CUSTOM_LAYOUT (see Storage.h - stored as
	/// serialized JSON text, since StorageKey's default value must be consteval-constructible and
	/// nlohmann::json isn't a literal type). std::nullopt if it's unset/empty or fails to parse - the
	/// reason is logged either way, matching loadLayoutDocument().
	std::optional<nlohmann::json> loadCustomLayoutDocument();

	/// Serializes `doc` and persists it under ID_CUSTOM_LAYOUT.
	void saveCustomLayoutDocument(const nlohmann::json& doc);
} // namespace UI::Layout

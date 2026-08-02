/*
 * LayoutFileTransfer.h
 *
 *  Created on: 2026-08-02
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
	 * @brief Import/export of layout documents via a USB stick plugged into the touchscreen itself
	 * (docs/LAYOUT_ENGINE_DESIGN.md section 5.3, Tier 3). Deliberately USB-only for this phase - the
	 * alternative the design doc mentions (pushing a layout to the Duet controller's own SD card over
	 * the existing connection) would need Comm::Duet's generic download primitive, which isn't
	 * implemented over serial transport today, only network - USB reuses proven, already-working code
	 * (the same USB::UsbMonitor/ListEntriesInDirectory/ReadFileContents machinery behind the existing
	 * firmware-upgrade-from-USB flow) instead of extending a half-finished path.
	 */

	/// The mounted USB path to use, or std::nullopt if none is currently mounted. Thin wrapper around
	/// USB::UsbMonitor so callers don't need to know that API.
	std::optional<std::string> findMountedUsbDrive();

	/// Writes `doc` to "<usbRoot>/<sanitized name>_<timestamp>.json". The name comes from doc's own
	/// "name" field (falls back to "layout" if absent/not a string); forbidden FAT32 characters
	/// (\ / : * ? " < > |) become underscores, since USB sticks are commonly FAT32-formatted.
	///
	/// @return The written filename (not the full path), or std::nullopt on failure (logged).
	std::optional<std::string> exportLayoutToUsb(const nlohmann::json& doc, std::string_view usbRoot);

	struct ImportCandidate
	{
		std::string filename; // relative to the USB root, e.g. "My_Layout_20260802-143012.json"
		std::string name;	   // the document's own "name" field, falling back to `filename`
		nlohmann::json doc;   // already parsed and validated - see LayoutBuilder::validate()
	};

	/// Scans the root of `usbRoot` for regular *.json files, parsing and validating each with
	/// LayoutBuilder::validate(). A file that fails to open, parse, or validate is silently skipped
	/// (still logged) - matching getAvailableLayouts()'s existing "one bad file shouldn't stop the
	/// rest" convention - rather than failing the whole scan.
	std::vector<ImportCandidate> findImportableLayoutsOnUsb(std::string_view usbRoot);
} // namespace UI::Layout

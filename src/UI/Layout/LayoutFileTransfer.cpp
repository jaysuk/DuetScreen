/*
 * LayoutFileTransfer.cpp
 *
 *  Created on: 2026-08-02
 *      Author: Jay S
 */

#include "LayoutFileTransfer.h"
#include "Debug.h"
#include "Hardware/Usb.h"
#include "UI/Layout/LayoutBuilder.h"
#include <algorithm>
#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <ctime>

namespace UI::Layout
{
	namespace
	{
		std::string sanitizeForFilename(std::string_view name)
		{
			std::string result(name);
			// FAT32 forbids these in long filenames - USB sticks are commonly FAT32-formatted.
			static constexpr std::string_view forbidden = "\\/:*?\"<>|";
			std::replace_if(
				result.begin(), result.end(),
				[](char c) { return forbidden.find(c) != std::string_view::npos; }, '_');
			return result;
		}

		std::string timestampSuffix()
		{
			const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm tm{};
			localtime_r(&t, &tm);
			char buf[32];
			std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
			return buf;
		}
	} // namespace

	std::optional<std::string> findMountedUsbDrive()
	{
		ZoneScoped;
		const auto drives = USB::UsbMonitor::getInstance().getMountedDrives();
		if (drives.empty())
		{
			return std::nullopt;
		}
		return drives.front();
	}

	std::optional<std::string> exportLayoutToUsb(const nlohmann::json& doc, std::string_view usbRoot)
	{
		ZoneScoped;
		const std::string name = sanitizeForFilename(doc.value("name", std::string("layout")));
		const std::string filename = fmt::format("{}_{}.json", name, timestampSuffix());
		const std::string fullPath = fmt::format("{}/{}", usbRoot, filename);

		std::ofstream file(fullPath);
		if (!file.is_open())
		{
			LOG_ERROR("Failed to open '{:s}' for writing", fullPath);
			return std::nullopt;
		}

		const std::string json = doc.dump(2);
		file << json;
		if (!file.good())
		{
			LOG_ERROR("Failed to write layout document to '{:s}'", fullPath);
			return std::nullopt;
		}

		LOG_INFO("Exported layout document to '{:s}'", fullPath);
		return filename;
	}

	std::vector<ImportCandidate> findImportableLayoutsOnUsb(std::string_view usbRoot)
	{
		ZoneScoped;
		std::vector<ImportCandidate> candidates;

		for (const auto& entry : USB::ListEntriesInDirectory(std::string(usbRoot)))
		{
			if (entry.d_type != DT_REG)
			{
				continue;
			}
			const std::string filename = entry.d_name;
			if (!filename.ends_with(".json"))
			{
				continue;
			}

			std::string contents;
			const std::string fullPath = fmt::format("{}/{}", usbRoot, filename);
			if (!USB::ReadFileContents(fullPath, contents))
			{
				LOG_WARN("Failed to read '{:s}', skipping", fullPath);
				continue;
			}

			nlohmann::json doc;
			try
			{
				doc = nlohmann::json::parse(contents);
			}
			catch (const std::exception& e)
			{
				LOG_WARN("Failed to parse '{:s}' as JSON, skipping: {:s}", fullPath, e.what());
				continue;
			}

			if (!LayoutBuilder::validate(doc).ok)
			{
				LOG_WARN("'{:s}' is not a valid layout document, skipping", fullPath);
				continue;
			}

			candidates.push_back({.filename = filename, .name = doc.value("name", filename), .doc = std::move(doc)});
		}

		return candidates;
	}
} // namespace UI::Layout

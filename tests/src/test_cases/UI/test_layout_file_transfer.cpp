/*
 * test_layout_file_transfer.cpp
 *
 *  Created on: 2026-08-02
 *      Author: Jay S
 */

#include "UI/Layout/DashboardWidgets.h"
#include "UI/Layout/LayoutFileTransfer.h"
#include "test_utils/TestSuite.h"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <gtest/gtest.h>

using namespace UI::Layout;
using json = nlohmann::json;

namespace
{
	json validDoc(std::string_view name)
	{
		return json::parse(fmt::format(R"({{
			"schema": 1,
			"name": "{}",
			"root": {{ "widget": "status", "id": "status" }}
		}})",
										name));
	}
} // namespace

// A real temp directory stands in for a USB mount - LayoutFileTransfer's scan/read functions are
// plain filesystem operations (via USB::ListEntriesInDirectory/ReadFileContents), not actually
// USB-specific, so no real USB hardware is needed to exercise them.
class TestLayoutFileTransfer : public TestSuite
{
  protected:
	void SetUp() override
	{
		// LayoutBuilder::validate() (used internally by findImportableLayoutsOnUsb()) checks widget
		// ids against the real WidgetRegistry::get() singleton - normally populated by Dashboard's
		// constructor, which this plain (non-UI) test never runs. registerDashboardWidgets() is
		// idempotent (see TestDashboardWidgets) and only registers descriptors/factories, not live
		// LVGL widgets, so it's safe to call without a UI context.
		UI::Layout::registerDashboardWidgets();
		m_dir = std::filesystem::temp_directory_path() / "duetscreen_layout_transfer_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override { std::filesystem::remove_all(m_dir); }

	std::filesystem::path m_dir;
};

TEST_F(TestLayoutFileTransfer, ExportWritesAParseableFileNamedFromTheDocument)
{
	auto filename = exportLayoutToUsb(validDoc("My Custom Layout"), m_dir.string());
	ASSERT_TRUE(filename.has_value());
	EXPECT_NE(filename->find("My Custom Layout"), std::string::npos);
	EXPECT_TRUE(filename->ends_with(".json"));
	EXPECT_TRUE(std::filesystem::exists(m_dir / *filename));
}

TEST_F(TestLayoutFileTransfer, ExportSanitizesForbiddenFilenameCharacters)
{
	auto filename = exportLayoutToUsb(validDoc("Weird:Name?"), m_dir.string());
	ASSERT_TRUE(filename.has_value());
	EXPECT_EQ(filename->find(':'), std::string::npos);
	EXPECT_EQ(filename->find('?'), std::string::npos);
	EXPECT_TRUE(std::filesystem::exists(m_dir / *filename));
}

TEST_F(TestLayoutFileTransfer, ExportedDocumentRoundTripsThroughImport)
{
	const json original = validDoc("Round Trip");
	auto filename = exportLayoutToUsb(original, m_dir.string());
	ASSERT_TRUE(filename.has_value());

	auto candidates = findImportableLayoutsOnUsb(m_dir.string());
	ASSERT_EQ(candidates.size(), 1u);
	EXPECT_EQ(candidates[0].filename, *filename);
	EXPECT_EQ(candidates[0].name, "Round Trip");
	EXPECT_EQ(candidates[0].doc, original);
}

TEST_F(TestLayoutFileTransfer, ImportSkipsInvalidAndNonJsonFilesButKeepsValidOnes)
{
	ASSERT_TRUE(exportLayoutToUsb(validDoc("Good Layout"), m_dir.string()).has_value());

	{
		std::ofstream badJson(m_dir / "not_a_layout.json");
		badJson << R"({"schema": 1, "root": {"widget": "does_not_exist"}})";
	}
	{
		std::ofstream notJson(m_dir / "readme.txt");
		notJson << "this is not json at all";
	}

	auto candidates = findImportableLayoutsOnUsb(m_dir.string());
	ASSERT_EQ(candidates.size(), 1u);
	EXPECT_EQ(candidates[0].name, "Good Layout");
}

TEST_F(TestLayoutFileTransfer, ImportFromEmptyDirectoryFindsNothing)
{
	EXPECT_TRUE(findImportableLayoutsOnUsb(m_dir.string()).empty());
}

TEST_F(TestLayoutFileTransfer, ExportFailsGracefullyForANonExistentDirectory)
{
	auto filename = exportLayoutToUsb(validDoc("Nowhere"), (m_dir / "does_not_exist").string());
	EXPECT_FALSE(filename.has_value());
}

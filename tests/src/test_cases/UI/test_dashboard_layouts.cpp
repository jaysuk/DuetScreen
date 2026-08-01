/*
 * test_dashboard_layouts.cpp
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#include "Storage.h"
#include "UI/Layout/LayoutLoader.h"
#include "UI/Widgets/Dashboard/Dashboard.h"
#include "test_utils/UiTestSuite.h"
#include "utils/StorageHelper.h"
#include <gtest/gtest.h>

using namespace UI;

class TestDashboardLayouts : public UiTestSuite
{
};

TEST_F(TestDashboardLayouts, GetAvailableLayoutsFindsTheShippedPresets)
{
	auto layouts = Layout::getAvailableLayouts();

	auto findByFile = [&](std::string_view file) -> const Layout::LayoutInfo*
	{
		for (const auto& info : layouts)
		{
			if (info.file == file)
			{
				return &info;
			}
		}
		return nullptr;
	};

	const Layout::LayoutInfo* classic = findByFile("default.json");
	ASSERT_NE(classic, nullptr);
	EXPECT_EQ(classic->name, "Classic");

	const Layout::LayoutInfo* bigTemps = findByFile("big_temps.json");
	ASSERT_NE(bigTemps, nullptr);
	EXPECT_EQ(bigTemps->name, "Big Temps");

	const Layout::LayoutInfo* filesFirst = findByFile("files_first.json");
	ASSERT_NE(filesFirst, nullptr);
	EXPECT_EQ(filesFirst->name, "Files First");

	const Layout::LayoutInfo* minimal = findByFile("minimal.json");
	ASSERT_NE(minimal, nullptr);
	EXPECT_EQ(minimal->name, "Minimal");
}

TEST_F(TestDashboardLayouts, GetAvailableLayoutsIsSortedByFilename)
{
	auto layouts = Layout::getAvailableLayouts();
	ASSERT_GE(layouts.size(), 2u);
	for (size_t i = 1; i < layouts.size(); i++)
	{
		EXPECT_LT(layouts[i - 1].file, layouts[i].file);
	}
}

TEST_F(TestDashboardLayouts, DefaultLayoutHasToolListAndGraph)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);

	ASSERT_NE(dashboard.getToolList(), nullptr);
	ASSERT_NE(dashboard.getGraph(), nullptr);
	ASSERT_NE(dashboard.getFileView(), nullptr);
	ASSERT_NE(dashboard.getStatusView(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
	EXPECT_TRUE(dashboard.getGraph()->isValid());
	EXPECT_TRUE(dashboard.getFileView()->isValid());
	EXPECT_TRUE(dashboard.getStatusView()->isValid());
}

TEST_F(TestDashboardLayouts, ReloadSwitchesToADifferentLayout)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);
	ASSERT_NE(dashboard.getToolList(), nullptr);
	ASSERT_TRUE(dashboard.getToolList()->isValid());

	// big_temps.json has tool_list too, so this stays safe to call afterwards - it exercises the
	// "swap between two layouts that both use a widget" path without touching the old (by then
	// destroyed) tree at all.
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("big_temps.json"));
	EXPECT_TRUE(dashboard.reload());
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
}

TEST_F(TestDashboardLayouts, MinimalLayoutOmitsToolListAndGraphButKeepsTabs)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("minimal.json"));
	Dashboard dashboard("dashboard", screen);

	// Minimal deliberately has no tool_list/temperature_graph - this test only checks the widgets
	// every shipped preset has, and now that the accessors are pointer-returning, getToolList()/
	// getGraph() would simply return nullptr here rather than needing special avoidance.
	EXPECT_EQ(dashboard.getToolList(), nullptr);
	EXPECT_EQ(dashboard.getGraph(), nullptr);
	ASSERT_NE(dashboard.getFileView(), nullptr);
	ASSERT_NE(dashboard.getStatusView(), nullptr);
	EXPECT_TRUE(dashboard.getFileView()->isValid());
	EXPECT_TRUE(dashboard.getStatusView()->isValid());
}

TEST_F(TestDashboardLayouts, ClearToleratesALayoutWithNoToolListOrGraph)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("minimal.json"));
	Dashboard dashboard("dashboard", screen);

	// The real regression this guards against: clear() used to assume tool_list/graph always
	// exist, which minimal.json deliberately violates. Success is simply not crashing.
	dashboard.clear();
}

TEST_F(TestDashboardLayouts, ReloadWithUnknownFileFailsAndKeepsCurrentLayout)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);

	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("does_not_exist.json"));
	EXPECT_FALSE(dashboard.reload());

	// A failed reload() must leave the old (still fully valid) layout completely untouched.
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
}

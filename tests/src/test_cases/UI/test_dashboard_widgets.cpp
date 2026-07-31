/*
 * test_dashboard_widgets.cpp
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#include "ObjectModel/Directories.h"
#include "UI/Layout/DashboardWidgets.h"
#include "UI/Layout/WidgetRegistry.h"
#include "UI/Screens/File/FilePresenter.h"
#include "UI/Screens/File/FileView.h"
#include "test_utils/UiTestSuite.h"
#include <gtest/gtest.h>

using namespace UI;
using namespace UI::Layout;

class TestDashboardWidgets : public UiTestSuite
{
  public:
	TestDashboardWidgets() { registerDashboardWidgets(); }
};

TEST_F(TestDashboardWidgets, RegistersAllFiveDashboardWidgets)
{
	auto& registry = WidgetRegistry::get();

	for (std::string_view id : {"tool_list", "temperature_graph", "file_browser_jobs", "file_browser_macros", "status"})
	{
		EXPECT_NE(registry.find(id), nullptr) << "expected widget id '" << id << "' to be registered";
	}
}

TEST_F(TestDashboardWidgets, CallingRegisterTwiceIsIdempotent)
{
	auto& registry = WidgetRegistry::get();
	size_t before = registry.availableWidgets().size();

	registerDashboardWidgets();
	registerDashboardWidgets();

	EXPECT_EQ(registry.availableWidgets().size(), before);
}

TEST_F(TestDashboardWidgets, AllFiveDescriptorsAreSingletons)
{
	auto& registry = WidgetRegistry::get();

	for (std::string_view id : {"tool_list", "temperature_graph", "file_browser_jobs", "file_browser_macros", "status"})
	{
		const WidgetDescriptor* descriptor = registry.find(id);
		ASSERT_NE(descriptor, nullptr);
		EXPECT_TRUE(descriptor->singleton) << "expected widget id '" << id << "' to be a singleton";
	}
}

TEST_F(TestDashboardWidgets, ToolListFactoryCreatesAWidget)
{
	const WidgetDescriptor* descriptor = WidgetRegistry::get().find("tool_list");
	ASSERT_NE(descriptor, nullptr);

	std::unique_ptr<LvObj> widget = descriptor->create("instance", screen, nlohmann::json{});
	ASSERT_NE(widget, nullptr);
	EXPECT_TRUE(widget->isValid());
}

TEST_F(TestDashboardWidgets, TemperatureGraphFactoryCreatesAWidget)
{
	const WidgetDescriptor* descriptor = WidgetRegistry::get().find("temperature_graph");
	ASSERT_NE(descriptor, nullptr);

	std::unique_ptr<LvObj> widget = descriptor->create("instance", screen, nlohmann::json{});
	ASSERT_NE(widget, nullptr);
	EXPECT_TRUE(widget->isValid());
}

TEST_F(TestDashboardWidgets, StatusFactoryCreatesAWidget)
{
	const WidgetDescriptor* descriptor = WidgetRegistry::get().find("status");
	ASSERT_NE(descriptor, nullptr);

	std::unique_ptr<LvObj> widget = descriptor->create("instance", screen, nlohmann::json{});
	ASSERT_NE(widget, nullptr);
	EXPECT_TRUE(widget->isValid());
}

TEST_F(TestDashboardWidgets, FileBrowserJobsFactoryDefaultsToGcodesFolder)
{
	const WidgetDescriptor* descriptor = WidgetRegistry::get().find("file_browser_jobs");
	ASSERT_NE(descriptor, nullptr);

	std::unique_ptr<LvObj> widget = descriptor->create("instance", screen, nlohmann::json{});
	ASSERT_NE(widget, nullptr);

	auto* fileView = static_cast<FileView*>(widget.get());
	// getBaseFolderPath() strips the trailing slash GetGcodesDirectory() includes.
	EXPECT_EQ(std::string(fileView->getPresenter()->getBaseFolderPath()) + "/", OM::Directories::GetGcodesDirectory());
}

TEST_F(TestDashboardWidgets, FileBrowserMacrosFactorySetsMacrosFolder)
{
	const WidgetDescriptor* descriptor = WidgetRegistry::get().find("file_browser_macros");
	ASSERT_NE(descriptor, nullptr);

	std::unique_ptr<LvObj> widget = descriptor->create("instance", screen, nlohmann::json{});
	ASSERT_NE(widget, nullptr);

	auto* fileView = static_cast<FileView*>(widget.get());
	// getBaseFolderPath() strips the trailing slash GetMacrosDirectory() includes.
	EXPECT_EQ(std::string(fileView->getPresenter()->getBaseFolderPath()) + "/", OM::Directories::GetMacrosDirectory());
}

TEST_F(TestDashboardWidgets, AllFiveWidgetsAreAvailableByDefault)
{
	auto available = WidgetRegistry::get().availableWidgets();

	size_t matched = 0;
	for (std::string_view id : {"tool_list", "temperature_graph", "file_browser_jobs", "file_browser_macros", "status"})
	{
		bool found = false;
		for (const WidgetDescriptor* descriptor : available)
		{
			if (descriptor->id == id)
			{
				found = true;
				break;
			}
		}
		EXPECT_TRUE(found) << "expected widget id '" << id << "' to be available by default";
		matched += found ? 1 : 0;
	}
	EXPECT_EQ(matched, 5u);
}

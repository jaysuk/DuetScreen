/*
 * DashboardWidgets.cpp
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#include "DashboardWidgets.h"
#include "WidgetRegistry.h"
#include "UI/Screens/File/FileView.h"
#include "UI/Screens/Status/StatusView.h"
#include "UI/Widgets/Temperature/TemperatureGraph.h"
#include "UI/Widgets/ToolList/ToolList.h"

namespace UI::Layout
{
	namespace
	{
		bool s_registered = false;
	}

	void registerDashboardWidgets()
	{
		ZoneScoped;
		if (s_registered)
		{
			return;
		}
		s_registered = true;

		auto& registry = WidgetRegistry::get();

		registry.add({
			.id = "tool_list",
			.nameKey = "layout.widget.tool_list", // TODO: add this i18n key once a picker UI renders it (Phase 4+)
			.icon = "control.png",
			.hint = {.minCols = 2, .minRows = 1},
			.singleton = true,
			.create =
				[](const std::string&, LvObj& parent, const nlohmann::json&) -> std::unique_ptr<LvObj>
			{ return std::make_unique<ToolList>("tool_list", parent); },
		});

		registry.add({
			.id = "temperature_graph",
			.nameKey = "layout.widget.temperature_graph", // TODO: same as above
			.icon = "temperature.png",
			.hint = {.minCols = 2, .minRows = 1},
			.singleton = true,
			.create =
				[](const std::string&, LvObj& parent, const nlohmann::json&) -> std::unique_ptr<LvObj>
			{ return std::make_unique<TemperatureGraph>("graph", parent); },
		});

		// The two file_browser variants use Dashboard's existing fixed storage keys rather than
		// deriving one from `instanceId`, because FileView::StorageKeys is built from StorageKey<T>
		// (a consteval, compile-time-only string) - it cannot accept a runtime-computed key. Truly
		// generic multi-instance file browsers need FileView moved onto StorageKeyRunTime<T> first;
		// until then these are registered as two distinct singleton widgets, matching how Dashboard
		// already uses them today.
		registry.add({
			.id = "file_browser_jobs",
			.nameKey = "file.jobs",
			.icon = "folder.png",
			.hint = {.minCols = 1, .minRows = 1},
			.singleton = true,
			.create =
				[](const std::string&, LvObj& parent, const nlohmann::json&) -> std::unique_ptr<LvObj>
			{
				return std::make_unique<FileView>(
					"files",
					parent,
					FileView::StorageKeys{
						.sortBy = {"ui:dashboard:file:jobs:sort_by", OM::FileSystem::SortBy::DATE},
						.sortDescending = {"ui:dashboard:file:jobs:sort_descending", true},
						.displayMode = {"ui:dashboard:file:jobs:display_mode", FileView::DisplayMode::List}});
			},
		});

		registry.add({
			.id = "file_browser_macros",
			.nameKey = "file.macros",
			.icon = "macros.png",
			.hint = {.minCols = 1, .minRows = 1},
			.singleton = true,
			.create =
				[](const std::string&, LvObj& parent, const nlohmann::json&) -> std::unique_ptr<LvObj>
			{
				auto view = std::make_unique<FileView>(
					"macros",
					parent,
					FileView::StorageKeys{
						.sortBy = {"ui:dashboard:file:macros:sort_by", OM::FileSystem::SortBy::NAME},
						.sortDescending = {"ui:dashboard:file:macros:sort_descending", false},
						.displayMode = {"ui:dashboard:file:macros:display_mode", FileView::DisplayMode::List}});
				view->getPresenter()->setBaseFolder(FilePresenter::BaseFolder::MACROS);
				return view;
			},
		});

		registry.add({
			.id = "status",
			.nameKey = "app_drawer.status",
			.icon = "status.png",
			.hint = {.minCols = 1, .minRows = 1},
			.singleton = true,
			.create =
				[](const std::string&, LvObj& parent, const nlohmann::json&) -> std::unique_ptr<LvObj>
			{ return std::make_unique<StatusView>("status", parent); },
		});
	}
} // namespace UI::Layout

/*
 * Dashboard.h
 *
 *  Created on: 2025-10-02
 *      Author: Andy Everitt
 */

#pragma once

#include "DashboardPresenter.h"
#include "UI/Components/Containers/TabView.h"
#include "UI/Core/View.h"
#include "UI/Layout/LayoutBuilder.h"
#include "UI/Screens/File/FileView.h"
#include "UI/Screens/Status/StatusView.h"
#include "UI/Widgets/Temperature/TemperatureGraph.h"
#include "UI/Widgets/ToolList/ToolList.h"
#include <memory>

namespace UI
{
	/**
	 * @brief The dashboard's composition is data-driven - see assets/layouts/ and
	 * docs/LAYOUT_ENGINE_DESIGN.md. The user can pick a shipped preset (Settings > Display > Layout)
	 * or, from Phase 5 on, edit a layout freely (Settings > Display > Edit Layout, or long-press the
	 * dashboard) - so any widget, including the tabs container and its jobs/status tabs, may be
	 * absent from the current document. Every accessor below is therefore pointer-returning and may
	 * return nullptr; callers must check before use.
	 */
	class Dashboard : public View<DashboardPresenter>
	{
	  public:
		Dashboard(const std::string& name, LvObj& parent);

		ToolList* getToolList() { return static_cast<ToolList*>(m_layout->find("tool_list")); }
		TemperatureGraph* getGraph() { return static_cast<TemperatureGraph*>(m_layout->find("temperature_graph")); }
		TabView* getTabs() { return static_cast<TabView*>(m_layout->find("tabs")); }
		FileView* getFileView() { return static_cast<FileView*>(m_layout->find("jobs")); }
		StatusView* getStatusView() { return static_cast<StatusView*>(m_layout->find("status")); }

		void showJobsTab()
		{
			if (TabView* tabs = getTabs())
			{
				tabs->setActiveTabById("jobs");
			}
		}
		void showStatusTab()
		{
			if (TabView* tabs = getTabs())
			{
				tabs->setActiveTabById("status");
			}
		}
		void disableJobsTab(bool disable);

		void setNumberPad(ModalNumberPad* np);

		void clear();

		/**
		 * @brief Tears down the current layout and rebuilds from whichever file ID_LAYOUT_FILE
		 * currently names (see the preset picker in Settings > Display). Rebuilds wholesale rather
		 * than diffing, per docs/LAYOUT_ENGINE_DESIGN.md section 4.3.
		 *
		 * @return false (leaving the current layout in place, unlike the constructor's use of this
		 * same loading path) if the named file can't be loaded/parsed/built - unlike at startup,
		 * there's always a known-good layout already on screen to fall back to here.
		 */
		bool reload();

	  protected:
		void onHide() override;

	  private:
		// Re-applies configuration that was set via setNumberPad()/disableJobsTab() while the target
		// widget didn't exist, or that a widget rebuilt from scratch (a fresh TabView/StatusView has
		// no memory of what the previous instance was told) needs re-telling. Called at the end of
		// every rebuild (reload(), and later the layout editor's preview rebuilds).
		void applyPendingConfiguration();

		std::unique_ptr<Layout::LayoutInstance> m_layout;
		ModalNumberPad* m_pendingNumberPad = nullptr;
		bool m_jobsTabDisabled = false;
	};
} // namespace UI

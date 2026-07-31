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
	 * docs/LAYOUT_ENGINE_DESIGN.md. The user can pick from a curated set of shipped presets (Settings
	 * > Display > Layout), each fixed and known-good, but not every preset contains every widget (see
	 * assets/layouts/minimal.json). The accessors below still return concrete references rather than
	 * becoming nullable, because every widget *except* tool_list/temperature_graph is present in
	 * every preset shipped today - see the comment on m_layout for how those two are handled. Once
	 * layouts are genuinely user-*editable*, rather than a choice between fixed presets, this
	 * guarantee stops holding for everything and these will need to change.
	 */
	class Dashboard : public View<DashboardPresenter>
	{
	  public:
		Dashboard(const std::string& name, LvObj& parent);

		ToolList& getToolList() { return static_cast<ToolList&>(*m_layout->find("tool_list")); }
		TemperatureGraph& getGraph() { return static_cast<TemperatureGraph&>(*m_layout->find("temperature_graph")); }
		TabView& getTabs() { return static_cast<TabView&>(*m_layout->find("tabs")); }
		FileView& getFileView() { return static_cast<FileView&>(*m_layout->find("jobs")); }
		StatusView& getStatusView() { return static_cast<StatusView&>(*m_layout->find("status")); }

		void showJobsTab() { getTabs().setActiveTabById("jobs"); }
		void showStatusTab() { getTabs().setActiveTabById("status"); }
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
		// tool_list/temperature_graph are the only widgets a shipped preset may omit (see
		// assets/layouts/minimal.json) - clear()/onHide() are the only places that touch them
		// outside the accessors above, so they're the only places that need to tolerate absence.
		// The accessors themselves stay reference-returning (see the class comment) because every
		// *other* widget they look up is present in every preset shipped today.
		std::unique_ptr<Layout::LayoutInstance> m_layout;
	};
} // namespace UI

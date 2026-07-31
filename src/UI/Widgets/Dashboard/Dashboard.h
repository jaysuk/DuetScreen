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
	 * @brief The dashboard's composition is data-driven - see assets/layouts/default.json and
	 * docs/LAYOUT_ENGINE_DESIGN.md. The accessors below still return concrete references (rather
	 * than becoming nullable) because, for now, the default layout is fixed and known-good: every
	 * id they look up is guaranteed present by construction, the same guarantee static members gave
	 * before. That stops being true once layouts become user-editable (see the design doc's later
	 * phases), at which point these will need to change.
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

	  protected:
		void onHide() override;

	  private:
		std::unique_ptr<Layout::LayoutInstance> m_layout;
	};
} // namespace UI

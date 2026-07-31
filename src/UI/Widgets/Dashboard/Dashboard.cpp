/*
 * Dashboard.cpp
 *
 *  Created on: 2025-10-02
 *      Author: Andy Everitt
 */

#include "Dashboard.h"
#include "Debug.h"
#include "UI/Layout/DashboardWidgets.h"
#include "UI/Layout/LayoutLoader.h"

namespace UI
{
	Dashboard::Dashboard(const std::string& name, LvObj& parent)
		: View(name, parent, layout_t(0, 0, 100, 100))
	{
		ZoneScoped;
		setStylePad(0);
		// setExtDrawSize(100); /* for outer tab buttons */
		setFlag(LV_OBJ_FLAG_SCROLLABLE, false);

		Layout::registerDashboardWidgets();

		std::optional<nlohmann::json> doc = Layout::loadLayoutDocument("default.json");
		if (!doc)
		{
			LOG_FATAL_THROW("Failed to load the default dashboard layout");
			return;
		}

		m_layout = Layout::LayoutBuilder::build(*doc, *this);
		if (!m_layout)
		{
			LOG_FATAL_THROW("Failed to build the default dashboard layout");
			return;
		}

		/* Tabs (Jobs & Status) */
		getTabs().setActiveTabById("jobs");
	}

	void Dashboard::disableJobsTab(bool disable)
	{
		ZoneScoped;
		getTabs().disableTabById("jobs", disable);
	}

	void Dashboard::setNumberPad(ModalNumberPad* np)
	{
		ZoneScoped;
		getStatusView().setNumberPad(np);
	}

	void Dashboard::clear()
	{
		ZoneScoped;
		getToolList().setToolCount(0);
		getToolList().hideNumberPad();
		getGraph().clear();
	}

	void Dashboard::onHide()
	{
		ZoneScoped;
		// Keep only the temperature graph subscriptions active while hidden.
		getGraph().activate();
	}

} // namespace UI

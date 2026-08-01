/*
 * Dashboard.cpp
 *
 *  Created on: 2025-10-02
 *      Author: Andy Everitt
 */

#include "Dashboard.h"
#include "Debug.h"
#include "Storage.h"
#include "UI/Layout/DashboardWidgets.h"
#include "UI/Layout/LayoutLoader.h"
#include "utils/StorageHelper.h"

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

		if (!reload())
		{
			// Unlike a later reload() failure, there's no already-built layout to fall back to here.
			LOG_FATAL_THROW("Failed to build the initial dashboard layout");
		}
	}

	bool Dashboard::reload()
	{
		ZoneScoped;
		const std::string_view layoutFile = StorageHelper::getData(ID_LAYOUT_FILE);

		std::optional<nlohmann::json> doc = Layout::loadLayoutDocument(layoutFile);
		if (!doc)
		{
			LOG_ERROR("Failed to load dashboard layout '{:s}', keeping the current one", layoutFile);
			return false;
		}

		std::unique_ptr<Layout::LayoutInstance> newLayout = Layout::LayoutBuilder::build(*doc, *this);
		if (!newLayout)
		{
			LOG_ERROR("Failed to build dashboard layout '{:s}', keeping the current one", layoutFile);
			return false;
		}

		// Destroy the old tree before installing the new one: LayoutInstance owns its LvObjs, and
		// View<Presenter>'s destructor unbinds each widget's presenter from the Model as it goes.
		m_layout.reset();
		m_layout = std::move(newLayout);

		applyPendingConfiguration();
		return true;
	}

	void Dashboard::applyPendingConfiguration()
	{
		ZoneScoped;
		if (TabView* tabs = getTabs())
		{
			tabs->setActiveTabById("jobs");
			tabs->disableTabById("jobs", m_jobsTabDisabled);
		}
		if (StatusView* status = getStatusView())
		{
			status->setNumberPad(m_pendingNumberPad);
		}
	}

	void Dashboard::disableJobsTab(bool disable)
	{
		ZoneScoped;
		m_jobsTabDisabled = disable;
		if (TabView* tabs = getTabs())
		{
			tabs->disableTabById("jobs", disable);
		}
	}

	void Dashboard::setNumberPad(ModalNumberPad* np)
	{
		ZoneScoped;
		m_pendingNumberPad = np;
		if (StatusView* status = getStatusView())
		{
			status->setNumberPad(np);
		}
	}

	void Dashboard::clear()
	{
		ZoneScoped;
		if (LvObj* toolList = m_layout->find("tool_list"))
		{
			static_cast<ToolList*>(toolList)->setToolCount(0);
			static_cast<ToolList*>(toolList)->hideNumberPad();
		}
		if (LvObj* graph = m_layout->find("temperature_graph"))
		{
			static_cast<TemperatureGraph*>(graph)->clear();
		}
	}

	void Dashboard::onHide()
	{
		ZoneScoped;
		// Keep only the temperature graph subscriptions active while hidden.
		if (LvObj* graph = m_layout->find("temperature_graph"))
		{
			static_cast<TemperatureGraph*>(graph)->activate();
		}
	}

} // namespace UI

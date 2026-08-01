/*
 * WidgetPickerList.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "WidgetPickerList.h"
#include "UI/Components/Button/Button.h"
#include "i18n/i18n.h"
#include <fmt/format.h>

namespace UI
{
	WidgetPickerList::WidgetPickerList(const std::string& name, LvObj& parent)
		: LvContainer(name, parent)
	{
		ZoneScoped;
		setFlexFlow(LV_FLEX_FLOW_COLUMN);
		setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
		setSize(LV_PCT(70), LV_PCT(70));
	}

	void WidgetPickerList::setEntries(const std::vector<const Layout::WidgetDescriptor*>& entries)
	{
		ZoneScoped;
		UI_LOCK();
		m_buttons.clear();

		for (const Layout::WidgetDescriptor* descriptor : entries)
		{
			auto button = std::make_unique<Button>(
				fmt::format("entry_{}", descriptor->id), *this, _(descriptor->nameKey));
			button->setSize(LV_PCT(100), LV_SIZE_CONTENT);
			if (!descriptor->icon.empty())
			{
				button->setIcon(descriptor->icon);
			}

			const std::string widgetId(descriptor->id);
			button->addClickedCallback(
				[this, widgetId](lv_event_t*)
				{
					if (m_cb)
					{
						m_cb(widgetId);
					}
				});

			m_buttons.push_back(std::move(button));
		}
	}
} // namespace UI

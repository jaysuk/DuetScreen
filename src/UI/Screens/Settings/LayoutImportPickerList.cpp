/*
 * LayoutImportPickerList.cpp
 *
 *  Created on: 2026-08-02
 *      Author: Jay S
 */

#include "LayoutImportPickerList.h"
#include "UI/Components/Button/Button.h"
#include <fmt/format.h>

namespace UI
{
	LayoutImportPickerList::LayoutImportPickerList(const std::string& name, LvObj& parent)
		: LvContainer(name, parent)
	{
		ZoneScoped;
		setFlexFlow(LV_FLEX_FLOW_COLUMN);
		setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
		setSize(LV_PCT(70), LV_PCT(70));
	}

	void LayoutImportPickerList::setEntries(const std::vector<Layout::ImportCandidate>& entries)
	{
		ZoneScoped;
		UI_LOCK();
		m_buttons.clear();

		for (size_t i = 0; i < entries.size(); i++)
		{
			auto button = std::make_unique<Button>(fmt::format("entry_{}", i), *this, entries[i].name);
			button->setSize(LV_PCT(100), LV_SIZE_CONTENT);

			button->addClickedCallback(
				[this, i](lv_event_t*)
				{
					if (m_cb)
					{
						m_cb(i);
					}
				});

			m_buttons.push_back(std::move(button));
		}
	}
} // namespace UI

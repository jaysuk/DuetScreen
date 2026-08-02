/*
 * LayoutImportPickerList.h
 *
 *  Created on: 2026-08-02
 *      Author: Jay S
 */

#pragma once

#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Layout/LayoutFileTransfer.h"
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace UI
{
	/**
	 * @brief A simple scrollable list of importable layout files found on a USB stick (Settings >
	 * Display > Import from USB), wrapped as Modal<LayoutImportPickerList> - structurally identical
	 * to the dashboard layout editor's WidgetPickerList
	 * (src/UI/Widgets/Dashboard/Editor/WidgetPickerList.h). Tapping an entry fires the selected
	 * callback with that candidate's index into the vector last passed to setEntries().
	 */
	class LayoutImportPickerList : public LvContainer
	{
	  public:
		LayoutImportPickerList(const std::string& name, LvObj& parent);

		void setEntries(const std::vector<Layout::ImportCandidate>& entries);
		void setSelectedCallback(std::function<void(size_t index)> cb) { m_cb = std::move(cb); }

	  private:
		// std::deque so button addresses stay stable if setEntries() is ever called again while some
		// are already visible (matches WidgetPickerList's own reasoning).
		std::deque<std::unique_ptr<LvObj>> m_buttons;
		std::function<void(size_t)> m_cb;
	};
} // namespace UI

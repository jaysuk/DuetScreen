/*
 * WidgetPickerList.h
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#pragma once

#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Layout/WidgetRegistry.h"
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace UI
{
	/**
	 * @brief A simple scrollable list of widget entries the layout editor's "+" button opens
	 * (wrapped as Modal<WidgetPickerList>, matching ModalNumberPad/ModalMessageBox's own use of the
	 * Modal<T> template). Tapping an entry fires the selected callback with that widget's registry id.
	 */
	class WidgetPickerList : public LvContainer
	{
	  public:
		WidgetPickerList(const std::string& name, LvObj& parent);

		void setEntries(const std::vector<const Layout::WidgetDescriptor*>& entries);
		void setSelectedCallback(std::function<void(std::string_view widgetId)> cb) { m_cb = std::move(cb); }

	  private:
		// std::deque so button addresses stay stable if setEntries() is ever called again while some
		// are already visible (not currently done, but matches the module's general convention).
		std::deque<std::unique_ptr<LvObj>> m_buttons;
		std::function<void(std::string_view)> m_cb;
	};
} // namespace UI

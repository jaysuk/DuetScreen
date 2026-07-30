/*
 * WidgetRegistry.h
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#pragma once

#include "UI/Components/LVGL/LvObj.h"
#include <deque>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace UI::Layout
{
	/**
	 * @brief Minimum grid footprint a widget needs to remain usable, so a layout editor can't
	 * shrink it below a sensible size.
	 */
	struct GridHint
	{
		uint8_t minCols = 1;
		uint8_t minRows = 1;
	};

	struct WidgetDescriptor
	{
		std::string_view id;	   // Stable identifier - this is the wire format, never translate or rename it
		std::string_view nameKey; // i18n key for the display name, resolved lazily at render time
		std::string_view icon;	   // Asset name, as used by AppDrawer
		GridHint hint;
		bool singleton = false; // Whether a layout document may only ever place one instance of this widget

		/**
		 * @brief Creates the widget.
		 *
		 * @param instanceId Stable per-instance identifier, intended to derive storage key prefixes
		 * so multiple instances of the same widget type don't share settings. Not yet honoured by
		 * every widget - see FileView's factories in DashboardWidgets.cpp for the current
		 * limitation.
		 * @param parent The LvObj this widget should be created under.
		 * @param props Widget-specific configuration from the layout document.
		 */
		std::function<std::unique_ptr<LvObj>(
			const std::string& instanceId, LvObj& parent, const nlohmann::json& props)>
			create;

		/// Optional availability gate, e.g. hide a widget that needs hardware this machine lacks. Absent means always available.
		std::function<bool()> available;
	};

	/**
	 * @brief Registry of widget types that a data-driven layout can place. See
	 * docs/LAYOUT_ENGINE_DESIGN.md for the design this is part of.
	 *
	 * @note This class is deliberately independent of process-lifetime state: `get()` is the
	 * production singleton accessor, but tests should construct their own local instances rather
	 * than registering into the shared one, to avoid cross-test pollution.
	 */
	class WidgetRegistry
	{
	  public:
		/// The registry used by the running application.
		static WidgetRegistry& get();

		/// Registers a widget descriptor. Fatal if `descriptor.id` is already registered.
		void add(WidgetDescriptor descriptor);

		/// Returns the descriptor for `id`, or nullptr if no widget is registered under that id.
		const WidgetDescriptor* find(std::string_view id) const;

		/// Returns every registered widget whose available() (if set) currently returns true.
		std::vector<const WidgetDescriptor*> availableWidgets() const;

	  private:
		// std::deque so that pointers returned by find()/availableWidgets() stay valid across
		// later add() calls - a std::vector could reallocate and invalidate them.
		std::deque<WidgetDescriptor> m_descriptors;
	};
} // namespace UI::Layout

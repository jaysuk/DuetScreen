/*
 * DashboardWidgets.h
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#pragma once

namespace UI::Layout
{
	/**
	 * @brief Registers WidgetRegistry descriptors for DuetScreen's current dashboard widgets (tool
	 * list, temperature graph, file browsers, status). Safe to call more than once.
	 *
	 * @note This does not yet change what Dashboard actually builds - see
	 * docs/LAYOUT_ENGINE_DESIGN.md Phase 3 for when the registry becomes the real construction
	 * path.
	 */
	void registerDashboardWidgets();
} // namespace UI::Layout

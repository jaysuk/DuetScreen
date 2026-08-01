/*
 * LayoutDocumentEditor.h
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#pragma once

#include "UI/Layout/WidgetRegistry.h"
#include <nlohmann/json.hpp>
#include <string_view>

namespace UI::Layout
{
	struct EditResult
	{
		bool ok = true;
		std::string error;
	};

	/**
	 * @brief Pure JSON-document mutations for the on-screen layout editor (Phase 5). None of these
	 * touch LVGL or build anything - callers are expected to re-validate the result with
	 * LayoutBuilder::validate() before treating it as the new current document, and to rebuild with
	 * LayoutBuilder::build() to preview it. Every function here leaves `doc` completely untouched if
	 * it returns a failing EditResult, matching LayoutBuilder::build()'s "never partially build a bad
	 * document" rule (docs/LAYOUT_ENGINE_DESIGN.md section 4.3), applied here to mutation instead.
	 *
	 * Node addressing uses JSON Pointer strings (e.g. "/root/children/0/children/1") pointing at the
	 * node itself. Every addressable node other than the document root lives at some
	 * ".../children/N" position, which these functions rely on to find the node's parent and index.
	 */

	/// Moves the widget at `widgetPath` to (col, row) within its parent grid. If another widget's own
	/// (col, row) anchor exactly matches the target, the two widgets swap position *and* size
	/// (col/row/colSpan/rowSpan) wholesale - this is the "drag onto another cell to move or swap it"
	/// behaviour from docs/LAYOUT_ENGINE_DESIGN.md section 5.2. Fails if `widgetPath`'s parent isn't a
	/// grid container, if the target only partially overlaps another widget's span (ambiguous - not a
	/// clean swap), or if it would fall outside the grid's own track counts.
	EditResult moveWidget(nlohmann::json& doc, std::string_view widgetPath, int col, int row);

	/// Resizes the widget at `widgetPath` to `colSpan` x `rowSpan`. Fails (does not clamp) if the
	/// requested span is below the widget's registered GridHint minimums, would overlap another
	/// child, or would extend outside the grid's track counts.
	EditResult resizeWidget(nlohmann::json& doc, std::string_view widgetPath, int colSpan, int rowSpan,
							 const WidgetRegistry& registry = WidgetRegistry::get());

	/// Removes the node at `path` from its parent's children array. Works for a widget or an entire
	/// container node, under any container type (grid/row/column/tabs).
	EditResult removeWidget(nlohmann::json& doc, std::string_view path);

	/// Adds a new widget node at (col, row) as a child of the grid container at `gridPath`. Fails if
	/// `gridPath` isn't a grid container, `widgetId` isn't registered, the cell is already occupied, or
	/// (for a singleton widget) one is already placed anywhere in the document.
	EditResult addWidget(nlohmann::json& doc, std::string_view gridPath, int col, int row,
						  std::string_view widgetId, const WidgetRegistry& registry = WidgetRegistry::get());

	/// Appends a new track (a column if `isColumn`, else a row) to the grid at `gridPath` with the
	/// given size token (e.g. "1fr"). Always appends at the end - never renumbers existing placements.
	EditResult addTrack(nlohmann::json& doc, std::string_view gridPath, bool isColumn, std::string_view sizeToken);

	/// Removes track `index` (a column if `isColumn`, else a row) from the grid at `gridPath`. Fails
	/// if any child currently occupies that track, or if it would leave the grid with zero tracks.
	/// Renumbers any child placed after the removed track.
	EditResult removeTrack(nlohmann::json& doc, std::string_view gridPath, bool isColumn, int index);

	/// Replaces the size token of track `index` (a column if `isColumn`, else a row) of the grid at
	/// `gridPath`.
	EditResult setTrackSize(nlohmann::json& doc, std::string_view gridPath, bool isColumn, int index,
							std::string_view sizeToken);
} // namespace UI::Layout

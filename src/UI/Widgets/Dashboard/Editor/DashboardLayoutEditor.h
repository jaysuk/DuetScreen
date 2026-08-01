/*
 * DashboardLayoutEditor.h
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#pragma once

#include "GridTracksEditor.h"
#include "UI/Components/Button/Button.h"
#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Components/MessageBox/ModalMessageBox.h"
#include "UI/Components/Modal/Modal.h"
#include "UI/Layout/LayoutDocumentEditor.h"
#include "WidgetPickerList.h"
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace UI
{
	class Dashboard;

	/**
	 * @brief The on-screen dashboard layout editor (Phase 5, docs/LAYOUT_ENGINE_DESIGN.md section
	 * 5.2). Entered via Dashboard's long-press or Settings > Display > Edit Layout.
	 *
	 * Holds a working copy of the current layout document (seeded from
	 * Dashboard::getCurrentDocument() on entry) and mutates it via UI::Layout's pure
	 * moveWidget/resizeWidget/removeWidget/addWidget/addTrack/removeTrack/setTrackSize functions.
	 * Every mutation is followed by LayoutBuilder::validate() and, if it still passes,
	 * Dashboard::previewDocument() - which rebuilds the *real* LayoutInstance from the edited
	 * document, matching docs/LAYOUT_ENGINE_DESIGN.md section 4.3's "rebuild wholesale, only on
	 * explicit commit" rule (a drag/resize *gesture* only moves a lightweight floating ghost -
	 * nothing is rebuilt until the gesture ends). After every successful rebuild, edit-mode chrome
	 * (drag handle + resize handle + remove button on each grid widget; remove button only on a
	 * tabs-container widget, since drag/resize are grid-specific; a "+" add-widget button in every
	 * empty grid cell) is attached directly to the freshly-built widgets as floating children, so
	 * their position tracks LVGL's own layout with no manual pixel bookkeeping.
	 *
	 * Grid *structure* (adding/removing/resizing tracks) doesn't lend itself to the same per-widget
	 * chrome - it's edited through a separate compact list (GridTracksEditor), opened from the
	 * toolbar, rather than in-place per-track handles.
	 */
	class DashboardLayoutEditor : public LvContainer
	{
	  public:
		DashboardLayoutEditor(const std::string& name, LvObj& parent, Dashboard& dashboard);

		/// Seeds the working document from the dashboard's current one, shows the toolbar, and
		/// attaches edit-mode chrome to the live tree. No-op if already active.
		void enter();

		/// Discards the working document, rebuilds the dashboard from whatever's actually persisted
		/// (Dashboard::reload()), and hides the toolbar/chrome. No-op if not active.
		void exit();

		bool isActive() const { return m_active; }

	  private:
		// Tears down and re-creates every chrome object against the *current* built tree - called
		// after every successful preview rebuild, since the rebuild destroyed the previous tree's
		// widgets (and with them, their chrome children).
		void rebuildChrome();
		void addChromeForNode(const nlohmann::json& node, const std::string& path, bool parentIsGrid);
		void addChromeForGridChild(LvObj& widget, const nlohmann::json& node, const std::string& path);
		void addEmptyCellButton(LvObj& grid, const std::string& gridPath, int col, int row);
		void clearChrome();

		// Applies `result` if ok: re-validates, previews, rebuilds chrome. Reverts (shows a brief
		// error, chrome/document left exactly as they were) if `result` itself failed or the
		// resulting document doesn't validate/build - defence in depth, since the gesture code above
		// this is expected to only ever construct requests that should succeed.
		void applyMutation(Layout::EditResult result);

		void openAddWidgetPicker(const std::string& gridPath, int col, int row);
		void openGridTracksEditor(const std::string& gridPath);

		void onSave();
		void onCancel();
		void onResetToDefault();

		// Drag-to-move-or-swap tracking, started from a widget's drag-handle button.
		struct DragState
		{
			std::string widgetPath;
			LvObj* ghost = nullptr;
			int32_t startPointerX = 0;
			int32_t startPointerY = 0;
			int32_t widgetStartX = 0;
			int32_t widgetStartY = 0;
			int startCol = 0;
			int startRow = 0;
		};
		void beginDrag(const std::string& widgetPath, LvObj& widget);
		void updateDrag(int32_t pointerX, int32_t pointerY);
		void endDrag(int32_t pointerX, int32_t pointerY);

		// Resize tracking, started from a widget's resize-handle button. Cell-size is *approximated*
		// from the widget's own current size / its own current span for live visual feedback while
		// dragging - only the final whole-cell colSpan/rowSpan chosen on release is ever committed
		// (via resizeWidget, which is exact), so the approximation only affects how the ghost/handle
		// tracks the finger mid-gesture, never the result.
		struct ResizeState
		{
			std::string widgetPath;
			int32_t startPointerX = 0;
			int32_t startPointerY = 0;
			int32_t startWidth = 0;
			int32_t startHeight = 0;
			int startColSpan = 1;
			int startRowSpan = 1;
			int32_t cellPxX = 1;
			int32_t cellPxY = 1;
		};
		void beginResize(const std::string& widgetPath, LvObj& widget);
		void updateResize(int32_t pointerX, int32_t pointerY);
		void endResize(int32_t pointerX, int32_t pointerY);

		Dashboard& m_dashboard;
		nlohmann::json m_workingDoc;
		bool m_active = false;

		LvContainer m_toolbar;
		Button m_saveBtn;
		Button m_cancelBtn;
		Button m_resetBtn;
		Button m_gridBtn;

		// Long-lived (constructed once, re-populated and re-opened as needed) rather than heap-
		// allocated per use - matches how ModalNumberPad/ModalMessageBox are owned elsewhere in the
		// app (e.g. HomeView's m_numberpad), and avoids a leak from an ephemeral modal with no owner.
		Modal<WidgetPickerList> m_widgetPicker;
		Modal<GridTracksEditor> m_gridTracksEditor;
		ModalMessageBox m_errorBox;

		// Every chrome object created since the last rebuildChrome() - std::deque so addresses stay
		// stable while more are appended (matches LayoutInstance::m_owned's own reasoning).
		std::deque<std::unique_ptr<LvObj>> m_chrome;

		// Every grid-cell drop target currently on screen: either a placed widget (hit-testing it
		// means "swap with this widget", using its own anchor col/row) or an empty-cell "+" button
		// (hit-testing it means "move here"). Populated fresh in rebuildChrome(), read by endDrag() to
		// turn a release point into a (col, row) without recomputing LVGL's own track layout.
		std::vector<std::tuple<LvObj*, int, int>> m_gridCellTargets;

		std::optional<DragState> m_drag;
		std::optional<ResizeState> m_resize;
	};
} // namespace UI

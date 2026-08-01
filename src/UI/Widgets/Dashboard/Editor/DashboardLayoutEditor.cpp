/*
 * DashboardLayoutEditor.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "DashboardLayoutEditor.h"
#include "Debug.h"
#include "Storage.h"
#include "Subscribers/ResponseSubscribers.h"
#include "UI/Layout/LayoutLoader.h"
#include "UI/Screens/Home/HomeView.h"
#include "UI/Styles/Styles.h"
#include "UI/Widgets/Dashboard/Dashboard.h"
#include "i18n/i18n.h"
#include "utils/StorageHelper.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>
#include <set>

namespace UI
{
	namespace
	{
		std::string childPath(const std::string& parentPath, size_t index)
		{
			return fmt::format("{}/children/{}", parentPath, index);
		}
	} // namespace

	DashboardLayoutEditor::DashboardLayoutEditor(const std::string& name, LvObj& parent, Dashboard& dashboard)
		: LvContainer(name, parent, layout_t(0, 0, 100, 100))
		, m_dashboard(dashboard)
		, m_toolbar("toolbar", *this)
		, m_saveBtn("save", m_toolbar, _("common.save"))
		, m_cancelBtn("cancel", m_toolbar, _("common.cancel"))
		, m_resetBtn("reset", m_toolbar, _("layout_editor.reset_to_default"))
		, m_gridBtn("grid", m_toolbar, _("layout_editor.grid"))
		, m_widgetPicker("widget_picker", *this)
		, m_gridTracksEditor("grid_tracks", *this)
	{
		ZoneScoped;
		// This container only exists to host the toolbar and the drag ghost - every other piece of
		// chrome (drag/resize/remove buttons, "+" add-cell buttons) is attached directly to the real
		// widgets as floating children, so it tracks LVGL's own layout with no manual bookkeeping.
		// Clicks must reach the dashboard underneath everywhere except the toolbar itself.
		setFlag(LV_OBJ_FLAG_FLOATING, true);
		setFlag(LV_OBJ_FLAG_SCROLLABLE, false);
		setFlag(LV_OBJ_FLAG_CLICKABLE, false);
		setStyleBgOpa(LV_OPA_TRANSP);

		m_toolbar.setAlign(LV_ALIGN_BOTTOM_MID, 0, 0);
		m_toolbar.setFlexFlow(LV_FLEX_FLOW_ROW);
		m_toolbar.setSize(LV_SIZE_CONTENT, LV_SIZE_CONTENT);
		m_toolbar.addStyle(Themes::getLvglStyles().card);

		m_saveBtn.addClickedCallback([this](lv_event_t*) { onSave(); });
		m_cancelBtn.addClickedCallback([this](lv_event_t*) { onCancel(); });
		m_resetBtn.addClickedCallback([this](lv_event_t*) { onResetToDefault(); });
		m_gridBtn.addClickedCallback(
			[this](lv_event_t*)
			{
				if (m_workingDoc.contains("root") && m_workingDoc["root"].value("type", std::string()) == "grid")
				{
					openGridTracksEditor("/root");
				}
			});

		hide();
	}

	void DashboardLayoutEditor::enter()
	{
		ZoneScoped;
		if (m_active)
		{
			return;
		}
		m_active = true;
		m_workingDoc = m_dashboard.getCurrentDocument();
		show();
		rebuildChrome();
	}

	void DashboardLayoutEditor::exit()
	{
		ZoneScoped;
		if (!m_active)
		{
			return;
		}
		m_active = false;
		clearChrome();
		hide();
		m_dashboard.reload();
	}

	void DashboardLayoutEditor::clearChrome()
	{
		ZoneScoped;
		m_drag.reset();
		m_resize.reset();
		// Destroying each wrapper removes its underlying LVGL object (a floating child of whichever
		// real widget it was attached to) - the widgets themselves are untouched.
		m_chrome.clear();
		m_gridCellTargets.clear();
	}

	void DashboardLayoutEditor::rebuildChrome()
	{
		ZoneScoped;
		clearChrome();
		if (!m_workingDoc.contains("root"))
		{
			return;
		}
		addChromeForNode(m_workingDoc["root"], "/root", false);
	}

	void DashboardLayoutEditor::addChromeForNode(const nlohmann::json& node, const std::string& path, bool parentIsGrid)
	{
		ZoneScoped;
		if (!node.is_object() || !node.contains("id") || !node["id"].is_string())
		{
			return;
		}
		LvObj* obj = m_dashboard.findWidget(node["id"].get<std::string>());
		if (obj == nullptr)
		{
			return;
		}

		if (node.contains("widget"))
		{
			if (parentIsGrid)
			{
				addChromeForGridChild(*obj, node, path);
			}
			else
			{
				// A widget inside a tabs/row/column container: removable, but drag/resize are
				// grid-cell concepts that don't apply here (see the class comment).
				auto removeBtn = std::make_unique<Button>(fmt::format("{}_remove", getName()), *obj);
				removeBtn->setFlag(LV_OBJ_FLAG_FLOATING, true);
				removeBtn->setAlign(LV_ALIGN_TOP_RIGHT, 0, 0);
				removeBtn->setIcon("close.png");
				removeBtn->setSize(28, 28);
				const std::string capturedPath = path;
				removeBtn->addClickedCallback([this, capturedPath](lv_event_t*)
											   { applyMutation(Layout::removeWidget(m_workingDoc, capturedPath)); });
				m_chrome.push_back(std::move(removeBtn));
			}
			return;
		}

		if (!node.contains("type"))
		{
			return;
		}
		const std::string type = node.value("type", std::string());
		const bool isGridType = (type == "grid");

		if (parentIsGrid)
		{
			// A container placed as a grid cell (e.g. the "tabs" container in default.json) is
			// removable as a unit, same as a widget would be - just without drag/resize, which this
			// module only supports for widgets (see LayoutDocumentEditor::moveWidget/resizeWidget).
			auto removeBtn = std::make_unique<Button>(fmt::format("{}_remove", getName()), *obj);
			removeBtn->setFlag(LV_OBJ_FLAG_FLOATING, true);
			removeBtn->setAlign(LV_ALIGN_TOP_RIGHT, 0, 0);
			removeBtn->setIcon("close.png");
			removeBtn->setSize(28, 28);
			const std::string capturedPath = path;
			removeBtn->addClickedCallback([this, capturedPath](lv_event_t*)
										   { applyMutation(Layout::removeWidget(m_workingDoc, capturedPath)); });
			m_chrome.push_back(std::move(removeBtn));
		}

		if (!node.contains("children") || !node["children"].is_array())
		{
			return;
		}
		const auto& children = node["children"];
		for (size_t i = 0; i < children.size(); i++)
		{
			addChromeForNode(children[i], childPath(path, i), isGridType);
		}

		if (isGridType)
		{
			const int64_t colCount = node.contains("cols") && node["cols"].is_array()
										 ? static_cast<int64_t>(node["cols"].size())
										 : 0;
			const int64_t rowCount = node.contains("rows") && node["rows"].is_array()
										 ? static_cast<int64_t>(node["rows"].size())
										 : 0;

			std::set<std::pair<int64_t, int64_t>> occupied;
			for (const auto& child : children)
			{
				if (!child.is_object() || !child.contains("col") || !child.contains("row"))
				{
					continue;
				}
				const int64_t col = child["col"].get<int64_t>();
				const int64_t row = child["row"].get<int64_t>();
				const int64_t colSpan = child.value("colSpan", 1);
				const int64_t rowSpan = child.value("rowSpan", 1);
				for (int64_t c = col; c < col + colSpan; c++)
				{
					for (int64_t r = row; r < row + rowSpan; r++)
					{
						occupied.insert({c, r});
					}
				}
			}

			for (int64_t c = 0; c < colCount; c++)
			{
				for (int64_t r = 0; r < rowCount; r++)
				{
					if (!occupied.count({c, r}))
					{
						addEmptyCellButton(*obj, path, static_cast<int>(c), static_cast<int>(r));
					}
				}
			}
		}
	}

	void DashboardLayoutEditor::addChromeForGridChild(LvObj& widget, const nlohmann::json& node, const std::string& path)
	{
		ZoneScoped;
		widget.addStyle(Themes::getLvglStyles().border_highlight);
		m_gridCellTargets.emplace_back(&widget, node.value("col", 0), node.value("row", 0));

		auto dragBtn = std::make_unique<Button>(fmt::format("{}_drag", getName()), widget);
		dragBtn->setFlag(LV_OBJ_FLAG_FLOATING, true);
		dragBtn->setAlign(LV_ALIGN_TOP_LEFT, 0, 0);
		dragBtn->setIcon("move.png");
		dragBtn->setSize(28, 28);
		const std::string widgetPath = path;
		Button* dragBtnPtr = dragBtn.get();
		dragBtnPtr->addEventCallback(
			[this, widgetPath, dragBtnPtr](lv_event_t* e)
			{
				const lv_event_code_t code = lv_event_get_code(e);
				lv_point_t point{};
				lv_indev_get_point(lv_indev_active(), &point);
				if (code == LV_EVENT_PRESSED)
				{
					beginDrag(widgetPath, *dragBtnPtr->getParent());
				}
				else if (code == LV_EVENT_PRESSING)
				{
					updateDrag(point.x, point.y);
				}
				else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
				{
					endDrag(point.x, point.y);
				}
			},
			LV_EVENT_ALL);

		auto removeBtn = std::make_unique<Button>(fmt::format("{}_remove", getName()), widget);
		removeBtn->setFlag(LV_OBJ_FLAG_FLOATING, true);
		removeBtn->setAlign(LV_ALIGN_TOP_RIGHT, 0, 0);
		removeBtn->setIcon("close.png");
		removeBtn->setSize(28, 28);
		removeBtn->addClickedCallback([this, widgetPath](lv_event_t*)
									  { applyMutation(Layout::removeWidget(m_workingDoc, widgetPath)); });

		auto resizeBtn = std::make_unique<Button>(fmt::format("{}_resize", getName()), widget);
		resizeBtn->setFlag(LV_OBJ_FLAG_FLOATING, true);
		resizeBtn->setAlign(LV_ALIGN_BOTTOM_RIGHT, 0, 0);
		resizeBtn->setIcon("move.png");
		resizeBtn->setSize(28, 28);
		Button* resizeBtnPtr = resizeBtn.get();
		resizeBtnPtr->addEventCallback(
			[this, widgetPath, resizeBtnPtr](lv_event_t* e)
			{
				const lv_event_code_t code = lv_event_get_code(e);
				lv_point_t point{};
				lv_indev_get_point(lv_indev_active(), &point);
				if (code == LV_EVENT_PRESSED)
				{
					beginResize(widgetPath, *resizeBtnPtr->getParent());
				}
				else if (code == LV_EVENT_PRESSING)
				{
					updateResize(point.x, point.y);
				}
				else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
				{
					endResize(point.x, point.y);
				}
			},
			LV_EVENT_ALL);

		m_chrome.push_back(std::move(dragBtn));
		m_chrome.push_back(std::move(removeBtn));
		m_chrome.push_back(std::move(resizeBtn));
	}

	void DashboardLayoutEditor::addEmptyCellButton(LvObj& grid, const std::string& gridPath, int col, int row)
	{
		ZoneScoped;
		auto button =
			std::make_unique<Button>(fmt::format("{}_add_{}_{}", getName(), col, row), grid, _("layout_editor.add"));
		Button* raw = button.get();
		// A real grid child (not a floating overlay) at exactly this cell, so LVGL's own track
		// resolution positions it correctly - the same mechanism LayoutBuilder uses for real widgets,
		// which is also why its resolved getCoords() is safe to hit-test against in endDrag().
		grid.setGridCell(*raw, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
		raw->addClickedCallback([this, gridPath, col, row](lv_event_t*) { openAddWidgetPicker(gridPath, col, row); });
		m_gridCellTargets.emplace_back(raw, col, row);
		m_chrome.push_back(std::move(button));
	}

	void DashboardLayoutEditor::applyMutation(Layout::EditResult result)
	{
		ZoneScoped;
		if (!result.ok)
		{
			LOG_WARN("Layout edit rejected: {:s}", result.error);
			return;
		}

		auto validation = Layout::LayoutBuilder::validate(m_workingDoc);
		if (!validation.ok || !m_dashboard.previewDocument(m_workingDoc))
		{
			LOG_ERROR("Layout edit produced an invalid document, reverting: {:s}",
					  validation.errors.empty() ? "build failed" : validation.errors[0]);
			m_workingDoc = m_dashboard.getCurrentDocument();
			rebuildChrome();
			return;
		}

		rebuildChrome();
	}

	void DashboardLayoutEditor::openAddWidgetPicker(const std::string& gridPath, int col, int row)
	{
		ZoneScoped;
		std::vector<const Layout::WidgetDescriptor*> available;
		for (const Layout::WidgetDescriptor* descriptor : Layout::WidgetRegistry::get().availableWidgets())
		{
			if (descriptor->singleton && m_workingDoc.contains("root"))
			{
				bool placed = false;
				std::function<void(const nlohmann::json&)> walk = [&](const nlohmann::json& n)
				{
					if (!n.is_object())
					{
						return;
					}
					if (n.value("widget", std::string()) == descriptor->id)
					{
						placed = true;
					}
					if (n.contains("children") && n["children"].is_array())
					{
						for (const auto& c : n["children"])
						{
							walk(c);
						}
					}
				};
				walk(m_workingDoc["root"]);
				if (placed)
				{
					continue;
				}
			}
			available.push_back(descriptor);
		}
		m_widgetPicker.setEntries(available);
		m_widgetPicker.setSelectedCallback(
			[this, gridPath, col, row](std::string_view widgetId)
			{
				applyMutation(Layout::addWidget(m_workingDoc, gridPath, col, row, widgetId));
				m_widgetPicker.close();
			});
		m_widgetPicker.open();
	}

	void DashboardLayoutEditor::openGridTracksEditor(const std::string& gridPath)
	{
		ZoneScoped;
		if (!m_workingDoc.contains("root"))
		{
			return;
		}

		auto snapshot = [this, gridPath]()
		{
			const nlohmann::json& grid = m_workingDoc["root"]; // gridPath is always "/root" - see enter()
			auto readTracks = [&](const char* key, const char* posKey, const char* spanKey)
			{
				std::vector<GridTracksEditor::TrackRow> tracks;
				if (!grid.contains(key) || !grid[key].is_array())
				{
					return tracks;
				}
				for (size_t i = 0; i < grid[key].size(); i++)
				{
					GridTracksEditor::TrackRow row;
					row.sizeToken = grid[key][i].get<std::string>();
					if (grid.contains("children"))
					{
						for (const auto& child : grid["children"])
						{
							if (!child.is_object() || !child.contains(posKey))
							{
								continue;
							}
							const int64_t pos = child[posKey].get<int64_t>();
							const int64_t span = child.value(spanKey, 1);
							if (pos <= static_cast<int64_t>(i) && static_cast<int64_t>(i) < pos + span)
							{
								row.occupied = true;
							}
						}
					}
					tracks.push_back(row);
				}
				return tracks;
			};
			m_gridTracksEditor.setTracks(readTracks("cols", "col", "colSpan"), readTracks("rows", "row", "rowSpan"));
		};
		snapshot();

		m_gridTracksEditor.setCycleSizeCallback(
			[this, gridPath, snapshot](bool isColumn, int index)
			{
				static constexpr std::array<std::string_view, 4> kTokens = {"1fr", "2fr", "3fr", "content"};
				const char* key = isColumn ? "cols" : "rows";
				const std::string current = m_workingDoc["root"][key][static_cast<size_t>(index)].get<std::string>();
				size_t next = 0;
				for (size_t i = 0; i < kTokens.size(); i++)
				{
					if (kTokens[i] == current)
					{
						next = (i + 1) % kTokens.size();
						break;
					}
				}
				applyMutation(Layout::setTrackSize(m_workingDoc, gridPath, isColumn, index, kTokens[next]));
				snapshot();
			});
		m_gridTracksEditor.setRemoveCallback(
			[this, gridPath, snapshot](bool isColumn, int index)
			{
				applyMutation(Layout::removeTrack(m_workingDoc, gridPath, isColumn, index));
				snapshot();
			});
		m_gridTracksEditor.setAddCallback(
			[this, gridPath, snapshot](bool isColumn)
			{
				applyMutation(Layout::addTrack(m_workingDoc, gridPath, isColumn, "1fr"));
				snapshot();
			});

		m_gridTracksEditor.open();
	}

	void DashboardLayoutEditor::onSave()
	{
		ZoneScoped;
		auto validation = Layout::LayoutBuilder::validate(m_workingDoc);
		if (!validation.ok)
		{
			// Reuses the app's existing transient-notification mechanism (same one behind the
			// Response/SuccessResponse/WarningResponse/ErrorResponse tests) rather than a modal
			// dialog - simpler, and this path should be unreachable in practice anyway, since every
			// mutation that reaches m_workingDoc already went through applyMutation()'s own
			// validate-or-revert check.
			HomeView::instance().getPresenter()->newResponse(
				ResponseType::ERROR, validation.errors.empty() ? _("layout_editor.save_failed_title") : validation.errors[0]);
			return;
		}

		Layout::saveCustomLayoutDocument(m_workingDoc);
		StorageHelper::setData(ID_LAYOUT_FILE, Layout::CUSTOM_LAYOUT_SENTINEL);
		exit();
	}

	void DashboardLayoutEditor::onCancel()
	{
		ZoneScoped;
		exit();
	}

	void DashboardLayoutEditor::onResetToDefault()
	{
		ZoneScoped;
		auto doc = Layout::loadLayoutDocument("default.json");
		if (!doc)
		{
			return;
		}
		m_workingDoc = *doc;
		m_dashboard.previewDocument(m_workingDoc);
		rebuildChrome();
	}

	void DashboardLayoutEditor::beginDrag(const std::string& widgetPath, LvObj& widget)
	{
		ZoneScoped;
		DragState state;
		state.widgetPath = widgetPath;
		lv_point_t point{};
		lv_indev_get_point(lv_indev_active(), &point);
		state.startPointerX = point.x;
		state.startPointerY = point.y;
		const lv_area_t area = widget.getCoords();
		state.widgetStartX = area.x1;
		state.widgetStartY = area.y1;

		try
		{
			const nlohmann::json& node = m_workingDoc.at(nlohmann::json::json_pointer(widgetPath));
			state.startCol = node.value("col", 0);
			state.startRow = node.value("row", 0);
		}
		catch (const std::exception&)
		{
			return; // widgetPath doesn't resolve - leave m_drag unset, later events become no-ops
		}

		auto ghost = std::make_unique<LvContainer>("drag_ghost", *this);
		ghost->setFlag(LV_OBJ_FLAG_FLOATING, true);
		ghost->setFlag(LV_OBJ_FLAG_CLICKABLE, false);
		ghost->setSize(area.x2 - area.x1, area.y2 - area.y1);
		ghost->setPos(state.widgetStartX, state.widgetStartY);
		ghost->addStyle(Themes::getLvglStyles().border_highlight);
		ghost->setStyleBgOpa(LV_OPA_30);
		state.ghost = ghost.get();
		m_chrome.push_back(std::move(ghost));

		m_drag = state;
	}

	void DashboardLayoutEditor::updateDrag(int32_t pointerX, int32_t pointerY)
	{
		ZoneScoped;
		if (!m_drag || m_drag->ghost == nullptr)
		{
			return;
		}
		const int32_t dx = pointerX - m_drag->startPointerX;
		const int32_t dy = pointerY - m_drag->startPointerY;
		m_drag->ghost->setPos(m_drag->widgetStartX + dx, m_drag->widgetStartY + dy);
	}

	void DashboardLayoutEditor::endDrag(int32_t pointerX, int32_t pointerY)
	{
		ZoneScoped;
		if (!m_drag)
		{
			return;
		}
		const std::string widgetPath = m_drag->widgetPath;
		const int startCol = m_drag->startCol;
		const int startRow = m_drag->startRow;
		m_drag.reset(); // drop the ghost (owned by m_chrome, cleared below via rebuildChrome/applyMutation)

		// Hit-test the release point against every other grid-cell target: every placed widget's own
		// anchor cell, and every empty-cell "+" button. Both are real children of their grid
		// (setGridCell), so their resolved getCoords() already reflects LVGL's own track math - no
		// manual pixel bookkeeping needed here. The dragged widget's own starting cell is skipped -
		// dropping back onto yourself is a no-op, not a swap-with-yourself.
		int targetCol = -1, targetRow = -1;
		bool found = false;
		for (const auto& [raw, col, row] : m_gridCellTargets)
		{
			if (col == startCol && row == startRow)
			{
				continue;
			}
			const lv_area_t area = raw->getCoords();
			if (pointerX >= area.x1 && pointerX <= area.x2 && pointerY >= area.y1 && pointerY <= area.y2)
			{
				targetCol = col;
				targetRow = row;
				found = true;
				break;
			}
		}

		if (found)
		{
			applyMutation(Layout::moveWidget(m_workingDoc, widgetPath, targetCol, targetRow));
		}
		else
		{
			rebuildChrome(); // no valid target - drop the ghost and leave the document unchanged
		}
	}

	void DashboardLayoutEditor::beginResize(const std::string& widgetPath, LvObj& widget)
	{
		ZoneScoped;
		ResizeState state;
		state.widgetPath = widgetPath;
		lv_point_t point{};
		lv_indev_get_point(lv_indev_active(), &point);
		state.startPointerX = point.x;
		state.startPointerY = point.y;
		const lv_area_t area = widget.getCoords();
		state.startWidth = area.x2 - area.x1;
		state.startHeight = area.y2 - area.y1;

		// Read the widget's current span straight out of the working document (the same path used to
		// commit the resize), so the pixel-per-cell approximation used for live feedback matches what
		// resizeWidget() will actually be asked to change.
		nlohmann::json* node = nullptr;
		try
		{
			node = &m_workingDoc.at(nlohmann::json::json_pointer(widgetPath));
		}
		catch (const std::exception&)
		{
			return;
		}
		state.startColSpan = node->value("colSpan", 1);
		state.startRowSpan = node->value("rowSpan", 1);
		state.cellPxX = std::max(1, state.startWidth / std::max(1, state.startColSpan));
		state.cellPxY = std::max(1, state.startHeight / std::max(1, state.startRowSpan));

		m_resize = state;
	}

	void DashboardLayoutEditor::updateResize(int32_t /*pointerX*/, int32_t /*pointerY*/)
	{
		// Live visual feedback during the gesture is intentionally not rendered (no separate resize
		// ghost) - the resize-handle button itself, being a floating child of the widget, stays where
		// it was drawn since the widget doesn't resize until the gesture commits. This keeps the
		// interaction simple: the user drags, releases, and sees the real (snapped-to-whole-cells)
		// result immediately via the rebuild in endResize().
	}

	void DashboardLayoutEditor::endResize(int32_t pointerX, int32_t pointerY)
	{
		ZoneScoped;
		if (!m_resize)
		{
			return;
		}
		const int32_t dx = pointerX - m_resize->startPointerX;
		const int32_t dy = pointerY - m_resize->startPointerY;
		const int colDelta = static_cast<int>(std::lround(static_cast<double>(dx) / m_resize->cellPxX));
		const int rowDelta = static_cast<int>(std::lround(static_cast<double>(dy) / m_resize->cellPxY));
		const int newColSpan = std::max(1, m_resize->startColSpan + colDelta);
		const int newRowSpan = std::max(1, m_resize->startRowSpan + rowDelta);
		const std::string widgetPath = m_resize->widgetPath;
		m_resize.reset();

		applyMutation(Layout::resizeWidget(m_workingDoc, widgetPath, newColSpan, newRowSpan));
	}
} // namespace UI

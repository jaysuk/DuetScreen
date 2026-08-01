/*
 * LayoutDocumentEditor.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "LayoutDocumentEditor.h"
#include "UI/Layout/LayoutBuilder.h"
#include <charconv>
#include <fmt/format.h>
#include <map>
#include <optional>

namespace UI::Layout
{
	namespace
	{
		using json_pointer = nlohmann::json::json_pointer;

		/// A node's parent container pointer + its index within that parent's "children" array.
		struct ChildRef
		{
			json_pointer parentPointer;
			size_t index;
		};

		/// `path` must look like ".../children/N" - every node other than the document root lives at
		/// such a position. Returns std::nullopt if `path` isn't shaped that way.
		std::optional<ChildRef> parseChildPath(std::string_view path)
		{
			json_pointer ptr;
			try
			{
				ptr = json_pointer(std::string(path));
			}
			catch (const std::exception&)
			{
				return std::nullopt;
			}
			if (ptr.empty())
			{
				return std::nullopt;
			}

			const std::string indexStr = ptr.back();
			ptr.pop_back();
			if (ptr.empty() || ptr.back() != "children")
			{
				return std::nullopt;
			}
			ptr.pop_back();

			size_t index = 0;
			auto [p, ec] = std::from_chars(indexStr.data(), indexStr.data() + indexStr.size(), index);
			if (ec != std::errc{} || p != indexStr.data() + indexStr.size())
			{
				return std::nullopt;
			}

			return ChildRef{std::move(ptr), index};
		}

		bool isGrid(const nlohmann::json& node)
		{
			return node.is_object() && node.value("type", std::string()) == "grid";
		}

		/// (col, row) -> the index (within `children`) of whichever child occupies that cell.
		/// `skipIndex`, if given, is excluded - used to check a widget's *new* placement against only
		/// its siblings, not its own current claim.
		std::map<std::pair<int64_t, int64_t>, size_t> computeOccupancy(
			const nlohmann::json& children, std::optional<size_t> skipIndex = std::nullopt)
		{
			std::map<std::pair<int64_t, int64_t>, size_t> occupied;
			for (size_t i = 0; i < children.size(); i++)
			{
				if (skipIndex && i == *skipIndex)
				{
					continue;
				}
				const auto& child = children[i];
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
						occupied[{c, r}] = i;
					}
				}
			}
			return occupied;
		}

		bool widgetIdUsedAnywhere(const nlohmann::json& node, std::string_view widgetId)
		{
			if (!node.is_object())
			{
				return false;
			}
			if (node.value("widget", std::string()) == widgetId)
			{
				return true;
			}
			if (node.contains("children") && node["children"].is_array())
			{
				for (const auto& child : node["children"])
				{
					if (widgetIdUsedAnywhere(child, widgetId))
					{
						return true;
					}
				}
			}
			return false;
		}

		int64_t trackCount(const nlohmann::json& grid, const char* key)
		{
			return grid.contains(key) && grid[key].is_array() ? static_cast<int64_t>(grid[key].size()) : 0;
		}
	} // namespace

	EditResult moveWidget(nlohmann::json& doc, std::string_view widgetPath, int col, int row)
	{
		ZoneScoped;
		if (col < 0 || row < 0)
		{
			return {false, "col/row must be non-negative"};
		}

		auto ref = parseChildPath(widgetPath);
		if (!ref)
		{
			return {false, "widgetPath must address a node inside a 'children' array"};
		}

		nlohmann::json working = doc;
		nlohmann::json* parent = nullptr;
		try
		{
			parent = &working.at(ref->parentPointer);
		}
		catch (const std::exception&)
		{
			return {false, "widgetPath's parent was not found"};
		}
		if (!isGrid(*parent))
		{
			return {false, "widget's parent is not a grid container"};
		}
		if (!parent->contains("children") || ref->index >= (*parent)["children"].size())
		{
			return {false, "widget index out of range"};
		}

		nlohmann::json& children = (*parent)["children"];
		nlohmann::json& widget = children[ref->index];
		if (!widget.contains("widget"))
		{
			return {false, "widgetPath does not address a widget node"};
		}

		const int64_t colCount = trackCount(*parent, "cols");
		const int64_t rowCount = trackCount(*parent, "rows");

		// A swap happens when some *other* child's own (col, row) anchor exactly matches the target -
		// otherwise the target cell must be entirely free for a plain move. Anything in between (the
		// target lands only partially inside some larger widget's span) is rejected as ambiguous
		// rather than guessed at.
		std::optional<size_t> swapIndex;
		for (size_t i = 0; i < children.size(); i++)
		{
			if (i == ref->index)
			{
				continue;
			}
			const auto& c = children[i];
			if (c.is_object() && c.value("col", int64_t(-1)) == col && c.value("row", int64_t(-1)) == row)
			{
				swapIndex = i;
				break;
			}
		}

		if (swapIndex)
		{
			// A full footprint exchange: each widget adopts the other's exact former col/row/colSpan/
			// rowSpan. No overlap-with-a-third-widget check is needed - both footprints being traded
			// were already valid, non-overlapping placements in the document as it stood before this
			// call (the caller is expected to only ever pass an already-validated document in), so
			// swapping which widget sits in each one can't newly conflict with anyone else.
			nlohmann::json& other = children[*swapIndex];
			const int64_t widgetOrigCol = widget.value("col", int64_t(0));
			const int64_t widgetOrigRow = widget.value("row", int64_t(0));
			const int64_t widgetColSpan = widget.value("colSpan", 1);
			const int64_t widgetRowSpan = widget.value("rowSpan", 1);
			const int64_t otherColSpan = other.value("colSpan", 1);
			const int64_t otherRowSpan = other.value("rowSpan", 1);

			widget["col"] = col;
			widget["row"] = row;
			if (otherColSpan == 1)
			{
				widget.erase("colSpan");
			}
			else
			{
				widget["colSpan"] = otherColSpan;
			}
			if (otherRowSpan == 1)
			{
				widget.erase("rowSpan");
			}
			else
			{
				widget["rowSpan"] = otherRowSpan;
			}

			other["col"] = widgetOrigCol;
			other["row"] = widgetOrigRow;
			if (widgetColSpan == 1)
			{
				other.erase("colSpan");
			}
			else
			{
				other["colSpan"] = widgetColSpan;
			}
			if (widgetRowSpan == 1)
			{
				other.erase("rowSpan");
			}
			else
			{
				other["rowSpan"] = widgetRowSpan;
			}
		}
		else
		{
			const int64_t colSpan = widget.value("colSpan", 1);
			const int64_t rowSpan = widget.value("rowSpan", 1);
			if (col + colSpan > colCount || row + rowSpan > rowCount)
			{
				return {false, "target cell is outside the grid"};
			}

			auto occ = computeOccupancy(children, ref->index);
			for (int64_t c = col; c < col + colSpan; c++)
			{
				for (int64_t r = row; r < row + rowSpan; r++)
				{
					if (occ.count({c, r}))
					{
						return {false, "target cell is only partially free - not a clean move or swap"};
					}
				}
			}
			widget["col"] = col;
			widget["row"] = row;
		}

		doc = std::move(working);
		return {true, ""};
	}

	EditResult resizeWidget(nlohmann::json& doc, std::string_view widgetPath, int colSpan, int rowSpan,
							 const WidgetRegistry& registry)
	{
		ZoneScoped;
		if (colSpan < 1 || rowSpan < 1)
		{
			return {false, "colSpan/rowSpan must be at least 1"};
		}

		auto ref = parseChildPath(widgetPath);
		if (!ref)
		{
			return {false, "widgetPath must address a node inside a 'children' array"};
		}

		nlohmann::json working = doc;
		nlohmann::json* parent = nullptr;
		try
		{
			parent = &working.at(ref->parentPointer);
		}
		catch (const std::exception&)
		{
			return {false, "widgetPath's parent was not found"};
		}
		if (!isGrid(*parent))
		{
			return {false, "widget's parent is not a grid container"};
		}
		if (!parent->contains("children") || ref->index >= (*parent)["children"].size())
		{
			return {false, "widget index out of range"};
		}

		nlohmann::json& children = (*parent)["children"];
		nlohmann::json& widget = children[ref->index];
		if (!widget.contains("widget"))
		{
			return {false, "widgetPath does not address a widget node"};
		}

		const std::string widgetId = widget["widget"].get<std::string>();
		const WidgetDescriptor* descriptor = registry.find(widgetId);
		if (descriptor == nullptr)
		{
			return {false, "unknown widget id"};
		}
		if (colSpan < descriptor->hint.minCols || rowSpan < descriptor->hint.minRows)
		{
			return {false, "requested span is below this widget's minimum size"};
		}

		const int64_t col = widget.value("col", int64_t(0));
		const int64_t row = widget.value("row", int64_t(0));
		const int64_t colCount = trackCount(*parent, "cols");
		const int64_t rowCount = trackCount(*parent, "rows");
		if (col + colSpan > colCount || row + rowSpan > rowCount)
		{
			return {false, "requested span extends outside the grid"};
		}

		auto occ = computeOccupancy(children, ref->index);
		for (int64_t c = col; c < col + colSpan; c++)
		{
			for (int64_t r = row; r < row + rowSpan; r++)
			{
				if (occ.count({c, r}))
				{
					return {false, "requested span overlaps another widget"};
				}
			}
		}

		if (colSpan == 1)
		{
			widget.erase("colSpan");
		}
		else
		{
			widget["colSpan"] = colSpan;
		}
		if (rowSpan == 1)
		{
			widget.erase("rowSpan");
		}
		else
		{
			widget["rowSpan"] = rowSpan;
		}

		doc = std::move(working);
		return {true, ""};
	}

	EditResult removeWidget(nlohmann::json& doc, std::string_view path)
	{
		ZoneScoped;
		auto ref = parseChildPath(path);
		if (!ref)
		{
			return {false, "path must address a node inside a 'children' array"};
		}

		nlohmann::json working = doc;
		nlohmann::json* parent = nullptr;
		try
		{
			parent = &working.at(ref->parentPointer);
		}
		catch (const std::exception&)
		{
			return {false, "path's parent was not found"};
		}
		if (!parent->contains("children") || !(*parent)["children"].is_array())
		{
			return {false, "parent has no children array"};
		}

		nlohmann::json& children = (*parent)["children"];
		if (ref->index >= children.size())
		{
			return {false, "index out of range"};
		}

		children.erase(children.begin() + static_cast<long>(ref->index));
		doc = std::move(working);
		return {true, ""};
	}

	EditResult addWidget(nlohmann::json& doc, std::string_view gridPath, int col, int row,
						  std::string_view widgetId, const WidgetRegistry& registry)
	{
		ZoneScoped;
		if (col < 0 || row < 0)
		{
			return {false, "col/row must be non-negative"};
		}
		const WidgetDescriptor* descriptor = registry.find(widgetId);
		if (descriptor == nullptr)
		{
			return {false, "unknown widget id"};
		}

		nlohmann::json working = doc;
		nlohmann::json* grid = nullptr;
		try
		{
			grid = &working.at(json_pointer(std::string(gridPath)));
		}
		catch (const std::exception&)
		{
			return {false, "gridPath was not found"};
		}
		if (!isGrid(*grid))
		{
			return {false, "gridPath is not a grid container"};
		}

		if (descriptor->singleton && working.contains("root") && widgetIdUsedAnywhere(working["root"], widgetId))
		{
			return {false, "this widget is a singleton and is already placed elsewhere in the document"};
		}

		const int64_t colCount = trackCount(*grid, "cols");
		const int64_t rowCount = trackCount(*grid, "rows");
		if (col >= colCount || row >= rowCount)
		{
			return {false, "target cell is outside the grid"};
		}

		if (!grid->contains("children") || !(*grid)["children"].is_array())
		{
			(*grid)["children"] = nlohmann::json::array();
		}
		nlohmann::json& children = (*grid)["children"];

		auto occ = computeOccupancy(children);
		if (occ.count({col, row}))
		{
			return {false, "target cell is already occupied"};
		}

		nlohmann::json newNode;
		newNode["widget"] = std::string(widgetId);
		newNode["col"] = col;
		newNode["row"] = row;
		children.push_back(std::move(newNode));

		doc = std::move(working);
		return {true, ""};
	}

	EditResult addTrack(nlohmann::json& doc, std::string_view gridPath, bool isColumn, std::string_view sizeToken)
	{
		ZoneScoped;
		int32_t dummy;
		if (!LayoutBuilder::parseTrackSize(sizeToken, dummy))
		{
			return {false, "invalid track size token"};
		}

		nlohmann::json working = doc;
		nlohmann::json* grid = nullptr;
		try
		{
			grid = &working.at(json_pointer(std::string(gridPath)));
		}
		catch (const std::exception&)
		{
			return {false, "gridPath was not found"};
		}
		if (!isGrid(*grid))
		{
			return {false, "gridPath is not a grid container"};
		}

		const char* key = isColumn ? "cols" : "rows";
		if (!grid->contains(key) || !(*grid)[key].is_array())
		{
			return {false, "grid is missing its cols/rows array"};
		}
		(*grid)[key].push_back(std::string(sizeToken));

		doc = std::move(working);
		return {true, ""};
	}

	EditResult removeTrack(nlohmann::json& doc, std::string_view gridPath, bool isColumn, int index)
	{
		ZoneScoped;
		if (index < 0)
		{
			return {false, "index must be non-negative"};
		}

		nlohmann::json working = doc;
		nlohmann::json* grid = nullptr;
		try
		{
			grid = &working.at(json_pointer(std::string(gridPath)));
		}
		catch (const std::exception&)
		{
			return {false, "gridPath was not found"};
		}
		if (!isGrid(*grid))
		{
			return {false, "gridPath is not a grid container"};
		}

		const char* key = isColumn ? "cols" : "rows";
		if (!grid->contains(key) || !(*grid)[key].is_array())
		{
			return {false, "grid is missing its cols/rows array"};
		}
		nlohmann::json& tracks = (*grid)[key];
		if (static_cast<size_t>(index) >= tracks.size())
		{
			return {false, "track index out of range"};
		}
		if (tracks.size() <= 1)
		{
			return {false, "cannot remove the grid's last remaining track"};
		}

		if (grid->contains("children") && (*grid)["children"].is_array())
		{
			nlohmann::json& children = (*grid)["children"];
			const char* posKey = isColumn ? "col" : "row";
			const char* spanKey = isColumn ? "colSpan" : "rowSpan";
			for (const auto& child : children)
			{
				if (!child.is_object() || !child.contains(posKey))
				{
					continue;
				}
				const int64_t pos = child[posKey].get<int64_t>();
				const int64_t span = child.value(spanKey, 1);
				if (pos <= index && index < pos + span)
				{
					return {false, fmt::format("track {} is occupied by a widget", index)};
				}
			}

			for (auto& child : children)
			{
				if (!child.is_object() || !child.contains(posKey))
				{
					continue;
				}
				const int64_t pos = child[posKey].get<int64_t>();
				if (pos > index)
				{
					child[posKey] = pos - 1;
				}
			}
		}

		tracks.erase(tracks.begin() + index);
		doc = std::move(working);
		return {true, ""};
	}

	EditResult setTrackSize(nlohmann::json& doc, std::string_view gridPath, bool isColumn, int index,
							std::string_view sizeToken)
	{
		ZoneScoped;
		int32_t dummy;
		if (!LayoutBuilder::parseTrackSize(sizeToken, dummy))
		{
			return {false, "invalid track size token"};
		}
		if (index < 0)
		{
			return {false, "index must be non-negative"};
		}

		nlohmann::json working = doc;
		nlohmann::json* grid = nullptr;
		try
		{
			grid = &working.at(json_pointer(std::string(gridPath)));
		}
		catch (const std::exception&)
		{
			return {false, "gridPath was not found"};
		}
		if (!isGrid(*grid))
		{
			return {false, "gridPath is not a grid container"};
		}

		const char* key = isColumn ? "cols" : "rows";
		if (!grid->contains(key) || !(*grid)[key].is_array())
		{
			return {false, "grid is missing its cols/rows array"};
		}
		nlohmann::json& tracks = (*grid)[key];
		if (static_cast<size_t>(index) >= tracks.size())
		{
			return {false, "track index out of range"};
		}

		tracks[index] = std::string(sizeToken);
		doc = std::move(working);
		return {true, ""};
	}
} // namespace UI::Layout

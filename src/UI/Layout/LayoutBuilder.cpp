/*
 * LayoutBuilder.cpp
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#include "LayoutBuilder.h"
#include "Debug.h"
#include "UI/Components/Containers/Column.h"
#include "UI/Components/Containers/Row.h"
#include "UI/Components/Containers/TabView.h"
#include "UI/Components/LVGL/LvContainer.h"
#include "i18n/i18n.h"
#include <charconv>
#include <fmt/format.h>
#include <map>
#include <set>

namespace UI::Layout
{
	namespace
	{
		constexpr int CURRENT_SCHEMA_VERSION = 1;

		std::string childPath(const std::string& parentPath, size_t index)
		{
			return parentPath.empty() ? std::to_string(index) : fmt::format("{}.{}", parentPath, index);
		}

		/// The node's own "id" if given, otherwise a path-derived default. See
		/// docs/LAYOUT_ENGINE_DESIGN.md section 4.2 on why per-instance ids matter.
		std::string nodeId(const nlohmann::json& node, const std::string& path)
		{
			if (node.contains("id") && node["id"].is_string())
			{
				return node["id"].get<std::string>();
			}
			return path;
		}

		bool isContainerType(std::string_view type)
		{
			return type == "grid" || type == "row" || type == "column" || type == "tabs";
		}

		// Shared by validate() and build() so the two can never disagree on what's well-formed.
		void validateNode(
			const nlohmann::json& node,
			const WidgetRegistry& registry,
			const std::string& path,
			bool mustBeWidget,
			std::set<std::string>& seenIds,
			std::set<std::string>& usedSingletons,
			std::vector<std::string>& errors)
		{
			if (!node.is_object())
			{
				errors.push_back(fmt::format("node at '{}': expected an object", path));
				return;
			}

			if (node.contains("id") && node["id"].is_string())
			{
				const std::string id = node["id"].get<std::string>();
				if (!seenIds.insert(id).second)
				{
					errors.push_back(fmt::format("node at '{}': id '{}' is used more than once", path, id));
				}
			}

			const bool hasWidget = node.contains("widget");
			const bool hasType = node.contains("type");
			if (hasWidget == hasType)
			{
				errors.push_back(
					fmt::format("node at '{}': must have exactly one of 'widget' or 'type'", path));
				return;
			}

			if (hasWidget)
			{
				if (!node["widget"].is_string())
				{
					errors.push_back(fmt::format("node at '{}': 'widget' must be a string", path));
					return;
				}
				const std::string widgetId = node["widget"].get<std::string>();
				const WidgetDescriptor* descriptor = registry.find(widgetId);
				if (descriptor == nullptr)
				{
					errors.push_back(fmt::format("node at '{}': unknown widget id '{}'", path, widgetId));
					return;
				}
				if (descriptor->singleton && !usedSingletons.insert(widgetId).second)
				{
					errors.push_back(
						fmt::format("node at '{}': widget '{}' is a singleton and cannot be placed more than once",
									path,
									widgetId));
				}
				return;
			}

			// hasType
			if (mustBeWidget)
			{
				errors.push_back(fmt::format("node at '{}': tabs children must be widgets, not containers", path));
				return;
			}

			if (!node["type"].is_string())
			{
				errors.push_back(fmt::format("node at '{}': 'type' must be a string", path));
				return;
			}
			const std::string type = node["type"].get<std::string>();
			if (!isContainerType(type))
			{
				errors.push_back(fmt::format("node at '{}': unknown container type '{}'", path, type));
				return;
			}

			if (!node.contains("children") || !node["children"].is_array())
			{
				errors.push_back(fmt::format("node at '{}': '{}' container requires a 'children' array", path, type));
				return;
			}
			const auto& children = node["children"];

			if (type == "grid")
			{
				if (!node.contains("cols") || !node["cols"].is_array() || node["cols"].empty())
				{
					errors.push_back(fmt::format("node at '{}': grid requires a non-empty 'cols' array", path));
					return;
				}
				if (!node.contains("rows") || !node["rows"].is_array() || node["rows"].empty())
				{
					errors.push_back(fmt::format("node at '{}': grid requires a non-empty 'rows' array", path));
					return;
				}

				int32_t dummy;
				for (size_t i = 0; i < node["cols"].size(); i++)
				{
					if (!node["cols"][i].is_string() || !LayoutBuilder::parseTrackSize(node["cols"][i].get<std::string>(), dummy))
					{
						errors.push_back(fmt::format("node at '{}': cols[{}] is not a valid track size", path, i));
					}
				}
				for (size_t i = 0; i < node["rows"].size(); i++)
				{
					if (!node["rows"][i].is_string() || !LayoutBuilder::parseTrackSize(node["rows"][i].get<std::string>(), dummy))
					{
						errors.push_back(fmt::format("node at '{}': rows[{}] is not a valid track size", path, i));
					}
				}

				const int64_t colCount = static_cast<int64_t>(node["cols"].size());
				const int64_t rowCount = static_cast<int64_t>(node["rows"].size());
				// (col, row) -> the child index that first claimed it, for a friendlier overlap message.
				std::map<std::pair<int64_t, int64_t>, size_t> occupied;

				for (size_t i = 0; i < children.size(); i++)
				{
					const std::string childP = childPath(path, i);
					const auto& child = children[i];
					if (!child.is_object() || !child.contains("col") || !child["col"].is_number_integer() ||
						!child.contains("row") || !child["row"].is_number_integer())
					{
						errors.push_back(
							fmt::format("node at '{}': grid child requires integer 'col' and 'row'", childP));
					}
					else
					{
						const int64_t col = child["col"].get<int64_t>();
						const int64_t row = child["row"].get<int64_t>();
						const int64_t colSpan = child.value("colSpan", 1);
						const int64_t rowSpan = child.value("rowSpan", 1);

						if (col < 0 || row < 0 || colSpan < 1 || rowSpan < 1 || col + colSpan > colCount ||
							row + rowSpan > rowCount)
						{
							errors.push_back(fmt::format(
								"node at '{}': cell [col={}, row={}, colSpan={}, rowSpan={}] is out of the {}x{} grid",
								childP,
								col,
								row,
								colSpan,
								rowSpan,
								colCount,
								rowCount));
						}
						else
						{
							for (int64_t c = col; c < col + colSpan; c++)
							{
								for (int64_t r = row; r < row + rowSpan; r++)
								{
									auto [it, inserted] = occupied.try_emplace({c, r}, i);
									if (!inserted)
									{
										errors.push_back(fmt::format(
											"node at '{}': cell [col={}, row={}] overlaps the child at index {}",
											childP,
											c,
											r,
											it->second));
									}
								}
							}
						}
					}

					if (child.contains("xAlign") && child["xAlign"].is_string())
					{
						lv_grid_align_t dummyAlign;
						if (!LayoutBuilder::parseAlign(child["xAlign"].get<std::string>(), dummyAlign))
						{
							errors.push_back(fmt::format("node at '{}': invalid 'xAlign'", childP));
						}
					}
					if (child.contains("yAlign") && child["yAlign"].is_string())
					{
						lv_grid_align_t dummyAlign;
						if (!LayoutBuilder::parseAlign(child["yAlign"].get<std::string>(), dummyAlign))
						{
							errors.push_back(fmt::format("node at '{}': invalid 'yAlign'", childP));
						}
					}

					validateNode(child, registry, childP, false, seenIds, usedSingletons, errors);
				}
				return;
			}

			// row / column / tabs all just recurse into their children.
			const bool childrenMustBeWidgets = (type == "tabs");
			for (size_t i = 0; i < children.size(); i++)
			{
				validateNode(
					children[i], registry, childPath(path, i), childrenMustBeWidgets, seenIds, usedSingletons, errors);
			}
		}

		LvObj* buildNode(
			const nlohmann::json& node, LvObj& parent, const std::string& path, LayoutInstance& instance, const WidgetRegistry& registry)
		{
			const std::string id = nodeId(node, path);

			if (node.contains("widget"))
			{
				const std::string widgetId = node["widget"].get<std::string>();
				const WidgetDescriptor* descriptor = registry.find(widgetId);
				const nlohmann::json props = node.value("props", nlohmann::json::object());

				std::unique_ptr<LvObj> widget = descriptor->create(id, parent, props);
				LvObj* raw = widget.get();
				instance.m_owned.push_back(std::move(widget));
				instance.m_byId[id] = raw;
				return raw;
			}

			const std::string type = node["type"].get<std::string>();
			const auto& children = node["children"];

			if (type == "grid")
			{
				auto container = std::make_unique<LvContainer>(id, parent);
				LvObj* raw = container.get();

				std::vector<int32_t> colDsc;
				for (const auto& token : node["cols"])
				{
					int32_t value = 0;
					LayoutBuilder::parseTrackSize(token.get<std::string>(), value);
					colDsc.push_back(value);
				}
				colDsc.push_back(LV_GRID_TEMPLATE_LAST);

				std::vector<int32_t> rowDsc;
				for (const auto& token : node["rows"])
				{
					int32_t value = 0;
					LayoutBuilder::parseTrackSize(token.get<std::string>(), value);
					rowDsc.push_back(value);
				}
				rowDsc.push_back(LV_GRID_TEMPLATE_LAST);

				raw->setLayoutStyle(LV_LAYOUT_GRID);
				instance.m_gridTrackStorage.push_back(std::move(colDsc));
				const auto& storedColDsc = instance.m_gridTrackStorage.back();
				instance.m_gridTrackStorage.push_back(std::move(rowDsc));
				const auto& storedRowDsc = instance.m_gridTrackStorage.back();
				raw->setGridDsc(storedColDsc, storedRowDsc);

				instance.m_owned.push_back(std::move(container));
				instance.m_byId[id] = raw;

				for (size_t i = 0; i < children.size(); i++)
				{
					const auto& child = children[i];
					LvObj* childObj = buildNode(child, *raw, childPath(path, i), instance, registry);

					lv_grid_align_t xAlign = LV_GRID_ALIGN_STRETCH;
					if (child.contains("xAlign"))
					{
						LayoutBuilder::parseAlign(child["xAlign"].get<std::string>(), xAlign);
					}
					lv_grid_align_t yAlign = LV_GRID_ALIGN_STRETCH;
					if (child.contains("yAlign"))
					{
						LayoutBuilder::parseAlign(child["yAlign"].get<std::string>(), yAlign);
					}

					raw->setGridCell(
						*childObj,
						xAlign,
						static_cast<int32_t>(child["col"].get<int64_t>()),
						static_cast<int32_t>(child.value("colSpan", 1)),
						yAlign,
						static_cast<int32_t>(child["row"].get<int64_t>()),
						static_cast<int32_t>(child.value("rowSpan", 1)));
				}
				return raw;
			}

			if (type == "row" || type == "column")
			{
				std::unique_ptr<LvObj> container;
				if (type == "row")
				{
					container = std::make_unique<Row>(id, parent);
				}
				else
				{
					container = std::make_unique<Column>(id, parent);
				}
				LvObj* raw = container.get();
				instance.m_owned.push_back(std::move(container));
				instance.m_byId[id] = raw;

				for (size_t i = 0; i < children.size(); i++)
				{
					const auto& child = children[i];
					LvObj* childObj = buildNode(child, *raw, childPath(path, i), instance, registry);
					childObj->setFlexGrow(static_cast<uint8_t>(child.value("grow", 0)));
				}
				return raw;
			}

			// tabs
			auto tabView = std::make_unique<TabView>(id, parent);
			LvObj* raw = tabView.get();
			instance.m_owned.push_back(std::move(tabView));
			instance.m_byId[id] = raw;

			for (size_t i = 0; i < children.size(); i++)
			{
				const auto& child = children[i];
				const std::string childId = nodeId(child, childPath(path, i));
				const WidgetDescriptor* descriptor = registry.find(child["widget"].get<std::string>());
				LvContainer& tabContent = static_cast<TabView*>(raw)->addTab(_(descriptor->nameKey), childId);
				buildNode(child, tabContent, childPath(path, i), instance, registry);
			}
			return raw;
		}
	} // namespace

	LvObj* LayoutInstance::find(std::string_view id) const
	{
		ZoneScoped;
		auto it = m_byId.find(std::string(id));
		return it == m_byId.end() ? nullptr : it->second;
	}

	LayoutBuilder::ValidationResult LayoutBuilder::validate(const nlohmann::json& doc, const WidgetRegistry& registry)
	{
		ZoneScoped;
		ValidationResult result;

		if (!doc.is_object())
		{
			result.errors.push_back("document must be an object");
			return result;
		}
		if (!doc.contains("schema") || !doc["schema"].is_number_integer() ||
			doc["schema"].get<int>() != CURRENT_SCHEMA_VERSION)
		{
			result.errors.push_back(fmt::format("document must have \"schema\": {}", CURRENT_SCHEMA_VERSION));
			return result;
		}
		if (!doc.contains("root") || !doc["root"].is_object())
		{
			result.errors.push_back("document must have a 'root' object");
			return result;
		}

		std::set<std::string> seenIds;
		std::set<std::string> usedSingletons;
		validateNode(doc["root"], registry, "root", false, seenIds, usedSingletons, result.errors);

		result.ok = result.errors.empty();
		return result;
	}

	std::unique_ptr<LayoutInstance> LayoutBuilder::build(
		const nlohmann::json& doc, LvObj& parent, const WidgetRegistry& registry, std::vector<std::string>* errorsOut)
	{
		ZoneScoped;
		ValidationResult validation = validate(doc, registry);
		if (!validation.ok)
		{
			for (const auto& error : validation.errors)
			{
				LOG_ERROR("Layout validation failed: {}", error);
			}
			if (errorsOut != nullptr)
			{
				*errorsOut = std::move(validation.errors);
			}
			return nullptr;
		}

		auto instance = std::make_unique<LayoutInstance>();
		instance->m_root = buildNode(doc["root"], parent, "root", *instance, registry);
		return instance;
	}

	bool LayoutBuilder::parseTrackSize(std::string_view token, int32_t& out)
	{
		ZoneScoped;
		if (token == "content")
		{
			out = LV_GRID_CONTENT;
			return true;
		}

		if (token.ends_with("fr"))
		{
			int value = 0;
			auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size() - 2, value);
			if (ec != std::errc{} || ptr != token.data() + token.size() - 2 || value <= 0)
			{
				return false;
			}
			out = LV_GRID_FR(value);
			return true;
		}

		if (token.ends_with('%'))
		{
			int value = 0;
			auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size() - 1, value);
			if (ec != std::errc{} || ptr != token.data() + token.size() - 1)
			{
				return false;
			}
			out = LV_PCT(value);
			return true;
		}

		std::string_view number = token.ends_with("px") ? token.substr(0, token.size() - 2) : token;
		int value = 0;
		auto [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
		if (ec != std::errc{} || ptr != number.data() + number.size() || value < 0)
		{
			return false;
		}
		out = value;
		return true;
	}

	bool LayoutBuilder::parseAlign(std::string_view token, lv_grid_align_t& out)
	{
		ZoneScoped;
		if (token == "start")
		{
			out = LV_GRID_ALIGN_START;
		}
		else if (token == "center")
		{
			out = LV_GRID_ALIGN_CENTER;
		}
		else if (token == "end")
		{
			out = LV_GRID_ALIGN_END;
		}
		else if (token == "stretch")
		{
			out = LV_GRID_ALIGN_STRETCH;
		}
		else
		{
			return false;
		}
		return true;
	}
} // namespace UI::Layout

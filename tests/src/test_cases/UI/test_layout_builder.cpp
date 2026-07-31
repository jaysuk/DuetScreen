/*
 * test_layout_builder.cpp
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#include "UI/Components/Containers/TabView.h"
#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Layout/DashboardWidgets.h"
#include "UI/Layout/LayoutBuilder.h"
#include "test_utils/TestSuite.h"
#include "test_utils/UiTestSuite.h"
#include <gtest/gtest.h>

using namespace UI;
using namespace UI::Layout;
using json = nlohmann::json;

namespace
{
	// A small, local registry of fake widgets - kept separate from the real dashboard registry so
	// these tests don't depend on (or pollute) WidgetRegistry::get().
	WidgetRegistry makeTestRegistry()
	{
		WidgetRegistry registry;
		registry.add({
			.id = "widget_a",
			.nameKey = {},
			.icon = {},
			.hint = {},
			.singleton = false,
			.create = [](const std::string& id, LvObj& parent, const json&) -> std::unique_ptr<LvObj>
			{ return std::make_unique<LvContainer>(id, parent); },
			.available = nullptr,
		});
		registry.add({
			.id = "widget_singleton",
			.nameKey = "widget.singleton.name",
			.icon = {},
			.hint = {},
			.singleton = true,
			.create = [](const std::string& id, LvObj& parent, const json&) -> std::unique_ptr<LvObj>
			{ return std::make_unique<LvContainer>(id, parent); },
			.available = nullptr,
		});
		return registry;
	}
} // namespace

// -Wmissing-field-initializers, and no LVGL context needed here.
class TestLayoutBuilderValidate : public TestSuite
{
};

TEST_F(TestLayoutBuilderValidate, MinimalValidDocumentPasses)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "widget_a", "id": "a1" }
	})");

	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors[0]);
	EXPECT_TRUE(result.errors.empty());
}

TEST_F(TestLayoutBuilderValidate, NonObjectDocumentFails)
{
	EXPECT_FALSE(LayoutBuilder::validate(json::array()).ok);
}

TEST_F(TestLayoutBuilderValidate, WrongSchemaVersionFails)
{
	json doc = json::parse(R"({
		"schema": 999,
		"root": { "widget": "widget_a" }
	})");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, MissingRootFails)
{
	json doc = json::parse(R"({ "schema": 1 })");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, NodeWithNeitherWidgetNorTypeFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "id": "a1" }
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("exactly one of"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, NodeWithBothWidgetAndTypeFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "widget_a", "type": "grid" }
	})");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, UnknownWidgetIdFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "does_not_exist" }
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("unknown widget id"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, UnknownContainerTypeFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "flexbox", "children": [] }
	})");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, SingletonWidgetPlacedTwiceFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "row",
			"children": [
				{ "widget": "widget_singleton", "id": "s1" },
				{ "widget": "widget_singleton", "id": "s2" }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("singleton"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, SameNonSingletonWidgetPlacedTwicePasses)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "row",
			"children": [
				{ "widget": "widget_a", "id": "a1" },
				{ "widget": "widget_a", "id": "a2" }
			]
		}
	})");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, DuplicateExplicitIdFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "row",
			"children": [
				{ "widget": "widget_a", "id": "dup" },
				{ "widget": "widget_a", "id": "dup" }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("used more than once"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, GridMissingColsFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "rows": ["1fr"], "children": [] }
	})");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, GridChildOutOfBoundsFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid",
			"cols": ["1fr"],
			"rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 5, "row": 0 }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("out of the"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, GridOverlappingCellsFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid",
			"cols": ["1fr", "1fr"],
			"rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0, "colSpan": 2 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0 }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("overlaps"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, GridNonOverlappingCellsPass)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid",
			"cols": ["1fr", "1fr"],
			"rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0 }
			]
		}
	})");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, GridInvalidTrackSizeFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["nonsense"], "rows": ["1fr"], "children": [] }
	})");
	EXPECT_FALSE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, TabsChildThatIsAContainerFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "tabs",
			"children": [
				{ "type": "row", "children": [] }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	ASSERT_FALSE(result.errors.empty());
	EXPECT_NE(result.errors[0].find("tabs children must be widgets"), std::string::npos);
}

TEST_F(TestLayoutBuilderValidate, TabsChildThatIsAWidgetPasses)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "tabs",
			"children": [
				{ "widget": "widget_a", "id": "a1" }
			]
		}
	})");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutBuilderValidate, CollectsMultipleErrorsRatherThanStoppingAtTheFirst)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "row",
			"children": [
				{ "widget": "does_not_exist_a" },
				{ "widget": "does_not_exist_b" }
			]
		}
	})");
	auto result = LayoutBuilder::validate(doc, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(result.errors.size(), 2u);
}

TEST_F(TestLayoutBuilderValidate, ParseTrackSizeAcceptsKnownForms)
{
	int32_t value = 0;
	EXPECT_TRUE(LayoutBuilder::parseTrackSize("3fr", value));
	EXPECT_EQ(value, LV_GRID_FR(3));

	EXPECT_TRUE(LayoutBuilder::parseTrackSize("content", value));
	EXPECT_EQ(value, LV_GRID_CONTENT);

	EXPECT_TRUE(LayoutBuilder::parseTrackSize("120px", value));
	EXPECT_EQ(value, 120);

	EXPECT_TRUE(LayoutBuilder::parseTrackSize("50%", value));
	EXPECT_EQ(value, LV_PCT(50));

	EXPECT_TRUE(LayoutBuilder::parseTrackSize("42", value));
	EXPECT_EQ(value, 42);
}

TEST_F(TestLayoutBuilderValidate, ParseTrackSizeRejectsGarbage)
{
	int32_t value = 0;
	EXPECT_FALSE(LayoutBuilder::parseTrackSize("nonsense", value));
	EXPECT_FALSE(LayoutBuilder::parseTrackSize("", value));
	EXPECT_FALSE(LayoutBuilder::parseTrackSize("fr", value));
	EXPECT_FALSE(LayoutBuilder::parseTrackSize("-1fr", value));
}

TEST_F(TestLayoutBuilderValidate, ParseAlignAcceptsKnownTokensAndRejectsOthers)
{
	lv_grid_align_t align;
	EXPECT_TRUE(LayoutBuilder::parseAlign("start", align));
	EXPECT_EQ(align, LV_GRID_ALIGN_START);
	EXPECT_TRUE(LayoutBuilder::parseAlign("center", align));
	EXPECT_EQ(align, LV_GRID_ALIGN_CENTER);
	EXPECT_TRUE(LayoutBuilder::parseAlign("end", align));
	EXPECT_EQ(align, LV_GRID_ALIGN_END);
	EXPECT_TRUE(LayoutBuilder::parseAlign("stretch", align));
	EXPECT_EQ(align, LV_GRID_ALIGN_STRETCH);
	EXPECT_FALSE(LayoutBuilder::parseAlign("nonsense", align));
}

// Needs a real LvObj to build under.
class TestLayoutBuilderBuild : public UiTestSuite
{
};

TEST_F(TestLayoutBuilderBuild, InvalidDocumentReturnsNullptrAndPopulatesErrors)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "does_not_exist" }
	})");
	std::vector<std::string> errors;

	auto instance = LayoutBuilder::build(doc, screen, makeTestRegistry(), &errors);
	EXPECT_EQ(instance, nullptr);
	EXPECT_FALSE(errors.empty());
}

TEST_F(TestLayoutBuilderBuild, SingleWidgetBuildsAndIsFindable)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "widget_a", "id": "a1" }
	})");

	auto instance = LayoutBuilder::build(doc, screen, makeTestRegistry());
	ASSERT_NE(instance, nullptr);
	ASSERT_NE(instance->getRoot(), nullptr);
	EXPECT_EQ(instance->find("a1"), instance->getRoot());
	EXPECT_EQ(instance->find("nonexistent"), nullptr);
}

TEST_F(TestLayoutBuilderBuild, GridBuildsAllChildrenAsGridChildrenOfRoot)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid",
			"id": "grid1",
			"cols": ["1fr", "1fr"],
			"rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0 }
			]
		}
	})");

	auto instance = LayoutBuilder::build(doc, screen, makeTestRegistry());
	ASSERT_NE(instance, nullptr);

	LvObj* root = instance->getRoot();
	ASSERT_NE(root, nullptr);
	EXPECT_EQ(root, instance->find("grid1"));
	EXPECT_EQ(root->getChildCount(), 2u);

	LvObj* a1 = instance->find("a1");
	LvObj* a2 = instance->find("a2");
	ASSERT_NE(a1, nullptr);
	ASSERT_NE(a2, nullptr);
	EXPECT_EQ(a1->getParent(), root);
	EXPECT_EQ(a2->getParent(), root);
}

TEST_F(TestLayoutBuilderBuild, RowSetsFlexGrowFromChildren)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "row",
			"children": [
				{ "widget": "widget_a", "id": "a1", "grow": 2 },
				{ "widget": "widget_a", "id": "a2" }
			]
		}
	})");

	auto instance = LayoutBuilder::build(doc, screen, makeTestRegistry());
	ASSERT_NE(instance, nullptr);
	EXPECT_EQ(instance->getRoot()->getChildCount(), 2u);
}

TEST_F(TestLayoutBuilderBuild, TabsBuildsATabPerChildWithWidgetNameKeyAsLabel)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "tabs",
			"id": "tabs1",
			"children": [
				{ "widget": "widget_singleton", "id": "s1" }
			]
		}
	})");

	auto instance = LayoutBuilder::build(doc, screen, makeTestRegistry());
	ASSERT_NE(instance, nullptr);

	auto* tabView = static_cast<TabView*>(instance->getRoot());
	ASSERT_NE(tabView, nullptr);
	EXPECT_EQ(tabView->getTabCount(), 1u);
	EXPECT_EQ(tabView->getTabIndexById("s1"), 0u);

	LvObj* widget = instance->find("s1");
	ASSERT_NE(widget, nullptr);
	EXPECT_EQ(widget->getParent(), tabView->getTab(0));
}

TEST_F(TestLayoutBuilderBuild, RealDashboardWidgetsBuildEndToEnd)
{
	registerDashboardWidgets();

	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid",
			"cols": ["3fr", "2fr"],
			"rows": ["content", "1fr"],
			"children": [
				{ "widget": "tool_list", "id": "tools", "col": 0, "row": 0, "yAlign": "start" },
				{ "widget": "temperature_graph", "id": "graph", "col": 0, "row": 1 },
				{
					"type": "tabs",
					"id": "tabs",
					"col": 1,
					"row": 0,
					"rowSpan": 2,
					"children": [
						{ "widget": "file_browser_jobs", "id": "jobs" },
						{ "widget": "file_browser_macros", "id": "macros" },
						{ "widget": "status", "id": "status" }
					]
				}
			]
		}
	})");

	auto instance = LayoutBuilder::build(doc, screen);
	ASSERT_NE(instance, nullptr);
	EXPECT_NE(instance->find("tools"), nullptr);
	EXPECT_NE(instance->find("graph"), nullptr);
	EXPECT_NE(instance->find("jobs"), nullptr);
	EXPECT_NE(instance->find("macros"), nullptr);
	EXPECT_NE(instance->find("status"), nullptr);

	auto* tabView = static_cast<TabView*>(instance->find("tabs"));
	ASSERT_NE(tabView, nullptr);
	EXPECT_EQ(tabView->getTabCount(), 3u);
}

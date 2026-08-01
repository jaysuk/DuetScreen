/*
 * test_layout_document_editor.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Layout/LayoutBuilder.h"
#include "UI/Layout/LayoutDocumentEditor.h"
#include "test_utils/TestSuite.h"
#include <gtest/gtest.h>

using namespace UI;
using namespace UI::Layout;
using json = nlohmann::json;

namespace
{
	// A small, local registry - kept separate from the real dashboard registry so these tests don't
	// depend on (or pollute) WidgetRegistry::get(). "widget_big" has a non-trivial GridHint so
	// resizeWidget's minimum-size rejection has something real to check against.
	WidgetRegistry makeTestRegistry()
	{
		WidgetRegistry registry;
		auto create = [](const std::string& id, LvObj& parent, const json&) -> std::unique_ptr<LvObj>
		{ return std::make_unique<LvContainer>(id, parent); };
		registry.add({
			.id = "widget_a",
			.nameKey = {},
			.icon = {},
			.hint = {},
			.singleton = false,
			.create = create,
			.available = nullptr,
		});
		registry.add({
			.id = "widget_big",
			.nameKey = {},
			.icon = {},
			.hint = {.minCols = 2, .minRows = 2},
			.singleton = false,
			.create = create,
			.available = nullptr,
		});
		registry.add({
			.id = "widget_singleton",
			.nameKey = {},
			.icon = {},
			.hint = {},
			.singleton = true,
			.create = create,
			.available = nullptr,
		});
		return registry;
	}
} // namespace

class TestLayoutDocumentEditor : public TestSuite
{
};

// --- moveWidget ---

TEST_F(TestLayoutDocumentEditor, MoveWidgetToEmptyCellSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");

	auto result = moveWidget(doc, "/root/children/0", 1, 0);
	EXPECT_TRUE(result.ok) << result.error;
	EXPECT_EQ(doc["root"]["children"][0]["col"], 1);
	EXPECT_EQ(doc["root"]["children"][0]["row"], 0);
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, MoveWidgetOntoAnotherWidgetsAnchorSwapsPositionAndSize)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr", "1fr"], "rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0, "colSpan": 2 },
				{ "widget": "widget_a", "id": "a2", "col": 2, "row": 0 }
			]
		}
	})");

	// a2 (1x1) swaps with a1 (2x1): each fully adopts the other's former footprint, not just its cell
	// - a2 becomes 2-wide at (0,0), a1 becomes 1-wide (its implicit default) at (2,0). This is the
	// only swap semantics that's always grid-valid: keeping each widget's *own* original size while
	// only swapping position could push a wide widget past the grid's edge (exactly what a1 moving
	// to col=2 with its own colSpan=2 would do here - there is no col=3).
	auto result = moveWidget(doc, "/root/children/1", 0, 0);
	EXPECT_TRUE(result.ok) << result.error;
	EXPECT_EQ(doc["root"]["children"][1]["col"], 0);
	EXPECT_EQ(doc["root"]["children"][1]["row"], 0);
	EXPECT_EQ(doc["root"]["children"][1]["colSpan"], 2);
	EXPECT_EQ(doc["root"]["children"][0]["col"], 2);
	EXPECT_EQ(doc["root"]["children"][0]["row"], 0);
	EXPECT_FALSE(doc["root"]["children"][0].contains("colSpan"));
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, MoveWidgetPartiallyOverlappingAThirdWidgetFailsAndLeavesDocUnchanged)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr", "1fr"], "rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0, "colSpan": 2 }
			]
		}
	})");
	json before = doc;

	// col=2 is inside a2's span (cols 1-2) but is not a2's *own* anchor (col=1) - not a clean swap
	// target, since there's no single widget whose whole footprint could be exchanged for it.
	auto result = moveWidget(doc, "/root/children/0", 2, 0);
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, MoveWidgetOutsideGridFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");
	json before = doc;

	auto result = moveWidget(doc, "/root/children/0", 5, 5);
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, MoveWidgetWhoseParentIsNotAGridFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "tabs", "children": [ { "widget": "widget_a", "id": "a1" } ] }
	})");
	json before = doc;

	auto result = moveWidget(doc, "/root/children/0", 0, 0);
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

// --- resizeWidget ---

TEST_F(TestLayoutDocumentEditor, ResizeWidgetSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr", "1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");

	auto result = resizeWidget(doc, "/root/children/0", 2, 1, makeTestRegistry());
	EXPECT_TRUE(result.ok) << result.error;
	EXPECT_EQ(doc["root"]["children"][0]["colSpan"], 2);
	EXPECT_FALSE(doc["root"]["children"][0].contains("rowSpan"));
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, ResizeWidgetBelowMinimumSizeFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr", "1fr"],
			"children": [ { "widget": "widget_big", "id": "b1", "col": 0, "row": 0, "colSpan": 2, "rowSpan": 2 } ]
		}
	})");
	json before = doc;

	auto result = resizeWidget(doc, "/root/children/0", 1, 1, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, ResizeWidgetOverlappingANeighbourFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0 }
			]
		}
	})");
	json before = doc;

	auto result = resizeWidget(doc, "/root/children/0", 2, 1, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, ResizeWidgetExtendingOutsideGridFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");
	json before = doc;

	auto result = resizeWidget(doc, "/root/children/0", 2, 1, makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

// --- removeWidget ---

TEST_F(TestLayoutDocumentEditor, RemoveWidgetFromGridSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 1, "row": 0 }
			]
		}
	})");

	auto result = removeWidget(doc, "/root/children/0");
	EXPECT_TRUE(result.ok) << result.error;
	ASSERT_EQ(doc["root"]["children"].size(), 1u);
	EXPECT_EQ(doc["root"]["children"][0]["id"], "a2");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, RemoveWidgetFromTabsSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "tabs",
			"children": [
				{ "widget": "widget_a", "id": "a1" },
				{ "widget": "widget_a", "id": "a2" }
			]
		}
	})");

	auto result = removeWidget(doc, "/root/children/1");
	EXPECT_TRUE(result.ok) << result.error;
	ASSERT_EQ(doc["root"]["children"].size(), 1u);
	EXPECT_EQ(doc["root"]["children"][0]["id"], "a1");
}

TEST_F(TestLayoutDocumentEditor, RemoveWidgetWithBadPathFailsAndLeavesDocUnchanged)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "widget": "widget_a", "id": "a1" }
	})");
	json before = doc;

	auto result = removeWidget(doc, "/root");
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

// --- addWidget ---

TEST_F(TestLayoutDocumentEditor, AddWidgetToEmptyCellSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"], "children": [] }
	})");

	auto result = addWidget(doc, "/root", 1, 0, "widget_a", makeTestRegistry());
	EXPECT_TRUE(result.ok) << result.error;
	ASSERT_EQ(doc["root"]["children"].size(), 1u);
	EXPECT_EQ(doc["root"]["children"][0]["id"], "widget_a_1");
	EXPECT_EQ(doc["root"]["children"][0]["widget"], "widget_a");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, AddWidgetGeneratesAUniqueIdEvenWhenTheBaseNameCollides)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "widget_a_1", "col": 0, "row": 0 } ]
		}
	})");

	auto result = addWidget(doc, "/root", 1, 0, "widget_a", makeTestRegistry());
	EXPECT_TRUE(result.ok) << result.error;
	EXPECT_EQ(doc["root"]["children"][1]["id"], "widget_a_2");
}

TEST_F(TestLayoutDocumentEditor, AddWidgetToOccupiedCellFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");
	json before = doc;

	auto result = addWidget(doc, "/root", 0, 0, "widget_a", makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, AddSecondSingletonWidgetFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_singleton", "id": "s1", "col": 0, "row": 0 } ]
		}
	})");
	json before = doc;

	auto result = addWidget(doc, "/root", 1, 0, "widget_singleton", makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, AddWidgetWithUnknownIdFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr"], "rows": ["1fr"], "children": [] }
	})");
	json before = doc;

	auto result = addWidget(doc, "/root", 0, 0, "does_not_exist", makeTestRegistry());
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

// --- addTrack / removeTrack / setTrackSize ---

TEST_F(TestLayoutDocumentEditor, AddTrackAppendsWithoutRenumberingChildren)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");

	auto result = addTrack(doc, "/root", true, "2fr");
	EXPECT_TRUE(result.ok) << result.error;
	ASSERT_EQ(doc["root"]["cols"].size(), 2u);
	EXPECT_EQ(doc["root"]["cols"][1], "2fr");
	EXPECT_EQ(doc["root"]["children"][0]["col"], 0);
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, AddTrackWithInvalidTokenFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr"], "rows": ["1fr"], "children": [] }
	})");
	json before = doc;

	auto result = addTrack(doc, "/root", true, "nonsense");
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, RemoveUnoccupiedTrackRenumbersLaterChildren)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr", "1fr"], "rows": ["1fr"],
			"children": [
				{ "widget": "widget_a", "id": "a1", "col": 0, "row": 0 },
				{ "widget": "widget_a", "id": "a2", "col": 2, "row": 0 }
			]
		}
	})");

	auto result = removeTrack(doc, "/root", true, 1);
	EXPECT_TRUE(result.ok) << result.error;
	ASSERT_EQ(doc["root"]["cols"].size(), 2u);
	EXPECT_EQ(doc["root"]["children"][0]["col"], 0);
	EXPECT_EQ(doc["root"]["children"][1]["col"], 1);
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, RemoveOccupiedTrackFailsAndLeavesDocUnchanged)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": {
			"type": "grid", "cols": ["1fr", "1fr"], "rows": ["1fr"],
			"children": [ { "widget": "widget_a", "id": "a1", "col": 0, "row": 0 } ]
		}
	})");
	json before = doc;

	auto result = removeTrack(doc, "/root", true, 0);
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, RemoveLastRemainingTrackFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr"], "rows": ["1fr"], "children": [] }
	})");
	json before = doc;

	auto result = removeTrack(doc, "/root", true, 0);
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

TEST_F(TestLayoutDocumentEditor, SetTrackSizeSucceeds)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr"], "rows": ["1fr"], "children": [] }
	})");

	auto result = setTrackSize(doc, "/root", false, 0, "content");
	EXPECT_TRUE(result.ok) << result.error;
	EXPECT_EQ(doc["root"]["rows"][0], "content");
	EXPECT_TRUE(LayoutBuilder::validate(doc, makeTestRegistry()).ok);
}

TEST_F(TestLayoutDocumentEditor, SetTrackSizeWithInvalidTokenFails)
{
	json doc = json::parse(R"({
		"schema": 1,
		"root": { "type": "grid", "cols": ["1fr"], "rows": ["1fr"], "children": [] }
	})");
	json before = doc;

	auto result = setTrackSize(doc, "/root", false, 0, "nonsense");
	EXPECT_FALSE(result.ok);
	EXPECT_EQ(doc, before);
}

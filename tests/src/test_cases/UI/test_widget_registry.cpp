/*
 * test_widget_registry.cpp
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#include "UI/Layout/WidgetRegistry.h"
#include "test_utils/TestSuite.h"
#include <deque>
#include <gtest/gtest.h>

using namespace UI::Layout;

namespace
{
	// -Wmissing-field-initializers requires every field to be listed once any designated
	// initializer is used, so tests that don't care about a field still have to name it.
	WidgetDescriptor makeDescriptor(std::string_view id, std::function<bool()> available = nullptr)
	{
		return WidgetDescriptor{
			.id = id,
			.nameKey = {},
			.icon = {},
			.hint = {},
			.singleton = false,
			.create = nullptr,
			.available = std::move(available),
		};
	}
} // namespace

// Each test constructs its own local WidgetRegistry rather than touching WidgetRegistry::get(),
// so tests don't pollute the process-lifetime production singleton or each other.
class TestWidgetRegistry : public TestSuite
{
};

TEST_F(TestWidgetRegistry, FindOnEmptyRegistryReturnsNullptr)
{
	WidgetRegistry registry;
	EXPECT_EQ(registry.find("anything"), nullptr);
}

TEST_F(TestWidgetRegistry, AddThenFindReturnsTheDescriptor)
{
	WidgetRegistry registry;
	registry.add(makeDescriptor("widget_a"));

	const WidgetDescriptor* found = registry.find("widget_a");
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->id, "widget_a");
}

TEST_F(TestWidgetRegistry, FindUnknownIdReturnsNullptr)
{
	WidgetRegistry registry;
	registry.add(makeDescriptor("widget_a"));

	EXPECT_EQ(registry.find("widget_b"), nullptr);
}

TEST_F(TestWidgetRegistry, FieldsRoundTripThroughAdd)
{
	WidgetRegistry registry;
	bool availableCalled = false;
	registry.add({
		.id = "widget_a",
		.nameKey = "widget.a.name",
		.icon = "a.png",
		.hint = {.minCols = 2, .minRows = 3},
		.singleton = true,
		.create = [](const std::string&, UI::LvObj&, const nlohmann::json&) -> std::unique_ptr<UI::LvObj>
		{ return nullptr; },
		.available =
			[&availableCalled]()
		{
			availableCalled = true;
			return true;
		},
	});

	const WidgetDescriptor* found = registry.find("widget_a");
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->nameKey, "widget.a.name");
	EXPECT_EQ(found->icon, "a.png");
	EXPECT_EQ(found->hint.minCols, 2);
	EXPECT_EQ(found->hint.minRows, 3);
	EXPECT_TRUE(found->singleton);
	ASSERT_TRUE(static_cast<bool>(found->available));
	EXPECT_TRUE(found->available());
	EXPECT_TRUE(availableCalled);
}

TEST_F(TestWidgetRegistry, PointersStayValidAfterFurtherAdds)
{
	// m_descriptors is a std::deque specifically so this holds even across many add() calls.
	WidgetRegistry registry;
	registry.add(makeDescriptor("widget_a"));
	const WidgetDescriptor* first = registry.find("widget_a");
	ASSERT_NE(first, nullptr);

	// WidgetDescriptor::id is a non-owning string_view (production code only ever gives it string
	// literals), so these ids must outlive the registry - kept alive in `ids` for that reason.
	// std::deque, not std::vector: these are short strings, so a std::string holds their bytes
	// inline (SSO) - a std::vector could still relocate that storage on growth and dangle the
	// string_view already captured from an earlier iteration; std::deque never does.
	std::deque<std::string> ids;
	for (int i = 0; i < 50; i++)
	{
		ids.push_back(std::to_string(i));
		registry.add(makeDescriptor(ids.back()));
	}

	EXPECT_EQ(registry.find("widget_a"), first);
	EXPECT_EQ(first->id, "widget_a");
}

TEST_F(TestWidgetRegistry, AvailableWidgetsIncludesEntriesWithNoAvailableGate)
{
	WidgetRegistry registry;
	registry.add(makeDescriptor("widget_a"));

	auto available = registry.availableWidgets();
	ASSERT_EQ(available.size(), 1u);
	EXPECT_EQ(available[0]->id, "widget_a");
}

TEST_F(TestWidgetRegistry, AvailableWidgetsExcludesGatedFalseEntries)
{
	WidgetRegistry registry;
	registry.add(makeDescriptor("widget_a", []() { return true; }));
	registry.add(makeDescriptor("widget_b", []() { return false; }));

	auto available = registry.availableWidgets();
	ASSERT_EQ(available.size(), 1u);
	EXPECT_EQ(available[0]->id, "widget_a");
}

TEST_F(TestWidgetRegistry, AvailableWidgetsGateCanChangeBetweenCalls)
{
	WidgetRegistry registry;
	bool isAvailable = false;
	registry.add(makeDescriptor("widget_a", [&isAvailable]() { return isAvailable; }));

	EXPECT_EQ(registry.availableWidgets().size(), 0u);

	isAvailable = true;
	EXPECT_EQ(registry.availableWidgets().size(), 1u);
}

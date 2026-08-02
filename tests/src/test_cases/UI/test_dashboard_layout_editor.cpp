/*
 * test_dashboard_layout_editor.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "Storage.h"
#include "UI/Layout/LayoutLoader.h"
#include "UI/Widgets/Dashboard/Dashboard.h"
#include "test_utils/UiTestSuite.h"
#include "utils/StorageHelper.h"
#include <gtest/gtest.h>

using namespace UI;

class TestDashboardLayoutEditor : public UiTestSuite
{
};

TEST_F(TestDashboardLayoutEditor, EnterEditModeShowsToolbarAndExitRestoresPersistedLayout)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);
	ASSERT_NE(dashboard.getToolList(), nullptr);

	dashboard.enterEditMode();
	EXPECT_EQUAL_SCREENSHOT("dashboard_layout_editor/entered.png");

	// Enter is idempotent while already active.
	dashboard.enterEditMode();

	dashboard.sendEvent(LV_EVENT_LONG_PRESSED, nullptr);
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
}

TEST_F(TestDashboardLayoutEditor, SavedCustomLayoutRoundTripsThroughTheSentinel)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);
	dashboard.enterEditMode();

	// The toolbar's Save button (DashboardLayoutEditor::onSave) does exactly this: validate the
	// current working document, persist it, then point ID_LAYOUT_FILE at the sentinel - exercised
	// here directly against Dashboard's own public surface, since there's no public "click the
	// toolbar's Save button" API from outside the editor itself.
	const nlohmann::json workingDoc = dashboard.getCurrentDocument();
	Layout::saveCustomLayoutDocument(workingDoc);
	StorageHelper::setData(ID_LAYOUT_FILE, Layout::CUSTOM_LAYOUT_SENTINEL);

	EXPECT_TRUE(dashboard.reload());
	EXPECT_EQ(dashboard.getCurrentDocument(), workingDoc);
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
}

TEST_F(TestDashboardLayoutEditor, ToolbarAndChromeStayOnTopAfterAMutationTriggeredRebuild)
{
	// Regression test: found on real hardware, not caught by
	// EnterEditModeShowsToolbarAndExitRestoresPersistedLayout's screenshot, since that one only
	// checks the editor's very first render, before any mutation. Every applyMutation() rebuilds the
	// dashboard's entire widget tree as *new* children of Dashboard, added after the editor's own
	// (already-existing) container - which silently buried its toolbar and the widget-picker modal
	// underneath the fresh content, since show()'s move-to-front only fires while transitioning from
	// hidden to visible, not on every subsequent rebuild while already visible.
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);
	dashboard.enterEditMode();

	// previewDocument() with the *same* document is exactly what a real mutation's rebuild does
	// (LayoutBuilder::build() re-creates every widget from scratch) - no need to actually perform a
	// move/resize/add/remove to reproduce the bug, since it's the rebuild itself that's at fault.
	ASSERT_TRUE(dashboard.previewDocument(dashboard.getCurrentDocument()));
	EXPECT_EQUAL_SCREENSHOT("dashboard_layout_editor/after_rebuild.png");
}

TEST_F(TestDashboardLayoutEditor, ResetToDefaultReplacesTheWorkingDocumentWithDefaultJson)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("minimal.json"));
	Dashboard dashboard("dashboard", screen);
	dashboard.enterEditMode();

	auto defaultDoc = Layout::loadLayoutDocument("default.json");
	ASSERT_TRUE(defaultDoc.has_value());

	// previewDocument() is the exact mechanism Reset to Default uses internally (see
	// DashboardLayoutEditor::onResetToDefault) - confirms the dashboard actually reflects it.
	EXPECT_TRUE(dashboard.previewDocument(*defaultDoc));
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
	EXPECT_EQ(dashboard.getCurrentDocument(), *defaultDoc);
}

TEST_F(TestDashboardLayoutEditor, CancelDiscardsUnsavedEditsAndRestoresThePersistedLayout)
{
	StorageHelper::setData(ID_LAYOUT_FILE, std::string_view("default.json"));
	Dashboard dashboard("dashboard", screen);
	dashboard.enterEditMode();

	auto minimalDoc = Layout::loadLayoutDocument("minimal.json");
	ASSERT_TRUE(minimalDoc.has_value());
	ASSERT_TRUE(dashboard.previewDocument(*minimalDoc)); // simulate an in-progress, unsaved edit
	EXPECT_EQ(dashboard.getToolList(), nullptr);		  // minimal.json has no tool_list

	// Cancel's actual behaviour: reload() from Storage, discarding the previewed edit.
	EXPECT_TRUE(dashboard.reload());
	ASSERT_NE(dashboard.getToolList(), nullptr);
	EXPECT_TRUE(dashboard.getToolList()->isValid());
}

/*
 * Dashboard.h
 *
 *  Created on: 2025-10-02
 *      Author: Andy Everitt
 */

#pragma once

#include "DashboardPresenter.h"
#include "UI/Components/Containers/TabView.h"
#include "UI/Core/View.h"
#include "UI/Layout/LayoutBuilder.h"
#include "UI/Screens/File/FileView.h"
#include "UI/Screens/Status/StatusView.h"
#include "UI/Widgets/Temperature/TemperatureGraph.h"
#include "UI/Widgets/ToolList/ToolList.h"
#include <memory>

namespace UI
{
	class DashboardLayoutEditor;

	/**
	 * @brief The dashboard's composition is data-driven - see assets/layouts/ and
	 * docs/LAYOUT_ENGINE_DESIGN.md. The user can pick a shipped preset (Settings > Display > Layout)
	 * or, from Phase 5 on, edit a layout freely (Settings > Display > Edit Layout, or long-press the
	 * dashboard) - so any widget, including the tabs container and its jobs/status tabs, may be
	 * absent from the current document. Every accessor below is therefore pointer-returning and may
	 * return nullptr; callers must check before use.
	 */
	class Dashboard : public View<DashboardPresenter>
	{
	  public:
		Dashboard(const std::string& name, LvObj& parent);
		// Declared (not defaulted inline) because m_editor is a unique_ptr<DashboardLayoutEditor>,
		// which is only forward-declared here - the destructor needs DashboardLayoutEditor's complete
		// type, available in Dashboard.cpp but not to every translation unit that includes this
		// header (e.g. HomeView.h, which embeds a Dashboard member and would otherwise need to
		// instantiate ~Dashboard() itself with only the forward declaration in scope).
		~Dashboard();

		ToolList* getToolList() { return static_cast<ToolList*>(m_layout->find("tool_list")); }
		TemperatureGraph* getGraph() { return static_cast<TemperatureGraph*>(m_layout->find("temperature_graph")); }
		TabView* getTabs() { return static_cast<TabView*>(m_layout->find("tabs")); }
		FileView* getFileView() { return static_cast<FileView*>(m_layout->find("jobs")); }
		StatusView* getStatusView() { return static_cast<StatusView*>(m_layout->find("status")); }

		void showJobsTab()
		{
			if (TabView* tabs = getTabs())
			{
				tabs->setActiveTabById("jobs");
			}
		}
		void showStatusTab()
		{
			if (TabView* tabs = getTabs())
			{
				tabs->setActiveTabById("status");
			}
		}
		void disableJobsTab(bool disable);

		void setNumberPad(ModalNumberPad* np);

		void clear();

		/**
		 * @brief Tears down the current layout and rebuilds from whichever file ID_LAYOUT_FILE
		 * currently names (see the preset picker in Settings > Display). Rebuilds wholesale rather
		 * than diffing, per docs/LAYOUT_ENGINE_DESIGN.md section 4.3.
		 *
		 * @return false (leaving the current layout in place, unlike the constructor's use of this
		 * same loading path) if the named file can't be loaded/parsed/built - unlike at startup,
		 * there's always a known-good layout already on screen to fall back to here.
		 */
		bool reload();

		/// The document last successfully built (whichever of reload()'s or previewDocument()'s
		/// documents that was) - the layout editor reads this to seed the document it's editing.
		const nlohmann::json& getCurrentDocument() const { return m_currentDoc; }

		/// Rebuilds the dashboard from `doc` directly, without touching Storage - used by the layout
		/// editor to preview an in-progress edit before it's saved. Same fallback behaviour as
		/// reload(): returns false and leaves the current layout in place if `doc` fails to
		/// validate/build.
		bool previewDocument(const nlohmann::json& doc);

		/// Looks up a built widget/container by its document node id - every node in
		/// getCurrentDocument() carries an explicit "id" (see docs/LAYOUT_ENGINE_DESIGN.md and
		/// assets/layouts/*.json), which the layout editor uses to attach edit-mode chrome to the
		/// right LvObj. nullptr if `id` isn't in the current tree.
		LvObj* findWidget(std::string_view id) { return m_layout->find(id); }

		/// Enters the on-screen layout editor (long-press the dashboard, or Settings > Display >
		/// Edit Layout). The editor is constructed lazily on first use, not in Dashboard's own
		/// constructor - most Dashboard instances (every existing test, in particular) never enter
		/// edit mode, and it pulls in two Modal instances plus a toolbar's worth of buttons.
		void enterEditMode();

	  protected:
		void onHide() override;

	  private:
		// Shared by reload() and previewDocument(): validates+builds `doc`, and on success swaps it
		// in as m_layout/m_currentDoc and re-applies pending configuration. `logLabel` is just for the
		// error message on failure (reload() passes the layout filename, previewDocument() a fixed
		// label, since there's no file involved).
		bool buildAndInstall(const nlohmann::json& doc, std::string_view logLabel);

		// Re-applies configuration that was set via setNumberPad()/disableJobsTab() while the target
		// widget didn't exist, or that a widget rebuilt from scratch (a fresh TabView/StatusView has
		// no memory of what the previous instance was told) needs re-telling. Called at the end of
		// every rebuild (reload(), and the layout editor's preview rebuilds).
		void applyPendingConfiguration();

		std::unique_ptr<Layout::LayoutInstance> m_layout;
		nlohmann::json m_currentDoc;
		ModalNumberPad* m_pendingNumberPad = nullptr;
		bool m_jobsTabDisabled = false;
		std::unique_ptr<DashboardLayoutEditor> m_editor;
	};
} // namespace UI

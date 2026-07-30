/*
 * TabView.h
 *
 *  Created on: 2025-11-06
 *      Author: Andy Everitt
 */

#pragma once

#include "UI/Components/Button/Button.h"
#include "UI/Components/LVGL/LvContainer.h"
#include "UI/Components/List/List.h"
#include <optional>
#include <string>
#include <vector>

namespace UI
{
	class TabView : public LvContainer
	{
	  public:
		using TabButton = Button;

		TabView(const std::string& name, LvObj& parent);

		/**
		 * @param id Stable identifier for the tab, independent of its (translatable) display name.
		 *           Not required to be unique, but @ref getTabIndexById only ever returns the first
		 *           match. Omit for tabs that will only ever be addressed by index.
		 */
		LvContainer& addTab(std::string_view tab_name, std::string_view id = {});
		bool renameTab(size_t index, std::string_view new_name);

		LvContainer& getActiveTab();
		TabButton* getTabButton(size_t index);
		LvContainer* getTab(size_t index);
		size_t getTabCount() const { return m_tabs.size(); };
		size_t getActiveTabIndex() const { return m_currentTabIndex; };

		/**
		 * @return The index of the first tab added with the given stable id, or std::nullopt if no
		 *         tab has that id. Does not log - absence is an expected outcome for callers.
		 */
		std::optional<size_t> getTabIndexById(std::string_view id) const;

		void setActiveTab(size_t index);
		void setTabBarPosition(lv_dir_t dir);

		bool disableTab(size_t index, bool disable);

		/**
		 * @return false (and logs) if no tab has the given id, otherwise the result of setActiveTab.
		 */
		bool setActiveTabById(std::string_view id);

		/**
		 * @return false (and logs) if no tab has the given id, otherwise the result of disableTab.
		 */
		bool disableTabById(std::string_view id, bool disable);

	  private:
		List<TabButton> m_tabButtons{"tab_buttons", getRoot()};
		LvContainer m_tabContent{"tab_content", getRoot()};

		std::vector<std::unique_ptr<LvContainer>> m_tabs;
		std::vector<std::string> m_tabIds; // parallel to m_tabs; empty entry means the tab has no stable id
		size_t m_currentTabIndex = -1;
	};
} // namespace UI

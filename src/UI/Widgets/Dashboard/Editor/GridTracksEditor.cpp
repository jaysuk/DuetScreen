/*
 * GridTracksEditor.cpp
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#include "GridTracksEditor.h"
#include "UI/Components/Button/Button.h"
#include "UI/Components/Containers/Row.h"
#include "UI/Components/LVGL/LvLabel.h"
#include "i18n/i18n.h"
#include <fmt/format.h>

namespace UI
{
	GridTracksEditor::GridTracksEditor(const std::string& name, LvObj& parent)
		: LvContainer(name, parent)
		, m_columnsSection("columns_section", *this)
		, m_rowsSection("rows_section", *this)
	{
		ZoneScoped;
		setFlexFlow(LV_FLEX_FLOW_COLUMN);
		setSize(LV_PCT(80), LV_PCT(70));

		m_columnsSection.setFlexFlow(LV_FLEX_FLOW_COLUMN);
		m_columnsSection.setSize(LV_PCT(100), LV_SIZE_CONTENT);
		m_rowsSection.setFlexFlow(LV_FLEX_FLOW_COLUMN);
		m_rowsSection.setSize(LV_PCT(100), LV_SIZE_CONTENT);
	}

	void GridTracksEditor::setTracks(const std::vector<TrackRow>& columns, const std::vector<TrackRow>& rows)
	{
		ZoneScoped;
		UI_LOCK();
		// Destroying each wrapper (via its own destructor) tears down its underlying LVGL object too -
		// matches List<T>::clear()'s own reasoning. Nothing else should be done to the section
		// containers directly: they have no children left once every wrapper that owned one is gone.
		m_rowWidgets.clear();

		addAxisSection(_("layout_editor.columns"), true, columns);
		addAxisSection(_("layout_editor.rows"), false, rows);
	}

	void GridTracksEditor::addAxisSection(std::string_view title, bool isColumn, const std::vector<TrackRow>& tracks)
	{
		ZoneScoped;
		LvContainer& section = isColumn ? m_columnsSection : m_rowsSection;

		auto heading = std::make_unique<LvLabel>("heading", section);
		heading->setText(title);
		m_rowWidgets.push_back(std::move(heading));

		for (size_t i = 0; i < tracks.size(); i++)
		{
			const TrackRow& track = tracks[i];
			const int index = static_cast<int>(i);

			auto row = std::make_unique<Row>(fmt::format("track_{}", i), section);
			row->setSize(LV_PCT(100), LV_SIZE_CONTENT);

			auto label = std::make_unique<LvLabel>("label", *row);
			label->setText(fmt::format("{}: {}", i, track.sizeToken));
			label->setFlexGrow(1);

			auto cycleBtn = std::make_unique<Button>("cycle", *row, _("layout_editor.cycle_size"));
			cycleBtn->addClickedCallback(
				[this, isColumn, index](lv_event_t*)
				{
					if (m_cycleCb)
					{
						m_cycleCb(isColumn, index);
					}
				});

			auto removeBtn = std::make_unique<Button>("remove", *row, _("common.remove"));
			removeBtn->setDisabled(track.occupied || tracks.size() <= 1);
			removeBtn->addClickedCallback(
				[this, isColumn, index](lv_event_t*)
				{
					if (m_removeCb)
					{
						m_removeCb(isColumn, index);
					}
				});

			m_rowWidgets.push_back(std::move(label));
			m_rowWidgets.push_back(std::move(cycleBtn));
			m_rowWidgets.push_back(std::move(removeBtn));
			m_rowWidgets.push_back(std::move(row));
		}

		auto addBtn = std::make_unique<Button>("add", section, _("layout_editor.add_track"));
		addBtn->addClickedCallback(
			[this, isColumn](lv_event_t*)
			{
				if (m_addCb)
				{
					m_addCb(isColumn);
				}
			});
		m_rowWidgets.push_back(std::move(addBtn));
	}
} // namespace UI

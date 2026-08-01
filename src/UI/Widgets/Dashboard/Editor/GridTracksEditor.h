/*
 * GridTracksEditor.h
 *
 *  Created on: 2026-08-01
 *      Author: Jay S
 */

#pragma once

#include "UI/Components/LVGL/LvContainer.h"
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace UI
{
	/**
	 * @brief Manages a grid's row/column *structure* (docs/LAYOUT_ENGINE_DESIGN.md section 4.2's
	 * "cols"/"rows" arrays) - opened from the layout editor's toolbar rather than given in-place
	 * per-track handles, since accurately positioning a handle at each track's actual boundary would
	 * mean reproducing LVGL's own fr/content/%/px track-resolution math just for chrome placement.
	 * A plain list sidesteps that entirely: each row shows the track's current size token with a
	 * "cycle size" button and a "remove" button (hidden when a widget currently occupies that track),
	 * plus an "add" button per axis. Every button just reports back to the owner - this class never
	 * mutates the document itself, matching how the rest of the editor's chrome is purely presentational.
	 */
	class GridTracksEditor : public LvContainer
	{
	  public:
		struct TrackRow
		{
			std::string sizeToken;
			bool occupied = false; // removal disabled when true
		};

		GridTracksEditor(const std::string& name, LvObj& parent);

		/// Rebuilds the displayed rows from scratch - call again after any mutation to reflect it.
		void setTracks(const std::vector<TrackRow>& columns, const std::vector<TrackRow>& rows);

		/// `isColumn` distinguishes which axis's `index`'th track was acted on.
		void setCycleSizeCallback(std::function<void(bool isColumn, int index)> cb) { m_cycleCb = std::move(cb); }
		void setRemoveCallback(std::function<void(bool isColumn, int index)> cb) { m_removeCb = std::move(cb); }
		void setAddCallback(std::function<void(bool isColumn)> cb) { m_addCb = std::move(cb); }

	  private:
		void addAxisSection(std::string_view title, bool isColumn, const std::vector<TrackRow>& tracks);

		LvContainer m_columnsSection;
		LvContainer m_rowsSection;
		// std::deque so addresses stay stable across repeated setTracks() calls while earlier rows
		// might still be mid-callback (matches the module's general convention elsewhere).
		std::deque<std::unique_ptr<LvObj>> m_rowWidgets;

		std::function<void(bool, int)> m_cycleCb;
		std::function<void(bool, int)> m_removeCb;
		std::function<void(bool)> m_addCb;
	};
} // namespace UI

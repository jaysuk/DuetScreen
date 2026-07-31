/*
 * LayoutBuilder.h
 *
 *  Created on: 2026-07-31
 *      Author: Jay S
 */

#pragma once

#include "UI/Layout/WidgetRegistry.h"
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace UI::Layout
{
	/**
	 * @brief Owns every LvObj a LayoutBuilder::build() call created, plus a lookup from each node's
	 * layout id to its LvObj.
	 *
	 * @note Destroying this destroys the entire built tree - LayoutBuilder rebuilds wholesale
	 * rather than diffing (see docs/LAYOUT_ENGINE_DESIGN.md section 4.3), so callers replace a
	 * LayoutInstance outright rather than mutating one in place.
	 */
	class LayoutInstance
	{
	  public:
		LvObj* getRoot() const { return m_root; }

		/// Returns the LvObj for the given node id, or nullptr if no node in this instance has it.
		LvObj* find(std::string_view id) const;

	  private:
		friend class LayoutBuilder;

		LvObj* m_root = nullptr;
		// Every node this instance owns, in creation order (containers before the children built
		// under them). std::deque so LvObj addresses stay stable as more nodes are added.
		std::deque<std::unique_ptr<LvObj>> m_owned;
		// Grid track-descriptor arrays: LvObj::setGridDsc stores the raw pointer it's given rather
		// than copying it (see lv_obj_set_grid_dsc_array), and holds onto it for as long as the
		// grid container exists - not just for the duration of the setGridDsc() call. std::deque,
		// not std::vector: a vector reallocating on a *later* grid's push_back would relocate an
		// *earlier* grid's std::vector<int32_t> element, dangling the pointer LVGL is still
		// holding for that earlier grid even though it was never touched again. A deque never
		// moves existing elements as more are appended.
		std::deque<std::vector<int32_t>> m_gridTrackStorage;
		std::unordered_map<std::string, LvObj*> m_byId;
	};

	/**
	 * @brief Builds a LayoutInstance from a JSON layout document. See
	 * docs/LAYOUT_ENGINE_DESIGN.md section 4.2/4.3 for the schema and design this implements.
	 */
	class LayoutBuilder
	{
	  public:
		struct ValidationResult
		{
			bool ok = false;
			std::vector<std::string> errors;
		};

		/**
		 * @brief Checks a layout document for structural problems - unknown schema version,
		 * unknown widget ids, a singleton widget placed more than once, out-of-range or
		 * overlapping grid cells, a container (rather than a widget) placed inside a tabs node -
		 * without building anything. Collects every problem found rather than stopping at the
		 * first, so a caller can report everything wrong at once.
		 */
		static ValidationResult validate(const nlohmann::json& doc, const WidgetRegistry& registry = WidgetRegistry::get());

		/**
		 * @brief Validates `doc`, and if it passes, builds it under `parent`.
		 *
		 * @param errorsOut If non-null and validation fails, receives the validation errors.
		 * @return nullptr if `doc` fails validation - a bad layout must never leave the caller with
		 * a partially-built tree. The caller is responsible for falling back to a known-good
		 * document; this function does not invent one.
		 */
		static std::unique_ptr<LayoutInstance> build(
			const nlohmann::json& doc,
			LvObj& parent,
			const WidgetRegistry& registry = WidgetRegistry::get(),
			std::vector<std::string>* errorsOut = nullptr);

		/// Parses a grid track-size token ("3fr", "content", "120px", "50%") into an LVGL grid
		/// track descriptor value. Returns false (leaving `out` unchanged) if `token` isn't valid.
		static bool parseTrackSize(std::string_view token, int32_t& out);

		/// Parses a cell-alignment token ("stretch", "start", "center", "end"). Returns false
		/// (leaving `out` unchanged) if `token` isn't one of those four.
		static bool parseAlign(std::string_view token, lv_grid_align_t& out);

	  private:
		// A member (rather than a free function) specifically so it can reach LayoutInstance's
		// private members via the friend declaration below.
		//
		// `isRoot` sizes the freshly-created node to LV_PCT(100) x LV_PCT(100) immediately after
		// creation, before its own layout/children are configured - a layout document describes
		// everything that goes in the space it's given, so the root has no independent "natural
		// size" to leave unset. This has to happen *before* children/grid cells are configured, not
		// as an afterthought once the whole subtree already exists: a grid's cell placement is
		// resolved against whatever size the container has *at the time*, and a container's default
		// size (absent an explicit one) is a small fixed default, not 100% of its parent - sizing
		// it only after the fact produced a dashboard rendered into a tiny corner of the screen
		// during testing, not full width/height.
		static LvObj* buildNode(
			const nlohmann::json& node,
			LvObj& parent,
			const std::string& path,
			LayoutInstance& instance,
			const WidgetRegistry& registry,
			bool isRoot = false);
	};
} // namespace UI::Layout

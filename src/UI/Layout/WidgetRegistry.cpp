/*
 * WidgetRegistry.cpp
 *
 *  Created on: 2026-07-30
 *      Author: Jay S
 */

#include "WidgetRegistry.h"
#include "Debug.h"

namespace UI::Layout
{
	WidgetRegistry& WidgetRegistry::get()
	{
		static WidgetRegistry instance;
		return instance;
	}

	void WidgetRegistry::add(WidgetDescriptor descriptor)
	{
		ZoneScoped;
		if (find(descriptor.id) != nullptr)
		{
			LOG_FATAL_THROW("Widget with id '{:s}' already registered", descriptor.id);
			return;
		}
		m_descriptors.push_back(std::move(descriptor));
	}

	const WidgetDescriptor* WidgetRegistry::find(std::string_view id) const
	{
		ZoneScoped;
		for (const auto& descriptor : m_descriptors)
		{
			if (descriptor.id == id)
			{
				return &descriptor;
			}
		}
		return nullptr;
	}

	std::vector<const WidgetDescriptor*> WidgetRegistry::availableWidgets() const
	{
		ZoneScoped;
		std::vector<const WidgetDescriptor*> result;
		result.reserve(m_descriptors.size());
		for (const auto& descriptor : m_descriptors)
		{
			if (!descriptor.available || descriptor.available())
			{
				result.push_back(&descriptor);
			}
		}
		return result;
	}
} // namespace UI::Layout

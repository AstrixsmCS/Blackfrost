#pragma once

#include "Event.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

using EventListenerID = uint64_t;

// Deferred, subscription-based event broadcasting.
//
// Publish() queues events and Process() dispatches them.
// Events published during Process() are dispatched by the next Process().
//
// This implementation is single-threaded and should only be accessed
// from the main thread.

class EventBus
{
public:
	template<std::derived_from<Event> TEvent, typename TCallback>
		requires std::invocable<TCallback&, TEvent&>
	static EventListenerID Subscribe(TCallback&& callback)
	{
		auto& bus = Get();

		Listener listener;
		listener.ID       = bus.m_NextID++;
		listener.Callback = [callback = std::forward<TCallback>(callback)](Event& event) mutable
		{
			callback(static_cast<TEvent&>(event));
		};

		const EventListenerID id = listener.ID;
		bus.AddListener(typeid(TEvent), std::move(listener));

		return id;
	}

	template<std::derived_from<Event> TEvent>
	static void Unsubscribe(EventListenerID id)
	{
		Get().RemoveListener(typeid(TEvent), id);
	}

	template<std::derived_from<Event> TEvent, typename... TArguments>
	static void Publish(TArguments&&... arguments)
	{
		PendingEvent event{
			.Type  = typeid(TEvent),
			.Value = std::make_unique<TEvent>(std::forward<TArguments>(arguments)...)
		};

		Get().m_PendingEvents.push_back(std::move(event));
	}

	static void Process()
	{
		auto& bus = Get();

		assert(!bus.m_Dispatching && "EventBus::Process() called from inside an event callback");

		std::swap(bus.m_PendingEvents, bus.m_ProcessingEvents);

		bus.m_Dispatching = true;

		for (PendingEvent& event : bus.m_ProcessingEvents)
			bus.Dispatch(event.Type, *event.Value);

		bus.m_Dispatching = false;

		bus.m_ProcessingEvents.clear();
		bus.ApplyDeferredChanges();
	}

	static void Clear()
	{
		auto& bus = Get();

		assert(!bus.m_Dispatching && "EventBus::Clear() called from inside an event callback");

		bus.m_Listeners.clear();
		bus.m_PendingAdds.clear();
		bus.m_PendingEvents.clear();
		bus.m_HasRemovals = false;
	}

private:
	struct Listener
	{
		EventListenerID             ID = 0;
		std::function<void(Event&)> Callback;
		bool                        Removed = false;
	};

	struct PendingEvent
	{
		std::type_index        Type;
		std::unique_ptr<Event> Value;
	};

	static EventBus& Get()
	{
		static EventBus instance;
		return instance;
	}

	void AddListener(std::type_index type, Listener&& listener)
	{
		if (m_Dispatching)
		{
			m_PendingAdds.emplace_back(type, std::move(listener));
			return;
		}

		m_Listeners[type].push_back(std::move(listener));
	}

	void RemoveListener(std::type_index type, EventListenerID id)
	{
		std::erase_if(m_PendingAdds, [id](const auto& pending)
		{
			return pending.second.ID == id;
		});

		const auto iterator = m_Listeners.find(type);

		if (iterator == m_Listeners.end())
			return;

		std::vector<Listener>& listeners = iterator->second;

		const auto listener = std::ranges::find(listeners, id, &Listener::ID);

		if (listener == listeners.end())
			return;

		if (m_Dispatching)
		{
			listener->Removed = true;
			m_HasRemovals     = true;
			return;
		}

		listeners.erase(listener);

		if (listeners.empty())
			m_Listeners.erase(iterator);
	}

	void Dispatch(std::type_index type, Event& event)
	{
		const auto iterator = m_Listeners.find(type);

		if (iterator == m_Listeners.end())
			return;

		for (const Listener& listener : iterator->second)
		{
			if (event.m_Handled)
				break;

			if (!listener.Removed)
				listener.Callback(event);
		}
	}

	void ApplyDeferredChanges()
	{
		if (m_HasRemovals)
		{
			for (auto iterator = m_Listeners.begin(); iterator != m_Listeners.end();)
			{
				std::erase_if(iterator->second, [](const Listener& listener)
				{
					return listener.Removed;
				});

				iterator = iterator->second.empty() ? m_Listeners.erase(iterator) : std::next(iterator);
			}

			m_HasRemovals = false;
		}

		for (auto& [type, listener] : m_PendingAdds)
			m_Listeners[type].push_back(std::move(listener));

		m_PendingAdds.clear();
	}

private:
	std::unordered_map<std::type_index, std::vector<Listener>> m_Listeners;
	std::vector<std::pair<std::type_index, Listener>>          m_PendingAdds;

	std::vector<PendingEvent> m_PendingEvents;
	std::vector<PendingEvent> m_ProcessingEvents;

	EventListenerID m_NextID      = 1;
	bool            m_Dispatching = false;
	bool            m_HasRemovals = false;
};

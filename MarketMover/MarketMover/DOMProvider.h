#pragma once

#include "pch.h"
#include "Defs.h"
#include "IDOMProvider.h"

namespace Mover
{

class DOMProvider : public IDOMProvider
{
public:
	DOMProvider()
		: _notificationThread([this](std::stop_token st) { NotifyConsumers(st); })
	{
	}

	void Subscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.insert(consumer);
	}
	void Unsubscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.erase(consumer);
	}
	DOMDescription GetDOM(Level levels) const override
	{
		return {};
	}
	BBADescription GetBBA() const override
	{
		return {};
	}

private:
	void NotifyConsumers(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			try
			{
				std::scoped_lock lock(_mutex);
				for (auto consumer : _consumers)
				{
					if (consumer)
						consumer->OnDOM();
				}
			}
			catch (const std::exception& exc)
			{
				std::cerr << "Exception in DOM notification thread: " << exc.what() << std::endl;
			}
		}
	}

private:
	std::mutex _mutex;
	std::unordered_set<IDOMConsumer*> _consumers;
	std::jthread _notificationThread;
};

}

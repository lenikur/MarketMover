#pragma once

#include "Defs.h"
#include "IDOMProvider.h"

namespace Mover
{

class DOMProvider : public IDOMProvider
{
public:
	void Subscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.push_back(consumer);
	}
	void Unsubscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.erase(std::remove(_consumers.begin(), _consumers.end(), consumer), _consumers.end());
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
	std::mutex _mutex;
	std::vector<IDOMConsumer*> _consumers;
};

}

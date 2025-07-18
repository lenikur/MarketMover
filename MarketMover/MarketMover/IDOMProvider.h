#pragma once

#include "Defs.h"

namespace Mover
{

struct Request
{
	int64_t id{};
	Volume volume{};
	int64_t time{};
};

struct VolumeDescription
{
	Volume volume{};
	std::vector<Request> requests;
};

struct DOMDescription
{
	std::map<Price, VolumeDescription> asks;
	std::map<Price, VolumeDescription, std::greater<Price>> bids;
};

struct BBADescription
{
	Price bestAsk{};
	Volume bestAskVolume{};
	Price bestBid{};
	Volume bestBidVolume{};
};

struct IDOMConsumer
{
	virtual ~IDOMConsumer() = default;
	virtual void OnDOM() = 0;
};

struct IDOMProvider
{
	virtual ~IDOMProvider() = default;
	virtual void Subscribe(IDOMConsumer* consumer) = 0;
	virtual void Unsubscribe(IDOMConsumer* consumer) = 0;
	virtual DOMDescription GetDOM(Level levels) const = 0;
	virtual BBADescription GetBBA() const = 0;
};

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

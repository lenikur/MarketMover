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
};

void DisplayDOM(const DOMDescription& dom)
{
	PLOG_INFO << "Asks:";
	for (auto it = dom.asks.rbegin(); it != dom.asks.rend(); ++it)
	{
		PLOG_INFO << "Price: " << it->first << ", Volume: " << it->second.volume;
	}

	PLOG_INFO << "Bids:";
	for (const auto& [price, desc] : dom.bids)
	{
		PLOG_INFO << "Price: " << price << ", Volume: " << desc.volume;
	}
	PLOG_INFO << "------------------------";
}
}
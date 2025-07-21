#pragma once

#include "Defs.h"
#include "IDOMProvider.h"

namespace Mover
{

/// @brief A simple market analyzer that estimates the probability of price movements based on the DOM
class MarketAnalyzer
{
public:
	double Estimate(MovingDirection direction) const
	{
		DOMDescription dom;
		{
			std::scoped_lock lock(_mutex);
			dom = _dom;
		}

		static const Level analyzeLevelsCount{ 2 };
		
		Volume totalAsk{};
		Volume totalBid{};

		auto askIt = dom.asks.begin();
		auto bidIt = dom.bids.begin();
		for (Level level{}; askIt != dom.asks.end() && level < analyzeLevelsCount; ++level, ++askIt, ++bidIt)
		{
			{
				totalAsk += askIt->second.volume;
				totalBid += bidIt->second.volume;
			}
		}

		const auto totalVolume{ totalAsk + totalBid };
		if (totalVolume == 0)
			return 0;

		if (direction == MovingDirection::Down)
			return static_cast<double>(totalAsk) / totalVolume;
		else
			return static_cast<double>(totalBid) / totalVolume;
	}

	void OnDOM(const DOMDescription& dom)
	{
		std::scoped_lock lock(_mutex);
		_dom = dom;
	}

private:
	mutable std::mutex _mutex;
	DOMDescription _dom;
};

}

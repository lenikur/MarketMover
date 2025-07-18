#pragma once

#include "Defs.h"
#include "IDOMProvider.h"

namespace Mover
{

class MarketAnalyzer
{
public:
	double Estimate(MovingDirection direction)
	{
		// Provides a market estimation, a probability of the price movements success
		return 0.5;
	}

	void OnDOM(const DOMDescription& dom)
	{
		// DOM changes analyzing
	}
};

}

#pragma once

#include "Defs.h"

namespace Mover
{

struct Config
{
	Instrument instrument{ "PUMA" };
	Commission commissionInCents{ 0 }; // Broker commission in cents
	Volume initialCapitalInCents{ 3'000'000'000 };
	// MarketAnalyzer estimates probability of the market movement to required direction. 
	// This threshold defines minimal probability of success that allows to the strategy performing.
	double probabilityOfSuccessThreshold{ 0.4 }; 
	// Percentage of initial capital that will be used for spoofing orders
	double spoofingPercentage{ 0.8 }; 
	// Number of spoofing orders to place
	size_t spoofingOrderCount{ 2 };
	// Interval between ignition orders placing in milliseconds
	std::chrono::milliseconds ignitionInterval{ 1000 };
	// Required moving direction of the market
	MovingDirection movingDirection{ MovingDirection::Up };
	// Level of the market moving, it defines how many levels will be moved by the strategy
	Level movingLevel{ 1 };
	// Level of the market moving for analysis, it defines how many levels will be analyzed by the strategy
	Level domLevelsForAnalysis{ 5 };
};

}

#pragma once

#include "Defs.h"

namespace Mover
{

struct Config
{
	Instrument instrument{ "PUMA" };
	Commission commission{ 0 };
	Volume initialCapitalInCents{ 3'000'000'000 };
	double probabilityOfSuccess{ 0.5 };
	double spoofingPercentage{ 0.8 };
	size_t spoofingOrderCount{ 2 };
	std::chrono::milliseconds ignitionInterval{ 100 };
	MovingDirection movingDirection{ MovingDirection::Up };
	Level movingLevel{ 1 };
	Level domLevelsForAnalysis{ 5 };
};

}

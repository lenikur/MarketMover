#pragma once

#include "Defs.h"

namespace Mover
{

struct IStrategyConsumer
{
	virtual ~IStrategyConsumer() = default;
	virtual void OnStrategyResult() = 0;
};

struct IStrategy
{
	virtual ~IStrategy() = default;
	virtual void Stop() = 0;
};

}

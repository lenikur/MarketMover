#pragma once

#include "Defs.h"
#include "MarketMoverStrategy.h"
#include "Config.h"
#include "InstrumentProvider.h"

namespace Mover
{

class Manager : public IStrategyConsumer
{
	Price CalculateGoalPrice(Config config)
	{
		const auto bba{ _domProvider.GetBBA() };
		const auto instrumentInfo{ _instrumentProvider.GetInstrumentInfo(config.instrument) };

		return config.movingDirection == MovingDirection::Up ?
			bba.bestAsk + instrumentInfo.tickSize * config.movingLevel :
			bba.bestBid - instrumentInfo.tickSize * config.movingLevel;
	}

public:
	explicit Manager(Config config)
		: _strategy(*this, config, CalculateGoalPrice(config), _instrumentProvider.GetInstrumentInfo(config.instrument), _domProvider, _orderManager)
	{
	}

	void WaitForCompletion()
	{
		std::unique_lock<std::mutex> lock(_doneMutex);
		_doneSignal.wait(lock, [this] { return _done; });
	}

	void OnStrategyResult() override
	{
		std::cout << "Strategy result received." << std::endl;

		{
			std::scoped_lock lock{ _doneMutex };
			_done = true;
		}

		_doneSignal.notify_all();
	}

private:
	std::mutex _doneMutex;
	std::condition_variable _doneSignal;
	bool _done{ false };

	OrderManager _orderManager;
	DOMProvider _domProvider;
	InstrumentProvider _instrumentProvider;
	MarketMoverStrategy _strategy;
};

}

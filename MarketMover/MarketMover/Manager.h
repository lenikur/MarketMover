#pragma once

#include "Defs.h"
#include "MarketMoverStrategy.h"
#include "Config.h"
#include "InstrumentProvider.h"
#include "DOMProvider.h"
#include "OrderManager.h"

namespace Mover
{

/// @brief The class bootstraps the Market Mover strategy.
class Manager : public IStrategyConsumer
{
	Price CalculateGoalPrice(Config config)
	{
		const auto bba{ _domProvider.GetDOM(1) };
		const auto instrumentInfo{ _instrumentProvider.GetInstrumentInfo(config.instrument) };

		return config.movingDirection == MovingDirection::Up ?
			bba.bids.begin()->first + instrumentInfo.tickSize * config.movingLevel :
			bba.asks.begin()->first - instrumentInfo.tickSize * config.movingLevel;
	}

public:
	explicit Manager(Config config)
		: _domProvider(config.movingDirection, 5, _instrumentProvider.GetInstrumentInfo(config.instrument).tickSize, 1000, 999, 1000)
		, _strategy(*this, config, CalculateGoalPrice(config), _instrumentProvider.GetInstrumentInfo(config.instrument), _domProvider, _orderManager)
	{
	}

	void WaitForCompletion()
	{
		std::unique_lock<std::mutex> lock(_doneMutex);
		_doneSignal.wait(lock, [this] { return _done; });
	}

	void OnStrategyResult() override
	{
		{
			std::scoped_lock lock{ _doneMutex };
			_done = true;
		}

		PLOG_INFO << "Strategy result received. Remaining budget in cents: " << _strategy.GetBudget();

		_doneSignal.notify_all();
	}

private:
	std::mutex _doneMutex;
	std::condition_variable _doneSignal;
	bool _done{ false };

	InstrumentProvider _instrumentProvider;
	OrderManager _orderManager;
	DOMProvider _domProvider;
	MarketMoverStrategy _strategy;
};

}

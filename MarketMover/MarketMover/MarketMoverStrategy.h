#pragma once

#include "Defs.h"
#include "IStrategy.h"
#include "Config.h"
#include "IInstrumentProvider.h"
#include "IDOMProvider.h"
#include "IOrderManager.h"
#include "MarketAnalyzer.h"
#include "SpoofOrderManager.h"

namespace Mover
{

class MarketMoverStrategy
	: public IStrategy
	, public IDOMConsumer
	, public IOrderConsumer
{
public:
	MarketMoverStrategy(IStrategyConsumer& strategyConsumer, Config config, Price goalPrice, const InstrumentInfo instrumentInfo, IDOMProvider& domProvider, IOrderManager& orderManager)
		: _strategyConsumer{ strategyConsumer }
		, _config{ std::move(config) }
		, _goalPrice{ goalPrice }
		, _instrumentInfo{ instrumentInfo }
		, _domProvider{ domProvider }
		, _orderManager{ orderManager }
		, _spoofer{ GetSideFromDirection(_config.movingDirection), _orderManager, _instrumentInfo,
			static_cast<Budget>(_config.initialCapitalInCents * _config.spoofingPercentage), _config.spoofingOrderCount }
		, _ignitionBudget{ static_cast<Budget>(_config.initialCapitalInCents - static_cast<Budget>(_config.initialCapitalInCents * _config.spoofingPercentage)) }
	, _ignitionThread{ ([this](std::stop_token st) { Ignite(st); }) }
	{
		_domProvider.Subscribe(this);
	}

	~MarketMoverStrategy() override
	{
		Stop();
	}

	void Stop() override
	{
		std::cout << "Stopping strategy..." << std::endl;

		_domProvider.Unsubscribe(this);

		if (_ignitionThread.joinable())
		{
			_ignitionThread.request_stop();
			_ignitionThread.join();
		}
	}

private:
	bool IsGoalReached(const DOMDescription& dom)
	{
		const auto market{ GetBestPrice(dom) };
		return (_config.movingDirection == MovingDirection::Up && market >= _goalPrice) ||
			(_config.movingDirection == MovingDirection::Down && market <= _goalPrice);
	}

	void OnDOM() override
	{
		const auto dom{ _domProvider.GetDOM(_config.domLevelsForAnalysis) };

		if (IsGoalReached(dom))
		{
			_done.store(true, std::memory_order_release);
			_orderManager.CancelAll();
			_strategyConsumer.OnStrategyResult();

			return;
		}

		_spoofer.OnDOM(dom);
		_analyzer.OnDOM(dom);

		if (_spoofer.IsFullyLoaded())
			return;

		if (_analyzer.Estimate(_config.movingDirection) < _config.probabilityOfSuccess)
			return;

		_spoofer.PlaceOrder(dom);
	}

	void OnOrderChange(const Order& order) override
	{
		if (order.type != OrderType::Market || order.instrument != _config.instrument)
		{
			return;
		}

		if (order.status == OrderStatus::Rejected || order.status == OrderStatus::Canceled)
		{
			std::scoped_lock lock{ _mutex };
			_ignitionBudget += (order.volume * order.price + _config.commission);
		}
	}

	static Budget CalculateBudget(Volume volume, Price price, Commission commission)
	{
		return (volume * price) + commission;
	}

	Price GetBestPrice(const BBADescription& bba) const
	{
		return _config.movingDirection == MovingDirection::Up ? bba.bestBid : bba.bestAsk;
	}

	Price GetBestPrice(const DOMDescription& dom) const
	{
		return _config.movingDirection == MovingDirection::Up ? dom.bids.begin()->first : dom.asks.begin()->first;
	}

	void Ignite(std::stop_token st)
	{
		std::cout << "Ignition thread started." << std::endl;

		while (!st.stop_requested() && !_done.load(std::memory_order_acquire))
		{
			try
			{
				std::this_thread::sleep_for(_config.ignitionInterval);

				if (_analyzer.Estimate(_config.movingDirection) >= _config.probabilityOfSuccess)
					continue;

				const auto bba{ _domProvider.GetBBA() };
				const auto requiredBudget{ CalculateBudget(_instrumentInfo.minimalVolume, GetBestPrice(bba), _config.commission) };

				{
					std::scoped_lock lock{ _mutex };

					if (_ignitionBudget < requiredBudget)
					{
						std::cout << "Not enough budget to ignite. Required: " << requiredBudget << ", Available: " << _ignitionBudget << std::endl;
						return; // not enough budget to ignite
					}

					_ignitionBudget -= requiredBudget;
				}

				Order order
				{
					.instrument = _instrumentInfo.instrument,
					.side = GetSideFromDirection(_config.movingDirection),
					.type = OrderType::Market,
					.volume = _instrumentInfo.minimalVolume,
					.time = std::chrono::system_clock::now().time_since_epoch().count()
				};

				if (_done.load(std::memory_order_acquire))
				{
					std::cout << "Strategy is done, stopping ignition." << std::endl;
					return;
				}

				_orderManager.PlaceOrder(order);
			}
			catch (const std::exception& exc)
			{
				// TODO: add metric
				std::cout << "Exception in ignition thread: " << exc.what() << std::endl;
			}
		}
	}

private:
	IStrategyConsumer& _strategyConsumer;
	const Config _config;
	const Price _goalPrice{};
	const InstrumentInfo _instrumentInfo;
	IDOMProvider& _domProvider;
	IOrderManager& _orderManager;
	MarketAnalyzer _analyzer;
	SpoofOrderManager _spoofer;
	std::mutex _mutex;
	Budget _ignitionBudget{ 0 };
	std::atomic_bool _done{ false };
	std::jthread _ignitionThread;
};

}

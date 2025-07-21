#pragma once

#include "Defs.h"
#include "IStrategy.h"
#include "Config.h"
#include "IInstrumentProvider.h"
#include "IDOMProvider.h"
#include "OrderManager.h"
#include "MarketAnalyzer.h"
#include "SpoofOrderManager.h"

namespace Mover
{

/// @brief The Market Mover strategy is designed to move the market price in a specified direction
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
		PLOG_INFO << "Goal price: " << _goalPrice;
		_domProvider.Subscribe(this);
		_orderManager.Subscribe(this);
	}

	~MarketMoverStrategy() override
	{
		Stop();
	}

	void Stop() override
	{
		PLOG_INFO << "Stopping strategy..." ;

		_domProvider.Unsubscribe(this);

		if (_ignitionThread.joinable())
		{
			_ignitionThread.request_stop();
			_ignitionThread.join();
		}

		_orderManager.Unsubscribe(this);
	}

	Budget GetBudget() const
	{
		const auto budget{ _spoofer.GetRemainingBudget() };
		std::scoped_lock lock{ _mutex };
		return _ignitionBudget + budget;
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

		PLOG_INFO << "DOM updated" ;

		if (IsGoalReached(dom))
		{
			PLOG_INFO << "Goal reached. Market: " << GetBestPrice(dom) << ", Goal: " << _goalPrice ;

			_done.store(true, std::memory_order_release);
			_spoofer.StopSync();
			_strategyConsumer.OnStrategyResult();

			return;
		}

		_spoofer.OnDOM(dom);
		_analyzer.OnDOM(dom);

		if (_spoofer.IsFullyLoaded())
		{
			PLOG_INFO << "Spoof orders are fully loaded, not placing new spoof orders." ;
			return;
		}

		const auto analyzerResult{ _analyzer.Estimate(_config.movingDirection) };
		if (analyzerResult < _config.probabilityOfSuccessThreshold)
		{
			PLOG_INFO << "Analyzer doesn't recommend to place spoof orders: " << analyzerResult ;
			return;
		}

		PLOG_INFO << "Analyzer recommends to place spoof orders: " << analyzerResult;
		_spoofer.PlaceOrder(dom);
	}

	void OnOrderChange(const Order& order) override
	{
		if (order.type == OrderType::Limit)
		{
			_spoofer.OnOrderChange(order);
			return;
		}

		if (order.status == OrderStatus::Rejected || order.status == OrderStatus::Canceled)
		{
			std::scoped_lock lock{ _mutex };
			_ignitionBudget += (order.volume * order.price + _config.commissionInCents);
		}
	}

	static Budget CalculateBudget(Volume volume, Price price, Commission commissionInCents)
	{
		return (volume * price) + commissionInCents;
	}

	Price GetBestPrice(const DOMDescription& dom) const
	{
		return _config.movingDirection == MovingDirection::Up ? dom.bids.begin()->first : dom.asks.begin()->first;
	}

	void Ignite(std::stop_token st)
	{
		PLOG_INFO << "Ignition thread started." ;

		while (!st.stop_requested() && !_done.load(std::memory_order_acquire))
		{
			try
			{
				std::this_thread::sleep_for(_config.ignitionInterval);

				const auto analyzerResult{ _analyzer.Estimate(_config.movingDirection) };

				if (analyzerResult < _config.probabilityOfSuccessThreshold)
				{
					PLOG_INFO << "Analyzer doesn't recommend to move: " << analyzerResult ;
					continue;
				}

				PLOG_INFO << "Analyzer recommends to move: " << analyzerResult ;

				const auto bba{ _domProvider.GetDOM(1) };
				const auto requiredBudget{ CalculateBudget(_instrumentInfo.minimalVolume, GetBestPrice(bba), _config.commissionInCents) };

				{
					std::scoped_lock lock{ _mutex };

					if (_ignitionBudget < requiredBudget)
					{
						PLOG_INFO << "Not enough budget to ignite. Required: " << requiredBudget << ", Available: " << _ignitionBudget ;
						return; // not enough budget to ignite
					}

					_ignitionBudget -= requiredBudget;

					PLOG_INFO << "Ignition budget available: " << _ignitionBudget << ", Required: " << requiredBudget;
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
					PLOG_INFO << "Strategy is done, stopping ignition." ;
					return;
				}

				_orderManager.PlaceOrder(order);
			}
			catch (const std::exception& exc)
			{
				// TODO: add metric
				PLOG_INFO << "Exception in ignition thread: " << exc.what() ;
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
	mutable std::mutex _mutex;
	Budget _ignitionBudget{ 0 };
	std::atomic_bool _done{ false };
	std::jthread _ignitionThread;
};

}

// MarketMover.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>
#include <stop_token>
#include <chrono>

namespace Mover
{

using Budget = int64_t;
using Volume = int64_t;
using Price = int64_t;
using Instrument = std::string;
using OrderId = int64_t;
using Level = int32_t;

enum class OrderSide { Buy, Sell };
enum class OrderType { Limit, Market };
enum class OrderStatus { InTransit, Rejected, InModify, Canceled, Filled };

struct Config
{
	Instrument instrument{ "PUMA" };
	Volume initialCapitalInCents{ 3'000'000'000 };
	double probabilityOfSuccess{ 0.5 };
	double spoofingPercentage{ 0.6 };
	OrderSide movingSide{ OrderSide::Buy };
	Level movingLevel{ 1 };
};

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
	std::map<Price, VolumeDescription> bids;
};

struct IDOMConsumer
{
	virtual ~IDOMConsumer() = default;
	virtual void OnDOM() = 0;
};

struct IDOMProvider
{
	virtual ~IDOMProvider() = default;
	virtual void Subscribe(std::shared_ptr<IDOMConsumer> consumer) = 0;
	virtual void Unsubscribe(std::shared_ptr<IDOMConsumer> consumer) = 0;
	virtual DOMDescription GetDOM() const = 0;
};

struct Tick
{
	Price bestAsk{};
	Price bestBid{};
	Volume bestAskVolume{};
	Volume bestBidVolume{};
	int64_t time{};
};

struct ITickConsumer
{
	virtual ~ITickConsumer() = default;
	virtual void OnTick(const Tick& tick) = 0;
};

struct ITickProvider
{
	virtual ~ITickProvider() = default;
	virtual void Subscribe(const Instrument& instrument, std::shared_ptr<ITickConsumer> consumer) = 0;
	virtual void Unsubscribe(std::shared_ptr<ITickConsumer> consumer) = 0;
	virtual Tick GetTick() const = 0;
};

class DOMProviderFactory
{
public:
	std::shared_ptr<IDOMProvider> CreateDOMProvider(const Instrument& instrument)
	{
		// Implementation of factory method to create a DOM provider
		return nullptr;
	}
};

class TickProviderFactory
{
public:
	std::shared_ptr<ITickProvider> CreateTickProvider(const Instrument& instrument)
	{
		// Implementation of factory method to create a tick provider
		return nullptr;
	}
};

struct Order
{
	OrderId id{};
	Instrument instrument;
	OrderSide side{};
	OrderType type{};
	Price price{};
	Volume volume{};
	OrderStatus status{ OrderStatus::InTransit };
	int64_t time{};
};;

struct IOrderConsumer
{
	virtual ~IOrderConsumer() = default;
	virtual void OnOrderChange(const Order& order) = 0;
};

struct IOrderManager
{
	virtual ~IOrderManager() = default;
	virtual void PlaceOrder(const Order& order) = 0;
	virtual void ModifyOrder(const Order& order) = 0;
	virtual void CancelOrder(OrderId orderId) = 0;
	virtual void Subscribe(std::shared_ptr<IOrderConsumer> consumer) = 0;
	virtual void Unsubscribe(std::shared_ptr<IOrderConsumer> consumer) = 0;
};

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

class MarketAnalyzer
{
public:
	double Estimate(const DOMDescription& dom)
	{
		return 0.5;
	}
};

class Spoofer
	: public IOrderConsumer
	, public std::enable_shared_from_this<Spoofer>
{
public:
	Spoofer(IOrderManager& orderManager, OrderSide side)
		: _orderManager{ orderManager }
		, _side{ side }
	{
		_orderManager.Subscribe(shared_from_this());
	}

	~Spoofer() override
	{
		_orderManager.Unsubscribe(shared_from_this());
	}

	void PlaceOrder(Volume orderVolume, const DOMDescription& dom)
	{
		const auto safePrice{ CalculateSafePrice(dom) };
		
		if (!safePrice.has_value())
		{
			std::cerr << "No safe price found for the order." << std::endl;
			return;
		}
		
		Order order
		{
			.instrument = "PUMA",
			.side = _side,
			.type = OrderType::Limit,
			.price = *safePrice,
			.volume = orderVolume,
			.time = std::chrono::system_clock::now().time_since_epoch().count()
		};
		
		_orderManager.PlaceOrder(order);
		_orders[order.id] = order; // Store the order for tracking
	}

	void OnDOM(const DOMDescription& dom)
	{
		// keep orders safe from fills
	}

	virtual void OnOrderChange(const Order& order) override
	{
	}

private:
	std::optional<Price> CalculateSafePrice(const DOMDescription& dom)
	{
		if (_side == OrderSide::Buy)
		{
			if (dom.bids.size() > _safeLevel)
				return std::next(dom.bids.rbegin(), _safeLevel)->first;
		}
		else
		{
			if (dom.bids.size() > _safeLevel)
				return std::next(dom.asks.begin(), _safeLevel)->first;
		}

		return std::nullopt; 
	}

private:
	Level _safeLevel{ 2 };
	IOrderManager& _orderManager;
	OrderSide _side;
	std::unordered_map<OrderId, Order> _orders;
};

class MarketMoverStrategy 
	: public IStrategy
	, public IDOMConsumer
	, public ITickConsumer
	, public std::enable_shared_from_this<MarketMoverStrategy>
{
public:
	MarketMoverStrategy(Config config, DOMProviderFactory& domProviderFactory, TickProviderFactory& tickProviderFactory, IOrderManager& orderManager)
		: _config{ std::move(config) }
		, _spoofBudget{ _config.initialCapitalInCents / 2 }
		, _igintionBudget{ _config.initialCapitalInCents - _spoofBudget }
		, _domProviderFactory{ domProviderFactory }
		, _tickProviderFactory{ tickProviderFactory } 
		, _orderManager{ orderManager }
		, _starterThread{ [this](std::stop_token st) { Starter(st); } }
	{
	}

	~MarketMoverStrategy() override
	{
		Stop();
	}

	void Stop() override
	{
		if (_starterThread.joinable())
		{
			_starterThread.request_stop();
			_starterThread.join();
		}

		_isRunning.store(false, std::memory_order_release);

		if (_domProvider)
		{
			_domProvider->Unsubscribe(shared_from_this());
			_domProvider.reset();
		}

		if (_tickProvider)
		{
			_tickProvider->Unsubscribe(shared_from_this());
			_tickProvider.reset();
		}
	}

private:

	void OnDOM() override
	{
		if (!_isRunning.load(std::memory_order_acquire))
			return;

		const auto& dom{ _domProvider->GetDOM() };

		if (CheckIfGoalReached(dom))
		{
			std::cout << "Goal reached, stopping strategy." << std::endl;
			Stop();
			// notify consumers about success
			return;
		}

		_spoofer->OnDOM(dom);
	}

	bool CheckIfGoalReached(const DOMDescription& dom)
	{
		return false; // check _goalPrice against current DOM
	}

	std::optional<Price> CaclulateGoalPrice(const DOMDescription& dom) const
	{
		if (dom.asks.size() < _config.movingLevel || dom.bids.size() < _config.movingLevel)
		{
			std::cerr << "Not enough levels in DOM to calculate goal price." << std::endl;
			return std::nullopt; // or throw an exception
		}

		return (_config.movingSide == OrderSide::Buy)
			? std::next(dom.bids.rbegin(), _config.movingLevel)->first
			: std::next(dom.asks.begin(), _config.movingLevel)->first;
	}

	void Starter(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			const auto& dom{ _domProvider->GetDOM() };
			const auto goalPrice{ CaclulateGoalPrice(dom) };
			if (!goalPrice.has_value())
			{
				return; // notify consumers about failure
			}
			_goalPrice = *goalPrice;
			const auto probability{ _analyzer.Estimate(dom) };
			if (probability >= _config.probabilityOfSuccess)
			{
				_isRunning.store(true, std::memory_order_release);
				OnStart();
				
				return;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void OnStart()
	{
		_domProvider = _domProviderFactory.CreateDOMProvider(_config.instrument);
		_domProvider->Subscribe(shared_from_this());

		_tickProvider = _tickProviderFactory.CreateTickProvider(_config.instrument);
		_tickProvider->Subscribe(_config.instrument, shared_from_this());

		_spoofer = std::make_shared<Spoofer>(_orderManager);

		_spoofThread = std::jthread([this](std::stop_token st) { Spoof(st); });
		_ignitionThread = std::jthread([this](std::stop_token st) { Iginite(st); });
	}

	void Spoof(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			if (!_isRunning.load(std::memory_order_acquire))
				return;

			const auto dom{ _domProvider->GetDOM() };
			const auto volume{ CaclulateSpoofVolume(dom) };
			if (volume != 0)
				_spoofer->PlaceOrder(volume, dom);

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void Iginite(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			if (!_isRunning.load(std::memory_order_acquire))
				return;

			// Place small orders for ignition

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	Volume CaclulateSpoofVolume(const DOMDescription& dom)
	{
		return 1; // smart strategy considers current DOM state and the budget
	}

private:
	Config _config;
	std::atomic<Budget> _spoofBudget{ 0 };
	std::atomic<Budget> _igintionBudget{ 0 };
	std::atomic_bool _isRunning{ false };

	DOMProviderFactory& _domProviderFactory;
	TickProviderFactory& _tickProviderFactory;
	IOrderManager& _orderManager;

	std::jthread _starterThread;

	std::shared_ptr<IDOMProvider> _domProvider;
	std::shared_ptr<ITickProvider> _tickProvider;
	MarketAnalyzer _analyzer;
	std::shared_ptr<Spoofer> _spoofer;
	std::jthread _spoofThread;
	std::jthread _ignitionThread;
	Price _goalPrice{};
};

}

int main()
{
    std::cout << "Hello World!\n";
}

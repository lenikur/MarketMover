#pragma once

#include "Defs.h"
#include "IOrderManager.h"

namespace Mover
{

class SpoofOrderManager : public IOrderConsumer
{
public:
	explicit SpoofOrderManager(OrderSide side, IOrderManager& orderManager, InstrumentInfo instrumentInfo, Budget budget, size_t orderCount)
		: _orderManager{ orderManager }
		, _side{ side }
		, _instrumentInfo{ instrumentInfo }
		, _budget{ budget }
		, _orderCount{ orderCount }
	{
		if (_budget == 0 || _orderCount == 0)
		{
			throw std::runtime_error("Budget and order count must be greater than zero");
		}

		_orderManager.Subscribe(this);
	}

	~SpoofOrderManager() override
	{
		_orderManager.Unsubscribe(this);
	}

	void CancelAllOrders()
	{
		std::scoped_lock lock{ _mutex };
		for (const auto& order : _orders | std::views::values)
		{
			_orderManager.CancelOrder(order.id);
		}
	}

	bool IsFullyLoaded() const
	{
		std::scoped_lock lock{ _mutex };
		return _orderCount == 0;
	}

	void PlaceOrder(const DOMDescription& dom)
	{
		if (IsFullyLoaded())
			return;

		if (dom.asks.empty() || dom.bids.empty())
		{
			// TODO: add metric
			throw std::runtime_error("DOM is empty");
		}

		std::scoped_lock lock{ _mutex };

		const auto safePrice{ CalculateSafePrice(dom) };
		
		const auto budget{ _budget / _orderCount };
		const auto orderVolume{ CaclulateVolume(safePrice, budget, _commission)};

		if (orderVolume == 0)
			return;

		const auto trailingOffset{ (_side == OrderSide::Sell ? safePrice - dom.asks.begin()->first : dom.bids.begin()->first - safePrice) / _instrumentInfo.tickSize };

		if (trailingOffset < 0)
		{
			// TODO: add metric
			throw std::runtime_error("Trailing offset is negative");
		}
	
		Order order
		{
			.instrument = _instrumentInfo.instrument,
			.side = _side,
			.type = OrderType::Limit,
			.price = safePrice,
			.volume = orderVolume,
			.trailingOffset = trailingOffset,
			.time = std::chrono::system_clock::now().time_since_epoch().count()
		};
		
		_orderManager.PlaceOrder(order);

		_budget -= budget;
		--_orderCount;
	}

	void OnDOM(const DOMDescription& dom)
	{
		if (dom.asks.empty() || dom.bids.empty())
		{
			// TODO: add metric
			return;
		}

		// keeping safe spoof orders, it prevents them from being filled
		const auto safePrice{ CalculateSafePrice(dom) };

		std::scoped_lock lock{ _mutex };

		if (_side == OrderSide::Sell)
		{
			const auto itEnd = _orders.lower_bound(safePrice);
			for (auto it = _orders.begin(); it != itEnd;)
			{
				it->second.price = safePrice;
				_orderManager.ModifyOrder(it->second);
				_orders.insert({ safePrice, it->second });
				it = _orders.erase(it);
			}
		}
		else
		{
			for (auto it = _orders.upper_bound(safePrice); it != _orders.end(); ++it)
			{
				it->second.price = safePrice;
				_orderManager.ModifyOrder(it->second);
				_orders.insert({ safePrice, it->second });
				it = _orders.erase(it);
			}
		}
	}

	virtual void OnOrderChange(const Order& order) override
	{
		if (order.type != OrderType::Limit || order.instrument != _instrumentInfo.instrument)
		{
			return;
		}

		std::scoped_lock lock{ _mutex };
		if (order.status == OrderStatus::Filled || order.status == OrderStatus::Canceled)
		{
			// TODO: optimize search by using a map with OrderId as key
			for (auto it = _orders.begin(); it != _orders.end(); ++it)
			{
				if (it->second.id == order.id)
				{
					_orders.erase(it);
					break;
				}
			}
		}
		else if (order.status == OrderStatus::Modified)
		{
			// TODO: optimize search by using a map with OrderId as key
			for (auto it = _orders.begin(); it != _orders.end(); ++it)
			{
				if (it->second.id == order.id)
				{
					_orders.erase(it);
					_orders.insert({ order.price, order });
					break;
				}
			}
		}
		else if (order.status == OrderStatus::Rejected)
		{
			// TODO: add metric
			std::cout << "Order rejected: " << order.id << std::endl;
		}
		else if (order.status == OrderStatus::Placed)
		{
			_orders.insert({ order.price, order });
		}
	}

private:

	Price CalculateSafePrice(const DOMDescription& dom)
	{
		// Calculation of the safe price (where spoof orders affects market, but have a small chance to be filled)
		// Naive implementation
		const Level safeLevel{ 2 };

		if (_side == OrderSide::Buy)
		{
			if (dom.bids.size() > safeLevel)
				return std::next(dom.bids.begin(), safeLevel)->first;
			else
				return std::prev(dom.bids.end())->first;
		}
		else
		{
			if (dom.asks.size() > safeLevel)
				return std::next(dom.asks.begin(), safeLevel)->first;
			else
				return std::prev(dom.asks.end())->first;
		}
	}

	Volume CaclulateVolume(Price price, Budget budget, Commission commission)
	{
		return budget - commission > 0 ? (budget - commission) / price : 0;
	}

private:
	IOrderManager& _orderManager;
	OrderSide _side;
	InstrumentInfo _instrumentInfo;
	Budget _budget{ 0 };
	size_t _orderCount{ 0 };
	const Commission _commission{ 0 };
	mutable std::mutex _mutex;
	std::multimap<Price, Order> _orders;
};

}

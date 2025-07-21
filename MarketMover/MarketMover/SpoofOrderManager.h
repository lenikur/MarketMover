#pragma once

#include "Defs.h"
#include "IOrderManager.h"

namespace Mover
{

/// @brief The class is responsible for managing spoof orders.
/// It places limit orders and keeps them out of filling.
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
	}

	~SpoofOrderManager() override
	{
	}

	void StopSync()
	{
		if (CancelAllOrders() != 0)
		{
			std::unique_lock lock{ _mutex };
			_cv.wait(lock, [this] { return _orders.size() == 0; });
		}

		PLOG_INFO << "All spoof orders canceled.";
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

		const auto safePrice{ CalculateSafePrice(dom) };
		Budget budget{};

		{
			std::scoped_lock lock{ _mutex };
			if (_budget == 0 || _orderCount == 0)
			{
				PLOG_INFO << "No budget or orders left to place.";
				return;
			}
			budget = _budget / _orderCount;
			_budget -= budget;
			--_orderCount;
		}
		
		const auto orderVolume{ CaclulateVolume(safePrice, budget, _commissionInCents)};

		if (orderVolume == 0)
			return;

		Order order
		{
			.instrument = _instrumentInfo.instrument,
			.side = _side,
			.type = OrderType::Limit,
			.price = safePrice,
			.volume = orderVolume,
			.time = std::chrono::system_clock::now().time_since_epoch().count()
		};
		
		_orderManager.PlaceOrder(order);

		PLOG_INFO << "Spoof order placed: " << order.id << "; Price: " << order.price << "; Volume: " << order.volume;
	}

	Budget GetRemainingBudget() const
	{
		std::scoped_lock lock{ _mutex };
		return _budget;
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

		PLOG_INFO << "Order change received: " << order.id << "; Status: " << static_cast<int>(order.status)
			<< "; Price: " << order.price << "; Volume: " << order.volume;

		std::scoped_lock lock{ _mutex };
		if (order.status == OrderStatus::Filled || order.status == OrderStatus::Canceled)
		{
			for (auto it = _orders.begin(); it != _orders.end(); ++it)
			{
				if (it->second.id == order.id)
				{
					_orders.erase(it);
					if (order.status == OrderStatus::Canceled)
					{
						PLOG_INFO << "Order canceled: " << order.id;
						_budget += (order.volume * order.price + _commissionInCents);

						if (_orders.empty())
						{
							_cv.notify_all();
						}
					}
					break;
				}
			}
		}
		else if (order.status == OrderStatus::Modified)
		{
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
			PLOG_INFO << "Order rejected: " << order.id ;
		}
		else if (order.status == OrderStatus::Placed)
		{
			_orders.insert({ order.price, order });
		}
	}

private:

	size_t CancelAllOrders()
	{
		std::multimap<Price, Order> orders;
		{
			std::scoped_lock lock{ _mutex };
			orders = _orders;
		}

		for (const auto& order : orders | std::views::values)
		{
			_orderManager.CancelOrder(order);
		}

		return orders.size();
	}

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

	Volume CaclulateVolume(Price price, Budget budget, Commission commissionInCents)
	{
		return budget - commissionInCents > 0 ? (budget - commissionInCents) / price : 0;
	}

private:
	IOrderManager& _orderManager;
	const OrderSide _side;
	InstrumentInfo _instrumentInfo;
	Budget _budget{ 0 };
	size_t _orderCount{ 0 };
	const Commission _commissionInCents{ 0 };
	mutable std::mutex _mutex;
	std::multimap<Price, Order> _orders;
	std::condition_variable _cv;
};

}

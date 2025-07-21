#pragma once

#include "IOrderManager.h"

namespace Mover
{

/// @brief The class is responsible for managing orders in the system.
class OrderManager : public IOrderManager
{
public:
	OrderManager()
		: _notificationThread([this](std::stop_token st) { NotifyConsumers(st); })
	{
		PLOG_INFO << "OrderManager initialized.";
	}

	~OrderManager()
	{
		if (_notificationThread.joinable())
		{
			_notificationThread.request_stop();
			_cv.notify_all();
		}
	}

	void PlaceOrder(const Order& order) override
	{
		PLOG_INFO << "Placing order: " << order.id << "; " << order.price << "; " << order.volume ;

		Order newOrder{ order };
		newOrder.id = std::chrono::system_clock::now().time_since_epoch().count(); // simple ID generation based on time
		newOrder.status = OrderStatus::Placed;

		{
			std::scoped_lock lock{ _mutex };
			_orders.push_back(newOrder);
		}

		_cv.notify_all();
	}
	void ModifyOrder(const Order& order) override
	{
		PLOG_INFO << "Modifying order: " << order.id << "; " << order.price << "; " << order.volume ;

		Order newOrder{ order };
		newOrder.status = OrderStatus::Modified;

		{
			std::scoped_lock lock{ _mutex };
			_orders.push_back(newOrder);
		}

		_cv.notify_all();
	}
	void CancelOrder(const Order& order) override
	{
		PLOG_INFO << "Canceling order: " << order.id << "; " << order.price << "; " << order.volume;

		Order newOrder{ order };
		newOrder.status = OrderStatus::Canceled;

		{
			std::scoped_lock lock{ _mutex };
			_orders.push_back(newOrder);
		}

		_cv.notify_all();
	}
	void Subscribe(IOrderConsumer* consumer) override
	{
		std::scoped_lock lock{ _mutex };
		_consumers.insert(consumer);
		PLOG_INFO << "Subscribed consumer to order updates.";
	}
	void Unsubscribe(IOrderConsumer* consumer) override
	{
		std::scoped_lock lock{ _mutex };
		_consumers.erase(consumer);
		PLOG_INFO << "Unsubscribed consumer from order updates.";
	}

private:
	void NotifyConsumers(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			try
			{
				std::unique_lock lock{ _mutex };
				if (!_cv.wait(lock, st, [this] { return !_orders.empty(); }))
					return;

				for (const auto& order : _orders)
				{
					for (auto consumer : _consumers)
					{
						if (consumer)
							consumer->OnOrderChange(order); // bad practice to call under lock, but for simplicity
					}
				}

				_orders.clear(); // clear after notifying consumers
			}
			catch (const std::exception& exc)
			{
				PLOG_ERROR << "Exception in DOM notification thread: " << exc.what();
			}
		}
	}

private:
	mutable std::mutex _mutex;
	std::condition_variable_any _cv;
	std::deque<Order> _orders;
	std::unordered_set<IOrderConsumer*> _consumers;
	std::jthread _notificationThread;

};

}

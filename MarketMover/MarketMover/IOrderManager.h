#pragma once

#include "Defs.h"

namespace Mover
{

struct Order
{
	OrderId id{};
	Instrument instrument;
	OrderSide side{};
	OrderType type{};
	Price price{};
	Volume volume{};
	OrderStatus status{};
	std::optional<size_t> trailingOffset;
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
	virtual void CancelAll() = 0;
	virtual void Subscribe(IOrderConsumer* consumer) = 0;
	virtual void Unsubscribe(IOrderConsumer* consumer) = 0;
};

class OrderManager : public IOrderManager
{
public:
	void PlaceOrder(const Order& order) override
	{
		// Implementation of placing an order
		std::cout << "Placing order: " << order.id << std::endl;
	}
	void ModifyOrder(const Order& order) override
	{
		// Implementation of modifying an order
		std::cout << "Modifying order: " << order.id << std::endl;
	}
	void CancelOrder(OrderId orderId) override
	{
		// Implementation of canceling an order
		std::cout << "Canceling order: " << orderId << std::endl;
	}
	void CancelAll() override
	{
		// Implementation of canceling all orders
		std::cout << "Canceling all orders." << std::endl;
	}
	void Subscribe(IOrderConsumer* consumer) override
	{
		// Implementation of subscribing to order changes
	}
	void Unsubscribe(IOrderConsumer* consumer) override
	{
		// Implementation of unsubscribing from order changes
	}
};

}

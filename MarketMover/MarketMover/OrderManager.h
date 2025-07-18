#pragma once

#include "IOrderManager.h"

namespace Mover
{

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

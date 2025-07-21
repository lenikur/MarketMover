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
	virtual void CancelOrder(const Order& order) = 0;
	virtual void Subscribe(IOrderConsumer* consumer) = 0;
	virtual void Unsubscribe(IOrderConsumer* consumer) = 0;
};

}

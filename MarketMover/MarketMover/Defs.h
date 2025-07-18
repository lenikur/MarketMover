#pragma once

namespace Mover
{

using Budget = int64_t;
using Volume = int64_t;
using Price = int64_t;
using TickSize = int64_t;
using Instrument = std::string;
using OrderId = int64_t;
using Level = int32_t;
using Commission = int64_t;

enum class OrderSide { Buy, Sell };
enum class OrderType { Limit, Market };
enum class OrderStatus { Placed, Modified, Canceled, Filled, Rejected };
enum class MovingDirection { Up, Down };

OrderSide GetSideFromDirection(MovingDirection direction)
{
	return direction == MovingDirection::Up ? OrderSide::Buy : OrderSide::Sell;
}

}

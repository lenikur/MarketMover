Market Mover is a solution written in C++ using MS Visual Studio.

The solution goal is to implement a sample of strategy that moves a market to defined direction (Up or Down) using a defined budget.
The strategy uses huge limit orders and small market orders.
The idea is placing of limit orders having huge volume that make DOM looks like the market is interested in a movement to defined direction. 
The small market orders executed very often make impression of the market reaction to DOM.
These limit orders shouldn't be filled, they are just make impression. So the strategy cares about it.

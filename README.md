Market Mover is a solution written in C++ using MS Visual Studio.

The solution goal is to implement a sample strategy that moves a market in a defined direction (Up or Down) using a specified budget.

The strategy uses huge limit orders and small market orders.

The idea is to place limit orders with huge volume that make DOM look like the market is interested in a movement in the defined direction. 

The small market orders are executed very often and give an impression of the market reaction to DOM.

These limit orders shouldn't be filled; they are just making an impression. So the strategy cares about it.
<img width="1252" height="740" alt="marketmover" src="https://github.com/user-attachments/assets/cc2066b9-8177-4137-91bd-d3fcde11451e" />

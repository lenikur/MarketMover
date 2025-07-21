#pragma once

#include "pch.h"

#include "Defs.h"
#include "IDOMProvider.h"
#include "IInstrumentProvider.h"

namespace Mover
{

/// @brief The class is responsible for providing a simulated Depth of Market (DOM) history.
class DOMProvider : public IDOMProvider
{
public:
	DOMProvider(MovingDirection movingDirection, Level levels, TickSize tickSize, Price bestAsk, Price bestBid, Volume volume)
		: _dom{ GenerateDOMHistory(movingDirection, levels, _instrumentInfo.tickSize, bestAsk, bestBid, 1000) }
		, _notificationThread([this](std::stop_token st) { NotifyConsumers(st); })
	{
	}

	void Subscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.insert(consumer);
		PLOG_INFO << "Subscribed consumer to DOM updates.";
	}

	void Unsubscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.erase(consumer);
		PLOG_INFO << "Unsubscribed consumer from DOM updates.";
	}

	DOMDescription GetDOM(Level levels) const override
	{
		return *std::next(_dom.begin(), _callsCount);
	}

private:
	void NotifyConsumers(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			try
			{
				std::scoped_lock lock(_mutex);
				for (auto consumer : _consumers)
				{
					if (consumer)
						consumer->OnDOM(); // bad practice to call under lock, but for simplicity
				}

				if ((_callsCount + 1) < _dom.size())
					++_callsCount;
			}
			catch (const std::exception& exc)
			{
				PLOG_ERROR << "Exception in DOM notification thread: " << exc.what();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // simulate periodic updates
		}
	}

	static std::deque<DOMDescription> GenerateDOMHistory(MovingDirection movingDirection, Level levels, TickSize tickSize, Price bestAsk, Price bestBid, Volume volume)
	{
		static size_t stepsCount{ 10 };
		std::deque<DOMDescription> history;

		const auto oppositeDirection{ GetOppositeDirection(movingDirection) };

		auto domInitial{ AddDOMNoise(GenerateDOM(levels, tickSize, bestAsk, bestBid, volume), 0.1) };
		DisplayDOM(domInitial);

		history.push_back(GenerateDOMTrend(domInitial, oppositeDirection, .9));
		history.push_back(GenerateDOMTrend(domInitial, oppositeDirection, .1));
		history.push_back(GenerateDOMTrend(domInitial, movingDirection, .1));
		history.push_back(GenerateDOMTrend(domInitial, movingDirection, .2));
		history.push_back(GenerateDOMTrend(domInitial, movingDirection, .3));
		history.push_back(GenerateDOMTrend(domInitial, movingDirection, .4));
		history.push_back(GenerateDOMTrend(domInitial, movingDirection, .9));
		history.push_back(MoveDOM(domInitial, movingDirection, tickSize));

		for (auto dom : history)
		{
			DisplayDOM(dom);
		}

		return history;
	}


	static DOMDescription GenerateDOM(Level levels, TickSize tickSize, Price bestAsk, Price bestBid, Volume volume)
	{
		DOMDescription dom;

		const VolumeDescription desc{ .volume = volume };

		for (size_t i{}; i < levels; ++i)
		{
			dom.bids[bestBid - i * tickSize] = desc;
			dom.asks[bestAsk + i * tickSize] = desc;
		}

		return dom;
	}

	static DOMDescription AddDOMNoise(DOMDescription dom, double deviation)
	{
		auto gen = [deviation](auto& storage)
			{
				for (auto& [price, desc] : storage)
				{
					const auto delta{ desc.volume * deviation };
					desc.volume = std::max(GenerateVolume(desc.volume - delta, desc.volume + delta), Volume{ 1 });
				}
			};

		gen(dom.bids);
		gen(dom.asks);

		return dom;
	}

	// trendPower is in range [0, 1]
	static DOMDescription GenerateDOMTrend(DOMDescription dom, MovingDirection trendDirection, double trendPower)
	{
		trendPower = std::min(trendPower, 1.0);
		trendPower = std::max(trendPower, 0.0);

		const auto invertedTrendPower{ 1.0 - trendPower };

		auto askIt = dom.asks.begin();
		auto bidIt = dom.bids.begin();
		for (; askIt != dom.asks.end(); ++askIt, ++bidIt)
		{
			{
				auto& descAsk = askIt->second;
				auto& descBid = bidIt->second;
				if (trendDirection == MovingDirection::Down)
				{
					descBid.volume = std::max(Volume(invertedTrendPower * descAsk.volume), Volume{ 1 });
				}
				else
				{
					descAsk.volume = std::max(Volume(invertedTrendPower * descBid.volume), Volume{ 1 });
				}
			}
		}

		return dom;
	}

	static DOMDescription MoveDOM(DOMDescription dom, MovingDirection trendDirection, TickSize tickSize, Volume volume = 1'000)
	{
		if (trendDirection == MovingDirection::Up)
		{
			dom.asks[dom.asks.rbegin()->first + tickSize] = VolumeDescription{ .volume = volume };
			dom.bids.emplace(*dom.asks.begin());
			dom.bids.erase(std::prev(dom.bids.end()));
			dom.asks.erase(dom.asks.begin());
		}
		else
		{
			dom.bids[dom.bids.rbegin()->first - tickSize] = VolumeDescription{ .volume = volume };
			dom.asks.emplace(*dom.bids.begin());
			dom.asks.erase(std::prev(dom.asks.end()));
			dom.bids.erase(dom.bids.begin());
		}

		return dom;
	}

	static Volume GenerateVolume(Volume minVolume, Volume maxVolume)
	{
		static std::mt19937 generator{ std::random_device{}() };
		std::uniform_int_distribution<Volume> distribution(minVolume, maxVolume);
		return distribution(generator);
	}

private:
	const InstrumentInfo _instrumentInfo;
	std::deque<DOMDescription> _dom;

	mutable std::mutex _mutex;
	std::unordered_set<IDOMConsumer*> _consumers;
	std::atomic_int _callsCount{ 0 };
	std::jthread _notificationThread;
};

}

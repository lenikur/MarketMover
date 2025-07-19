#pragma once

#include "pch.h"

#include "Defs.h"
#include "IDOMProvider.h"
#include "IInstrumentProvider.h"

namespace Mover
{

class DOMProvider : public IDOMProvider
{
	std::vector<DOMDescription> GenerateDOMHistory(Level levels, TickSize tickSize, Price bestAsk, Price bestBid, Volume volume)
	{
		static size_t stepsCount{ 10 };
		std::vector<DOMDescription> history;
		history.reserve(stepsCount);

		for (size_t i = 0; i < 100; ++i)
		{
			auto dom = GenerateDOM(levels, tickSize, bestAsk, bestBid, volume);
			dom = AddDOMNoise(dom, 0.1);
			history.push_back(std::move(dom));
		}
		return history;
	}

public:
	DOMProvider(InstrumentInfo instrumentInfo, Level levels)
		: _instrumentInfo(std::move(instrumentInfo))
		, _dom{ GenerateDOMHistory(levels, _instrumentInfo.tickSize, 100, 99, 1000) }
		, _notificationThread([this](std::stop_token st) { NotifyConsumers(st); })
	{
	}

	void Subscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.insert(consumer);
	}
	void Unsubscribe(IDOMConsumer* consumer) override
	{
		std::scoped_lock lock(_mutex);
		_consumers.erase(consumer);
	}

	DOMDescription GetDOM(Level levels) const override
	{
		return {};
	}
	BBADescription GetBBA() const override
	{
		return {};
	}

private:
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
					desc.volume += std::max(GenerateVolume(desc.volume - delta, desc.volume + delta), Volume{ 1 });
				}
			};

		gen(dom.bids);
		gen(dom.asks);

		return dom;
	}

	// trendPower is in range [0, 1]
	static DOMDescription GenerateDOMTrend(DOMDescription dom, MovingDirection trendDirection, double trendPower)
	{
		const auto koefStep{ trendPower / dom.asks.size() };
		const auto invertedTrendPower{ 1.0 - trendPower };

		if (trendDirection == MovingDirection::Up)
		{
			auto askIt = dom.asks.begin();
			auto bidIt = dom.bids.begin();
			for (Level level{}; askIt != dom.asks.end(); ++level, ++askIt, ++bidIt)
			{
				{
					auto& desc = askIt->second;
					desc.volume *= (invertedTrendPower + (koefStep * level));
					desc.volume = std::max(desc.volume, Volume{ 1 });
				}
				{
					auto& desc = bidIt->second;
					desc.volume *= (trendPower - (koefStep * level));
					desc.volume = std::max(desc.volume, Volume{ 1 });
				}
			}
		}
		else
		{
			auto askIt = dom.asks.begin();
			auto bidIt = dom.bids.begin();
			for (Level level{}; askIt != dom.asks.end(); ++level, ++askIt, ++bidIt)
			{
				{
					auto& desc = askIt->second;
					desc.volume *= (trendPower - (koefStep * level));
					desc.volume = std::max(desc.volume, Volume{ 1 });
				}
				{
					auto& desc = bidIt->second;
					desc.volume *= (invertedTrendPower + (koefStep * level));
					desc.volume = std::max(desc.volume, Volume{ 1 });
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
			dom.bids.emplace(dom.asks.begin());
			dom.bids.erase(dom.bids.begin());
			dom.asks.erase(std::prev(dom.asks.end()));
		}
		else
		{
			dom.bids[dom.bids.rbegin()->first - tickSize] = VolumeDescription{ .volume = volume };
			dom.asks.emplace(dom.bids.begin());
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
						consumer->OnDOM();
				}
			}
			catch (const std::exception& exc)
			{
				std::cerr << "Exception in DOM notification thread: " << exc.what() << std::endl;
			}
		}
	}

private:
	const InstrumentInfo _instrumentInfo;
	std::mutex _domMutex;
	std::vector<DOMDescription> _dom;
	size_t _callsCount{ 0 };

	std::mutex _mutex;
	std::unordered_set<IDOMConsumer*> _consumers;
	std::jthread _notificationThread;
};

}

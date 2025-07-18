#pragma once

#include "Defs.h"

namespace Mover
{

struct InstrumentInfo
{
	Instrument instrument;
	TickSize tickSize{ 1 };
	Volume minimalVolume{ 1 };
};

struct IInstrumentProvider
{
	virtual ~IInstrumentProvider() = default;
	virtual InstrumentInfo GetInstrumentInfo(const Instrument& instrument) const = 0;
};

}

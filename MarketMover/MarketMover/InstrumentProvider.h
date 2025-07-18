#pragma once

#include "IInstrumentProvider.h"

namespace Mover
{

class InstrumentProvider : public IInstrumentProvider
{
public:
	InstrumentInfo GetInstrumentInfo(const Instrument& instrument) const override
	{
		return { instrument, 1, 1 };
	}
};

}

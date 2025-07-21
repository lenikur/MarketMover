#include "pch.h"

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "Defs.h"
#include "Config.h"
#include "Manager.h"


static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;

int main()
{
	plog::init(plog::debug, &consoleAppender);

	try
	{
		Mover::Config config;
		Mover::Manager manager{ config };
		manager.WaitForCompletion();
		PLOG_INFO << "Market Mover Strategy completed." ;
		return 0;
	}
	catch (const std::exception& exc)
	{
		PLOG_INFO << "Exception: " << exc.what() ;
		return 1;
	}
}

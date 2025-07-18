#include "pch.h"

#include "Defs.h"
#include "Config.h"
#include "Manager.h"

int main()
{
	try
	{
		Mover::Config config;
		Mover::Manager manager{ config };
		manager.WaitForCompletion();
		std::cout << "Market Mover Strategy completed." << std::endl;
		return 0;
	}
	catch (const std::exception& exc)
	{
		std::cout << "Exception: " << exc.what() << std::endl;
		return 1;
	}
}

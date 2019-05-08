/*
	Startup
*/

#include "SDL_main.h"
#include "App.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

int main(int argc, char** argv)
{
#ifdef WIN32
	SetProcessDPIAware(); //workaround for windows 10
#endif

	try
	{
		App app;
		app.run();
	}
	catch (std::exception e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

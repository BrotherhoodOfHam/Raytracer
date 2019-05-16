/*
	Startup
*/

#include "SDL_main.h"
//#include "Triangle.h"
#include "Raytracer.h"

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
		Raytracer app;
		app.run();
	}
	catch (std::exception e)
	{
		Log::error(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

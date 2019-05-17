/*
	Utils
*/

#ifdef WIN32
#include <Windows.h>
#endif

#include <iostream>
#include <sstream>

#include "Utils.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

void Log::pushState(Level lvl)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != NULL)
	{
		switch (lvl)
		{
		case LVL_INFO:
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
			break;
		case LVL_DEBUG:
			SetConsoleTextAttribute(h, FOREGROUND_GREEN);
			break;
		case LVL_WARN:
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			break;
		case LVL_ERROR:
			SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY);
			break;
		}
	}
}

void Log::popState()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	if (h != NULL)
	{
		SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	}
}

#else

void Log::pushState(uint32_t lvl) {}
void Log::popState() {}

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////

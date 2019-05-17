/*
	Utils header
*/

#pragma once

#include <iostream>

// Logger
class Log
{
private:

	enum Level
	{
		LVL_INFO,
		LVL_DEBUG,
		LVL_WARN,
		LVL_ERROR
	};

	//internal state
	static void pushState(Level lvl);
	static void popState();

public:

	template<typename A, typename ... T>
	inline static void info(A&& first, T&& ... rest)
	{
		std::cout << first << " ";
		Log::info(rest...);
	}

	inline static void info()
	{
		std::cout << std::endl;
	}

	template<typename ... T>
	inline static void debug(T&& ... args)
	{
		pushState(LVL_DEBUG);
		Log::info(args...);
		popState();
	}

	template<typename ... T>
	inline static void warn(T&& ... args)
	{
		pushState(LVL_WARN);
		Log::info("WARN:", args...);
		popState();
	}

	template<typename ... T>
	inline static void error(T&& ... args)
	{
		pushState(LVL_ERROR);
		Log::info("ERROR:", args...);
		popState();
	}
};

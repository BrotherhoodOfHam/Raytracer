/*
	Vulkan debugging extensions
*/

#pragma once

#include "Common.h"

class DebugExtension
{
private:

	vk::Instance _instance;

	vk::DebugReportCallbackEXT _reporterCallback;
	vk::DebugUtilsMessengerEXT _messengerCallback;

public:

	void init(vk::Instance instance);
	void destroy();
};

/*
	Vulkan debugging extensions
*/

#include "DebugExt.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////

VkBool32 debugReportCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char*                                 pLayerPrefix,
	const char*                                 pMessage,
	void*                                       pUserData)
{
	switch ((vk::DebugReportFlagBitsEXT)flags)
	{
	case vk::DebugReportFlagBitsEXT::eInformation:
		Log::info(pLayerPrefix, pMessage);
		break;
	case vk::DebugReportFlagBitsEXT::eDebug:
		Log::debug(pLayerPrefix, pMessage);
		break;
	case vk::DebugReportFlagBitsEXT::eWarning:
	case vk::DebugReportFlagBitsEXT::ePerformanceWarning:
		Log::warn(pLayerPrefix, pMessage);
		break;
	case vk::DebugReportFlagBitsEXT::eError:
		Log::error(pLayerPrefix, pMessage);
		break;
	}

	return true;
}

VkBool32 debugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
	void*                                            pUserData)
{
	Log::debug("MESSAGE: [", pCallbackData->pMessageIdName, "]\n", pCallbackData->pMessage);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

void DebugExtension::init(vk::Instance instance)
{
	_instance = instance;
	if (!_instance)
		return;

	auto createReporter = (PFN_vkCreateDebugReportCallbackEXT)_instance.getProcAddr("vkCreateDebugReportCallbackEXT");
	auto debugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");

	if (createReporter == nullptr)
	{
		Log::warn("could not load function vkCreateDebugReportCallbackEXT");
	}
	else
	{
		vk::DebugReportCallbackCreateInfoEXT reportInfo;
		reportInfo.pfnCallback = debugReportCallback;
		reportInfo.flags = vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning;
		VkResult r = createReporter(_instance, (const VkDebugReportCallbackCreateInfoEXT*)&reportInfo, nullptr, (VkDebugReportCallbackEXT*)&_reporterCallback);
		if (r != VK_SUCCESS)
		{
			Log::warn("vkCreateDebugReportCallbackEXT failed", r);
		}
	}

	if (debugMessenger == nullptr)
	{
		Log::warn("could not load function vkCreateDebugUtilsMessengerEXT");
	}
	else
	{
		vk::DebugUtilsMessengerCreateInfoEXT utilsInfo;
		utilsInfo.pfnUserCallback = debugUtilsMessengerCallback;
		utilsInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
		utilsInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
		VkResult r = debugMessenger(_instance, (const VkDebugUtilsMessengerCreateInfoEXT*)&utilsInfo, nullptr, (VkDebugUtilsMessengerEXT*)&_messengerCallback);
		if (r != VK_SUCCESS)
		{
			Log::warn("vkCreateDebugUtilsMessengerEXT failed");
		}
	}
}

void DebugExtension::destroy()
{
	if (_instance)
	{
		auto destroyReporter = (PFN_vkDestroyDebugReportCallbackEXT)_instance.getProcAddr("vkDestroyDebugReportCallbackEXT");
		auto destroyMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)_instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");

		if (_reporterCallback && destroyReporter != nullptr)
			destroyReporter(_instance, _reporterCallback, nullptr);

		if (_messengerCallback && destroyMessenger != nullptr)
			destroyMessenger(_instance, _messengerCallback, nullptr);
	}
}

#pragma once

#include <windows.h>

class AntiFrida
{
public:
	AntiFrida() = default;
	~AntiFrida() = default;

	static bool IsFridaModuleLoaded();
	static bool IsFridaPipePresent();
	static bool IsDetected();
	static void TriggerAntiUnpack();
	static void PerformChecks();

private:
	static void ToLower(char *str);
};

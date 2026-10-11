#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_ERRORSPIZDELKA_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_ERRORSPIZDELKA_H

#include <base/detect.h>
#include <string>

class CErrorSpizdelka
{
public:
	static CErrorSpizdelka &Instance();

	void Install();
	void Uninstall();
	bool IsInstalled() const { return m_Installed; }

	static void ShowCrashMessage(const char *pTitle, const char *pMessage);
	static std::string CollectSystemInfo();
	static std::string GetCallstackString(void *pContext);

private:
	CErrorSpizdelka();

	bool m_Installed = false;
	static bool s_InsideCrashHandler;

	static std::string GetSignalString(int Signal);

	static void AssertHandler(const char *pMessage);
	static void SignalHandler(int Signal);
	static void TerminateHandler();
};

#endif
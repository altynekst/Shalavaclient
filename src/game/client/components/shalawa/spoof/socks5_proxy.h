#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_SPOOF_SOCKS5_PROXY_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_SPOOF_SOCKS5_PROXY_H

#include <base/types.h>
#include <engine/shared/config.h>

class CSocks5Proxy
{
public:
	enum EState
	{
		STATE_NONE = 0,
		STATE_CONNECTING,
		STATE_GREETING_SENT,
		STATE_AUTH_SENT,
		STATE_REQUEST_SENT,
		STATE_CONNECTED,
		STATE_ERROR,
	};

	static CSocks5Proxy &Instance();

	bool IsEnabled() const;
	bool ParseProxyAddr(NETADDR *pOut) const;

	int ConnectThroughProxy(NETSOCKET Socket, const NETADDR *pTarget);

	EState GetState() const { return m_State; }
	const char *GetError() const { return m_aError; }
	void Reset();

private:
	CSocks5Proxy() = default;

	EState m_State = STATE_NONE;
	char m_aError[256] = "";

	bool SendAll(NETSOCKET Socket, const NETADDR *pAddr, const void *pData, int Size);
	int RecvWithTimeout(NETSOCKET Socket, NETADDR *pAddr, unsigned char *pData, int MaxSize, int TimeoutMs);

	int DoGreeting(NETSOCKET Socket, const NETADDR *pProxyAddr);
	int DoAuth(NETSOCKET Socket, const NETADDR *pProxyAddr);
	int DoConnect(NETSOCKET Socket, const NETADDR *pProxyAddr, const NETADDR *pTarget);
};

#endif
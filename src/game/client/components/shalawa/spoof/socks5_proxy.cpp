#include "socks5_proxy.h"

#include <base/system.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

#include <engine/shared/config.h>
#include <cstring>
#include <SDL.h>

CSocks5Proxy &CSocks5Proxy::Instance()
{
	ANTI_DISASM_BLOCK();
	static CSocks5Proxy s_Instance;
	return s_Instance;
}

bool CSocks5Proxy::IsEnabled() const
{
	ANTI_DISASM_BLOCK();
	return g_Config.m_shalawaSocks5Enabled && g_Config.m_shalawaSocks5Addr[OI(0)] != '\0';
}

bool CSocks5Proxy::ParseProxyAddr(NETADDR *pOut) const
{
	ANTI_DISASM_BLOCK();
	if(!IsEnabled())
		return false;
	return net_addr_from_str(pOut, g_Config.m_shalawaSocks5Addr) == OI(0);
}

void CSocks5Proxy::Reset()
{
	ANTI_DISASM_BLOCK();
	m_State = STATE_NONE;
	m_aError[OI(0)] = '\0';
}

bool CSocks5Proxy::SendAll(NETSOCKET Socket, const NETADDR *pAddr, const void *pData, int Size)
{
	ANTI_DISASM_BLOCK();
	return net_udp_send(Socket, pAddr, pData, Size) == Size;
}

int CSocks5Proxy::RecvWithTimeout(NETSOCKET Socket, NETADDR *pAddr, unsigned char *pData, int MaxSize, int TimeoutMs)
{
	ANTI_DISASM_BLOCK();
	int64_t Start = time_get();
	int64_t Freq = time_freq();

	while(true)
	{
		unsigned char *pRecv = nullptr;
		int Bytes = net_udp_recv(Socket, pAddr, &pRecv);
		if(Bytes > OI(0) && pRecv)
		{
			int CopySize = Bytes < MaxSize ? Bytes : MaxSize;
			mem_copy(pData, pRecv, CopySize);
			return CopySize;
		}

		if((time_get() - Start) * OI(1000) / Freq > TimeoutMs)
			return -OI(1);

		SDL_Delay(OI(1));
	}
}

int CSocks5Proxy::DoGreeting(NETSOCKET Socket, const NETADDR *pProxyAddr)
{
	ANTI_DISASM_BLOCK();
	bool NeedAuth = g_Config.m_shalawaSocks5User[OI(0)] != '\0';

	unsigned char aGreeting[4];
	aGreeting[OI(0)] = 0x05;
	aGreeting[OI(1)] = NeedAuth ? 0x02 : 0x01;
	aGreeting[OI(2)] = 0x00;
	if(NeedAuth)
		aGreeting[OI(3)] = 0x02;

	int GreetSize = NeedAuth ? OI(4) : OI(3);
	if(!SendAll(Socket, pProxyAddr, aGreeting, GreetSize))
	{
		str_copy(m_aError, "failed send SOCKS5 greeting");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	m_State = STATE_GREETING_SENT;

	NETADDR RecvAddr;
	unsigned char aResp[2];
	int Recv = RecvWithTimeout(Socket, &RecvAddr, aResp, sizeof(aResp), OI(5000));
	if(Recv < OI(2))
	{
		str_copy(m_aError, "baby ur socks is dead");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(0)] != 0x05)
	{
		str_copy(m_aError, "ur socks is had bad vers for client");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(1)] == 0xFF)
	{
		str_copy(m_aError, "socks5 no love this client:<");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(1)] == 0x02)
		return OI(1);

	return OI(0);
}

int CSocks5Proxy::DoAuth(NETSOCKET Socket, const NETADDR *pProxyAddr)
{
	ANTI_DISASM_BLOCK();
	int UserLen = str_length(g_Config.m_shalawaSocks5User);
	int PassLen = str_length(g_Config.m_shalawaSocks5Pass);

	if(UserLen <= OI(0) || UserLen > OI(255) || PassLen > OI(255))
	{
		str_copy(m_aError, "invalid socks5 infos");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	unsigned char aBuf[515];
	int Pos = OI(0);
	aBuf[Pos++] = 0x01;
	aBuf[Pos++] = (unsigned char)UserLen;
	mem_copy(&aBuf[Pos], g_Config.m_shalawaSocks5User, UserLen);
	Pos += UserLen;
	aBuf[Pos++] = (unsigned char)PassLen;
	mem_copy(&aBuf[Pos], g_Config.m_shalawaSocks5Pass, PassLen);
	Pos += PassLen;

	if(!SendAll(Socket, pProxyAddr, aBuf, Pos))
	{
		str_copy(m_aError, "failed send auth shity to socks5");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	m_State = STATE_AUTH_SENT;

	NETADDR RecvAddr;
	unsigned char aResp[2];
	int Recv = RecvWithTimeout(Socket, &RecvAddr, aResp, sizeof(aResp), OI(5000));
	if(Recv < OI(2))
	{
		str_copy(m_aError, "baby ur socks5 dead");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(1)] != 0x00)
	{
		str_copy(m_aError, "socks5 doesnt autothienifiatted");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	return OI(0);
}

int CSocks5Proxy::DoConnect(NETSOCKET Socket, const NETADDR *pProxyAddr, const NETADDR *pTarget)
{
	ANTI_DISASM_BLOCK();
	unsigned char aBuf[22];
	int Pos = OI(0);

	aBuf[Pos++] = 0x05;
	aBuf[Pos++] = 0x03;
	aBuf[Pos++] = 0x00;

	if(pTarget->type == NETTYPE_IPV4)
	{
		aBuf[Pos++] = 0x01;
		mem_copy(&aBuf[Pos], pTarget->ip, OI(4));
		Pos += OI(4);
	}
	else if(pTarget->type == NETTYPE_IPV6)
	{
		aBuf[Pos++] = 0x04;
		mem_copy(&aBuf[Pos], pTarget->ip, OI(16));
		Pos += OI(16);
	}
	else
	{
		str_copy(m_aError, "unssupport type of prosyx");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	aBuf[Pos++] = (pTarget->port >> OI(8)) & 0xFF;
	aBuf[Pos++] = pTarget->port & 0xFF;

	if(!SendAll(Socket, pProxyAddr, aBuf, Pos))
	{
		str_copy(m_aError, "failed sent connect request, mayb ur proxy is dead");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	m_State = STATE_REQUEST_SENT;

	NETADDR RecvAddr;
	unsigned char aResp[22];
	int Recv = RecvWithTimeout(Socket, &RecvAddr, aResp, sizeof(aResp), OI(10000));
	if(Recv < OI(4))
	{
		str_copy(m_aError, "ur socks5 dead mb");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(0)] != 0x05)
	{
		str_copy(m_aError, "invalid SOCKS5 connect response version");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	if(aResp[OI(1)] != 0x00)
	{
		const char *apErrors[] = {
			"success",
			"general failure",
			"connection not allowed",
			"network unreachable",
			"host unreachable",
			"connection refused",
			"TTL expired",
			"command not supported",
			"address type not supported"
		};
		int ErrIdx = aResp[OI(1)];
		if(ErrIdx > OI(8)) ErrIdx = OI(1);
		str_format(m_aError, sizeof(m_aError), "SOCKS5: %s", apErrors[ErrIdx]);
		m_State = STATE_ERROR;
		return -OI(1);
	}

	m_State = STATE_CONNECTED;
	return OI(0);
}

int CSocks5Proxy::ConnectThroughProxy(NETSOCKET Socket, const NETADDR *pTarget)
{
	ANTI_DISASM_BLOCK();
	Reset();

	NETADDR ProxyAddr;
	if(!ParseProxyAddr(&ProxyAddr))
	{
		str_copy(m_aError, "proxy invalid");
		m_State = STATE_ERROR;
		return -OI(1);
	}

	m_State = STATE_CONNECTING;

	int AuthNeeded = DoGreeting(Socket, &ProxyAddr);
	if(AuthNeeded < OI(0))
		return -OI(1);

	if(AuthNeeded == OI(1))
	{
		if(DoAuth(Socket, &ProxyAddr) < OI(0))
			return -OI(1);
	}

	if(DoConnect(Socket, &ProxyAddr, pTarget) < OI(0))
		return -OI(1);

	return OI(0);
}
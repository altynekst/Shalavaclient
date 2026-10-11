#include "chatspam.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <base/system.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

#include <cstdlib>


void CChatSpam::OnReset()
{
	ANTI_DISASM_BLOCK();
	m_CurrentIndex = OI(0);
	m_LastSendTime = OI(0);
	m_Running = false;
}


void CChatSpam::OnRender()
{
	ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaChatSpam)
	{
		m_Running = false;
		return;
	}

	if(!m_Running)
		return;

	if(m_vMessages.empty())
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	int64_t Now = time_get();
	int64_t Interval = static_cast<int64_t>(g_Config.m_shalawaChatSpamDelay) * time_freq() / OI(1000);

	if(Now - m_LastSendTime >= Interval)
	{
		SendNextMessage();
		m_LastSendTime = Now;
	}
}


void CChatSpam::SendNextMessage()
{
	ANTI_DISASM_BLOCK();
	if(m_vMessages.empty())
		return;

	if(m_CurrentIndex >= static_cast<int>(m_vMessages.size()))
	{
		if(g_Config.m_shalawaChatSpamLoop)
			m_CurrentIndex = OI(0);
		else
		{
			m_Running = false;
			return;
		}
	}

	std::string ProcessedMessage = ProcessMessage(m_vMessages[m_CurrentIndex]);

	if(!ProcessedMessage.empty())
	{
		if(g_Config.m_shalawaChatSpamTeam)
			m_pClient->m_Chat.SendChat(OI(1), ProcessedMessage.c_str());
		else
			m_pClient->m_Chat.SendChat(OI(0), ProcessedMessage.c_str());
	}

	m_CurrentIndex++;
}


std::string CChatSpam::ProcessMessage(const std::string &Message) const
{
	ANTI_DISASM_BLOCK();
	std::string Result = Message;

	size_t Pos = OI(0);
	while((Pos = Result.find("{random}", Pos)) != std::string::npos)
	{
		char aRandom[8];
		str_format(aRandom, sizeof(aRandom), "%d", rand() % 10000);
		Result.replace(Pos, (size_t)8, aRandom);
		Pos += str_length(aRandom);
	}

	Pos = OI(0);
	while((Pos = Result.find("{name}", Pos)) != std::string::npos)
	{
		const char *pName = m_pClient->m_aClients[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_aName;
		Result.replace(Pos, (size_t)6, pName);
		Pos += str_length(pName);
	}

	Pos = OI(0);
	while((Pos = Result.find("{map}", Pos)) != std::string::npos)
	{
		const char *pMap = Client()->GetCurrentMap();
		Result.replace(Pos, (size_t)5, pMap);
		Pos += str_length(pMap);
	}

	Pos = OI(0);
	while((Pos = Result.find("{time}", Pos)) != std::string::npos)
	{
		char aTime[32];
		str_timestamp(aTime, sizeof(aTime));
		Result.replace(Pos, (size_t)6, aTime);
		Pos += str_length(aTime);
	}

	return Result;
}


void CChatSpam::AddMessage(const char *pMessage)
{
	ANTI_DISASM_BLOCK();
	if(!pMessage || pMessage[OI(0)] == '\0')
		return;

	m_vMessages.push_back(pMessage);
}


void CChatSpam::RemoveMessage(int Index)
{
	ANTI_DISASM_BLOCK();
	if(Index < OI(0) || Index >= static_cast<int>(m_vMessages.size()))
		return;

	m_vMessages.erase(m_vMessages.begin() + Index);

	if(m_CurrentIndex >= static_cast<int>(m_vMessages.size()))
		m_CurrentIndex = OI(0);
}


void CChatSpam::UpdateMessage(int Index, const char *pMessage)
{
	ANTI_DISASM_BLOCK();
	if(Index < OI(0) || Index >= static_cast<int>(m_vMessages.size()))
		return;

	if(pMessage)
		m_vMessages[Index] = pMessage;
}


void CChatSpam::ClearMessages()
{
	ANTI_DISASM_BLOCK();
	m_vMessages.clear();
	m_CurrentIndex = OI(0);
}


void CChatSpam::SetMessages(const std::vector<std::string> &Messages)
{
	ANTI_DISASM_BLOCK();
	m_vMessages = Messages;
	m_CurrentIndex = OI(0);
}


void CChatSpam::Start()
{
	ANTI_DISASM_BLOCK();
	if(m_vMessages.empty())
		return;

	m_Running = true;
	m_CurrentIndex = OI(0);
	m_LastSendTime = OI(0);
	srand(static_cast<unsigned>(time_get() & 0xFFFFFFFF));
}


void CChatSpam::Stop()
{
	ANTI_DISASM_BLOCK();
	m_Running = false;
}

#include "autoanswer.h"

#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <base/system.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <cstdio>
#include <cstring>

static const char *TRIGGERS_FILE = "shalawa_triggers.txt";

void CAutoAnswer::OnInit()
{
	LoadTriggers();
}

void CAutoAnswer::OnReset()
{
	ANTI_DISASM_BLOCK();
	m_LastResponseTime = OI(0);
}

void CAutoAnswer::OnMessage(int MsgType, void *pRawMsg)
{
	ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaAutoAnswer)
		return;

	if(MsgType != NETMSGTYPE_SV_CHAT)
		return;

	CNetMsg_Sv_Chat *pMsg = static_cast<CNetMsg_Sv_Chat *>(pRawMsg);

	if(pMsg->m_ClientId < OI(0))
		return;

	if(pMsg->m_ClientId == m_pClient->m_aLocalIds[OI(0)] || pMsg->m_ClientId == m_pClient->m_aLocalIds[OI(1)])
		return;

	int64_t Now = time_get();
	int64_t Cooldown = static_cast<int64_t>(g_Config.m_shalawaAutoAnswerDelay) * time_freq() / OI(1000);

	if(Now - m_LastResponseTime < Cooldown)
		return;

	const char *pSenderName = m_pClient->m_aClients[pMsg->m_ClientId].m_aName;

	for(const auto &Trigger : m_vTriggers)
	{
		if(MatchesTrigger(pMsg->m_pMessage, Trigger))
		{
			SendResponse(pSenderName, Trigger.m_Response.c_str(), pMsg->m_Team);
			m_LastResponseTime = Now;
			return;
		}
	}

	if(ContainsMention(pMsg->m_pMessage))
	{
		SendResponse(pSenderName, g_Config.m_shalawaAutoAnswerMentionResponse, pMsg->m_Team);
		m_LastResponseTime = Now;
	}
}

bool CAutoAnswer::ContainsMention(const char *pMessage) const
{
	ANTI_DISASM_BLOCK();
	const char *pOwnName = m_pClient->m_aClients[m_pClient->m_aLocalIds[OI(0)]].m_aName;
	if(!pOwnName || pOwnName[OI(0)] == '\0')
		return false;
	return str_find_nocase(pMessage, pOwnName) != nullptr;
}

bool CAutoAnswer::MatchesTrigger(const char *pMessage, const STrigger &Trigger) const
{
	ANTI_DISASM_BLOCK();
	if(Trigger.m_ExactMatch)
	{
		if(Trigger.m_CaseSensitive)
			return str_comp(pMessage, Trigger.m_Trigger.c_str()) == OI(0);
		else
			return str_comp_nocase(pMessage, Trigger.m_Trigger.c_str()) == OI(0);
	}
	else
	{
		if(Trigger.m_CaseSensitive)
			return str_find(pMessage, Trigger.m_Trigger.c_str()) != nullptr;
		else
			return str_find_nocase(pMessage, Trigger.m_Trigger.c_str()) != nullptr;
	}
}

void CAutoAnswer::SendResponse(const char *pSenderName, const char *pResponse, int Team)
{
	ANTI_DISASM_BLOCK();
	if(!pResponse || pResponse[OI(0)] == '\0')
		return;

	char aFullResponse[512];
	if(g_Config.m_shalawaAutoAnswerIncludeName && pSenderName && pSenderName[OI(0)] != '\0')
		str_format(aFullResponse, sizeof(aFullResponse), "%s: %s", pSenderName, pResponse);
	else
		str_copy(aFullResponse, pResponse, sizeof(aFullResponse));

	if(g_Config.m_shalawaAutoAnswerTeam && Team == OI(1))
		m_pClient->m_Chat.SendChat(OI(1), aFullResponse);
	else
		m_pClient->m_Chat.SendChat(OI(0), aFullResponse);
}

void CAutoAnswer::AddTrigger(const char *pTrigger, const char *pResponse, bool CaseSensitive, bool ExactMatch)
{
	if(!pTrigger || !pResponse)
		return;

	STrigger NewTrigger;
	NewTrigger.m_Trigger = pTrigger;
	NewTrigger.m_Response = pResponse;
	NewTrigger.m_CaseSensitive = CaseSensitive;
	NewTrigger.m_ExactMatch = ExactMatch;
	m_vTriggers.push_back(NewTrigger);
	SaveTriggers();
}

void CAutoAnswer::RemoveTrigger(int Index)
{
	if(Index < 0 || Index >= static_cast<int>(m_vTriggers.size()))
		return;
	m_vTriggers.erase(m_vTriggers.begin() + Index);
	SaveTriggers();
}

void CAutoAnswer::UpdateTrigger(int Index, const char *pTrigger, const char *pResponse, bool CaseSensitive, bool ExactMatch)
{
	if(Index < 0 || Index >= static_cast<int>(m_vTriggers.size()))
		return;
	m_vTriggers[Index].m_Trigger = pTrigger;
	m_vTriggers[Index].m_Response = pResponse;
	m_vTriggers[Index].m_CaseSensitive = CaseSensitive;
	m_vTriggers[Index].m_ExactMatch = ExactMatch;
	SaveTriggers();
}

void CAutoAnswer::ClearTriggers()
{
	m_vTriggers.clear();
	SaveTriggers();
}

void CAutoAnswer::SaveTriggers()
{
	char aPath[512];
	str_format(aPath, sizeof(aPath), "%s", TRIGGERS_FILE);

	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	for(const auto &t : m_vTriggers)
	{
		char aLine[1024];
		str_format(aLine, sizeof(aLine), "%s\t%s\t%d\t%d\n",
			t.m_Trigger.c_str(), t.m_Response.c_str(),
			t.m_CaseSensitive ? 1 : 0, t.m_ExactMatch ? 1 : 0);
		io_write(File, aLine, str_length(aLine));
	}

	io_close(File);
}

void CAutoAnswer::LoadTriggers()
{
	m_vTriggers.clear();

	char aPath[512];
	str_format(aPath, sizeof(aPath), "%s", TRIGGERS_FILE);

	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aLineBuf[1024];
	while(true)
	{
		int i = 0;
		bool eof = false;
		while(i < (int)sizeof(aLineBuf) - 1)
		{
			unsigned char c;
			if(io_read(File, &c, 1) != 1)
			{
				eof = true;
				break;
			}
			if(c == '\n')
				break;
			if(c == '\r')
				continue;
			aLineBuf[i++] = c;
		}
		aLineBuf[i] = '\0';

		if(i > 0)
		{
			char *pTrigger = aLineBuf;
			char *pTab1 = strchr(pTrigger, '\t');
			if(pTab1)
			{
				*pTab1 = '\0';
				char *pResponse = pTab1 + 1;
				char *pTab2 = strchr(pResponse, '\t');
				if(pTab2)
				{
					*pTab2 = '\0';
					char *pCaseSens = pTab2 + 1;
					char *pTab3 = strchr(pCaseSens, '\t');
					if(pTab3)
					{
						*pTab3 = '\0';
						char *pExact = pTab3 + 1;

						STrigger t;
						t.m_Trigger = pTrigger;
						t.m_Response = pResponse;
						t.m_CaseSensitive = (atoi(pCaseSens) != 0);
						t.m_ExactMatch = (atoi(pExact) != 0);
						m_vTriggers.push_back(t);
					}
				}
			}
		}

		if(eof)
			break;
	}

	io_close(File);
}
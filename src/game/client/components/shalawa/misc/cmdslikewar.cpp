#include "cmdslikewar.h"

#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/console.h>
#include <game/client/gameclient.h>
#include <game/client/components/chat.h>
#include <generated/protocol.h>
#include <engine/shared/linereader.h>
#include <game/client/components/shalawa/visuals/hud.h>
#include <algorithm>
#include <cstring>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>


void CCmdsLikeWar::OnConsoleInit()
{
    ANTI_DISASM_BLOCK();
    dbg_msg(AY_OBFUSCATE("cmdswar"), AY_OBFUSCATE("MAX_CLIENTS=%d sizeof_array=%d"), MAX_CLIENTS, (int)sizeof(m_aClientActive));
}


void CCmdsLikeWar::OnInit()
{
    ANTI_DISASM_BLOCK();
    dbg_msg(AY_OBFUSCATE("cmdswar"), AY_OBFUSCATE("MAX_CLIENTS=%d sizeof_array=%d"), MAX_CLIENTS, (int)sizeof(m_aClientActive));
    LoadWarList();
    LoadTeamList();
    mem_zero(m_aClientActive, sizeof(m_aClientActive));
    m_SnapshotReceived = false;
}



void CCmdsLikeWar::OnMessage(int MsgType, void *pRawMsg)
{
    ANTI_DISASM_BLOCK();
    if(GameClient()->m_SuppressEvents)
        return;

    if(MsgType == NETMSGTYPE_SV_CHAT)
    {
        CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
        
        if(pMsg->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
        {
            ProcessChatCommand(pMsg->m_ClientId, pMsg->m_pMessage);
        }
    }
}


void CCmdsLikeWar::ProcessChatCommand(int ClientId, const char *pMessage)
{
    ANTI_DISASM_BLOCK();
    if(!pMessage || pMessage[OI(0)] != '?')
        return;

    char aCommand[64];
    char aArg[MAX_NAME_LENGTH];
    aCommand[0] = '\0';
    aArg[0] = '\0';

    const char *pSpace = str_find(pMessage + 1, " ");
    if(pSpace)
    {
        int CommandLen = minimum((int)(pSpace - pMessage - 1), (int)sizeof(aCommand) - 1);
        str_copy(aCommand, pMessage + 1, CommandLen + 1);
        
        while(*pSpace == ' ')
            pSpace++;
        str_copy(aArg, pSpace, sizeof(aArg));
        
        int Len = str_length(aArg);
        while(Len > 0 && aArg[Len - 1] == ' ')
        {
            aArg[Len - 1] = '\0';
            Len--;
        }
    }
    else
    {
        str_copy(aCommand, pMessage + 1, sizeof(aCommand));
    }


    if(str_comp_nocase(aCommand, AY_OBFUSCATE("help")) == OI(0))
    {
        ShowHelp();
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("war")) == OI(0))
    {
        if(aArg[OI(0)] == '\0')
            Echo(AY_OBFUSCATE("Использование: ?war <nickname>"));
        else
            AddWar(aArg);
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("unwar")) == OI(0))
    {
        if(aArg[OI(0)] == '\0')
            Echo(AY_OBFUSCATE("Использование: ?unwar <nickname>"));
        else
            RemoveWar(aArg);
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("warlist")) == OI(0))
    {
        ListWars();
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("team")) == OI(0))
    {
        if(aArg[OI(0)] == '\0')
            Echo(AY_OBFUSCATE("Использование: ?team <nickname>"));
        else
            AddTeam(aArg);
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("unteam")) == OI(0))
    {
        if(aArg[OI(0)] == '\0')
            Echo(AY_OBFUSCATE("Использование: ?unteam <nickname>"));
        else
            RemoveTeam(aArg);
    }
    else if(str_comp_nocase(aCommand, AY_OBFUSCATE("teamlist")) == OI(0))
    {
        ListTeams();
    }
}


void CCmdsLikeWar::Echo(const char *pMessage)
{
    ANTI_DISASM_BLOCK();
    if(!pMessage || !GameClient())
        return;
    GameClient()->m_Chat.Echo(pMessage);
}


void CCmdsLikeWar::ShowHelp()
{
    ANTI_DISASM_BLOCK();
    Echo(AY_OBFUSCATE("=== Вар система кмдлист ==="));
    Echo(AY_OBFUSCATE("?help - Показ этого сообщения"));
    Echo(AY_OBFUSCATE("?war <nick> - Добавить чела в вары"));
    Echo(AY_OBFUSCATE("?unwar <nick> - Убрать вара"));
    Echo(AY_OBFUSCATE("?warlist - Показать варов"));
    Echo(AY_OBFUSCATE("?team <nick> - Добавить чела в тиму"));
    Echo(AY_OBFUSCATE("?unteam <nick> - Удалить чела из тимы"));
    Echo(AY_OBFUSCATE("?teamlist - Показать тиммейтов"));
}


void CCmdsLikeWar::AddWar(const char *pName)
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[OI(0)] == '\0')
        return;

    for(const auto &Name : m_vWarList)
    {
        if(str_comp_nocase(Name.c_str(), pName) == OI(0))
        {
            char aBuf[128];
            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' уже в твой вар"), pName);
            Echo(aBuf);
            return;
        }
    }

    for(auto it = m_vTeamList.begin(); it != m_vTeamList.end(); ++it)
    {
        if(str_comp_nocase(it->c_str(), pName) == OI(0))
        {
            m_vTeamList.erase(it);
            SaveTeamList();
            
            char aBuf[128];
            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' убран из варов"), pName);
            Echo(aBuf);
            break;
        }
    }

    m_vWarList.push_back(std::string(pName));
    SaveWarList();

    char aBuf[128];
    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' добавлен в вары"), pName);
    Echo(aBuf);
}


void CCmdsLikeWar::RemoveWar(const char *pName)
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[OI(0)] == '\0')
        return;

    for(auto it = m_vWarList.begin(); it != m_vWarList.end(); ++it)
    {
        if(str_comp_nocase(it->c_str(), pName) == OI(0))
        {
            m_vWarList.erase(it);
            SaveWarList();

            char aBuf[128];
            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' удален из варов"), pName);
            Echo(aBuf);
            return;
        }
    }

    char aBuf[128];
    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' не твой вар"), pName);
    Echo(aBuf);
}


bool CCmdsLikeWar::IsWarPlayer(const char *pName) const
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[0] == '\0')
        return false;

    for(const auto &Name : m_vWarList)
    {
        if(str_comp_nocase(Name.c_str(), pName) == 0)

            return true;
    }
    return false;
}


bool CCmdsLikeWar::IsWarPlayer(int ClientId) const
{
    ANTI_DISASM_BLOCK();
    if(ClientId < 0 || ClientId >= MAX_CLIENTS)
        return false;


    if(!GameClient() || !GameClient()->m_aClients[ClientId].m_Active)
        return false;

    return IsWarPlayer(GameClient()->m_aClients[ClientId].m_aName);
}


void CCmdsLikeWar::ListWars()
{
    ANTI_DISASM_BLOCK();
    if(m_vWarList.empty())
    {
        Echo(AY_OBFUSCATE("Варов нет"));
        return;
    }

    char aBuf[256];
    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("=== Вары (%d игроков) ==="), (int)m_vWarList.size());
    Echo(aBuf);

    for(size_t i = 0; i < m_vWarList.size(); i++)
    {
        str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("%d. %s"), (int)(i + 1), m_vWarList[i].c_str());

        Echo(aBuf);
    }
}


void CCmdsLikeWar::AddTeam(const char *pName)
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[0] == '\0')
        return;

    for(const auto &Name : m_vTeamList)
    {
        if(str_comp_nocase(Name.c_str(), pName) == 0)
        {
            char aBuf[128];

            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' уже твой тим"), pName);
            Echo(aBuf);
            return;
        }
    }

    for(auto it = m_vWarList.begin(); it != m_vWarList.end(); ++it)
    {
        if(str_comp_nocase(it->c_str(), pName) == OI(0))
        {
            m_vWarList.erase(it);
            SaveWarList();
            
            char aBuf[128];
            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' убран из варов"), pName);
            Echo(aBuf);
            break;
        }
    }

    m_vTeamList.push_back(std::string(pName));
    SaveTeamList();

    char aBuf[128];
    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' добавлен в тим"), pName);
    Echo(aBuf);
}


void CCmdsLikeWar::RemoveTeam(const char *pName)
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[OI(0)] == '\0')
        return;

    for(auto it = m_vTeamList.begin(); it != m_vTeamList.end(); ++it)
    {
        if(str_comp_nocase(it->c_str(), pName) == OI(0))
        {
            m_vTeamList.erase(it);
            SaveTeamList();

            char aBuf[128];
            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' больше не твой тим"), pName);
            Echo(aBuf);
            return;
        }
    }

    char aBuf[128];
    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("'%s' он не твой тим"), pName);
    Echo(aBuf);
}


bool CCmdsLikeWar::IsTeamPlayer(const char *pName) const
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[0] == '\0')
        return false;

    for(const auto &Name : m_vTeamList)
    {
        if(str_comp_nocase(Name.c_str(), pName) == 0)

            return true;
    }
    return false;
}


bool CCmdsLikeWar::IsTeamPlayer(int ClientId) const
{
    ANTI_DISASM_BLOCK();
    if(ClientId < 0 || ClientId >= MAX_CLIENTS)
        return false;


    if(!GameClient() || !GameClient()->m_aClients[ClientId].m_Active)
        return false;

    return IsTeamPlayer(GameClient()->m_aClients[ClientId].m_aName);
}


void CCmdsLikeWar::ListTeams()
{
    ANTI_DISASM_BLOCK();
    if(m_vTeamList.empty())
    {
        Echo(AY_OBFUSCATE("У тебя нет тимов (allwar lolxD?)"));
        return;
    }

    char aBuf[256];

    str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("=== Тимы (%d людей) ==="), (int)m_vTeamList.size());
    Echo(aBuf);

    for(size_t i = 0; i < m_vTeamList.size(); i++)
    {
        str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("%d. %s"), (int)(i + 1), m_vTeamList[i].c_str());

        Echo(aBuf);
    }
}


int CCmdsLikeWar::FindPlayerByName(const char *pName) const
{
    ANTI_DISASM_BLOCK();
    if(!pName || pName[0] == '\0')
        return -1;

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(!GameClient()->m_aClients[i].m_Active)
            continue;

        if(str_comp_nocase(GameClient()->m_aClients[i].m_aName, pName) == 0)

            return i;
    }

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(!GameClient()->m_aClients[i].m_Active)
            continue;

        if(str_utf8_find_nocase(GameClient()->m_aClients[i].m_aName, pName))

            return i;
    }

    return -1;

}


void CCmdsLikeWar::OnNewSnapshot()
{
    ANTI_DISASM_BLOCK();
    if(!GameClient())
        return;

    if(GameClient()->m_SuppressEvents)
        return;

    int LocalId = GameClient()->m_Snap.m_LocalClientId;
    if(LocalId < OI(0) || LocalId >= OI(64))
        return;

    if(!m_SnapshotReceived)
    {
        m_SnapshotReceived = true;
        for(int i = OI(0); i < OI(64); i++)
            m_aClientActive[i] = GameClient()->m_aClients[i].m_Active;
        return;
    }

    for(int i = OI(0); i < OI(64); i++)
    {
        bool wasActive = m_aClientActive[i];
        bool isActive = GameClient()->m_aClients[i].m_Active;
        m_aClientActive[i] = isActive;

        if(!isActive || wasActive || i == LocalId)
            continue;

        const char *pName = GameClient()->m_aClients[i].m_aName;
        if(!pName || pName[0] == '\0')
            continue;

        if(IsWarPlayer(pName))
        {
            SPendingNotify n;
            n.m_Text[0] = '\0';
            char aBuf[128], aBuf2[128];

            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("Вар '%s' зашел на сервер!"), pName);
            str_format(aBuf2, sizeof(aBuf2), AY_OBFUSCATE("War: %s"), pName);
            n.m_Text = aBuf;
            n.m_NotifyText = aBuf2;
            n.m_IsTeam = false;
            m_vPendingNotifications.push_back(n);
        }
        else if(IsTeamPlayer(pName))
        {
            SPendingNotify n;
            char aBuf[128], aBuf2[128];

            str_format(aBuf, sizeof(aBuf), AY_OBFUSCATE("Тим '%s' зашел на сервер!"), pName);
            str_format(aBuf2, sizeof(aBuf2), AY_OBFUSCATE("Team: %s"), pName);
            n.m_Text = aBuf;
            n.m_NotifyText = aBuf2;
            n.m_IsTeam = true;
            m_vPendingNotifications.push_back(n);
        }
    }
}


void CCmdsLikeWar::OnRender()
{
    ANTI_DISASM_BLOCK();
    if(m_vPendingNotifications.empty())
        return;

    if(!GameClient())
        return;

    if(Client()->State() != IClient::STATE_ONLINE)
    {
        m_vPendingNotifications.clear();
        return;
    }

    for(const auto &n : m_vPendingNotifications)
    {
        GameClient()->m_Chat.Echo(n.m_Text.c_str());
        if(n.m_IsTeam)
            CShalawaHud::Instance().AddNotification(n.m_NotifyText, std::string(AY_OBFUSCATE("Joined Server")), true);
        else
            CShalawaHud::Instance().AddNotification(n.m_NotifyText, std::string(AY_OBFUSCATE("Joined Server")), false);
    }

    m_vPendingNotifications.clear();
}

void CCmdsLikeWar::LoadTeamList()
{
    ANTI_DISASM_BLOCK();
    m_vTeamList.clear();

    CLineReader Reader;
    if(!Reader.OpenFile(Storage()->OpenFile(TEAM_FILE, IOFLAG_READ, IStorage::TYPE_SAVE)))
        return;

    while(const char *pLine = Reader.Get())
    {
        if(pLine[0] != '\0')
            m_vTeamList.push_back(std::string(pLine));
    }

}

void CCmdsLikeWar::LoadWarList()
{
    ANTI_DISASM_BLOCK();
    m_vWarList.clear();

    CLineReader Reader;
    if(!Reader.OpenFile(Storage()->OpenFile(WAR_FILE, IOFLAG_READ, IStorage::TYPE_SAVE)))
        return;

    while(const char *pLine = Reader.Get())
    {
        if(pLine[0] != '\0')
            m_vWarList.push_back(std::string(pLine));
    }

}

void CCmdsLikeWar::SaveWarList()
{
    ANTI_DISASM_BLOCK();
    IOHANDLE File = Storage()->OpenFile(WAR_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
    if(!File)
        return;

    for(const auto &Name : m_vWarList)
    {
        io_write(File, Name.c_str(), Name.length());
        io_write_newline(File);
    }

    io_close(File);
}

void CCmdsLikeWar::SaveTeamList()
{
    ANTI_DISASM_BLOCK();
    IOHANDLE File = Storage()->OpenFile(TEAM_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
    if(!File)
        return;

    for(const auto &Name : m_vTeamList)
    {
        io_write(File, Name.c_str(), Name.length());
        io_write_newline(File);
    }

    io_close(File);
}

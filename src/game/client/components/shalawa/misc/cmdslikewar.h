#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MISC_CMDSLIKEWAR_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MISC_CMDSLIKEWAR_H

#include <game/client/component.h>
#include <base/system.h>
#include <vector>
#include <string>

class CCmdsLikeWar : public CComponent
{
public:
    CCmdsLikeWar() { mem_zero(m_aClientActive, sizeof(m_aClientActive)); m_SnapshotReceived = false; }
    virtual int Sizeof() const override { return sizeof(*this); }
    virtual void OnConsoleInit() override;
    virtual void OnInit() override;
    virtual void OnMessage(int MsgType, void *pRawMsg) override;

    void AddWar(const char *pName);
    void RemoveWar(const char *pName);
    virtual void OnRender() override;
    bool IsWarPlayer(const char *pName) const;
    bool IsWarPlayer(int ClientId) const;
    void ListWars();
    virtual void OnNewSnapshot() override;

    void AddTeam(const char *pName);
    void RemoveTeam(const char *pName);
    bool IsTeamPlayer(const char *pName) const;
    bool IsTeamPlayer(int ClientId) const;
    void ListTeams();

    void ShowHelp();

    const std::vector<std::string> &GetWarList() const { return m_vWarList; }
    const std::vector<std::string> &GetTeamList() const { return m_vTeamList; }

private:
    std::vector<std::string> m_vWarList;
    std::vector<std::string> m_vTeamList;

    static constexpr const char *WAR_FILE = "ddnet_wars.txt";
    static constexpr const char *TEAM_FILE = "ddnet_teams.txt";
    bool m_aClientActive[64];  // MAX_CLIENTS = 64
    bool m_SnapshotReceived;
    struct SPendingNotify
    {
        std::string m_Text;
        std::string m_NotifyText;
        bool m_IsTeam;
    };
    std::vector<SPendingNotify> m_vPendingNotifications;

    void LoadWarList();
    void SaveWarList();
    void LoadTeamList();
    void SaveTeamList();

    void ProcessChatCommand(int ClientId, const char *pMessage);
    void Echo(const char *pMessage);
    int FindPlayerByName(const char *pName) const;
};

#endif
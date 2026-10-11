#include "menus_start.h"

#include <engine/sound.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/components/sounds.h>
#include <game/client/components/menus.h>
#include <game/version.h>

#include <engine/client/imgui_manager.h>

void CMenusStart::OnInit()
{
    m_MusicStarted = false;
    m_MusicSampleId = -1;
    m_MusicVoice = ISound::CVoiceHandle();
}

void CMenusStart::OnReset()
{
    if(m_MusicSampleId >= 0)
        Sound()->Stop(m_MusicSampleId);
    m_MusicStarted = false;
    m_MusicSampleId = -1;
    m_MusicVoice = ISound::CVoiceHandle();
}

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
    GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

    if(!m_MusicStarted)
    {
        m_MusicStarted = true;
        if(Sound()->IsSoundEnabled())
        {
            m_MusicSampleId = Sound()->LoadWV("fonmusic/nya.wv", IStorage::TYPE_ALL);
            if(m_MusicSampleId >= 0)
                m_MusicVoice = Sound()->Play(CSounds::CHN_MUSIC, m_MusicSampleId, ISound::FLAG_LOOP, 0.35f);
        }
    }

    CImGuiManager &imgui = CImGuiManager::Instance();
    imgui.m_ShowStartMenu = true;

    if(imgui.m_StartMenuClickedButton >= 0)
    {
        int btn = imgui.m_StartMenuClickedButton;
        imgui.m_StartMenuClickedButton = -1;

        switch(btn)
        {
            case 0:
            {
                GameClient()->m_Menus.SetShowStart(false);
                int page = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5
                    ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
                GameClient()->m_Menus.SetMenuPage(page);
                imgui.m_ShowStartMenu = false;
                break;
            }
            case 1:
            {
                if(GameClient()->m_LocalServer.IsServerRunning())
                    GameClient()->m_LocalServer.KillServer();
                else
                    GameClient()->m_LocalServer.RunServer({});
                break;
            }
            case 2:
            {
                g_Config.m_ClEditor = 1;
                Input()->MouseModeRelative();
                imgui.m_ShowStartMenu = false;
                break;
            }
            case 3:
            {
                GameClient()->m_Menus.SetShowStart(false);
                GameClient()->m_Menus.SetMenuPage(CMenus::PAGE_SETTINGS);
                imgui.m_ShowStartMenu = false;
                break;
            }
            case 4:
            {
                Client()->ViewLink("https://t.me/shalawa_client");
                break;
            }
            case 5:
            {
                Client()->Quit();
                break;
            }
        }
    }
}
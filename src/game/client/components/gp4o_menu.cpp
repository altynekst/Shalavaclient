#include "gp4o_menu.h"

#include <engine/input.h>
#include <engine/keys.h>
#include <game/client/gameclient.h>
#include <engine/client/imgui_manager.h>
#include <SDL.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

CGp4oMenu::CGp4oMenu()
{
    m_Open = false;
    m_WasRelativeMode = false;
}

void CGp4oMenu::OnInit()
{
    ANTI_DISASM_BLOCK();
    CImGuiManager::Instance().SetGameClient(GameClient());
}

void CGp4oMenu::OnReset()
{
    ANTI_DISASM_BLOCK();
    m_Open = false;
    CImGuiManager::Instance().m_MenuOpen = false;
}

bool CGp4oMenu::OnInput(const IInput::CEvent &Event)
{
    ANTI_DISASM_BLOCK();
    if((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_INSERT)
    {
        m_Open = !m_Open;
        CImGuiManager::Instance().m_MenuOpen = m_Open;
        
        if(m_Open)
        {
            m_WasRelativeMode = SDL_GetRelativeMouseMode() == SDL_TRUE;
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_ShowCursor(SDL_ENABLE);
            Input()->MouseModeAbsolute();
        }
        else
        {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_ShowCursor(SDL_DISABLE);
            Input()->MouseModeRelative();
        }
        
        return true;
    }
    
    if(m_Open)
    {
        return true;
    }
    
    return false;
}

void CGp4oMenu::OnRender()
{
    ANTI_DISASM_BLOCK();
    if(m_Open)
    {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

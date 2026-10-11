#ifndef GAME_CLIENT_COMPONENTS_MENUS_START_H
#define GAME_CLIENT_COMPONENTS_MENUS_START_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>
#include <engine/graphics.h>
#include <engine/sound.h>

class CMenusStart : public CComponent
{
    bool m_MusicStarted = false;
    int m_MusicSampleId = -1;
    ISound::CVoiceHandle m_MusicVoice;

public:
    virtual int Sizeof() const override { return sizeof(*this); }
    virtual void OnInit() override;
    virtual void OnReset() override;
    void RenderStartMenu(CUIRect MainView);
};

#endif
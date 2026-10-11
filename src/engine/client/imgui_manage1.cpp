#include "imgui_manage1.h"
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL.h>
#include <base/system.h>
#include <base/str.h>
#include <engine/shared/config.h>
#include <engine/input.h>
#include <engine/storage.h>
#include "imgui_manager.h"
#include <cmath>
#include <algorithm>

#include <game/client/gameclient.h>
#include <game/client/components/shalawa/misc/autoanswer.h>
#include <game/client/components/shalawa/misc/chatspam.h>
#include <game/client/components/shalawa/misc/watermark.h>
#include <game/client/components/shalawa/visuals/hud.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

namespace
{
	struct STasReplayListData
	{
		std::vector<std::string> *m_pList;
	};

	int TasReplayListCallback(const char *pName, int IsDir, int DirType, void *pUser)
	{
		(void)DirType;
		if(IsDir)
			return 0;
		if(!str_endswith_nocase(pName, ".tas"))
			return 0;
		auto *pData = static_cast<STasReplayListData *>(pUser);
		if(!pData || !pData->m_pList)
			return 0;
		char aName[IO_MAX_PATH_LENGTH];
		IStorage::StripPathAndExtension(pName, aName, sizeof(aName));
		pData->m_pList->push_back(aName);
		return 0;
	}
}

CImGuiManagerModern::CImGuiManagerModern() { ANTI_DISASM_BLOCK(); }

void CImGuiManagerModern::RefreshTasReplayList()
{
	ANTI_DISASM_BLOCK();
	m_vTasReplays.clear();
	m_TasReplayIndex = -1;
	m_TasReplayListDirty = false;

	if(!m_pGameClient || !m_pGameClient->Storage())
		return;

	IStorage *pStorage = m_pGameClient->Storage();
	if(!pStorage->FolderExists("shalawa.xyz", IStorage::TYPE_SAVE))
		return;

	STasReplayListData Data{&m_vTasReplays};
	pStorage->ListDirectory(IStorage::TYPE_SAVE, "shalawa.xyz", TasReplayListCallback, &Data);
	std::sort(m_vTasReplays.begin(), m_vTasReplays.end());

	if(!m_vTasReplays.empty())
	{
		m_TasReplayIndex = 0;
		for(size_t i = 0; i < m_vTasReplays.size(); i++)
		{
			if(str_comp_nocase(m_vTasReplays[i].c_str(), g_Config.m_shalawaRecordName) == 0)
			{
				m_TasReplayIndex = (int)i;
				break;
			}
		}
	}
}

CImGuiManagerModern &CImGuiManagerModern::Instance()
{
    ANTI_DISASM_BLOCK();
    static CImGuiManagerModern s_Instance;
    return s_Instance;
}

void CImGuiManagerModern::Init(SDL_Window *pWindow, SDL_GLContext Context)
{
    ANTI_DISASM_BLOCK();
    if(m_Initialized)
        return;

    m_pWindow = pWindow;
    SetupStyle();
    InitFeatureCards();

    if(m_pGameClient)
        LoadIcons();

    m_Initialized = true;
    m_LastTime = (double)SDL_GetTicks() / 1000.0;
}

void CImGuiManagerModern::SetGameClient(CGameClient *pGameClient) 
{ 
    ANTI_DISASM_BLOCK();
    m_pGameClient = pGameClient;
    m_Watermark.Init(pGameClient);
    
    if(pGameClient)
    {
        m_pAutoAnswer = &pGameClient->m_AutoAnswer;
        m_pChatSpam = &pGameClient->m_ChatSpam;
    }
    
    if(m_Initialized && !m_IconsLoaded)
        LoadIcons();
    m_FeatureCards[0].clear();
    m_FeatureCards[1].clear();
    m_FeatureCards[2].clear();
    m_FeatureCards[3].clear();
    m_FeatureCards[4].clear();
    m_FeatureCards[5].clear();
    m_FeatureCards[6].clear();
    m_FeatureCards[7].clear();
    InitFeatureCards();
}

void CImGuiManagerModern::SetupStyle()
{
    ANTI_DISASM_BLOCK();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 12.0f;
    s.FrameRounding = 8.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding = 8.0f;
    s.TabRounding = 8.0f;
    s.ChildRounding = 8.0f;
    
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(8, 4);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize = 10.0f;
    s.GrabMinSize = 10.0f;
    
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    
    m_Theme.m_BgPrimary = ImVec4(0.055f, 0.055f, 0.063f, 1.0f);
    m_Theme.m_BgSecondary = ImVec4(0.094f, 0.094f, 0.106f, 1.0f);
    m_Theme.m_BgTertiary = ImVec4(0.122f, 0.122f, 0.137f, 1.0f);
    m_Theme.m_BgHover = ImVec4(0.149f, 0.149f, 0.173f, 1.0f);
    m_Theme.m_TextPrimary = ImVec4(0.937f, 0.937f, 0.945f, 1.0f);
    m_Theme.m_TextSecondary = ImVec4(0.678f, 0.678f, 0.722f, 1.0f);
    m_Theme.m_TextTertiary = ImVec4(0.518f, 0.518f, 0.580f, 1.0f);
    m_Theme.m_Border = ImVec4(0.165f, 0.165f, 0.176f, 1.0f);
    m_Theme.m_BorderLight = ImVec4(0.227f, 0.227f, 0.239f, 1.0f);
    
    ImVec4 *colors = s.Colors;
    colors[ImGuiCol_Text] = m_Theme.m_TextPrimary;
    colors[ImGuiCol_TextDisabled] = m_Theme.m_TextTertiary;
    colors[ImGuiCol_WindowBg] = m_Theme.m_BgPrimary;
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = m_Theme.m_BgSecondary;
    colors[ImGuiCol_Border] = m_Theme.m_Border;
    colors[ImGuiCol_FrameBg] = m_Theme.m_BgTertiary;
    colors[ImGuiCol_FrameBgHovered] = m_Theme.m_BgHover;
    colors[ImGuiCol_FrameBgActive] = m_Theme.m_BgHover;
    colors[ImGuiCol_ScrollbarBg] = m_Theme.m_BgSecondary;
    colors[ImGuiCol_ScrollbarGrab] = m_Theme.m_BgTertiary;
    colors[ImGuiCol_ScrollbarGrabHovered] = m_Theme.m_BgHover;
    colors[ImGuiCol_ScrollbarGrabActive] = m_Theme.m_BgHover;
    colors[ImGuiCol_Header] = m_Theme.m_BgTertiary;
    colors[ImGuiCol_HeaderHovered] = m_Theme.m_BgHover;
    colors[ImGuiCol_HeaderActive] = m_Theme.m_BgHover;
}

ImTextureID CImGuiManagerModern::GetWeaponTexture(int weapon)
{
    ANTI_DISASM_BLOCK();
    if(!m_pGameClient || !m_pGameClient->m_GameSkinLoaded)
        return (ImTextureID)0;
    
    IGraphics::CTextureHandle tex;
    switch(weapon)
    {
        case WEAPON_HAMMER: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponHammer; break;
        case WEAPON_GUN: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponGun; break;
        case WEAPON_SHOTGUN: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponShotgun; break;
        case WEAPON_GRENADE: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponGrenade; break;
        case WEAPON_LASER: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponLaser; break;
        case WEAPON_NINJA: tex = m_pGameClient->m_GameSkin.m_SpriteWeaponNinja; break;
        case WEAPON_HOOK: tex = m_pGameClient->m_GameSkin.m_SpriteHookHead; break;
        default: return (ImTextureID)0;
    }
    
    if(!tex.IsValid())
        return (ImTextureID)0;
    
    return (ImTextureID)(intptr_t)tex.Id();
}

void CImGuiManagerModern::RenderWeaponIcon(ImDrawList *d, ImVec2 pos, ImVec2 size, int weapon, float alpha)
{
    ImTextureID tex = GetWeaponTexture(weapon);
    if(tex)
    {
        float padding = 2.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, alpha));
        d->AddImage(tex, ImVec2(pos.x + padding, pos.y + padding), 
            ImVec2(pos.x + size.x - padding, pos.y + size.y - padding), ImVec2(0, 0), ImVec2(1, 1), col);
    }
    else
    {
        const char *fallback[] = {"H", "G", "S", "R", "L", "N", ">"};
        if(weapon >= 0 && weapon < NUM_WEAPONS)
        {
            ImVec2 textSize = ImGui::CalcTextSize(fallback[weapon]);
            d->AddText(ImVec2(pos.x + (size.x - textSize.x) / 2, pos.y + (size.y - textSize.y) / 2),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, alpha)), fallback[weapon]);
        }
    }
}

void CImGuiManagerModern::InitFeatureCards()
{
    m_FeatureCards[1].push_back({"avoid_mode", "Avoid Mode", "Avoidance behavior", -1, "avoidmode", false,
        [this]() {
            static const char *modes[] = {"Off", "Legit", "Blatant", "AI Bot"};
            RenderCombo("Mode", &g_Config.m_shalawaAvoidMode, modes, 4);
        },
        [this]() { return g_Config.m_shalawaAvoidMode > 0; }
    });
    
    m_FeatureCards[1].push_back({"avoid_legit_visuals", "Legit Visuals", "Visual feedback for avoidance", -1, "avoidlegitvisuals", false,
        [this]() {
            RenderToggle("Enable Visuals", &g_Config.m_shalawaLegitAvoidVisuals);
            RenderSlider("Alpha", &g_Config.m_shalawaLegitAvoidVisualsAlpha, 10, 100);
            RenderToggle("Direction Trail", &g_Config.m_shalawaLegitAvoidVisualsTrail);
            RenderToggle("Danger Zone", &g_Config.m_shalawaLegitAvoidVisualsDanger);
            RenderToggle("Hook Release Ring", &g_Config.m_shalawaLegitAvoidVisualsHook);
            RenderToggle("Velocity Arrow", &g_Config.m_shalawaLegitAvoidVisualsVelocity);
            RenderToggle("Predicted Path", &g_Config.m_shalawaLegitAvoidVisualsPath);
            RenderToggle("Status Text", &g_Config.m_shalawaLegitAvoidVisualsText);
        },
        [this]() { return g_Config.m_shalawaAvoidMode == 1; }
    });

    m_FeatureCards[1].push_back({"avoid_legit", "Legit Settings", "Legit avoidance", -1, "avoidlegit", false,
        [this]() {
            RenderToggle("Avoid Freeze", &g_Config.m_shalawaLegitAvoidFreeze);
            RenderSlider("Delay (ms)", &g_Config.m_shalawaLegitAvoidDelay, 0, 500);
            RenderSlider("Avoid Ticks", &g_Config.m_shalawaLegitAvoidTicks, 0, 5);
            RenderSlider("Predict Ticks", &g_Config.m_shalawaLegitPredictTicks, 0, 20);
            RenderSlider("Sensitivity", &g_Config.m_shalawaLegitAvoidSensitivity, 1, 10);
            RenderToggle("Hook Assist", &g_Config.m_shalawaLegitHookAssist);
            RenderSlider("Hook Duration", &g_Config.m_shalawaLegitHookDuration, 1, 5);
            RenderSlider("Hook Ticks", &g_Config.m_shalawaLegitHookTicks, 1, 5);
            RenderSlider("Hook Sensitivity", &g_Config.m_shalawaLegitHookAssistSensitivity, 1, 10);
            RenderSlider("AFK Protection (ms)", &g_Config.m_shalawaLegitAfkProtection, 0, 1000);
            RenderSlider("Unfreeze Check Radius", &g_Config.m_shalawaLegitUnfreezeCheckRadius, 1, 20);
            RenderToggle("Avoid Teles", &g_Config.m_shalawaLegitAvoidTeles);
            RenderToggle("Avoid Death", &g_Config.m_shalawaLegitAvoidDeath);
        },
        [this]() { return g_Config.m_shalawaAvoidMode == 1; }
    });
    
    m_FeatureCards[1].push_back({"avoid_blatant", "Blatant Settings", "Blatant avoidance", -1, "avoidrage", false,
        [this]() {
            RenderToggle("Avoid Freeze", &g_Config.m_shalawaAvoidFreeze);
            RenderToggle("Block Input", &g_Config.m_shalawaAvoidBlockInput);
            RenderToggle("Can Set Direction", &g_Config.m_shalawaAvoidCanSetDir);
            RenderSlider("Min Ticks", &g_Config.m_shalawaAvoidMinTicks, 0, 30);
            RenderSlider("Max Ticks", &g_Config.m_shalawaAvoidMaxTicks, 0, 30);
            RenderSlider("Velocity Factor", &g_Config.m_shalawaAvoidVelocityTickFactor, 1, 50);
            RenderSlider("FOV", &g_Config.m_shalawaAvoidFov, 0, 360);
        },
        [this]() { return g_Config.m_shalawaAvoidMode == 2; }
    });

    m_FeatureCards[1].push_back({"avoid_visuals", "Avoid Visuals", "Visual indicators", -1, "avoidlegit", false,
        [this]() {
            RenderToggle("Enable Visuals", &g_Config.m_shalawaAvoidVisuals);
            RenderToggle("Danger Zones", &g_Config.m_shalawaAvoidDrawDangerZones);
            RenderToggle("Hitbox Circles", &g_Config.m_shalawaAvoidDrawHitbox);
            RenderToggle("Predicted Path", &g_Config.m_shalawaAvoidDrawPrediction);
            RenderToggle("Escape Path", &g_Config.m_shalawaAvoidDrawEscapePath);
            RenderToggle("Hook Target", &g_Config.m_shalawaAvoidDrawHookTarget);
            RenderToggle("Escape Arrow", &g_Config.m_shalawaAvoidDrawEscapeDir);
            RenderToggle("Danger Indicator", &g_Config.m_shalawaAvoidDrawDangerIndicator);
            RenderToggle("Status Overlay", &g_Config.m_shalawaAvoidDrawStatus);
        },
        [this]() { return g_Config.m_shalawaAvoidMode == 2 && g_Config.m_shalawaAvoidVisuals != 0; }
    });
    
    m_FeatureCards[2].push_back({"tas_mode", "TAS Mode", "Basic or Auto", -1, "tasmode", false,
        [this]() {
            static const char *modes[] = {"Basic", "Auto"};
            if(RenderCombo("Mode", &g_Config.m_shalawaTasAutoMode, modes, 2))
            {
                if(g_Config.m_shalawaTasAutoMode == 0)
                    g_Config.m_shalawaTasAutoRunning = 0;
            }
        },
        [this]() { return false; }
    });

    m_FeatureCards[2].push_back({"tas_auto_runner", "Auto Runner", "Plan and follow", -1, "tasauto", false,
        [this]() {
            if(RenderButton(g_Config.m_shalawaTasAutoRunning ? "Stop" : "Start"))
                g_Config.m_shalawaTasAutoRunning = !g_Config.m_shalawaTasAutoRunning;
            RenderSlider("Replan Ticks", &g_Config.m_shalawaTasAutoReplanTicks, 1, 50);
            RenderSlider("Plan Ticks", &g_Config.m_shalawaTasAutoPlanTicks, 5, 200);
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 1; }
    });

    m_FeatureCards[2].push_back({"tas_auto_search", "Auto Search", "Population search", -1, "tasauto", false,
        [this]() {
            RenderSlider("Population Size", &g_Config.m_shalawaTasAutoPopulation, 4, 256);
            RenderSlider("Top-K Candidates", &g_Config.m_shalawaTasAutoTopK, 1, 64);
            RenderSlider("Generations", &g_Config.m_shalawaTasAutoGenerations, 1, 16);
            RenderSlider("Mutation Rate", &g_Config.m_shalawaTasAutoMutation, 0, 100);
            RenderSlider("Hook Bias", &g_Config.m_shalawaTasAutoHookBias, 0, 100);
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 1; }
    });

    m_FeatureCards[2].push_back({"tas_auto_scoring", "Auto Scoring", "Safety vs speed", -1, "tasauto", false,
        [this]() {
            RenderSlider("Safety Weight", &g_Config.m_shalawaTasAutoSafetyWeight, 0, 200);
            RenderSlider("Speed Weight", &g_Config.m_shalawaTasAutoSpeedWeight, 0, 200);
            RenderSlider("Progress Weight", &g_Config.m_shalawaTasAutoProgressWeight, 0, 200);
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 1; }
    });

    m_FeatureCards[2].push_back({"tas_controls", "TAS Controls", "Tool-assisted", -1, "tascontrol", false,
        [this]() {
            RenderSlider("Tick Speed", &g_Config.m_shalawaTasTickSpeed, 1, 50);
            RenderToggle("Legit Mode", &g_Config.m_shalawaTasLegitMode);
            if(RenderButton("Init/Reset"))
            {
                if(m_pGameClient)
                    m_pGameClient->m_Controls.TasInit();
            }
            if(RenderButton("Clear", false))
            {
                if(m_pGameClient)
                    m_pGameClient->m_Controls.ClearTasRecords();
            }
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 0; }
    });
    
    m_FeatureCards[2].push_back({"tas_playback", "Playback", "Play and record", -1, "tasplayback", false,
        [this]() {
            if(RenderButton(g_Config.m_shalawaPlaying ? "Stop Play" : "Play"))
                g_Config.m_shalawaPlaying = !g_Config.m_shalawaPlaying;
            if(RenderButton(g_Config.m_shalawaRecording ? "Stop Rec" : "Record"))
                g_Config.m_shalawaRecording = !g_Config.m_shalawaRecording;
            if(RenderButton(g_Config.m_shalawaTasPause ? "Unpause" : "Pause", false))
                g_Config.m_shalawaTasPause = !g_Config.m_shalawaTasPause;
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 0; }
    });
    
    m_FeatureCards[2].push_back({"tas_visuals", "TAS Visuals", "Visual helpers", -1, "tasvisuals", false,
        [this]() {
            RenderToggle("Draw Start/End", &g_Config.m_shalawaTasDrawStartEndPos);
            RenderToggle("Draw Path", &g_Config.m_shalawaTasDrawPath);
            RenderSlider("Path Width", &g_Config.m_shalawaTasDrawPathWidth, 1, 20);
            RenderToggle("Predict Line", &g_Config.m_shalawaPredictTas);
            RenderSlider("Predict Width", &g_Config.m_shalawaTasPredictWidth, 1, 20);
            RenderToggle("TAS HUD", &g_Config.m_shalawaTasHud);
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 0 && (g_Config.m_shalawaTasDrawPath || g_Config.m_shalawaPredictTas || g_Config.m_shalawaTasDrawStartEndPos); }
    });

    m_FeatureCards[2].push_back({"tas_replays", "TAS Replays", "Save/load recordings", -1, "tasreplay", false,
        [this]() {
            if(m_TasReplayListDirty)
                RefreshTasReplayList();

            if(m_aTasRecordName[0] == '\0')
                str_copy(m_aTasRecordName, g_Config.m_shalawaRecordName, sizeof(m_aTasRecordName));
            if(RenderInputText("Replay Name", m_aTasRecordName, sizeof(m_aTasRecordName), "Replay name"))
                str_copy(g_Config.m_shalawaRecordName, m_aTasRecordName, sizeof(g_Config.m_shalawaRecordName));

            if(!m_vTasReplays.empty())
            {
                std::vector<const char *> aItems;
                aItems.reserve(m_vTasReplays.size());
                for(const auto &Name : m_vTasReplays)
                    aItems.push_back(Name.c_str());

                if(RenderCombo("Saved Replays", &m_TasReplayIndex, aItems.data(), (int)aItems.size()))
                {
                    if(m_TasReplayIndex >= 0 && m_TasReplayIndex < (int)m_vTasReplays.size())
                    {
                        str_copy(g_Config.m_shalawaRecordName, m_vTasReplays[m_TasReplayIndex].c_str(), sizeof(g_Config.m_shalawaRecordName));
                        str_copy(m_aTasRecordName, g_Config.m_shalawaRecordName, sizeof(m_aTasRecordName));
                    }
                }
            }

            if(RenderButton("Save"))
            {
                if(m_pGameClient)
                    m_pGameClient->m_Controls.SaveRecord();
                m_TasReplayListDirty = true;
            }
            if(RenderButton("Load", false))
            {
                if(m_pGameClient)
                    m_pGameClient->m_Controls.LoadRecordsFromFile(g_Config.m_shalawaRecordName);
            }
            if(RenderButton("Delete", false))
            {
                if(m_pGameClient)
                    m_pGameClient->m_Controls.DeleteRecord(g_Config.m_shalawaRecordName);
                m_TasReplayListDirty = true;
            }
            if(RenderButton("Refresh", false))
            {
                m_TasReplayListDirty = true;
            }
        },
        [this]() { return g_Config.m_shalawaTasAutoMode == 0; }
    });

    m_FeatureCards[3].push_back({"auto_answer", "Auto Answer", "Auto responses", -1, "autoanswer", false,
        [this]() {
            RenderToggle("Enable", &g_Config.m_shalawaAutoAnswer);
            RenderToggle("Include Name", &g_Config.m_shalawaAutoAnswerIncludeName);
            RenderToggle("Team Chat", &g_Config.m_shalawaAutoAnswerTeam);
            RenderSlider("Delay (ms)", &g_Config.m_shalawaAutoAnswerDelay, 100, 10000);
            
            RenderSectionHeader("Add Trigger");
            RenderInputText("##trigger", m_aAutoAnswerTrigger, sizeof(m_aAutoAnswerTrigger), "Trigger word");
            RenderInputText("##response", m_aAutoAnswerResponse, sizeof(m_aAutoAnswerResponse), "Response");
            if(RenderButton("Add Trigger"))
            {
                if(m_pAutoAnswer && m_aAutoAnswerTrigger[0] && m_aAutoAnswerResponse[0])
                {
                    m_pAutoAnswer->AddTrigger(m_aAutoAnswerTrigger, m_aAutoAnswerResponse, false, false);
                    m_aAutoAnswerTrigger[0] = '\0';
                    m_aAutoAnswerResponse[0] = '\0';
                }
            }
            
            if(m_pAutoAnswer && !m_pAutoAnswer->GetTriggers().empty())
            {
                RenderSectionHeader("Triggers");
                auto &triggers = m_pAutoAnswer->GetTriggers();
                int toRemove = -1;
                for(size_t i = 0; i < triggers.size(); i++)
                {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "%s -> %s", triggers[i].m_Trigger.c_str(), triggers[i].m_Response.c_str());
                    
                    ImGui::PushID((int)i);
                    float width = ImGui::GetContentRegionAvail().x;
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(width, 22.0f));
                    
                    ImDrawList *d = ImGui::GetWindowDrawList();
                    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + 22), 
                        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgTertiary), 4.0f);
                    
                    d->PushClipRect(pos, ImVec2(pos.x + width - 25, pos.y + 22), true);
                    d->AddText(ImVec2(pos.x + 6, pos.y + 4), 
                        ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), buf);
                    d->PopClipRect();
                    
                    ImVec2 btnPos(pos.x + width - 20, pos.y + 4);
                    ImGui::SetCursorScreenPos(btnPos);
                    if(ImGui::InvisibleButton("##del", ImVec2(14, 14)))
                        toRemove = (int)i;
                    
                    if(ImGui::IsItemHovered())
                        d->AddText(btnPos, IM_COL32(255, 100, 100, 255), "X");
                    else
                        d->AddText(btnPos, IM_COL32(150, 150, 150, 255), "X");
                    
                    ImGui::PopID();
                }
                if(toRemove >= 0)
                    m_pAutoAnswer->RemoveTrigger(toRemove);
            }
            
            if(m_pAutoAnswer && !m_pAutoAnswer->GetTriggers().empty())
            {
                if(RenderButton("Clear All", false))
                    m_pAutoAnswer->ClearTriggers();
            }
        },
        [this]() { return g_Config.m_shalawaAutoAnswer != 0; }
    });
    
    m_FeatureCards[3].push_back({"chat_spam", "Chat Spam", "Auto messages", -1, "chatspam", false,
        [this]() {
            RenderToggle("Enable", &g_Config.m_shalawaChatSpam);
            RenderToggle("Loop", &g_Config.m_shalawaChatSpamLoop);
            RenderToggle("Team Chat", &g_Config.m_shalawaChatSpamTeam);
            RenderSlider("Delay (ms)", &g_Config.m_shalawaChatSpamDelay, 500, 60000);
            
            RenderSectionHeader("Add Message");
            RenderInputText("##msg", m_aChatSpamMessage, sizeof(m_aChatSpamMessage), "Message to spam");
            if(RenderButton("Add Message"))
            {
                if(m_pChatSpam && m_aChatSpamMessage[0])
                {
                    m_pChatSpam->AddMessage(m_aChatSpamMessage);
                    m_aChatSpamMessage[0] = '\0';
                }
            }
            
            if(m_pChatSpam && !m_pChatSpam->GetMessages().empty())
            {
                RenderSectionHeader("Messages");
                auto &messages = m_pChatSpam->GetMessages();
                int toRemove = -1;
                for(size_t i = 0; i < messages.size(); i++)
                {
                    ImGui::PushID((int)i);
                    float width = ImGui::GetContentRegionAvail().x;
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(width, 22.0f));
                    
                    ImDrawList *d = ImGui::GetWindowDrawList();
                    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + 22), 
                        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgTertiary), 4.0f);
                    
                    d->PushClipRect(pos, ImVec2(pos.x + width - 25, pos.y + 22), true);
                    d->AddText(ImVec2(pos.x + 6, pos.y + 4), 
                        ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), messages[i].c_str());
                    d->PopClipRect();
                    
                    ImVec2 btnPos(pos.x + width - 20, pos.y + 4);
                    ImGui::SetCursorScreenPos(btnPos);
                    if(ImGui::InvisibleButton("##del", ImVec2(14, 14)))
                        toRemove = (int)i;
                    
                    if(ImGui::IsItemHovered())
                        d->AddText(btnPos, IM_COL32(255, 100, 100, 255), "X");
                    else
                        d->AddText(btnPos, IM_COL32(150, 150, 150, 255), "X");
                    
                    ImGui::PopID();
                }
                if(toRemove >= 0)
                    m_pChatSpam->RemoveMessage(toRemove);
            }
            
            if(m_pChatSpam)
            {
                if(RenderButton(m_pChatSpam->IsRunning() ? "Stop Spam" : "Start Spam", !m_pChatSpam->IsRunning()))
                {
                    if(m_pChatSpam->IsRunning()) 
                        m_pChatSpam->Stop();
                    else 
                        m_pChatSpam->Start();
                }
                
                if(!m_pChatSpam->GetMessages().empty())
                {
                    if(RenderButton("Clear All", false))
                        m_pChatSpam->ClearMessages();
                }
            }
        },
        [this]() { return g_Config.m_shalawaChatSpam != 0; }
    });

    m_FeatureCards[4].push_back({"theme_accent", "Accent Color", "Menu colors", -1, "accentcolormenu", false,
        [this]() {
            if(RenderButton(m_Config.m_ColorSettings.m_Mode == 0 ? "> Solid" : "  Solid", m_Config.m_ColorSettings.m_Mode == 0))
                m_Config.m_ColorSettings.m_Mode = 0;
            if(RenderButton(m_Config.m_ColorSettings.m_Mode == 1 ? "> Gradient" : "  Gradient", m_Config.m_ColorSettings.m_Mode == 1))
                m_Config.m_ColorSettings.m_Mode = 1;
            
            if(m_Config.m_ColorSettings.m_Mode == 0)
            {
                RenderColorPicker("Accent", m_Config.m_ColorSettings.m_SolidColor);
            }
            else
            {
                for(int i = 0; i < m_Config.m_ColorSettings.m_GradientColorCount; i++)
                {
                    char label[32];
                    snprintf(label, sizeof(label), "Color %d", i + 1);
                    RenderColorPicker(label, m_Config.m_ColorSettings.m_GradientColors[i]);
                }
                if(m_Config.m_ColorSettings.m_GradientColorCount < 4) 
                    if(RenderButton("+ Add", false)) m_Config.m_ColorSettings.m_GradientColorCount++;
                if(m_Config.m_ColorSettings.m_GradientColorCount > 2) 
                    if(RenderButton("- Remove", false)) m_Config.m_ColorSettings.m_GradientColorCount--;
                RenderSliderFloat("Speed", &m_Config.m_ColorSettings.m_AnimationSpeed, 1.0f, 10.0f);
            }
        },
        [this]() { return m_Config.m_ColorSettings.m_Mode == 1; }
    });
    
    m_FeatureCards[4].push_back({"theme_mode", "Theme", "Light/Dark", -1, "thememenu", false,
        [this]() {
            if(RenderButton(m_Config.m_Theme == 0 ? "> Dark" : "  Dark", m_Config.m_Theme == 0))
            {
                m_Config.m_Theme = 0;
                m_Theme.m_BgPrimary = ImVec4(0.055f, 0.055f, 0.063f, 1.0f);
                m_Theme.m_BgSecondary = ImVec4(0.094f, 0.094f, 0.106f, 1.0f);
                m_Theme.m_BgTertiary = ImVec4(0.122f, 0.122f, 0.137f, 1.0f);
                m_Theme.m_BgHover = ImVec4(0.149f, 0.149f, 0.173f, 1.0f);
                m_Theme.m_TextPrimary = ImVec4(0.937f, 0.937f, 0.945f, 1.0f);
                m_Theme.m_TextSecondary = ImVec4(0.678f, 0.678f, 0.722f, 1.0f);
                m_Theme.m_TextTertiary = ImVec4(0.518f, 0.518f, 0.580f, 1.0f);
            }
            if(RenderButton(m_Config.m_Theme == 1 ? "> Light" : "  Light", m_Config.m_Theme == 1))
            {
                m_Config.m_Theme = 1;
                m_Theme.m_BgPrimary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                m_Theme.m_BgSecondary = ImVec4(0.96f, 0.96f, 0.97f, 1.0f);
                m_Theme.m_BgTertiary = ImVec4(0.93f, 0.93f, 0.94f, 1.0f);
                m_Theme.m_BgHover = ImVec4(0.90f, 0.90f, 0.91f, 1.0f);
                m_Theme.m_TextPrimary = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
                m_Theme.m_TextSecondary = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
                m_Theme.m_TextTertiary = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);
            }
        },
        [this]() { return false; }
    });
    
    m_FeatureCards[4].push_back({"appearance", "Appearance", "Menu settings", -1, "appearance", false,
        [this]() {
            float opacity = m_Config.m_MenuOpacity * 100.0f;
            if(RenderSliderFloat("Opacity", &opacity, 50.0f, 100.0f))
                m_Config.m_MenuOpacity = opacity / 100.0f;
            RenderToggle("Animations", &m_Config.m_Animations);
            RenderToggle("Collapse Sidebar", &m_Config.m_CollapseSidebar);
            RenderToggle("Compact Mode", &m_Config.m_CompactMode);
            RenderToggle("Descriptions", &m_Config.m_ShowDescriptions);
        },
        [this]() { return false; }
    });
    
    m_FeatureCards[4].push_back({"watermark_main", "Watermark", "Main display", -1, "menuvisuals", false,
        [this]() {
            RenderToggle("Enable", &g_Config.m_shalawaWatermark);
            static const char *styles[] = {"Modern", "Rise", "Gamesense", "NL", "Skeet", "Stylo", "Minimal", "Pills"};
            RenderCombo("Style", &g_Config.m_shalawaWatermarkStyle, styles, 8);
        },
        [this]() { return g_Config.m_shalawaWatermark != 0; }
    });

    m_FeatureCards[4].push_back({"watermark_info", "Watermark Info", "Display elements", -1, "menuvisuals", false,
        [this]() {
            RenderToggle("Show FPS", &g_Config.m_shalawaWatermarkFps);
            RenderToggle("Show Ping", &g_Config.m_shalawaWatermarkPing);
            RenderToggle("Show Time", &g_Config.m_shalawaWatermarkTime);
            RenderToggle("Show Server", &g_Config.m_shalawaWatermarkServer);
            RenderToggle("Show Players", &g_Config.m_shalawaWatermarkPlayers);
        },
        [this]() { return g_Config.m_shalawaWatermark != 0; }
    });

    m_FeatureCards[4].push_back({"watermark_hud", "HUD Elements", "Extra displays", -1, "menuvisuals", false,
        [this]() {
            RenderToggle("Keybinds Display", &g_Config.m_shalawaWatermarkKeybinds);
            RenderToggle("Target HUD", &g_Config.m_shalawaWatermarkTarget);
            RenderToggle("Notifications", &g_Config.m_shalawaWatermarkNotifications);
        },
        [this]() { return g_Config.m_shalawaWatermarkKeybinds || g_Config.m_shalawaWatermarkTarget; }
    });

    m_FeatureCards[4].push_back({"hide_cheat", "Hide Cheat", "Hide menu", -1, "hidecheat", false,
        [this]() {
            RenderSubText("Press DELETE to unhide");
            if(RenderButton("Hide Now"))
            {
                g_Config.m_shalawaHideCheat = 1;
                m_MenuOpen = false;
            }
        },
        [this]() { return g_Config.m_shalawaHideCheat != 0; }
    });

    m_FeatureCards[5].push_back({"config_manage", "Configs", "Save/Load", -1, "configs", false,
        [this]() {
            RenderInputText("##cfgname", m_ConfigName, sizeof(m_ConfigName), "Config name");
            RenderButton("Save");
            RenderButton("Load", false);
            RenderButton("Export", false);
            RenderButton("Import", false);
        },
        [this]() { return false; }
    });

    m_FeatureCards[6].push_back({"keybinds", "Keybinds", "Shortcuts", -1, "keybinds", false,
        [this]() {
            RenderSubText("Menu: INSERT");
            RenderSubText("Unhide: DELETE");
        },
        [this]() { return false; }
    });

    m_FeatureCards[6].push_back({"menu_style", "Menu Style", "Switch menu type", -1, "setting", false,
        [this]() {
            CImGuiManager &manager = CImGuiManager::Instance();
            if(RenderButton(g_Config.m_shalawaMenuStyle == 0 ? "  ClickGUI" : "> ClickGUI", g_Config.m_shalawaMenuStyle == 0))
            {
                g_Config.m_shalawaMenuStyle = 0;
                manager.m_MenuStyle = 0;
                manager.m_Config.m_MenuStyle = 0;
            }
            if(RenderButton(g_Config.m_shalawaMenuStyle == 1 ? "> Modern" : "  Modern", g_Config.m_shalawaMenuStyle == 1))
            {
                g_Config.m_shalawaMenuStyle = 1;
                manager.m_MenuStyle = 1;
                manager.m_Config.m_MenuStyle = 1;
            }
        },
        [this]() { return false; }
    });

    m_FeatureCards[7].push_back({"info_credits", "Credits", "Developer info", -1, "info", false,
        [this]() {
            ImDrawList *d = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            
            float pulse = 0.7f + 0.3f * sinf(m_GlowPulse * 2.0f);
            ImVec4 accentPulse = GetAccentColor(pulse);
            
            d->AddText(ImGui::GetFont(), 16.0f, pos, 
                ImGui::ColorConvertFloat4ToU32(accentPulse), "soup ezy coder <3");
            d->AddText(ImGui::GetFont(), 14.0f, ImVec2(pos.x, pos.y + 24), 
                ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), "- by ueh");
            
            ImGui::Dummy(ImVec2(0, 50));
        },
        [this]() { return false; }
    });

    m_FeaturesNav.clear();
    m_FeaturesNav.push_back({"aim", "Aimbot", 0});
    m_FeaturesNav.push_back({"avoid", "Avoid", 1});
    m_FeaturesNav.push_back({"tas", "TAS", 2});
    m_FeaturesNav.push_back({"misc", "Misc", 3});

    m_SystemNav.clear();
    m_SystemNav.push_back({"menuvisuals", "Visuals", 4});
    m_SystemNav.push_back({"configs", "Configs", 5});
    m_SystemNav.push_back({"setting", "Settings", 6});
    m_SystemNav.push_back({"info", "Info", 7});
}

GLuint CImGuiManagerModern::LoadTextureFromPng(const char *path)
{
    if(!m_pGameClient || !m_pGameClient->Graphics())
        return 0;
    
    IGraphics *pGraphics = m_pGameClient->Graphics();
    CImageInfo ImgInfo;
    
    if(!pGraphics->LoadPng(ImgInfo, path, IStorage::TYPE_ALL))
        return 0;
    
    GLuint texId = 0;
    glGenTextures(1, &texId);
    
    if(texId == 0)
    {
        ImgInfo.Free();
        return 0;
    }
    
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    GLenum format = (ImgInfo.m_Format == CImageInfo::FORMAT_RGB) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, format, ImgInfo.m_Width, ImgInfo.m_Height, 
                0, format, GL_UNSIGNED_BYTE, ImgInfo.m_pData);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    ImgInfo.Free();
    
    return texId;
}

void CImGuiManagerModern::LoadTabIcons()
{
    if(m_TabIconsLoaded)
        return;
    
    if(!m_pGameClient || !m_pGameClient->Graphics())
        return;
    
    struct { const char *key; const char *file; } icons[] = {
        {"avoid", "data/shalawamenuicnos/avoid.png"},
        {"tas", "data/shalawamenuicnos/tas.png"},
        {"misc", "data/shalawamenuicnos/misc.png"},
        {"menuvisuals", "data/shalawamenuicnos/visuals.png"},
        {"configs", "data/shalawamenuicnos/configsnya.png"},
        {"setting", "data/shalawamenuicnos/setting.png"},
        {"info", "data/shalawamenuicnos/info.png"},
    };
    
    for(auto &icon : icons)
    {
        GLuint tex = LoadTextureFromPng(icon.file);
        if(tex != 0)
        {
            m_TabIconsGL[icon.key] = tex;
            dbg_msg("imgui", "tab icon loaded: %s -> GL_ID=%u", icon.key, tex);
        }
        else
        {
            dbg_msg("imgui", "failed load tab icon: %s", icon.file);
        }
    }
    
    m_TabIconsLoaded = true;
}

void CImGuiManagerModern::RenderSidebar(ImVec2 pos, ImVec2 size)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    float sidebarWidth = m_Config.m_CollapseSidebar ? 56.0f : 160.0f;
    
    d->AddRectFilled(pos, ImVec2(pos.x + sidebarWidth, pos.y + size.y), 
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgSecondary), 12.0f, ImDrawFlags_RoundCornersLeft);
    
    if(!m_Config.m_CollapseSidebar)
    {
        const char *title = "SHALAWA";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        d->AddText(ImVec2(pos.x + (sidebarWidth - titleSize.x) / 2, pos.y + 18), 
            ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextPrimary), title);
        
        const char *version = "v1";
        ImVec2 versionSize = ImGui::CalcTextSize(version);
        ImVec2 versionPos(pos.x + (sidebarWidth - versionSize.x - 12) / 2, pos.y + 38);
        d->AddRectFilled(versionPos, ImVec2(versionPos.x + versionSize.x + 12, versionPos.y + versionSize.y + 4),
            GetAccentU32(0.9f), 4.0f);
        d->AddText(ImVec2(versionPos.x + 6, versionPos.y + 2), IM_COL32(255, 255, 255, 255), version);
    }
    
    float y = 70.0f;

    auto renderNav = [&](std::vector<SNavItem> &items) {
        for(size_t i = 0; i < items.size(); i++)
        {
            float itemHeight = 36.0f;
            float itemWidth = sidebarWidth - 12;
            
            bool isActive = m_CurrentTab == items[i].m_Tab;
            float selectAnim = GetAnimValue("nav_" + std::to_string(items[i].m_Tab), isActive ? 1.0f : 0.0f);
            
            ImVec2 itemMin(pos.x + 6, pos.y + y);
            ImVec2 itemMax(itemMin.x + itemWidth, itemMin.y + itemHeight);
            
            ImGui::SetCursorScreenPos(itemMin);
            ImGui::PushID(items[i].m_Tab);
            bool clicked = ImGui::InvisibleButton("##nav", ImVec2(itemWidth, itemHeight));
            bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            
            if(clicked) m_CurrentTab = items[i].m_Tab;
            
            float hoverAnim = GetAnimValue("nav_h_" + std::to_string(items[i].m_Tab), hovered ? 1.0f : 0.0f);
            ImVec4 bgClr = LerpColor(ImVec4(0, 0, 0, 0), GetAccentColor(0.8f), selectAnim * 0.9f + hoverAnim * 0.1f);
            d->AddRectFilled(itemMin, itemMax, ImGui::ColorConvertFloat4ToU32(bgClr), 6.0f);
            
            if(selectAnim > 0.01f)
            {
                float indH = (itemHeight - 10) * selectAnim;
                d->AddRectFilled(ImVec2(itemMin.x, itemMin.y + (itemHeight - indH) / 2),
                    ImVec2(itemMin.x + 3, itemMin.y + (itemHeight + indH) / 2), GetAccentU32(selectAnim), 2.0f);
            }
            
            if(m_Config.m_CollapseSidebar)
            {
                auto it = m_TabIconsGL.find(items[i].m_Icon);
                if(it != m_TabIconsGL.end() && it->second != 0)
                {
                    float iconSize = 20.0f;
                    float iconX = itemMin.x + (itemWidth - iconSize) / 2;
                    float iconY = itemMin.y + (itemHeight - iconSize) / 2;
                    
                    ImTextureID tex = (ImTextureID)(intptr_t)it->second;
                    float alpha = 0.7f + selectAnim * 0.3f + hoverAnim * 0.2f;
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, alpha));
                    
                    d->AddImage(tex, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize),
                        ImVec2(0, 0), ImVec2(1, 1), col);
                }
                else
                {
                    const char *fallback = items[i].m_Label.c_str();
                    char letter[2] = {fallback[0], 0};
                    ImVec2 textSize = ImGui::CalcTextSize(letter);
                    ImVec4 textClr = LerpColor(m_Theme.m_TextSecondary, ImVec4(1, 1, 1, 1), selectAnim);
                    d->AddText(ImVec2(itemMin.x + (itemWidth - textSize.x) / 2, itemMin.y + (itemHeight - textSize.y) / 2),
                        ImGui::ColorConvertFloat4ToU32(textClr), letter);
                }
            }
            else
            {
                ImVec4 textClr = LerpColor(m_Theme.m_TextSecondary, ImVec4(1, 1, 1, 1), selectAnim);
                d->AddText(ImVec2(itemMin.x + 8, itemMin.y + (itemHeight - 14) / 2),
                    ImGui::ColorConvertFloat4ToU32(textClr), items[i].m_Label.c_str());
            }
            
            y += itemHeight + 4;
        }
    };
    
    renderNav(m_FeaturesNav);
    y += 8;
    d->AddLine(ImVec2(pos.x + 10, pos.y + y), ImVec2(pos.x + sidebarWidth - 10, pos.y + y),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_Border));
    y += 12;
    renderNav(m_SystemNav);
    
    d->AddLine(ImVec2(pos.x + sidebarWidth, pos.y + 8), ImVec2(pos.x + sidebarWidth, pos.y + size.y - 8),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_Border));
}

bool CImGuiManagerModern::ProcessEvent(SDL_Event *event)
{
    if(!m_Initialized)
        return false;
    
    if(m_pGameClient)
    {
        CShalawaHud &hud = CShalawaHud::Instance();
        if(hud.GetEditor().IsActive())
        {
            if(hud.ProcessEvent(event))
                return true;
        }
    }


    if(event->type == SDL_KEYDOWN)
    {
        if(event->key.keysym.scancode == SDL_SCANCODE_DELETE && g_Config.m_shalawaHideCheat)
        {
            g_Config.m_shalawaHideCheat = 0;
            m_MenuOpen = true;
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_ShowCursor(SDL_ENABLE);
            return true;
        }
        
        if(event->key.keysym.scancode == SDL_SCANCODE_INSERT)
        {
            m_MenuOpen = !m_MenuOpen;
            CImGuiManager::Instance().m_MenuOpen = m_MenuOpen;
            if(m_MenuOpen)
            {
                g_Config.m_shalawaHideCheat = 0;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
            }
            else
            {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_ShowCursor(SDL_DISABLE);
            }
            return true;
        }
    }

    if(!m_MenuOpen)
        return false;

    return ImGui_ImplSDL2_ProcessEvent(event);
}

void CImGuiManagerModern::Shutdown()
{
    if(!m_Initialized)
        return;
    m_Initialized = false;
}

void CImGuiManagerModern::BeginFrame()
{
}

void CImGuiManagerModern::EndFrame()
{
}

void CImGuiManagerModern::RenderContent_Internal()
{
    UpdateAnimations();
    
    if(!m_TabIconsLoaded && m_pGameClient)
        LoadTabIcons();
    
    m_Watermark.Render();
    
    if(m_MenuOpen || m_MenuAlpha >= 0.01f)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 menuSize(850, 520);
        ImVec2 menuPos((io.DisplaySize.x - menuSize.x) / 2, (io.DisplaySize.y - menuSize.y) / 2);
        
        ImVec2 scaledSize(menuSize.x * m_MenuScale, menuSize.y * m_MenuScale);
        ImVec2 scaledPos(menuPos.x + (menuSize.x - scaledSize.x) / 2, menuPos.y + (menuSize.y - scaledSize.y) / 2);
        
        ImGui::SetNextWindowPos(scaledPos);
        ImGui::SetNextWindowSize(scaledSize);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_MenuAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
        
        if(ImGui::Begin("##shalawa", nullptr, flags))
        {
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            
            RenderBackground(winPos, winSize);
            RenderSidebar(winPos, winSize);
            RenderHeader(winPos, winSize);
            RenderContent(winPos, winSize);
        }
        ImGui::End();
        
        ImGui::PopStyleVar(3);
    }

    if(m_pGameClient)
    {
        IInput *pInput = m_pGameClient->Input();
        if(pInput)
        {
            const bool WantTextInput = ImGui::GetIO().WantTextInput && (m_MenuOpen || m_MenuAlpha >= 0.01f);
            if(WantTextInput && !m_TextInputActive)
            {
                pInput->StartTextInput();
                m_TextInputActive = true;
            }
            else if(!WantTextInput && m_TextInputActive)
            {
                pInput->StopTextInput();
                m_TextInputActive = false;
            }
        }
    }
}

void CImGuiManagerModern::UpdateAnimations()
{
    double currentTime = (double)SDL_GetTicks() / 1000.0;
    m_DeltaTime = (float)(currentTime - m_LastTime);
    m_LastTime = currentTime;
    m_DeltaTime = std::clamp(m_DeltaTime, 0.0001f, 0.05f);
    
    m_GradientTime += m_DeltaTime * (1.0f / m_Config.m_ColorSettings.m_AnimationSpeed);
    if(m_GradientTime > 1.0f) m_GradientTime -= 1.0f;
    
    m_GlowPulse += m_DeltaTime * 3.0f;
    if(m_GlowPulse > 6.28318f) m_GlowPulse -= 6.28318f;
    
    float targetAlpha = m_MenuOpen ? m_Config.m_MenuOpacity : 0.0f;
    float targetScale = m_MenuOpen ? 1.0f : 0.85f;
    
    float speed = m_Config.m_Animations ? 12.0f : 100.0f;
    m_MenuAlpha += (targetAlpha - m_MenuAlpha) * m_DeltaTime * speed;
    m_MenuScale += (targetScale - m_MenuScale) * m_DeltaTime * speed;
    
    if(m_CurrentTab != m_PreviousTab)
    {
        m_TabTransition = 0.0f;
        m_PreviousTab = m_CurrentTab;
    }
    m_TabTransition += (1.0f - m_TabTransition) * m_DeltaTime * speed;
    m_TabTransition = std::clamp(m_TabTransition, 0.0f, 1.0f);
}

ImVec4 CImGuiManagerModern::GetAccentColor(float alpha)
{
    if(m_Config.m_ColorSettings.m_Mode == 0)
        return ImVec4(m_Config.m_ColorSettings.m_SolidColor[0], m_Config.m_ColorSettings.m_SolidColor[1], 
            m_Config.m_ColorSettings.m_SolidColor[2], alpha);
    return GetGradientColor(m_GradientTime, alpha);
}

ImVec4 CImGuiManagerModern::GetGradientColor(float t, float alpha)
{
    int count = m_Config.m_ColorSettings.m_GradientColorCount;
    if(count < 2) count = 2;
    
    float cycleT = fmodf(t * 2.0f, 1.0f);
    if(fmodf(t * 2.0f, 2.0f) >= 1.0f) cycleT = 1.0f - cycleT;
    
    float segment = 1.0f / (float)(count - 1);
    int idx = std::min((int)(cycleT / segment), count - 2);
    float localT = (cycleT - idx * segment) / segment;
    
    float *c1 = m_Config.m_ColorSettings.m_GradientColors[idx];
    float *c2 = m_Config.m_ColorSettings.m_GradientColors[idx + 1];
    
    return ImVec4(c1[0] + (c2[0] - c1[0]) * localT, c1[1] + (c2[1] - c1[1]) * localT,
        c1[2] + (c2[2] - c1[2]) * localT, alpha);
}

ImU32 CImGuiManagerModern::GetAccentU32(float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(GetAccentColor(alpha));
}

ImVec4 CImGuiManagerModern::LerpColor(ImVec4 a, ImVec4 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

float CImGuiManagerModern::GetAnimValue(const std::string &key, float target, float speed)
{
    auto it = m_AnimState.find(key);
    if(it == m_AnimState.end())
    {
        m_AnimState[key] = m_Config.m_Animations ? (target > 0.5f ? 0.0f : 1.0f) : target;
        it = m_AnimState.find(key);
    }
    
    float effectiveSpeed = m_Config.m_Animations ? speed : 100.0f;
    it->second += (target - it->second) * m_DeltaTime * effectiveSpeed;
    
    if(std::abs(it->second - target) < 0.001f)
        it->second = target;
    
    return it->second;
}

void CImGuiManagerModern::RenderBackground(ImVec2 pos, ImVec2 size)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    d->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgPrimary), 12.0f);
    
    for(int i = 3; i >= 0; i--)
    {
        float glowAlpha = (0.06f - i * 0.012f) * m_MenuAlpha * (0.7f + 0.3f * sinf(m_GlowPulse));
        d->AddRect(ImVec2(pos.x - i * 2, pos.y - i * 2), ImVec2(pos.x + size.x + i * 2, pos.y + size.y + i * 2),
            GetAccentU32(glowAlpha), 12.0f + i, 0, 2.0f);
    }
}


void CImGuiManagerModern::RenderHeader(ImVec2 pos, ImVec2 size)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    float sidebarWidth = m_Config.m_CollapseSidebar ? 56.0f : 160.0f;
    float headerHeight = 50.0f;
    
    float searchWidth = 280.0f;
    float searchX = pos.x + sidebarWidth + (size.x - sidebarWidth - searchWidth) / 2;
    RenderSearchBar(ImVec2(searchX, pos.y + 10), searchWidth);
    
    ImVec2 closePos(pos.x + size.x - 38, pos.y + 11);
    ImGui::SetCursorScreenPos(closePos);
    ImGui::PushID("close");
    bool closeClicked = ImGui::InvisibleButton("##close", ImVec2(28, 28));
    bool closeHovered = ImGui::IsItemHovered();
    ImGui::PopID();
    
    if(closeClicked)
    {
        m_MenuOpen = false;
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
    }
    
    float closeAnim = GetAnimValue("close", closeHovered ? 1.0f : 0.0f);
    ImVec4 closeBg = LerpColor(m_Theme.m_BgTertiary, ImVec4(0.9f, 0.2f, 0.3f, 1.0f), closeAnim);
    d->AddRectFilled(closePos, ImVec2(closePos.x + 28, closePos.y + 28), ImGui::ColorConvertFloat4ToU32(closeBg), 6.0f);
    
    float cx = closePos.x + 14, cy = closePos.y + 14;
    d->AddLine(ImVec2(cx - 5, cy - 5), ImVec2(cx + 5, cy + 5), IM_COL32(255, 255, 255, 200), 2.0f);
    d->AddLine(ImVec2(cx + 5, cy - 5), ImVec2(cx - 5, cy + 5), IM_COL32(255, 255, 255, 200), 2.0f);
    
    d->AddLine(ImVec2(pos.x + sidebarWidth + 15, pos.y + headerHeight),
        ImVec2(pos.x + size.x - 15, pos.y + headerHeight), ImGui::ColorConvertFloat4ToU32(m_Theme.m_Border));
}

void CImGuiManagerModern::RenderSearchBar(ImVec2 pos, float width)
{
    ImDrawList *d = ImGui::GetWindowDrawList();
    float height = 30.0f;
    
    float focusAnim = GetAnimValue("search_focus", m_SearchFocused ? 1.0f : 0.0f);
    ImVec4 bgClr = LerpColor(m_Theme.m_BgTertiary, m_Theme.m_BgHover, focusAnim);
    ImVec4 borderClr = LerpColor(m_Theme.m_Border, GetAccentColor(0.8f), focusAnim);
    
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(bgClr), 6.0f);
    d->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(borderClr), 6.0f);
    d->AddText(ImVec2(pos.x + 10, pos.y + (height - 14) / 2), ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), "?");
    
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 28, pos.y + 5));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, m_Theme.m_TextPrimary);
    ImGui::PushItemWidth(width - 40);
    ImGui::InputText("##search", m_SearchQuery, sizeof(m_SearchQuery));
    m_SearchFocused = ImGui::IsItemActive();
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void CImGuiManagerModern::RenderSearchResults()
{
}

void CImGuiManagerModern::RenderSectionHeader(const char *label)
{
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(width, 18.0f));
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetItemRectMin();
    d->AddText(ImGui::GetFont(), 12.0f, pos, ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), label);
}

void CImGuiManagerModern::RenderSubText(const char *text)
{
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(width, 18.0f));
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetItemRectMin();
    d->AddText(pos, ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), text);
}

void CImGuiManagerModern::RenderContent(ImVec2 pos, ImVec2 size)
{
    float sidebarWidth = m_Config.m_CollapseSidebar ? 56.0f : 160.0f;
    float headerHeight = 50.0f;
    
    ImVec2 contentPos(pos.x + sidebarWidth + 15, pos.y + headerHeight + 15);
    ImVec2 contentSize(size.x - sidebarWidth - 30, size.y - headerHeight - 30);
    
    const char *tabTitles[] = {"Aimbot", "Avoid", "TAS", "Misc", "Visuals", "Configs", "Settings", "Info"};
    const char *tabDescs[] = {"Aim assistance", "Avoidance", "Tool-assisted", "Utilities", "Appearance", "Configurations", "Preferences", "Beta Test Info"};
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    d->AddText(ImGui::GetFont(), 18.0f, contentPos, ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextPrimary), tabTitles[m_CurrentTab]);
    d->AddText(ImVec2(contentPos.x, contentPos.y + 22), ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), tabDescs[m_CurrentTab]);
    
    ImVec2 cardsPos(contentPos.x, contentPos.y + 50);
    ImVec2 cardsSize(contentSize.x, contentSize.y - 50);
    
    ImGui::SetCursorScreenPos(cardsPos);
    ImGui::BeginChild("##cards", cardsSize, false, ImGuiWindowFlags_NoBackground);


    if(m_CurrentTab >= 0 && m_CurrentTab < 8)
    {
        auto &cards = m_FeatureCards[m_CurrentTab];
        float cardWidth = cardsSize.x - 15;

        if(cards.empty())
        {
            ImDrawList *dd = ImGui::GetWindowDrawList();
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dd->AddText(ImVec2(cp.x + 10, cp.y + 10),
                IM_COL32(150, 150, 150, 200), "No content");
        }

        for(size_t i = 0; i < cards.size(); i++)
        {
            // Filter cards based on avoid mode
            if(m_CurrentTab == 1) // Avoid tab
            {
                if(cards[i].m_Id == "avoid_legit" && g_Config.m_shalawaAvoidMode != 1)
                    continue;
                if(cards[i].m_Id == "avoid_blatant" && g_Config.m_shalawaAvoidMode != 2)
                    continue;
                if(cards[i].m_Id == "avoid_visuals" && g_Config.m_shalawaAvoidMode != 2)
                    continue;
            }
            if(m_CurrentTab == 2) // TAS tab
            {
                const bool AutoMode = g_Config.m_shalawaTasAutoMode == 1;
                const bool IsAutoCard =
                    cards[i].m_Id == "tas_auto_runner" ||
                    cards[i].m_Id == "tas_auto_search" ||
                    cards[i].m_Id == "tas_auto_scoring";
                const bool IsBasicCard =
                    cards[i].m_Id == "tas_controls" ||
                    cards[i].m_Id == "tas_playback" ||
                    cards[i].m_Id == "tas_visuals" ||
                    cards[i].m_Id == "tas_replays";

                if(AutoMode && IsBasicCard)
                    continue;
                if(!AutoMode && IsAutoCard)
                    continue;
            }
            
            RenderFeatureCard(cards[i], cardWidth);
        }
    }
    
    ImGui::EndChild();
}

void CImGuiManagerModern::InitFromParent(SDL_Window *pWindow)
{
    if(m_Initialized)
        return;
    
    m_pWindow = pWindow;
    SetupStyle();
    InitFeatureCards();
    
    if(m_pGameClient)
        LoadIcons();
    
    m_Initialized = true;
    m_LastTime = (double)SDL_GetTicks() / 1000.0;
}

void CImGuiManagerModern::RenderFeatureCard(SFeatureCard &card, float width)
{
    float headerHeight = 52.0f;
    
    std::string animKey = "card_" + card.m_Id;
    std::string heightKey = "cardh_" + card.m_Id;
    
    float expandAnim = GetAnimValue(animKey, card.m_Expanded ? 1.0f : 0.0f, 8.0f);
    
    float storedHeight = 100.0f;
    auto it = m_AnimState.find(heightKey);
    if(it != m_AnimState.end())
        storedHeight = it->second;
    
    float contentHeight = storedHeight * expandAnim;
    float totalHeight = headerHeight + contentHeight;
    
    bool isEnabled = card.m_IsEnabled ? card.m_IsEnabled() : false;
    float enableAnim = GetAnimValue("carden_" + card.m_Id, isEnabled ? 1.0f : 0.0f);
    
    ImGui::PushID(card.m_Id.c_str());
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_Theme.m_BgSecondary);
    ImGui::PushStyleColor(ImGuiCol_Border, m_Theme.m_Border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGui::BeginChild("##card", ImVec2(width, totalHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    
    if(ImGui::InvisibleButton("##hdr", ImVec2(width, headerHeight)))
        card.m_Expanded = !card.m_Expanded;
    
    float textY = m_Config.m_ShowDescriptions ? (headerHeight - 28) / 2 : (headerHeight - 14) / 2;
    d->AddText(ImVec2(wp.x + 10, wp.y + textY), 
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextPrimary), card.m_Name.c_str());
    if(m_Config.m_ShowDescriptions)
        d->AddText(ImVec2(wp.x + 10, wp.y + textY + 14),
            ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), card.m_Description.c_str());
    
    float ax = wp.x + width - 20, ay = wp.y + headerHeight / 2;
    float ao = (0.5f - expandAnim) * 6.0f;
    d->AddLine(ImVec2(ax - 5, ay - ao), ImVec2(ax, ay + ao * 1.5f), ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), 2.0f);
    d->AddLine(ImVec2(ax, ay + ao * 1.5f), ImVec2(ax + 5, ay - ao), ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), 2.0f);
    
    if(card.m_RenderContent && (card.m_Expanded || expandAnim > 0.01f))
    {
        d->AddLine(ImVec2(wp.x + 8, wp.y + headerHeight), ImVec2(wp.x + width - 8, wp.y + headerHeight),
            ImGui::ColorConvertFloat4ToU32(ImVec4(m_Theme.m_Border.x, m_Theme.m_Border.y, m_Theme.m_Border.z, expandAnim)));
        
        ImGui::PushClipRect(ImVec2(wp.x, wp.y + headerHeight), ImVec2(wp.x + width, wp.y + totalHeight), true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * expandAnim);
        
        ImGui::SetCursorPos(ImVec2(10, headerHeight + 4));
        ImGui::PushItemWidth(width - 20);
        
        float y1 = ImGui::GetCursorPosY();
        card.m_RenderContent();
        float y2 = ImGui::GetCursorPosY();
        
        ImGui::PopItemWidth();
        ImGui::PopStyleVar();
        ImGui::PopClipRect();
        
        if(card.m_Expanded)
            m_AnimState[heightKey] = y2 - y1 + 12;
    }
    
    ImGui::EndChild();
    
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    
    ImGui::Spacing();
}

bool CImGuiManagerModern::RenderToggle(const char *label, bool *v)
{
    float width = ImGui::GetContentRegionAvail().x;
    float toggleW = 38.0f, toggleH = 20.0f, height = 26.0f;
    
    ImGui::PushID(label);
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##tog", ImVec2(width, height));
    if(clicked) *v = !*v;
    bool hovered = ImGui::IsItemHovered();
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    
    float anim = GetAnimValue(std::string("t_") + label, *v ? 1.0f : 0.0f);
    float hoverAnim = GetAnimValue(std::string("th_") + label, hovered ? 1.0f : 0.0f);
    
    d->AddText(ImVec2(pos.x, pos.y + (height - 14) / 2), ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), label);
    
    ImVec2 tPos(pos.x + width - toggleW, pos.y + (height - toggleH) / 2);
    ImVec4 trackBg = LerpColor(m_Theme.m_BgTertiary, GetAccentColor(0.9f), anim);
    d->AddRectFilled(tPos, ImVec2(tPos.x + toggleW, tPos.y + toggleH), ImGui::ColorConvertFloat4ToU32(trackBg), toggleH / 2);
    
    float knobR = (toggleH - 4) / 2;
    float knobX = tPos.x + 2 + knobR + anim * (toggleW - 4 - knobR * 2);
    d->AddCircleFilled(ImVec2(knobX, tPos.y + toggleH / 2), knobR * (1.0f + hoverAnim * 0.1f), IM_COL32(255, 255, 255, 255));
    
    ImGui::PopID();
    return clicked;
}

bool CImGuiManagerModern::RenderToggle(const char *label, int *v)
{
    bool boolVal = (*v != 0);
    bool changed = RenderToggle(label, &boolVal);
    if(changed) *v = boolVal ? 1 : 0;
    return changed;
}

bool CImGuiManagerModern::RenderSlider(const char *label, int *v, int min, int max)
{
    float width = ImGui::GetContentRegionAvail().x;
    float sliderH = 4.0f, grabR = 7.0f;
    
    ImGui::PushID(label);
    
    ImVec2 labelPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, 16.0f));
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    d->AddText(labelPos, ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), label);
    
    char valTxt[16];
    snprintf(valTxt, sizeof(valTxt), "%d", *v);
    ImVec2 valSize = ImGui::CalcTextSize(valTxt);
    d->AddText(ImVec2(labelPos.x + width - valSize.x, labelPos.y), GetAccentU32(1.0f), valTxt);
    
    ImVec2 sliderPos = ImGui::GetCursorScreenPos();
    float sliderAreaH = grabR * 2 + 4;
    bool clicked = ImGui::InvisibleButton("##sl", ImVec2(width, sliderAreaH));
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    
    if(active)
    {
        float t = std::clamp((ImGui::GetMousePos().x - sliderPos.x) / width, 0.0f, 1.0f);
        *v = min + (int)(t * (max - min) + 0.5f);
    }
    
    float hoverAnim = GetAnimValue(std::string("sl_") + label, (hovered || active) ? 1.0f : 0.0f);
    float sliderY = sliderPos.y + grabR + 2;
    
    d->AddRectFilled(ImVec2(sliderPos.x, sliderY - sliderH / 2), ImVec2(sliderPos.x + width, sliderY + sliderH / 2),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgTertiary), sliderH / 2);
    
    float t = (max > min) ? (float)(*v - min) / (float)(max - min) : 0.0f;
    float fillX = sliderPos.x + t * width;
    d->AddRectFilled(ImVec2(sliderPos.x, sliderY - sliderH / 2), ImVec2(fillX, sliderY + sliderH / 2), GetAccentU32(0.9f), sliderH / 2);
    d->AddCircleFilled(ImVec2(fillX, sliderY), grabR * (1.0f + hoverAnim * 0.15f), GetAccentU32(1.0f));
    d->AddCircleFilled(ImVec2(fillX, sliderY), grabR * 0.4f, IM_COL32(255, 255, 255, 230));
    
    ImGui::PopID();
    return clicked;
}

bool CImGuiManagerModern::RenderSliderFloat(const char *label, float *v, float min, float max)
{
    int intVal = (int)(*v * 10);
    bool changed = RenderSlider(label, &intVal, (int)(min * 10), (int)(max * 10));
    *v = intVal / 10.0f;
    return changed;
}

bool CImGuiManagerModern::RenderCombo(const char *label, int *current, const char *const items[], int count)
{
    float width = ImGui::GetContentRegionAvail().x;
    float comboH = 28.0f;
    
    ImGui::PushID(label);
    
    ImVec2 labelPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, 16.0f));
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    d->AddText(labelPos, ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), label);
    
    ImVec2 comboPos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##cb", ImVec2(width, comboH));
    
    d->AddRectFilled(comboPos, ImVec2(comboPos.x + width, comboPos.y + comboH),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_BgTertiary), 6.0f);
    d->AddRect(comboPos, ImVec2(comboPos.x + width, comboPos.y + comboH),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_Border), 6.0f);
    
    const char *txt = (*current >= 0 && *current < count) ? items[*current] : "---";
    d->AddText(ImVec2(comboPos.x + 8, comboPos.y + (comboH - 14) / 2),
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextPrimary), txt);
    
    float ax = comboPos.x + width - 14, ay = comboPos.y + comboH / 2;
    d->AddTriangleFilled(ImVec2(ax - 4, ay - 2), ImVec2(ax + 4, ay - 2), ImVec2(ax, ay + 3), GetAccentU32(0.9f));
    
    if(clicked)
        ImGui::OpenPopup("##pop");
    
    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(comboPos.x, comboPos.y + comboH + 2));
    ImGui::SetNextWindowSize(ImVec2(width, 0));
    
    if(ImGui::BeginPopup("##pop"))
    {
        for(int i = 0; i < count; i++)
        {
            if(ImGui::Selectable(items[i], i == *current))
            {
                *current = i;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    
    ImGui::PopID();
    return changed;
}

bool CImGuiManagerModern::RenderButton(const char *label, bool primary)
{
    float width = ImGui::GetContentRegionAvail().x;
    float height = 28.0f;
    
    ImGui::PushID(label);
    
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##btn", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    
    float hoverAnim = GetAnimValue(std::string("btn_") + label, hovered ? 1.0f : 0.0f);
    ImVec4 bgClr = primary ? LerpColor(GetAccentColor(0.85f), GetAccentColor(1.0f), hoverAnim)
                           : LerpColor(m_Theme.m_BgTertiary, m_Theme.m_BgHover, hoverAnim);
    
    d->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(bgClr), 6.0f);
    if(!primary)
        d->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::ColorConvertFloat4ToU32(m_Theme.m_Border), 6.0f);
    
    ImVec2 tsz = ImGui::CalcTextSize(label);
    ImVec4 tcol = primary ? ImVec4(1, 1, 1, 1) : m_Theme.m_TextPrimary;
    d->AddText(ImVec2(pos.x + (width - tsz.x) / 2, pos.y + (height - tsz.y) / 2), ImGui::ColorConvertFloat4ToU32(tcol), label);
    
    ImGui::PopID();
    return clicked;
}

void CImGuiManagerModern::LoadIcons()
{
    if(!m_pGameClient || !m_pGameClient->Graphics())
        return;
    
    IGraphics *pGraphics = m_pGameClient->Graphics();
    
    const char *icons[] = {
        "aim", "aimvisual", "appearance", "autoanswer", "avoid", 
        "avoidlegit", "avoidmode", "avoidrage", "chatspam", "configs",
        "hidecheat", "keybinds", "menuvisuals", "misc", "setting",
        "tas", "tascontrol", "tasplayback", "tasvisuals", "thememenu",
        "accentcolormenu"
    };
    
    m_MissingIcons.clear();
    m_IconsError = false;
    
    for(const char *name : icons)
    {
        char aPath[256];
        str_format(aPath, sizeof(aPath), "shalawamenuicnos/%s.png", name);
        
        CImageInfo ImgInfo;
        
        if(pGraphics->LoadPng(ImgInfo, aPath, IStorage::TYPE_ALL))
        {
            IGraphics::CTextureHandle tex = pGraphics->LoadTextureRaw(ImgInfo, 0, aPath);
            ImgInfo.Free();
            
            if(tex.IsValid())
            {
                m_Icons[name] = tex;
                continue;
            }
        }
        
        m_MissingIcons.push_back(name);
    }
    
    if(!m_MissingIcons.empty())
        m_IconsError = true;
    else
        m_IconsLoaded = true;
}

void CImGuiManagerModern::RenderErrorWindow()
{
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 windowSize(400, 250);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) / 2, (io.DisplaySize.y - windowSize.y) / 2);
    
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, GetAccentColor(0.8f));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;
    
    if(ImGui::Begin("##error", nullptr, flags))
    {
        ImDrawList *d = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        
        const char *title = "Sowwy!! :<";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        float titleScale = 2.0f;
        d->AddText(ImGui::GetFont(), ImGui::GetFontSize() * titleScale,
            ImVec2(wp.x + (ws.x - titleSize.x * titleScale) / 2, wp.y + 30),
            GetAccentU32(1.0f), title);
        
        const char *msg1 = "i cant find some icons for menu...";
        const char *msg2 = "i cant load without icons!!! :<<<";
        
        ImVec2 msg1Size = ImGui::CalcTextSize(msg1);
        ImVec2 msg2Size = ImGui::CalcTextSize(msg2);
        
        d->AddText(ImVec2(wp.x + (ws.x - msg1Size.x) / 2, wp.y + 90),
            ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), msg1);
        d->AddText(ImVec2(wp.x + (ws.x - msg2Size.x) / 2, wp.y + 110),
            ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), msg2);
        
        d->AddText(ImVec2(wp.x + 20, wp.y + 145),
            ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), "missing iconss:");
        
        float y = 165;
        for(size_t i = 0; i < m_MissingIcons.size() && i < 4; i++)
        {
            char buf[64];
            str_format(buf, sizeof(buf), "- %s.png", m_MissingIcons[i].c_str());
            d->AddText(ImVec2(wp.x + 30, wp.y + y),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.4f, 0.4f, 1.0f)), buf);
            y += 16;
        }
        
        if(m_MissingIcons.size() > 4)
        {
            char buf[32];
            str_format(buf, sizeof(buf), "...and %d more", (int)(m_MissingIcons.size() - 4));
            d->AddText(ImVec2(wp.x + 30, wp.y + y),
                ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextTertiary), buf);
        }
    }
    ImGui::End();
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

ImTextureID CImGuiManagerModern::GetIconTexture(const std::string &name)
{
    auto it = m_Icons.find(name);
    if(it != m_Icons.end() && it->second.IsValid())
        return (ImTextureID)(intptr_t)it->second.Id();
    return (ImTextureID)0;
}

void CImGuiManagerModern::RenderIcon(ImDrawList *d, ImVec2 pos, ImVec2 size, const std::string &name, float alpha)
{
    ImTextureID tex = GetIconTexture(name);
    if(tex)
    {
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, alpha));
        d->AddImage(tex, pos, ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 0), ImVec2(1, 1), col);
    }
}

bool CImGuiManagerModern::RenderColorPicker(const char *label, float col[4])
{
    float width = ImGui::GetContentRegionAvail().x;
    float previewSize = 22.0f, height = 26.0f;
    
    ImGui::PushID(label);
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    bool clicked = ImGui::InvisibleButton("##color", ImVec2(width, height));
    if(clicked)
        ImGui::OpenPopup("##picker");
    
    ImDrawList *d = ImGui::GetWindowDrawList();
    d->AddText(ImVec2(startPos.x, startPos.y + (height - 14) / 2), 
        ImGui::ColorConvertFloat4ToU32(m_Theme.m_TextSecondary), label);
    
    ImVec2 previewPos(startPos.x + width - previewSize, startPos.y + (height - previewSize) / 2);
    d->AddRectFilled(previewPos, ImVec2(previewPos.x + previewSize, previewPos.y + previewSize),
        ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3])), 4.0f);
    d->AddRect(previewPos, ImVec2(previewPos.x + previewSize, previewPos.y + previewSize),
        IM_COL32(255, 255, 255, 80), 4.0f);
    
    bool changed = false;
    if(ImGui::BeginPopup("##picker"))
    {
        changed = ImGui::ColorPicker4("##p", col, 
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        ImGui::EndPopup();
    }
    
    ImGui::PopID();
    return changed;
}

bool CImGuiManagerModern::RenderInputText(const char *label, char *buf, size_t bufSize, const char *hint)
{
    float width = ImGui::GetContentRegionAvail().x;
    
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, m_Theme.m_BgTertiary);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, m_Theme.m_BgHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, m_Theme.m_BgHover);
    ImGui::PushStyleColor(ImGuiCol_Text, m_Theme.m_TextPrimary);
    
    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::InputTextWithHint("##in", hint ? hint : "", buf, bufSize);
    
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    
    return changed;
}

void CImGuiManagerModern::Render()
{
    if(!m_Initialized || g_Config.m_shalawaHideCheat)
        return;
    RenderContent_Internal();
}

// CONTORLS OFFFFFFFFFFFFF SHALAWA CLIENT !!!!!!!! nya
#include "controls.h"

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/items.h>
#include <game/client/components/menus.h>
#include <game/client/components/scoreboard.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/laser_data.h>
#include <game/client/projectile_data.h>
#include <game/collision.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <array>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>

namespace
{
static const int AVOID_FORCE_MS = 120;
static int64_t s_LastAvoidTime = 0;
}

CControls::CControls()
{
	mem_zero(&m_aLastData, sizeof(m_aLastData));
	std::fill(std::begin(m_aMousePos), std::end(m_aMousePos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aMousePosOnAction), std::end(m_aMousePosOnAction), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aTargetPos), std::end(m_aTargetPos), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aMouseInputType), std::end(m_aMouseInputType), EMouseInputType::ABSOLUTE);
	std::fill(std::begin(m_aLastMousePos), std::end(m_aLastMousePos), vec2(0.0f, 0.0f));
	mem_zero(m_aAvoidForcing, sizeof(m_aAvoidForcing));
	mem_zero(m_aAvoidForcedDir, sizeof(m_aAvoidForcedDir));
	mem_zero(m_aAvoidForceUntil, sizeof(m_aAvoidForceUntil));
	mem_zero(m_aAvoidWasInDanger, sizeof(m_aAvoidWasInDanger));
	m_LastUserActivity = time_get();
}

void CControls::OnUpdate()
{
	ANTI_DISASM_BLOCK();
	// Shalawa binds now work through the bind system (+shalawa_avoid_freeze, +shalawa_avoid_toggle, +shalawa_hook_assist)
	// The old direct key checking is removed as it's now handled by the bind system
	
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(!AutomationAllowed())
		return;
	
	const SAvoidSettings &Settings = ActiveAvoidSettings();
	if(g_Config.m_shalawaAvoidMode == 1) // Legit mode
	{
		if(!Settings.m_EnableAvoidFreeze && !Settings.m_EnableHookAssist)
			return;
		if(Settings.m_EnableAvoidFreeze)
			AvoidFreeze(Settings);
		if(Settings.m_EnableHookAssist)
			HookAssist(Settings);
	}
}
void CControls::OnReset()
{
	ANTI_DISASM_BLOCK();
	ResetInput(0);
	ResetInput(1);
	m_BlatantAvoid.Reset(); // AI GENERATED DOWN!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ezZ cursor
	// Pilot bot
	m_PilotHasFinish = false;
	m_PilotFinishPos = vec2(0.0f, 0.0f);
	m_BalanceBot.Reset();
	m_QuickStop.OnReset();
	m_PilotLastTick = -1;
	m_UnfreezeBot.Init(this, GameClient());
	m_Aimbot.Reset();
	m_PilotHookTicksLeft = 0;
	m_aAutoFiring[0] = false;
	m_aAutoFiring[1] = false;
	m_PilotLastRescanTick = -100000;
	m_PilotHookCooldownTicks = 0;
	m_PilotHookAttemptTicks = 0;
	m_PilotHookGrabTicks = 0;
	m_PilotHookGrabStartPos = vec2(0.0f, 0.0f);
	
    for(int i = 0; i < NUM_DUMMIES; i++)
    {
        m_aAvoidVisuals[i].m_Active = false;
        m_aAvoidVisuals[i].m_Direction = 0;
        m_aAvoidVisuals[i].m_DangerPos = vec2(0, 0);
        m_aAvoidVisuals[i].m_DangerDistance = 0;
        m_aAvoidVisuals[i].m_DangerTick = -1;
        m_aAvoidVisuals[i].m_ActivationTime = 0;
        m_aAvoidVisuals[i].m_HookReleased = false;
        m_aAvoidVisuals[i].m_PlayerPos = vec2(0, 0);
        m_aAvoidVisuals[i].m_PlayerVel = vec2(0, 0);
    }

	// TAS reset
	if(m_pTasWorld)
	{
		delete m_pTasWorld;
		m_pTasWorld = nullptr;
	}
	if(m_pTasWorldPredicted)
	{
		delete m_pTasWorldPredicted;
		m_pTasWorldPredicted = nullptr;
	}
	m_TasLastTickTime = 0.0;
	m_TasNextTickTime = 0.0;
	m_vTasTicks.clear();
	m_vTasActions.clear();
	m_vTasMouse.clear();
	m_vTasDummySwitch.clear();
	m_vTasKill.clear();
	m_vTasAttackTick.clear();
	m_vTasPositions.clear();
	m_vTasChars.clear();
	g_Config.m_shalawaTasInit = 0;
	TasAutoReset();

	for(int &AmmoCount : m_aAmmoCount)
		AmmoCount = 0;

	m_LastSendTime = 0;
}

void CControls::ResetInput(int Dummy)
{
	m_aLastData[Dummy].m_Direction = 0;
	// simulate releasing the fire button
	if((m_aLastData[Dummy].m_Fire & 1) != 0)
		m_aLastData[Dummy].m_Fire++;
	m_aLastData[Dummy].m_Fire &= INPUT_STATE_MASK;
	m_aLastData[Dummy].m_Jump = 0;
	m_aInputData[Dummy] = m_aLastData[Dummy];

	m_aInputDirectionLeft[Dummy] = 0;
	m_aInputDirectionRight[Dummy] = 0;
}

void CControls::OnPlayerDeath()
{
	for(int &AmmoCount : m_aAmmoCount)
		AmmoCount = 0;
}

struct CInputState
{
	CControls *m_pControls;
	int *m_apVariables[NUM_DUMMIES];
};

void CControls::ConKeyInputState(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	if(pState->m_pControls->GameClient()->m_GameInfo.m_BugDDRaceInput && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	*pState->m_apVariables[g_Config.m_ClDummy] = pResult->GetInteger(0);
}

void CControls::ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData)
{
	CInputState *pState = (CInputState *)pUserData;

	if((pState->m_pControls->GameClient()->m_GameInfo.m_BugDDRaceInput && pState->m_pControls->GameClient()->m_Snap.m_SpecInfo.m_Active) || pState->m_pControls->GameClient()->m_Spectator.IsActive())
		return;

	int *pVariable = pState->m_apVariables[g_Config.m_ClDummy];
	if(((*pVariable) & 1) != pResult->GetInteger(0))
		(*pVariable)++;
	*pVariable &= INPUT_STATE_MASK;
}

struct CInputSet
{
	CControls *m_pControls;
	int *m_apVariables[NUM_DUMMIES];
	int m_Value;
};

void CControls::ConKeyInputSet(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	if(pResult->GetInteger(0))
	{
		*pSet->m_apVariables[g_Config.m_ClDummy] = pSet->m_Value;
	}
}

void CControls::ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CInputSet *pSet = (CInputSet *)pUserData;
	ConKeyInputCounter(pResult, pSet);
	pSet->m_pControls->m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 0;
}

void CControls::ConDummyAimFire(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		CControls *pControls = (CControls *)pUserData;
		pControls->GameClient()->RequestDummyAimFire();
	}
}

void CControls::ConShalawaAim(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaAimbot = !g_Config.m_shalawaAimbot;
	}
}

void CControls::ConShalawaAvoidFreeze(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaLegitAvoidFreeze = !g_Config.m_shalawaLegitAvoidFreeze;
	}
}

void CControls::ConShalawaAvoidToggle(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaLegitAvoidFreeze = !g_Config.m_shalawaLegitAvoidFreeze;
		g_Config.m_shalawaLegitHookAssist = !g_Config.m_shalawaLegitHookAssist;
	}
}

void CControls::ConShalawaHookAssist(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaLegitHookAssist = !g_Config.m_shalawaLegitHookAssist;
	}
}

void CControls::ConShalawaTasRecord(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaRecording = !g_Config.m_shalawaRecording;
	}
}

void CControls::ConShalawaTasPause(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaTasPause = !g_Config.m_shalawaTasPause;
	}
}

void CControls::ConShalawaTasRewind(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaTasPrev = 1;
	}
}

void CControls::ConShalawaTasLoad(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		// TAS Load command - this would need to be implemented in TAS system
		// For now, just toggle init
		g_Config.m_shalawaTasInit = 1;
	}
}

void CControls::ConShalawaTasInit(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaTasInit = !g_Config.m_shalawaTasInit;
	}
}

void CControls::ConShalawaHookRide(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaHookRide = !g_Config.m_shalawaHookRide;
	}
}

void CControls::ConShalawaBalanceBot(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaBalanceBot = !g_Config.m_shalawaBalanceBot;
	}
}

void CControls::ConShalawaUnfreezeBot(IConsole::IResult *pResult, void *pUserData)
{
	if(pResult->GetInteger(0))
	{
		g_Config.m_shalawaUnfreezeBot = !g_Config.m_shalawaUnfreezeBot;
	}
}


void CControls::OnConsoleInit()
{
	ANTI_DISASM_BLOCK();
	m_BlatantAvoid.Init(this, GameClient());
	m_Aimbot.Init(this, GameClient());
    m_QuickStop.OnInterfacesInit(GameClient());
	// game commands
	{
		static CInputState s_State = {this, {&m_aInputDirectionLeft[0], &m_aInputDirectionLeft[1]}};
		Console()->Register("+left", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move left");
	}
	{
		static CInputState s_State = {this, {&m_aInputDirectionRight[0], &m_aInputDirectionRight[1]}};
		Console()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move right");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Jump, &m_aInputData[1].m_Jump}};
		Console()->Register("+jump", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Jump");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Hook, &m_aInputData[1].m_Hook}};
		Console()->Register("+hook", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Hook");
	}
	{
		static CInputState s_State = {this, {&m_aInputData[0].m_Fire, &m_aInputData[1].m_Fire}};
		Console()->Register("+fire", "", CFGFLAG_CLIENT, ConKeyInputCounter, &s_State, "Fire");
	}
	{
		Console()->Register("+dummy_aim_fire", "", CFGFLAG_CLIENT, ConDummyAimFire, this, "Dummy aim at nearest target (excluding main) and hammer once");
	}
	{
		static CInputState s_State = {this, {&m_aShowHookColl[0], &m_aShowHookColl[1]}};
		Console()->Register("+showhookcoll", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Show Hook Collision");
	}

	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 1};
		Console()->Register("+weapon1", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to hammer");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 2};
		Console()->Register("+weapon2", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to gun");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 3};
		Console()->Register("+weapon3", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to shotgun");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 4};
		Console()->Register("+weapon4", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to grenade");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_WantedWeapon, &m_aInputData[1].m_WantedWeapon}, 5};
		Console()->Register("+weapon5", "", CFGFLAG_CLIENT, ConKeyInputSet, &s_Set, "Switch to laser");
	}

	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_NextWeapon, &m_aInputData[1].m_NextWeapon}, 0};
		Console()->Register("+nextweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, &s_Set, "Switch to next weapon");
	}
	{
		static CInputSet s_Set = {this, {&m_aInputData[0].m_PrevWeapon, &m_aInputData[1].m_PrevWeapon}, 0};
		Console()->Register("+prevweapon", "", CFGFLAG_CLIENT, ConKeyInputNextPrevWeapon, &s_Set, "Switch to previous weapon");
	}

	// Shalawa binds
	{
		Console()->Register("+shalawa_avoid_freeze", "", CFGFLAG_CLIENT, ConShalawaAvoidFreeze, this, "Toggle Shalawa Avoid Freeze");
	}
	{
		Console()->Register("+shalawa_avoid_toggle", "", CFGFLAG_CLIENT, ConShalawaAvoidToggle, this, "Toggle Shalawa Avoid Freeze and Hook Assist");
	}
	{
		Console()->Register("+shalawa_hook_assist", "", CFGFLAG_CLIENT, ConShalawaHookAssist, this, "Toggle Shalawa Hook Assist");
	}
	{
		Console()->Register("+shalawa_tas_record", "", CFGFLAG_CLIENT, ConShalawaTasRecord, this, "Toggle TAS Recording");
	}
	{
		Console()->Register("+shalawa_tas_pause", "", CFGFLAG_CLIENT, ConShalawaTasPause, this, "Toggle TAS Pause");
	}
	{
		Console()->Register("+shalawa_tas_rewind", "", CFGFLAG_CLIENT, ConShalawaTasRewind, this, "TAS Rewind (Step Back)");
	}
	{
		Console()->Register("+shalawa_tas_load", "", CFGFLAG_CLIENT, ConShalawaTasLoad, this, "TAS Load");
	}
	{
		Console()->Register("+shalawa_tas_init", "", CFGFLAG_CLIENT, ConShalawaTasInit, this, "Toggle TAS Init");
	}
	{
		Console()->Register("+shalawa_hookride", "", CFGFLAG_CLIENT, ConShalawaHookRide, this, "Toggle Hook Ride");
	}
	{
		Console()->Register("+shalawa_balance_bot", "", CFGFLAG_CLIENT, ConShalawaBalanceBot, this, "Toggle Balance Bot");
	}
	{
		Console()->Register("+shalawa_unfreeze_bot", "", CFGFLAG_CLIENT, ConShalawaUnfreezeBot, this, "Toggle Unfreeze Bot");
	}

	{
		Console()->Register("+shalawa_aimbot", "", CFGFLAG_CLIENT, ConShalawaAim, this, "Toggle Aimbot");
	}

}

void CControls::OnMessage(int Msg, void *pRawMsg)
{
	ANTI_DISASM_BLOCK();
	if(Msg == NETMSGTYPE_SV_WEAPONPICKUP)
	{
		CNetMsg_Sv_WeaponPickup *pMsg = (CNetMsg_Sv_WeaponPickup *)pRawMsg;
		if(g_Config.m_ClAutoswitchWeapons)
			m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = pMsg->m_Weapon + 1;
		// We don't really know ammo count, until we'll switch to that weapon, but any non-zero count will suffice here
		m_aAmmoCount[maximum(0, pMsg->m_Weapon % NUM_WEAPONS)] = 10;
	}
}

int CControls::SnapInput(int *pData)
{
	ANTI_DISASM_BLOCK();
	const int Dummy = g_Config.m_ClDummy;

	// If TAS playback is active, use recorded input instead of real input
	if(g_Config.m_shalawaPlaying)
	{
		mem_copy(pData, &m_aInputData[Dummy], sizeof(m_aInputData[0]));
		return sizeof(m_aInputData[0]);
	}
	
	// While TAS-world is enabled and we are recording, we intentionally do not send input to the server.
	// Instead we send a neutral "no-move/no-fire" input every tick to keep prediction in sync.
	if(g_Config.m_shalawaRecording && g_Config.m_shalawaTasInit && g_Config.m_shalawaTasWorld && !g_Config.m_shalawaPlaying)
	{
		// Send neutral input to server during TAS recording
		CNetObj_PlayerInput Neutral = m_aInputData[Dummy];
		Neutral.m_Direction = 0;
		Neutral.m_Jump = 0;
		Neutral.m_Hook = 0;
		Neutral.m_Fire = 0;
		Neutral.m_WantedWeapon = 0;
		Neutral.m_NextWeapon = 0;
		Neutral.m_PrevWeapon = 0;
		
		// Ensure deltas are consistent with what we actually send.
		m_aLastData[Dummy] = Neutral;
		m_LastSendTime = time_get();
		mem_copy(pData, &Neutral, sizeof(Neutral));
		return sizeof(Neutral);
	}
	// update player state
	if(GameClient()->m_Chat.IsActive())
		m_aInputData[Dummy].m_PlayerFlags = g_Config.m_ClHideChatBubble ? PLAYERFLAG_PLAYING : PLAYERFLAG_CHATTING;
	else if(GameClient()->m_Menus.IsActive())
		m_aInputData[Dummy].m_PlayerFlags = PLAYERFLAG_IN_MENU;
	else
		m_aInputData[Dummy].m_PlayerFlags = PLAYERFLAG_PLAYING;

	if(GameClient()->m_Scoreboard.IsActive())
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_SCOREBOARD;

	if(Client()->ServerCapAnyPlayerFlag() && m_aShowHookColl[Dummy])
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_AIM;

	if(Client()->ServerCapAnyPlayerFlag() && GameClient()->m_Camera.CamType() == CCamera::CAMTYPE_SPEC)
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_SPEC_CAM;

	switch(m_aMouseInputType[Dummy])
	{
	case CControls::EMouseInputType::AUTOMATED:
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE;
		break;
	case CControls::EMouseInputType::ABSOLUTE:
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE | PLAYERFLAG_INPUT_MANUAL;
		break;
	case CControls::EMouseInputType::RELATIVE:
		m_aInputData[Dummy].m_PlayerFlags |= PLAYERFLAG_INPUT_MANUAL;
		break;
	}

	bool Send = m_aLastData[Dummy].m_PlayerFlags != m_aInputData[Dummy].m_PlayerFlags;

	m_aLastData[Dummy].m_PlayerFlags = m_aInputData[Dummy].m_PlayerFlags;

	// we freeze the input if chat or menu is activated
	if(!(m_aInputData[Dummy].m_PlayerFlags & PLAYERFLAG_PLAYING))
	{
		if(!GameClient()->m_GameInfo.m_BugDDRaceInput)
			ResetInput(Dummy);

		mem_copy(pData, &m_aInputData[Dummy], sizeof(m_aInputData[0]));

		// set the target anyway though so that we can keep seeing our surroundings,
		// even if chat or menu are activated
		m_aInputData[Dummy].m_TargetX = (int)m_aMousePos[Dummy].x;
		m_aInputData[Dummy].m_TargetY = (int)m_aMousePos[Dummy].y;

		// send once a second just to be sure
		Send = Send || time_get() > m_LastSendTime + time_freq();
	}
	else
	{
		m_aInputData[Dummy].m_TargetX = (int)m_aMousePos[Dummy].x;
		m_aInputData[Dummy].m_TargetY = (int)m_aMousePos[Dummy].y;

		if(g_Config.m_ClSubTickAiming && m_aMousePosOnAction[Dummy] != vec2(0.0f, 0.0f))
		{
			m_aInputData[Dummy].m_TargetX = (int)m_aMousePosOnAction[Dummy].x;
			m_aInputData[Dummy].m_TargetY = (int)m_aMousePosOnAction[Dummy].y;
			m_aMousePosOnAction[Dummy] = vec2(0.0f, 0.0f);
		}

		if(!m_aInputData[Dummy].m_TargetX && !m_aInputData[Dummy].m_TargetY)
		{
			m_aInputData[Dummy].m_TargetX = 1;
			m_aMousePos[Dummy].x = 1;
		}

		// set direction
		m_aInputData[Dummy].m_Direction = 0;
		if(m_aInputDirectionLeft[Dummy] && !m_aInputDirectionRight[Dummy])
			m_aInputData[Dummy].m_Direction = -1;
		if(!m_aInputDirectionLeft[Dummy] && m_aInputDirectionRight[Dummy])
			m_aInputData[Dummy].m_Direction = 1;

		// Apply avoid freeze forced direction
		if(m_aAvoidForcing[Dummy])
		{
			const int Local = Dummy;
			const int64_t Now = time_get();
			if(Now <= m_aAvoidForceUntil[Local])
			{
				int IntendedDir = 0;
				if(m_aInputDirectionLeft[Local] && !m_aInputDirectionRight[Local])
					IntendedDir = -1;
				else if(!m_aInputDirectionLeft[Local] && m_aInputDirectionRight[Local])
					IntendedDir = 1;

				bool Release = false;
				const SAvoidSettings &Settings = ActiveAvoidSettings();
				if(!Settings.m_EnableAvoidFreeze)
				{
					Release = true;
				}
				else if(IntendedDir != m_aAvoidForcedDir[Local])
				{
					CNetObj_PlayerInput Test = m_aInputData[Local];
					Test.m_Direction = IntendedDir;
					const int CheckTicks = std::max(1, Settings.m_PredictTicks);
					if(!PredictFreeze(Test, CheckTicks, nullptr, nullptr))
						Release = true;
				}

				if(!Release)
					m_aInputData[Local].m_Direction = m_aAvoidForcedDir[Local];
				else
					m_aAvoidForcing[Local] = false;
			}
			else
			{
				m_aAvoidForcing[Local] = false;
			}
		}

		if(AutomationAllowed())
		{
			TasAutoApply(Dummy);
		}

		SyncDummyInput();

        if(AutomationAllowed())
        {
            int ClientId = GameClient()->m_aLocalIds[Dummy];
            vec2 Vel = vec2(0.0f, 0.0f);
            const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
            if(pChar && pChar->Core())
                Vel = pChar->Core()->m_Vel;
            m_QuickStop.Tick(Dummy, m_aInputData[Dummy], Vel);
        }

		if(AutomationAllowed() && !(g_Config.m_shalawaTasAutoMode == 1 && g_Config.m_shalawaTasAutoRunning))
		{
			m_UnfreezeBot.Tick(Dummy);
		}

		if(AutomationAllowed())
		{
			m_BalanceBot.Tick(Dummy);
		}

		if(AutomationAllowed())
		{
			m_HookRide.Tick(Dummy);
		}
		if(AutomationAllowed())
		{
			m_Aimbot.Tick(Dummy);
		}
		// Call appropriate avoid functions based on mode
		if(AutomationAllowed())
		{
			if(g_Config.m_shalawaAvoidMode == 2) //ok
			{
				if(g_Config.m_shalawaAvoidFreeze)
				{
					if(g_Config.m_shalawaAvoidOnlyHorizontally)
						m_BlatantAvoid.AvoidFreezeHorizontal();
					else
						m_BlatantAvoid.AvoidFreeze();
				}
				m_BlatantAvoid.FreezeReset();
			}
		}

		// stress testing
#ifdef CONF_DEBUG
		if(g_Config.m_DbgStress)
		{
			float t = Client()->LocalTime();
			mem_zero(&m_aInputData[g_Config.m_ClDummy], sizeof(m_aInputData[0]));

			m_aInputData[g_Config.m_ClDummy].m_Direction = ((int)t / 2) & 1;
			m_aInputData[g_Config.m_ClDummy].m_Jump = ((int)t);
			m_aInputData[g_Config.m_ClDummy].m_Fire = ((int)(t * 10));
			m_aInputData[g_Config.m_ClDummy].m_Hook = ((int)(t * 2)) & 1;
			m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = ((int)t) % NUM_WEAPONS;
			m_aInputData[g_Config.m_ClDummy].m_TargetX = (int)(std::sin(t * 3) * 100.0f);
			m_aInputData[g_Config.m_ClDummy].m_TargetY = (int)(std::cos(t * 3) * 100.0f);
		}
#endif
		// check if we need to send input
		Send = Send || m_aInputData[Dummy].m_Direction != m_aLastData[Dummy].m_Direction;
		Send = Send || m_aInputData[Dummy].m_Jump != m_aLastData[Dummy].m_Jump;
		Send = Send || m_aInputData[Dummy].m_Fire != m_aLastData[Dummy].m_Fire;
		Send = Send || m_aInputData[Dummy].m_Hook != m_aLastData[Dummy].m_Hook;
		Send = Send || m_aInputData[Dummy].m_WantedWeapon != m_aLastData[Dummy].m_WantedWeapon;
		Send = Send || m_aInputData[Dummy].m_NextWeapon != m_aLastData[Dummy].m_NextWeapon;
		Send = Send || m_aInputData[Dummy].m_PrevWeapon != m_aLastData[Dummy].m_PrevWeapon;
		Send = Send || time_get() > m_LastSendTime + time_freq() / 25; // send at least 25 Hz
		Send = Send || (GameClient()->m_Snap.m_pLocalCharacter && GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == WEAPON_NINJA && (m_aInputData[Dummy].m_Direction || m_aInputData[Dummy].m_Jump || m_aInputData[Dummy].m_Hook));
	}

	// copy and return size
	if(g_Config.m_shalawaFakeAim && GameClient()->m_FakeAim.IsActive())
	{
		bool NewFire = ((m_aInputData[Dummy].m_Fire & 1) != 0) && ((m_aLastData[Dummy].m_Fire & 1) == 0);
		bool NewHook = (m_aInputData[Dummy].m_Hook == 1 && m_aLastData[Dummy].m_Hook == 0);
		if(NewFire || NewHook)
			GameClient()->m_FakeAim.OnAction(Dummy);
	}

	m_aLastData[Dummy] = m_aInputData[Dummy];

	if(g_Config.m_shalawaFakeAim && g_Config.m_shalawaFakeAimSendAlways)
		Send = true;

	if(!Send)
		return 0;

	m_LastSendTime = time_get();

	CNetObj_PlayerInput SendData = m_aInputData[Dummy];
	if(g_Config.m_shalawaFakeAim && GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_FakeAim.IsBlocked(Dummy))
	{
		const vec2 FakeTarget = GameClient()->m_FakeAim.GetFakeTarget(Dummy);
		SendData.m_TargetX = (int)FakeTarget.x;
		SendData.m_TargetY = (int)FakeTarget.y;
	}

	mem_copy(pData, &SendData, sizeof(SendData));
	return sizeof(SendData);
}

void CControls::UpdateAvoidVisuals(int Local, bool Activated, int Direction, vec2 DangerPos, float DangerDist, int DangerTick, bool HookReleased)
{
    if(!g_Config.m_shalawaLegitAvoidVisuals)
        return;
        
    m_aAvoidVisuals[Local].m_Active = Activated;
    m_aAvoidVisuals[Local].m_Direction = Direction;
    m_aAvoidVisuals[Local].m_DangerPos = DangerPos;
    m_aAvoidVisuals[Local].m_DangerDistance = DangerDist;
    m_aAvoidVisuals[Local].m_DangerTick = DangerTick;
    m_aAvoidVisuals[Local].m_HookReleased = HookReleased;
    
    if(Activated)
        m_aAvoidVisuals[Local].m_ActivationTime = time_get();
        
    int ClientId = GameClient()->m_aLocalIds[Local];
    if(ClientId >= 0 && GameClient()->m_aClients[ClientId].m_Active)
    {
        m_aAvoidVisuals[Local].m_PlayerPos = GameClient()->m_aClients[ClientId].m_Predicted.m_Pos;
        const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
        if(pChar && pChar->Core())
            m_aAvoidVisuals[Local].m_PlayerVel = pChar->Core()->m_Vel;
    }
}

void CControls::RenderLegitAvoidVisuals()
{
    if(!g_Config.m_shalawaLegitAvoidVisuals)
        return;
        
    if(Client()->State() != IClient::STATE_ONLINE)
        return;
        
    const int Local = g_Config.m_ClDummy;
    const int ClientId = GameClient()->m_aLocalIds[Local];
    
    if(ClientId < 0 || !GameClient()->m_aClients[ClientId].m_Active)
        return;
        
    if(!GameClient()->m_Snap.m_pLocalCharacter)
        return;
        
    vec2 PlayerPos = GameClient()->m_aClients[ClientId].m_Predicted.m_Pos;
    
    Graphics()->TextureClear();
    Graphics()->QuadsBegin();
    
    const int64_t Now = time_get();
    const float FadeTime = 0.3f;
    
    if(m_aAvoidForcing[Local] && Now <= m_aAvoidForceUntil[Local])
    {
        float Alpha = g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f;
        
        ColorRGBA DirColor;
        if(m_aAvoidForcedDir[Local] == -1)
            DirColor = ColorRGBA(1.0f, 0.3f, 0.3f, Alpha);
        else if(m_aAvoidForcedDir[Local] == 1)
            DirColor = ColorRGBA(0.3f, 1.0f, 0.3f, Alpha);
        else
            DirColor = ColorRGBA(1.0f, 1.0f, 0.3f, Alpha);
            
        Graphics()->SetColor(DirColor);
        
        float ArrowSize = 24.0f;
        float ArrowOffset = 40.0f;
        vec2 ArrowPos = PlayerPos + vec2(m_aAvoidForcedDir[Local] * ArrowOffset, -20.0f);
        
        if(m_aAvoidForcedDir[Local] != 0)
        {
            IGraphics::CFreeformItem Arrow(
                ArrowPos.x, ArrowPos.y - ArrowSize * 0.5f,
                ArrowPos.x, ArrowPos.y + ArrowSize * 0.5f,
                ArrowPos.x + m_aAvoidForcedDir[Local] * ArrowSize, ArrowPos.y,
                ArrowPos.x + m_aAvoidForcedDir[Local] * ArrowSize, ArrowPos.y
            );
            Graphics()->QuadsDrawFreeform(&Arrow, 1);
        }
        
        if(g_Config.m_shalawaLegitAvoidVisualsTrail)
        {
            float TrailLength = 60.0f;
            float TrailWidth = 4.0f;
            ColorRGBA TrailColor = DirColor;
            TrailColor.a *= 0.5f;
            Graphics()->SetColor(TrailColor);
            
            vec2 TrailStart = PlayerPos;
            vec2 TrailEnd = PlayerPos + vec2(m_aAvoidForcedDir[Local] * TrailLength, 0);
            
            IGraphics::CFreeformItem Trail(
                TrailStart.x, TrailStart.y - TrailWidth,
                TrailStart.x, TrailStart.y + TrailWidth,
                TrailEnd.x, TrailEnd.y - TrailWidth * 0.3f,
                TrailEnd.x, TrailEnd.y + TrailWidth * 0.3f
            );
            Graphics()->QuadsDrawFreeform(&Trail, 1);
        }
    }
    
    if(g_Config.m_shalawaLegitAvoidVisualsDanger && m_aAvoidVisuals[Local].m_Active)
    {
        float TimeSince = (Now - m_aAvoidVisuals[Local].m_ActivationTime) / (float)time_freq();
        if(TimeSince < FadeTime * 2)
        {
            float Alpha = (1.0f - TimeSince / (FadeTime * 2)) * (g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f);
            
            Graphics()->SetColor(1.0f, 0.2f, 0.2f, Alpha * 0.6f);
            
            float DangerRadius = 16.0f + m_aAvoidVisuals[Local].m_DangerDistance * 0.1f;
            vec2 DangerPos = m_aAvoidVisuals[Local].m_DangerPos;
            
            const int Segments = 16;
            for(int i = 0; i < Segments; i++)
            {
                float Angle1 = 2.0f * pi * i / Segments;
                float Angle2 = 2.0f * pi * (i + 1) / Segments;
                
                IGraphics::CFreeformItem Segment(
                    DangerPos.x, DangerPos.y,
                    DangerPos.x, DangerPos.y,
                    DangerPos.x + cosf(Angle1) * DangerRadius, DangerPos.y + sinf(Angle1) * DangerRadius,
                    DangerPos.x + cosf(Angle2) * DangerRadius, DangerPos.y + sinf(Angle2) * DangerRadius
                );
                Graphics()->QuadsDrawFreeform(&Segment, 1);
            }
            
            Graphics()->SetColor(1.0f, 0.4f, 0.0f, Alpha * 0.3f);
            
            vec2 LineStart = PlayerPos;
            vec2 LineEnd = DangerPos;
            float LineWidth = 2.0f;
            vec2 Dir = normalize(LineEnd - LineStart);
            vec2 Perp = vec2(-Dir.y, Dir.x) * LineWidth;
            
            IGraphics::CFreeformItem Line(
                LineStart.x + Perp.x, LineStart.y + Perp.y,
                LineStart.x - Perp.x, LineStart.y - Perp.y,
                LineEnd.x + Perp.x, LineEnd.y + Perp.y,
                LineEnd.x - Perp.x, LineEnd.y - Perp.y
            );
            Graphics()->QuadsDrawFreeform(&Line, 1);
        }
    }
    
    if(g_Config.m_shalawaLegitAvoidVisualsHook && m_aAvoidVisuals[Local].m_HookReleased)
    {
        float TimeSince = (Now - m_aAvoidVisuals[Local].m_ActivationTime) / (float)time_freq();
        if(TimeSince < FadeTime)
        {
            float Alpha = (1.0f - TimeSince / FadeTime) * (g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f);
            
            Graphics()->SetColor(0.3f, 0.7f, 1.0f, Alpha);
            
            float RingRadius = 32.0f * (1.0f + TimeSince * 2.0f);
            float RingWidth = 3.0f * (1.0f - TimeSince / FadeTime);
            
            const int Segments = 24;
            for(int i = 0; i < Segments; i++)
            {
                float Angle1 = 2.0f * pi * i / Segments;
                float Angle2 = 2.0f * pi * (i + 1) / Segments;
                
                IGraphics::CFreeformItem Segment(
                    PlayerPos.x + cosf(Angle1) * (RingRadius - RingWidth), PlayerPos.y + sinf(Angle1) * (RingRadius - RingWidth),
                    PlayerPos.x + cosf(Angle1) * (RingRadius + RingWidth), PlayerPos.y + sinf(Angle1) * (RingRadius + RingWidth),
                    PlayerPos.x + cosf(Angle2) * (RingRadius - RingWidth), PlayerPos.y + sinf(Angle2) * (RingRadius - RingWidth),
                    PlayerPos.x + cosf(Angle2) * (RingRadius + RingWidth), PlayerPos.y + sinf(Angle2) * (RingRadius + RingWidth)
                );
                Graphics()->QuadsDrawFreeform(&Segment, 1);
            }
        }
    }
    
    if(g_Config.m_shalawaLegitAvoidVisualsVelocity)
    {
        const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
        if(pChar && pChar->Core())
        {
            vec2 Vel = pChar->Core()->m_Vel;
            float Speed = length(Vel);
            
            if(Speed > 1.0f)
            {
                float Alpha = g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f * 0.5f;
                
                float Hue = std::min(Speed / 20.0f, 1.0f);
                ColorRGBA VelColor;
                if(Hue < 0.5f)
                    VelColor = ColorRGBA(Hue * 2.0f, 1.0f, 0.0f, Alpha);
                else
                    VelColor = ColorRGBA(1.0f, 2.0f - Hue * 2.0f, 0.0f, Alpha);
                    
                Graphics()->SetColor(VelColor);
                
                float VelScale = std::min(Speed * 3.0f, 80.0f);
                vec2 VelDir = normalize(Vel);
                vec2 VelEnd = PlayerPos + VelDir * VelScale;
                
                float LineWidth = 2.0f;
                vec2 Perp = vec2(-VelDir.y, VelDir.x) * LineWidth;
                
                IGraphics::CFreeformItem VelLine(
                    PlayerPos.x + Perp.x, PlayerPos.y + Perp.y,
                    PlayerPos.x - Perp.x, PlayerPos.y - Perp.y,
                    VelEnd.x + Perp.x * 0.3f, VelEnd.y + Perp.y * 0.3f,
                    VelEnd.x - Perp.x * 0.3f, VelEnd.y - Perp.y * 0.3f
                );
                Graphics()->QuadsDrawFreeform(&VelLine, 1);
            }
        }
    }
    
    if(g_Config.m_shalawaLegitAvoidVisualsPath)
    {
        const SAvoidSettings &Settings = ActiveAvoidSettings();
        const int CheckTicks = std::max(1, Settings.m_PredictTicks);
        
        CGameWorld PredWorld;
        PredWorld.CopyWorld(&GameClient()->m_PredictedWorld);
        
        CCharacter *pChar = PredWorld.GetCharacterById(ClientId);
        if(pChar)
        {
            CNetObj_PlayerInput Input = m_aInputData[Local];
            pChar->OnDirectInput(&Input);
            
            float Alpha = g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f * 0.4f;
            
            vec2 LastPos = pChar->m_Pos;
            for(int i = 0; i < CheckTicks; i++)
            {
                pChar->OnPredictedInput(&Input);
                PredWorld.m_GameTick++;
                PredWorld.Tick();
                
                vec2 CurrentPos = pChar->m_Pos;
                bool InFreeze = GetFreeze(CurrentPos, pChar->m_FreezeTime, Settings);
                
                if(InFreeze)
                    Graphics()->SetColor(1.0f, 0.2f, 0.2f, Alpha);
                else
                    Graphics()->SetColor(0.2f, 1.0f, 0.2f, Alpha);
                    
                float DotSize = 4.0f;
                IGraphics::CFreeformItem Dot(
                    CurrentPos.x - DotSize, CurrentPos.y - DotSize,
                    CurrentPos.x + DotSize, CurrentPos.y - DotSize,
                    CurrentPos.x - DotSize, CurrentPos.y + DotSize,
                    CurrentPos.x + DotSize, CurrentPos.y + DotSize
                );
                Graphics()->QuadsDrawFreeform(&Dot, 1);
                
                LastPos = CurrentPos;
            }
        }
    }
    
    Graphics()->QuadsEnd();
    
    if(g_Config.m_shalawaLegitAvoidVisualsText && m_aAvoidForcing[Local])
    {
        float TimeSince = (Now - m_aAvoidVisuals[Local].m_ActivationTime) / (float)time_freq();
        if(TimeSince < 0.5f)
        {
            float Alpha = (1.0f - TimeSince * 2.0f) * (g_Config.m_shalawaLegitAvoidVisualsAlpha / 100.0f);
            
            char aBuf[32];
            if(m_aAvoidVisuals[Local].m_HookReleased)
                str_copy(aBuf, "HOOK RELEASED");
            else
                str_format(aBuf, sizeof(aBuf), "AVOID %s", m_aAvoidForcedDir[Local] == -1 ? "LEFT" : (m_aAvoidForcedDir[Local] == 1 ? "RIGHT" : "STOP"));
                
            vec2 TextPos = PlayerPos + vec2(0, -60.0f);
            
            float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
            Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
            
            TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
            TextRender()->Text(TextPos.x - TextRender()->TextWidth(8.0f, aBuf) * 0.5f, TextPos.y, 8.0f, aBuf);
            TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }
}

void CControls::OnRender()
{
	ANTI_DISASM_BLOCK();
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_ClAutoswitchWeaponsOutOfAmmo && !GameClient()->m_GameInfo.m_UnlimitedAmmo && GameClient()->m_Snap.m_pLocalCharacter)
	{
		m_aAmmoCount[maximum(0, GameClient()->m_Snap.m_pLocalCharacter->m_Weapon % NUM_WEAPONS)] = GameClient()->m_Snap.m_pLocalCharacter->m_AmmoCount;
		if(m_aInputData[g_Config.m_ClDummy].m_Fire % 2 != 0 &&
			GameClient()->m_Snap.m_pLocalCharacter->m_AmmoCount == 0 &&
			GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER &&
			GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_NINJA)
		{
			int Weapon;
			for(Weapon = WEAPON_LASER; Weapon > WEAPON_GUN; Weapon--)
			{
				if(Weapon == GameClient()->m_Snap.m_pLocalCharacter->m_Weapon)
					continue;
				if(m_aAmmoCount[Weapon] > 0)
					break;
			}
			if(Weapon != GameClient()->m_Snap.m_pLocalCharacter->m_Weapon)
				m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = Weapon + 1;
		}
	}

	m_BlatantAvoid.OnRender();
	m_Aimbot.OnRender();
	RenderLegitAvoidVisuals();
	// Update m_aInputData with real input from keyboard/mouse BEFORE TasUpdate()
	// This ensures TAS World uses the latest input
	const int Dummy = g_Config.m_ClDummy;
	if(GameClient()->m_Snap.m_pGameInfoObj && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		// Update mouse position
		m_aInputData[Dummy].m_TargetX = (int)m_aMousePos[Dummy].x;
		m_aInputData[Dummy].m_TargetY = (int)m_aMousePos[Dummy].y;
		
		// Update direction from keyboard input
		m_aInputData[Dummy].m_Direction = 0;
		if(m_aInputDirectionLeft[Dummy] && !m_aInputDirectionRight[Dummy])
			m_aInputData[Dummy].m_Direction = -1;
		if(!m_aInputDirectionLeft[Dummy] && m_aInputDirectionRight[Dummy])
			m_aInputData[Dummy].m_Direction = 1;
		
		// Sync dummy input
		SyncDummyInput();
		
		vec2 DyncamOffsetDelta = GameClient()->m_Camera.m_DyncamTargetCameraOffset - GameClient()->m_Camera.m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy];
		float Zoom = GameClient()->m_Camera.m_Zoom;
		// Keep cursor feel identical to normal gameplay during TAS recording.
		if(g_Config.m_shalawaRecording && g_Config.m_shalawaTasWorld)
		{
			m_aTargetPos[g_Config.m_ClDummy] = GameClient()->m_LocalCharacterPos + m_aMousePos[g_Config.m_ClDummy] - DyncamOffsetDelta + DyncamOffsetDelta / Zoom;
		}
		else
		{
			m_aTargetPos[g_Config.m_ClDummy] = GameClient()->m_LocalCharacterPos + m_aMousePos[g_Config.m_ClDummy] - DyncamOffsetDelta + DyncamOffsetDelta / Zoom;
		}
	}
	else if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_UsePosition)
	{
		m_aTargetPos[g_Config.m_ClDummy] = GameClient()->m_Snap.m_SpecInfo.m_Position + m_aMousePos[g_Config.m_ClDummy];
	}
	else
	{
		m_aTargetPos[g_Config.m_ClDummy] = m_aMousePos[g_Config.m_ClDummy];
	}
	
	// TAS Update - must be called AFTER m_aInputData is updated with real input
	TasUpdate();

	// Note: RenderTasWorld() is now called from CPlayers::OnRender() to ensure correct render order
	// (after normal players, but before MapLayersForeground)
}

// GetInterpolatedMouseForRendering and GetInterpolatedPlayerPosition implementations moved to controls_tas.cpp

bool CControls::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	ANTI_DISASM_BLOCK();
	if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		return false;

	if(CursorType == IInput::CURSOR_JOYSTICK && g_Config.m_InpControllerAbsolute && GameClient()->m_Snap.m_pGameInfoObj && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		vec2 AbsoluteDirection;
		if(Input()->GetActiveJoystick()->Absolute(&AbsoluteDirection.x, &AbsoluteDirection.y))
		{
			m_aMousePos[g_Config.m_ClDummy] = AbsoluteDirection * GetMaxMouseDistance();
			GameClient()->m_Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::ABSOLUTE;
		}
		return true;
	}

	float Factor = 1.0f;
	if(g_Config.m_ClDyncam && g_Config.m_ClDyncamMousesens)
	{
		Factor = g_Config.m_ClDyncamMousesens / 100.0f;
	}
	else
	{
		switch(CursorType)
		{
		case IInput::CURSOR_MOUSE:
			Factor = g_Config.m_InpMousesens / 100.0f;
			break;
		case IInput::CURSOR_JOYSTICK:
			Factor = g_Config.m_InpControllerSens / 100.0f;
			break;
		default:
			dbg_assert_failed("CControls::OnCursorMove CursorType %d", (int)CursorType);
		}
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId < 0)
		Factor *= GameClient()->m_Camera.m_Zoom;

	m_aMousePos[g_Config.m_ClDummy] += vec2(x, y) * Factor;
	GameClient()->m_Controls.m_aMouseInputType[g_Config.m_ClDummy] = CControls::EMouseInputType::RELATIVE;
	ClampMousePos();
	return true;
}

void CControls::ClampMousePos()
{
	if(GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId < 0)
	{
		m_aMousePos[g_Config.m_ClDummy].x = std::clamp(m_aMousePos[g_Config.m_ClDummy].x, -201.0f * 32, (Collision()->GetWidth() + 201.0f) * 32.0f);
		m_aMousePos[g_Config.m_ClDummy].y = std::clamp(m_aMousePos[g_Config.m_ClDummy].y, -201.0f * 32, (Collision()->GetHeight() + 201.0f) * 32.0f);
	}
	else
	{
		const float MouseMin = GetMinMouseDistance();
		const float MouseMax = GetMaxMouseDistance();

		float MouseDistance = length(m_aMousePos[g_Config.m_ClDummy]);
		if(MouseDistance < 0.001f)
		{
			m_aMousePos[g_Config.m_ClDummy].x = 0.001f;
			m_aMousePos[g_Config.m_ClDummy].y = 0;
			MouseDistance = 0.001f;
		}
		if(MouseDistance < MouseMin)
			m_aMousePos[g_Config.m_ClDummy] = normalize_pre_length(m_aMousePos[g_Config.m_ClDummy], MouseDistance) * MouseMin;
		MouseDistance = length(m_aMousePos[g_Config.m_ClDummy]);
		if(MouseDistance > MouseMax)
			m_aMousePos[g_Config.m_ClDummy] = normalize_pre_length(m_aMousePos[g_Config.m_ClDummy], MouseDistance) * MouseMax;
	}
}

float CControls::GetMinMouseDistance() const
{
	return g_Config.m_ClDyncam ? g_Config.m_ClDyncamMinDistance : g_Config.m_ClMouseMinDistance;
}

float CControls::GetMaxMouseDistance() const
{
	float CameraMaxDistance = 200.0f;
	float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
	float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
	float MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
	return minimum((FollowFactor != 0 ? CameraMaxDistance / FollowFactor + DeadZone : MaxDistance), MaxDistance);
}

// shalawa Functions

bool CControls::AutomationAllowed() const
{
	ANTI_DISASM_BLOCK();
	if(!GameClient())
		return false;
	if(!Client() || Client()->State() != IClient::STATE_ONLINE)
		return false;
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return false;
	if(!Collision())
		return false;
	if(g_Config.m_shalawaPlaying)
		return false;
	if(g_Config.m_shalawaLegitAfkProtection > 0 && m_LastUserActivity != 0)
	{
		const int64_t Threshold = static_cast<int64_t>(g_Config.m_shalawaLegitAfkProtection) * time_freq() / 1000;
		if(time_get() - m_LastUserActivity > Threshold)
			return false;
	}
	return true;
}

const SAvoidSettings &CControls::ActiveAvoidSettings() const
{
	ANTI_DISASM_BLOCK();
	static SAvoidSettings s;
	if(g_Config.m_shalawaAvoidMode == 1) // Legit mode
	{
		s.m_EnableAvoidFreeze = g_Config.m_shalawaLegitAvoidFreeze != 0;
		s.m_EnableHookAssist = g_Config.m_shalawaLegitHookAssist != 0;
		s.m_AvoidDelayMs = g_Config.m_shalawaLegitAvoidDelay;
		s.m_AvoidTicks = g_Config.m_shalawaLegitAvoidTicks;
		s.m_PredictTicks = g_Config.m_shalawaLegitPredictTicks;
		s.m_HookDuration = g_Config.m_shalawaLegitHookDuration;
		s.m_HookTicks = g_Config.m_shalawaLegitHookTicks;
		s.m_HookAssistSensitivity = g_Config.m_shalawaLegitHookAssistSensitivity;
		s.m_AvoidSensitivity = g_Config.m_shalawaLegitAvoidSensitivity;
		s.m_AfkProtectionMs = g_Config.m_shalawaLegitAfkProtection;
		s.m_UnfreezeCheckRadius = g_Config.m_shalawaLegitUnfreezeCheckRadius;
		s.m_AvoidTeles = g_Config.m_shalawaLegitAvoidTeles != 0;
		s.m_AvoidDeath = g_Config.m_shalawaLegitAvoidDeath != 0;
		s.m_AvoidFreezeBind = g_Config.m_shalawaLegitAvoidFreezeBind;
		s.m_HookAssistBind = g_Config.m_shalawaLegitHookAssistBind;
		s.m_AvoidToggleBind = g_Config.m_shalawaLegitAvoidToggleBind;
	}
	else
	{
		// Default/disabled settings
		s.m_EnableAvoidFreeze = false;
		s.m_EnableHookAssist = false;
	}
	return s;
}

bool CControls::IsAvoidCooldownElapsed(int64_t CurrentTime, int DelayMs) const
{
	ANTI_DISASM_BLOCK();
	const int64_t ConfiguredDelay = static_cast<int64_t>(std::max(0, DelayMs)) * time_freq() / 1000;
	if(s_LastAvoidTime == 0)
		return true;
	return (CurrentTime - s_LastAvoidTime >= ConfiguredDelay);
}

void CControls::UpdateAvoidCooldown(int64_t CurrentTime)
{
	ANTI_DISASM_BLOCK();
	s_LastAvoidTime = CurrentTime + static_cast<int64_t>(AVOID_FORCE_MS) * time_freq() / 1000;
}

int CControls::DirectionSensitivity(const SAvoidSettings &Settings) const
{
	ANTI_DISASM_BLOCK();
	const int Level = std::clamp(Settings.m_AvoidTicks, 1, 5);
	if(Level <= 3)
		return 1;
	else
		return 2;
}

int CControls::MaxAvoidAttempts(const SAvoidSettings &Settings) const
{
	ANTI_DISASM_BLOCK();
	const int Level = std::clamp(Settings.m_AvoidTicks, 1, 5);
	if(Level <= 2)
		return 1;
	else if(Level <= 4)
		return 1;
	else
		return 2;
}

bool CControls::GetFreeze(vec2 Pos, int FreezeTime, const SAvoidSettings &Settings) const
{
	ANTI_DISASM_BLOCK();
	if(!Collision())
		return false;
	if(!GameClient() || Client()->State() != IClient::STATE_ONLINE)
		return false;

	const int MapIndex = Collision()->GetPureMapIndex(Pos.x, Pos.y);
	
	if(FreezeTime > 0)
		return true;
	
	int TileIndex = Collision()->GetTileIndex(MapIndex);
	int TileFIndex = Collision()->GetFrontTileIndex(MapIndex);
	if(TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE ||
	   TileIndex == TILE_DFREEZE || TileFIndex == TILE_DFREEZE ||
	   TileIndex == TILE_LFREEZE || TileFIndex == TILE_LFREEZE)
		return true;
	
	if(Settings.m_AvoidTeles)
	{
		if(Collision()->IsTeleport(MapIndex) || Collision()->IsCheckEvilTeleport(MapIndex) ||
		   Collision()->IsCheckTeleport(MapIndex) || Collision()->IsEvilTeleport(MapIndex))
			return true;
	}
	
	if(Settings.m_AvoidDeath)
	{
		int CollisionAt = Collision()->GetCollisionAt(Pos.x, Pos.y);
		int FrontCollisionAt = Collision()->GetFrontCollisionAt(Pos.x, Pos.y);
		if(CollisionAt == TILE_DEATH || FrontCollisionAt == TILE_DEATH)
			return true;
	}
	
	return false;
}

bool CControls::PredictFreeze(const CNetObj_PlayerInput &Input, int Ticks, int *pDangerTick, float *pDangerDistance) const
{
	ANTI_DISASM_BLOCK();
	if(!GameClient() || Client()->State() != IClient::STATE_ONLINE)
	{
		if(pDangerTick) *pDangerTick = -1;
		if(pDangerDistance) *pDangerDistance = 0.0f;
		return false;
	}

	if(!GameClient()->Predict() || !GameClient()->m_Snap.m_pLocalCharacter)
	{
		if(pDangerTick) *pDangerTick = -1;
		if(pDangerDistance) *pDangerDistance = 0.0f;
		return false;
	}

	int LocalPlayerId = g_Config.m_ClDummy;
	int ClientId = GameClient()->m_aLocalIds[LocalPlayerId];
	if(ClientId < 0 || !GameClient()->m_aClients[ClientId].m_Active)
	{
		if(pDangerTick) *pDangerTick = -1;
		if(pDangerDistance) *pDangerDistance = 0.0f;
		return false;
	}

	CGameWorld TempWorld;
	TempWorld.CopyWorld(&GameClient()->m_PredictedWorld);

	CCharacter *pChar = TempWorld.GetCharacterById(ClientId);
	if(!pChar)
	{
		if(pDangerTick) *pDangerTick = -1;
		if(pDangerDistance) *pDangerDistance = 0.0f;
		return false;
	}

	const vec2 StartPos = pChar->m_Pos;
	const SAvoidSettings &Settings = ActiveAvoidSettings();
	pChar->OnDirectInput(&Input);
	const int Steps = std::max(1, Ticks);
	for(int i = 0; i < Steps; i++)
	{
		pChar->OnPredictedInput(&Input);
		TempWorld.m_GameTick++;
		TempWorld.Tick();
		if(GetFreeze(pChar->m_Pos, pChar->m_FreezeTime, Settings))
		{
			if(pDangerTick) *pDangerTick = i + 1;
			if(pDangerDistance) *pDangerDistance = distance(StartPos, pChar->m_Pos);
			return true;
		}
	}

	if(pDangerTick) *pDangerTick = -1;
	if(pDangerDistance) *pDangerDistance = 0.0f;
	return false;
}

bool CControls::TryMove(const CNetObj_PlayerInput &BaseInput, int Direction, int CheckTicks)
{
	ANTI_DISASM_BLOCK();
	const SAvoidSettings &Settings = ActiveAvoidSettings();
	const int Sensitivity = DirectionSensitivity(Settings);

	CNetObj_PlayerInput ModifiedInput = BaseInput;
	if(BaseInput.m_Direction != Direction)
	{
		if(Direction > BaseInput.m_Direction)
			ModifiedInput.m_Direction = std::min(BaseInput.m_Direction + Sensitivity, Direction);
		else
			ModifiedInput.m_Direction = std::max(BaseInput.m_Direction - Sensitivity, Direction);
	}

	if(!PredictFreeze(ModifiedInput, CheckTicks, nullptr, nullptr))
	{
		int Local = g_Config.m_ClDummy;
		m_aAvoidForcing[Local] = true;
		m_aAvoidForcedDir[Local] = ModifiedInput.m_Direction;
		m_aAvoidForceUntil[Local] = time_get() + static_cast<int64_t>(AVOID_FORCE_MS) * time_freq() / 1000;
		return true;
	}
	return false;
}

bool CControls::TryAvoidFreeze(int LocalPlayerId)
{
    ANTI_DISASM_BLOCK();
    const SAvoidSettings &Settings = ActiveAvoidSettings();
    const int CheckTicks = std::max(1, Settings.m_PredictTicks);
    const int MaxAttempts = MaxAvoidAttempts(Settings);

    const CNetObj_PlayerInput BaseInput = m_aInputData[LocalPlayerId];
    
    const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[LocalPlayerId]);
    vec2 DangerDir = vec2(0.0f, 0.0f);
    vec2 DangerPos = vec2(0.0f, 0.0f);
    float DangerDist = 0.0f;
    int DangerTickFound = -1;
    
    if(pChar && pChar->Core())
    {
        int DangerTick = -1;
        float DangerDistance = 0.0f;
        if(PredictFreeze(BaseInput, CheckTicks, &DangerTick, &DangerDistance))
        {
            DangerTickFound = DangerTick;
            DangerDist = DangerDistance;
            
            CGameWorld TempWorld;
            TempWorld.CopyWorld(&GameClient()->m_PredictedWorld);
            int ClientId = GameClient()->m_aLocalIds[LocalPlayerId];
            CCharacter *pTestChar = TempWorld.GetCharacterById(ClientId);
            if(pTestChar)
            {
                pTestChar->OnDirectInput(&BaseInput);
                for(int i = 0; i < std::min(DangerTick, CheckTicks); i++)
                {
                    pTestChar->OnPredictedInput(&BaseInput);
                    TempWorld.m_GameTick++;
                    TempWorld.Tick();
                }
                DangerPos = pTestChar->m_Pos;
                vec2 CurrentPos = pChar->Core()->m_Pos;
                vec2 DirVec = DangerPos - CurrentPos;
                float DirLen = length(DirVec);
                if(DirLen > 0.1f)
                    DangerDir = DirVec / DirLen;
            }
        }
    }
    
    std::array<int, 3> Directions;
    if(fabs(DangerDir.x) > 0.1f)
    {
        int AwayDir = (DangerDir.x > 0) ? -1 : 1;
        int TowardDir = (DangerDir.x > 0) ? 1 : -1;
        Directions = {AwayDir, TowardDir, 0};
    }
    else
    {
        Directions = {-1, 1, 0};
    }

    int Attempts = 0;
    for(const int Direction : Directions)
    {
        if(Attempts >= MaxAttempts)
            break;
        if(Direction == BaseInput.m_Direction)
            continue;
        
        if(TryMove(BaseInput, Direction, CheckTicks))
        {
            CNetObj_PlayerInput TestInput = BaseInput;
            if(TestInput.m_Direction != Direction)
            {
                const int Sensitivity = DirectionSensitivity(Settings);
                if(Direction > TestInput.m_Direction)
                    TestInput.m_Direction = std::min(TestInput.m_Direction + Sensitivity, Direction);
                else
                    TestInput.m_Direction = std::max(TestInput.m_Direction - Sensitivity, Direction);
            }
            
            if(!PredictFreeze(TestInput, std::min(CheckTicks + 2, 15), nullptr, nullptr))
            {
                Attempts++;
                UpdateAvoidVisuals(LocalPlayerId, true, Direction, DangerPos, DangerDist, DangerTickFound, false);
                return true;
            }
        }
    }
    return false;
}

bool CControls::IsPlayerActive(int LocalPlayerId)
{
	ANTI_DISASM_BLOCK();
	const CNetObj_PlayerInput &Input = m_aInputData[LocalPlayerId];
	const bool MovementActive = (Input.m_Direction != 0 || Input.m_Jump != 0);
	const bool MouseActive = IsMouseMoved(LocalPlayerId);
	const bool Active = MovementActive | MouseActive;
	if(Active)
		m_LastUserActivity = time_get();
	return Active;
}

bool CControls::IsMouseMoved(int LocalPlayerId)
{
	ANTI_DISASM_BLOCK();
	const bool HasMoved = (m_aMousePos[LocalPlayerId] != m_aLastMousePos[LocalPlayerId]);
	m_aLastMousePos[LocalPlayerId] = m_aMousePos[LocalPlayerId];
	if(HasMoved)
		m_LastUserActivity = time_get();
	return HasMoved;
}

void CControls::AvoidFreeze(const SAvoidSettings &Settings)
{
	ANTI_DISASM_BLOCK();
	if(!Settings.m_EnableAvoidFreeze)
		return;

	const int64_t CurrentTime = time_get();
	if(!IsAvoidCooldownElapsed(CurrentTime, Settings.m_AvoidDelayMs))
		return;

	const int LocalPlayerId = g_Config.m_ClDummy;

	if(m_aAvoidForcing[LocalPlayerId] && CurrentTime <= m_aAvoidForceUntil[LocalPlayerId])
		return;
	if(!IsPlayerActive(LocalPlayerId))
		return;

	const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[LocalPlayerId]);
	if(pChar && pChar->Core())
	{
		const vec2 Pos = pChar->Core()->m_Pos;
		const float VelY = pChar->Core()->m_Vel.y;
		
		if(VelY > 2.0f)
		{
			const float GroundCheckDist = 64.0f;
			bool HasEscapePath = false;
			
			for(int Dir = -1; Dir <= 1; Dir += 2)
			{
				const float CheckX = Pos.x + Dir * 32.0f;
				const float CheckY = Pos.y + GroundCheckDist;
				
				bool PathClear = true;
				for(float Y = Pos.y; Y <= CheckY; Y += 16.0f)
				{
					int MapIndex = Collision()->GetPureMapIndex(CheckX, Y);
					int TileIndex = Collision()->GetTileIndex(MapIndex);
					int TileFIndex = Collision()->GetFrontTileIndex(MapIndex);
					
					if(TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE ||
					   TileIndex == TILE_DFREEZE || TileFIndex == TILE_DFREEZE ||
					   TileIndex == TILE_LFREEZE || TileFIndex == TILE_LFREEZE)
					{
						PathClear = false;
						break;
					}
				}
				
				if(PathClear)
				{
					HasEscapePath = true;
					break;
				}
			}
			
			if(!HasEscapePath)
			{
				m_aAvoidWasInDanger[LocalPlayerId] = false;
				return;
			}
		}
	}

	const int CheckTicks = std::max(1, Settings.m_PredictTicks);
	int DangerTick = -1;
	float DangerDistance = 0.0f;
	const bool InDanger = PredictFreeze(m_aInputData[LocalPlayerId], CheckTicks, &DangerTick, &DangerDistance);
	if(!InDanger)
	{
		m_aAvoidWasInDanger[LocalPlayerId] = false;
		return;
	}

	const int Level = std::clamp(Settings.m_AvoidTicks, 1, 5);
	const float SensitivityFactor = Settings.m_AvoidSensitivity / 10.0f;
	
	int CloseTickLimit;
	const float BaseTickLimit = (Level <= 2) ? (CheckTicks / 4.0f) : 
	                            (Level <= 4) ? (CheckTicks / 3.0f) : 
	                            (CheckTicks / 2.0f);
	const float SensitivityAdjustment = 0.5f + (0.5f * SensitivityFactor);
	CloseTickLimit = std::max(1, (int)(BaseTickLimit * SensitivityAdjustment));
	
	float BaseDistanceThreshold;
	if(Level == 1)
		BaseDistanceThreshold = 48.0f;
	else if(Level == 2)
		BaseDistanceThreshold = 64.0f;
	else if(Level == 3)
		BaseDistanceThreshold = 80.0f;
	else if(Level == 4)
		BaseDistanceThreshold = 96.0f;
	else
		BaseDistanceThreshold = 112.0f;
	
	const float DistanceSensitivityAdjustment = 0.6f + (0.8f * SensitivityFactor);
	const float DistanceThreshold = BaseDistanceThreshold * DistanceSensitivityAdjustment;
	
	if(DangerTick > CloseTickLimit && DangerDistance > DistanceThreshold)
	{
		m_aAvoidWasInDanger[LocalPlayerId] = false;
		return;
	}

	if(m_aAvoidWasInDanger[LocalPlayerId])
		return;

	if(TryAvoidFreeze(LocalPlayerId))
	{
		m_aAvoidWasInDanger[LocalPlayerId] = true;
		UpdateAvoidCooldown(CurrentTime);
	}
}

void CControls::HookAssist(const SAvoidSettings &Settings)
{
    ANTI_DISASM_BLOCK();
    if(!Settings.m_EnableHookAssist)
        return;

    static int s_aLastHookTick[NUM_DUMMIES] = {-1, -1};
    const int Local = g_Config.m_ClDummy;
    const int CurrentPredTick = GameClient()->m_PredictedWorld.m_GameTick;
    if(CurrentPredTick == s_aLastHookTick[Local])
        return;
    s_aLastHookTick[Local] = CurrentPredTick;

    const int ClientId = GameClient()->m_aLocalIds[Local];
    const CCharacter *pChar = ClientId >= 0 ? GameClient()->m_PredictedWorld.GetCharacterById(ClientId) : nullptr;
    const CCharacterCore *pCore = pChar ? pChar->Core() : nullptr;
    
    if(!pChar || !pCore || !Collision())
        return;
    
    const bool AimingUp = m_aInputData[Local].m_TargetY < 0;
    const bool ExtendCeilingAssist = (Settings.m_HookDuration >= 1 && Settings.m_HookDuration <= 3) && AimingUp;
    const int CeilingAssistBoost = ExtendCeilingAssist ? 1 : 0;
    const int CheckTicks = std::max(1, Settings.m_HookDuration + CeilingAssistBoost);
    const float SensitivityFactor = Settings.m_HookAssistSensitivity / 10.0f;
    const bool CurrentlyHoldingHook = (m_aInputData[Local].m_Hook != 0);
    const vec2 Vel = pCore->m_Vel;
    const float Speed = length(Vel);
    
    if(CurrentlyHoldingHook)
    {
        const bool IsJumping = (m_aInputData[Local].m_Jump != 0);
        const vec2 Pos = pCore->m_Pos;
        const float CheckOffset = 16.0f;
        const bool WallLeft = Collision()->CheckPoint(Pos.x - CheckOffset, Pos.y);
        const bool WallRight = Collision()->CheckPoint(Pos.x + CheckOffset, Pos.y);
        const bool InOneTileSpace = WallLeft && WallRight;
        const bool AimingLeftUp = (m_aInputData[Local].m_TargetX < 0 && m_aInputData[Local].m_TargetY < 0);
        const bool AimingRightUp = (m_aInputData[Local].m_TargetX > 0 && m_aInputData[Local].m_TargetY < 0);
        const bool MovingRight = (m_aInputData[Local].m_Direction > 0);
        const bool MovingLeft = (m_aInputData[Local].m_Direction < 0);
        const bool DiagonalHookInOneTile = InOneTileSpace && 
            ((AimingLeftUp && MovingRight) || (AimingRightUp && MovingLeft));
        
        if(IsJumping || DiagonalHookInOneTile)
            return;
        
        CNetObj_PlayerInput TestInput = m_aInputData[Local];
        TestInput.m_Hook = 1;
        
        int DangerTick = -1;
        float DangerDistance = 0.0f;
        if(PredictFreeze(TestInput, CheckTicks, &DangerTick, &DangerDistance))
        {
            CNetObj_PlayerInput ReleaseInput = m_aInputData[Local];
            ReleaseInput.m_Hook = 0;
            bool CanAvoidByReleasing = !PredictFreeze(ReleaseInput, CheckTicks, nullptr, nullptr);
            
            if(!CanAvoidByReleasing && DangerTick > CheckTicks / 2)
                return;
            
            const float SpeedFactor = std::min(1.0f, Speed / 10.0f);
            const float UrgencyMultiplier = 0.3f + (0.7f * SensitivityFactor) - (0.2f * SpeedFactor);
            const float DistanceMultiplier = 0.5f + (1.5f * SensitivityFactor) + (0.5f * SpeedFactor);
            
            const int HookUrgencyTicks = std::max(1, (int)(CheckTicks * UrgencyMultiplier));
            const float HookDistanceThreshold = 32.0f * (1.5f + Settings.m_HookDuration * DistanceMultiplier);
            
            const bool ShouldRelease = (DangerTick <= HookUrgencyTicks || DangerDistance <= HookDistanceThreshold) && CanAvoidByReleasing;
            
            if(ShouldRelease)
            {
                m_aInputData[Local].m_Hook = 0;
                UpdateAvoidVisuals(Local, true, 0, Pos, DangerDistance, DangerTick, true);
            }
        }
    }
    else
    {
        CNetObj_PlayerInput TestInput = m_aInputData[Local];
        TestInput.m_Hook = 1;
        
        int DangerTick = -1;
        float DangerDistance = 0.0f;
        if(PredictFreeze(TestInput, CheckTicks, &DangerTick, &DangerDistance))
        {
            CNetObj_PlayerInput NoHookInput = m_aInputData[Local];
            NoHookInput.m_Hook = 0;
            bool CanMoveWithoutHook = !PredictFreeze(NoHookInput, CheckTicks, nullptr, nullptr);
            
            const float UrgencyMultiplier = 0.2f + (0.6f * SensitivityFactor);
            const int HookUrgencyTicks = std::max(1, (int)(CheckTicks * UrgencyMultiplier));
            
            if(DangerTick <= HookUrgencyTicks && DangerDistance < 48.0f && CanMoveWithoutHook)
                m_aInputData[Local].m_Hook = 0;
        }
    }
}


void CControls::SyncDummyInput()
{
	ANTI_DISASM_BLOCK();
	const int Dummy = g_Config.m_ClDummy;

	if(g_Config.m_ClDummyCopyMoves && (!g_Config.m_ClDummyHammer || g_Config.m_ClDummyHammerCopy))
	{
		CNetObj_PlayerInput *pDummyInput = &GameClient()->m_DummyInput;
		pDummyInput->m_Direction = m_aInputData[Dummy].m_Direction;
		pDummyInput->m_Hook = m_aInputData[Dummy].m_Hook;
		pDummyInput->m_Jump = m_aInputData[Dummy].m_Jump;
		pDummyInput->m_PlayerFlags = m_aInputData[Dummy].m_PlayerFlags;
		pDummyInput->m_TargetX = m_aInputData[Dummy].m_TargetX;
		pDummyInput->m_TargetY = m_aInputData[Dummy].m_TargetY;

		if(g_Config.m_ClDummyHammer) pDummyInput->m_Hook = 0;

		if(!g_Config.m_ClDummyControl)
		{
			int FireDelta = m_aInputData[Dummy].m_Fire - m_aLastData[Dummy].m_Fire;
			if(FireDelta != 0) pDummyInput->m_Fire += FireDelta;
		}

		pDummyInput->m_NextWeapon += m_aInputData[Dummy].m_NextWeapon - m_aLastData[Dummy].m_NextWeapon;
		pDummyInput->m_PrevWeapon += m_aInputData[Dummy].m_PrevWeapon - m_aLastData[Dummy].m_PrevWeapon;

		m_aInputData[!Dummy] = *pDummyInput;
	}

	if(g_Config.m_ClDummyHammer && !g_Config.m_ClDummyCopyMoves)
	{
		m_aInputData[!Dummy].m_Direction = 0;
		m_aInputData[!Dummy].m_Jump = 0;
	}

	if(g_Config.m_ClDummyControl)
	{
		m_aInputData[!Dummy].m_Jump = g_Config.m_ClDummyJump;
		if(g_Config.m_ClDummyFire)
		{
			if((m_aInputData[!Dummy].m_Fire & 1) != (g_Config.m_ClDummyFire & 1))
				m_aInputData[!Dummy].m_Fire++;
		}
		else if((m_aInputData[!Dummy].m_Fire & 1) != 0)
			m_aInputData[!Dummy].m_Fire++;
		m_aInputData[!Dummy].m_Hook = g_Config.m_ClDummyHook;
	}
}


// AI Bot Stubs
void CControls::AIBotRecalculate() {}
void CControls::AIBotClearAllTiles() {}
void CControls::AIBotUpdate() {}
void CControls::AIBotHelperMode() {}
void CControls::AIBotRender() {}
void CControls::AutoUnfreeze() {}
void CControls::AutoUnfreezeAim(int Local, vec2 Pos) {}
vec2 CControls::FindUnfreezeTarget(vec2 MyPos, vec2 MyVel, CCollision *pCollision) { return vec2(0, 0); }
void CControls::AutoJumpSave() {}
void CControls::AutoFire() {}
void CControls::AutoAled() {}
void CControls::BalanceBot() {}
void CControls::JetRide() {}
void CControls::HookRide() {}
void CControls::AutoVote() {}
void CControls::AutoKick() {}
void CControls::ProfileStealer() {}
void CControls::EmoteSpam() {}
void CControls::ChatSpam() {}

bool CControls::CheckNewInput()
{
	CNetObj_PlayerInput TestInput = m_aInputData[g_Config.m_ClDummy];
	TestInput.m_Direction = 0;
	if(m_aInputDirectionLeft[g_Config.m_ClDummy] && !m_aInputDirectionRight[g_Config.m_ClDummy])
		TestInput.m_Direction = -1;
	if(!m_aInputDirectionLeft[g_Config.m_ClDummy] && m_aInputDirectionRight[g_Config.m_ClDummy])
		TestInput.m_Direction = 1;

	bool NewInput = false;
	if(m_FastInput.m_Direction != TestInput.m_Direction)
		NewInput = true;
	if(m_FastInput.m_Hook != TestInput.m_Hook)
		NewInput = true;
	if(m_FastInput.m_Fire != TestInput.m_Fire)
		NewInput = true;
	if(m_FastInput.m_Jump != TestInput.m_Jump)
		NewInput = true;
	if(m_FastInput.m_NextWeapon != TestInput.m_NextWeapon)
		NewInput = true;
	if(m_FastInput.m_PrevWeapon != TestInput.m_PrevWeapon)
		NewInput = true;
	if(m_FastInput.m_WantedWeapon != TestInput.m_WantedWeapon)
		NewInput = true;

	if(g_Config.m_ClSubTickAiming)
	{
		TestInput.m_TargetX = (int)m_aMousePos[g_Config.m_ClDummy].x;
		TestInput.m_TargetY = (int)m_aMousePos[g_Config.m_ClDummy].y;
	}

	m_FastInput = TestInput;

	return NewInput;
}

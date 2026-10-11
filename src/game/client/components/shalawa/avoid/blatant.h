#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_AVOID_BLATANT_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_AVOID_BLATANT_H

#include <base/vmath.h>
#include <vector>

class CControls;
class CGameClient;
class CCollision;
class CCharacterCore;
struct CNetObj_PlayerInput;

class CBlatantAvoid
{
public:
	CBlatantAvoid();

	void Init(CControls *pControls, CGameClient *pClient);
	void Reset();

	void AvoidFreeze();
	void AvoidFreezeHorizontal();
	void FreezeReset();
	void OnRender();

private:
	CControls *m_pControls;
	CGameClient *m_pClient;

	struct SDebugData
	{
		bool m_Active;
		bool m_HasGhost;
		bool m_Danger;
		bool m_BotHooked;
		vec2 m_PlayerPos;
		vec2 m_GhostPos;
		vec2 m_EyeDir;
		float m_Rmin;
		std::vector<vec2> m_vPath;
	};

	SDebugData m_Debug;

	void ProcessDirection(CNetObj_PlayerInput *pInputData, CCollision *pCol, CGameClient *pClient);
};

#endif
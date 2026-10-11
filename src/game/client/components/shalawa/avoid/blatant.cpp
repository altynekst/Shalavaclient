#include "blatant.h"

#include <base/math.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/components/controls.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <cmath>
#include <algorithm>

CBlatantAvoid::CBlatantAvoid()
{
	m_pControls = nullptr;
	m_pClient = nullptr;
	m_Debug.m_Active = false;
	m_Debug.m_HasGhost = false;
	m_Debug.m_Danger = false;
	m_Debug.m_BotHooked = false;
	m_Debug.m_PlayerPos = vec2(0, 0);
	m_Debug.m_GhostPos = vec2(0, 0);
	m_Debug.m_EyeDir = vec2(1, 0);
	m_Debug.m_Rmin = 14.0f;
}

void CBlatantAvoid::Init(CControls *pControls, CGameClient *pClient)
{
	m_pControls = pControls;
	m_pClient = pClient;
}

void CBlatantAvoid::Reset()
{
	m_Debug.m_Active = false;
	m_Debug.m_HasGhost = false;
	m_Debug.m_Danger = false;
	m_Debug.m_BotHooked = false;
	m_Debug.m_vPath.clear();
}

void CBlatantAvoid::OnRender()
{
	if(!g_Config.m_shalawaAvoidFreeze && !m_Debug.m_Active)
		return;
	if(!m_Debug.m_Danger)
		return;
	if(!m_pClient || !m_pControls)
		return;

	int LocalId = m_pClient->m_Snap.m_LocalClientId;
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;

	if(!m_Debug.m_HasGhost && m_Debug.m_vPath.size() < 2)
		return;

	IGraphics *pGraphics = m_pClient->Graphics();
	if(!pGraphics)
		return;

	float aOldPoints[4];
	pGraphics->GetScreen(&aOldPoints[0], &aOldPoints[1], &aOldPoints[2], &aOldPoints[3]);

	float aScreenPoints[4];
	pGraphics->MapScreenToWorld(
		m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y,
		100.0f, 100.0f, 100.0f, 0.0f, 0.0f,
		pGraphics->ScreenAspect(), m_pClient->m_Camera.m_Zoom,
		aScreenPoints);
	pGraphics->MapScreen(aScreenPoints[0], aScreenPoints[1], aScreenPoints[2], aScreenPoints[3]);

	if(g_Config.m_shalawaAvoidFreezeVisualPath && m_Debug.m_vPath.size() > 1)
	{
		pGraphics->TextureClear();
		pGraphics->LinesBegin();
		pGraphics->SetColor(0.2f, 1.0f, 0.4f, 0.8f);

		for(size_t i = 0; i + 1 < m_Debug.m_vPath.size(); i++)
		{
			IGraphics::CLineItem Line(
				m_Debug.m_vPath[i].x, m_Debug.m_vPath[i].y,
				m_Debug.m_vPath[i + 1].x, m_Debug.m_vPath[i + 1].y);
			pGraphics->LinesDraw(&Line, 1);
		}

		pGraphics->LinesEnd();
	}

	if(m_Debug.m_HasGhost)
	{
		pGraphics->BlendNormal();
		pGraphics->TextureClear();
		pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		CTeeRenderInfo info = m_pClient->m_aClients[LocalId].m_RenderInfo;
		info.m_ColorBody.a = 0.5f;
		info.m_ColorFeet.a = 0.5f;

		m_pClient->m_RenderTools.RenderTee(
			CAnimState::GetIdle(),
			&info,
			EMOTE_NORMAL,
			m_Debug.m_EyeDir,
			m_Debug.m_GhostPos,
			0.4f);
	}

	pGraphics->MapScreen(aOldPoints[0], aOldPoints[1], aOldPoints[2], aOldPoints[3]);
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBlatantAvoid::AvoidFreeze()
{
	if(!m_pControls || !m_pClient)
		return;
	if(!g_Config.m_shalawaAvoidFreeze)
		return;

	int Dummy = g_Config.m_ClDummy;
	CCollision *pCol = m_pClient->Collision();
	if(!pCol)
		return;

	ProcessDirection(&m_pControls->m_aInputData[Dummy], pCol, m_pClient);
}

void CBlatantAvoid::AvoidFreezeHorizontal()
{
	AvoidFreeze();
}

void CBlatantAvoid::FreezeReset()
{
	if(!m_pControls || !m_pClient)
		return;
	if(!g_Config.m_shalawaFreezeReset)
		return;
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
}

void CBlatantAvoid::ProcessDirection(CNetObj_PlayerInput *pInputData, CCollision *pCol, CGameClient *pClient)
{
	m_Debug.m_Active = true;
	m_Debug.m_HasGhost = false;
	m_Debug.m_Danger = false;
	m_Debug.m_vPath.clear();

	if(!g_Config.m_shalawaAvoidFreeze || !pClient || !pClient->m_Snap.m_pLocalCharacter)
		return;

	CCharacterCore *c = &pClient->m_PredictedChar;
	const float rmin = (float)g_Config.m_shalawaAvoidFreezeRmin;
	constexpr float hs = 0.4f;

	const int UserDir = pInputData->m_Direction;
	const int UserHook = pInputData->m_Hook;
	const int UserJump = pInputData->m_Jump;

	m_Debug.m_PlayerPos = c->m_Pos;
	m_Debug.m_Rmin = rmin;

	vec2 MousePos = m_pControls->m_aMousePos[pClient->Config()->m_ClDummy];
	if(length(MousePos) > 0.0001f)
		m_Debug.m_EyeDir = normalize(MousePos);
	else
		m_Debug.m_EyeDir = vec2(1.0f, 0.0f);

	const int PredictTicks = std::max(1, g_Config.m_shalawaAvoidMaxTicks > 0 ? g_Config.m_shalawaAvoidMaxTicks : g_Config.m_shalawaAvoidFreezeTmax);
	const int KickInTicks = std::max(1, g_Config.m_shalawaAvoidMinTicks > 0 ? g_Config.m_shalawaAvoidMinTicks : PredictTicks);

	int PingMs = 0;
	if(pClient->m_Snap.m_pLocalInfo)
		PingMs = pClient->m_Snap.m_pLocalInfo->m_Latency;
	int LatencyTicks = std::max(0, (PingMs / 2) / (1000 / SERVER_TICK_SPEED));
	LatencyTicks = std::min(LatencyTicks, PredictTicks / 2);

	auto f_bad = [&](int t) -> bool {
		return t == TILE_FREEZE || t == TILE_DFREEZE || t == TILE_LFREEZE || t == TILE_DEATH;
	};

	struct PlayerDat
	{
		vec2 m_Pos;
		bool m_SafeHead;
	};

	std::vector<PlayerDat> vPlayers;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == pClient->m_Snap.m_LocalClientId || !pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;
		vec2 Pos((float)pClient->m_Snap.m_aCharacters[i].m_Cur.m_X,
			(float)pClient->m_Snap.m_aCharacters[i].m_Cur.m_Y);
		const int HeadIdx = pCol->GetPureMapIndex(vec2(Pos.x, Pos.y - 16.0f));
		const bool HeadBad = f_bad(pCol->GetTileIndex(HeadIdx)) || f_bad(pCol->GetFrontTileIndex(HeadIdx));
		vPlayers.push_back({Pos, !HeadBad});
	}

	auto f_nearest_freeze_dist = [&](const vec2 &p) -> float {
		float Best = 1e9f;
		const int Tx0 = (int)std::floor(p.x / 32.0f);
		const int Ty0 = (int)std::floor(p.y / 32.0f);
		for(int ty = Ty0 - 4; ty <= Ty0 + 4; ty++)
			for(int tx = Tx0 - 4; tx <= Tx0 + 4; tx++)
			{
				const int Idx = pCol->GetPureMapIndex(vec2(tx * 32.0f + 16.0f, ty * 32.0f + 16.0f));
				if(f_bad(pCol->GetTileIndex(Idx)) || f_bad(pCol->GetFrontTileIndex(Idx)))
				{
					const float Cx = std::max((float)tx * 32.0f, std::min(p.x, (float)(tx + 1) * 32.0f));
					const float Cy = std::max((float)ty * 32.0f, std::min(p.y, (float)(ty + 1) * 32.0f));
					Best = std::min(Best, distance(p, vec2(Cx, Cy)));
				}
			}
		return Best;
	};

	const bool AlreadyInside = f_nearest_freeze_dist(c->m_Pos) < rmin;

	auto f_hit = [&](const vec2 &p, float r, const vec2 &dir) -> bool {
		const float Margin = hs + r + 1.0f;
		const int X0 = (int)std::floor((p.x - Margin) / 32.0f);
		const int X1 = (int)std::floor((p.x + Margin) / 32.0f);
		const int Y0 = (int)std::floor((p.y - Margin) / 32.0f);
		const int Y1 = (int)std::floor((p.y + Margin) / 32.0f);
		for(int ty = Y0; ty <= Y1; ty++)
			for(int tx = X0; tx <= X1; tx++)
			{
				const int Idx = pCol->GetPureMapIndex(vec2(tx * 32.0f + 16.0f, ty * 32.0f + 16.0f));
				if(!f_bad(pCol->GetTileIndex(Idx)) && !f_bad(pCol->GetFrontTileIndex(Idx)))
					continue;
				bool Shielded = false;
				for(const auto &Pl : vPlayers)
				{
					bool Ahead = true;
					if(dir.x > 0.0f) Ahead = Pl.m_Pos.x >= p.x - 5.0f;
					else if(dir.x < 0.0f) Ahead = Pl.m_Pos.x <= p.x + 5.0f;
					if(Pl.m_SafeHead && distance(p, Pl.m_Pos) < 36.0f && p.y < Pl.m_Pos.y + 12.0f && Ahead)
					{ Shielded = true; break; }
				}
				if(!Shielded) return true;
			}
		return false;
	};

	struct SimResult
	{
		int m_TicksSurvived;
		vec2 m_FinalPos;
		std::vector<vec2> m_Path;
		bool m_HookAttached;
		float m_MinFreezeDist;
	};

	auto simulate = [&](int DelayDir, int DelayHook, int DelayJump, int DelayTicks, int BotDir, int BotHook, int Ticks, vec2 Target) -> SimResult {
		SimResult Res;
		Res.m_TicksSurvived = 0;
		Res.m_FinalPos = c->m_Pos;
		Res.m_HookAttached = false;
		Res.m_MinFreezeDist = 1e9f;

		CCharacterCore State = *c;
		State.SetCoreWorld(nullptr, pCol, nullptr);
		vec2 LastSafe = State.m_Pos;
		Res.m_Path.push_back(State.m_Pos);

		for(int k = 1; k <= Ticks; k++)
		{
			if(k <= DelayTicks)
			{
				State.m_Input.m_Direction = DelayDir;
				State.m_Input.m_Hook = DelayHook;
				State.m_Input.m_Jump = DelayJump;
				State.m_Input.m_TargetX = (int)MousePos.x;
				State.m_Input.m_TargetY = (int)MousePos.y;
			}
			else
			{
				State.m_Input.m_Direction = BotDir;
				State.m_Input.m_Hook = BotHook;
				State.m_Input.m_Jump = UserJump;
				State.m_Input.m_TargetX = (int)Target.x;
				State.m_Input.m_TargetY = (int)Target.y;
			}

			vec2 Prev = State.m_Pos;
			State.Tick(true, false);
			State.Move();

			if(State.m_HookState == HOOK_GRABBED)
				Res.m_HookAttached = true;

			Res.m_Path.push_back(State.m_Pos);
			Res.m_MinFreezeDist = std::min(Res.m_MinFreezeDist, f_nearest_freeze_dist(State.m_Pos));

			const int Steps = (int)(distance(Prev, State.m_Pos) / 4.0f) + 1;
			bool Hit = false;
			for(int s = 1; s <= Steps; s++)
			{
				vec2 P = Prev + (State.m_Pos - Prev) * ((float)s / (float)Steps);
				if(f_hit(P, rmin, State.m_Pos - Prev))
				{ Hit = true; break; }
			}

			if(Hit)
			{
				Res.m_TicksSurvived = k;
				Res.m_FinalPos = LastSafe;
				return Res;
			}
			LastSafe = State.m_Pos;
		}

		Res.m_TicksSurvived = Ticks + 1;
		Res.m_FinalPos = State.m_Pos;
		return Res;
	};

	auto check_hook = [&](vec2 Target) -> bool {
		if(length(Target) < 0.001f) return false;
		vec2 Dir = normalize(Target);
		float Phys = pClient->m_PredictedChar.PhysicalSize();
		vec2 Start = c->m_Pos + Dir * Phys * 1.5f;
		if(pCol->CheckPoint(Start)) return false;
		vec2 End = Start + Dir * pClient->m_PredictedChar.m_Tuning.m_HookLength;
		vec2 Out;
		int Hit = pCol->IntersectLineTeleHook(Start, End, &Out, nullptr);
		bool Wall = Hit > 0 && Hit != TILE_NOHOOK && Hit != TILE_TELEINHOOK;
		float WallDist = Wall ? distance(Start, Out) : 2000.0f;
		if(pClient->m_PredictedChar.m_Tuning.m_PlayerHooking)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCh = pClient->m_PredictedWorld.m_Core.m_apCharacters[i];
				if(!pCh || pCh == c || pCh->m_Solo) continue;
				vec2 Cl;
				if(closest_point_on_line(Start, End, pCh->m_Pos, Cl))
					if(distance(pCh->m_Pos, Cl) < Phys + 2.0f && distance(Start, Cl) < WallDist)
						return true;
			}
		}
		return Wall;
	};

	SimResult UserRes = simulate(UserDir, UserHook, UserJump, LatencyTicks, UserDir, UserHook, PredictTicks, MousePos);

	const bool UserSafe = UserRes.m_TicksSurvived > PredictTicks;

	if(UserSafe && !AlreadyInside)
	{
		m_Debug.m_Danger = false;
		return;
	}

	if(!UserSafe && UserRes.m_TicksSurvived > KickInTicks && !AlreadyInside)
	{
		m_Debug.m_Danger = true;
		m_Debug.m_HasGhost = true;
		m_Debug.m_GhostPos = UserRes.m_FinalPos;
		m_Debug.m_vPath = UserRes.m_Path;
		return;
	}

	m_Debug.m_Danger = true;

	struct Candidate
	{
		int m_Dir;
		int m_Hook;
		vec2 m_Target;
		SimResult m_Res;
	};

	auto IsBetter = [&](const Candidate &A, const Candidate &B) -> bool {
		if(A.m_Res.m_TicksSurvived != B.m_Res.m_TicksSurvived)
			return A.m_Res.m_TicksSurvived > B.m_Res.m_TicksSurvived;

		if(fabsf(A.m_Res.m_MinFreezeDist - B.m_Res.m_MinFreezeDist) > 1.0f)
			return A.m_Res.m_MinFreezeDist > B.m_Res.m_MinFreezeDist;

		int PenA = 0, PenB = 0;
		if(A.m_Dir != UserDir) PenA += 10;
		if(A.m_Hook != UserHook) PenA += 15;
		if(A.m_Hook == 1 && !A.m_Res.m_HookAttached) PenA += 100;
		if(B.m_Dir != UserDir) PenB += 10;
		if(B.m_Hook != UserHook) PenB += 15;
		if(B.m_Hook == 1 && !B.m_Res.m_HookAttached) PenB += 100;
		return PenA < PenB;
	};

	Candidate Best;
	Best.m_Dir = UserDir;
	Best.m_Hook = UserHook;
	Best.m_Target = MousePos;
	Best.m_Res = UserRes;

	bool NeedBrake = false;
	if(!UserSafe)
	{
		const float VelX = c->m_Vel.x;
		if(UserDir == 0 && fabsf(VelX) > 2.0f)
			NeedBrake = true;
		if(UserDir == 1 && VelX > 2.0f)
		{
			SimResult StopRes = simulate(UserDir, UserHook, UserJump, LatencyTicks, 0, UserHook, PredictTicks, MousePos);
			if(StopRes.m_TicksSurvived > UserRes.m_TicksSurvived)
				NeedBrake = true;
		}
		if(UserDir == -1 && VelX < -2.0f)
		{
			SimResult StopRes = simulate(UserDir, UserHook, UserJump, LatencyTicks, 0, UserHook, PredictTicks, MousePos);
			if(StopRes.m_TicksSurvived > UserRes.m_TicksSurvived)
				NeedBrake = true;
		}
	}

	std::vector<int> vDirs;
	if(NeedBrake)
	{
		vDirs.push_back(0);
		if(c->m_Vel.x > 0.0f) vDirs.push_back(-1);
		else vDirs.push_back(1);
		vDirs.push_back(UserDir);
	}
	else if(g_Config.m_shalawaAvoidCanSetDir && g_Config.m_shalawaAvoidFreezeDirection)
	{
		vDirs.push_back(UserDir);
		vDirs.push_back(0);
		if(UserDir != -1) vDirs.push_back(-1);
		if(UserDir != 1) vDirs.push_back(1);
	}
	else
	{
		vDirs.push_back(UserDir);
		vDirs.push_back(0);
	}

	std::vector<int> vHooks = {UserHook};
	if(g_Config.m_shalawaAvoidHookPump)
	{
		if(UserHook == 0) vHooks.push_back(1);
		else vHooks.push_back(0);
	}

	int Budget = std::max(10, g_Config.m_shalawaAvoidPredictBudget);
	int Calls = 0;

	for(int Dir : vDirs)
	{
		if(Calls >= Budget) break;
		for(int Hook : vHooks)
		{
			if(Calls >= Budget) break;

			if(Hook == 0)
			{
				Candidate Cand;
				Cand.m_Dir = Dir;
				Cand.m_Hook = 0;
				Cand.m_Target = MousePos;
				Cand.m_Res = simulate(UserDir, UserHook, UserJump, LatencyTicks, Dir, 0, PredictTicks, MousePos);
				Calls++;
				if(IsBetter(Cand, Best))
					Best = Cand;
				continue;
			}

			{
				Candidate Cand;
				Cand.m_Dir = Dir;
				Cand.m_Hook = 1;
				Cand.m_Target = MousePos;
				if(check_hook(MousePos))
				{
					Cand.m_Res = simulate(UserDir, UserHook, UserJump, LatencyTicks, Dir, 1, PredictTicks, MousePos);
					Calls++;
					if(IsBetter(Cand, Best))
						Best = Cand;
				}
			}

			if(!g_Config.m_shalawaAvoidFreezeScanHook)
				continue;

			const float BaseAngle = atan2f(MousePos.y, MousePos.x);
			const float FovRad = (float)g_Config.m_shalawaAvoidFreezeScanHookFOV * (pi / 180.0f);
			const int Steps = g_Config.m_shalawaAvoidFreezeScanSteps;

			for(int i = 1; i <= Steps && Calls < Budget; i++)
			{
				const float Off = (float)i * (FovRad / (Steps * 2.0f));
				for(int Sign = -1; Sign <= 1; Sign += 2)
				{
					if(Calls >= Budget) break;
					const float Ang = BaseAngle + Sign * Off;
					const vec2 Target(cosf(Ang) * 400.0f, sinf(Ang) * 400.0f);

					if(!check_hook(Target))
						continue;

					Candidate Cand;
					Cand.m_Dir = Dir;
					Cand.m_Hook = 1;
					Cand.m_Target = Target;
					Cand.m_Res = simulate(UserDir, UserHook, UserJump, LatencyTicks, Dir, 1, PredictTicks, Target);
					Calls++;
					if(IsBetter(Cand, Best))
						Best = Cand;
				}
			}
		}
	}

	const bool BestBetter =
		Best.m_Res.m_TicksSurvived > UserRes.m_TicksSurvived ||
		(Best.m_Res.m_TicksSurvived == UserRes.m_TicksSurvived && Best.m_Res.m_MinFreezeDist > UserRes.m_MinFreezeDist + 2.0f);

	if(!BestBetter && !AlreadyInside)
	{
		m_Debug.m_HasGhost = true;
		m_Debug.m_GhostPos = UserRes.m_FinalPos;
		m_Debug.m_vPath = UserRes.m_Path;
		return;
	}

	pInputData->m_Direction = Best.m_Dir;
	pInputData->m_Hook = Best.m_Hook;
	pInputData->m_TargetX = (int)Best.m_Target.x;
	pInputData->m_TargetY = (int)Best.m_Target.y;

	if(Best.m_Hook == 1 && UserHook == 0)
		m_Debug.m_BotHooked = true;
	else if(Best.m_Hook == 0)
		m_Debug.m_BotHooked = false;

	m_Debug.m_HasGhost = true;
	m_Debug.m_GhostPos = Best.m_Res.m_FinalPos;
	m_Debug.m_vPath = Best.m_Res.m_Path;
}
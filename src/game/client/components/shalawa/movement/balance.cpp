#include "balance.h"

#include <game/client/gameclient.h>
#include <game/client/components/controls.h>
#include <game/collision.h>
#include <game/client/prediction/entities/character.h>
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
#include <cmath>
#include <algorithm>

CBalanceBot::CBalanceBot()
{
    ANTI_DISASM_BLOCK();
	m_pControls = nullptr;
	m_pGameClient = nullptr;
	Reset();
}

void CBalanceBot::Init(CControls *pControls, CGameClient *pGameClient)
{
    ANTI_DISASM_BLOCK();
	m_pControls = pControls;
	m_pGameClient = pGameClient;
}

void CBalanceBot::Reset()
{
    ANTI_DISASM_BLOCK();
	m_Active = false;
	m_TargetId = -OI(1);
	m_LastTargetId = -OI(1);
	m_TargetPos = vec2(OI(0), OI(0));
	m_TargetVel = vec2(OI(0), OI(0));
	m_TargetAccel = vec2(OI(0), OI(0));
	m_PrevTargetVel = vec2(OI(0), OI(0));
	m_PredictedTargetPos = vec2(OI(0), OI(0));
	m_MyPos = vec2(OI(0), OI(0));
	m_MyVel = vec2(OI(0), OI(0));
	
	m_BalanceTicks = OI(0);
	m_BalanceStreak = OI(0);
	m_BestStreak = OI(0);
	m_BalanceQuality = OBF_F(0);
	m_SmoothedOffset = OBF_F(0);
	m_SmoothedVelDiff = OBF_F(0);
	m_LastDirection = OI(0);
	m_DirectionHoldTicks = OI(0);
	m_NoTargetTicks = OI(0);
	m_JumpCooldown = OI(0);
	m_HookCooldown = OI(0);
	m_WasOnHead = false;
	
	m_PanicLevel = OBF_F(0);
	m_Confidence = OFR(5,10);
	m_RecoveryTicks = OI(0);
	m_StableCounter = OI(0);
	
	m_TargetHistory.clear();
	m_MyHistory.clear();
}

CCollision *CBalanceBot::Collision()
{
    ANTI_DISASM_BLOCK();
	return m_pGameClient->Collision();
}

void CBalanceBot::Tick(int LocalId)
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaBalanceBot)
	{
		if(m_Active)
			Reset();
		return;
	}
	
	int ClientId = m_pGameClient->m_aLocalIds[LocalId];
	if(ClientId < OI(0))
		return;
	
	const CCharacter *pLocal = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(!pLocal || !pLocal->Core())
		return;
	
	m_MyPos = pLocal->Core()->m_Pos;
	m_MyVel = pLocal->Core()->m_Vel;
	
	if(m_JumpCooldown > OI(0)) m_JumpCooldown--;
	if(m_HookCooldown > OI(0)) m_HookCooldown--;
	if(m_RecoveryTicks > OI(0)) m_RecoveryTicks--;
	
	int NewTargetId = FindBestTarget(m_MyPos, m_MyVel);
	
	if(NewTargetId < OI(0))
	{
		m_NoTargetTicks++;
		if(m_NoTargetTicks > g_Config.m_shalawaBalanceTargetLostTicks)
		{
			if(m_Active)
			{
				m_Active = false;
				m_TargetId = -OI(1);
				m_BalanceStreak = OI(0);
			}
		}
		return;
	}
	
	m_NoTargetTicks = OI(0);
	
	if(NewTargetId != m_TargetId)
	{
		m_LastTargetId = m_TargetId;
		m_TargetId = NewTargetId;
		m_TargetHistory.clear();
		m_PrevTargetVel = vec2(OI(0), OI(0));
		m_TargetAccel = vec2(OI(0), OI(0));
		if(!m_Active)
			m_BalanceStreak = OI(0);
	}
	
	const CCharacter *pTarget = m_pGameClient->m_PredictedWorld.GetCharacterById(m_TargetId);
	if(!pTarget || !pTarget->Core())
	{
		m_Active = false;
		m_TargetId = -OI(1);
		return;
	}
	
	m_TargetPos = pTarget->Core()->m_Pos;
	m_TargetVel = pTarget->Core()->m_Vel;
	
	m_TargetAccel = m_TargetVel - m_PrevTargetVel;
	m_PrevTargetVel = m_TargetVel;
	
	UpdateHistory(m_MyPos, m_TargetPos);
	
	float Tolerance = g_Config.m_shalawaBalanceTolerance / OBF_F(10);
	bool OnHead = IsOnHead(m_MyPos, m_TargetPos, Tolerance);
	
	if(OnHead)
	{
		if(!m_Active)
		{
			m_Active = true;
			m_BalanceTicks = OI(0);
		}
		m_BalanceTicks++;
		m_WasOnHead = true;
		
		if(m_BalanceTicks > m_BalanceStreak)
			m_BalanceStreak = m_BalanceTicks;
		if(m_BalanceStreak > m_BestStreak)
			m_BestStreak = m_BalanceStreak;
	}
	else
	{
		if(m_WasOnHead)
		{
			m_RecoveryTicks = g_Config.m_shalawaBalanceRecoveryTicks;
			m_WasOnHead = false;
		}
		
		if(m_RecoveryTicks <= OI(0) && m_Active)
		{
			if(!CanReachHead(m_MyPos, m_MyVel, m_TargetPos, m_TargetVel))
			{
				m_Active = false;
				m_BalanceTicks = OI(0);
			}
		}
	}
	
	if(!m_Active && !OnHead)
	{
		if(CanReachHead(m_MyPos, m_MyVel, m_TargetPos, m_TargetVel))
			m_Active = true;
		else
			return;
	}
	
	UpdateStats(m_MyPos, m_TargetPos);
	
	int PredictTicks = g_Config.m_shalawaBalancePredictTicks;
	m_PredictedTargetPos = PredictTargetPosition(m_TargetPos, m_TargetVel, m_TargetAccel, PredictTicks);
	
	int Direction = CalculateDirection(m_MyPos, m_MyVel, m_TargetPos, m_TargetVel);
	bool Jump = ShouldJump(m_MyPos, m_MyVel, m_TargetPos, m_TargetVel);
	
	vec2 HookDir = vec2(OI(0), OI(0));
	bool Hook = false;
	if(g_Config.m_shalawaBalanceUseHook)
		Hook = ShouldHook(m_MyPos, m_MyVel, m_TargetPos, m_TargetVel, &HookDir);
	
	ApplyInput(LocalId, Direction, Jump, Hook, HookDir);
}

int CBalanceBot::FindBestTarget(vec2 MyPos, vec2 MyVel)
{
    ANTI_DISASM_BLOCK();
	int BestId = -OI(1);
	float BestScore = -OBF_F(99999);
	float SearchRadius = (float)g_Config.m_shalawaBalanceSearchRadius;
	
	for(int i = OI(0); i < MAX_CLIENTS; i++)
	{
		if(i == m_pGameClient->m_aLocalIds[OI(0)] || i == m_pGameClient->m_aLocalIds[OI(1)])
			continue;
		
		if(!m_pGameClient->m_aClients[i].m_Active)
			continue;
		
		if(!IsValidTarget(i, MyPos))
			continue;
		
		float Score = ScoreTarget(i, MyPos, MyVel);
		
		if(Score > BestScore)
		{
			BestScore = Score;
			BestId = i;
		}
	}
	
	return BestId;
}

float CBalanceBot::ScoreTarget(int ClientId, vec2 MyPos, vec2 MyVel)
{
    ANTI_DISASM_BLOCK();
	const CCharacter *pChar = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(!pChar || !pChar->Core())
		return -OBF_F(99999);
	
	vec2 Pos = pChar->Core()->m_Pos;
	vec2 Vel = pChar->Core()->m_Vel;
	
	float Score = OBF_F(0);
	
	float VertDist = Pos.y - MyPos.y;
	float HorizDist = absolute(Pos.x - MyPos.x);
	float IdealVertDist = HEAD_OFFSET_Y;
	
	float VertScore = OBF_F(100) - absolute(VertDist - IdealVertDist) * OBF_F(3);
	Score += VertScore;
	
	float HorizScore = OBF_F(50) - HorizDist * OBF_F(2);
	Score += HorizScore;
	
	float VelMatchScore = OBF_F(30) - absolute(MyVel.x - Vel.x) * OBF_F(2);
	Score += VelMatchScore;
	
	if(absolute(Vel.x) < OBF_F(2))
		Score += OBF_F(20);
	
	if(ClientId == m_TargetId)
		Score += g_Config.m_shalawaBalanceStickyTarget;
	
	if(ClientId == m_LastTargetId)
		Score += g_Config.m_shalawaBalanceStickyTarget / OBF_F(2);
	
	bool Grounded = Collision()->CheckPoint(Pos.x, Pos.y + OI(18));
	if(Grounded)
		Score += OBF_F(15);
	
	return Score;
}

bool CBalanceBot::IsValidTarget(int ClientId, vec2 MyPos)
{
    ANTI_DISASM_BLOCK();
	const CCharacter *pChar = m_pGameClient->m_PredictedWorld.GetCharacterById(ClientId);
	if(!pChar || !pChar->Core())
		return false;
	
	vec2 Pos = pChar->Core()->m_Pos;
	
	float VertDist = Pos.y - MyPos.y;
	if(VertDist < HEAD_OFFSET_Y - OBF_F(15) || VertDist > HEAD_OFFSET_Y + OBF_F(40))
		return false;
	
	float HorizDist = absolute(Pos.x - MyPos.x);
	float SearchRadius = (float)g_Config.m_shalawaBalanceSearchRadius;
	if(HorizDist > SearchRadius)
		return false;
	
	return true;
}

bool CBalanceBot::IsOnHead(vec2 MyPos, vec2 TargetPos, float Tolerance)
{
    ANTI_DISASM_BLOCK();
	float HorizDist = absolute(MyPos.x - TargetPos.x);
	float VertDist = TargetPos.y - MyPos.y;
	
	float HorizTolerance = TEE_SIZE * OFR(5,10) * Tolerance;
	float VertMin = HEAD_OFFSET_Y - OBF_F(8);
	float VertMax = HEAD_OFFSET_Y + OBF_F(12);
	
	return HorizDist < HorizTolerance && VertDist > VertMin && VertDist < VertMax;
}

bool CBalanceBot::CanReachHead(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel)
{
    ANTI_DISASM_BLOCK();
	float HorizDist = absolute(MyPos.x - TargetPos.x);
	float VertDist = TargetPos.y - MyPos.y;
	
	if(HorizDist > TEE_SIZE * OFR(25,10))
		return false;
	
	if(VertDist < HEAD_OFFSET_Y - OBF_F(20) || VertDist > HEAD_OFFSET_Y + OBF_F(50))
		return false;
	
	float RelVelX = MyVel.x - TargetVel.x;
	float TimeToReach = HorizDist / (absolute(RelVelX) + OFR(1,10));
	
	if(TimeToReach > OBF_F(30) && HorizDist > TEE_SIZE)
		return false;
	
	return true;
}

vec2 CBalanceBot::PredictTargetPosition(vec2 Pos, vec2 Vel, vec2 Accel, int Ticks)
{
    ANTI_DISASM_BLOCK();
	vec2 Predicted = Pos;
	vec2 PredVel = Vel;
	
	float Friction = OFR(5,10);
	float Gravity = OFR(5,10);
	
	for(int i = OI(0); i < Ticks; i++)
	{
		if(g_Config.m_shalawaBalanceUseAccel)
			PredVel = PredVel + Accel * OFR(3,10);
		
		PredVel.x *= Friction;
		
		bool Grounded = Collision()->CheckPoint(Predicted.x, Predicted.y + OI(18));
		if(!Grounded)
			PredVel.y += Gravity;
		else
			PredVel.y = OI(0);
		
		Predicted = Predicted + PredVel;
	}
	
	return Predicted;
}

vec2 CBalanceBot::PredictMyPosition(vec2 Pos, vec2 Vel, int Direction, bool Jump, int Ticks)
{
    ANTI_DISASM_BLOCK();
	vec2 Predicted = Pos;
	vec2 PredVel = Vel;
	
	float Accel = OBF_F(1);
	float Friction = OFR(5,10);
	float Gravity = OFR(5,10);
	float MaxSpeed = OBF_F(6);
	
	for(int i = OI(0); i < Ticks; i++)
	{
		if(Direction != OI(0))
		{
			PredVel.x += Direction * Accel;
			PredVel.x = std::clamp(PredVel.x, -MaxSpeed, MaxSpeed);
		}
		else
		{
			PredVel.x *= Friction;
		}
		
		bool Grounded = Collision()->CheckPoint(Predicted.x, Predicted.y + OI(18));
		if(Jump && i == OI(0) && Grounded)
			PredVel.y = -OBF_F(13);
		else if(!Grounded)
			PredVel.y += Gravity;
		else
			PredVel.y = OI(0);
		
		Predicted = Predicted + PredVel;
	}
	
	return Predicted;
}

float CBalanceBot::PredictOffset(int Ticks, int Direction)
{
    ANTI_DISASM_BLOCK();
	vec2 PredMy = PredictMyPosition(m_MyPos, m_MyVel, Direction, false, Ticks);
	vec2 PredTarget = PredictTargetPosition(m_TargetPos, m_TargetVel, m_TargetAccel, Ticks);
	return PredMy.x - PredTarget.x;
}

int CBalanceBot::CalculateDirection(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel)
{
    ANTI_DISASM_BLOCK();
	float CurrentOffset = MyPos.x - TargetPos.x;
	float VelDiff = MyVel.x - TargetVel.x;
	
	float Smoothing = g_Config.m_shalawaBalanceSmoothing / OBF_F(100);
	m_SmoothedOffset = m_SmoothedOffset * Smoothing + CurrentOffset * (OBF_F(1) - Smoothing);
	m_SmoothedVelDiff = m_SmoothedVelDiff * Smoothing + VelDiff * (OBF_F(1) - Smoothing);
	
	int PredictTicks = g_Config.m_shalawaBalancePredictTicks;
	float PredOffset = PredictOffset(PredictTicks, OI(0));
	
	int Dir = OI(0);
	int Mode = g_Config.m_shalawaBalanceMode;
	
	switch(Mode)
	{
	case 0:
		Dir = CalculateDirectionPassive(m_SmoothedOffset, m_SmoothedVelDiff);
		break;
	case 1:
		Dir = CalculateDirectionAggressive(m_SmoothedOffset, m_SmoothedVelDiff, PredOffset);
		break;
	case 2:
		Dir = CalculateDirectionAdaptive(m_SmoothedOffset, m_SmoothedVelDiff, PredOffset);
		break;
	case 3:
		Dir = CalculateDirectionPredictive(MyPos, MyVel, TargetPos, TargetVel);
		break;
	}
	
	float Sensitivity = g_Config.m_shalawaBalanceSensitivity / OBF_F(10);
	float DeadZone = g_Config.m_shalawaBalanceDeadZone / OBF_F(10);
	
	if(absolute(m_SmoothedOffset) < DeadZone && absolute(m_SmoothedVelDiff) < OBF_F(1))
		Dir = OI(0);
	
	int MinHoldTicks = g_Config.m_shalawaBalanceMinHoldTicks;
	if(Dir != m_LastDirection)
	{
		if(m_DirectionHoldTicks < MinHoldTicks && m_LastDirection != OI(0))
			Dir = m_LastDirection;
		else
			m_DirectionHoldTicks = OI(0);
	}
	m_DirectionHoldTicks++;
	m_LastDirection = Dir;
	
	return Dir;
}

int CBalanceBot::CalculateDirectionPassive(float Offset, float VelDiff)
{
    ANTI_DISASM_BLOCK();
	float Threshold = g_Config.m_shalawaBalanceZone / OBF_F(10);
	
	if(absolute(Offset) < Threshold)
		return OI(0);
	
	return (Offset > OBF_F(0)) ? -OI(1) : OI(1);
}

int CBalanceBot::CalculateDirectionAggressive(float Offset, float VelDiff, float PredOffset)
{
    ANTI_DISASM_BLOCK();
	float Threshold = g_Config.m_shalawaBalanceZone / OBF_F(10);
	float PanicZone = g_Config.m_shalawaBalancePanicZone / OBF_F(10);
	
	if(absolute(PredOffset) > PanicZone)
		return (PredOffset > OBF_F(0)) ? -OI(1) : OI(1);
	
	if(absolute(Offset) > Threshold)
		return (Offset > OBF_F(0)) ? -OI(1) : OI(1);
	
	if(absolute(VelDiff) > OBF_F(2))
		return (VelDiff > OBF_F(0)) ? -OI(1) : OI(1);
	
	return OI(0);
}

int CBalanceBot::CalculateDirectionAdaptive(float Offset, float VelDiff, float PredOffset)
{
    ANTI_DISASM_BLOCK();
	float Weight = m_Confidence;
	float CombinedOffset = Offset * (OBF_F(1) - Weight) + PredOffset * Weight;
	
	float Threshold = g_Config.m_shalawaBalanceZone / OBF_F(10);
	
	if(absolute(CombinedOffset) > Threshold)
		return (CombinedOffset > OBF_F(0)) ? -OI(1) : OI(1);
	
	if(absolute(VelDiff) > OBF_F(3))
		return (VelDiff > OBF_F(0)) ? -OI(1) : OI(1);
	
	return OI(0);
}

int CBalanceBot::CalculateDirectionPredictive(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel)
{
    ANTI_DISASM_BLOCK();
	int BestDir = OI(0);
	float BestScore = -OBF_F(99999);
	int SimTicks = g_Config.m_shalawaBalancePredictTicks;
	
	for(int Dir = -OI(1); Dir <= OI(1); Dir++)
	{
		vec2 PredMy = PredictMyPosition(MyPos, MyVel, Dir, false, SimTicks);
		vec2 PredTarget = PredictTargetPosition(TargetPos, TargetVel, m_TargetAccel, SimTicks);
		
		float Offset = absolute(PredMy.x - PredTarget.x);
		float VertDist = absolute((PredTarget.y - PredMy.y) - HEAD_OFFSET_Y);
		
		float Score = OBF_F(100) - Offset * OBF_F(3) - VertDist * OBF_F(2);
		
		if(Dir == OI(0))
			Score += OBF_F(5);
		
		if(Score > BestScore)
		{
			BestScore = Score;
			BestDir = Dir;
		}
	}
	
	return BestDir;
}

bool CBalanceBot::ShouldJump(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel)
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaBalanceAutoJump)
		return false;
	
	if(m_JumpCooldown > OI(0))
		return false;
	
	float VertDist = TargetPos.y - MyPos.y;
	float Offset = absolute(MyPos.x - TargetPos.x);
	
	if(MyVel.y > OBF_F(4) && Offset > TEE_SIZE * OFR(8,10))
	{
		m_JumpCooldown = g_Config.m_shalawaBalanceJumpCooldown;
		return true;
	}
	
	if(TargetVel.y < -OBF_F(8) && m_PrevTargetVel.y > -OBF_F(2))
	{
		m_JumpCooldown = g_Config.m_shalawaBalanceJumpCooldown;
		return true;
	}
	
	if(VertDist > HEAD_OFFSET_Y + OBF_F(15) && MyVel.y > OBF_F(0))
	{
		m_JumpCooldown = g_Config.m_shalawaBalanceJumpCooldown;
		return true;
	}
	
	if(m_PanicLevel > OFR(7,10) && !IsOnHead(MyPos, TargetPos, OBF_F(2)))
	{
		m_JumpCooldown = g_Config.m_shalawaBalanceJumpCooldown;
		return true;
	}
	
	return false;
}

bool CBalanceBot::ShouldHook(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel, vec2 *pHookDir)
{
    ANTI_DISASM_BLOCK();
	if(m_HookCooldown > OI(0))
		return false;
	
	float Offset = absolute(MyPos.x - TargetPos.x);
	float VertDist = TargetPos.y - MyPos.y;
	
	if(Offset > TEE_SIZE * OFR(15,10) && MyVel.y > OBF_F(2))
	{
		vec2 Dir = TargetPos - MyPos;
		float Len = length(Dir);
		if(Len > OFR(1,100))
		{
			*pHookDir = Dir / Len;
			m_HookCooldown = g_Config.m_shalawaBalanceHookCooldown;
			return true;
		}
	}
	
	if(m_PanicLevel > OFR(8,10) && VertDist > HEAD_OFFSET_Y + OBF_F(20))
	{
		*pHookDir = vec2(OI(0), OI(1));
		m_HookCooldown = g_Config.m_shalawaBalanceHookCooldown;
		return true;
	}
	
	return false;
}

bool CBalanceBot::ShouldPanic()
{
    ANTI_DISASM_BLOCK();
	return m_PanicLevel > OFR(6,10);
}

void CBalanceBot::UpdateHistory(vec2 MyPos, vec2 TargetPos)
{
    ANTI_DISASM_BLOCK();
	m_TargetHistory.push_back(TargetPos);
	m_MyHistory.push_back(MyPos);
	
	while((int)m_TargetHistory.size() > HISTORY_SIZE)
		m_TargetHistory.erase(m_TargetHistory.begin());
	while((int)m_MyHistory.size() > HISTORY_SIZE)
		m_MyHistory.erase(m_MyHistory.begin());
}

void CBalanceBot::UpdateStats(vec2 MyPos, vec2 TargetPos)
{
    ANTI_DISASM_BLOCK();
	float Offset = absolute(MyPos.x - TargetPos.x);
	float IdealVert = HEAD_OFFSET_Y;
	float VertError = absolute((TargetPos.y - MyPos.y) - IdealVert);
	
	float Quality = OBF_F(1) - (Offset / TEE_SIZE) - (VertError / OBF_F(30));
	Quality = std::clamp(Quality, OBF_F(0), OBF_F(1));
	m_BalanceQuality = m_BalanceQuality * OFR(9,10) + Quality * OFR(1,10);
	
	float Stability = CalculateStability();
	m_Confidence = m_Confidence * OFR(95,100) + Stability * OFR(5,100);
	m_Confidence = std::clamp(m_Confidence, OFR(2,10), OBF_F(1));
	
	float PanicThreshold = g_Config.m_shalawaBalancePanicZone / OBF_F(10);
	if(Offset > PanicThreshold)
		m_PanicLevel = std::min(OBF_F(1), m_PanicLevel + OFR(1,10));
	else
		m_PanicLevel = std::max(OBF_F(0), m_PanicLevel - OFR(5,100));
	
	if(Quality > OFR(8,10))
		m_StableCounter++;
	else
		m_StableCounter = OI(0);
}

vec2 CBalanceBot::AnalyzeTargetPattern()
{
    ANTI_DISASM_BLOCK();
	if(m_TargetHistory.size() < OI(10))
		return vec2(OI(0), OI(0));
	
	vec2 AvgVel = vec2(OI(0), OI(0));
	int Count = OI(0);
	
	for(size_t i = OI(1); i < m_TargetHistory.size(); i++)
	{
		AvgVel = AvgVel + (m_TargetHistory[i] - m_TargetHistory[i-OI(1)]);
		Count++;
	}
	
	if(Count > OI(0))
		AvgVel = AvgVel / (float)Count;
	
	return AvgVel;
}

float CBalanceBot::CalculateStability()
{
    ANTI_DISASM_BLOCK();
	if(m_MyHistory.size() < OI(5))
		return OFR(5,10);
	
	float TotalVariance = OBF_F(0);
	
	for(size_t i = OI(1); i < m_MyHistory.size() && i < m_TargetHistory.size(); i++)
	{
		float Offset1 = m_MyHistory[i-OI(1)].x - m_TargetHistory[i-OI(1)].x;
		float Offset2 = m_MyHistory[i].x - m_TargetHistory[i].x;
		TotalVariance += absolute(Offset2 - Offset1);
	}
	
	float AvgVariance = TotalVariance / (float)m_MyHistory.size();
	float Stability = OBF_F(1) - std::min(OBF_F(1), AvgVariance / OBF_F(10));
	
	return Stability;
}

void CBalanceBot::ApplyInput(int LocalId, int Direction, bool Jump, bool Hook, vec2 HookDir)
{
    ANTI_DISASM_BLOCK();
	CNetObj_PlayerInput *pInput = &m_pControls->m_aInputData[LocalId];
	
	pInput->m_Direction = Direction;
	
	if(Jump)
		pInput->m_Jump = OI(1);
	
	if(Hook && length(HookDir) > OFR(1,100))
	{
		pInput->m_Hook = OI(1);
		pInput->m_TargetX = (int)(HookDir.x * OBF_F(100));
		pInput->m_TargetY = (int)(HookDir.y * OBF_F(100));
	}
}

void CBalanceBot::OnRender()
{
    ANTI_DISASM_BLOCK();
	if(!g_Config.m_shalawaBalanceBot || !g_Config.m_shalawaBalanceVisuals)
		return;
	
	if(!m_Active || m_TargetId < OI(0))
		return;
}

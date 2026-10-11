#ifndef SHALAWA_MOVEMENT_BALANCE_H
#define SHALAWA_MOVEMENT_BALANCE_H

#include <base/vmath.h>
#include <vector>

class CGameClient;
class CControls;
class CCollision;
class CCharacter;

class CBalanceBot
{
public:
	CBalanceBot();
	void Init(CControls *pControls, CGameClient *pGameClient);
	void Reset();
	void Tick(int LocalId);
	void OnRender();
	
	bool IsActive() const { return m_Active; }
	int GetTargetId() const { return m_TargetId; }
	vec2 GetTargetPos() const { return m_TargetPos; }
	vec2 GetPredictedPos() const { return m_PredictedTargetPos; }
	float GetBalanceQuality() const { return m_BalanceQuality; }
	int GetBalanceStreak() const { return m_BalanceStreak; }

private:
	CControls *m_pControls;
	CGameClient *m_pGameClient;
	
	bool m_Active;
	int m_TargetId;
	int m_LastTargetId;
	vec2 m_TargetPos;
	vec2 m_TargetVel;
	vec2 m_TargetAccel;
	vec2 m_PrevTargetVel;
	vec2 m_PredictedTargetPos;
	vec2 m_MyPos;
	vec2 m_MyVel;
	
	int m_BalanceTicks;
	int m_BalanceStreak;
	int m_BestStreak;
	float m_BalanceQuality;
	float m_SmoothedOffset;
	float m_SmoothedVelDiff;
	int m_LastDirection;
	int m_DirectionHoldTicks;
	int m_NoTargetTicks;
	int m_JumpCooldown;
	int m_HookCooldown;
	bool m_WasOnHead;
	
	float m_PanicLevel;
	float m_Confidence;
	int m_RecoveryTicks;
	int m_StableCounter;
	
	std::vector<vec2> m_TargetHistory;
	std::vector<vec2> m_MyHistory;
	
	static constexpr float TEE_SIZE = 28.0f;
	static constexpr float HEAD_OFFSET_Y = 24.0f;
	static constexpr int HISTORY_SIZE = 32;
	
	CCollision *Collision();
	
	int FindBestTarget(vec2 MyPos, vec2 MyVel);
	float ScoreTarget(int ClientId, vec2 MyPos, vec2 MyVel);
	bool IsValidTarget(int ClientId, vec2 MyPos);
	bool IsOnHead(vec2 MyPos, vec2 TargetPos, float Tolerance);
	bool CanReachHead(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel);
	
	vec2 PredictTargetPosition(vec2 Pos, vec2 Vel, vec2 Accel, int Ticks);
	vec2 PredictMyPosition(vec2 Pos, vec2 Vel, int Direction, bool Jump, int Ticks);
	float PredictOffset(int Ticks, int Direction);
	
	int CalculateDirection(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel);
	int CalculateDirectionAggressive(float Offset, float VelDiff, float PredOffset);
	int CalculateDirectionPassive(float Offset, float VelDiff);
	int CalculateDirectionAdaptive(float Offset, float VelDiff, float PredOffset);
	int CalculateDirectionPredictive(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel);
	
	bool ShouldJump(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel);
	bool ShouldHook(vec2 MyPos, vec2 MyVel, vec2 TargetPos, vec2 TargetVel, vec2 *pHookDir);
	bool ShouldPanic();
	
	void UpdateHistory(vec2 MyPos, vec2 TargetPos);
	void UpdateStats(vec2 MyPos, vec2 TargetPos);
	vec2 AnalyzeTargetPattern();
	float CalculateStability();
	
	void ApplyInput(int LocalId, int Direction, bool Jump, bool Hook, vec2 HookDir);
};

#endif
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_CONTROLS_H

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>

#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/components/shalawa/avoid/blatant.h>
#include <game/client/components/shalawa/movement/balance.h>
#include <game/client/components/shalawa/movement/hookride.h>
#include <game/client/components/shalawa/movement/unfreeze.h>
#include <game/client/components/shalawa/movement/quickstop.h>
#include <game/client/components/shalawa/aimbot/aimbot.h>
#include <game/client/components/shalawa/misc/fake_aim.h>

#include <string>
#include <cstdint>
#include <deque>
#include <vector>
#include <chrono>
#include <algorithm>
#include <map>

struct SAvoidSettings
{
	bool m_EnableAvoidFreeze = true;
	bool m_EnableHookAssist = true;
	int m_AvoidDelayMs = 180;
	int m_AvoidTicks = 1;
	int m_PredictTicks = 4;
	int m_HookDuration = 3;
	int m_HookTicks = 4;
	int m_HookAssistSensitivity = 10;
	int m_AvoidSensitivity = 10;
	int m_AfkProtectionMs = 0;
	int m_UnfreezeCheckRadius = 5;
	bool m_AvoidTeles = false;
	bool m_AvoidDeath = false;
	int m_AvoidFreezeBind = 0;
	int m_HookAssistBind = 0;
	int m_AvoidToggleBind = 0;
};

struct SAvoidVisualData
{
    bool m_Active;
    int m_Direction;
    vec2 m_DangerPos;
    float m_DangerDistance;
    int m_DangerTick;
    int64_t m_ActivationTime;
    bool m_HookReleased;
    vec2 m_PlayerPos;
    vec2 m_PlayerVel;
};

// Forward declaration
class CCharacterCore;

// TAS Character History structure (declared before class for use in function signatures)
struct STasCharacterHistory
{
	int m_Id = -1;
	std::vector<vec2> m_Positions;
	std::vector<vec2> m_PrevPositions;
	std::vector<int> m_FreezeTime;
	std::vector<int> m_ReloadTimer;
	// Note: CCharacterCore will be fully defined when this is used
	std::vector<CCharacterCore> m_Cores;
};

struct STasAutoState
{
	bool m_Running = false;
	int m_LastPlanTick = -100000;
	int m_LastAppliedTick = -100000;
	int m_LastJumpTick = -100000;
	int m_LastPlanMs = 0;
	int m_PlanIndex = 0;
	vec2 m_LastFinishPos = vec2(0.0f, 0.0f);
	bool m_HasFinish = false;
	bool m_TilesCached = false;
	std::vector<vec2> m_FinishTiles;
	std::vector<int> m_FinishTileIndices;
	std::vector<vec2> m_HookTiles;
	std::vector<CNetObj_PlayerInput> m_Plan;
	std::vector<vec2> m_PathPositions;
	std::vector<vec2> m_PathTiles;
	std::vector<int> m_PathTileIndices;
	bool m_FieldReady = false;
	int m_FieldWidth = 0;
	int m_FieldHeight = 0;
	std::vector<int8_t> m_FieldDirX;
	std::vector<int8_t> m_FieldDirY;
};

class CControls : public CComponent
{
public:
	float GetMinMouseDistance() const;
	float GetMaxMouseDistance() const;

	enum class EMouseInputType
	{
		ABSOLUTE,
		RELATIVE,
		AUTOMATED,
	};

	vec2 m_aMousePos[NUM_DUMMIES];
	vec2 m_aMousePosOnAction[NUM_DUMMIES];
	vec2 m_aTargetPos[NUM_DUMMIES];
	CBlatantAvoid m_BlatantAvoid;

	EMouseInputType m_aMouseInputType[NUM_DUMMIES];

	int m_aAmmoCount[NUM_WEAPONS];

	int64_t m_LastSendTime;
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];
	CNetObj_PlayerInput m_aLastData[NUM_DUMMIES];
	int m_aInputDirectionLeft[NUM_DUMMIES];
	int m_aInputDirectionRight[NUM_DUMMIES];
	int m_aShowHookColl[NUM_DUMMIES];

	CNetObj_PlayerInput m_FastInput;

	CControls();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	void OnConsoleInit() override;
	virtual void OnPlayerDeath();

	// Avoid logic
	void OnUpdate() override; // Override to add avoid logic

	// TAS functions
	void TasUpdate(); // Added for TAS interpolation
	void TasInit();
	void TasRecordTick();
	void TasStepBack();
	void TasPlayTick();
	void TasPlaybackReset();
	void TasPlaybackEnsurePrepared();
	void DetectTasRollback();
	void HandleTasTeleport(CCharacter *pChar);
	void ApplyTasDummyHammer(CCharacter *pDummyChar, CGameWorld *pWorld);
	const STasCharacterHistory *TasFindCharHistory(int Id) const;
	STasCharacterHistory *TasFindCharHistory(int Id);
	void TasAutoReset();
	bool TasAutoApply(int Dummy);
	
	void SaveRecordsToFile(const std::string &Filename);
	void LoadRecordsFromFile(const std::string &Filename);
	void DeleteRecord(const std::string &Filename);
	void SaveRecord();
	void ClearTasRecords();
	void RenderTasLoadDropdown(int *pCurrentValue);
	int GetTasTick() const;
	void RenderTasWorld();
	void RenderTasFreezeBars();

	vec2 GetInterpolatedMouseForRendering(int Dummy);
	vec2 GetInterpolatedPlayerPosition(int Dummy); // Get interpolated player position for camera
	int SnapInput(int *pData);
	void ClampMousePos();
	void ResetInput(int Dummy);
	bool CheckNewInput();

private:
	void SyncDummyInput();
	CBalanceBot m_BalanceBot;
	CAimbot m_Aimbot;
	CQuickStop m_QuickStop;
	CHookRide m_HookRide;
	CUnfreezeBot m_UnfreezeBot;
	void ClPilotBot(int Dummy);

	SAvoidVisualData m_aAvoidVisuals[NUM_DUMMIES];

	void RenderLegitAvoidVisuals();
	void UpdateAvoidVisuals(int Local, bool Activated, int Direction, vec2 DangerPos, float DangerDist, int DangerTick, bool HookReleased);

	// Pilot bot state (client-side only).
	bool m_PilotHasFinish = false;
	vec2 m_PilotFinishPos = vec2(0.0f, 0.0f);
	int m_PilotLastTick = -1;
	int m_PilotHookTicksLeft = 0;

	int m_PilotLastRescanTick = -100000;
	int m_PilotHookCooldownTicks = 0;
	int m_PilotHookAttemptTicks = 0;
	int m_PilotHookGrabTicks = 0;
	vec2 m_PilotHookGrabStartPos = vec2(0.0f, 0.0f);

	// TAS variables (nones-twave TAS ported)
	CGameWorld *m_pTasWorld = nullptr;
	CGameWorld *m_pTasWorldPredicted = nullptr;
	int64_t m_TasLastUpdateTime = 0;
	int64_t m_TasLastFrameTime = 0;
	double m_TasAccumulator = 0.0;
	int64_t m_TasRecordingStartTime = 0;
	int m_TasRecordingStartTick = 0;
	int m_TasJumpCount = 0;
	int m_TasPrevJump = 0;
	int m_TasPlayLastGameTick = -1;
	int64_t m_TasManualStepLastTime = 0;
	int m_TasLastAttackTick = -100000;
	bool m_aAutoFiring[NUM_DUMMIES] = {false, false};
	int m_aTasLastAttackTick[NUM_DUMMIES] = {-100000, -100000};
	double m_aTasLastAttackTime[NUM_DUMMIES] = {-100000.0, -100000.0};
	std::vector<int> m_vTasAttackTick;
	int m_TasClientTickBase = 0;
	int m_TasPrevFireForAttack = 0;
	int m_aTasPrevFireForAttack[NUM_DUMMIES] = {0, 0};
	int m_TasLastObservedTasAttackTick = -1;
	int m_aTasLastObservedTasAttackTick[NUM_DUMMIES] = {-1, -1};
	vec2 m_aTasLastTickMouse[NUM_DUMMIES] = {};
	vec2 m_aTasNextTickMouse[NUM_DUMMIES] = {};
	vec2 m_aTasInterpMouse[NUM_DUMMIES] = {};
	double m_TasLastTickTime = 0.0;
	double m_TasNextTickTime = 0.0;
	float m_TasSmoothIntra = 0.0f;
	std::vector<int> m_vTasTicks;
	std::vector<CNetObj_PlayerInput> m_vTasActions;
	std::vector<vec2> m_vTasMouse;
	std::vector<int> m_vTasDummySwitch;
	std::vector<int> m_vTasKill;
	std::vector<STasCharacterHistory> m_vTasChars;
	std::vector<vec2> m_vTasPositions;
	
	// Playback state
	bool m_TasPlaybackPrepared = false;
	int m_TasPlayIndex = 0;
	CNetObj_PlayerInput m_TasPlayCachedInput[NUM_DUMMIES] = {};
	vec2 m_TasPlayCachedMouse[NUM_DUMMIES] = {};
	int m_TasPlayCachedKill[NUM_DUMMIES] = {};
	
	// TAS desync detection
	int m_TasDesyncCount = 0;
	int m_TasLastSyncIndex = -1;
	vec2 m_TasLastSyncPos = vec2(0.0f, 0.0f);
	int m_TasDesyncWaitTicks = 0;

	// TAS Auto state
	STasAutoState m_TasAuto;

	// Avoid logic
	int64_t m_LastUserActivity = 0;
	vec2 m_aLastMousePos[NUM_DUMMIES];

	// Quick Stop variables
	int m_aQuickStopTicks[2] = {0, 0}; // Counter for how many ticks to hold opposite direction
	bool m_aLastDirectionLeft[2] = {false, false};
	bool m_aLastDirectionRight[2] = {false, false};



	// Auto Unfreeze state
	int m_aAutoUnfreezeFireTicks[NUM_DUMMIES] = {0, 0};
	bool m_aAutoUnfreezeWasActive[NUM_DUMMIES] = {false, false};
	
	// Auto Fire state
	int m_aAutoFireLastTick[NUM_DUMMIES] = {-1, -1};
	
	// Auto Jump Save state
	int m_aAutoJumpSaveFireTicks[NUM_DUMMIES] = {0, 0};
	
	// Auto Aled state
	int m_aAutoAledFireTicks[NUM_DUMMIES] = {0, 0};
	
	// Auto Unfreeze debug data
	struct SAutoUnfreezeDebug
	{
		vec2 m_StartPos;
		vec2 m_ShootDirection;
		vec2 m_PredictedHitPos;
		bool m_WasFrozenBefore;
		int m_CheckTicks;
		vec2 m_LastLaserPos; // Last known laser position
		bool m_Valid = false;
	};
	SAutoUnfreezeDebug m_aAutoUnfreezeDebug[NUM_DUMMIES];

	void AutoUnfreeze();
	void AutoUnfreezeAim(int Local, vec2 Pos);
	vec2 FindUnfreezeTarget(vec2 MyPos, vec2 MyVel, CCollision *pCollision);
	void AutoJumpSave();
	void AutoFire();
	void AutoAled();
	bool AutomationAllowed() const;
	bool IsPlayerActive(int LocalPlayerId);
	bool IsMouseMoved(int LocalPlayerId);
	void BalanceBot();
	void JetRide();
	void HookRide();
	void AutoVote();
	void AutoKick();
	void ProfileStealer();
	void EmoteSpam();
	void ChatSpam();

public:
	void AIBotRecalculate();
	void AIBotClearAllTiles();
	void AIBotUpdate();
	void AIBotHelperMode(); // Helper mode - only assists, doesn't play
	void AIBotRender();
	void RenderTrajectory();

	// Performance metrics (public for HUD access)
	float m_AIBotCurrentSpeed = 0.0f;
	int m_AIBotHooksPerSecond = 0;
	float m_AIBotDistanceToFinish = 0.0f;
	bool m_AIBotInFreeze = false;
	int m_AIBotFallTimePercent = 0;
	int m_AIBotDeathCount = 0;
	vec2 m_AIBotLastPos = vec2(0.0f, 0.0f);
	
	// AI Bot path (public for rendering)
	std::vector<vec2> m_aAIBotPath; // A* path to finish
	bool m_AIBotHasFinish = false; // Public: whether finish is found and path is active
	
	// Bot log file
	void AIBotLog(const char *pMsg);
	IOHANDLE m_BotLogFile = nullptr;

	// Jump counter and navigation system
	bool AIBotCanJump(const CCharacter *pChar);
	bool AIBotIsPathBlocked(vec2 From, vec2 To);
	vec2 AIBotFindDetourDirection(vec2 CurrentPos, vec2 TargetPos, vec2 CurrentVel);
	bool AIBotIsStuck(vec2 CurrentPos, vec2 CurrentVel, int64_t Now);
	vec2 AIBotScanForExit(vec2 CurrentPos, vec2 TargetPos);

	// Pre-simulation intelligence system
	struct SVirtualCharacter
	{
		vec2 m_Pos;
		vec2 m_Vel;
		int m_JumpsLeft;
		bool m_HookActive;
		vec2 m_HookPos;
		int m_HookTicksLeft;
		bool m_InFreeze;
		int m_SimulationTick;

		SVirtualCharacter() : m_JumpsLeft(0), m_HookActive(false), m_HookTicksLeft(0), m_InFreeze(false), m_SimulationTick(0) {}
	};

	struct SActionVariant
	{
		std::string m_Name;
		float m_Score;
		float m_TimeToComplete;
		float m_RiskLevel;
		bool m_Successful;
		vec2 m_FinalPos;
		vec2 m_FinalVel;
		int m_JumpsUsed;
	};

	// Physics simulation
	void AIBotSimulatePhysicsTick(SVirtualCharacter *pChar, const vec2 &TargetPos);
	bool AIBotCheckTileAtPos(vec2 Pos, int TileType);
	float AIBotEvaluateVariant(const SActionVariant &Variant, vec2 TargetPos);

	// Action generation and simulation
	std::vector<SActionVariant> AIBotGenerateActionVariants(vec2 CurrentPos, vec2 CurrentVel, vec2 TargetPos, int AvailableJumps);
	SActionVariant AIBotSimulateActionVariant(const std::string &Action, vec2 StartPos, vec2 StartVel, vec2 TargetPos, int StartJumps);

	// Decision making
	SActionVariant AIBotChooseBestAction(const std::vector<SActionVariant> &Variants);
	void AIBotExecuteAction(const SActionVariant &Action);

private:
	// AI Bot structures
	struct SAIBotAction
	{
		int m_Direction; // -1, 0, 1
		bool m_Jump;
		bool m_Hook; // true = press, false = release
		vec2 m_HookTarget; // hook direction (normalized)
		vec2 m_AimDirection; // aim direction (normalized) - where to point cursor
	};

	struct SAIBotActionSequence
	{
		std::vector<SAIBotAction> m_Actions;
		float m_Score = 0.0f;
		vec2 m_FinalPos = vec2(0.0f, 0.0f);
		vec2 m_FinalVel = vec2(0.0f, 0.0f);
		bool m_HitFreeze = false;
		bool m_HitWall = false;
		bool m_HitDeath = false;
		float m_DistanceToFinish = 1e30f;
		float m_DistanceTraveled = 0.0f;
		float m_AverageSpeed = 0.0f;
		bool m_UsedHook = false;
	};

	struct SAIBotPathNode
	{
		vec2 m_Pos;
		float m_G = 0.0f; // cost from start
		float m_H = 0.0f; // heuristic to finish
		float m_F = 0.0f; // total cost (G + H)
		SAIBotPathNode *m_pParent = nullptr;
	};

	struct SAIBotFinishTile
	{
		vec2 m_Pos;
		float m_Distance = 0.0f;
		int m_Accessibility = 0; // 0 = easy, higher = harder
	};

	// AI Bot state
	std::vector<SAIBotActionSequence> m_aAIBotCandidates;
	std::vector<SAIBotFinishTile> m_aAIBotFinishes;
	vec2 m_AIBotTargetFinish = vec2(0.0f, 0.0f);
	// m_AIBotHasFinish is declared in public section above
	int64_t m_AIBotLastPathRecalc = 0;
	int64_t m_AIBotLastProgressCheck = 0;
	int m_AIBotLastSimulationTick = 0; // Track when we last did full simulation
	vec2 m_AIBotLastProgressPos = vec2(0.0f, 0.0f);
	std::vector<std::vector<vec2>> m_aAIBotRenderedPaths; // For visualization - vector of paths
	std::vector<ColorRGBA> m_aAIBotRenderedPathColors; // For visualization
	float m_AIBotCurrentMouseAngle = 0.0f; // Current mouse angle in radians for smooth 360° rotation
	
	// Emergency stop system
	int m_AIBotSameActionCounter = 0;
	int m_AIBotLastDirection = 0;
	int m_AIBotLastJump = 0;
	int m_AIBotLastHook = 0;
	int64_t m_AIBotLastPosCheck = 0;

	// AI Bot helper functions
	void AIBotScanEnvironment(vec2 Pos, float Radius);
	void AIBotGenerateCandidates(int PopulationSize, int SequenceLength);
	float AIBotEvaluateSequence(const SAIBotActionSequence &Sequence, CGameWorld *pWorld, int ClientId, vec2 StartPos, vec2 StartVel);
	SAIBotActionSequence AIBotSimulateSequence(const std::vector<SAIBotAction> &Actions, CGameWorld *pWorld, int ClientId, vec2 StartPos, vec2 StartVel, int Depth);
	void AIBotFindFinishes(vec2 Pos, float Radius);
	void AIBotPathfindingAStar(vec2 Start, vec2 Finish);
	void AIBotSimulatePathToFinish(vec2 Start, vec2 Finish);
	float AIBotHeuristic(vec2 A, vec2 B);
	bool AIBotIsWalkable(vec2 Pos);
	void AIBotCheckProgress();
	void AIBotRenderPath();
	static constexpr int64_t AVOID_FORCE_MS = 100;
	static constexpr int AVOID_COOLDOWN_MS = 180;
	static constexpr int MAX_AVOID_ATTEMPTS = 3;

	int64_t m_aAvoidForceUntil[NUM_DUMMIES] = {0, 0};
	int m_aAvoidForcedDir[NUM_DUMMIES] = {0, 0};
	int64_t m_AvoidCooldownUntil = 0;
	bool m_aAvoidWasInDanger[NUM_DUMMIES] = {false, false};
	bool m_aAvoidForcing[NUM_DUMMIES] = {false, false};
	bool m_LastShowFpsState = false;
	bool m_LastVKeyState = false;
	void AvoidFreeze(const SAvoidSettings &Settings);
	void HookAssist(const SAvoidSettings &Settings);
	bool PredictFreeze(const CNetObj_PlayerInput &Input, int Ticks, int *pDangerTick, float *pDangerDistance) const;
	bool GetFreeze(vec2 Pos, int FreezeTime, const SAvoidSettings &Settings) const;
	bool TryAvoidFreeze(int LocalPlayerId);
	bool TryMove(const CNetObj_PlayerInput &BaseInput, int Direction, int CheckTicks);
	bool IsAvoidCooldownElapsed(int64_t CurrentTime, int CooldownMs) const;
	void UpdateAvoidCooldown(int64_t CurrentTime);
	int DirectionSensitivity(const SAvoidSettings &Settings) const;
	int MaxAvoidAttempts(const SAvoidSettings &Settings) const;
	const SAvoidSettings &ActiveAvoidSettings() const;
	void AIBotRenderPathfinding();
	bool AIBotIsPathBlocked(vec2 Start, vec2 End, int NumChecks);
	vec2 AIBotFindDetourDirection(vec2 Start, vec2 Finish);
	bool AIBotIsInDeadEnd(vec2 Pos);
	vec2 AIBotFindEscapeFromDeadEnd(vec2 Pos);
	bool AIBotIsWallAt(vec2 Pos);
	bool AIBotIsCeilingAt(vec2 Pos);
	bool AIBotIsDeathTileAt(vec2 Pos);
	bool AIBotIsFreezeTileAt(vec2 Pos);
	bool AIBotIsSafeToMoveInDirection(vec2 Pos, int Direction);
	bool AIBotCheckDeathAhead(vec2 Pos, int Direction);
	bool AIBotCheckFreezeNearby(vec2 Pos, float Radius);
	bool AIBotCheckWallCollision(vec2 Pos, int Direction);
	bool AIBotCheckCliffAhead(vec2 Pos, int Direction);
	void AIBotEmergencyStop();
	void AIBotCheckStuck(vec2 CurrentPos);
	bool AIBotIsTileHookable(vec2 Pos);
	vec2 AIBotFindHookTarget(vec2 FromPos, vec2 Direction, float MaxDistance);
	vec2 AIBotGetBestHookDirection(vec2 CurrentPos, vec2 FinishPos, bool HasFinish);
	float AIBotCalculateHookScore(vec2 HookDir, vec2 ToFinish, bool HasFinish);
	
	// Hook fail-safe
	int m_AIBotHookFailCount = 0;
	int64_t m_AIBotLastHookTime = 0;
	int64_t m_AIBotHookCooldownUntil = 0;
	int m_AIBotHookHoldTicks = 0; // How many ticks to hold hook
	int m_AIBotHookGoal = 0; // Hook goal: 0=none, 1=speed, 2=gap, 3=climb, 4=rescue, 5=redirect
	vec2 m_AIBotHookTargetPos = vec2(0.0f, 0.0f); // Target position for hook
	float m_AIBotHookStartSpeed = 0.0f; // Speed when hook started
	
	// Safety simulation
	bool AIBotSimulateTrajectory(vec2 StartPos, vec2 StartVel, bool UseDoubleJump, int MaxTicks, vec2 *pLandingPos, bool *pIsSafe);

	static void ConKeyInputState(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputCounter(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputSet(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyAimFire(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaAvoidFreeze(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaAvoidToggle(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaHookAssist(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaAim(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaTasRecord(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaTasPause(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaTasRewind(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaTasLoad(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaTasInit(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaHookRide(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaBalanceBot(IConsole::IResult *pResult, void *pUserData);
	static void ConShalawaUnfreezeBot(IConsole::IResult *pResult, void *pUserData);
	static void ConKeyInputNextPrevWeapon(IConsole::IResult *pResult, void *pUserData);
};
#endif

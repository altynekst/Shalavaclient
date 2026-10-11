// TAS functions copied from original TClient project
#include "controls.h"
#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>
#include <base/color.h>
#include <base/str.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/animstate.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <game/gamecore.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/graphics.h>
#include <generated/protocol.h>
#include <algorithm>
#include <limits>
#include <random>
#include <unordered_set>
#include <queue>

namespace
{
	struct STasAutoSettings
	{
		int m_ReplanTicks = 8;
		int m_PlanTicks = 40;
		int m_Population = 48;
		int m_TopK = 8;
		int m_Generations = 4;
		int m_MutationRate = 18;
		int m_HookBias = 35;
		int m_SafetyWeight = 70;
		int m_SpeedWeight = 60;
		int m_ProgressWeight = 80;
	};

	struct STasAutoAction
	{
		int m_Direction = 0;
		int m_Jump = 0;
		int m_Hook = 0;
	};

	struct STasAutoCandidate
	{
		std::vector<STasAutoAction> m_Actions;
		std::vector<CNetObj_PlayerInput> m_Inputs;
		float m_Score = -std::numeric_limits<float>::infinity();
		int m_FinishTick = -1;
		vec2 m_EndPos = vec2(0.0f, 0.0f);
		bool m_Finished = false;
	};

	static STasAutoSettings GetAutoSettings()
	{
		STasAutoSettings Settings;
		Settings.m_ReplanTicks = std::clamp(g_Config.m_shalawaTasAutoReplanTicks, 1, 50);
		Settings.m_PlanTicks = std::clamp(g_Config.m_shalawaTasAutoPlanTicks, 5, 200);
		Settings.m_Population = std::clamp(g_Config.m_shalawaTasAutoPopulation, 4, 256);
		Settings.m_TopK = std::clamp(g_Config.m_shalawaTasAutoTopK, 1, 64);
		Settings.m_Generations = std::clamp(g_Config.m_shalawaTasAutoGenerations, 1, 16);
		Settings.m_MutationRate = std::clamp(g_Config.m_shalawaTasAutoMutation, 0, 100);
		Settings.m_HookBias = std::clamp(g_Config.m_shalawaTasAutoHookBias, 0, 100);
		Settings.m_SafetyWeight = std::clamp(g_Config.m_shalawaTasAutoSafetyWeight, 0, 200);
		Settings.m_SpeedWeight = std::clamp(g_Config.m_shalawaTasAutoSpeedWeight, 0, 200);
		Settings.m_ProgressWeight = std::clamp(g_Config.m_shalawaTasAutoProgressWeight, 0, 200);
		const int MaxSimSteps = 60000;
		const int MaxPopulation = std::max(4, MaxSimSteps / std::max(1, Settings.m_PlanTicks * Settings.m_Generations));
		Settings.m_Population = std::min(Settings.m_Population, MaxPopulation);
		Settings.m_TopK = std::min(Settings.m_TopK, Settings.m_Population);
		return Settings;
	}

	static bool IsFreezeTileIndex(int Tile)
	{
		return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE;
	}

	static bool IsFinishTileIndex(int Tile)
	{
		return Tile == TILE_FINISH;
	}

	static bool IsDeathTileIndex(int Tile)
	{
		return Tile == TILE_DEATH;
	}

	static bool IsBlockedTile(const CCollision *pCollision, int Index)
	{
		if(!pCollision)
			return true;
		const int Tile = pCollision->GetTileIndex(Index);
		const int FrontTile = pCollision->GetFrontTileIndex(Index);
		if(Tile == TILE_SOLID)
			return true;
		if(IsDeathTileIndex(Tile) || IsDeathTileIndex(FrontTile))
			return true;
		if(IsFreezeTileIndex(Tile) || IsFreezeTileIndex(FrontTile))
			return true;
		return false;
	}

	static bool IsFreezeAtPos(const CCollision *pCollision, vec2 Pos)
	{
		if(!pCollision)
			return false;
		const int Index = pCollision->GetPureMapIndex(Pos);
		const int Tile = pCollision->GetTileIndex(Index);
		const int FrontTile = pCollision->GetFrontTileIndex(Index);
		return IsFreezeTileIndex(Tile) || IsFreezeTileIndex(FrontTile);
	}

	static bool IsFinishAtPos(const CCollision *pCollision, vec2 Pos)
	{
		if(!pCollision)
			return false;
		const int Index = pCollision->GetPureMapIndex(Pos);
		const int Tile = pCollision->GetTileIndex(Index);
		const int FrontTile = pCollision->GetFrontTileIndex(Index);
		return IsFinishTileIndex(Tile) || IsFinishTileIndex(FrontTile);
	}

	static bool IsDeathAtPos(const CCollision *pCollision, vec2 Pos)
	{
		if(!pCollision)
			return false;
		const int Index = pCollision->GetPureMapIndex(Pos);
		const int Tile = pCollision->GetTileIndex(Index);
		const int FrontTile = pCollision->GetFrontTileIndex(Index);
		return IsDeathTileIndex(Tile) || IsDeathTileIndex(FrontTile);
	}

	static float SafetyScore(const CCollision *pCollision, vec2 Pos)
	{
		if(!pCollision)
			return 0.0f;
		float Danger = 0.0f;
		const float Step = 24.0f;
		for(int dx = -1; dx <= 1; dx++)
		{
			for(int dy = -1; dy <= 1; dy++)
			{
				vec2 CheckPos = Pos + vec2(dx, dy) * Step;
				if(IsFreezeAtPos(pCollision, CheckPos) || IsDeathAtPos(pCollision, CheckPos))
					Danger += 1.0f;
			}
		}
		return 1.0f - std::clamp(Danger / 9.0f, 0.0f, 1.0f);
	}

	static vec2 PickNearestFinish(const std::vector<vec2> &FinishTiles, vec2 Pos, bool &HasFinish)
	{
		HasFinish = false;
		float BestDist = std::numeric_limits<float>::max();
		vec2 Best = vec2(0.0f, 0.0f);
		for(const auto &FinishPos : FinishTiles)
		{
			float Dist = distance(Pos, FinishPos);
			if(Dist < BestDist)
			{
				BestDist = Dist;
				Best = FinishPos;
				HasFinish = true;
			}
		}
		return Best;
	}

	static bool PickHookTarget(const CCollision *pCollision, const std::vector<vec2> &HookTiles, vec2 Pos, vec2 FinishPos, float HookLength, vec2 &OutTarget, bool PreferUp = false)
	{
		if(!pCollision)
			return false;
		float BestScore = -std::numeric_limits<float>::infinity();
		vec2 Best = vec2(0.0f, 0.0f);
		const vec2 ToFinish = normalize(FinishPos - Pos);
		
		// Check for freeze tiles ahead
		bool HasFreezeAhead = false;
		for(float dx = 0.0f; dx <= 64.0f; dx += 8.0f)
		{
			vec2 CheckPos = Pos + vec2((ToFinish.x > 0 ? 1.0f : -1.0f) * dx, 0.0f);
			if(IsFreezeAtPos(pCollision, CheckPos))
			{
				HasFreezeAhead = true;
				PreferUp = true;
				break;
			}
		}

		for(const auto &TilePos : HookTiles)
		{
			const float Dist = distance(Pos, TilePos);
			if(Dist > HookLength || Dist < 1.0f)
				continue;
			vec2 CollisionPos;
			vec2 BeforeCollision;
			if(pCollision->IntersectLine(Pos, TilePos, &CollisionPos, &BeforeCollision))
			{
				if(distance(CollisionPos, TilePos) > 20.0f)
					continue;
			}
			const vec2 Dir = normalize(TilePos - Pos);
			const float Toward = dot(Dir, ToFinish);
			if(Toward < 0.1f && !PreferUp)
				continue;
			
			// Strong preference for upward hooks when needed
			const float UpBias = (TilePos.y < Pos.y - 12.0f) ? (PreferUp ? 3.0f : 1.5f) : 0.0f;
			const float Score = Toward * 2.0f - (Dist / HookLength) + UpBias;
			if(Score > BestScore)
			{
				BestScore = Score;
				Best = TilePos;
			}
		}

		if(BestScore > -std::numeric_limits<float>::infinity())
		{
			OutTarget = Best;
			return true;
		}
		return false;
	}

	static void EnsureAimNonZero(CNetObj_PlayerInput &Input)
	{
		if(!Input.m_TargetX && !Input.m_TargetY)
			Input.m_TargetX = 1;
	}

	static void BuildAutoVisualization(CGameClient *pGameClient, int LocalId, const std::vector<CNetObj_PlayerInput> &Plan,
		std::vector<vec2> &OutPositions, std::vector<vec2> &OutTiles, std::vector<int> &OutTileIndices)
	{
		OutPositions.clear();
		OutTiles.clear();
		OutTileIndices.clear();
		if(!pGameClient || Plan.empty())
			return;
		const CCollision *pCollision = pGameClient->Collision();
		if(!pCollision)
			return;

		CGameWorld SimWorld;
		SimWorld.Init(pGameClient->m_GameWorld.Collision(), pGameClient->m_GameWorld.TuningList(), nullptr);
		SimWorld.m_WorldConfig = pGameClient->m_GameWorld.m_WorldConfig;
		SimWorld.m_Teams = pGameClient->m_GameWorld.m_Teams;
		SimWorld.CopyWorld(&pGameClient->m_GameWorld);

		CCharacter *pChar = SimWorld.GetCharacterById(LocalId);
		if(!pChar)
			return;

		std::unordered_set<int> VisitedTiles;
		OutPositions.reserve(Plan.size() + 1);
		OutPositions.push_back(pChar->GetPos());

		for(const auto &Input : Plan)
		{
			pChar->OnDirectInput(&Input);
			pChar->OnDirectInput(&Input);
			pChar->OnPredictedInput(&Input);
			SimWorld.Tick();
			SimWorld.m_GameTick++;

			vec2 Pos = pChar->GetPos();
			OutPositions.push_back(Pos);

			const int Index = pCollision->GetPureMapIndex(Pos);
			if(VisitedTiles.insert(Index).second)
			{
				OutTiles.push_back(pCollision->GetPos(Index));
				OutTileIndices.push_back(Index);
			}
		}
	}

	static void BuildFlowField(const CCollision *pCollision, int Width, int Height, const std::vector<int> &FinishIndices,
		std::vector<int8_t> &OutDirX, std::vector<int8_t> &OutDirY)
	{
		const int Total = Width * Height;
		OutDirX.assign(Total, 0);
		OutDirY.assign(Total, 0);
		std::vector<int> Dist(Total, -1);
		std::queue<int> Queue;

		for(int Index : FinishIndices)
		{
			if(Index < 0 || Index >= Total)
				continue;
			Dist[Index] = 0;
			Queue.push(Index);
		}

		const int Offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		while(!Queue.empty())
		{
			const int Cur = Queue.front();
			Queue.pop();
			const int CurX = Cur % Width;
			const int CurY = Cur / Width;
			for(const auto &Offset : Offsets)
			{
				const int Nx = CurX + Offset[0];
				const int Ny = CurY + Offset[1];
				if(Nx < 0 || Ny < 0 || Nx >= Width || Ny >= Height)
					continue;
				const int Next = Ny * Width + Nx;
				if(Dist[Next] != -1)
					continue;
				if(IsBlockedTile(pCollision, Next))
					continue;
				Dist[Next] = Dist[Cur] + 1;
				OutDirX[Next] = (int8_t)(CurX - Nx);
				OutDirY[Next] = (int8_t)(CurY - Ny);
				Queue.push(Next);
			}
		}
	}

	static bool FindNearestFlowDir(const CCollision *pCollision, int Width, int Height, int StartIndex,
		const std::vector<int8_t> &DirX, const std::vector<int8_t> &DirY, int Radius,
		int &OutDirX, int &OutDirY, int &OutIndex)
	{
		if(!pCollision || Width <= 0 || Height <= 0)
			return false;
		const int StartX = StartIndex % Width;
		const int StartY = StartIndex / Width;
		int BestDist = 999999;
		bool Found = false;
		for(int dy = -Radius; dy <= Radius; dy++)
		{
			for(int dx = -Radius; dx <= Radius; dx++)
			{
				const int Nx = StartX + dx;
				const int Ny = StartY + dy;
				if(Nx < 0 || Ny < 0 || Nx >= Width || Ny >= Height)
					continue;
				const int Index = Ny * Width + Nx;
				if(IsBlockedTile(pCollision, Index))
					continue;
				const int DirXVal = DirX[Index];
				const int DirYVal = DirY[Index];
				if(DirXVal == 0 && DirYVal == 0)
					continue;
				const int Dist = abs(dx) + abs(dy);
				if(Dist < BestDist)
				{
					BestDist = Dist;
					OutDirX = DirXVal;
					OutDirY = DirYVal;
					OutIndex = Index;
					Found = true;
				}
			}
		}
		return Found;
	}

	static CNetObj_PlayerInput BuildFlowInput(const CCollision *pCollision, CCharacter *pChar, const std::vector<int8_t> &DirX,
		const std::vector<int8_t> &DirY, int Width, int Height, vec2 FinishPos, const std::vector<vec2> &HookTiles, int TickNow, int &LastJumpTick)
	{
		CNetObj_PlayerInput Input = {};
		if(!pCollision || !pChar || Width <= 0 || Height <= 0)
			return Input;

		const vec2 Pos = pChar->GetPos();
		const int Index = pCollision->GetPureMapIndex(Pos);
		int DirXVal = (Index >= 0 && Index < (int)DirX.size()) ? DirX[Index] : 0;
		int DirYVal = (Index >= 0 && Index < (int)DirY.size()) ? DirY[Index] : 0;
		if(DirXVal == 0 && DirYVal == 0)
		{
			int FallbackDirX = 0;
			int FallbackDirY = 0;
			int FallbackIndex = Index;
			if(FindNearestFlowDir(pCollision, Width, Height, Index, DirX, DirY, 6, FallbackDirX, FallbackDirY, FallbackIndex))
			{
				DirXVal = FallbackDirX;
				DirYVal = FallbackDirY;
			}
			else
			{
				// Fallback: simple direction to finish
				const vec2 ToFinish = FinishPos - Pos;
				if(std::abs(ToFinish.x) > 8.0f)
					DirXVal = ToFinish.x > 0.0f ? 1 : -1;
				if(ToFinish.y < -8.0f)
					DirYVal = -1;
			}
		}

		Input.m_Direction = DirXVal == 0 ? 0 : (DirXVal > 0 ? 1 : -1);

		const bool WantsUp = DirYVal < 0;
		const bool Grounded = pChar->IsGrounded();
		const bool BlockedAhead = Input.m_Direction != 0 && pCollision->TestBox(Pos + vec2((float)Input.m_Direction * 16.0f, 0.0f), vec2(28.0f, 28.0f));
		
		// Check for freeze ahead
		bool HasFreezeAhead = false;
		if(Input.m_Direction != 0)
		{
			for(float dx = 8.0f; dx <= 48.0f; dx += 8.0f)
			{
				vec2 CheckPos = Pos + vec2((float)Input.m_Direction * dx, 0.0f);
				if(IsFreezeAtPos(pCollision, CheckPos))
				{
					HasFreezeAhead = true;
					break;
				}
			}
		}

		if((WantsUp || BlockedAhead || HasFreezeAhead) && Grounded && TickNow - LastJumpTick > 1)
		{
			Input.m_Jump = 1;
			LastJumpTick = TickNow;
		}

		vec2 TargetPos = FinishPos;
		const bool NeedsHook = WantsUp || !Grounded || BlockedAhead || HasFreezeAhead;
		if(NeedsHook)
		{
			vec2 HookTarget;
			const float HookLen = pChar->Core() ? pChar->Core()->m_Tuning.m_HookLength : 380.0f;
			const bool PreferUp = WantsUp || HasFreezeAhead;
			if(PickHookTarget(pCollision, HookTiles, Pos, FinishPos, HookLen, HookTarget, PreferUp))
			{
				Input.m_Hook = 1;
				TargetPos = HookTarget;
			}
			else if(BlockedAhead || WantsUp || HasFreezeAhead)
			{
				// Try to hook upward or at wall
				vec2 Aim = vec2((Input.m_Direction == 0 ? 1.0f : (float)Input.m_Direction) * 96.0f, -160.0f);
				vec2 CollisionPos;
				vec2 BeforeCollision;
				if(pCollision->IntersectLine(Pos, Pos + Aim, &CollisionPos, &BeforeCollision))
				{
					if(CollisionPos.y < Pos.y - 8.0f || HasFreezeAhead)
					{
						Input.m_Hook = 1;
						TargetPos = CollisionPos;
					}
				}
			}
		}

		vec2 AimDir = TargetPos - Pos;
		Input.m_TargetX = (int)AimDir.x;
		Input.m_TargetY = (int)AimDir.y;
		EnsureAimNonZero(Input);
		return Input;
	}

	static std::vector<CNetObj_PlayerInput> BuildFlowPlan(CGameClient *pGameClient, int LocalId, const std::vector<int8_t> &DirX,
		const std::vector<int8_t> &DirY, int Width, int Height, vec2 FinishPos, const std::vector<vec2> &HookTiles, int Ticks)
	{
		std::vector<CNetObj_PlayerInput> Plan;
		if(!pGameClient || Width <= 0)
			return Plan;
		const CCollision *pCollision = pGameClient->Collision();
		if(!pCollision)
			return Plan;

		CGameWorld SimWorld;
		SimWorld.Init(pGameClient->m_GameWorld.Collision(), pGameClient->m_GameWorld.TuningList(), nullptr);
		SimWorld.m_WorldConfig = pGameClient->m_GameWorld.m_WorldConfig;
		SimWorld.m_Teams = pGameClient->m_GameWorld.m_Teams;
		SimWorld.CopyWorld(&pGameClient->m_GameWorld);

		CCharacter *pChar = SimWorld.GetCharacterById(LocalId);
		if(!pChar)
			return Plan;

		Plan.reserve(Ticks);
		int LastJumpTick = -100000;
		for(int t = 0; t < Ticks; t++)
		{
			CNetObj_PlayerInput Input = BuildFlowInput(pCollision, pChar, DirX, DirY, Width, Height, FinishPos, HookTiles, t, LastJumpTick);
			Plan.push_back(Input);
			pChar->OnDirectInput(&Input);
			pChar->OnDirectInput(&Input);
			pChar->OnPredictedInput(&Input);
			SimWorld.Tick();
			SimWorld.m_GameTick++;
		}
		return Plan;
	}

	static void BuildFlowTilePath(const CCollision *pCollision, int Width, int Height, int StartIndex, const std::vector<int8_t> &DirX,
		const std::vector<int8_t> &DirY, int Steps, std::vector<vec2> &OutPositions, std::vector<vec2> &OutTiles, std::vector<int> &OutTileIndices)
	{
		OutPositions.clear();
		OutTiles.clear();
		OutTileIndices.clear();
		if(!pCollision || Width <= 0 || Height <= 0)
			return;

		int Index = StartIndex;
		for(int i = 0; i < Steps; i++)
		{
			if(Index < 0 || Index >= Width * Height)
				break;
			if(i == 0 || (OutTileIndices.empty() || OutTileIndices.back() != Index))
			{
				OutTileIndices.push_back(Index);
				OutTiles.push_back(pCollision->GetPos(Index));
				OutPositions.push_back(pCollision->GetPos(Index));
			}

			const int DirXVal = DirX[Index];
			const int DirYVal = DirY[Index];
			if(DirXVal == 0 && DirYVal == 0)
				break;

			const int X = Index % Width;
			const int Y = Index / Width;
			const int NX = X + DirXVal;
			const int NY = Y + DirYVal;
			if(NX < 0 || NY < 0 || NX >= Width || NY >= Height)
				break;
			Index = NY * Width + NX;
		}
	}

	static STasAutoAction RandomAction(std::mt19937 &Rng, int DesiredDir, bool PreferJump, int HookBias)
	{
		std::uniform_int_distribution<int> Chance(0, 99);
		std::uniform_int_distribution<int> DirDist(0, 2);
		STasAutoAction Action;
		Action.m_Direction = (Chance(Rng) < 70) ? DesiredDir : (DirDist(Rng) - 1);
		Action.m_Jump = (Chance(Rng) < (PreferJump ? 45 : 18)) ? 1 : 0;
		Action.m_Hook = (Chance(Rng) < HookBias) ? 1 : 0;
		return Action;
	}

	static void MutateActions(std::vector<STasAutoAction> &Actions, std::mt19937 &Rng, int MutationRate, int DesiredDir, bool PreferJump, int HookBias)
	{
		std::uniform_int_distribution<int> Chance(0, 99);
		for(auto &Action : Actions)
		{
			if(Chance(Rng) < MutationRate)
				Action = RandomAction(Rng, DesiredDir, PreferJump, HookBias);
		}
	}

	static float EvaluateAutoCandidate(CGameClient *pGameClient, int LocalId, const CNetObj_PlayerInput &BaseInput,
		vec2 FinishPos, const std::vector<vec2> &HookTiles, const STasAutoSettings &Settings,
		const std::vector<STasAutoAction> &Actions, STasAutoCandidate &OutCandidate)
	{
		if(!pGameClient)
			return -std::numeric_limits<float>::infinity();
		const CCollision *pCollision = pGameClient->Collision();
		if(!pCollision)
			return -std::numeric_limits<float>::infinity();

		CGameWorld SimWorld;
		SimWorld.Init(pGameClient->m_GameWorld.Collision(), pGameClient->m_GameWorld.TuningList(), nullptr);
		SimWorld.m_WorldConfig = pGameClient->m_GameWorld.m_WorldConfig;
		SimWorld.m_Teams = pGameClient->m_GameWorld.m_Teams;
		SimWorld.CopyWorld(&pGameClient->m_GameWorld);

		CCharacter *pChar = SimWorld.GetCharacterById(LocalId);
		if(!pChar)
			return -std::numeric_limits<float>::infinity();

		float BestDist = distance(pChar->GetPos(), FinishPos);
		float SafetyAccum = 0.0f;
		float WallPenalty = 0.0f;
		float StuckPenalty = 0.0f;
		bool Finished = false;
		int FinishTick = -1;
		OutCandidate.m_Inputs.clear();

		for(size_t t = 0; t < Actions.size(); t++)
		{
			const STasAutoAction &Action = Actions[t];
			const vec2 PrevPos = pChar->GetPos();
			CNetObj_PlayerInput Input = BaseInput;
			Input.m_Direction = Action.m_Direction;
			Input.m_Jump = Action.m_Jump;
			Input.m_Hook = Action.m_Hook;

			vec2 TargetPos = FinishPos;
			if(Input.m_Hook != 0 && !HookTiles.empty())
			{
				vec2 HookTarget;
				const float HookLen = pChar->Core() ? pChar->Core()->m_Tuning.m_HookLength : 380.0f;
				if(PickHookTarget(pCollision, HookTiles, pChar->GetPos(), FinishPos, HookLen, HookTarget))
					TargetPos = HookTarget;
				else
					Input.m_Hook = 0;
			}

			vec2 AimDir = TargetPos - pChar->GetPos();
			Input.m_TargetX = (int)AimDir.x;
			Input.m_TargetY = (int)AimDir.y;
			EnsureAimNonZero(Input);

			OutCandidate.m_Inputs.push_back(Input);

			pChar->OnDirectInput(&Input);
			pChar->OnDirectInput(&Input);
			pChar->OnPredictedInput(&Input);
			SimWorld.Tick();
			SimWorld.m_GameTick++;

			vec2 Pos = pChar->GetPos();
			const float Moved = distance(PrevPos, Pos);
			if(Moved < 0.5f)
				StuckPenalty += 1.0f;

			if(Action.m_Direction != 0)
			{
				vec2 CheckPos = PrevPos + vec2((float)Action.m_Direction * 16.0f, 0.0f);
				if(pCollision->TestBox(CheckPos, vec2(28.0f, 28.0f)))
					WallPenalty += 1.0f;
			}
			vec2 CollisionPos;
			vec2 BeforeCollision;
			if(pCollision->IntersectLine(PrevPos, FinishPos, &CollisionPos, &BeforeCollision) && Input.m_Hook == 0)
				WallPenalty += 0.5f;

			if(IsDeathAtPos(pCollision, Pos) || IsFreezeAtPos(pCollision, Pos) || pChar->m_FreezeTime > 0)
			{
				BestDist += 500.0f;
				break;
			}

			const float Dist = distance(Pos, FinishPos);
			BestDist = std::min(BestDist, Dist);
			SafetyAccum += SafetyScore(pCollision, Pos);

			if(IsFinishAtPos(pCollision, Pos))
			{
				Finished = true;
				FinishTick = (int)t;
				break;
			}
		}

		const float AvgSafety = SafetyAccum / std::max<size_t>(1, Actions.size());
		const float ProgressScore = -BestDist;
		const float SpeedScore = Finished ? (float)((int)Actions.size() - FinishTick) : 0.0f;
		float Score = ProgressScore * Settings.m_ProgressWeight +
					  AvgSafety * Settings.m_SafetyWeight +
					  SpeedScore * Settings.m_SpeedWeight;
		Score -= WallPenalty * 250.0f;
		Score -= StuckPenalty * 150.0f;
		if(Finished)
			Score += 50000.0f;

		OutCandidate.m_Score = Score;
		OutCandidate.m_Finished = Finished;
		OutCandidate.m_FinishTick = FinishTick;
		OutCandidate.m_EndPos = pChar->GetPos();
		return Score;
	}
}

void CControls::TasInit()
{
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
	m_pTasWorld = new CGameWorld();
	m_pTasWorld->Init(GameClient()->m_GameWorld.Collision(), GameClient()->m_GameWorld.TuningList(), nullptr);
	m_pTasWorld->m_WorldConfig = GameClient()->m_GameWorld.m_WorldConfig;
	m_pTasWorld->m_Teams = GameClient()->m_GameWorld.m_Teams;
	m_pTasWorld->CopyWorld(&GameClient()->m_GameWorld);
	m_pTasWorld->m_GameTick = 0;
	m_pTasWorldPredicted = new CGameWorld();
	m_pTasWorldPredicted->Init(GameClient()->m_GameWorld.Collision(), GameClient()->m_GameWorld.TuningList(), nullptr);
	m_pTasWorldPredicted->m_WorldConfig = GameClient()->m_GameWorld.m_WorldConfig;
	m_pTasWorldPredicted->m_Teams = GameClient()->m_GameWorld.m_Teams;
	m_pTasWorldPredicted->CopyWorld(m_pTasWorld);

	m_vTasChars.clear();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!GameClient()->m_aClients[ClientId].m_Active)
			continue;

		STasCharacterHistory Entry;
		Entry.m_Id = ClientId;
		m_vTasChars.push_back(std::move(Entry));
	}

	m_vTasTicks.clear();
	m_vTasActions.clear();
	m_vTasAttackTick.clear();
	m_vTasMouse.clear();
	m_vTasDummySwitch.clear();
	m_vTasKill.clear();
	m_vTasPositions.clear();
	m_TasLastUpdateTime = time_get();
	m_TasLastFrameTime = time_get();
	m_TasAccumulator = 0.0;
	m_TasPlayLastGameTick = -1;
	m_TasManualStepLastTime = 0;
	m_TasLastAttackTick = -100000;
	m_aTasLastAttackTick[0] = -100000;
	m_aTasLastAttackTick[1] = -100000;
	m_TasClientTickBase = Client()->PrevGameTick(g_Config.m_ClDummy);
	m_TasPrevFireForAttack = 0;
	m_TasLastObservedTasAttackTick = -1;
	m_aTasPrevFireForAttack[0] = 0;
	m_aTasPrevFireForAttack[1] = 0;
	m_aTasLastObservedTasAttackTick[0] = -1;
	m_aTasLastObservedTasAttackTick[1] = -1;
	std::fill(std::begin(m_aTasLastTickMouse), std::end(m_aTasLastTickMouse), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aTasNextTickMouse), std::end(m_aTasNextTickMouse), vec2(0.0f, 0.0f));
	std::fill(std::begin(m_aTasInterpMouse), std::end(m_aTasInterpMouse), vec2(0.0f, 0.0f));
	m_TasSmoothIntra = 0.0f;
	TasPlaybackReset();
	g_Config.m_shalawaTasInit = 1;
	// TAS Mode is always 1 (timed mode) when recording
	if(g_Config.m_shalawaRecording)
	{
		g_Config.m_shalawaTasMode = 1;
		// Automatically enable TAS World when recording starts
		g_Config.m_shalawaTasWorld = 1;
	}
}

void CControls::TasPlaybackReset()
{
	m_TasPlaybackPrepared = false;
	m_TasPlayIndex = 0;
	m_TasPlayLastGameTick = -1;
	mem_zero(&m_TasPlayCachedInput, sizeof(m_TasPlayCachedInput));
	std::fill(std::begin(m_TasPlayCachedMouse), std::end(m_TasPlayCachedMouse), vec2(0.0f, 0.0f));
	mem_zero(&m_TasPlayCachedKill, sizeof(m_TasPlayCachedKill));
}

void CControls::TasPlaybackEnsurePrepared()
{
	if(m_TasPlaybackPrepared)
		return;
	m_TasPlaybackPrepared = true;
	m_TasPlayIndex = 0;
	m_TasPlayLastGameTick = -1;
	mem_zero(&m_TasPlayCachedInput, sizeof(m_TasPlayCachedInput));
	std::fill(std::begin(m_TasPlayCachedMouse), std::end(m_TasPlayCachedMouse), vec2(0.0f, 0.0f));
	mem_zero(&m_TasPlayCachedKill, sizeof(m_TasPlayCachedKill));
}

void CControls::TasRecordTick()
{
	if(!m_pTasWorld)
		return;

	// Record inputs and positions for all local characters for this tick
	for(int i = 0; i < NUM_DUMMIES; i++)
	{
		const int LocalId = GameClient()->m_aLocalIds[i];
		if(LocalId < 0)
			continue;

		m_vTasTicks.push_back(m_pTasWorld->m_GameTick);
		m_vTasActions.push_back(m_aInputData[i]);
		m_vTasMouse.push_back(vec2(m_aInputData[i].m_TargetX, m_aInputData[i].m_TargetY));
		m_vTasDummySwitch.push_back(i);
		m_vTasKill.push_back(0); // Kill tracking
		m_vTasAttackTick.push_back(m_aTasLastAttackTick[i]);

		// Record position for this dummy too
		if(const CCharacter *pChar = m_pTasWorld->GetCharacterById(LocalId))
		{
			m_vTasPositions.push_back(pChar->m_Pos);
		}
		else
		{
			m_vTasPositions.push_back(vec2(0, 0));
		}
	}

	// Kill tracking removed

	for(auto &Entry : m_vTasChars)
	{
		CCharacter *pChar = m_pTasWorld->GetCharacterById(Entry.m_Id);
		if(!pChar)
			continue;

		Entry.m_Positions.push_back(pChar->m_Pos);
		Entry.m_PrevPositions.push_back(pChar->m_PrevPos);
		Entry.m_FreezeTime.push_back(pChar->m_FreezeTime);
		Entry.m_ReloadTimer.push_back(pChar->GetReloadTimer());
		Entry.m_Cores.push_back(pChar->GetCore());
	}
}

void CControls::TasUpdate()
{
	static bool s_WasRecording = false;
	if(s_WasRecording && !g_Config.m_shalawaRecording)
	{
		g_Config.m_shalawaTasWorld = 0;
		g_Config.m_shalawaTasInit = 0;
	}
	s_WasRecording = g_Config.m_shalawaRecording != 0;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// TAS Mode is always 1 (timed mode) when recording
	if(g_Config.m_shalawaRecording)
	{
		g_Config.m_shalawaTasMode = 1;
		// Automatically enable TAS World when recording starts
		if(!g_Config.m_shalawaTasWorld)
			g_Config.m_shalawaTasWorld = 1;
	}

	// TAS timed stepping (mode=1, ported from nones-twave)
	if(g_Config.m_shalawaRecording && g_Config.m_shalawaTasMode)
	{
		if(!g_Config.m_shalawaTasInit)
		{
			TasInit();
			// After init, wait one frame for proper initialization
			return;
		}
		
		// TAS World must tick when recording, regardless of g_Config.m_shalawaTasWorld toggle
		if(g_Config.m_shalawaTasInit && m_pTasWorld)
		{
			const int64_t NowTick = time_get();
			const double Now = (double)NowTick / (double)time_freq();
			const int TasTickSpeed = std::max(1, std::min((int)g_Config.m_shalawaTasTickSpeed, 50));
			const double TickInterval = 1.0 / (50.0 * (double)TasTickSpeed);

			// First time initialization
			if(m_TasLastTickTime == 0.0)
			{
				m_TasLastTickTime = Now;
				m_TasNextTickTime = Now + TickInterval;
				for(int i = 0; i < NUM_DUMMIES; i++)
				{
					m_aTasLastTickMouse[i] = m_aMousePos[i];
					m_aTasNextTickMouse[i] = m_aMousePos[i];
				}
			}

			double DeltaTime = (double)Client()->RenderFrameTime();
			if(DeltaTime > 0.1) DeltaTime = 0.1;

			bool DidRewind = false;
			if(g_Config.m_shalawaTasPrev && !m_vTasActions.empty())
			{
				m_TasAccumulator -= DeltaTime * (double)TasTickSpeed;
				if(m_TasAccumulator <= 0.0)
				{
					while(m_TasAccumulator <= 0.0)
					{
						m_TasAccumulator += 1.0;
						TasStepBack();
					}
				}
				DidRewind = true;
				if(g_Config.m_shalawaTasPrev == 2)
					g_Config.m_shalawaTasPrev = 0;
			}

			if(!DidRewind && !g_Config.m_shalawaTasPause)
			{
				m_TasAccumulator += DeltaTime * (double)TasTickSpeed;

				if(m_TasAccumulator >= 1.0)
				{
					while(m_TasAccumulator >= 1.0)
					{
						m_TasAccumulator -= 1.0;
						m_TasLastUpdateTime = time_get();

						// Advancement
						m_TasLastTickTime = m_TasNextTickTime;
						m_TasNextTickTime += TickInterval;

						for(int i = 0; i < NUM_DUMMIES; i++)
						{
							m_aTasLastTickMouse[i] = m_aTasNextTickMouse[i];
							m_aTasNextTickMouse[i] = m_aMousePos[i];
						}

						const int Id = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
						CCharacter *pChar = m_pTasWorld->GetCharacterById(Id);
						if(pChar)
						{
							// CRITICAL: Update m_aInputData with REAL input from keyboard/mouse BEFORE applying to TAS World
							// This ensures TAS World uses the latest input from the player
							for(int i = 0; i < NUM_DUMMIES; i++)
							{
								// Update direction from keyboard input (WASD)
								m_aInputData[i].m_Direction = 0;
								if(m_aInputDirectionLeft[i] && !m_aInputDirectionRight[i])
									m_aInputData[i].m_Direction = -1;
								if(!m_aInputDirectionLeft[i] && m_aInputDirectionRight[i])
									m_aInputData[i].m_Direction = 1;
								
								// Update mouse position from TAS interpolated mouse
								// Use m_aTasNextTickMouse which was updated at the start of this tick
								vec2 Pos = m_aTasNextTickMouse[i];
								// Scale mouse distance if needed
								if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
								{
									const int MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
									if(MaxDistance > 5 && MaxDistance < 1000)
										Pos *= 1000.0f / (float)MaxDistance;
								}
								m_aInputData[i].m_TargetX = (int)Pos.x;
								m_aInputData[i].m_TargetY = (int)Pos.y;
								if(!m_aInputData[i].m_TargetX && !m_aInputData[i].m_TargetY)
									m_aInputData[i].m_TargetX = 1;
							}
							
							// Sync dummy input after updating with real input
							SyncDummyInput();

							for(int i = 0; i < NUM_DUMMIES; i++)
							{
								const int LocalId = GameClient()->m_aLocalIds[i];
								if(LocalId >= 0)
								{
									if(CCharacter *pLocalChar = m_pTasWorld->GetCharacterById(LocalId))
									{
										// Apply input twice (as in original TAS code)
										pLocalChar->OnDirectInput(&m_aInputData[i]);
										pLocalChar->OnDirectInput(&m_aInputData[i]);
										pLocalChar->OnPredictedInput(&m_aInputData[i]);
									}
								}
							}

							// CRITICAL: Apply cl_dummy_hammer logic to dummy in TAS world
							if(g_Config.m_ClDummyHammer)
							{
								const int DummyId = GameClient()->m_aLocalIds[!g_Config.m_ClDummy];
								if(DummyId >= 0)
								{
									CCharacter *pDummyChar = m_pTasWorld->GetCharacterById(DummyId);
									if(pDummyChar)
									{
										ApplyTasDummyHammer(pDummyChar, m_pTasWorld);
									}
								}
							}

							for(int Slot = 0; Slot < NUM_DUMMIES; Slot++)
							{
								const int CurFire = m_aInputData[Slot].m_Fire;
								if((m_aTasPrevFireForAttack[Slot] & 1) == 0 && (CurFire & 1) == 1)
									m_aTasLastAttackTick[Slot] = m_pTasWorld->m_GameTick;
								m_aTasPrevFireForAttack[Slot] = CurFire;
							}

							m_pTasWorld->Tick();
							m_pTasWorld->m_GameTick++;

							for(int Slot = 0; Slot < NUM_DUMMIES; Slot++)
							{
								const int LocalId = GameClient()->m_aLocalIds[Slot];
								if(LocalId >= 0)
								{
									if(CCharacter *pLocalChar = m_pTasWorld->GetCharacterById(LocalId))
										HandleTasTeleport(pLocalChar);
								}
							}
							for(int Slot = 0; Slot < NUM_DUMMIES; Slot++)
							{
								const int LocalId = GameClient()->m_aLocalIds[Slot];
								if(LocalId >= 0)
								{
									if(CCharacter *pLocalChar = m_pTasWorld->GetCharacterById(LocalId))
									{
										const int TasAttackTick = pLocalChar->GetAttackTick();
										if(TasAttackTick != m_aTasLastObservedTasAttackTick[Slot])
										{
											m_aTasLastObservedTasAttackTick[Slot] = TasAttackTick;
											m_aTasLastAttackTick[Slot] = m_pTasWorld->m_GameTick;
										}
									}
								}
							}
							TasRecordTick();
							DetectTasRollback();
						}
					}

					// Safety resync
					if(fabs(Now - m_TasNextTickTime) > TickInterval * 5.0)
					{
						m_TasLastTickTime = Now - TickInterval;
						m_TasNextTickTime = Now;
					}
				}
			}

			// INTRA: Use the accumulator directly for standard movement interpolation
			m_TasSmoothIntra = (float)m_TasAccumulator;
			if(m_TasSmoothIntra < 0.0f) m_TasSmoothIntra = 0.0f;
			if(m_TasSmoothIntra > 1.0f) m_TasSmoothIntra = 1.0f;

			for(int i = 0; i < NUM_DUMMIES; i++)
			{
				m_aTasInterpMouse[i] = mix(m_aTasLastTickMouse[i], m_aTasNextTickMouse[i], m_TasSmoothIntra);
			}

			// Update target pos - use TAS World position
			{
				const int Dummy = g_Config.m_ClDummy;
				const float Zoom = GameClient()->m_Camera.m_Zoom;
				const vec2 DyncamOffsetDelta = GameClient()->m_Camera.m_DyncamTargetCameraOffset - GameClient()->m_Camera.m_aDyncamCurrentCameraOffset[Dummy];
				const vec2 TasPos = GetInterpolatedPlayerPosition(Dummy);
				const vec2 MouseOffset = g_Config.m_shalawaRecording ? m_aMousePos[Dummy] : m_aTasInterpMouse[Dummy];
				m_aTargetPos[Dummy] = TasPos + MouseOffset - DyncamOffsetDelta + DyncamOffsetDelta / Zoom;
			}

			// Predicted view for slow-mo
			if(m_pTasWorldPredicted)
			{
				m_pTasWorldPredicted->CopyWorld(m_pTasWorld);
				const int Id = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
				if(CCharacter *pChar = m_pTasWorldPredicted->GetCharacterById(Id))
				{
					CNetObj_PlayerInput PredictedInput = m_aInputData[g_Config.m_ClDummy];
					PredictedInput.m_TargetX = (int)m_aTasNextTickMouse[g_Config.m_ClDummy].x;
					PredictedInput.m_TargetY = (int)m_aTasNextTickMouse[g_Config.m_ClDummy].y;
					if(!PredictedInput.m_TargetX && !PredictedInput.m_TargetY)
						PredictedInput.m_TargetX = 1;

					pChar->OnDirectInput(&PredictedInput);
					pChar->OnPredictedInput(&PredictedInput);
					m_pTasWorldPredicted->Tick();
					m_pTasWorldPredicted->m_GameTick++;

					for(int Slot = 0; Slot < NUM_DUMMIES; Slot++)
					{
						const int LocalId = GameClient()->m_aLocalIds[Slot];
						if(LocalId >= 0)
						{
							if(CCharacter *pLocalChar = m_pTasWorldPredicted->GetCharacterById(LocalId))
								HandleTasTeleport(pLocalChar);
						}
					}
				}
			}
		}
	}
	else if(g_Config.m_shalawaPlaying)
	{
		TasPlaybackEnsurePrepared();

		const int CurrentGameTick = Client()->PredGameTick(0);
		if(CurrentGameTick != m_TasPlayLastGameTick)
		{
			m_TasPlayLastGameTick = CurrentGameTick;

			const int Count = (int)minimum(m_vTasTicks.size(),
				minimum(m_vTasActions.size(), minimum(m_vTasMouse.size(), minimum(m_vTasDummySwitch.size(), m_vTasKill.size()))));
			if(m_TasPlayIndex < Count)
			{
				const int TickKey = m_vTasTicks[m_TasPlayIndex];

				for(int Slot = 0; Slot < NUM_DUMMIES; Slot++)
				{
					CNetObj_PlayerInput Idle = m_TasPlayCachedInput[Slot];
					Idle.m_Direction = 0;
					Idle.m_Jump = 0;
					Idle.m_Hook = 0;
					if(Idle.m_Fire & 1)
						Idle.m_Fire = (Idle.m_Fire + 1) & INPUT_STATE_MASK;
					Idle.m_WantedWeapon = 0;
					Idle.m_NextWeapon = 0;
					Idle.m_PrevWeapon = 0;
					m_TasPlayCachedInput[Slot] = Idle;
					m_TasPlayCachedMouse[Slot] = vec2(0.0f, 0.0f);
				}

				while(m_TasPlayIndex < Count && m_vTasTicks[m_TasPlayIndex] == TickKey)
				{
					const int RecordedDummy = m_vTasDummySwitch[m_TasPlayIndex] ? 1 : 0;
					m_TasPlayCachedInput[RecordedDummy] = m_vTasActions[m_TasPlayIndex];
					m_TasPlayCachedMouse[RecordedDummy] = m_vTasMouse[m_TasPlayIndex];
					m_TasPlayCachedKill[RecordedDummy] = m_vTasKill[m_TasPlayIndex];
					m_TasPlayIndex++;
				}
			}
			else
				g_Config.m_shalawaPlaying = 0;
		}

		// Apply cached input for this slot every tick.
		const int Dummy = g_Config.m_ClDummy;
		m_aInputData[Dummy] = m_TasPlayCachedInput[Dummy];
		m_aInputData[Dummy].m_TargetX = (int)m_TasPlayCachedMouse[Dummy].x;
		m_aInputData[Dummy].m_TargetY = (int)m_TasPlayCachedMouse[Dummy].y;
		if(m_TasPlayCachedKill[Dummy])
		{
			m_TasPlayCachedKill[Dummy] = 0;
			GameClient()->SendKill();
		}
	}
}

const STasCharacterHistory *CControls::TasFindCharHistory(int Id) const
{
	for(const auto &Entry : m_vTasChars)
	{
		if(Entry.m_Id == Id)
			return &Entry;
	}
	return nullptr;
}

STasCharacterHistory *CControls::TasFindCharHistory(int Id)
{
	for(auto &Entry : m_vTasChars)
	{
		if(Entry.m_Id == Id)
			return &Entry;
	}
	return nullptr;
}

void CControls::TasAutoReset()
{
	m_TasAuto = STasAutoState();
	g_Config.m_shalawaTasAutoRunning = 0;
}

bool CControls::TasAutoApply(int Dummy)
{
	if(g_Config.m_shalawaTasAutoMode == 0 || !g_Config.m_shalawaTasAutoRunning)
	{
		if(m_TasAuto.m_Running)
			dbg_msg("tas_auto", "Auto stopped");
		m_TasAuto.m_Running = false;
		return false;
	}

	if(!GameClient())
		return false;

	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || !GameClient()->m_aClients[LocalId].m_Active)
		return false;

	const CCollision *pCollision = GameClient()->Collision();
	if(!pCollision)
		return false;

	if(!m_TasAuto.m_Running)
	{
		m_TasAuto = STasAutoState();
		m_TasAuto.m_Running = true;
		dbg_msg("tas_auto", "Auto started");
	}

	if(!m_TasAuto.m_TilesCached)
	{
		m_TasAuto.m_FinishTiles.clear();
		m_TasAuto.m_FinishTileIndices.clear();
		m_TasAuto.m_HookTiles.clear();
		const int Width = pCollision->GetWidth();
		const int Height = pCollision->GetHeight();
		for(int y = 0; y < Height; y++)
		{
			for(int x = 0; x < Width; x++)
			{
				const int Index = y * Width + x;
				const int Tile = pCollision->GetTileIndex(Index);
				const int FrontTile = pCollision->GetFrontTileIndex(Index);
				if(IsFinishTileIndex(Tile) || IsFinishTileIndex(FrontTile))
				{
					m_TasAuto.m_FinishTiles.push_back(pCollision->GetPos(Index));
					m_TasAuto.m_FinishTileIndices.push_back(Index);
				}
				if(Tile == TILE_SOLID && FrontTile != TILE_NOHOOK)
					m_TasAuto.m_HookTiles.push_back(pCollision->GetPos(Index));
			}
		}
		m_TasAuto.m_TilesCached = true;
		m_TasAuto.m_FieldReady = false;
		dbg_msg("tas_auto", "Cached finish tiles: %d, hook tiles: %d", (int)m_TasAuto.m_FinishTiles.size(), (int)m_TasAuto.m_HookTiles.size());
	}

	const vec2 CurPos = GameClient()->m_aClients[LocalId].m_Predicted.m_Pos;
	bool HasFinish = false;
	const vec2 FinishPos = PickNearestFinish(m_TasAuto.m_FinishTiles, CurPos, HasFinish);
	if(!HasFinish)
	{
		dbg_msg("tas_auto", "No finish tiles found - cannot run");
		return false;
	}
	m_TasAuto.m_LastFinishPos = FinishPos;
	m_TasAuto.m_HasFinish = HasFinish;

	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	if(!m_TasAuto.m_FieldReady || m_TasAuto.m_FieldWidth != Width || m_TasAuto.m_FieldHeight != Height)
	{
		m_TasAuto.m_FieldWidth = Width;
		m_TasAuto.m_FieldHeight = Height;
		BuildFlowField(pCollision, Width, Height, m_TasAuto.m_FinishTileIndices, m_TasAuto.m_FieldDirX, m_TasAuto.m_FieldDirY);
		m_TasAuto.m_FieldReady = true;
	}

	const STasAutoSettings Settings = GetAutoSettings();
	const int TickNow = Client()->GameTick(g_Config.m_ClDummy);
	bool NeedReplan = m_TasAuto.m_Plan.empty() || m_TasAuto.m_PlanIndex >= (int)m_TasAuto.m_Plan.size();
	if(!NeedReplan && TickNow - m_TasAuto.m_LastPlanTick >= Settings.m_ReplanTicks)
		NeedReplan = true;

	if(NeedReplan)
	{
		const int64_t PlanStart = time_get();
		std::mt19937 Rng((unsigned)time_get());
		const int DesiredDir = (FinishPos.x > CurPos.x + 8.0f) ? 1 : (FinishPos.x < CurPos.x - 8.0f ? -1 : 0);
		const bool PreferJump = FinishPos.y < CurPos.y - 24.0f;
		const CNetObj_PlayerInput BaseInput = m_aInputData[Dummy];
		const int SimBudget = Settings.m_Population * Settings.m_Generations * Settings.m_PlanTicks;
		const bool UseFlowOnly = true;

		if(UseFlowOnly && m_TasAuto.m_FieldReady)
		{
			m_TasAuto.m_Plan = BuildFlowPlan(GameClient(), LocalId, m_TasAuto.m_FieldDirX, m_TasAuto.m_FieldDirY,
				m_TasAuto.m_FieldWidth, m_TasAuto.m_FieldHeight, FinishPos, m_TasAuto.m_HookTiles, Settings.m_PlanTicks);
			m_TasAuto.m_PlanIndex = 0;
			m_TasAuto.m_LastPlanTick = TickNow;
			const int StartIndex = pCollision->GetPureMapIndex(CurPos);
			BuildFlowTilePath(pCollision, m_TasAuto.m_FieldWidth, m_TasAuto.m_FieldHeight, StartIndex, m_TasAuto.m_FieldDirX, m_TasAuto.m_FieldDirY,
				Settings.m_PlanTicks, m_TasAuto.m_PathPositions, m_TasAuto.m_PathTiles, m_TasAuto.m_PathTileIndices);
			dbg_msg("tas_auto", "Flow plan built: len=%d (plan_ticks=%d)", (int)m_TasAuto.m_Plan.size(), Settings.m_PlanTicks);
		}
		const int64_t PlanEnd = time_get();
		m_TasAuto.m_LastPlanMs = (int)((PlanEnd - PlanStart) * 1000 / time_freq());
		dbg_msg("tas_auto", "Plan time: %d ms (pop=%d gen=%d ticks=%d)", m_TasAuto.m_LastPlanMs,
			Settings.m_Population, Settings.m_Generations, Settings.m_PlanTicks);
	}

	if(m_TasAuto.m_Plan.empty())
		return false;

	const int PlanIndex = std::clamp(m_TasAuto.m_PlanIndex, 0, (int)m_TasAuto.m_Plan.size() - 1);
	const CNetObj_PlayerInput Planned = m_TasAuto.m_Plan[PlanIndex];
	CNetObj_PlayerInput Current = m_aInputData[Dummy];
	Current.m_Direction = Planned.m_Direction;
	Current.m_Jump = Planned.m_Jump;
	Current.m_Hook = Planned.m_Hook;
	Current.m_TargetX = Planned.m_TargetX;
	Current.m_TargetY = Planned.m_TargetY;

	if(m_TasAuto.m_FieldReady)
	{
		CCharacter *pChar = GameClient()->m_GameWorld.GetCharacterById(LocalId);
		if(pChar)
		{
			int LastJumpTick = m_TasAuto.m_LastJumpTick;
			CNetObj_PlayerInput Flow = BuildFlowInput(pCollision, pChar, m_TasAuto.m_FieldDirX, m_TasAuto.m_FieldDirY,
				m_TasAuto.m_FieldWidth, m_TasAuto.m_FieldHeight, FinishPos, m_TasAuto.m_HookTiles, TickNow, LastJumpTick);
			m_TasAuto.m_LastJumpTick = LastJumpTick;
			Current = Flow;
		}
	}
	EnsureAimNonZero(Current);

	const int FreezeCheckTicks = 8;
	if(PredictFreeze(Current, FreezeCheckTicks, nullptr, nullptr))
	{
		CNetObj_PlayerInput Alt = Current;
		Alt.m_Direction = 0;
		Alt.m_Hook = 0;
		Alt.m_Jump = 0;
		if(!PredictFreeze(Alt, FreezeCheckTicks, nullptr, nullptr))
			Current = Alt;
		else
		{
			CNetObj_PlayerInput Reverse = Current;
			Reverse.m_Direction = -Current.m_Direction;
			if(!PredictFreeze(Reverse, FreezeCheckTicks, nullptr, nullptr))
				Current = Reverse;
		}
	}

	m_aInputData[Dummy] = Current;
	m_aMousePos[Dummy] = vec2((float)Current.m_TargetX, (float)Current.m_TargetY);
	if(TickNow != m_TasAuto.m_LastAppliedTick)
	{
		m_TasAuto.m_LastAppliedTick = TickNow;
		m_TasAuto.m_PlanIndex++;
	}
	return true;
}

void CControls::TasStepBack()
{
	if(!m_pTasWorld || m_vTasActions.empty())
		return;

	const int LastTick = m_vTasTicks.back();
	while(!m_vTasTicks.empty() && m_vTasTicks.back() == LastTick)
	{
		m_vTasActions.pop_back();
		m_vTasAttackTick.pop_back();
		m_vTasMouse.pop_back();
		m_vTasDummySwitch.pop_back();
		m_vTasKill.pop_back();
		m_vTasTicks.pop_back();
		if(!m_vTasPositions.empty())
			m_vTasPositions.pop_back();
	}

	for(auto &Entry : m_vTasChars)
	{
		if(!Entry.m_Positions.empty())
			Entry.m_Positions.pop_back();
		if(!Entry.m_PrevPositions.empty())
			Entry.m_PrevPositions.pop_back();
		if(!Entry.m_FreezeTime.empty())
			Entry.m_FreezeTime.pop_back();
		if(!Entry.m_ReloadTimer.empty())
			Entry.m_ReloadTimer.pop_back();
		if(!Entry.m_Cores.empty())
			Entry.m_Cores.pop_back();
	}

	if(m_vTasTicks.empty())
		return;

	const int Last = (int)m_vTasTicks.size() - 1;
	m_pTasWorld->m_GameTick = m_vTasTicks[Last];

	if(g_Config.m_shalawaTasLegitMode)
	{
		for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
		{
			for(int i = (int)m_vTasTicks.size() - 1; i >= 0; i--)
			{
				if(m_vTasDummySwitch[i] == DummyIdx)
				{
					const vec2 LastMouse = m_vTasMouse[i];
					m_aMousePos[DummyIdx] = LastMouse;
					m_aInputData[DummyIdx] = m_vTasActions[i];
					m_aInputData[DummyIdx].m_TargetX = (int)LastMouse.x;
					m_aInputData[DummyIdx].m_TargetY = (int)LastMouse.y;
					m_aTasNextTickMouse[DummyIdx] = LastMouse;
					m_aTasLastTickMouse[DummyIdx] = LastMouse;
					break;
				}
			}
		}
	}
	ClampMousePos();

	for(const auto &Entry : m_vTasChars)
	{
		CCharacter *pChar = m_pTasWorld->GetCharacterById(Entry.m_Id);
		if(!pChar || Entry.m_Cores.empty())
			continue;

		pChar->SetCore(Entry.m_Cores.back());
		pChar->m_FreezeTime = Entry.m_FreezeTime.back();
		if(!Entry.m_ReloadTimer.empty())
			pChar->SetReloadTimer(Entry.m_ReloadTimer.back());
		pChar->m_Pos = Entry.m_Positions.back();
		pChar->m_PrevPos = Entry.m_PrevPositions.back();

		int Slot = -1;
		if(Entry.m_Id == GameClient()->m_aLocalIds[0]) Slot = 0;
		else if(Entry.m_Id == GameClient()->m_aLocalIds[1]) Slot = 1;
		if(Slot >= 0)
		{
			for(int i = (int)m_vTasTicks.size() - 1; i >= 0; i--)
			{
				if(m_vTasDummySwitch[i] == Slot)
				{
					m_aTasLastAttackTick[Slot] = m_vTasAttackTick[i];
					break;
				}
			}
		}
		
		if(pChar->GetNumInputs() < 2)
		{
			CNetObj_PlayerInput RestoreInput = m_aInputData[Slot >= 0 ? Slot : g_Config.m_ClDummy];
			pChar->OnDirectInput(&RestoreInput);
			pChar->OnDirectInput(&RestoreInput);
		}
	}
}

void CControls::DetectTasRollback()
{
	if(!g_Config.m_shalawaTasWorld || !g_Config.m_shalawaTasInit || !m_pTasWorld)
		return;
	if(m_vTasActions.size() < 2)
		return;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	CCharacter *pRealChar = GameClient()->m_GameWorld.GetCharacterById(LocalId);
	if(!pRealChar)
		return;

	const STasCharacterHistory *pHistory = TasFindCharHistory(LocalId);
	if(!pHistory || pHistory->m_Positions.size() < 2)
		return;

	static int s_LastHandledGameTick = -1;
	if(m_pTasWorld->m_GameTick == s_LastHandledGameTick)
		return;

	const int LastIndex = (int)pHistory->m_Positions.size() - 1;
	const vec2 LastTasPos = pHistory->m_Positions[LastIndex];
	const vec2 PrevTasPos = pHistory->m_Positions[LastIndex - 1];
	const vec2 RealPos = pRealChar->m_Pos;

	const float Threshold = 4.0f;
	if(distance(RealPos, PrevTasPos) < Threshold && distance(RealPos, LastTasPos) > Threshold * 1.5f)
	{
		TasStepBack();
		s_LastHandledGameTick = m_pTasWorld->m_GameTick;
	}
}

void CControls::HandleTasTeleport(CCharacter *pChar)
{
	if(!pChar || !m_pTasWorld)
		return;

	CCollision *pCollision = Collision();
	if(!pCollision)
		return;

	CCharacterCore Core = pChar->GetCore();
	vec2 Pos = Core.m_Pos;
	int MapIndex = pCollision->GetPureMapIndex(Pos);

	int TeleNumber = pCollision->IsTeleport(MapIndex);
	if(TeleNumber && !pCollision->TeleOuts(TeleNumber - 1).empty())
	{
		int TeleOut = m_pTasWorld->m_Core.RandomOr0(pCollision->TeleOuts(TeleNumber - 1).size());
		Core.m_Pos = pCollision->TeleOuts(TeleNumber - 1)[TeleOut];
		Core.m_Vel = vec2(0, 0);
		pChar->SetCore(Core);
		pChar->m_Pos = Core.m_Pos;
		pChar->m_FreezeTime = 0;
		pChar->ResetHook();
		return;
	}

	int EvilTeleNumber = pCollision->IsEvilTeleport(MapIndex);
	if(EvilTeleNumber && !pCollision->TeleOuts(EvilTeleNumber - 1).empty())
	{
		int TeleOut = m_pTasWorld->m_Core.RandomOr0(pCollision->TeleOuts(EvilTeleNumber - 1).size());
		Core.m_Pos = pCollision->TeleOuts(EvilTeleNumber - 1)[TeleOut];
		Core.m_Vel = vec2(0, 0);
		pChar->SetCore(Core);
		pChar->m_Pos = Core.m_Pos;
		pChar->m_FreezeTime = 0;
		pChar->ResetHook();
	}
}

void CControls::ApplyTasDummyHammer(CCharacter *pDummyChar, CGameWorld *pTasWorld)
{
	if(!pDummyChar || !pTasWorld)
		return;

	vec2 DummyPos = pDummyChar->m_Pos;
	int BestId = -1;
	float BestDist = 1e30f;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == pDummyChar->GetId())
			continue;

		CCharacter *pTargetChar = pTasWorld->GetCharacterById(ClientId);
		if(!pTargetChar)
			continue;

		vec2 TargetPos = pTargetChar->m_Pos;
		float Dist = distance(DummyPos, TargetPos);

		if(Dist < BestDist)
		{
			BestDist = Dist;
			BestId = ClientId;
		}
	}

	if(BestId >= 0)
	{
		CCharacter *pTargetChar = pTasWorld->GetCharacterById(BestId);
		if(pTargetChar)
		{
			vec2 TargetPos = pTargetChar->m_Pos;
			vec2 Dir = normalize(TargetPos - DummyPos);
			CNetObj_PlayerInput DummyInput = m_aInputData[!g_Config.m_ClDummy];
			DummyInput.m_TargetX = (int)(DummyPos.x + Dir.x * 1000.0f);
			DummyInput.m_TargetY = (int)(DummyPos.y + Dir.y * 1000.0f);
			pDummyChar->OnDirectInput(&DummyInput);
			pDummyChar->OnPredictedInput(&DummyInput);
		}
	}
}

int CControls::GetTasTick() const
{
	if(!m_pTasWorld)
		return 0;
	return m_pTasWorld->m_GameTick;
}

void CControls::RenderTasWorld()
{
	// Render Start/End positions FIRST (show during recording OR when TAS World is OFF)
	if(g_Config.m_shalawaTasDrawStartEndPos && !m_vTasPositions.empty() && (g_Config.m_shalawaRecording || !g_Config.m_shalawaTasWorld))
	{
		const CAnimState *pIdleState = CAnimState::GetIdle();
		
		for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
		{
			const int LocalId = GameClient()->m_aLocalIds[DummyIdx];
			if(LocalId < 0)
				continue;

			CTeeRenderInfo TeeInfo = GameClient()->m_aClients[LocalId].m_RenderInfo;
			TeeInfo.m_Size = 64.0f;

			// Find start position
			vec2 StartPos = vec2(0, 0);
			bool FoundStart = false;
			for(size_t i = 0; i < m_vTasDummySwitch.size(); i++)
			{
				if(m_vTasDummySwitch[i] == DummyIdx && i < m_vTasPositions.size())
				{
					StartPos = m_vTasPositions[i];
					FoundStart = true;
					break;
				}
			}

			if(FoundStart)
			{
				CTeeRenderInfo GreenTeeInfo = TeeInfo;
				GreenTeeInfo.m_ColorBody = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
				GreenTeeInfo.m_ColorFeet = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
				GreenTeeInfo.m_CustomColoredSkin = true;
				GameClient()->RenderTools()->RenderTee(pIdleState, &GreenTeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), StartPos);
			}

			// Find end position (only if not recording)
			if(!g_Config.m_shalawaRecording && m_vTasPositions.size() > 1)
			{
				vec2 EndPos = vec2(0, 0);
				bool FoundEnd = false;
				for(int i = (int)m_vTasDummySwitch.size() - 1; i >= 0; i--)
				{
					if(m_vTasDummySwitch[i] == DummyIdx && i < (int)m_vTasPositions.size())
					{
						EndPos = m_vTasPositions[i];
						FoundEnd = true;
						break;
					}
				}

				if(FoundEnd)
				{
					CTeeRenderInfo PurpleTeeInfo = TeeInfo;
					PurpleTeeInfo.m_ColorBody = ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f);
					PurpleTeeInfo.m_ColorFeet = ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f);
					PurpleTeeInfo.m_CustomColoredSkin = true;
					GameClient()->RenderTools()->RenderTee(pIdleState, &PurpleTeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), EndPos);
				}
			}
		}
	}

	// Render replay path (show during playback only)
	if(g_Config.m_shalawaTasDrawPath && !m_vTasPositions.empty() && g_Config.m_shalawaPlaying && !g_Config.m_shalawaRecording)
	{
		Graphics()->TextureClear();
		ColorRGBA PathCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasDrawPathColor));
		const float PathWidth = (float)g_Config.m_shalawaTasDrawPathWidth;

		for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
		{
			std::vector<vec2> FilteredPath;
			for(size_t i = 0; i < m_vTasDummySwitch.size() && i < m_vTasPositions.size(); i++)
			{
				if(m_vTasDummySwitch[i] == DummyIdx)
					FilteredPath.push_back(m_vTasPositions[i]);
			}

			if(FilteredPath.empty())
				continue;

			const int NumPoints = (int)FilteredPath.size();
			const int StartIdx = std::max(0, NumPoints - 200);
			const int EndIdx = NumPoints - 1;

			// Always segmented full line: erase older path behind the player.
			Graphics()->LinesBegin();
			Graphics()->SetColor(PathCol.r, PathCol.g, PathCol.b, PathCol.a);
			for(int i = StartIdx; i < EndIdx; i++)
			{
				vec2 Pos1 = FilteredPath[i];
				vec2 Pos2 = FilteredPath[i + 1];
				IGraphics::CLineItem LineItem(Pos1.x, Pos1.y, Pos2.x, Pos2.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			Graphics()->LinesEnd();
		}
	}

	// Render Auto path and tiles
	if(g_Config.m_shalawaTasAutoMode == 1 && !m_TasAuto.m_PathPositions.empty())
	{
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(0.2f, 0.9f, 0.7f, 0.9f);
		for(size_t i = 1; i < m_TasAuto.m_PathPositions.size(); i++)
		{
			const vec2 Pos1 = m_TasAuto.m_PathPositions[i - 1];
			const vec2 Pos2 = m_TasAuto.m_PathPositions[i];
			IGraphics::CLineItem LineItem(Pos1.x, Pos1.y, Pos2.x, Pos2.y);
			Graphics()->LinesDraw(&LineItem, 1);
		}
		Graphics()->LinesEnd();

		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.1f, 0.6f, 1.0f, 0.4f);
		for(const auto &TilePos : m_TasAuto.m_PathTiles)
		{
			const float TileSize = 32.0f;
			Graphics()->DrawRectExt(TilePos.x - TileSize * 0.5f, TilePos.y - TileSize * 0.5f, TileSize, TileSize, 0.0f, IGraphics::CORNER_NONE);
		}
		Graphics()->QuadsEnd();

		if(m_TasAuto.m_FieldReady && !m_TasAuto.m_PathTileIndices.empty())
		{
			Graphics()->LinesBegin();
			Graphics()->SetColor(1.0f, 0.85f, 0.2f, 0.9f);
			const float ArrowLen = 10.0f;
			for(size_t i = 0; i < m_TasAuto.m_PathTileIndices.size(); i++)
			{
				const int Index = m_TasAuto.m_PathTileIndices[i];
				if(Index < 0 || Index >= (int)m_TasAuto.m_FieldDirX.size())
					continue;
				const int DirX = m_TasAuto.m_FieldDirX[Index];
				const int DirY = m_TasAuto.m_FieldDirY[Index];
				if(DirX == 0 && DirY == 0)
					continue;
				vec2 Center = m_TasAuto.m_PathTiles[i];
				vec2 Dir = normalize(vec2((float)DirX, (float)DirY));
				vec2 Tip = Center + Dir * ArrowLen;
				IGraphics::CLineItem Shaft(Center.x, Center.y, Tip.x, Tip.y);
				Graphics()->LinesDraw(&Shaft, 1);

				vec2 Left = Tip + vec2(-Dir.y, Dir.x) * 4.0f - Dir * 4.0f;
				vec2 Right = Tip + vec2(Dir.y, -Dir.x) * 4.0f - Dir * 4.0f;
				IGraphics::CLineItem HeadL(Tip.x, Tip.y, Left.x, Left.y);
				IGraphics::CLineItem HeadR(Tip.x, Tip.y, Right.x, Right.y);
				Graphics()->LinesDraw(&HeadL, 1);
				Graphics()->LinesDraw(&HeadR, 1);
			}
			Graphics()->LinesEnd();
		}
	}

	// Render prediction path (BEFORE TAS World rendering, using proper TAS World simulation)
	if(g_Config.m_shalawaPredictTas && g_Config.m_shalawaRecording && g_Config.m_shalawaTasWorld && m_pTasWorld)
	{
		for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; DummyIdx++)
		{
			const int LocalId = GameClient()->m_aLocalIds[DummyIdx];
			if(LocalId < 0)
				continue;

			CCharacter *pTasChar = m_pTasWorld->GetCharacterById(LocalId);
			if(!pTasChar)
				continue;

			// Use temporary world to simulate prediction
			static CGameWorld s_TempWorld;
			s_TempWorld.CopyWorld(m_pTasWorld);
			CCharacter *pTempChar = s_TempWorld.GetCharacterById(LocalId);
			if(!pTempChar)
				continue;

			std::vector<vec2> Trajectory;
			CNetObj_PlayerInput TempInput = m_aInputData[DummyIdx];
			int OriginalGameTick = s_TempWorld.m_GameTick;

			// Simulate 30 ticks forward
			for(int t = 0; t < 30; ++t)
			{
				Trajectory.push_back(pTempChar->m_Pos);
				s_TempWorld.m_GameTick++;
				pTempChar->OnPredictedInput(&TempInput);
				s_TempWorld.Tick();
			}
			Trajectory.push_back(pTempChar->m_Pos);

			s_TempWorld.m_GameTick = OriginalGameTick;

			if(Trajectory.empty())
				continue;

			Graphics()->TextureClear();
			Graphics()->LinesBegin();
			ColorRGBA PredictCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasPredictColor));
			ColorRGBA FrozenCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_shalawaTasPredictColorFrozen));

			for(size_t j = 1; j < Trajectory.size(); ++j)
			{
				vec2 Start = Trajectory[j - 1];
				vec2 End = Trajectory[j];
				ColorRGBA LineColor = PredictCol;

				if(Collision())
				{
					int TileStart = Collision()->GetTileIndex(Collision()->GetPureMapIndex(Start));
					if(TileStart == TILE_FREEZE || TileStart == TILE_DFREEZE || TileStart == TILE_LFREEZE)
						LineColor = FrozenCol;
				}

				Graphics()->SetColor(LineColor.r, LineColor.g, LineColor.b, LineColor.a);
				IGraphics::CLineItem LineItem(Start.x, Start.y, End.x, End.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			Graphics()->LinesEnd();
		}
	}

	// Render TAS World characters LAST (show when recording OR when TAS World is enabled)
	if(!g_Config.m_shalawaTasInit || !m_pTasWorld)
		return;
	
	// Only render TAS characters when recording or TAS World is explicitly enabled
	if(!g_Config.m_shalawaRecording && !g_Config.m_shalawaTasWorld)
		return;

	float Intra = std::clamp(m_TasSmoothIntra, 0.0f, 1.0f);

	// Structure to hold render data for each TAS character
	struct STasRenderData
	{
		int m_ClientId;
		CNetObj_Character m_PrevObj;
		CNetObj_Character m_CurObj;
		CTeeRenderInfo m_TeeInfo;
		vec2 m_PrevPos;
		vec2 m_CurPos;
		bool m_Frozen;
		float m_FreezeProgress;
	};

	std::vector<STasRenderData> aTasRenderData;
	aTasRenderData.reserve(m_vTasChars.size());

	// First pass: collect all render data
	for(const auto &Entry : m_vTasChars)
	{
		const int ClientId = Entry.m_Id;
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			continue;

		CCharacter *pCharCommitted = m_pTasWorld->GetCharacterById(ClientId);
		CCharacter *pCharPredicted = m_pTasWorldPredicted ? m_pTasWorldPredicted->GetCharacterById(ClientId) : nullptr;
		if(!pCharCommitted)
			continue;

		STasRenderData Data;
		Data.m_ClientId = ClientId;

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;
		TeeInfo.m_Size = 64.0f;
		TeeInfo.m_TeeRenderFlags = 0;

		const CCharacterCore &PrevCore = *pCharCommitted->Core();
		const CCharacterCore &CurCore = pCharPredicted ? *pCharPredicted->Core() : PrevCore;
		Data.m_PrevPos = pCharCommitted->m_Pos;
		Data.m_CurPos = pCharPredicted ? pCharPredicted->m_Pos : Data.m_PrevPos;

		Data.m_Frozen = CurCore.m_FreezeEnd != 0 || CurCore.m_DeepFrozen;
		if(CurCore.m_FreezeEnd != 0 || CurCore.m_DeepFrozen)
			TeeInfo.m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
		if(CurCore.m_LiveFrozen)
			TeeInfo.m_TeeRenderFlags |= TEE_EFFECT_FROZEN;

		if((CurCore.m_ActiveWeapon == WEAPON_NINJA || (Data.m_Frozen && !GameClient()->m_GameInfo.m_NoSkinChangeForFrozen)) && g_Config.m_ClShowNinja)
		{
			const bool IsTeamPlay = GameClient()->IsTeamPlay();
			TeeInfo.m_aSixup[g_Config.m_ClDummy].Reset();
			TeeInfo.ApplySkin(GameClient()->m_Players.NinjaTeeRenderInfo()->TeeRenderInfo());
			TeeInfo.m_CustomColoredSkin = IsTeamPlay;
			if(!IsTeamPlay)
			{
				TeeInfo.m_ColorBody = ColorRGBA(1, 1, 1);
				TeeInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
			}
		}

		Data.m_TeeInfo = TeeInfo;

		// Build CNetObj_Character for proper rendering with hooks and weapons
		Data.m_PrevObj.m_X = (int)Data.m_PrevPos.x;
		Data.m_PrevObj.m_Y = (int)Data.m_PrevPos.y;
		Data.m_PrevObj.m_VelX = (int)(PrevCore.m_Vel.x * 256.0f);
		Data.m_PrevObj.m_VelY = (int)(PrevCore.m_Vel.y * 256.0f);
		
		// Use interpolated mouse for angle
		int DummyIdx = -1;
		if(ClientId == GameClient()->m_aLocalIds[0]) DummyIdx = 0;
		else if(ClientId == GameClient()->m_aLocalIds[1]) DummyIdx = 1;
		vec2 InterpMouse = (DummyIdx >= 0) ? GetInterpolatedMouseForRendering(DummyIdx) : vec2(1.0f, 0.0f);
		if(length(InterpMouse) < 0.001f)
			InterpMouse = vec2(1.0f, 0.0f);
		Data.m_PrevObj.m_Angle = (int)(angle(normalize(InterpMouse)) * 256.0f);
		
		Data.m_PrevObj.m_Direction = PrevCore.m_Direction;
		Data.m_PrevObj.m_Jumped = PrevCore.m_Jumped;
		Data.m_PrevObj.m_HookState = PrevCore.m_HookState;
		Data.m_PrevObj.m_HookX = (int)PrevCore.m_HookPos.x;
		Data.m_PrevObj.m_HookY = (int)PrevCore.m_HookPos.y;
		Data.m_PrevObj.m_HookedPlayer = PrevCore.HookedPlayer();
		Data.m_PrevObj.m_AttackTick = 0;
		Data.m_PrevObj.m_Weapon = PrevCore.m_ActiveWeapon;

		Data.m_CurObj.m_X = (int)Data.m_CurPos.x;
		Data.m_CurObj.m_Y = (int)Data.m_CurPos.y;
		Data.m_CurObj.m_VelX = (int)(CurCore.m_Vel.x * 256.0f);
		Data.m_CurObj.m_VelY = (int)(CurCore.m_Vel.y * 256.0f);
		Data.m_CurObj.m_Angle = Data.m_PrevObj.m_Angle;
		Data.m_CurObj.m_Direction = CurCore.m_Direction;
		Data.m_CurObj.m_Jumped = CurCore.m_Jumped;
		Data.m_CurObj.m_HookState = CurCore.m_HookState;
		Data.m_CurObj.m_HookX = (int)CurCore.m_HookPos.x;
		Data.m_CurObj.m_HookY = (int)CurCore.m_HookPos.y;
		Data.m_CurObj.m_HookedPlayer = CurCore.HookedPlayer();
		Data.m_CurObj.m_AttackTick = 0;
		Data.m_CurObj.m_Weapon = CurCore.m_ActiveWeapon;

		// Handle hooked players
		if(Data.m_PrevObj.m_HookedPlayer >= 0 && Data.m_PrevObj.m_HookedPlayer < MAX_CLIENTS)
		{
			if(CCharacter *pHooked = m_pTasWorld->GetCharacterById(Data.m_PrevObj.m_HookedPlayer))
			{
				Data.m_PrevObj.m_HookX = (int)pHooked->m_Pos.x;
				Data.m_PrevObj.m_HookY = (int)pHooked->m_Pos.y;
			}
		}
		if(Data.m_CurObj.m_HookedPlayer >= 0 && Data.m_CurObj.m_HookedPlayer < MAX_CLIENTS)
		{
			if(CCharacter *pHooked = m_pTasWorldPredicted ? m_pTasWorldPredicted->GetCharacterById(Data.m_CurObj.m_HookedPlayer) : m_pTasWorld->GetCharacterById(Data.m_CurObj.m_HookedPlayer))
			{
				Data.m_CurObj.m_HookX = (int)pHooked->m_Pos.x;
				Data.m_CurObj.m_HookY = (int)pHooked->m_Pos.y;
			}
		}

		// Set attack tick for local characters
		if(DummyIdx >= 0 && g_Config.m_shalawaRecording)
		{
			Data.m_CurObj.m_AttackTick = m_aTasLastAttackTick[DummyIdx];
			Data.m_PrevObj.m_AttackTick = m_aTasLastAttackTick[DummyIdx];
		}

		// Calculate freeze progress
		Data.m_FreezeProgress = 0.0f;
		if(Data.m_Frozen && CurCore.m_FreezeEnd > 0 && CurCore.m_FreezeStart > 0 && CurCore.m_FreezeEnd > CurCore.m_FreezeStart)
		{
			const int Max = CurCore.m_FreezeEnd - CurCore.m_FreezeStart;
			const int TickNow = m_pTasWorld ? m_pTasWorld->m_GameTick : Client()->GameTick(g_Config.m_ClDummy);
			Data.m_FreezeProgress = std::clamp(Max - (TickNow - CurCore.m_FreezeStart), 0, Max) / (float)Max;
		}

		aTasRenderData.push_back(Data);
	}

	// Second pass: render all hooks first (like in normal game)
	constexpr int TasClientId = -3;
	for(const auto &Data : aTasRenderData)
	{
		GameClient()->m_Players.RenderHook(&Data.m_PrevObj, &Data.m_CurObj, &Data.m_TeeInfo, TasClientId, Intra);
	}

	// Third pass: render all players (this includes freeze effect inside RenderPlayer)
	// Note: Hook collision lines are skipped for TAS (ClientId == -3) as they require valid client data
	for(const auto &Data : aTasRenderData)
	{
		GameClient()->m_Players.RenderPlayer(&Data.m_PrevObj, &Data.m_CurObj, &Data.m_TeeInfo, TasClientId, Intra);
	}

	// Note: Freeze bars are rendered in CFreezeBars::OnRender() (after MapLayersForeground)
	// to ensure they appear above all tiles and freeze effects

	// "Show Real Aim" overlay removed (was yellow aim line).
}

void CControls::RenderTasFreezeBars()
{
	if(!g_Config.m_shalawaTasInit || !m_pTasWorld)
		return;
	
	// Only render TAS freeze bars when recording or TAS World is explicitly enabled
	if(!g_Config.m_shalawaRecording && !g_Config.m_shalawaTasWorld)
		return;

	float Intra = std::clamp(m_TasSmoothIntra, 0.0f, 1.0f);

	// Render freeze bars for all TAS characters
	for(const auto &Entry : m_vTasChars)
	{
		const int ClientId = Entry.m_Id;
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			continue;

		CCharacter *pCharCommitted = m_pTasWorld->GetCharacterById(ClientId);
		CCharacter *pCharPredicted = m_pTasWorldPredicted ? m_pTasWorldPredicted->GetCharacterById(ClientId) : nullptr;
		if(!pCharCommitted)
			continue;

		const CCharacterCore &CurCore = pCharPredicted ? *pCharPredicted->Core() : *pCharCommitted->Core();
		const vec2 PrevPos = pCharCommitted->m_Pos;
		const vec2 CurPos = pCharPredicted ? pCharPredicted->m_Pos : PrevPos;

		const bool Frozen = CurCore.m_FreezeEnd != 0 || CurCore.m_DeepFrozen;
		if(Frozen && CurCore.m_FreezeEnd > 0 && CurCore.m_FreezeStart > 0 && CurCore.m_FreezeEnd > CurCore.m_FreezeStart)
		{
			const int Max = CurCore.m_FreezeEnd - CurCore.m_FreezeStart;
			const int TickNow = m_pTasWorld ? m_pTasWorld->m_GameTick : Client()->GameTick(g_Config.m_ClDummy);
			const float FreezeProgress = std::clamp(Max - (TickNow - CurCore.m_FreezeStart), 0, Max) / (float)Max;
			if(FreezeProgress > 0.0f)
			{
				const float FreezeBarWidth = 64.0f;
				const float FreezeBarHalfWidth = 32.0f;
				const float FreezeBarHeight = 16.0f;
				const vec2 RenderPos = mix(PrevPos, CurPos, Intra);
				const vec2 BarPos(RenderPos.x - FreezeBarHalfWidth, RenderPos.y + 32.0f);
				GameClient()->m_FreezeBars.RenderFreezeBarPos(BarPos.x, BarPos.y, FreezeBarWidth, FreezeBarHeight, FreezeProgress, 1.0f);
			}
		}
	}
}

vec2 CControls::GetInterpolatedMouseForRendering(int Dummy)
{
	// Keep cursor feel identical to normal gameplay while recording.
	if(g_Config.m_shalawaRecording)
		return m_aMousePos[Dummy];
	// Use TAS interpolated mouse when TAS World is explicitly enabled (non-recording).
	if(g_Config.m_shalawaTasWorld && g_Config.m_shalawaTasMode && g_Config.m_shalawaTasInit && m_pTasWorld)
		return m_aTasInterpMouse[Dummy];
	return m_aMousePos[Dummy];
}

vec2 CControls::GetInterpolatedPlayerPosition(int Dummy)
{
	if(!GameClient())
		return vec2(0.0f, 0.0f);
	
	// Use TAS World position when recording (TAS World is auto-enabled during recording) or when TAS World is explicitly enabled
	if((!g_Config.m_shalawaRecording && !g_Config.m_shalawaTasWorld) || !g_Config.m_shalawaTasMode || !g_Config.m_shalawaTasInit)
		return GameClient()->m_LocalCharacterPos;

	if(!m_pTasWorld)
		return GameClient()->m_LocalCharacterPos;

	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return GameClient()->m_LocalCharacterPos;

	CCharacter *pCharCommitted = m_pTasWorld->GetCharacterById(LocalId);
	if(!pCharCommitted)
		return GameClient()->m_LocalCharacterPos;

	// Use predicted world for smooth interpolation if available
	if(m_pTasWorldPredicted)
	{
		CCharacter *pCharPredicted = m_pTasWorldPredicted->GetCharacterById(LocalId);
		if(pCharPredicted)
		{
			const vec2 PrevPos = pCharCommitted->m_Pos;
			const vec2 CurPos = pCharPredicted->m_Pos;
			float Intra = std::clamp(m_TasSmoothIntra, 0.0f, 1.0f);
			return mix(PrevPos, CurPos, Intra);
		}
	}

	// Fallback to committed position
	return pCharCommitted->m_Pos;
}

namespace
{
	const char *TAS_DIR = "shalawa.xyz";
	const char *TAS_EXT = ".tas";
	const int TAS_FILE_VERSION = 1;
}

void CControls::SaveRecordsToFile(const std::string &Filename)
{
	if(Filename.empty() || !GameClient() || !GameClient()->Storage())
		return;

	GameClient()->Storage()->CreateFolder(TAS_DIR, IStorage::TYPE_SAVE);

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s%s", TAS_DIR, Filename.c_str(), TAS_EXT);
	IOHANDLE File = GameClient()->Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const size_t Count = std::min({m_vTasTicks.size(), m_vTasActions.size(), m_vTasMouse.size(), m_vTasDummySwitch.size(), m_vTasKill.size()});

	const char aMagic[4] = {'T', 'A', 'S', '1'};
	const int Version = TAS_FILE_VERSION;
	const int NumEntries = (int)Count;
	io_write(File, aMagic, sizeof(aMagic));
	io_write(File, &Version, sizeof(Version));
	io_write(File, &NumEntries, sizeof(NumEntries));

	for(size_t i = 0; i < Count; i++)
	{
		const int Tick = m_vTasTicks[i];
		const int Dummy = m_vTasDummySwitch[i];
		const int Kill = m_vTasKill[i];
		const int AttackTick = i < m_vTasAttackTick.size() ? m_vTasAttackTick[i] : -1;
		const CNetObj_PlayerInput Input = m_vTasActions[i];
		const vec2 Mouse = m_vTasMouse[i];
		const vec2 Pos = i < m_vTasPositions.size() ? m_vTasPositions[i] : vec2(0.0f, 0.0f);

		io_write(File, &Tick, sizeof(Tick));
		io_write(File, &Dummy, sizeof(Dummy));
		io_write(File, &Kill, sizeof(Kill));
		io_write(File, &AttackTick, sizeof(AttackTick));
		io_write(File, &Input, sizeof(Input));
		io_write(File, &Mouse, sizeof(Mouse));
		io_write(File, &Pos, sizeof(Pos));
	}

	io_close(File);
}

void CControls::LoadRecordsFromFile(const std::string &Filename)
{
	if(Filename.empty() || !GameClient() || !GameClient()->Storage())
		return;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s%s", TAS_DIR, Filename.c_str(), TAS_EXT);
	IOHANDLE File = GameClient()->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aMagic[4] = {};
	int Version = 0;
	int NumEntries = 0;
	if(io_read(File, aMagic, sizeof(aMagic)) != sizeof(aMagic) ||
		io_read(File, &Version, sizeof(Version)) != sizeof(Version) ||
		io_read(File, &NumEntries, sizeof(NumEntries)) != sizeof(NumEntries))
	{
		io_close(File);
		return;
	}

	if(mem_comp(aMagic, "TAS1", 4) != 0 || Version != TAS_FILE_VERSION || NumEntries < 0)
	{
		io_close(File);
		return;
	}

	ClearTasRecords();
	m_vTasTicks.reserve(NumEntries);
	m_vTasActions.reserve(NumEntries);
	m_vTasMouse.reserve(NumEntries);
	m_vTasDummySwitch.reserve(NumEntries);
	m_vTasKill.reserve(NumEntries);
	m_vTasAttackTick.reserve(NumEntries);
	m_vTasPositions.reserve(NumEntries);

	for(int i = 0; i < NumEntries; i++)
	{
		int Tick = 0;
		int Dummy = 0;
		int Kill = 0;
		int AttackTick = -1;
		CNetObj_PlayerInput Input{};
		vec2 Mouse(0.0f, 0.0f);
		vec2 Pos(0.0f, 0.0f);

		if(io_read(File, &Tick, sizeof(Tick)) != sizeof(Tick) ||
			io_read(File, &Dummy, sizeof(Dummy)) != sizeof(Dummy) ||
			io_read(File, &Kill, sizeof(Kill)) != sizeof(Kill) ||
			io_read(File, &AttackTick, sizeof(AttackTick)) != sizeof(AttackTick) ||
			io_read(File, &Input, sizeof(Input)) != sizeof(Input) ||
			io_read(File, &Mouse, sizeof(Mouse)) != sizeof(Mouse) ||
			io_read(File, &Pos, sizeof(Pos)) != sizeof(Pos))
		{
			io_close(File);
			ClearTasRecords();
			return;
		}

		m_vTasTicks.push_back(Tick);
		m_vTasActions.push_back(Input);
		m_vTasMouse.push_back(Mouse);
		m_vTasDummySwitch.push_back(Dummy);
		m_vTasKill.push_back(Kill);
		m_vTasAttackTick.push_back(AttackTick);
		m_vTasPositions.push_back(Pos);
	}

	io_close(File);
	TasPlaybackReset();
	g_Config.m_shalawaRecording = 0;
	g_Config.m_shalawaPlaying = 0;
}

void CControls::DeleteRecord(const std::string &Filename)
{
	if(Filename.empty() || !GameClient() || !GameClient()->Storage())
		return;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s%s", TAS_DIR, Filename.c_str(), TAS_EXT);
	GameClient()->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
}

void CControls::SaveRecord()
{
	const char *pName = g_Config.m_shalawaRecordName[0] ? g_Config.m_shalawaRecordName : "default";
	SaveRecordsToFile(pName);
}

void CControls::ClearTasRecords()
{
	m_vTasTicks.clear();
	m_vTasActions.clear();
	m_vTasMouse.clear();
	m_vTasDummySwitch.clear();
	m_vTasKill.clear();
	m_vTasAttackTick.clear();
	m_vTasPositions.clear();
	m_vTasChars.clear();
	TasPlaybackReset();
}

void CControls::RenderTasLoadDropdown(int *pCurrentValue)
{
	// Placeholder: UI handled by Shalawa Menu.
	(void)pCurrentValue;
}

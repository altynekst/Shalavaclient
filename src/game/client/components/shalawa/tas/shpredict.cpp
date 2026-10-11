#include "shpredict.h"
#include "shworld.h"

#include <base/vmath.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/collision.h>
#include <game/mapitems.h>

CTasPredict::CTasPredict()
{
	m_pTasWorld = nullptr;
}

void CTasPredict::OnReset()
{
}

void CTasPredict::PredictPath(int ClientId, int Ticks, std::vector<vec2> &vPath)
{
	vPath.clear();

	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return;

	CGameWorld *pWorld = m_pTasWorld->World();
	if(!pWorld)
		return;

	CCharacter *pChar = pWorld->GetCharacterById(ClientId);
	if(!pChar)
		return;

	static CGameWorld s_TempWorld;
	s_TempWorld.CopyWorld(pWorld);

	CCharacter *pTempChar = s_TempWorld.GetCharacterById(ClientId);
	if(!pTempChar)
		return;

	const int Dummy = g_Config.m_ClDummy;
	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[Dummy];

	for(int t = 0; t < Ticks; t++)
	{
		vPath.push_back(pTempChar->GetPos());
		s_TempWorld.m_GameTick++;
		pTempChar->OnPredictedInput(&Input);
		s_TempWorld.Tick();
	}
	vPath.push_back(pTempChar->GetPos());
}

bool CTasPredict::WillFreeze(int ClientId, int Ticks)
{
	if(!m_pTasWorld || !m_pTasWorld->IsInitialized())
		return false;
	if(!Collision())
		return false;

	CGameWorld *pWorld = m_pTasWorld->World();
	if(!pWorld)
		return false;

	CCharacter *pChar = pWorld->GetCharacterById(ClientId);
	if(!pChar)
		return false;

	static CGameWorld s_TempWorld;
	s_TempWorld.CopyWorld(pWorld);

	CCharacter *pTempChar = s_TempWorld.GetCharacterById(ClientId);
	if(!pTempChar)
		return false;

	const int Dummy = g_Config.m_ClDummy;
	CNetObj_PlayerInput Input = GameClient()->m_Controls.m_aInputData[Dummy];

	for(int t = 0; t < Ticks; t++)
	{
		s_TempWorld.m_GameTick++;
		pTempChar->OnPredictedInput(&Input);
		s_TempWorld.Tick();

		vec2 Pos = pTempChar->GetPos();
		int MapIndex = Collision()->GetPureMapIndex(Pos);
		int TileIndex = Collision()->GetTileIndex(MapIndex);
		int TileFIndex = Collision()->GetFrontTileIndex(MapIndex);

		if(TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE ||
		   TileIndex == TILE_DFREEZE || TileFIndex == TILE_DFREEZE ||
		   TileIndex == TILE_LFREEZE || TileFIndex == TILE_LFREEZE)
			return true;

		if(pTempChar->m_FreezeTime > 0)
			return true;
	}

	return false;
}
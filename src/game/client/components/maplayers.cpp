#include "maplayers.h"
#include <game/client/gameclient.h>
#include <game/localization.h>

CMapLayers::CMapLayers(ERenderType Type, bool OnlineOnly) :
	m_Type(Type), m_OnlineOnly(OnlineOnly)
{
	m_Params.m_RenderType = m_Type;
	m_Params.m_RenderInvalidTiles = false;
	m_Params.m_TileAndQuadBuffering = true;
	m_Params.m_RenderTileBorder = true;
}

void CMapLayers::OnInit()
{
	m_pLayers = Layers();
	m_pImages = &GameClient()->m_MapImages;
	m_MapRenderer.Clear();
}

CCamera *CMapLayers::GetCurCamera() { return &GameClient()->m_Camera; }

void CMapLayers::OnMapLoad()
{
	FRenderUploadCallback CB = [&](const char *pT, const char *pM, int Inc) { GameClient()->m_Menus.RenderLoading(pT, pM, Inc); };
	auto CBOpt = std::make_optional<FRenderUploadCallback>(CB);
	GameClient()->m_Menus.RenderLoading(Localize("Loading map"), Localize("Uploading map data to GPU"), 0);
	m_MapRenderer.OnInit(Graphics(), TextRender(), RenderMap());
	m_EnvEvaluator = CEnvelopeState(m_pLayers->Map(), m_OnlineOnly);
	m_EnvEvaluator.OnInterfacesInit(GameClient());
	m_MapRenderer.Load(m_Type, m_pLayers, m_pImages, &m_EnvEvaluator, CBOpt);
}

void CMapLayers::OnRender()
{
	if(m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	auto *pCam = GetCurCamera();
	m_Params.m_EntityOverlayVal = m_Type == RENDERTYPE_FULL_DESIGN ? 0 : g_Config.m_ClOverlayEntities;
	m_Params.m_Center = pCam->m_Center;
	m_Params.m_Zoom = pCam->m_Zoom;
	m_Params.m_RenderText = g_Config.m_ClTextEntities;
	m_Params.m_DebugRenderGroupClips = g_Config.m_DbgRenderGroupClips;
	m_Params.m_DebugRenderQuadClips = g_Config.m_DbgRenderQuadClips;
	m_Params.m_DebugRenderClusterClips = g_Config.m_DbgRenderClusterClips;
	m_Params.m_DebugRenderTileClips = g_Config.m_DbgRenderTileClips;
	m_MapRenderer.Render(m_Params);
}
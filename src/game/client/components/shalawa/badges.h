#pragma once

#include <base/system.h>
#include <engine/graphics.h>
#include <engine/shared/http.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>

class IHttp;
class IStorage;
class IGraphics;
class CGameClient;

#define BADGES_BASE_URL "http://93.88.206.9/"
#define BADGES_DATA_URL BADGES_BASE_URL "badges.json"
#define BADGES_TEXTURES_URL BADGES_BASE_URL "icons/"
#define BADGES_HEARTBEAT_URL BADGES_BASE_URL "backend/index.php?action=heartbeat"

struct SGifFrame
{
	IGraphics::CTextureHandle m_Texture;
	int m_DelayMs;
};

struct CBadgeDef
{
	char m_aId[64];
	char m_aName[128];
	char m_aTextureUrl[512];
	IGraphics::CTextureHandle m_Texture;
	bool m_TextureLoaded;
	bool m_IsRestricted;
	bool m_IsAnimated;
	std::vector<SGifFrame> m_vGifFrames;
	float m_AnimTimer;
	int m_CurrentFrame;

	CBadgeDef()
	{
		m_aId[0] = '\0';
		m_aName[0] = '\0';
		m_aTextureUrl[0] = '\0';
		m_Texture = IGraphics::CTextureHandle();
		m_TextureLoaded = false;
		m_IsRestricted = false;
		m_IsAnimated = false;
		m_AnimTimer = 0.0f;
		m_CurrentFrame = 0;
	}
};

struct SDecodeTask
{
	CBadgeDef *m_pBadge;
	unsigned char *m_pData;
	size_t m_DataSize;
	char m_aCachePath[512];
	bool m_IsPreDecoded;
};

struct SDecodedFrame
{
	uint8_t *m_pData;
	int m_Width;
	int m_Height;
	int m_DelayMs;
};

struct SDecodedBadge
{
	CBadgeDef *m_pBadge;
	std::vector<SDecodedFrame> m_vFrames;
	bool m_IsGif;
	uint8_t *m_pStaticData;
	int m_StaticWidth;
	int m_StaticHeight;
	char m_aCachePath[512];
};

struct SOnlinePlayer
{
	char m_aNickname[64];
	char m_aServer[64];
	char m_aBadgeId[64];
	bool m_Verified;
};

class CBadgeManager
{
public:
	CBadgeManager();
	~CBadgeManager();

	void Init(IGraphics *pGraphics, IStorage *pStorage, IHttp *pHttp, CGameClient *pGameClient);
	void SetLocalHwid(const char *pHwid);
	void Update(float DeltaTime);

	IGraphics::CTextureHandle GetCurrentBadgeTexture(const CBadgeDef *pBadge) const;
	const CBadgeDef *GetBadgeById(const char *pId) const;
	const CBadgeDef *GetPlayerBadge(const char *pName) const;
	bool IsPlayerVerified(const char *pName) const;
	bool IsLocalVerified() const { return m_IsLocalVerified; }

	void RenderBadge(const CBadgeDef *pBadge, float x, float y, float w, float h);
	void RenderVerifiedBadge(float x, float y, float w, float h);

	bool IsLoaded() const { return m_Loaded; }
	const std::vector<CBadgeDef> &GetPublicBadges() const { return m_vPublicBadges; }
	const CBadgeDef *GetVerifiedBadge() const
	{
		if(m_VerifiedBadge.m_TextureLoaded)
			return &m_VerifiedBadge;
		return nullptr;
	}

	void SetSelectedBadge(const char *pId);
	const char *GetSelectedBadgeId() const { return m_aSelectedBadgeId; }
	const CBadgeDef *GetSelectedBadge() const;

	void ForceHeartbeat()
	{
		if(m_HeartbeatPending)
			return;
		m_HeartbeatTimer = 0.0f;
		SendHeartbeat();
	}
	bool m_InitialHeartbeatDone;
	const CBadgeDef *GetOnlinePlayerBadge(const char *pNickname) const;
	const std::vector<SOnlinePlayer> &GetOnlinePlayers() const { return m_vOnlinePlayers; }
	bool IsOnlinePlayerVerified(const char *pNickname) const;
	bool IsFullDownloadRequested() const { return m_FullDownloadRequested; }
	void RequestFullDownload();
	void SetGameClient(CGameClient *pGameClient) { m_pGameClient = pGameClient; }
	bool IsInitialHeartbeatDone() const { return m_InitialHeartbeatDone; }

private:
	void StartFetch();
	void ProcessBadgesResponse();
	void ParseBadges(const char *pData, size_t Size);
	bool LoadImageFromMemory(const unsigned char *pData, size_t DataSize, CBadgeDef &Badge, const char *pCachePath);
	bool LoadGifFromMemory(const unsigned char *pData, size_t DataSize, CBadgeDef &Badge);
	bool IsGifData(const unsigned char *pData, size_t DataSize) const;
	void FetchTextures();
	void ProcessTextures();
	void SendHeartbeat();
	bool m_FullDownloadRequested;
	std::unordered_set<std::string> m_vQueuedForDownload;

	void QueueBadgeDownload(CBadgeDef &Badge);
	void CheckAndFetchMissingOnlineBadges();
	void ProcessHeartbeatResponse();
	void TryLoadCached(CBadgeDef &Badge);
	void CheckLocalVerified();
	std::string HashHwid(const char *pHwid) const;

	void UpdateAnimations(float DeltaTime);
	void DecodeThreadFunc();
	void ProcessDecodedBadges();

	void SaveGifCache(const char *pId, const std::vector<SDecodedFrame> &vFrames, int Width, int Height);
	bool ParseGifCache(const unsigned char *pData, size_t DataSize, SDecodedBadge &Decoded);

	void SaveSelectedBadge();
	void LoadSelectedBadge();

	CGameClient *m_pGameClient;
	IGraphics *m_pGraphics;
	IStorage *m_pStorage;
	IHttp *m_pHttp;

	std::vector<CBadgeDef> m_vPublicBadges;
	CBadgeDef m_VerifiedBadge;

	std::unordered_map<std::string, std::string> m_mPlayerBadge;
	std::unordered_set<std::string> m_VerifiedHwids;

	char m_aLocalHwid[128];
	bool m_IsLocalVerified;
	char m_aSelectedBadgeId[64];
	char m_aGifCacheDir[512];

	enum EFetchState
	{
		STATE_INIT,
		STATE_FETCHING_BADGES,
		STATE_FETCHING_TEXTURES,
		STATE_DONE
	};
	EFetchState m_FetchState;

	std::shared_ptr<CHttpRequest> m_pCurrentRequest;

	struct SPendingTexture
	{
		std::shared_ptr<CHttpRequest> m_pRequest;
		CBadgeDef *m_pBadge;
	};
	std::vector<SPendingTexture> m_vPendingTextures;

	bool m_Loaded;

	float m_HeartbeatTimer;
	static constexpr float HEARTBEAT_INTERVAL = 3.0f;
	std::shared_ptr<CHttpRequest> m_pHeartbeatRequest;
	bool m_HeartbeatPending;

	std::vector<SOnlinePlayer> m_vOnlinePlayers;

	std::mutex m_TaskMutex;
	std::queue<SDecodeTask> m_qDecodeTasks;
	std::mutex m_DecodedMutex;
	std::queue<SDecodedBadge> m_qDecodedBadges;
	std::thread m_DecodeThread;
	bool m_DecodeThreadRunning;
	static constexpr int MAX_TEXTURES_PER_FRAME = 10;
};
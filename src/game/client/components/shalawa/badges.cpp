#include "badges.h"

#include <base/log.h>
#include <base/system.h>
#include <base/hash_ctxt.h>

#include <engine/graphics.h>
#include <engine/client/hwid.h>
#include <game/client/gameclient.h>
#include <engine/external/stb/stb_image.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <engine/external/json-parser/json.h>

#include <cstring>
#include <cstdio>
#include <chrono>

static const char *HWID_SALT = "Shalawa_Client_2026_MySecretKey_xD";
static const uint32_t GIFCACHE_MAGIC = 0x47434348;
static const uint32_t GIFCACHE_VERSION = 1;

CBadgeManager::CBadgeManager()
{
	m_pGraphics = nullptr;
	m_pStorage = nullptr;
	m_pHttp = nullptr;
	m_pGameClient = nullptr;
	m_Loaded = false;
	m_InitialHeartbeatDone = false;
	m_FetchState = STATE_INIT;
	m_IsLocalVerified = false;
	m_HeartbeatTimer = 0.0f;
	m_HeartbeatPending = false;
	m_pHeartbeatRequest = nullptr;
	m_aLocalHwid[0] = '\0';
	m_aSelectedBadgeId[0] = '\0';
	m_aGifCacheDir[0] = '\0';
	m_VerifiedBadge.m_IsRestricted = true;
	m_DecodeThreadRunning = false;
}

CBadgeManager::~CBadgeManager()
{
	m_DecodeThreadRunning = false;
	if(m_DecodeThread.joinable())
		m_DecodeThread.join();
}

void CBadgeManager::Init(IGraphics *pGraphics, IStorage *pStorage, IHttp *pHttp, CGameClient *pGameClient)
{
	m_pGraphics = pGraphics;
	m_pStorage = pStorage;
	m_pHttp = pHttp;
	m_pGameClient = pGameClient;
	m_Loaded = false;
	m_FetchState = STATE_INIT;
	m_IsLocalVerified = false;
	m_HeartbeatTimer = HEARTBEAT_INTERVAL;
	m_HeartbeatPending = false;
	m_pHeartbeatRequest = nullptr;
	m_vPublicBadges.clear();
	m_FullDownloadRequested = false;
	m_vQueuedForDownload.clear();
	m_mPlayerBadge.clear();
	m_VerifiedHwids.clear();
	m_vPendingTextures.clear();
	m_vOnlinePlayers.clear();
	m_pCurrentRequest = nullptr;
	g_Config.m_HttpAllowInsecure = 1;

	m_pStorage->CreateFolder("downloadedicons", IStorage::TYPE_SAVE);
	m_pStorage->CreateFolder("downloadedicons/badges", IStorage::TYPE_SAVE);
	m_pStorage->CreateFolder("downloadedicons/badges/cache", IStorage::TYPE_SAVE);

	char aFullPath[512];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "downloadedicons/badges/cache", aFullPath, sizeof(aFullPath));
	if(aFullPath[0] != '\0')
		str_copy(m_aGifCacheDir, aFullPath, sizeof(m_aGifCacheDir));
	else
		m_aGifCacheDir[0] = '\0';

	LoadSelectedBadge();

	char aHwid[128];
	if(GetSystemHWID(aHwid, sizeof(aHwid)))
	{
		SetLocalHwid(aHwid);
		log_info("badges", "HWID loaded");
	}
	else
	{
		log_error("badges", "Failed to get HWID");
		m_aLocalHwid[0] = '\0';
	}

	m_DecodeThreadRunning = true;
	m_DecodeThread = std::thread(&CBadgeManager::DecodeThreadFunc, this);
}

bool CBadgeManager::IsGifData(const unsigned char *pData, size_t DataSize) const
{
	if(DataSize < 6)
		return false;
	return (pData[0] == 'G' && pData[1] == 'I' && pData[2] == 'F' &&
		pData[3] == '8' && (pData[4] == '7' || pData[4] == '9') && pData[5] == 'a');
}

bool CBadgeManager::LoadGifFromMemory(const unsigned char *pData, size_t DataSize, CBadgeDef &Badge)
{
	(void)pData;
	(void)DataSize;
	(void)Badge;
	return false;
}

bool CBadgeManager::LoadImageFromMemory(const unsigned char *pData, size_t DataSize, CBadgeDef &Badge, const char *pCachePath)
{
	(void)pData;
	(void)DataSize;
	(void)Badge;
	(void)pCachePath;
	return false;
}

void CBadgeManager::SaveGifCache(const char *pId, const std::vector<SDecodedFrame> &vFrames, int Width, int Height)
{
	if(m_aGifCacheDir[0] == '\0' || vFrames.empty())
		return;

	char aPath[512];
	str_format(aPath, sizeof(aPath), "%s/%s.gifcache", m_aGifCacheDir, pId);

	FILE *f = fopen(aPath, "wb");
	if(!f)
		return;

	uint32_t Magic = GIFCACHE_MAGIC;
	uint32_t Version = GIFCACHE_VERSION;
	uint32_t FC = (uint32_t)vFrames.size();
	uint32_t W = (uint32_t)Width;
	uint32_t H = (uint32_t)Height;

	fwrite(&Magic, 4, 1, f);
	fwrite(&Version, 4, 1, f);
	fwrite(&FC, 4, 1, f);
	fwrite(&W, 4, 1, f);
	fwrite(&H, 4, 1, f);

	size_t FrameSize = (size_t)Width * Height * 4;
	for(const auto &Frame : vFrames)
	{
		uint32_t Delay = (uint32_t)Frame.m_DelayMs;
		fwrite(&Delay, 4, 1, f);
		fwrite(Frame.m_pData, 1, FrameSize, f);
	}

	fclose(f);
	log_info("badges", "Saved GIF cache '%s': %d frames", pId, (int)vFrames.size());
}

bool CBadgeManager::ParseGifCache(const unsigned char *pData, size_t DataSize, SDecodedBadge &Decoded)
{
	if(DataSize < 20)
		return false;

	uint32_t Magic, Version, FrameCount, Width, Height;
	memcpy(&Magic, pData, 4);
	memcpy(&Version, pData + 4, 4);
	memcpy(&FrameCount, pData + 8, 4);
	memcpy(&Width, pData + 12, 4);
	memcpy(&Height, pData + 16, 4);

	if(Magic != GIFCACHE_MAGIC || Version != GIFCACHE_VERSION)
		return false;

	if(FrameCount == 0 || Width == 0 || Height == 0)
		return false;

	size_t FramePixelSize = (size_t)Width * Height * 4;
	size_t ExpectedSize = 20 + (size_t)FrameCount * (4 + FramePixelSize);
	if(DataSize < ExpectedSize)
		return false;

	Decoded.m_IsGif = true;
	Decoded.m_pStaticData = nullptr;
	Decoded.m_StaticWidth = 0;
	Decoded.m_StaticHeight = 0;

	const unsigned char *pCursor = pData + 20;
	for(uint32_t i = 0; i < FrameCount; i++)
	{
		uint32_t Delay;
		memcpy(&Delay, pCursor, 4);
		pCursor += 4;

		SDecodedFrame Frame;
		Frame.m_pData = (uint8_t *)malloc(FramePixelSize);
		memcpy(Frame.m_pData, pCursor, FramePixelSize);
		pCursor += FramePixelSize;
		Frame.m_Width = (int)Width;
		Frame.m_Height = (int)Height;
		Frame.m_DelayMs = (int)Delay;
		Decoded.m_vFrames.push_back(Frame);
	}

	return true;
}

void CBadgeManager::SaveSelectedBadge()
{
	IOHANDLE File = m_pStorage->OpenFile("downloadedicons/badges/selected.txt", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, m_aSelectedBadgeId, str_length(m_aSelectedBadgeId));
	io_close(File);
}

void CBadgeManager::LoadSelectedBadge()
{
	IOHANDLE File = m_pStorage->OpenFile("downloadedicons/badges/selected.txt", IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
	{
		m_aSelectedBadgeId[0] = '\0';
		return;
	}
	char aBuf[64] = "";
	unsigned Read = io_read(File, aBuf, sizeof(aBuf) - 1);
	io_close(File);
	aBuf[Read] = '\0';

	for(int i = 0; aBuf[i]; i++)
	{
		if(aBuf[i] == '\n' || aBuf[i] == '\r')
		{
			aBuf[i] = '\0';
			break;
		}
	}

	str_copy(m_aSelectedBadgeId, aBuf, sizeof(m_aSelectedBadgeId));
}

void CBadgeManager::SetSelectedBadge(const char *pId)
{
	if(pId)
		str_copy(m_aSelectedBadgeId, pId, sizeof(m_aSelectedBadgeId));
	else
		m_aSelectedBadgeId[0] = '\0';
	SaveSelectedBadge();
}

const CBadgeDef *CBadgeManager::GetSelectedBadge() const
{
	if(m_aSelectedBadgeId[0] == '\0')
		return nullptr;
	return GetBadgeById(m_aSelectedBadgeId);
}

void CBadgeManager::DecodeThreadFunc()
{
	while(m_DecodeThreadRunning)
	{
		SDecodeTask Task;
		bool HasTask = false;

		{
			std::lock_guard<std::mutex> Lock(m_TaskMutex);
			if(!m_qDecodeTasks.empty())
			{
				Task = m_qDecodeTasks.front();
				m_qDecodeTasks.pop();
				HasTask = true;
			}
		}

		if(!HasTask)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		if(Task.m_IsPreDecoded)
		{
			SDecodedBadge Decoded;
			Decoded.m_pBadge = Task.m_pBadge;
			str_copy(Decoded.m_aCachePath, Task.m_aCachePath, sizeof(Decoded.m_aCachePath));

			if(ParseGifCache(Task.m_pData, Task.m_DataSize, Decoded))
			{
				log_info("badges", "Loaded GIF cache '%s': %d frames", Task.m_pBadge->m_aId, (int)Decoded.m_vFrames.size());
				std::lock_guard<std::mutex> Lock(m_DecodedMutex);
				m_qDecodedBadges.push(std::move(Decoded));
			}
			else
			{
				log_error("badges", "Corrupted GIF cache '%s'", Task.m_pBadge->m_aId);
			}

			free(Task.m_pData);
			continue;
		}

		if(IsGifData(Task.m_pData, Task.m_DataSize))
		{
			int *pDelays = nullptr;
			int Width = 0, Height = 0, FrameCount = 0, Comp = 0;

			unsigned char *pFrames = stbi_load_gif_from_memory(Task.m_pData, (int)Task.m_DataSize, &pDelays, &Width, &Height, &FrameCount, &Comp, 4);
			if(pFrames && FrameCount > 0)
			{
				size_t FrameSize = (size_t)Width * Height * 4;
				SDecodedBadge Decoded;
				Decoded.m_pBadge = Task.m_pBadge;
				Decoded.m_IsGif = true;
				Decoded.m_pStaticData = nullptr;
				Decoded.m_StaticWidth = 0;
				Decoded.m_StaticHeight = 0;
				str_copy(Decoded.m_aCachePath, Task.m_aCachePath, sizeof(Decoded.m_aCachePath));

				for(int i = 0; i < FrameCount; i++)
				{
					SDecodedFrame Frame;
					Frame.m_pData = (uint8_t *)malloc(FrameSize);
					mem_copy(Frame.m_pData, pFrames + i * FrameSize, FrameSize);
					Frame.m_Width = Width;
					Frame.m_Height = Height;
					int Delay = pDelays[i];
					if(Delay <= 0) Delay = 100;
					else if(Delay < 20) Delay = Delay * 10;
					Frame.m_DelayMs = Delay;
					Decoded.m_vFrames.push_back(Frame);
				}

				stbi_image_free(pFrames);
				stbi_image_free(pDelays);

				SaveGifCache(Task.m_pBadge->m_aId, Decoded.m_vFrames, Width, Height);

				log_info("badges", "Decoded GIF '%s': %d frames (thread)", Task.m_pBadge->m_aId, FrameCount);

				{
					std::lock_guard<std::mutex> Lock(m_DecodedMutex);
					m_qDecodedBadges.push(std::move(Decoded));
				}
			}
			else
			{
				if(pFrames) stbi_image_free(pFrames);
				if(pDelays) stbi_image_free(pDelays);
				log_error("badges", "Failed to decode GIF '%s'", Task.m_pBadge->m_aId);
			}
		}
		else
		{
			int Width, Height, Comp;
			uint8_t *pPixels = stbi_load_from_memory(Task.m_pData, (int)Task.m_DataSize, &Width, &Height, &Comp, 4);
			if(pPixels)
			{
				SDecodedBadge Decoded;
				Decoded.m_pBadge = Task.m_pBadge;
				Decoded.m_IsGif = false;
				Decoded.m_pStaticData = pPixels;
				Decoded.m_StaticWidth = Width;
				Decoded.m_StaticHeight = Height;
				str_copy(Decoded.m_aCachePath, Task.m_aCachePath, sizeof(Decoded.m_aCachePath));

				{
					std::lock_guard<std::mutex> Lock(m_DecodedMutex);
					m_qDecodedBadges.push(std::move(Decoded));
				}
			}
			else
			{
				log_error("badges", "Failed to decode image '%s'", Task.m_pBadge->m_aId);
			}
		}

		free(Task.m_pData);
	}
}

void CBadgeManager::ProcessDecodedBadges()
{
	std::lock_guard<std::mutex> Lock(m_DecodedMutex);
	int Created = 0;

	while(!m_qDecodedBadges.empty() && Created < MAX_TEXTURES_PER_FRAME)
	{
		SDecodedBadge &Dec = m_qDecodedBadges.front();
		CBadgeDef *pBadge = Dec.m_pBadge;

		if(Dec.m_IsGif)
		{
			for(auto &Frame : Dec.m_vFrames)
			{
				CImageInfo Info;
				Info.m_Width = Frame.m_Width;
				Info.m_Height = Frame.m_Height;
				Info.m_Format = CImageInfo::FORMAT_RGBA;
				Info.m_pData = Frame.m_pData;

				char aName[128];
				str_format(aName, sizeof(aName), "%s_frame_%d", pBadge->m_aId, (int)pBadge->m_vGifFrames.size());

				IGraphics::CTextureHandle Tex = m_pGraphics->LoadTextureRawMove(Info, 0, aName);
				if(Tex.IsValid())
				{
					SGifFrame GFrame;
					GFrame.m_Texture = Tex;
					GFrame.m_DelayMs = Frame.m_DelayMs;
					pBadge->m_vGifFrames.push_back(GFrame);
				}
				else
				{
					free(Frame.m_pData);
				}
				Created++;
			}

			if(!pBadge->m_vGifFrames.empty())
			{
				pBadge->m_IsAnimated = true;
				pBadge->m_Texture = pBadge->m_vGifFrames[0].m_Texture;
				pBadge->m_TextureLoaded = true;
				pBadge->m_CurrentFrame = 0;
				pBadge->m_AnimTimer = 0.0f;
				log_info("badges", "Textures created '%s': %d frames", pBadge->m_aId, (int)pBadge->m_vGifFrames.size());
			}
		}
		else
		{
			CImageInfo Info;
			Info.m_Width = Dec.m_StaticWidth;
			Info.m_Height = Dec.m_StaticHeight;
			Info.m_Format = CImageInfo::FORMAT_RGBA;
			Info.m_pData = Dec.m_pStaticData;

			pBadge->m_Texture = m_pGraphics->LoadTextureRawMove(Info, 0, Dec.m_aCachePath);
			pBadge->m_TextureLoaded = pBadge->m_Texture.IsValid();
			pBadge->m_IsAnimated = false;
			Created++;
		}

		m_qDecodedBadges.pop();
	}
}

void CBadgeManager::UpdateAnimations(float DeltaTime)
{
	auto UpdateBadgeAnim = [&](CBadgeDef &Badge) {
		if(!Badge.m_IsAnimated || Badge.m_vGifFrames.empty())
			return;

		Badge.m_AnimTimer += DeltaTime * 1000.0f;
		int MaxSteps = (int)Badge.m_vGifFrames.size();
		int Steps = 0;
		int OriginalFrame = Badge.m_CurrentFrame;

		while(Badge.m_AnimTimer >= Badge.m_vGifFrames[Badge.m_CurrentFrame].m_DelayMs && Steps < MaxSteps)
		{
			Badge.m_AnimTimer -= Badge.m_vGifFrames[Badge.m_CurrentFrame].m_DelayMs;
			Badge.m_CurrentFrame = (Badge.m_CurrentFrame + 1) % (int)Badge.m_vGifFrames.size();
			Steps++;
		}

		if(Steps >= MaxSteps)
			Badge.m_AnimTimer = 0;

		if(OriginalFrame != Badge.m_CurrentFrame)
			Badge.m_Texture = Badge.m_vGifFrames[Badge.m_CurrentFrame].m_Texture;
	};

	for(auto &Badge : m_vPublicBadges)
		UpdateBadgeAnim(Badge);
	UpdateBadgeAnim(m_VerifiedBadge);
}

IGraphics::CTextureHandle CBadgeManager::GetCurrentBadgeTexture(const CBadgeDef *pBadge) const
{
	if(!pBadge || !pBadge->m_TextureLoaded)
		return IGraphics::CTextureHandle();

	if(pBadge->m_IsAnimated && !pBadge->m_vGifFrames.empty())
		return pBadge->m_vGifFrames[pBadge->m_CurrentFrame].m_Texture;

	return pBadge->m_Texture;
}

void CBadgeManager::SetLocalHwid(const char *pHwid)
{
	str_copy(m_aLocalHwid, pHwid, sizeof(m_aLocalHwid));
	if(m_FetchState == STATE_DONE)
		CheckLocalVerified();
}

std::string CBadgeManager::HashHwid(const char *pHwid) const
{
	char aSalted[512];
	str_format(aSalted, sizeof(aSalted), "%s%s%s", HWID_SALT, pHwid, HWID_SALT);

	SHA256_CTX Ctx;
	sha256_init(&Ctx);
	sha256_update(&Ctx, aSalted, str_length(aSalted));
	SHA256_DIGEST Digest = sha256_finish(&Ctx);

	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(Digest, aHash, sizeof(aHash));
	return std::string(aHash);
}

void CBadgeManager::CheckLocalVerified()
{
	if(m_aLocalHwid[0] == '\0')
	{
		m_IsLocalVerified = false;
		return;
	}

	std::string Hash = HashHwid(m_aLocalHwid);
	m_IsLocalVerified = m_VerifiedHwids.find(Hash) != m_VerifiedHwids.end();
	log_info("badges", "Local verified: %s", m_IsLocalVerified ? "YES" : "NO");
}

void CBadgeManager::Update(float DeltaTime)
{
	if(!m_pGraphics || !m_pStorage || !m_pHttp)
	{
		log_error("badges", "Update: missing pointers!");
		m_Loaded = true;
		return;
	}

	switch(m_FetchState)
	{
	case STATE_INIT:
		StartFetch();
		break;
	case STATE_FETCHING_BADGES:
		ProcessBadgesResponse();
		break;
	case STATE_FETCHING_TEXTURES:
		ProcessTextures();
		ProcessDecodedBadges();
		{
			bool AllDone = m_vPendingTextures.empty();
			{
				std::lock_guard<std::mutex> Lock1(m_TaskMutex);
				std::lock_guard<std::mutex> Lock2(m_DecodedMutex);
				if(!m_qDecodeTasks.empty() || !m_qDecodedBadges.empty())
					AllDone = false;
			}
			if(AllDone)
			{
				m_Loaded = true;
				m_FetchState = STATE_DONE;
				log_info("badges", "Initial badges loaded");
			}
		}
		break;
	case STATE_DONE:
		UpdateAnimations(DeltaTime);
		ProcessDecodedBadges();
		ProcessTextures();
		ProcessDecodedBadges();
		CheckAndFetchMissingOnlineBadges();
		m_HeartbeatTimer += DeltaTime;
		if(m_HeartbeatTimer >= HEARTBEAT_INTERVAL && !m_HeartbeatPending)
		{
			m_HeartbeatTimer = 0.0f;
			SendHeartbeat();
		}
		if(m_HeartbeatPending && m_pHeartbeatRequest && m_pHeartbeatRequest->Done())
			ProcessHeartbeatResponse();
		break;
	}
}

const CBadgeDef *CBadgeManager::GetOnlinePlayerBadge(const char *pNickname) const
{
	for(const auto &P : m_vOnlinePlayers)
	{
		if(str_comp(P.m_aNickname, pNickname) == 0 && P.m_aBadgeId[0] != '\0')
			return GetBadgeById(P.m_aBadgeId);
	}
	return nullptr;
}

void CBadgeManager::StartFetch()
{
	log_info("badges", "Starting fetch from: %s", BADGES_DATA_URL);
	m_pCurrentRequest = HttpGet(BADGES_DATA_URL);
	m_pCurrentRequest->Timeout(CTimeout{10000, 30000, 0, 0});
	m_pCurrentRequest->HeaderString("Connection", "close");
	m_pHttp->Run(m_pCurrentRequest);
	m_FetchState = STATE_FETCHING_BADGES;
}

void CBadgeManager::ProcessBadgesResponse()
{
	if(!m_pCurrentRequest || !m_pCurrentRequest->Done())
		return;

	if(m_pCurrentRequest->State() == EHttpState::DONE && m_pCurrentRequest->StatusCode() == 200)
	{
		unsigned char *pResult = nullptr;
		size_t ResultSize = 0;
		m_pCurrentRequest->Result(&pResult, &ResultSize);
		ParseBadges((const char *)pResult, ResultSize);
	}
	else
	{
		log_error("badges", "Failed to fetch badges.json");
	}

	m_pCurrentRequest = nullptr;
	CheckLocalVerified();
	FetchTextures();
	m_FetchState = STATE_FETCHING_TEXTURES;
}

void CBadgeManager::ParseBadges(const char *pData, size_t Size)
{
	json_value *pRoot = json_parse(pData, Size);
	if(!pRoot || pRoot->type != json_object)
	{
		if(pRoot) json_value_free(pRoot);
		return;
	}

	const json_value &Root = *pRoot;

	const json_value &Public = Root["public"];
	if(Public.type == json_array)
	{
		for(unsigned int i = 0; i < Public.u.array.length; i++)
		{
			const json_value &BadgeVal = *Public.u.array.values[i];
			if(BadgeVal.type != json_object)
				continue;

			CBadgeDef Badge;
			const json_value &Id = BadgeVal["id"];
			const json_value &Name = BadgeVal["name"];
			const json_value &Icon = BadgeVal["icon"];

			if(Id.type == json_string)
				str_copy(Badge.m_aId, Id.u.string.ptr, sizeof(Badge.m_aId));
			if(Name.type == json_string)
				str_copy(Badge.m_aName, Name.u.string.ptr, sizeof(Badge.m_aName));
			if(Icon.type == json_string)
			{
				const char *pIcon = Icon.u.string.ptr;
				if(str_startswith(pIcon, "http://") || str_startswith(pIcon, "https://"))
					str_copy(Badge.m_aTextureUrl, pIcon, sizeof(Badge.m_aTextureUrl));
				else
					str_format(Badge.m_aTextureUrl, sizeof(Badge.m_aTextureUrl), "%s%s", BADGES_TEXTURES_URL, pIcon);
			}

			if(Badge.m_aId[0] && Badge.m_aTextureUrl[0])
				m_vPublicBadges.push_back(Badge);
		}
	}

	const json_value &Restricted = Root["restricted"];
	if(Restricted.type == json_array)
	{
		for(unsigned int i = 0; i < Restricted.u.array.length; i++)
		{
			const json_value &BadgeVal = *Restricted.u.array.values[i];
			if(BadgeVal.type != json_object)
				continue;

			const json_value &Id = BadgeVal["id"];
			if(Id.type != json_string)
				continue;

			if(str_comp(Id.u.string.ptr, "verified") == 0)
			{
				const json_value &Name = BadgeVal["name"];
				const json_value &Icon = BadgeVal["icon"];

				m_VerifiedBadge = CBadgeDef();
				m_VerifiedBadge.m_IsRestricted = true;
				str_copy(m_VerifiedBadge.m_aId, Id.u.string.ptr, sizeof(m_VerifiedBadge.m_aId));
				if(Name.type == json_string)
					str_copy(m_VerifiedBadge.m_aName, Name.u.string.ptr, sizeof(m_VerifiedBadge.m_aName));
				if(Icon.type == json_string)
				{
					const char *pIcon = Icon.u.string.ptr;
					if(str_startswith(pIcon, "http://") || str_startswith(pIcon, "https://"))
						str_copy(m_VerifiedBadge.m_aTextureUrl, pIcon, sizeof(m_VerifiedBadge.m_aTextureUrl));
					else
						str_format(m_VerifiedBadge.m_aTextureUrl, sizeof(m_VerifiedBadge.m_aTextureUrl), "%s%s", BADGES_TEXTURES_URL, pIcon);
				}
			}
			else
			{
				CBadgeDef Badge;
				Badge.m_IsRestricted = true;
				const json_value &Name = BadgeVal["name"];
				const json_value &Icon = BadgeVal["icon"];

				str_copy(Badge.m_aId, Id.u.string.ptr, sizeof(Badge.m_aId));
				if(Name.type == json_string)
					str_copy(Badge.m_aName, Name.u.string.ptr, sizeof(Badge.m_aName));
				if(Icon.type == json_string)
				{
					const char *pIcon = Icon.u.string.ptr;
					if(str_startswith(pIcon, "http://") || str_startswith(pIcon, "https://"))
						str_copy(Badge.m_aTextureUrl, pIcon, sizeof(Badge.m_aTextureUrl));
					else
						str_format(Badge.m_aTextureUrl, sizeof(Badge.m_aTextureUrl), "%s%s", BADGES_TEXTURES_URL, pIcon);
				}

				if(Badge.m_aId[0] && Badge.m_aTextureUrl[0])
					m_vPublicBadges.push_back(Badge);
			}
		}
	}

	const json_value &Hwids = Root["hwids"];
	if(Hwids.type == json_array)
	{
		for(unsigned int i = 0; i < Hwids.u.array.length; i++)
		{
			const json_value *pHash = Hwids.u.array.values[i];
			if(pHash->type == json_string)
				m_VerifiedHwids.insert(pHash->u.string.ptr);
		}
	}

	const json_value &Players = Root["players"];
	if(Players.type == json_object)
	{
		for(unsigned int i = 0; i < Players.u.object.length; i++)
		{
			const char *pPlayerName = Players.u.object.values[i].name;
			const json_value *pBadgeId = Players.u.object.values[i].value;
			if(pBadgeId->type == json_string)
				m_mPlayerBadge[pPlayerName] = pBadgeId->u.string.ptr;
		}
	}

	json_value_free(pRoot);
}

void CBadgeManager::SendHeartbeat()
{
	if(!m_pGameClient || m_aLocalHwid[0] == '\0')
		return;

	const char *pNickname = m_pGameClient->Client()->PlayerName();
	if(!pNickname || !pNickname[0])
		return;

	char aServer[64] = "";
	if(m_pGameClient->Client()->State() == IClient::STATE_ONLINE)
	{
		NETADDR Addr = m_pGameClient->Client()->ServerAddress();
		net_addr_str(&Addr, aServer, sizeof(aServer), true);
	}

	std::string Hash = HashHwid(m_aLocalHwid);

	char aBody[512];
	str_format(aBody, sizeof(aBody),
		"{\"hwid\":\"%s\",\"nickname\":\"%s\",\"server\":\"%s\",\"badge\":\"%s\"}",
		Hash.c_str(), pNickname, aServer, m_aSelectedBadgeId);

	m_pHeartbeatRequest = HttpPost(BADGES_HEARTBEAT_URL, (const unsigned char *)aBody, str_length(aBody));
	m_pHeartbeatRequest->Timeout(CTimeout{10000, 20000, 0, 0});
	m_pHeartbeatRequest->HeaderString("Content-Type", "application/json");
	m_pHeartbeatRequest->HeaderString("Connection", "close");
	m_pHeartbeatRequest->HeaderString("User-Agent", "ShalawaClient/1.0");

	m_pHttp->Run(m_pHeartbeatRequest);
	m_HeartbeatPending = true;
	m_InitialHeartbeatDone = true;
}

void CBadgeManager::ProcessHeartbeatResponse()
{
	m_HeartbeatPending = false;
	if(!m_pHeartbeatRequest)
		return;

	if(m_pHeartbeatRequest->State() == EHttpState::DONE && m_pHeartbeatRequest->StatusCode() == 200)
	{
		unsigned char *pResult = nullptr;
		size_t ResultSize = 0;
		m_pHeartbeatRequest->Result(&pResult, &ResultSize);

		json_value *pRoot = json_parse((const char *)pResult, ResultSize);
		if(pRoot && pRoot->type == json_object)
		{
			const json_value &Verified = (*pRoot)["verified"];
			if(Verified.type == json_boolean)
				m_IsLocalVerified = Verified.u.boolean;

			const json_value &Players = (*pRoot)["players"];
			if(Players.type == json_array)
			{
				m_vOnlinePlayers.clear();
				for(unsigned int i = 0; i < Players.u.array.length; i++)
				{
					const json_value &P = *Players.u.array.values[i];
					if(P.type != json_object)
						continue;

					SOnlinePlayer Player;
					Player.m_aNickname[0] = '\0';
					Player.m_aServer[0] = '\0';
					Player.m_aBadgeId[0] = '\0';
					Player.m_Verified = false;

					const json_value &Nick = P["nickname"];
					const json_value &Server = P["server_addr"];
					const json_value &Badge = P["badge_id"];
					const json_value &V = P["verified"];

					if(Nick.type == json_string)
						str_copy(Player.m_aNickname, Nick.u.string.ptr, sizeof(Player.m_aNickname));
					if(Server.type == json_string)
						str_copy(Player.m_aServer, Server.u.string.ptr, sizeof(Player.m_aServer));
					if(Badge.type == json_string)
						str_copy(Player.m_aBadgeId, Badge.u.string.ptr, sizeof(Player.m_aBadgeId));
					if(V.type == json_boolean)
						Player.m_Verified = V.u.boolean;

					if(Player.m_aNickname[0])
						m_vOnlinePlayers.push_back(Player);
				}
			}
		}
		if(pRoot)
			json_value_free(pRoot);
	}

	m_pHeartbeatRequest = nullptr;
}

bool CBadgeManager::IsOnlinePlayerVerified(const char *pNickname) const
{
	for(const auto &P : m_vOnlinePlayers)
	{
		if(str_comp(P.m_aNickname, pNickname) == 0)
			return P.m_Verified;
	}
	return false;
}

void CBadgeManager::TryLoadCached(CBadgeDef &Badge)
{
	char aGifCachePath[512];
	str_format(aGifCachePath, sizeof(aGifCachePath), "downloadedicons/badges/cache/%s.gifcache", Badge.m_aId);

	IOHANDLE File = m_pStorage->OpenFile(aGifCachePath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(File)
	{
		void *pFileData;
		unsigned FileDataSize;
		if(io_read_all(File, &pFileData, &FileDataSize))
		{
			io_close(File);

			SDecodeTask Task;
			Task.m_pBadge = &Badge;
			Task.m_pData = (unsigned char *)pFileData;
			Task.m_DataSize = FileDataSize;
			Task.m_IsPreDecoded = true;
			str_copy(Task.m_aCachePath, aGifCachePath, sizeof(Task.m_aCachePath));

			std::lock_guard<std::mutex> Lock(m_TaskMutex);
			m_qDecodeTasks.push(Task);
			return;
		}
		io_close(File);
	}

	char aCachePathPng[512];
	char aCachePathGif[512];
	str_format(aCachePathPng, sizeof(aCachePathPng), "downloadedicons/badges/%s.png", Badge.m_aId);
	str_format(aCachePathGif, sizeof(aCachePathGif), "downloadedicons/badges/%s.gif", Badge.m_aId);

	const char *pPath = nullptr;
	File = m_pStorage->OpenFile(aCachePathGif, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(File)
	{
		pPath = aCachePathGif;
	}
	else
	{
		File = m_pStorage->OpenFile(aCachePathPng, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
			pPath = aCachePathPng;
	}

	if(!File)
		return;

	void *pFileData;
	unsigned FileDataSize;
	if(!io_read_all(File, &pFileData, &FileDataSize))
	{
		io_close(File);
		return;
	}
	io_close(File);

	SDecodeTask Task;
	Task.m_pBadge = &Badge;
	Task.m_pData = (unsigned char *)pFileData;
	Task.m_DataSize = FileDataSize;
	Task.m_IsPreDecoded = false;
	str_copy(Task.m_aCachePath, pPath, sizeof(Task.m_aCachePath));

	{
		std::lock_guard<std::mutex> Lock(m_TaskMutex);
		m_qDecodeTasks.push(Task);
	}
}

void CBadgeManager::QueueBadgeDownload(CBadgeDef &Badge)
{
	if(m_vQueuedForDownload.count(Badge.m_aId))
		return;
	m_vQueuedForDownload.insert(Badge.m_aId);

	TryLoadCached(Badge);
	if(Badge.m_TextureLoaded)
		return;

	if(Badge.m_aTextureUrl[0])
	{
		SPendingTexture Pending;
		Pending.m_pBadge = &Badge;
		Pending.m_pRequest = HttpGet(Badge.m_aTextureUrl);
		Pending.m_pRequest->HeaderString("Connection", "close");
		m_pHttp->Run(Pending.m_pRequest);
		m_vPendingTextures.push_back(std::move(Pending));
	}
}

void CBadgeManager::FetchTextures()
{
	if(m_VerifiedBadge.m_aTextureUrl[0])
		QueueBadgeDownload(m_VerifiedBadge);

	CBadgeDef *pSelectedBadge = nullptr;
	for(auto &Badge : m_vPublicBadges)
	{
		if(str_comp(Badge.m_aId, m_aSelectedBadgeId) == 0)
		{
			pSelectedBadge = &Badge;
			QueueBadgeDownload(Badge);
			break;
		}
	}

	std::vector<int> vIndices;
	for(int i = 0; i < (int)m_vPublicBadges.size(); i++)
	{
		if(pSelectedBadge && m_vPublicBadges[i].m_aId == pSelectedBadge->m_aId)
			continue;
		vIndices.push_back(i);
	}

	for(int i = (int)vIndices.size() - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		std::swap(vIndices[i], vIndices[j]);
	}

	int Downloaded = 0;
	for(int Idx : vIndices)
	{
		if(Downloaded >= 3)
			break;
		QueueBadgeDownload(m_vPublicBadges[Idx]);
		Downloaded++;
	}

	if(m_vPendingTextures.empty())
	{
		m_Loaded = true;
		m_FetchState = STATE_DONE;
	}
}

void CBadgeManager::RequestFullDownload()
{
	if(m_FullDownloadRequested)
		return;
	m_FullDownloadRequested = true;

	for(auto &Badge : m_vPublicBadges)
		QueueBadgeDownload(Badge);
}

void CBadgeManager::CheckAndFetchMissingOnlineBadges()
{
	for(const auto &Player : m_vOnlinePlayers)
	{
		if(!Player.m_aBadgeId[0])
			continue;

		if(m_vQueuedForDownload.count(Player.m_aBadgeId))
			continue;

		for(auto &Badge : m_vPublicBadges)
		{
			if(str_comp(Badge.m_aId, Player.m_aBadgeId) == 0)
			{
				log_info("badges", "fetching badge '%s' for online player '%s'", Badge.m_aId, Player.m_aNickname);
				QueueBadgeDownload(Badge);
				break;
			}
		}
	}
}

void CBadgeManager::ProcessTextures()
{
	for(auto it = m_vPendingTextures.begin(); it != m_vPendingTextures.end();)
	{
		if(!it->m_pRequest->Done())
		{
			++it;
			continue;
		}

		if(it->m_pRequest->State() == EHttpState::DONE && it->m_pRequest->StatusCode() == 200)
		{
			unsigned char *pResult = nullptr;
			size_t ResultSize = 0;
			it->m_pRequest->Result(&pResult, &ResultSize);

			const char *pExt = IsGifData(pResult, ResultSize) ? "gif" : "png";
			char aCachePath[512];
			str_format(aCachePath, sizeof(aCachePath), "downloadedicons/badges/%s.%s", it->m_pBadge->m_aId, pExt);

			IOHANDLE File = m_pStorage->OpenFile(aCachePath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(File)
			{
				io_write(File, pResult, ResultSize);
				io_close(File);
			}

			unsigned char *pCopy = (unsigned char *)malloc(ResultSize);
			mem_copy(pCopy, pResult, ResultSize);

			SDecodeTask Task;
			Task.m_pBadge = it->m_pBadge;
			Task.m_pData = pCopy;
			Task.m_DataSize = ResultSize;
			Task.m_IsPreDecoded = false;
			str_copy(Task.m_aCachePath, aCachePath, sizeof(Task.m_aCachePath));

			{
				std::lock_guard<std::mutex> Lock(m_TaskMutex);
				m_qDecodeTasks.push(Task);
			}
		}
		else
		{
			log_error("badges", "Failed to download texture for '%s'", it->m_pBadge->m_aId);
		}

		it = m_vPendingTextures.erase(it);
	}
}

const CBadgeDef *CBadgeManager::GetBadgeById(const char *pId) const
{
	if(!pId || !pId[0])
		return nullptr;

	for(const auto &Badge : m_vPublicBadges)
	{
		if(str_comp(Badge.m_aId, pId) == 0 && Badge.m_TextureLoaded)
			return &Badge;
	}

	return nullptr;
}

const CBadgeDef *CBadgeManager::GetPlayerBadge(const char *pName) const
{
	if(!pName || !pName[0])
		return nullptr;

	auto it = m_mPlayerBadge.find(pName);
	if(it == m_mPlayerBadge.end())
		return nullptr;

	return GetBadgeById(it->second.c_str());
}

bool CBadgeManager::IsPlayerVerified(const char *pName) const
{
	return IsOnlinePlayerVerified(pName);
}

void CBadgeManager::RenderBadge(const CBadgeDef *pBadge, float x, float y, float w, float h)
{
	if(!pBadge || !pBadge->m_TextureLoaded)
		return;

	IGraphics::CTextureHandle Tex = GetCurrentBadgeTexture(pBadge);
	if(!Tex.IsValid())
		return;

	m_pGraphics->TextureSet(Tex);
	m_pGraphics->QuadsBegin();
	m_pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	m_pGraphics->QuadsDrawTL(&QuadItem, 1);
	m_pGraphics->QuadsEnd();
}

void CBadgeManager::RenderVerifiedBadge(float x, float y, float w, float h)
{
	if(!m_VerifiedBadge.m_TextureLoaded)
		return;

	IGraphics::CTextureHandle Tex = GetCurrentBadgeTexture(&m_VerifiedBadge);
	if(!Tex.IsValid())
		return;

	m_pGraphics->TextureSet(Tex);
	m_pGraphics->QuadsBegin();
	m_pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	m_pGraphics->QuadsDrawTL(&QuadItem, 1);
	m_pGraphics->QuadsEnd();
}
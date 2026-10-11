#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHPLAYER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHPLAYER_H

#include <game/client/component.h>
#include <base/vmath.h>
#include <generated/protocol.h>
#include <engine/client/enums.h>

class CTasRecorder;

class CTasPlayer : public CComponent
{
public:
	CTasPlayer();
	CTasRecorder *m_pRecorder;
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;

	void SetRecorder(CTasRecorder *pRecorder) { m_pRecorder = pRecorder; }

	void StartPlayback();
	void StopPlayback();
	void OnRender() override;
	bool IsPlaying() const;

	void PlayTick();
	void Reset();
	void EnsurePrepared();

	size_t GetPlayIndex() const { return m_PlayIndex; }

private:
	bool m_Prepared;
	size_t m_PlayIndex;
	int m_LastGameTick;

	CNetObj_PlayerInput m_aCachedInput[NUM_DUMMIES];
	vec2 m_aCachedMouse[NUM_DUMMIES];
	int m_aCachedKill[NUM_DUMMIES];
};

#endif
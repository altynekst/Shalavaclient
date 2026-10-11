#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_TAS_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_TAS_H

#include <base/vmath.h>
#include <engine/client/enums.h>
#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/prediction/entities/character.h>
#include <generated/protocol.h>

#include <string>
#include <vector>

class CCharacter;
struct STasCharacterHistory; // Forward declaration - defined in controls.h

// TAS World
class CTasWorld : public CComponent
{
public:
	CTasWorld();
	virtual ~CTasWorld();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnInit() override;
	virtual void OnRender() override;

	void Init();
	void Shutdown();
	bool IsInitialized() const { return m_pWorld != nullptr && m_Initialized; }

	CGameWorld *World() { return m_pWorld; }
	CGameWorld *PredictedWorld() { return m_pWorldPredicted; }
	const CGameWorld *World() const { return m_pWorld; }
	const CGameWorld *PredictedWorld() const { return m_pWorldPredicted; }

	void HandleTeleport(CCharacter *pChar);
	void ApplyDummyHammer(CCharacter *pDummyChar);
	void CopyFromPredicted();

	int GameTick() const;
	void SetGameTick(int Tick);
	void IncrementGameTick();

private:
	CGameWorld *m_pWorld;
	CGameWorld *m_pWorldPredicted;
	bool m_Initialized;
};

// TAS Recorder
class CTasRecorder : public CComponent
{
public:
	CTasRecorder();
	CTasWorld *m_pTasWorld;
	
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnRender() override;

	void SetTasWorld(CTasWorld *pTasWorld) { m_pTasWorld = pTasWorld; }

	void StartRecording();
	void StopRecording();
	bool IsRecording() const;

	void RecordTick();
	void StepBack();
	void DetectRollback();

	void Clear();
	bool Save(const char *pName);
	bool Load(const char *pName);

	const STasCharacterHistory *FindCharHistory(int Id) const;
	STasCharacterHistory *FindCharHistory(int Id);

	std::string GetSavePath(const char *pName) const;

	std::vector<int> m_vTicks;
	std::vector<CNetObj_PlayerInput> m_vActions;
	std::vector<vec2> m_vMouse;
	std::vector<int> m_vDummySwitch;
	std::vector<int> m_vKill;
	std::vector<int> m_vAttackTick;
	std::vector<vec2> m_vPositions;
	std::vector<STasCharacterHistory> m_vChars;

	int m_aLastAttackTick[NUM_DUMMIES];
	int m_aPrevFireForAttack[NUM_DUMMIES];
	int m_aLastObservedTasAttackTick[NUM_DUMMIES];

	int64_t m_RecordingStartTime;
	int m_RecordingStartTick;
	int m_JumpCount;
	int m_PrevJump;
};

// TAS Player
class CTasPlayer : public CComponent
{
public:
	CTasPlayer();
	CTasRecorder *m_pRecorder;
	
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnRender() override;

	void SetRecorder(CTasRecorder *pRecorder) { m_pRecorder = pRecorder; }

	void StartPlayback();
	void StopPlayback();
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

// TAS Predict
class CTasPredict : public CComponent
{
public:
	CTasPredict();
	CTasWorld *m_pTasWorld;
	
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;

	void SetTasWorld(CTasWorld *pTasWorld) { m_pTasWorld = pTasWorld; }

	void PredictPath(int ClientId, int Ticks, std::vector<vec2> &vPath);
	bool WillFreeze(int ClientId, int Ticks);
};

// TAS Render
class CTasRender : public CComponent
{
public:
	CTasRender();

	CTasWorld *m_pTasWorld;
	CTasRecorder *m_pRecorder;
	CTasPredict *m_pPredict;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnRender() override;

	void SetTasWorld(CTasWorld *pTasWorld) { m_pTasWorld = pTasWorld; }
	void SetRecorder(CTasRecorder *pRecorder) { m_pRecorder = pRecorder; }
	void SetPredict(CTasPredict *pPredict) { m_pPredict = pPredict; }

	void SetSmoothIntra(float Intra) { m_SmoothIntra = Intra; }
	void SetInterpMouse(int Dummy, vec2 Mouse) { m_aInterpMouse[Dummy] = Mouse; }

	vec2 GetInterpolatedMouseForRendering(int Dummy) const;
	vec2 GetInterpolatedPlayerPosition(int Dummy) const;

private:
	void RenderTasWorld();
	void RenderStartEndPos();
	void RenderReplayPath();
	void RenderPredictionPath();

	float m_SmoothIntra;
	vec2 m_aInterpMouse[NUM_DUMMIES];
};

#endif


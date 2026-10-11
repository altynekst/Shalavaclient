#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHRECORDER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHRECORDER_H

#include <base/vmath.h>
#include <engine/client/enums.h>
#include <game/client/component.h>
#include <game/client/prediction/entities/character.h>
#include <generated/protocol.h>

#include <string>
#include <vector>

struct STasCharacterHistory
{
	int m_Id;
	std::vector<vec2> m_Positions;
	std::vector<vec2> m_PrevPositions;
	std::vector<int> m_FreezeTime;
	std::vector<int> m_ReloadTimer;
	std::vector<CCharacterCore> m_Cores;
};

class CTasWorld;

class CTasRecorder : public CComponent
{
public:
	CTasRecorder();
    CTasWorld *m_pTasWorld;
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;

	void SetTasWorld(CTasWorld *pTasWorld) { m_pTasWorld = pTasWorld; }

	void StartRecording();
    void OnRender() override;
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

private:
};

#endif
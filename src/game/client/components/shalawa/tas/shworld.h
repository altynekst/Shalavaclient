#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHWORLD_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHWORLD_H

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>

class CCharacter;

class CTasWorld : public CComponent
{
public:
	CTasWorld();
	virtual ~CTasWorld();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnInit() override;

	void Init();
    void OnRender() override;
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

#endif
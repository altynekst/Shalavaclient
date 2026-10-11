#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHPREDICT_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHPREDICT_H

#include <base/vmath.h>
#include <game/client/component.h>

class CTasWorld;

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

private:
};

#endif
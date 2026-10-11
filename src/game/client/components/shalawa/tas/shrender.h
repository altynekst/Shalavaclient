#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHRENDER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_TAS_SHRENDER_H

#include <base/vmath.h>
#include <engine/client/enums.h>
#include <game/client/component.h>

class CTasWorld;
class CTasRecorder;
class CTasPredict;

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
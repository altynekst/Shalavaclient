#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MISC_AUTOANSWER_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MISC_AUTOANSWER_H

#include <game/client/component.h>
#include <vector>
#include <string>

class CAutoAnswer : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnInit() override;

	struct STrigger
	{
		std::string m_Trigger;
		std::string m_Response;
		bool m_CaseSensitive;
		bool m_ExactMatch;
	};

	void AddTrigger(const char *pTrigger, const char *pResponse, bool CaseSensitive = false, bool ExactMatch = false);
	void RemoveTrigger(int Index);
	void ClearTriggers();
	void UpdateTrigger(int Index, const char *pTrigger, const char *pResponse, bool CaseSensitive, bool ExactMatch);

	std::vector<STrigger> &GetTriggers() { return m_vTriggers; }
	const std::vector<STrigger> &GetTriggers() const { return m_vTriggers; }

	void SaveTriggers();
	void LoadTriggers();

private:
	std::vector<STrigger> m_vTriggers;
	int64_t m_LastResponseTime = 0;

	bool MatchesTrigger(const char *pMessage, const STrigger &Trigger) const;
	bool ContainsMention(const char *pMessage) const;
	void SendResponse(const char *pSenderName, const char *pResponse, int Team);
};

#endif
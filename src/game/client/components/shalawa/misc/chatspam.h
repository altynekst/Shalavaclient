#ifndef GAME_CLIENT_COMPONENTS_SHALAWA_MISC_CHATSPAM_H
#define GAME_CLIENT_COMPONENTS_SHALAWA_MISC_CHATSPAM_H

#include <game/client/component.h>
#include <vector>
#include <string>

class CChatSpam : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnRender() override;

	void AddMessage(const char *pMessage);
	void RemoveMessage(int Index);
	void ClearMessages();
	void SetMessages(const std::vector<std::string> &Messages);
	void UpdateMessage(int Index, const char *pMessage);

	std::vector<std::string> &GetMessages() { return m_vMessages; }
	const std::vector<std::string> &GetMessages() const { return m_vMessages; }
	int GetCurrentIndex() const { return m_CurrentIndex; }

	void Start();
	void Stop();
	bool IsRunning() const { return m_Running; }

private:
	std::vector<std::string> m_vMessages;
	int m_CurrentIndex;
	int64_t m_LastSendTime;
	bool m_Running;

	void SendNextMessage();
	std::string ProcessMessage(const std::string &Message) const;
};

#endif
#pragma once

#include <game/client/component.h>

class CGp4oMenu : public CComponent
{
public:
    CGp4oMenu();
    
    virtual int Sizeof() const override { return sizeof(*this); }
    virtual void OnInit() override;
    virtual void OnReset() override;
    virtual bool OnInput(const IInput::CEvent &Event) override;
    virtual void OnRender() override;

private:
    bool m_Open;
    bool m_WasRelativeMode;
};
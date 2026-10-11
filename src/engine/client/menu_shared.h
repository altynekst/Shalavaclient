#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

enum EMenuCategory
{
    MCAT_COMBAT = 0,
    MCAT_MOVEMENT,
    MCAT_VISUAL,
    MCAT_MISC,
    MCAT_CONFIG,
    MCAT_KEYBINDS,
    MCAT_COUNT
};

struct SSettingBase
{
    enum EType
    {
        TOGGLE,
        SLIDER,
        COMBO,
        TEXT_INPUT,
        BUTTON,
        COLOR,
        MULTI_SELECT,
        TRIGGER_LIST
    };

    EType m_Type = TOGGLE;
    std::string m_Name;
    std::string m_Description;
    std::string m_Id;

    int *m_pIntValue = nullptr;
    float *m_pFloatValue = nullptr;

    bool m_IsFloat = false;
    float m_MinF = 0.0f;
    float m_MaxF = 100.0f;
    int m_Min = 0;
    int m_Max = 100;
    bool m_IsInteger = true;

    const char **m_pComboItems = nullptr;
    int m_ComboCount = 0;

    char *m_pTextBuffer = nullptr;
    int m_TextBufferSize = 0;
    int m_TextMin = 0;
    int m_TextMax = 256;

    char *m_pStringValue = nullptr;
    int m_StringMaxLen = 0;

    std::function<void()> m_Callback;
    std::string m_ButtonName;

    float m_ColorHue = 0.0f;
    float m_ColorSaturation = 1.0f;
    float m_ColorBrightness = 1.0f;
    float m_ColorAlpha = 1.0f;
    float *m_pColorHue = nullptr;
    float *m_pColorSaturation = nullptr;
    float *m_pColorBrightness = nullptr;
    float *m_pColorAlpha = nullptr;

    std::vector<std::string> m_MultiSelectOptions;
    std::unordered_set<std::string> *m_pMultiSelected = nullptr;

    bool m_Visible = true;
    std::function<bool()> m_VisibilityCheck;

    bool IsVisible() const
    {
        if(m_VisibilityCheck)
            return m_VisibilityCheck();
        return m_Visible;
    }
};

struct SModuleEntry
{
    std::string m_Id;
    std::string m_Name;
    std::string m_Description;
    EMenuCategory m_Category;
    int *m_pEnabled = nullptr;
    int m_Key = 0;
    bool m_Favorite = false;
    std::vector<SSettingBase> m_Settings;
};

class CMenuRegistry
{
public:
    static CMenuRegistry &Instance()
    {
        static CMenuRegistry s;
        return s;
    }

    void Clear() { m_Modules.clear(); }

    void AddModule(const SModuleEntry &mod)
    {
        m_Modules.push_back(mod);
    }

    SModuleEntry *FindModule(const std::string &id)
    {
        for(auto &m : m_Modules)
            if(m.m_Id == id)
                return &m;
        return nullptr;
    }

    std::vector<SModuleEntry> &GetModules() { return m_Modules; }

    std::vector<SModuleEntry *> GetModulesByCategory(EMenuCategory cat)
    {
        std::vector<SModuleEntry *> result;
        for(auto &m : m_Modules)
            if(m.m_Category == cat)
                result.push_back(&m);
        return result;
    }

private:
    CMenuRegistry() {}
    std::vector<SModuleEntry> m_Modules;
};
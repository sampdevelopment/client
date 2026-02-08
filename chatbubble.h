#pragma once

struct CHAT_BUBBLE_ENTRY
{
    char  szText[MAX_CHAT_BUBBLE_TEXT + 1];
    DWORD dwColor;
    DWORD dwExpireTick;
    float fDistance;
    int   iLineCount;
    bool  bValid;

    inline void Reset()
    {
        szText[0]   = '\0';
        dwColor     = 0;
        dwExpireTick = 0;
        fDistance   = 0.0f;
        iLineCount  = 0;
        bValid      = false;
    }
};

class CChatBubble
{
private:
    CHAT_BUBBLE_ENTRY m_ChatBubbleEntries[MAX_PLAYERS];

public:
    CChatBubble();

    void AddText(
        WORD        wPlayerID,
        const char* szText,
        DWORD       dwColor,
        float       fDistance,
        int         iDuration
    );

    void Draw();
};

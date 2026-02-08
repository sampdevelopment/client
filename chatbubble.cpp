#include "main.h"

static constexpr float BASE_TEXT_HEIGHT = 0.065f;
static constexpr float LINE_HEIGHT = 0.0125f;
static constexpr int HEAD_BONE_ID = 8;

CChatBubble::CChatBubble()
{
    for (auto &entry : m_ChatBubbleEntries)
        entry.Reset();
}

void CChatBubble::AddText(
    WORD wPlayerID,
    const char* szText,
    DWORD dwColor,
    float fDistance,
    int iDuration)
{
    if (!szText || wPlayerID >= MAX_PLAYERS)
        return;

    if (strlen(szText) > MAX_CHAT_BUBBLE_TEXT)
        return;

    CHAT_BUBBLE_ENTRY &entry = m_ChatBubbleEntries[wPlayerID];

    entry.Reset();

    strcpy_s(entry.szText, szText);
    entry.iLineCount = max(0, FormatChatBubbleText(entry.szText, 36, 12) - 1);

    entry.dwColor = (dwColor >> 8) | 0xFF000000;
    entry.fDistance = fDistance;
    entry.dwExpireTick = GetTickCount() + static_cast<DWORD>(iDuration);
    entry.bValid = true;
}

void CChatBubble::Draw()
{
    if (!pNetGame || !pGame || !pGame->FindPlayerPed() || !pLabel)
        return;

    const DWORD dwNow = GetTickCount();

    pLabel->Begin();

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        CHAT_BUBBLE_ENTRY &entry = m_ChatBubbleEntries[i];

        if (!entry.bValid)
            continue;

        if (dwNow > entry.dwExpireTick)
        {
            entry.bValid = false;
            continue;
        }

        CRemotePlayer* pRemote = pNetGame->GetPlayerPool()->GetAt(i);
        if (!pRemote || !pRemote->GetState())
            continue;

        CPlayerPed* pPed = pRemote->GetPlayerPed();
        if (!pPed)
            continue;

        if (pPed->GetDistanceFromLocalPlayerPed() > entry.fDistance)
            continue;

        VECTOR headPos;
        pPed->GetBonePosition(HEAD_BONE_ID, &headPos);

        D3DXVECTOR3 worldPos;
        worldPos.x = headPos.X;
        worldPos.y = headPos.Y;
        worldPos.z = headPos.Z;

        const float textOffset =
            BASE_TEXT_HEIGHT +
            (entry.iLineCount * LINE_HEIGHT * 2.0f);

        worldPos.z += pPed->GetDistanceFromCamera() * textOffset + 0.2f;

        pLabel->Draw(
            &worldPos,
            entry.szText,
            entry.dwColor,
            true,
            false
        );
    }

    pLabel->End();
}

#include "main.h"

#define BASS_STREAM_CREATE_URL_FLAG \
    BASS_STREAM_BLOCK | BASS_STREAM_STATUS | BASS_STREAM_AUTOFREE

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

CAudioStream::CAudioStream()
{
    m_Flags.bInited = false;
    m_Flags.bPlaying = false;
    m_hStream = 0;
    m_fX = m_fY = m_fZ = 0.0f;
    m_fDistance = -1.0f;
    m_ullLastUpdate = 0;
    m_fCurrentVol = 0.0f;
    m_fTargetVol = 0.0f;

    m_szTitle = (char*)calloc(256, 1);
    m_szLabel = (char*)calloc(256, 1);

    InitBass();
}

CAudioStream::~CAudioStream()
{
    Stop();
    BASS_Free();

    free(m_szTitle);
    free(m_szLabel);
    m_szTitle = m_szLabel = nullptr;
}

void CAudioStream::InitBass()
{
    if (HIWORD(BASS_GetVersion()) != BASSVERSION ||
        !BASS_Init(-1, 44100, 0, NULL, NULL))
        return;

    BASS_SetConfigPtr(BASS_CONFIG_NET_AGENT, "SA-MP/0.3");
    BASS_SetConfig(BASS_CONFIG_NET_PLAYLIST, 1);
    BASS_SetConfig(BASS_CONFIG_NET_TIMEOUT, 10000);
    BASS_SetConfigPtr(BASS_CONFIG_NET_PROXY, NULL);
    BASS_SetEAXParameters(-1, 0.0f, -1.0f, -1.0f);

    m_Flags.bInited = true;
}

void CAudioStream::Play(char* szURL,
    float fX, float fY, float fZ, float fDist)
{
    if (!m_Flags.bInited)
        return;

    Stop();

    m_fX = fX;
    m_fY = fY;
    m_fZ = fZ;
    m_fDistance = fDist;

    SecureZeroMemory(m_szTitle, 256);
    SecureZeroMemory(m_szLabel, 256);

    m_hStream = BASS_StreamCreateURL(
        szURL, 0, BASS_STREAM_CREATE_URL_FLAG, NULL, NULL);

    if (!m_hStream)
        return;

    BASS_ChannelSetSync(m_hStream, BASS_SYNC_META, 0, AudioStreamMetaSync, NULL);
    BASS_ChannelSetSync(m_hStream, BASS_SYNC_OGG_CHANGE, 0, AudioStreamMetaSync, NULL);

    BASS_ChannelPlay(m_hStream, FALSE);

    if (!pConfigFile->GetInt("audiomsgoff"))
        pChatWindow->AddInfoMessage("Audio stream: %s", szURL);

    m_fCurrentVol = 0.0f;
    m_fTargetVol = CGame::GetRadioVolume();

    m_Flags.bPlaying = true;
}

void CAudioStream::Stop()
{
    if (m_hStream)
    {
        BASS_StreamFree(m_hStream);
        m_hStream = 0;
    }

    SecureZeroMemory(m_szTitle, 256);
    SecureZeroMemory(m_szLabel, 256);

    m_fCurrentVol = 0.0f;
    m_fTargetVol = 0.0f;

    m_Flags.bPlaying = false;
}

void CAudioStream::Process()
{
    if (!m_Flags.bPlaying || !m_hStream)
        return;

    ULONGLONG now = GetTickCount64();

    CGame::StartRadio(-1);
    CGame::StopRadio();

    if (now - m_ullLastUpdate >= 50)
    {
        float baseVol = CGame::GetRadioVolume();

        if (pGame->IsMenuActive() ||
            GetForegroundWindow() != pGame->GetMainWindowHwnd())
        {
            m_fTargetVol = 0.0f;
        }
        else if (m_fDistance >= 0.0f)
        {
            CPlayerPed* ped = pGame->FindPlayerPed();

            if (ped)
            {
                float dist = ped->GetDistanceFromPoint(m_fX, m_fY, m_fZ);

                if (dist <= m_fDistance)
                {
                    float t = 1.0f - (dist / m_fDistance);
                    m_fTargetVol = baseVol * (t * t);
                }
                else
                {
                    m_fTargetVol = 0.0f;
                }
            }
            else
            {
                m_fTargetVol = 0.0f;
            }
        }
        else
        {
            m_fTargetVol = baseVol;
        }

        m_fCurrentVol = Lerp(m_fCurrentVol, m_fTargetVol, 0.15f);
        BASS_ChannelSetAttribute(m_hStream, BASS_ATTRIB_VOL, m_fCurrentVol);

        if (m_szTitle[0])
        {
            const char* tag = BASS_ChannelGetTags(m_hStream, BASS_TAG_ICY);
            if (!tag)
                tag = BASS_ChannelGetTags(m_hStream, BASS_TAG_HTTP);

            if (tag)
            {
                for (; *tag; tag += strlen(tag) + 1)
                {
                    if (!_strnicmp(tag, "icy-name:", 9))
                    {
                        sprintf_s(m_szLabel, 256, "%s - %s",
                            m_szTitle, tag + 9);
                    }
                }
            }
        }

        m_ullLastUpdate = now;
    }

    if (pDefaultFont &&
        !pGame->IsMenuActive() &&
        m_szLabel[0])
    {
        RECT r;
        r.left = 15;
        r.top = pGame->GetScreenHeight() - 20;
        r.right = pGame->GetScreenWidth();
        r.bottom = r.top + 30;

        pDefaultFont->RenderText(m_szLabel, r, 0x99FFFFFF);
    }
}

void CALLBACK CAudioStream::AudioStreamMetaSync(
    HSYNC, DWORD, DWORD, void*)
{
    if (!pAudioStream || !pAudioStream->m_szTitle)
        return;

    char* buf = pAudioStream->m_szTitle;
    SecureZeroMemory(buf, 256);

    const char* meta = BASS_ChannelGetTags(
        pAudioStream->m_hStream, BASS_TAG_META);

    if (meta)
    {
        const char* p1 = strstr(meta, "StreamTitle='");
        if (p1)
        {
            const char* p2 = strstr(p1, "';");
            if (p2)
            {
                size_t len = p2 - (p1 + 13);
                if (len < 255)
                    strncpy_s(buf, 256, p1 + 13, len);
            }
        }
    }
    else
    {
        meta = BASS_ChannelGetTags(
            pAudioStream->m_hStream, BASS_TAG_OGG);

        if (meta)
        {
            const char* artist = NULL;
            const char* title = NULL;

            for (const char* p = meta; *p; p += strlen(p) + 1)
            {
                if (!_strnicmp(p, "artist=", 7))
                    artist = p + 7;
                if (!_strnicmp(p, "title=", 6))
                    title = p + 6;
            }

            if (title)
            {
                if (artist)
                    sprintf_s(buf, 256, "%s - %s", artist, title);
                else
                    strncpy_s(buf, 256, title, _TRUNCATE);
            }
        }
    }
}

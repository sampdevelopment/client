#pragma once
#include <string>
#include <memory>
#include <d3dx9.h>
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"

#define MAX_DIALOG_CAPTION 256

class CDialog
{
private:
    IDirect3DDevice9* m_pDevice;
    std::unique_ptr<CDXUTDialog> m_pDialog;
    std::unique_ptr<CDXUTListBox> m_pListBox;
    std::unique_ptr<CDXUTIMEEditBox> m_pEditBox;

    int m_iWidth;
    int m_iHeight;
    int m_i20;
    int m_i24;
    int m_iScreenOffsetX;
    int m_iScreenOffsetY;

    bool m_bVisible;
    bool m_bSend;
    int m_iID;
    int m_iStyle;

    std::string m_szCaption;
    std::string m_szInfo;
    SIZE m_InfoSize;

public:
    CDialog(IDirect3DDevice9* pDevice);

    void ResetDialogControls();
    bool MsgProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LONG GetTextWidth(const char* szText);
    LONG GetFontHeight();
    SIZE MeasureText(const char* szText, int maxWidth = 0);
    void UpdateLayout();
    void GetRect(RECT* rect);
    bool IsCandicateActive();
    void Draw();
    void Show(int iID, int iStyle, const char* szCaption, const char* szText,
              const char* szButton1 = nullptr, const char* szButton2 = nullptr, bool bSend = false);
    void Hide();
};

#include "dialog.h"
#include "main.h"
#include <d3dx9.h>

#define IDC_DLGEDITBOX 19
#define IDC_DLGBUTTON1 20
#define IDC_DLGBUTTON2 21

VOID CALLBACK OnDialogEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);

std::string TokensStringUntilNewLine(const std::string& src)
{
    size_t pos = src.find('\n');
    if (pos != std::string::npos)
        return src.substr(0, pos);
    return src;
}

CDialog::CDialog(IDirect3DDevice9* pDevice)
    : m_pDevice(pDevice),
      m_iWidth(600),
      m_iHeight(300),
      m_i20(100),
      m_i24(30),
      m_bVisible(false),
      m_iScreenOffsetX(0),
      m_iScreenOffsetY(0),
      m_bSend(false),
      m_iID(-1),
      m_iStyle(0)
{
}

void CDialog::ResetDialogControls()
{
    m_pDialog = std::make_unique<CDXUTDialog>();
    if (!m_pDialog) return;

    m_pDialog->Init(pDialogResourceManager);
    m_pDialog->SetCallback(OnDialogEvent);
    m_pDialog->SetLocation(0, 0);
    m_pDialog->SetSize(m_iWidth, m_iHeight);
    m_pDialog->EnableMouseInput(true);
    m_pDialog->EnableKeyboardInput(true);
    m_pDialog->SetBackgroundColors(D3DCOLOR_ARGB(220, 5, 5, 5));
    m_pDialog->SetVisible(false);

    m_pListBox = std::make_unique<CDXUTListBox>(m_pDialog.get());
    m_pDialog->AddControl(m_pListBox.get());
    m_pListBox->SetLocation(10, 10);
    m_pListBox->SetSize(m_iWidth, m_iHeight - 100);
    m_pListBox->OnInit();
    m_pListBox->GetElement(0)->TextureColor.Init(D3DCOLOR_ARGB(200, 255, 255, 255));
    m_pListBox->m_nColumns = 0;
    m_pListBox->SetEnabled(false);
    m_pListBox->SetVisible(false);

    m_pDialog->AddButton(IDC_DLGBUTTON1, "BUTTON1", 10, 5, m_i20, m_i24);
    m_pDialog->AddButton(IDC_DLGBUTTON2, "BUTTON2", 110, 5, m_i20, m_i24);

    m_pEditBox = std::make_unique<CDXUTIMEEditBox>();
    m_pDialog->AddIMEEditBox(IDC_DLGEDITBOX, "", 10, 175, 570, 40, true, &m_pEditBox);
    if (pConfigFile->GetInt("ime"))
    {
        CDXUTIMEEditBox::EnableImeSystem(true);
        CDXUTIMEEditBox::StaticOnCreateDevice();
    }

    m_pEditBox->GetElement(0)->TextureColor.Init(D3DCOLOR_ARGB(240, 5, 5, 5));
    m_pEditBox->SetTextColor(D3DCOLOR_ARGB(255, 255, 255, 255));
    m_pEditBox->SetCaretColor(D3DCOLOR_ARGB(255, 150, 150, 150));
    m_pEditBox->SetSelectedBackColor(D3DCOLOR_ARGB(255, 185, 34, 40));
    m_pEditBox->SetSelectedTextColor(D3DCOLOR_ARGB(255, 10, 10, 15));
    m_pEditBox->SetEnabled(false);
    m_pEditBox->SetVisible(false);
}

bool CDialog::MsgProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CDXUTIMEEditBox::StaticMsgProc(uMsg, wParam, lParam) != 0;
}

LONG CDialog::GetTextWidth(const char* szText)
{
    if (!szText || !*szText || !m_pDialog) return -1;
    ID3DXFont* pFont = m_pDialog->GetFont(1)->pFont;
    if (!pFont) return -1;

    char szBuf[132];
    strcpy_s(szBuf, szText);
    RemoveColorEmbedsFromString(szBuf);

    RECT rect{};
    pFont->DrawTextA(NULL, szBuf, -1, &rect, DT_CALCRECT, D3DCOLOR_ARGB(255, 255, 255, 255));
    return rect.right - rect.left;
}

LONG CDialog::GetFontHeight()
{
    return m_pDialog->GetFont(1)->nHeight;
}

SIZE CDialog::MeasureText(const char* szText, int maxWidth)
{
    SIZE size{};
    if (pDefaultFont && szText)
        pDefaultFont->MeasureSmallerText(&size, szText, maxWidth);
    return size;
}

void CDialog::UpdateLayout()
{
    int minWidth = 2 * m_i20 + 10;
    if (m_iWidth < minWidth) m_iWidth = minWidth;

    m_pDialog->SetSize(m_iWidth, m_iHeight);

    RECT rect;
    GetClientRect(pGame->GetMainWindowHwnd(), &rect);
    m_iScreenOffsetX = (rect.right - m_iWidth) / 2;
    m_iScreenOffsetY = (rect.bottom - m_iHeight) / 2;
    m_pDialog->SetLocation(m_iScreenOffsetX, m_iScreenOffsetY);

    auto pButton1 = m_pDialog->GetControl(IDC_DLGBUTTON1);
    auto pButton2 = m_pDialog->GetControl(IDC_DLGBUTTON2);
    int formHeight = m_iHeight - m_pDialog->GetCaptionHeight();

    if (pButton2->GetVisible())
    {
        pButton1->SetLocation((m_iWidth - 2 * m_i20 - 10) / 2, formHeight - m_i24 - 10);
        pButton2->SetLocation(pButton1->GetLocation().x + m_i20 + 10, formHeight - m_i24 - 10);
    }
    else
    {
        pButton1->SetLocation((m_iWidth - m_i20) / 2, formHeight - m_i24 - 10);
    }

    LONG lHeight = LONG(m_pDialog->GetFont(0)->nHeight * -1.5f);
    m_pEditBox->SetSize(m_iWidth - 10, 14 - lHeight);
    lHeight = LONG(m_pDialog->GetFont(0)->nHeight * 1.5f);
    m_pEditBox->SetLocation(5, formHeight - lHeight - m_i24 - 24);
}

bool CDialog::IsCandicateActive()
{
    return m_pEditBox && CDXUTIMEEditBox::IsCandidateActive();
}

void CDialog::GetRect(RECT* rect)
{
    rect->left = m_iScreenOffsetX;
    rect->right = m_iScreenOffsetX + m_iWidth;
    rect->top = m_iScreenOffsetY;
    rect->bottom = m_iScreenOffsetY + m_iHeight;
}

void CDialog::Draw()
{
    pGame->ToggleKeyInputsDisabled(2);
    if (m_pDialog)
        m_pDialog->OnRender(10.0f);
}

void CDialog::Show(int iID, int iStyle, const char* szCaption, const char* szText,
                   const char* szButton1, const char* szButton2, bool bSend)
{
    if (iID < 0) { Hide(); return; }

    if (pCmdWindow && pCmdWindow->isEnabled())
        pCmdWindow->Disable();

    m_iID = iID;
    m_iStyle = iStyle;
    m_bSend = bSend;

    m_szCaption = szCaption ? szCaption : "";
    m_szInfo = szText ? szText : "";
    m_InfoSize = MeasureText(m_szInfo.c_str(), 64);

    if (!m_szCaption.empty())
    {
        SIZE size{};
        pDefaultFont->MeasureSmallerText(&size, "Y", 0);
        strcpy_s(m_pDialog->GetCaptionText(), 256, m_szCaption.c_str());
        m_pDialog->EnableCaption(true);
        m_pDialog->SetCaptionHeight(size.cy + 4);
    }

    switch (m_iStyle)
    {
    case DIALOG_STYLE_MSGBOX:
        m_iWidth = m_InfoSize.cx + 40;
        m_iHeight = m_InfoSize.cy + m_pDialog->GetCaptionHeight() + m_i24 + 25;
        m_pEditBox->SetVisible(false); m_pEditBox->SetEnabled(false);
        m_pListBox->SetVisible(false); m_pListBox->SetEnabled(false);
        break;
    case DIALOG_STYLE_INPUT:
        m_iWidth = m_InfoSize.cx + 40;
        m_iHeight = 0;
        m_pEditBox->SetVisible(true); m_pEditBox->SetEnabled(true);
        m_pEditBox->SetText("");
        m_pListBox->SetVisible(false); m_pListBox->SetEnabled(false);
        break;
    case DIALOG_STYLE_PASSWORD:
        m_pEditBox->SetText("");
        m_pListBox->SetVisible(false); m_pListBox->SetEnabled(false);
        break;
    case DIALOG_STYLE_LIST:
    case DIALOG_STYLE_TABLIST:
    case DIALOG_STYLE_TABLIST_HEADERS:
        break;
    }
}

void CDialog::Hide()
{
    pGame->ToggleKeyInputsDisabled(0);
    if (m_pDialog) m_pDialog->SetVisible(false);
    m_bVisible = false;
}

VOID CALLBACK OnDialogEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
}

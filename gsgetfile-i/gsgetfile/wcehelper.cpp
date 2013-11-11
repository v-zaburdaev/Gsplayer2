#include "resource.h"
#include "windows.h"
#include "wcehelper.h"
#include "defstring.h" // add by Y.N

// global helpers
void SetFormatSize(DWORD dwSize, TCHAR szBuff[64], LPTSTR pszFmtKB, LPTSTR pszFmtMB)
{
	double fSize;
	TCHAR szSize[64];
	if (dwSize < 1024000)
		fSize = (double)dwSize/1024;	
	else
		fSize = (double)dwSize/1048576;
	swprintf(szSize, _T("%f"), fSize);
	SetFormatDouble(szSize, 64);
		
	if (dwSize < 1024000)
		swprintf(szBuff, pszFmtKB, szSize);
	else
		swprintf(szBuff, pszFmtMB, szSize);
}

void SetFormatDouble(LPTSTR pszDouble, UINT nSize)
{
	LPTSTR psz = new TCHAR[nSize];
	memset(psz, 0, sizeof(TCHAR) * nSize);
	GetNumberFormat(LOCALE_USER_DEFAULT, 0, pszDouble, NULL, psz, nSize);
	_tcscpy(pszDouble, psz);
	delete[] psz;
}

void SetFormatDateTime(SYSTEMTIME* pst, LPTSTR pszBuff, UINT nSize)
{
	LPTSTR psz = new TCHAR[nSize];
	memset(psz, 0, sizeof(TCHAR) * nSize);

	GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, pst, NULL, psz, nSize);
	memset(pszBuff, 0, sizeof(TCHAR) * nSize);
	_tcscpy(pszBuff, psz);
	memset(psz, 0, sizeof(TCHAR) * nSize);
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, pst, NULL, psz, nSize);
	_tcscat(pszBuff, _T(" "));
	_tcscat(pszBuff, psz);
	delete[] psz;
}

HFONT CreatePointFont(int nPointSize, LPCTSTR pszFaceName, BOOL fBold)
{
#ifdef _WIN32_WCE
	LOGFONT logFont;
	memset(&logFont, 0, sizeof(LOGFONT));
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfHeight = nPointSize;
	if (fBold)
		logFont.lfWeight = 700;
	_tcscpy(logFont.lfFaceName, pszFaceName);

	HDC hDC = ::GetDC(NULL);

	POINT pt;
	pt.y = ::GetDeviceCaps(hDC, LOGPIXELSY) * logFont.lfHeight;
	pt.y /= 720;
	POINT ptOrg = { 0, 0 };
	logFont.lfHeight = -abs(pt.y - ptOrg.y);
	ReleaseDC(NULL, hDC);

	return CreateFontIndirect(&logFont);
#else
	return NULL;
#endif
}

// class CWinceHepler
CWinceHepler::CWinceHepler() : 
m_hAygShell(NULL), m_pSHInitDialog(NULL),
m_hCoreDll(NULL), m_pSipGetInfo(NULL),
m_bPocketPC(FALSE),m_bPPC2K3_SE(FALSE),m_bWM5(FALSE),
m_hNote_Prj(NULL)
{
	m_hCoreDll = LoadLibrary(_T("coredll.dll"));
	if (m_hCoreDll) {
		(FARPROC&)m_pSipGetInfo = GetProcAddress(m_hCoreDll, (LPCTSTR)1172);
	}

	m_bPocketPC = ((GetFileAttributes(_T("\\Windows\\shell32.exe"))&(0x80000000|FILE_ATTRIBUTE_INROM))==FILE_ATTRIBUTE_INROM);

	if(m_bPocketPC)
	{
		m_hAygShell = LoadLibrary(_T("aygshell.dll"));
		if (m_hAygShell) {
			(FARPROC&)m_pSHInitDialog = GetProcAddress(m_hAygShell, (LPCTSTR)56);
			(FARPROC&)m_pSHCreateMenuBar = GetProcAddress(m_hAygShell, (LPCTSTR)34);
			(FARPROC&)m_pSHInitExtraControls = GetProcAddress(m_hAygShell, (LPCTSTR)9);

			BOOL bRapier=!(GetFileAttributes(_T("\\Windows\\connmgr.exe"))&FILE_ATTRIBUTE_INROM);
			(FARPROC&)m_pSHChangeNotifyRegister = GetProcAddress(m_hAygShell, (LPCTSTR)(bRapier?5:113));
			(FARPROC&)m_pSHChangeNotifyDeregister = GetProcAddress(m_hAygShell, (LPCTSTR)(bRapier?6:114));
			(FARPROC&)m_pSHChangeNotifyFree = GetProcAddress(m_hAygShell, (LPCTSTR)(bRapier?7:115));
		}
	
		m_pSHInitExtraControls();

		m_hCeShell = LoadLibrary(_T("shellres.dll"));
		if(!m_hCeShell)
		{
			m_hCeShell = LoadLibrary(_T("ceshell.dll"));
			m_hNote_Prj = LoadLibrary(_T("note_prj.dll"));
		}
		else
		{
			m_hNote_Prj = LoadLibrary(_T("shellresapps.dll"));
			m_bPPC2K3_SE = TRUE;
			if(!FindResource(m_hNote_Prj,(LPCWSTR)13445,RT_DIALOG))
				m_bWM5=TRUE;
		}
	}
	else
		m_hCeShell = LoadLibrary(_T("ceshell.dll"));

}

CWinceHepler::~CWinceHepler()
{
	if (m_hAygShell)
		FreeLibrary(m_hAygShell);
	if (m_hCoreDll)
		FreeLibrary(m_hCoreDll);
	if (m_hCeShell)
		FreeLibrary(m_hCeShell);
	if (m_hNote_Prj)
		FreeLibrary(m_hNote_Prj);
}

BOOL CWinceHepler::DefDlgCtlColorStaticProc(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	if (IsPocketPC()) {
		int id = GetDlgCtrlID((HWND)lParam);
		if (id == IDC_TITLE ||
			id == IDC_STATIC_CURRENT_TEXT)
		{
			HDC hDC = (HDC)wParam;
			SetBkMode(hDC, TRANSPARENT);
			SetTextColor(hDC, GetSysColor(COLOR_HIGHLIGHT));
			return (long)GetSysColorBrush(COLOR_STATIC);
		}
		else
			return FALSE;
	}
	return FALSE;
}

void CWinceHepler::SHInitDialog(HWND hwndDlg)
{
	if (m_hAygShell && m_pSHInitDialog) {
		SHINITDLGINFO shidi;
		shidi.dwMask = SHIDIM_FLAGS;
		shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIZEDLGFULLSCREEN;
		shidi.hDlg = hwndDlg;
		m_pSHInitDialog(&shidi);
	}
}

HWND CWinceHepler::SHCreateMenuBar(HWND hwndParent, HINSTANCE hInst, int nMenuID, int nBmpID, int cBmpImages, DWORD dwFlags)
{
	if (m_pSHCreateMenuBar) {
		SHMENUBARINFO mbi;
		memset(&mbi, 0, sizeof(SHMENUBARINFO));
		mbi.cbSize = sizeof(SHMENUBARINFO);
		mbi.hwndParent = hwndParent;
		mbi.nToolBarId = nMenuID;
		mbi.hInstRes = hInst;
		mbi.nBmpId = nBmpID;
		mbi.cBmpImages = cBmpImages;
		mbi.dwFlags = dwFlags;

		if (m_pSHCreateMenuBar(&mbi)) 
			return mbi.hwndMB;
	}
	return NULL;
}

int CWinceHepler::SipPanelHeight()
{
	if (m_pSipGetInfo) {
		SIPINFO si;
		memset(&si, 0, sizeof(SIPINFO));
		si.cbSize = sizeof(SIPINFO);
		m_pSipGetInfo(&si);
		if ((si.fdwFlags & SIPF_ON))
			return si.rcSipRect.bottom-si.rcSipRect.top;
	}
	return 0;
}

BOOL CWinceHepler::DummyProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(IsWindowVisible(hwndDlg))
		ShowWindow(hwndDlg, SW_HIDE);
	return FALSE;
}

int CWinceHepler::LoadString(UINT uID, LPTSTR lpBuffer, int cchBufferMax)
{
	HINSTANCE hInst = m_hCeShell;
	UINT id=0;
	HMENU hMenu;
	MENUITEMINFO mii;
	switch(uID)
	{
	case IDS_OPEN_TITLE:
	case IDS_SAVE_TITLE:
		{
			if(m_bPocketPC)
			{
				hInst = m_hNote_Prj;
				id=(uID==IDS_OPEN_TITLE)?52:51;
				if(m_bPPC2K3_SE)
					id+=13454;
			}
			else
			{
				id=(uID==IDS_OPEN_TITLE)?36868:36869;
			}
		}
		break;
	case IDS_COLUMN_NAME:
		id=(!m_bPPC2K3_SE)?3:8451;
		break;
	case IDS_COLUMN_SIZE:
	case IDS_COLUMN_TYPE:
	case IDS_COLUMN_DATE:
		{
			if(m_bPocketPC)
			{
				switch(uID)
				{
				case IDS_COLUMN_TYPE:
					id+=1;
				case IDS_COLUMN_SIZE:
					id+=1;
				case IDS_COLUMN_DATE:
					id+=4;
				}
				if(m_bPPC2K3_SE)
					id+=8448;
			}
			else
			{
				switch(uID)
				{
				case IDS_COLUMN_DATE:
					id+=1;
				case IDS_COLUMN_TYPE:
					id+=1;
				case IDS_COLUMN_SIZE:
					id+=4;
				}
			}
		}
		break;
	case IDS_DEF_DIR:
		id=m_bPPC2K3_SE?8809:36933;
		break;
	case IDS_DEF_ROOT_NAME:
		{
			LPCTSTR szBrows;
			if(m_bPPC2K3_SE)
			{
				szBrows=_T("browsres.dll");
				id=4131;
			}
			else
			{
				szBrows=_T("browser.dll");
				id=533;
			}
			hInst=LoadLibrary(szBrows);
		}
		break;
	case IDS_MSG_PATH_MUST_EXIST:
		id=m_bPPC2K3_SE?8771:36934;
		break;
	case IDS_MSG_FILE_MUST_EXIST:
		id+=1;
	case IDS_MSG_CREATE_PROMPT:
		id+=1;
	case IDS_MSG_OVERWRITE_PROMPT:
		id+=36924;
		if(m_bPPC2K3_SE)
			id-=28160;
		break;
	case IDS_CAPTION_CANCEL:
	case IDS_CAPTION_NAME:
	case IDS_CAPTION_TYPE:
		{
			int iDlg;
			if(m_bPocketPC)
			{
				if(!m_bWM5)
				{
					hInst = m_hNote_Prj;
					iDlg = m_bPPC2K3_SE?13445:112;
					id=(uID==IDS_CAPTION_NAME)?2060:2067;
				}
				else
				{
					hInst=LoadLibrary(L"tshres.dll");
					iDlg = 16210;
					id=(uID==IDS_CAPTION_NAME)?16211:16215;
				}
			}
			else
			{
				iDlg = 1002;
				id = (uID==IDS_CAPTION_NAME)?1023:1022;
			}
			if(uID==IDS_CAPTION_CANCEL)
			{
				id=IDCANCEL;
				if(!m_bPocketPC)
				{
					iDlg=1011;
				}
			}
			HWND hOrgDlg = CreateDialog(hInst, (LPCTSTR)iDlg, NULL, DummyProc);
			if(!hOrgDlg)
			{
				hOrgDlg = CreateDialog(m_hCoreDll, (LPCTSTR)1602, GetForegroundWindow(), DummyProc);
				id=(uID==IDS_CAPTION_NAME)?1703:1702;
			}
			int r=GetDlgItemText(hOrgDlg, id, lpBuffer,cchBufferMax);
			DestroyWindow(hOrgDlg);
			if(hInst!=m_hNote_Prj && hInst!=m_hCeShell)
				FreeLibrary(hInst);

			// add by Y.N
			if (_tcslen(lpBuffer) == 0) {
				return GetDefString(uID, lpBuffer, cchBufferMax);
			}

			return r;
		}
		break;
	case IDS_CAPTION_NEWFOLDER:
		id=36870;
		if(m_bPPC2K3_SE)
			id=8610;
		break;
	case IDS_MSG_INVALIDNAME:
		id=36914;
		if(m_bPPC2K3_SE)
			id=8754;
		break;
	case IDS_MSG_ALREADYEXISTS:
		id=36915;
		if(m_bPPC2K3_SE)
			id=8755;
		break;
	case IDS_MSG_NEWFOLDERERROR:
		id=36890;
		if(m_bPPC2K3_SE)
			id=8730;
		break;
	case IDS_MSG_FOLDERTOOLONG:
		id=36893;
		if(m_bPPC2K3_SE)
			id=8733;
		break;
	case IDS_MENU_UP:
	case IDS_MENU_MENU:
	case IDS_MENU_SELECTALL:
		hInst=LoadLibrary(L"browsres.dll");
		hMenu=LoadMenu(hInst,MAKEINTRESOURCE(4184));
		*lpBuffer=0;
		mii.cbSize=sizeof(MENUITEMINFO);
		mii.fMask=MIIM_TYPE;
		mii.fType=MFT_STRING;
		mii.dwTypeData=lpBuffer;
		mii.cch=cchBufferMax;
		if(uID==IDS_MENU_SELECTALL)
			GetMenuItemInfo(hMenu,22005,FALSE,&mii);
		else
			GetMenuItemInfo(hMenu,(uID==IDS_MENU_UP)?0:1,TRUE,&mii);
		FreeLibrary(hInst);

		// add by Y.N
		if (_tcslen(lpBuffer) == 0) {
			return GetDefString(uID, lpBuffer, cchBufferMax);
		}

		return wcslen(lpBuffer);
	case IDS_DETAIL:
		id++;
	case IDS_SMALLICONS:
		id+=8661;
		break;
	// add by Y.N
	case IDS_MENU_KEYCTRL:
		return GetDefString(uID, lpBuffer, cchBufferMax);
	default:
		return 0;
	}
	int r=::LoadString(hInst,id,lpBuffer,cchBufferMax);
	if(hInst!=m_hNote_Prj && hInst!=m_hCeShell)
		FreeLibrary(hInst);

	// add by Y.N
	if (_tcslen(lpBuffer) == 0) {
		r = GetDefString(uID, lpBuffer, cchBufferMax);
	}

	return r;
}

BOOL CWinceHepler::IsWM5()
{
	return m_bWM5;
}

BOOL CWinceHepler::SHChangeNotifyRegister(HWND hwnd,SHCHANGENOTIFYENTRY * pshcne)
{
	if(m_pSHChangeNotifyRegister)
		return m_pSHChangeNotifyRegister(hwnd,pshcne);
	return FALSE;
}

BOOL CWinceHepler::SHChangeNotifyDeregister(HWND hwnd)
{
	if(m_pSHChangeNotifyDeregister)
		return m_pSHChangeNotifyDeregister(hwnd);
	return FALSE;
}

void CWinceHepler::SHChangeNotifyFree(FILECHANGENOTIFY *pfcn)
{
	if(m_pSHChangeNotifyFree)
		m_pSHChangeNotifyFree(pfcn);
}

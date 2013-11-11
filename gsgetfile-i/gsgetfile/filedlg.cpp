#include "resource.h"
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <commdlg.h>
#include "filedlg.h"


const static int s_nListColumnFmt[] = 
{
	LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT
};

const static int s_nListColumnWidth[] = 
{
	145, 65, 75, 120
};

const static UINT s_nListColumnTextId[] = 
{
	IDS_COLUMN_NAME, IDS_COLUMN_SIZE, IDS_COLUMN_TYPE, IDS_COLUMN_DATE
};

static LPCTSTR bslash = _T("\\");

// public members
CFileDialog::CFileDialog(LPOPENFILENAME pofn)
{
	InitCommonControls();
	m_pofn = pofn;
	m_hwndDlg = NULL;
	m_hFnt = NULL;
	
	m_nListSort = LIST_SORT_NAME;
	m_fSortOrder = TRUE;
	m_fShowExt = FALSE;
	
	m_fCtrl = FALSE;
	m_pszFilter = NULL;
	m_pszDefExt = NULL;
	
	m_pOrgListView = NULL;
	
	m_hInst=pofn->hInstance;
}

CFileDialog::~CFileDialog()
{
	if (m_hFnt)
		DeleteObject(m_hFnt);
}

int CFileDialog::DoModal(BOOL fSave)
{
	if (!m_pofn)
		return IDCANCEL;
	
	m_fSave = fSave;
	int nResource = GetDlgResourceID();
	
	return DialogBoxParam(m_hInst, MAKEINTRESOURCE(nResource), 
		m_pofn->hwndOwner, FileDlgProc, (LPARAM)this);
}

LRESULT CALLBACK ListViewProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#if (_WIN32_WCE >= 300 || _WIN32_WCE_EMULATION)
	WNDPROC p = (WNDPROC)GetWindowLong(hwnd, GWL_USERDATA);
#else
	FARPROC p = (FARPROC)GetWindowLong(hwnd, GWL_USERDATA);
#endif
	if (uMsg == WM_GETDLGCODE)
		return DLGC_WANTALLKEYS;
	return CallWindowProc(p, hwnd, uMsg, wParam, lParam);
}

// protected members
BOOL CFileDialog::OnInitDialog(HWND hwndDlg)
{
	m_hwndDlg = hwndDlg;
	m_hwndLV = GetDlgItem(m_hwndDlg, IDC_LIST_FILE);
	if (m_fDlgType == DLG_TYPE_PPC3)
		m_helper.SHInitDialog(m_hwndDlg);
	
	INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_BAR_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&iccex);
	
	m_pOrgListView = (WNDPROC)GetWindowLong(m_hwndLV, GWL_WNDPROC);
	if (m_pOrgListView) {
		SetWindowLong(m_hwndLV, GWL_WNDPROC, (DWORD)ListViewProc);
		SetWindowLong(m_hwndLV, GWL_USERDATA, (DWORD)m_pOrgListView);
	}
	
	WCHAR caption[32];
	m_helper.LoadString(IDS_CAPTION_CANCEL,caption,32);//Cancel
	SetDlgItemText(m_hwndDlg, IDCANCEL, caption);
	m_helper.LoadString(IDS_CAPTION_NAME,caption,32);//Name:
	SetDlgItemText(m_hwndDlg,IDC_STATIC_NAME,caption);
	m_helper.LoadString(IDS_CAPTION_TYPE,caption,32);//Type:
	SetDlgItemText(m_hwndDlg,IDC_STATIC_TYPE,caption);

	// add by Y.N
	if (m_helper.IsWM5()) {
		ShowWindow(GetDlgItem(m_hwndDlg, IDCANCEL), SW_HIDE);
	}

	GetShellSettings();
	CreateToolBar();
	CheckWindowSize();
	ParseFilter();
	CreateExtList();
	SendMessage(GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER), CB_SETEXTENDEDUI, 1, 0);
	
	InitListView();
	
	SetDlgItemText(m_hwndDlg, IDC_EDIT_NAME, m_pofn->lpstrFile);
	
	LPTSTR psz;
	TCHAR sz[MAX_LOADSTRING];
	if (m_pofn->lpstrInitialDir &&
		_tcslen(m_pofn->lpstrInitialDir) &&
		LoadFolderItem(m_pofn->lpstrInitialDir))
		goto done;
	
	_tcscpy(sz, m_pofn->lpstrFile);
	if (_tcslen(sz) && LoadFolderItem(sz))
		goto done;
	
	psz = _tcsrchr(sz, _T('\\'));
	if (psz) {
		SetDlgItemText(m_hwndDlg, IDC_EDIT_NAME, psz + 1);
		*psz = NULL;
	}
	if (_tcslen(sz) && LoadFolderItem(sz))
		goto done;
	
	m_helper.LoadString(IDS_DEF_DIR, sz, MAX_LOADSTRING);
	if (LoadFolderItem(sz))
		goto done;
	
	LoadFolderItem(bslash);
	
done:
	if (m_fSave)
		SetFocus(GetDlgItem(m_hwndDlg, IDC_EDIT_NAME));
	else
		SetFocus(GetDlgItem(m_hwndDlg, IDC_LIST_FILE));

	ChangeListStyle(LVS_LIST); // add by Y.N
	SetActiveWindow(NULL);
	SetForegroundWindow(m_hwndDlg);
	return FALSE;
}

void CFileDialog::OnOK()
{
	DWORD dwAttr;
	LPTSTR pszSrc = NULL, pszPath = NULL, p;
	TCHAR szMsg[MAX_LOADSTRING];
	TCHAR szTitle[MAX_LOADSTRING];
	
	if (m_pofn->lpstrTitle && _tcslen(m_pofn->lpstrTitle))
	{
		_tcsncpy(szTitle, m_pofn->lpstrTitle, MAX_LOADSTRING);
	}
	else
	{
		m_helper.LoadString(m_fSave ?  IDS_SAVE_TITLE : IDS_OPEN_TITLE, szTitle, MAX_LOADSTRING);
	}
	
	int n = GetWindowTextLength(GetDlgItem(m_hwndDlg, IDC_EDIT_NAME));
	if (!n)	return;
	
	SendMessage(GetDlgItem(m_hwndDlg, IDC_EDIT_NAME), EM_SETSEL, 0, -1);
	
	pszSrc = new TCHAR[n + 1];
	GetDlgItemText(m_hwndDlg, IDC_EDIT_NAME, pszSrc, n + 1);
	
	if (_tcschr(pszSrc, _T('/')) ||
		_tcschr(pszSrc, _T(':')) ||
		_tcschr(pszSrc, _T(';')) ||
		_tcschr(pszSrc, _T('?')) ||
		_tcschr(pszSrc, _T(':')) ||
		_tcschr(pszSrc, _T('<')) ||
		_tcschr(pszSrc, _T('>')) ||
		_tcschr(pszSrc, _T('|')))
		goto done;
	
	if (_tcschr(pszSrc, _T('*'))) {
		// search
		if (m_pszFilter) {
			delete [] m_pszFilter;
			m_pszFilter = NULL;
		}
		m_pszFilter = pszSrc; pszSrc = NULL;
		LoadFolderItem(m_szCurrent, FALSE);
		goto done;
	}
	else if (_tcschr(pszSrc, _T('\"'))) {
		// multi
		if (m_fSave || !(m_pofn->Flags & OFN_ALLOWMULTISELECT))
			goto done;
		
		n = m_pofn->nMaxFile;
		pszPath = m_pofn->lpstrFile;
		p = _tcslen(m_szCurrent) ? m_szCurrent : (LPTSTR)bslash;
		_tcsncpy(pszPath, p, n);
		n -= _tcslen(pszPath) + 2;
		pszPath += _tcslen(pszPath);
		*pszPath++ = NULL;
		
		p = pszSrc;
		while (n > 0 && *p != NULL) {
			LPTSTR psz;
			if (*p == _T('\"'))
				p++;
			
			psz = _tcschr(p, _T('\"'));
			if (m_pofn->Flags & OFN_FILEMUSTEXIST) {
				int nLen = MAX_PATH;
				TCHAR szPath[MAX_PATH + 1] = _T("");
				
				_tcscpy(szPath, m_szCurrent);
				_tcscat(szPath, bslash);
				nLen -= _tcslen(m_szCurrent) + 1;
				if (psz)
					_tcsncat(szPath, p, min(psz - p, nLen));
				else
					_tcsncat(szPath, p, nLen);
				dwAttr = GetFileAttributes(szPath);
				if (dwAttr == 0xFFFFFFFF ||
					(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
					m_helper.LoadString(IDS_MSG_FILE_MUST_EXIST, szMsg, MAX_LOADSTRING);
					p = new TCHAR[_tcslen(szPath) + _tcslen(szMsg) + 1];
					wsprintf(p, szMsg, szPath);
					MessageBox(m_hwndDlg, p, szTitle, MB_ICONEXCLAMATION | MB_OK);
					delete [] p;
					
					pszPath = NULL;
					goto done;
				}
			}
			
			if (psz) {
				_tcsncpy(pszPath, p, min(psz - p, n));
				n -= psz - p;
				pszPath += psz - p;
			}
			else {
				_tcsncpy(pszPath, p, n);
				n -= _tcslen(p);
				pszPath += _tcslen(p);
			}
			if (n > 0)
				*pszPath++ = NULL;
			
			if (!psz)
				break;
			
			p = psz;
			while (*p == _T(' ') || *p == _T('\"'))
				p++;
		}
		pszPath = NULL;
		
		n = (n > 1) ? IDOK : IDCANCEL;
		EndDialog(n);
	}
	else {
		// single
		n = (*pszSrc != _T('\\')) ? _tcslen(m_szCurrent) + _tcslen(pszSrc) + 1 : _tcslen(pszSrc);
		p = _tcsrchr(pszSrc, _T('.'));
		if (!p || _tcschr(p, _T('\\'))) {
			if (m_pszDefExt)
				n += _tcslen(m_pszDefExt);
			else if (m_pofn->lpstrDefExt) {
				n += _tcslen(m_pofn->lpstrDefExt) + 1;
			}
			else {
				p = _tcschr((LPTSTR)m_listExt.GetAt(0), _T('.'));
				if (p && !_tcschr(p, _T('*')))
					n += _tcslen(p);
			}
		}
		else {
			if (m_pofn->lpstrDefExt && m_pszDefExt)
				n += max(_tcslen(m_pszDefExt), _tcslen(m_pofn->lpstrDefExt) + 1);
			else if (m_pszDefExt)
				n += _tcslen(m_pszDefExt);
			else if (m_pofn->lpstrDefExt)
				n += _tcslen(m_pofn->lpstrDefExt) + 1;
		}
		pszPath = new TCHAR[n + 1];
		*pszPath = NULL;
		
		if (*pszSrc != _T('\\')) {
			_tcscpy(pszPath, m_szCurrent);
			_tcsncat(pszPath, bslash, n);
		}
		_tcscat(pszPath, pszSrc);
		
		p = _tcsrchr(pszSrc, _T('.'));
		if (!p || _tcschr(p, _T('\\'))) {
			if (m_pszDefExt) 
				_tcscat(pszPath, m_pszDefExt);
			else {
				dwAttr = GetFileAttributes(pszPath);
				if (dwAttr != 0xFFFFFFFF &&
					(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
					if (LoadFolderItem(pszPath, FALSE)) {
						goto done;
					}
				}
				else if (m_pofn->lpstrDefExt) {
					_tcscat(pszPath, _T("."));
					_tcscat(pszPath, m_pofn->lpstrDefExt);
				}
				else {
					p = _tcschr((LPTSTR)m_listExt.GetAt(0), _T('.'));
					if (p && !_tcschr(p, _T('*')))
						_tcscat(pszPath, p);
				}
			}
		}
		else {
			dwAttr = GetFileAttributes(pszPath);
			if (dwAttr == 0xFFFFFFFF ||
				(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
				if (m_pszDefExt)
					_tcscat(pszPath, m_pszDefExt);
				//else if (m_pofn->lpstrDefExt)
				//	_tcscat(pszPath, m_pofn->lpstrDefExt);
			}
		}
		if (!m_fSave && (m_pofn->Flags & OFN_FILEMUSTEXIST)) {
			dwAttr = GetFileAttributes(pszPath);
			if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
				m_helper.LoadString(IDS_MSG_FILE_MUST_EXIST, szMsg, MAX_LOADSTRING);
				p = new TCHAR[_tcslen(pszPath) + _tcslen(szMsg) + 1];
				wsprintf(p, szMsg, pszPath);
				MessageBox(m_hwndDlg, p, szTitle, MB_ICONEXCLAMATION | MB_OK);
				delete [] p;
				goto done;
			}
		}
		if (!m_fSave && (m_pofn->Flags & OFN_CREATEPROMPT)) {
			dwAttr = GetFileAttributes(pszPath);
			if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
				m_helper.LoadString(IDS_MSG_CREATE_PROMPT, szMsg, MAX_LOADSTRING);
				p = new TCHAR[_tcslen(pszPath) + _tcslen(szMsg) + 1];
				wsprintf(p, szMsg, pszPath);
				n = MessageBox(m_hwndDlg, p, szTitle, MB_ICONQUESTION | MB_YESNO);
				delete [] p;
				if (n == IDNO)
					goto done;
			}
		}
		if (m_fSave && (m_pofn->Flags & OFN_PATHMUSTEXIST)) {
			p = _tcsrchr(pszPath, _T('\\'));
			if (p) *p = NULL;
			dwAttr = GetFileAttributes(pszPath);
			if (p) *p = _T('\\');
			
			if (dwAttr == 0xFFFFFFFF ||
				!(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
				m_helper.LoadString(IDS_MSG_PATH_MUST_EXIST, szMsg, MAX_LOADSTRING);
				p = new TCHAR[_tcslen(pszPath) + _tcslen(szMsg) + 1];
				wsprintf(p, szMsg, pszPath);
				MessageBox(m_hwndDlg, p, szTitle, MB_ICONEXCLAMATION | MB_OK);
				delete [] p;
				goto done;
			}
		}
		if (m_fSave && (m_pofn->Flags & OFN_OVERWRITEPROMPT)) {
			dwAttr = GetFileAttributes(pszPath);
			if (dwAttr != 0xFFFFFFFF && 
				!(dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
				m_helper.LoadString(IDS_MSG_OVERWRITE_PROMPT, szMsg, MAX_LOADSTRING);
				p = new TCHAR[_tcslen(pszPath) + _tcslen(szMsg) + 1];
				wsprintf(p, szMsg, pszPath);
				n = MessageBox(m_hwndDlg, p, szTitle, MB_ICONEXCLAMATION | MB_YESNO);
				delete [] p;
				if (n == IDNO)
					goto done;
			}
		}
		_tcsncpy(m_pofn->lpstrFile, pszPath, m_pofn->nMaxFile - 1);
		m_pofn->lpstrFile[m_pofn->nMaxFile - 1] = NULL;
		if (m_pofn->lpstrFileTitle && m_pofn->nMaxFileTitle) {
			p = _tcsrchr(pszPath, _T('\\'));
			if (p) 
				_tcsncpy(m_pofn->lpstrFileTitle, p + 1, m_pofn->nMaxFileTitle - 1);
			else
				_tcsncpy(m_pofn->lpstrFileTitle, pszPath, m_pofn->nMaxFileTitle - 1);
			m_pofn->lpstrFileTitle[m_pofn->nMaxFileTitle - 1] = NULL;
		}
		
		n = (_tcslen(pszPath) + 1 <= m_pofn->nMaxFile) ? IDOK : IDCANCEL;
		EndDialog(n);
	}
	
done:
	if (pszPath)
		delete [] pszPath;
	if (pszSrc)
		delete [] pszSrc;
}

void CFileDialog::EndDialog(int nResult)
{
	if (nResult == IDOK) {
		int n = SendMessage(GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER), CB_GETCURSEL, 0, 0);
		if (n != CB_ERR && m_pofn->lpstrFilter)
			m_pofn->nFilterIndex = n + 1;
	}	
	if (m_fCtrl) {
		keybd_event(VK_CONTROL, 0x1e, KEYEVENTF_KEYUP, 1);
	}
	if (m_pszFilter) {
		delete [] m_pszFilter;
		m_pszFilter = NULL;
	}
	if (m_pszDefExt) {
		delete [] m_pszDefExt;
		m_pszDefExt = NULL;
	}
	if (m_pOrgListView) {
		SetWindowLong(m_hwndLV, GWL_WNDPROC, (DWORD)m_pOrgListView);
		m_pOrgListView = NULL;
	}
	
	m_helper.SHChangeNotifyDeregister(m_hwndDlg);
	ClearFilter();
	DeleteExtList();
	DestroyListView();
	CommandBar_Destroy(m_hwndCB);
	::EndDialog(m_hwndDlg, nResult);
}


BOOL CALLBACK CFileDialog::FileDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	
	CFileDialog* pDlg = (CFileDialog*)GetWindowLong(hwndDlg, DWL_USER);
	switch (uMsg) {
	case WM_INITDIALOG:
		pDlg = (CFileDialog*)lParam;
		SetWindowLong(hwndDlg, DWL_USER, (DWORD)lParam);
		return  pDlg->OnInitDialog(hwndDlg);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			pDlg->OnOK();
			return TRUE;
		case IDCANCEL:
			pDlg->EndDialog(LOWORD(wParam));
			return TRUE;
		case ID_LIST_STYLE_LIST:
			pDlg->ChangeListStyle(LVS_LIST);
			return TRUE;
		case ID_LIST_STYLE_REPORT:
			pDlg->ChangeListStyle(LVS_REPORT);
			return TRUE;
		case ID_LIST_SELECTALL:
			pDlg->SelectAllItems();
			return TRUE;
		case ID_UP: 
			pDlg->OnUp();
			return TRUE;
		case ID_KEY_CTRL:
			pDlg->m_fCtrl = !pDlg->m_fCtrl;
			keybd_event(VK_CONTROL, 0x1e, pDlg->m_fCtrl ? 0 : KEYEVENTF_KEYUP, 1);
			{
				TBBUTTON tbb;
				SendMessage(pDlg->m_hwndCB, TB_GETBUTTON, 1, (LPARAM)&tbb);
				HMENU hMenu=HMENU(tbb.dwData);
				if(hMenu)
					CheckMenuItem(hMenu,ID_KEY_CTRL,MF_BYCOMMAND|(pDlg->m_fCtrl)?MF_CHECKED:MF_UNCHECKED);
			}
			return TRUE;
		case IDC_COMBO_FILTER:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				pDlg->OnCBSelChange();
			return TRUE;
		case ID_NEW_FOLDER:
			pDlg->NewFolderDialog();
			return TRUE;
		}
		break;
		case WM_NOTIFY:
			{
				NMHDR* pnmh = (NMHDR*)lParam;
				switch (pnmh->code) {
				case LVN_GETDISPINFO:
					pDlg->OnGetDispInfo((NMLVDISPINFO*)lParam);
					return TRUE;
				case NM_CLICK:
					pDlg->OnListClick();
					return TRUE;
				case NM_DBLCLK:
				case NM_RETURN:
					pDlg->OnListDblClk();
					return TRUE;
				case LVN_KEYDOWN:
					pDlg->OnListKeyDown((NMLVKEYDOWN*)lParam);
					return TRUE;
				case LVN_COLUMNCLICK:
					pDlg->OnListColumnClick((NMLISTVIEW*)lParam);
					return TRUE;
				case LVN_ITEMCHANGED:
					pDlg->OnListItemChanged((NMLISTVIEW*)lParam);
					return TRUE;
				}
				break;
			}
		case WM_WININICHANGE:
			SetWindowLong(hwndDlg,GWL_STYLE,GetWindowLong(hwndDlg,GWL_STYLE)&~WS_VSCROLL);
			pDlg->CheckWindowSize();
			return TRUE;
		case WM_FILECHANGEINFO:
			if(HIWORD(lParam))
			{
				pDlg->OnFileChangeInfo(&(((FILECHANGENOTIFY *)lParam)->fci));
				pDlg->m_helper.SHChangeNotifyFree((FILECHANGENOTIFY *)lParam);
			}
			return TRUE;
		case WM_CTLCOLORSTATIC:
			return pDlg->m_helper.DefDlgCtlColorStaticProc(hwndDlg, wParam, lParam);
	}
	return FALSE;
}

int CFileDialog::GetDlgResourceID()
{
	if (m_helper.IsPocketPC()) {
		RECT rc;
		HWND hwndTB = FindWindow(_T("HHTaskBar"), NULL);
		GetWindowRect(hwndTB, &rc);
		if (rc.top == 0) {
			m_fDlgType = DLG_TYPE_PPC3;
			return IDD_OPENFILEDLG_PPC;
		}
	}
	else {
		if (GetSystemMetrics(SM_CXSCREEN) > 320) {
			m_fDlgType = DLG_TYPE_LARGE;
			return IDD_OPENFILEDLG_LARGE;
		}
		else if (GetSystemMetrics(SM_CXSCREEN) > 240) {
			m_fDlgType = DLG_TYPE_MEDIUM;
			return IDD_OPENFILEDLG_MEDIUM;
		}
	}
	m_fDlgType = DLG_TYPE_SMALL;
	return IDD_OPENFILEDLG_SMALL;
}

void CFileDialog::CreateToolBar()
{
	if (m_fDlgType == DLG_TYPE_PPC3) {
		// PocketPC
		TBBUTTON buttons[] = {
			//iBitmap, idCommand, fsState, fsStyle, dwData, iString
			{VIEW_PARENTFOLDER+2, ID_UP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
			{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0},
			{VIEW_NEWFOLDER+2, ID_NEW_FOLDER, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
			{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0},
			{VIEW_LIST+2, ID_LIST_STYLE_LIST, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
			{VIEW_DETAILS+2, ID_LIST_STYLE_REPORT, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
			{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0},
			{0, ID_LIST_SELECTALL, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
			{1, ID_KEY_CTRL, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0}
		};
		if(m_helper.IsWM5())//WM5 Later
		{
			HMENU hMenu=LoadMenu(m_hInst,MAKEINTRESOURCE(IDR_TITLE));
			MENUITEMINFO mii;
			WCHAR str[32];
			mii.cbSize=sizeof(MENUITEMINFO);
			mii.fMask=MIIM_TYPE;
			mii.fType=MFT_STRING;
			mii.dwTypeData=str;
			
			m_helper.LoadString(IDS_MENU_UP, str, 32);
			SetMenuItemInfo(hMenu,ID_UP,FALSE,&mii);
			m_helper.LoadString(IDS_MENU_MENU, str, 32);
			SetMenuItemInfo(hMenu,1,TRUE,&mii);
			// remove by Y.N (It's buggy!)
			//m_helper.LoadString(IDS_CAPTION_NEWFOLDER, str, 32);
			//wcscat(str,L"...");
			//SetMenuItemInfo(hMenu,ID_NEW_FOLDER,FALSE,&mii);
			m_helper.LoadString(IDS_SMALLICONS, str, 32);
			SetMenuItemInfo(hMenu,ID_LIST_STYLE_LIST,FALSE,&mii);
			m_helper.LoadString(IDS_DETAIL, str, 32);
			SetMenuItemInfo(hMenu,ID_LIST_STYLE_REPORT,FALSE,&mii);
			int r = m_helper.LoadString(IDS_MENU_SELECTALL, str, 32);
			SetMenuItemInfo(hMenu,ID_LIST_SELECTALL,FALSE,&mii);

			// add by Y.N start
			m_helper.LoadString(IDS_MENU_KEYCTRL, str, 32);
			SetMenuItemInfo(hMenu,ID_KEY_CTRL,FALSE,&mii);
			m_helper.LoadString(IDS_CAPTION_CANCEL, str, 32);
			SetMenuItemInfo(hMenu,IDCANCEL,FALSE,&mii);
			// add by Y.N end
			
			//CheckMenuRadioItem(hMenu, ID_LIST_STYLE_LIST, ID_LIST_STYLE_REPORT, ID_LIST_STYLE_LIST, MF_BYCOMMAND); // modified by Y.N
			if(m_fSave || !(m_pofn->Flags & OFN_ALLOWMULTISELECT)) 
			{
				DeleteMenu(hMenu,ID_LIST_SELECTALL,MF_BYCOMMAND);
				DeleteMenu(hMenu,ID_KEY_CTRL,MF_BYCOMMAND);
			}

			// add by Y.N (It's buggy!)
			DeleteMenu(hMenu, ID_NEW_FOLDER, MF_BYCOMMAND);

			m_hwndCB = m_helper.SHCreateMenuBar(m_hwndDlg, m_hInst, (int)hMenu, 0, 0,0x0010/*SHCMBF_HMENU*/);
		}
		else
		{
			m_hwndCB = m_helper.SHCreateMenuBar(m_hwndDlg, m_hInst, IDR_DUMMY, IDR_TOOLBAR_PPC, 2);
			CommandBar_AddBitmap(m_hwndCB, HINST_COMMCTRL , IDB_VIEW_SMALL_COLOR, 12, 16, 16);
			if (!m_fSave && (m_pofn->Flags & OFN_ALLOWMULTISELECT)) {
				// multi select (with Ctrl)
				CommandBar_AddButtons(m_hwndCB, sizeof(buttons) / sizeof(TBBUTTON), buttons);
			}
			else {
				// single select
				CommandBar_AddButtons(m_hwndCB, 6, buttons);
			}
		}
		
		if (m_pofn->lpstrTitle && _tcslen(m_pofn->lpstrTitle)) {
			SetWindowText(m_hwndDlg, m_pofn->lpstrTitle);
			SetDlgItemText(m_hwndDlg, IDC_TITLE, m_pofn->lpstrTitle);
		}
		else {
			TCHAR sz[MAX_LOADSTRING];
			m_helper.LoadString(m_fSave ? IDS_SAVE_TITLE : IDS_OPEN_TITLE, sz, MAX_LOADSTRING);
			SetWindowText(m_hwndDlg, sz);
			SetDlgItemText(m_hwndDlg, IDC_TITLE, sz);
		}
	}
	else {
		// other device
		TCHAR sz[MAX_LOADSTRING] = _T("");
		if (m_fDlgType != DLG_TYPE_SMALL && m_pofn->lpstrTitle && _tcslen(m_pofn->lpstrTitle))
			_tcsncpy(sz, m_pofn->lpstrTitle, MAX_LOADSTRING - 1);
		else {
			m_helper.LoadString(m_fSave ? IDS_SAVE_TITLE : IDS_OPEN_TITLE, sz, MAX_LOADSTRING);
		}
		m_hwndRB = CommandBands_Create(m_hInst, m_hwndDlg, 0, WS_VISIBLE | CCS_TOP | RBBS_NOGRIPPER, NULL);
		
		REBARBANDINFO rbi;
		memset(&rbi, 0, sizeof(REBARBANDINFO));
		rbi.cbSize = sizeof (REBARBANDINFO);
		rbi.fMask = RBBIM_STYLE | RBBIM_TEXT;
		rbi.fStyle = RBBS_NOGRIPPER;
		rbi.lpText = sz;
		CommandBands_AddBands(m_hwndRB, m_hInst, 1, &rbi);
		m_hwndCB = CommandBands_GetCommandBar(m_hwndRB, 0);
		CommandBar_AddBitmap(m_hwndCB, HINST_COMMCTRL , IDB_VIEW_SMALL_COLOR, 12, 16, 16);
		
		TBBUTTON buttons[] = {
			//iBitmap, idCommand, fsState, fsStyle, dwData, iString
			{VIEW_PARENTFOLDER, ID_UP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
			{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0},
			{VIEW_NEWFOLDER, ID_NEW_FOLDER, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0},
			{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0},
			{VIEW_LIST, ID_LIST_STYLE_LIST, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0},
			{VIEW_DETAILS, ID_LIST_STYLE_REPORT, TBSTATE_ENABLED, TBSTYLE_CHECK, 0, 0}
		};
		CommandBar_AddButtons(m_hwndCB, sizeof(buttons) / sizeof(TBBUTTON), buttons); 
		CommandBands_AddAdornments(m_hwndRB, m_hInst, CMDBAR_OK, 0);
		
	}
	
	if (m_hFnt)
		DeleteObject(m_hFnt);
	m_hFnt = CreatePointFont(90, _T(""), TRUE);
	SendMessage(GetDlgItem(m_hwndDlg, IDC_STATIC_CURRENT_TEXT), WM_SETFONT, (WPARAM)m_hFnt, 0);
	if (m_fDlgType != DLG_TYPE_PPC3)
		SendMessage(m_hwndRB, WM_SETFONT, (WPARAM)m_hFnt, 0);
}

void CFileDialog::CheckWindowSize()
{
	if (m_fDlgType == DLG_TYPE_PPC3) {
		RECT rcParent;
		GetClientRect(m_hwndDlg, &rcParent);
		int nRight = rcParent.right - 5;
		
		RECT rc;
		GetWindowRect(GetDlgItem(m_hwndDlg, IDCANCEL), &rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)&rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)(&rc) + 1);
		MoveWindow(GetDlgItem(m_hwndDlg, IDCANCEL), nRight - (rc.right - rc.left), rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		
		GetWindowRect(GetDlgItem(m_hwndDlg, IDC_EDIT_NAME), &rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)&rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)(&rc) + 1);
		MoveWindow(GetDlgItem(m_hwndDlg, IDC_EDIT_NAME), rc.left, rc.top, nRight - rc.left, rc.bottom - rc.top, TRUE);
		
		GetWindowRect(GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER), &rc);
		//ComboBox_GetDroppedControlRect(GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER), &rc); 
		ScreenToClient(m_hwndDlg, (LPPOINT)&rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)(&rc) + 1);
		MoveWindow(GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER), rc.left, rc.top, nRight - rc.left, 5/*rc.bottom - rc.top*/, TRUE);
		
		nRight += 5;
		GetWindowRect(GetDlgItem(m_hwndDlg, IDC_LIST_FILE), &rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)&rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)(&rc) + 1);
		MoveWindow(GetDlgItem(m_hwndDlg, IDC_LIST_FILE), rc.left, rc.top, nRight - rc.left, rc.bottom - rc.top, TRUE);
		
		CheckListHeight();
	}
}

void CFileDialog::CheckListHeight()
{
	if (m_fDlgType == DLG_TYPE_PPC3) {
		RECT rc, rcParent;
		GetClientRect(m_hwndDlg, &rcParent);
		GetWindowRect(GetDlgItem(m_hwndDlg, IDC_LIST_FILE), &rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)&rc);
		ScreenToClient(m_hwndDlg, (LPPOINT)(&rc) + 1);
		
		int nHeight = rcParent.bottom - rc.top - m_helper.SipPanelHeight() +1;
		
		MoveWindow(GetDlgItem(m_hwndDlg, IDC_LIST_FILE), rc.left, rc.top, 
			rc.right - rc.left, nHeight, TRUE);
	}
}

void CFileDialog::ParseFilter()
{
	int nIndex;
	TCHAR szFilter[MAX_LOADSTRING];
	TCHAR szExt[MAX_LOADSTRING];
	LPTSTR pszSrc, p;
	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER);
	
	if (!m_pofn->lpstrFilter)
		goto fail;
	
	pszSrc = (LPTSTR)m_pofn->lpstrFilter;
	while (TRUE) {
		// filter
		p = szFilter;
		while (*pszSrc != NULL) {
			*p++ = *pszSrc;
			pszSrc++;
		}
		*p = NULL;
		if (*pszSrc == NULL && *(pszSrc + 1) == NULL)
			break;
		pszSrc++;
		
		// ext
		p = szExt;
		while (*pszSrc != NULL) {
			*p++ = *pszSrc;
			pszSrc++;
		}
		*p = NULL;
		if (_tcslen(szFilter) && _tcslen(szExt)) {
			int nIndex = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)szFilter);
			if (nIndex == CB_ERR)
				break;
			
			p = new TCHAR[_tcslen(szExt) + 1];
			_tcscpy(p, szExt);
			SendMessage(hwndCombo, CB_SETITEMDATA, nIndex, (LPARAM)p);
		}
		if (*pszSrc == NULL && *(pszSrc + 1) == NULL)
			break;
		pszSrc++;
	}
	
	nIndex = m_pofn->nFilterIndex > 0 ? m_pofn->nFilterIndex - 1 : 0;
	SendMessage(hwndCombo, CB_SETCURSEL, nIndex, 0);
	return;
fail:
	ClearFilter();
	*szFilter=0;
	::LoadString(m_hInst,IDS_DEF_EXT, szExt, MAX_LOADSTRING);
	p = new TCHAR[_tcslen(szExt) + 1];
	_tcscpy(p, szExt);
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)szFilter);
	SendMessage(hwndCombo, CB_SETITEMDATA, 0, (LPARAM)p);
	SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
	EnableWindow(hwndCombo,FALSE);//
}

void CFileDialog::ClearFilter()
{
	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER);
	int n = SendMessage(hwndCombo, CB_GETCOUNT, 0, 0);
	for (int i = 0; i < n; i++) {
		LPTSTR p = (LPTSTR)SendMessage(hwndCombo, CB_GETITEMDATA, i, 0);
		delete [] p;
	}
	SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);
}

void CFileDialog::GetShellSettings()
{
	HKEY hKey = 0;
	_tcscpy(m_szRootName, bslash);
	if (m_fDlgType == DLG_TYPE_PPC3)
	{
		m_helper.LoadString(IDS_DEF_ROOT_NAME, m_szRootName, MAX_LOADSTRING);
	} else if (RegOpenKeyEx(HKEY_CLASSES_ROOT, _T("{000214A0-0000-0000-C000-000000000046}"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwType, dwSize = sizeof(m_szRootName);
		RegQueryValueEx(hKey, _T("DisplayName"), 0, &dwType, (LPBYTE)m_szRootName, &dwSize);
		RegCloseKey(hKey);
	}
	
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Explorer"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD dwSize = sizeof(m_fShowExt);
		RegQueryValueEx(hKey, _T("ShowExt"), 0, NULL, (LPBYTE)&m_fShowExt, &dwSize);
		dwSize = sizeof(m_fViewAll);
		RegQueryValueEx(hKey, _T("ViewAll"), 0, NULL, (LPBYTE)&m_fViewAll, &dwSize);
		RegCloseKey(hKey);
	}
}

void CFileDialog::InitListView()
{
	TCHAR sz[MAX_LOADSTRING];
	
	if (!(m_pofn->Flags & OFN_ALLOWMULTISELECT) || m_fSave)
		SetWindowLong(m_hwndLV, GWL_STYLE, GetWindowLong(m_hwndLV, GWL_STYLE) | LVS_SINGLESEL);
	
	LVCOLUMN lvc;
	lvc.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
	lvc.pszText = sz;
	
	int scale=GetSystemMetrics(SM_CXSMICON)/16;
	for(int i=0;i<4;i++)
	{
		lvc.fmt = s_nListColumnFmt[i];
		lvc.cx = s_nListColumnWidth[i]*scale;
		lvc.iSubItem = i;
		m_helper.LoadString(s_nListColumnTextId[i], sz, MAX_LOADSTRING);
		ListView_InsertColumn(m_hwndLV, 3, &lvc);
	}
	
	m_ImageList.Init(m_fDlgType == DLG_TYPE_SMALL);
	ListView_SetImageList(m_hwndLV, (HIMAGELIST)m_ImageList, LVSIL_SMALL);
	SendMessage(m_hwndCB, TB_SETSTATE, ID_LIST_STYLE_LIST, TBSTATE_CHECKED | TBSTATE_ENABLED);
	SendMessage(m_hwndCB, TB_SETSTATE, ID_LIST_STYLE_REPORT, TBSTATE_ENABLED);
}

void CFileDialog::DestroyListView()
{
	DeleteAllListItem();
	m_ImageList.Destroy();
}

void CFileDialog::DeleteAllListItem()
{
	for (int i = 0; i < ListView_GetItemCount(m_hwndLV); i++) {
		LIST_ITEM_INFO* pDel = GetListItemInfo(i);
		if (pDel->pszDispName) delete[] pDel->pszDispName;
		if (pDel->pszDispSize) delete[] pDel->pszDispSize;
		if (pDel->pszDispType) delete[] pDel->pszDispType;
		if (pDel->pszDispTime) delete[] pDel->pszDispTime;
		delete[] pDel->pszName; 
		delete pDel;
	}
	ListView_DeleteAllItems(m_hwndLV);
}

BOOL CFileDialog::LoadFolderItem(LPCTSTR pszPath, BOOL fFocus)
{
	if (!pszPath)
		return FALSE;
	
	DWORD dwAttr = GetFileAttributes(pszPath);
	if (dwAttr == 0xFFFFFFFF ||
		!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
		return FALSE;
	
	HCURSOR hCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	ShowWindow(m_hwndLV, SW_HIDE);
	
	m_helper.SHChangeNotifyDeregister(m_hwndDlg);

	DeleteAllListItem();
	
	TCHAR sz[MAX_PATH];
	TCHAR szPath[MAX_PATH];
	WIN32_FIND_DATA wfd;
	
	_tcscpy(szPath, pszPath);
	LPTSTR psz = _tcsrchr(szPath, _T('\\'));
	if (psz && *(psz + 1) == NULL)
		*psz = NULL;
	
	// folder
	wsprintf(sz, _T("%s\\*.*"), szPath);
	HANDLE hFind = FindFirstFile(sz, &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)||m_listExt.IsEmpty()||IsFolderShortcut(szPath, wfd.cFileName))
				AddListItem(szPath, &wfd);
		}
		while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	
	// files
	if (m_pszFilter) {
		wsprintf(sz, _T("%s\\%s"), szPath, m_pszFilter);
		hFind = FindFirstFile(sz, &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					!IsFolderShortcut(szPath, wfd.cFileName))
					AddListItem(szPath, &wfd);
			}
			while (FindNextFile(hFind, &wfd));
			FindClose(hFind);
		}
	}
	else {
		int n = m_listExt.GetCount();
		for (int i = 0; i < n; i++) {
			LPTSTR p = (LPTSTR)m_listExt.GetAt(i);
			wsprintf(sz, _T("%s\\%s"), szPath, p);
			hFind = FindFirstFile(sz, &wfd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
						!IsFolderShortcut(szPath, wfd.cFileName))
						AddListItem(szPath, &wfd);
				}
				while (FindNextFile(hFind, &wfd));
				FindClose(hFind);
			}
		}
	}
	
	if (ListView_GetItemCount(m_hwndLV)) {
		SortList();
		ListView_SetItemState(m_hwndLV, 0, LVIS_FOCUSED, LVIS_FOCUSED);
	}
	
	_tcscpy(m_szCurrent, szPath);
	SetDlgItemText(m_hwndDlg, IDC_STATIC_CURRENT_TEXT, GetDisplayName(m_szCurrent));
	
	BOOL fUp = (_tcslen(szPath) && _tcscmp(szPath, bslash) != 0);
	SendMessage(m_hwndCB, TB_SETSTATE, ID_UP, fUp ? TBSTATE_ENABLED : TBSTATE_INDETERMINATE);
	
	SHCHANGENOTIFYENTRY scne={-1,m_szCurrent};
	m_helper.SHChangeNotifyRegister(m_hwndDlg,&scne);

	SetCursor(hCursor);
	ShowWindow(m_hwndLV, SW_SHOW);
	UpdateWindow(m_hwndLV);
	
	if (fFocus)
		SetFocus(m_hwndLV);
	return TRUE;
}

void CFileDialog::SortList()
{
	const PFNLVCOMPARE fnSort[]={ListSortCompareFuncByName,ListSortCompareFuncByExt,ListSortCompareFuncBySize,ListSortCompareFuncByTime};
	ListView_SortItems(m_hwndLV, fnSort[m_nListSort], m_fSortOrder);
}

void CFileDialog::AddListItem(LPCTSTR pszPath, WIN32_FIND_DATA* pwfd, int ix)
{
	LVITEM li;
	LIST_ITEM_INFO* p;
	
	if (!m_fViewAll && (pwfd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
		return;
	
	if (pwfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		p = new LIST_ITEM_INFO;
		p->type = ITEM_TYPE_DIR;
		p->pszName = new TCHAR[_tcslen(pwfd->cFileName) + 1];
		memset(p->pszName, 0, sizeof(TCHAR) * (_tcslen(pwfd->cFileName) + 1));
		_tcscpy(p->pszName, pwfd->cFileName);
		p->llSize = 0;
		p->ft = pwfd->ftCreationTime;
		
		li.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
		li.iItem = ix;
		li.iSubItem = 0;
		li.iImage = I_IMAGECALLBACK;
		li.lParam = (DWORD)p;
		li.pszText = LPSTR_TEXTCALLBACK;
		ListView_InsertItem(m_hwndLV, &li);
		ListView_SetItemText(m_hwndLV, 0, 2, LPSTR_TEXTCALLBACK);
	}
	else {
		p = new LIST_ITEM_INFO;
		p->type = ITEM_TYPE_FILE;
		p->pszName = new TCHAR[_tcslen(pwfd->cFileName) + 1];
		memset(p->pszName, 0, sizeof(TCHAR) * (_tcslen(pwfd->cFileName) + 1));
		_tcscpy(p->pszName, pwfd->cFileName);
		p->llSize = ((ULONGLONG)pwfd->nFileSizeHigh << 32) | pwfd->nFileSizeLow;
		p->ft = pwfd->ftLastWriteTime;
		FileTimeToLocalFileTime(&p->ft, &p->ft);
		li.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
		li.iItem = ix;
		li.iSubItem = 0;
		li.iImage = I_IMAGECALLBACK;
		li.lParam = (DWORD)p;
		li.pszText = LPSTR_TEXTCALLBACK;
		ListView_InsertItem(m_hwndLV, &li);
		ListView_SetItemText(m_hwndLV, 0, 1, LPSTR_TEXTCALLBACK);
		ListView_SetItemText(m_hwndLV, 0, 2, LPSTR_TEXTCALLBACK);
		ListView_SetItemText(m_hwndLV, 0, 3, LPSTR_TEXTCALLBACK);
	}
}

LIST_ITEM_INFO* CFileDialog::GetListItemInfo(int nIndex)
{
	LVITEM li;
	li.iItem = nIndex;
	li.iSubItem = 0;
	li.mask = LVIF_PARAM;
	ListView_GetItem(m_hwndLV, &li);
	return (LIST_ITEM_INFO*)li.lParam;
}

int CFileDialog::GetSelectedItemIndex(int nStart)
{
	int nIndex = -1;
	int nItemCount = ListView_GetItemCount(m_hwndLV);
	for (int i = nStart; i < nItemCount; i++)	{
		if (ListView_GetItemState(m_hwndLV, i, LVIS_SELECTED) == LVIS_SELECTED)
			return i;
	}
	
	return -1;
}

void CFileDialog::OnGetDispInfo(NMLVDISPINFO* pnmdi)
{
	LIST_ITEM_INFO* p = GetListItemInfo(pnmdi->item.iItem);
	if (pnmdi->item.mask & LVIF_IMAGE) {
		if (p->nIcon == -1)
			p->nIcon = m_ImageList.GetImageListIndex(p->pszName, m_szCurrent);
		pnmdi->item.iImage = p->nIcon;
	}
	if ((pnmdi->item.mask & LVIF_TEXT)) {
		if (p->type == ITEM_TYPE_DIR) {
			switch (pnmdi->item.iSubItem) {
			case 0: //Name
				pnmdi->item.pszText = p->pszName;
				break;
			case 2: //Type
				{
					if (!p->pszDispType) {
						TCHAR sz[MAX_PATH];
						SHFILEINFO shfi;
						p->pszDispType = new TCHAR[80]; // ???
						wsprintf(sz, _T("%s\\%s"), m_szCurrent, p->pszName);
						SHGetFileInfo(sz, NULL, &shfi, sizeof(shfi), SHGFI_TYPENAME);
						_tcscpy(p->pszDispType, shfi.szTypeName);
					}
					pnmdi->item.pszText = p->pszDispType;
					break;
				}
			default: break;
			}
		}
		else
		{
			switch (pnmdi->item.iSubItem) {
			case 0: //Name
				{
					if (!p->pszDispName) {
						if (m_fShowExt) {
							pnmdi->item.pszText = p->pszName;
							break;
						}
						else {
							p->pszDispName = new TCHAR[_tcslen(p->pszName) + 1];
							_tcscpy(p->pszDispName, p->pszName);
							LPTSTR psz = _tcsrchr(p->pszDispName, _T('.'));
							if (psz)
								*psz = NULL;
						}
					}
					pnmdi->item.pszText = p->pszDispName;
					break;
				}
			case 1: //Size
				{
					if (!p->pszDispSize) {
						p->pszDispSize = new TCHAR[64];
						SetFormatSize((DWORD)p->llSize, p->pszDispSize, _T("%s KB"), _T("%s MB"));
						// todo : 2^32 bytesˆÈã‚Ìƒtƒ@ƒCƒ‹‘Î‰ž
					}
					pnmdi->item.pszText = p->pszDispSize;
					break;
				}
			case 2: //Type
				{
					if (!p->pszDispType) {
						TCHAR sz[MAX_PATH];
						SHFILEINFO shfi;
						p->pszDispType = new TCHAR[80]; // ???
						wsprintf(sz, _T("%s\\%s"), m_szCurrent, p->pszName);
						SHGetFileInfo(sz, NULL, &shfi, sizeof(shfi), SHGFI_TYPENAME);
						_tcscpy(p->pszDispType, shfi.szTypeName);
					}
					pnmdi->item.pszText = p->pszDispType;
					break;
				}
			case 3: //Date
				{
					if (!p->pszDispTime)
					{
						p->pszDispTime = new TCHAR[32];
						SYSTEMTIME st;
						FileTimeToSystemTime(&p->ft, &st);
						SetFormatDateTime(&st, p->pszDispTime, 32);
					}
					pnmdi->item.pszText = p->pszDispTime;
					break;
				}
			default: break;
			}
		}
	}
}

void CFileDialog::CreateExtList()
{
	DeleteExtList();
	
	HWND hwndCombo = GetDlgItem(m_hwndDlg, IDC_COMBO_FILTER);
	int nIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
	if (nIndex == CB_ERR)
		return;
	
	TCHAR sz[MAX_LOADSTRING];
	LPTSTR p;
	LPTSTR pszSrc = (LPTSTR)SendMessage(hwndCombo, CB_GETITEMDATA, nIndex, 0);
	while (TRUE) {
		p = sz;
		while (*pszSrc != _T(';') && *pszSrc != _T(',') && *pszSrc != NULL) {
			*p++ = *pszSrc;
			pszSrc++;
		}
		*p = NULL;
		p = new TCHAR[_tcslen(sz) + 1];
		_tcscpy(p, sz);
		m_listExt.Add((DWORD)p);
		
		if (*pszSrc == NULL)
			break;
		pszSrc++;
	}
}

void CFileDialog::DeleteExtList()
{
	LPTSTR p;
	while (!m_listExt.IsEmpty()) {
		p = (LPTSTR)m_listExt.RemoveAt(0);
		delete [] p;
	}
}

void CFileDialog::OnCBSelChange()
{
	if (m_pszFilter) {
		delete [] m_pszFilter;
		m_pszFilter = NULL;
	}
	
	CreateExtList();
	LoadFolderItem(m_szCurrent, FALSE);
}

LPTSTR CFileDialog::GetDisplayName(LPTSTR pszPath)
{
	if (!_tcslen(pszPath) || _tcscmp(pszPath, bslash) == 0)
		return m_szRootName;
	else
		return pszPath;
}

void CFileDialog::ChangeListStyle(DWORD dwNewStyle)
{
	DWORD dwStyle = GetWindowLong(m_hwndLV, GWL_STYLE);
#if 0
	switch (dwStyle & LVS_TYPEMASK) {
	case LVS_ICON: dwStyle ^= LVS_ICON; break;
	case LVS_SMALLICON: dwStyle ^= LVS_SMALLICON; break;
	case LVS_LIST: dwStyle ^= LVS_LIST; break;
	case LVS_REPORT: dwStyle ^= LVS_REPORT; break;
	}
#else
	dwStyle &= ~DWORD(LVS_TYPEMASK);
#endif
	dwStyle |= dwNewStyle;
	SetWindowLong(m_hwndLV, GWL_STYLE, dwStyle);
	
	if (m_fDlgType == DLG_TYPE_PPC3 && dwNewStyle == LVS_REPORT) {
		ListView_SetExtendedListViewStyle(m_hwndLV, ListView_GetExtendedListViewStyle(m_hwndLV) | 
			0x00000040 /*LVS_EX_ONECLICKACTIVATE*/ | LVS_EX_FULLROWSELECT | LVS_EX_TRACKSELECT);
	}
	else
		ListView_SetExtendedListViewStyle(m_hwndLV, 0);
	
	SendMessage(m_hwndCB, TB_SETSTATE, ID_LIST_STYLE_LIST, TBSTATE_ENABLED|((dwNewStyle == LVS_LIST)?TBSTATE_CHECKED:0));
	SendMessage(m_hwndCB, TB_SETSTATE, ID_LIST_STYLE_REPORT, TBSTATE_ENABLED|((dwNewStyle == LVS_REPORT)?TBSTATE_CHECKED:0));
	
	TBBUTTON tbb;
	SendMessage(m_hwndCB, TB_GETBUTTON, 1, (LPARAM)&tbb);
	HMENU hMenu=HMENU(tbb.dwData);
	if(hMenu)
	{
		CheckMenuRadioItem(hMenu,ID_LIST_STYLE_LIST,ID_LIST_STYLE_REPORT,(dwNewStyle == LVS_LIST)?ID_LIST_STYLE_LIST:ID_LIST_STYLE_REPORT,MF_BYCOMMAND);
	}
}

void CFileDialog::OnUp()
{
	TCHAR szPath[MAX_PATH] = _T("");
	if (!_tcslen(m_szCurrent) || _tcscmp(m_szCurrent, bslash) == 0)
		return;
	
	_tcscpy(szPath, m_szCurrent);
	LPTSTR psz = _tcsrchr(szPath, _T('\\'));
	if (psz) *psz = NULL;
	LoadFolderItem(szPath);
}

void CFileDialog::OnListClick()
{
	if (m_fDlgType == DLG_TYPE_PPC3 &&
		!GetAsyncKeyState(VK_CONTROL) &&
		!GetAsyncKeyState(VK_SHIFT) &&
		ListView_GetSelectedCount(m_hwndLV) < 2)
		OnListDblClk();
}

void CFileDialog::OnListDblClk()
{
	int nIndex = GetSelectedItemIndex(0);
	if (nIndex != -1) {
		TCHAR szPath[MAX_PATH];
		LIST_ITEM_INFO* p = GetListItemInfo(nIndex);
		if (p->type == ITEM_TYPE_DIR) {
			wsprintf(szPath, _T("%s\\%s"), m_szCurrent, p->pszName);
			LoadFolderItem(szPath);
		}
		else if (IsFolderShortcut(m_szCurrent, p->pszName)) {
			TCHAR szTarget[MAX_PATH];
			wsprintf(szPath, _T("%s\\%s"), m_szCurrent, p->pszName);
			SHGetShortcutTarget(szPath, szTarget, MAX_PATH);
			LPTSTR p = _tcsrchr(szTarget, _T('\"'));
			if (p) *p = NULL;
			p = (szTarget[0] == _T('\"')) ? szTarget + 1 : szTarget;
			LoadFolderItem(p);
		}
		else if (p->type == ITEM_TYPE_FILE) {
			OnOK();
		}
	}
}

void CFileDialog::OnListKeyDown(NMLVKEYDOWN* pnmk)
{
	switch (pnmk->wVKey) {
	case VK_BACK:
		OnUp(); break;
	case VK_ESCAPE:
		EndDialog(IDCANCEL); break;
	case VK_TAB:
		SetFocus(GetDlgItem(m_hwndDlg, 
			(GetAsyncKeyState(VK_SHIFT)) ? IDC_COMBO_FILTER :IDC_EDIT_NAME)); break;
	case 'A':
		SelectAllItems();
		break;
	}
}

void CFileDialog::OnListItemChanged(NM_LISTVIEW* pnmlv)
{
	TCHAR sz[MAX_PATH + 3];
	LIST_ITEM_INFO* p;
	
	if ((pnmlv->uNewState | pnmlv->uOldState) == LVIS_FOCUSED)
		return;
	
	HWND hwndEdit = GetDlgItem(m_hwndDlg, IDC_EDIT_NAME);
	int n = ListView_GetSelectedCount(m_hwndLV);
	if (n == 0) {
		if (!(GetWindowLong(m_hwndLV, GWL_STYLE) & LVS_SINGLESEL))
			SetWindowText(hwndEdit, _T(""));
		return;
	}
	
	n = 0;
	int nIndex;
	nIndex = GetSelectedItemIndex(0);
	do {
		p = GetListItemInfo(nIndex);
		if (p->type == ITEM_TYPE_FILE && !IsFolderShortcut(m_szCurrent, p->pszName))
			n++;
	}
	while ((nIndex = GetSelectedItemIndex(nIndex + 1)) != -1);
	if (n == 0) {
		if (!(GetWindowLong(m_hwndLV, GWL_STYLE) & LVS_SINGLESEL))
			SetWindowText(hwndEdit, _T(""));
		return;
	}
	
	if (n > 1) {
		p = GetListItemInfo(pnmlv->iItem);
		if (p->type == ITEM_TYPE_FILE && !IsFolderShortcut(m_szCurrent, p->pszName)) {
			int nLen = GetWindowTextLength(hwndEdit);
			wsprintf(sz, _T("\"%s\" "), p->pszName);
			if (pnmlv->uNewState & LVIS_SELECTED) {
				// add
				if (n == 2) {
					TCHAR sz2[MAX_PATH + 3];
					nIndex = GetSelectedItemIndex(0);
					do {
						p = GetListItemInfo(nIndex);
						if (p->type == ITEM_TYPE_FILE && !IsFolderShortcut(m_szCurrent, p->pszName) &&
							nIndex != pnmlv->iItem)
							break;
					}
					while ((nIndex = GetSelectedItemIndex(nIndex + 1)) != -1);					
					wsprintf(sz2, _T("\"%s\" "), p->pszName);
					
					LPTSTR psz = new TCHAR[_tcslen(sz) + _tcslen(sz2) + 1];
					if (psz) {
						*psz = NULL;
						_tcscpy(psz, sz2);
						_tcscat(psz, sz);
						SetWindowText(hwndEdit, psz);
						SendMessage(hwndEdit, EM_SETSEL, -1, -1);
						delete [] psz;
					}
				}
				else {
					LPTSTR psz = new TCHAR[nLen + _tcslen(sz) + 1];
					if (psz) {
						GetWindowText(hwndEdit, psz, nLen + 1);
						_tcscat(psz, sz);
						SetWindowText(hwndEdit, psz);
						SendMessage(hwndEdit, EM_SETSEL, -1	, -1);
						delete [] psz;
					}
				}
			}
			else {
				// delete
				LPTSTR psz = new TCHAR[nLen + 1];
				if (psz) {
					GetWindowText(hwndEdit, psz, nLen + 1);
					LPTSTR p = _tcsstr(psz, sz);
					if (p) {
						memmove(p, p + _tcslen(sz), sizeof(TCHAR) * (_tcslen(p + _tcslen(sz)) + 1));
						SetWindowText(hwndEdit, psz);
						SendMessage(hwndEdit, EM_SETSEL, 0, 0);
					}
					delete [] psz;
				}
			}
		}
	}
	else if (n) {
		nIndex = GetSelectedItemIndex(0);
		do {
			p = GetListItemInfo(nIndex);
			if (p->type == ITEM_TYPE_FILE && !IsFolderShortcut(m_szCurrent, p->pszName))
				break;
		}
		while ((nIndex = GetSelectedItemIndex(nIndex + 1)) != -1);
		if (nIndex != -1) {
			p = GetListItemInfo(nIndex);
			if (p->type == ITEM_TYPE_FILE && !IsFolderShortcut(m_szCurrent, p->pszName)) {
				_tcscpy(sz, p->pszName);
				if (!m_fShowExt) {
					LPTSTR psz = _tcsrchr(sz, _T('.'));
					if (psz) {
						if (m_pszDefExt) {
							delete [] m_pszDefExt;
							m_pszDefExt = NULL;
						}
						m_pszDefExt = new TCHAR[_tcslen(psz) + 1];
						_tcscpy(m_pszDefExt, psz);
						*psz = NULL;
					}
				}
				SetWindowText(hwndEdit, sz);
			}
		}
	}
}

void CFileDialog::OnListColumnClick(NMLISTVIEW* pnmlv)
{
	LIST_SORT sort = LIST_SORT_NAME;
	switch (pnmlv->iSubItem) {
		//		case 0: sort = LIST_SORT_NAME; break;
	case 1: sort = LIST_SORT_SIZE; break;
	case 2: sort = LIST_SORT_EXT; break;
	case 3: sort = LIST_SORT_TIME; break;
	}
	
	if (sort == m_nListSort)
		m_fSortOrder = !m_fSortOrder;
	
	m_nListSort = sort;
	SortList();
}

void CFileDialog::SelectAllItems()
{
	if (!(GetWindowLong(m_hwndLV, GWL_STYLE) & LVS_SINGLESEL))
		ListView_SetItemState(m_hwndLV, -1, LVIS_SELECTED, LVIS_SELECTED);
}

BOOL CFileDialog::IsFolderShortcut(LPCTSTR pszPath, LPCTSTR pszName)
{
	TCHAR sz[MAX_PATH];
	wsprintf(sz, _T("%s\\%s"), pszPath, pszName);
	_tcsupr(sz);
	
	LPTSTR psz = _tcsrchr(sz, _T('.'));
	if (psz && _tcscmp(psz, _T(".LNK")) == 0) {
		TCHAR szTarget[MAX_PATH];
		wsprintf(sz, _T("%s\\%s"), pszPath, pszName);
		if (SHGetShortcutTarget(sz, szTarget, MAX_PATH)) {
			LPTSTR p = _tcsrchr(szTarget, _T('\"'));
			if (p) *p = NULL;
			p = (szTarget[0] == _T('\"')) ? szTarget + 1 : szTarget;
			DWORD dwAttr = GetFileAttributes(p);
			if (dwAttr != 0xFFFFFFFF && 
				(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
				return TRUE;
		}
	}
	return FALSE;
}


int CFileDialog::NewFolderDialog()
{
	if (m_fCtrl) {
		keybd_event(VK_CONTROL, 0x1e, KEYEVENTF_KEYUP, 1);
	}
	int rtn = DialogBoxParam(m_hInst, MAKEINTRESOURCE(IDD_NEWFOLDERDIALOG), m_hwndDlg, NewFolderDlgProc, (LPARAM)this);
	if (m_fCtrl) {
		keybd_event(VK_CONTROL, 0x1e, 0, 1);
	}
	return rtn;
}

BOOL CFileDialog::NewFolderDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CFileDialog* pDlg = (CFileDialog*)GetWindowLong(hwndDlg, DWL_USER);
	switch (uMsg) {
	case WM_INITDIALOG:
		pDlg = (CFileDialog*)lParam;
		SetWindowLong(hwndDlg, DWL_USER, (DWORD)lParam);
		if(pDlg->m_helper.IsPocketPC())
			CreateWindow(_T("SIPPREF"),_T(""),0,-1,-1,0,0,hwndDlg,(HMENU)-1,NULL,NULL);
		{
			WCHAR caption[32];
			pDlg->m_helper.LoadString(IDS_CAPTION_NEWFOLDER, caption, 32);
			SetWindowText(hwndDlg,caption);
			pDlg->m_helper.LoadString(IDS_CAPTION_CANCEL,caption,32);//Cancel
			SetDlgItemText(hwndDlg, IDCANCEL, caption);
		}
		SendDlgItemMessage(hwndDlg, IDC_EDIT_NAME,EM_LIMITTEXT,(WPARAM)(MAX_PATH-_tcslen(pDlg->m_szCurrent)-1),0);
		SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_NAME));
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			{
				TCHAR folder[MAX_PATH];
				DWORD err=0;
				GetDlgItemText(hwndDlg, IDC_EDIT_NAME, folder, MAX_PATH);
				if(!folder[0])
					::EndDialog(hwndDlg,IDCANCEL);
				else if(!pDlg->CreateFolder(hwndDlg,folder))
					::EndDialog(hwndDlg,IDOK);
			}
			return TRUE;
		case IDCANCEL:
			::EndDialog(hwndDlg,IDCANCEL);
			return TRUE;
		}
		break;
		return TRUE;
	}
	return FALSE;
}

int CFileDialog::CreateFolder(HWND hOwner, LPCTSTR folder)
{
	DWORD err=0,ids;
	TCHAR fullpath[MAX_PATH+1];
	if(_tcscspn(folder,_T("\\/:*\?\"<>|"))!=_tcslen(folder))
		err=ERROR_INVALID_NAME;
	else
	{
		_stprintf(fullpath,_T("%s\\%s"),m_szCurrent,folder);
		if(!CreateDirectory(fullpath,NULL))
			err=GetLastError();
	}
	
	if(err)
	{
		TCHAR caption[32];
		m_helper.LoadString(IDS_CAPTION_NEWFOLDER, caption, 32);
		
		TCHAR szMsg[MAX_LOADSTRING];
		switch (err) {
		case ERROR_INVALID_NAME:	
			ids = IDS_MSG_INVALIDNAME;
			break;
		case ERROR_ALREADY_EXISTS:	
			ids = IDS_MSG_ALREADYEXISTS;
			break;
		case ERROR_FILENAME_EXCED_RANGE:	
			ids = IDS_MSG_FOLDERTOOLONG;
			break;
		default:
			ids = IDS_MSG_NEWFOLDERERROR;
			break;
		}
		
		m_helper.LoadString(ids, szMsg, MAX_LOADSTRING);
		TCHAR *p;
		p= new TCHAR[_tcslen(folder) + _tcslen(szMsg) + 1];
		wsprintf(p, szMsg, folder);
		MessageBox(hOwner, p, caption, MB_ICONEXCLAMATION | MB_OK);
		delete [] p;
	}
	else
	{
		LVITEM li;
		LIST_ITEM_INFO *p;
		p = new LIST_ITEM_INFO;
		p->type = ITEM_TYPE_DIR;
		p->pszName = new TCHAR[_tcslen(folder) + 1];
		memset(p->pszName, 0, sizeof(TCHAR) * (_tcslen(folder) + 1));
		_tcscpy(p->pszName, folder);
		
		li.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM |LVIF_STATE ;
		li.iItem = 0;
		li.iSubItem = 0;
		li.state = li.stateMask  = LVIS_FOCUSED ;
		li.iImage = I_IMAGECALLBACK;
		li.lParam = (DWORD)p;
		li.pszText = LPSTR_TEXTCALLBACK;
		
		SendMessage(m_hwndLV, WM_SETREDRAW, (WPARAM)FALSE, 0);
		ListView_InsertItem(m_hwndLV, &li);
		ListView_SetItemText(m_hwndLV, 0, 2, LPSTR_TEXTCALLBACK);
		SortList();
		LVFINDINFO lfi;
		lfi.flags = LVFI_STRING ;
		lfi.psz = folder;
		int iSel = ListView_FindItem(m_hwndLV, -1, &lfi);
		ListView_EnsureVisible(m_hwndLV, iSel, FALSE);
		SendMessage(m_hwndLV, WM_SETREDRAW, (WPARAM)TRUE, 0);
	}
	return err;
}

BOOL IsIncludedFolder(LPCWSTR file,LPCWSTR folder)
{
	LPCWSTR f=wcsrchr(file,L'\\');
	int n=f-file;
	if(!n)
	{
		if(!folder[0])
			return TRUE;
	}
	else if(!wcsncmp(file,folder,n))
		return TRUE;
	return FALSE;
}

void CFileDialog::OnFileChangeInfo(FILECHANGEINFO *lpfci)
{
	if(HIWORD(lpfci))
	{
		if(lpfci->wEventId==SHCNE_RENAMEITEM||lpfci->wEventId==SHCNE_RENAMEFOLDER)
		{
			FILECHANGEINFO fci={sizeof(FILECHANGEINFO),0,lpfci->uFlags};
			LPCWSTR file;
			file=LPCWSTR(lpfci->dwItem1);
			if(IsIncludedFolder(file,m_szCurrent))
			{
				fci.wEventId=(lpfci->wEventId==SHCNE_RENAMEITEM)?SHCNE_DELETE:SHCNE_RMDIR;
				fci.dwItem1=(DWORD)file;
				OnFileChangeInfo(&fci);
			}

			file=LPCWSTR(lpfci->dwItem2);
			if(IsIncludedFolder(file,m_szCurrent))
			{
				fci.wEventId=(lpfci->wEventId==SHCNE_RENAMEITEM)?SHCNE_CREATE:SHCNE_MKDIR;
				fci.dwItem1=(DWORD)file;
				OnFileChangeInfo(&fci);
			}
			return;
		}
		
		WIN32_FIND_DATA fd;
		LPCWSTR pszFile=wcsrchr(LPCWSTR(lpfci->dwItem1),L'\\');
		if(m_szCurrent[0]||(!m_szCurrent[0]&&pszFile==LPCWSTR(lpfci->dwItem1)))
		switch(lpfci->wEventId)
		{
		case SHCNE_CREATE:
			if(!m_listExt.IsEmpty()&&!IsFolderShortcut(m_szCurrent,pszFile+1))
			{
				BOOL b=TRUE;
				int n = m_listExt.GetCount();
				for (int i = 0; i < n; i++) {
					LPTSTR p = (LPTSTR)m_listExt.GetAt(i);
					WCHAR fmt[MAX_PATH];
					swprintf(fmt,L"%s\\%s",m_szCurrent,p);
					HANDLE hFind;
					hFind=FindFirstFile(fmt,&fd);
					if(hFind!=INVALID_HANDLE_VALUE)
					{
						do{
							if(!wcscmp(fd.cFileName,pszFile+1) )
							{
								b=FALSE;
								break;
							}
						}while(FindNextFile(hFind,&fd));
						FindClose(hFind);
					}
				}
				if(b)
					break;
			}
		case SHCNE_MKDIR:
		case SHCNE_DRIVEADD:
			FindClose(FindFirstFile(LPCWSTR(lpfci->dwItem1),&fd));
			AddListItem(m_szCurrent,&fd,ListView_GetItemCount(m_hwndLV));
			break;
		case SHCNE_DELETE:
		case SHCNE_RMDIR:
		case SHCNE_DRIVEREMOVED:
			{
				int i;
				for (i = 0; i < ListView_GetItemCount(m_hwndLV); i++) {
					LIST_ITEM_INFO* pDel = GetListItemInfo(i);
					if(!wcscmp(pszFile+1,pDel->pszName))
					{
						if (pDel->pszDispName) delete[] pDel->pszDispName;
						if (pDel->pszDispSize) delete[] pDel->pszDispSize;
						if (pDel->pszDispType) delete[] pDel->pszDispType;
						if (pDel->pszDispTime) delete[] pDel->pszDispTime;
						delete[] pDel->pszName; 
						delete pDel;
						ListView_DeleteItem(m_hwndLV,i);
						break;
					}
				}
			}
		}
	}
}

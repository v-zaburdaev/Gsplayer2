#ifndef __DEFSTRING_H__
#define __DEFSTRING_H__

// add by Y.N

#define DEF_STR_UP_JPN		_T("上へ")
#define DEF_STR_UP_ENG		_T("Up")
#define DEF_STR_MENU_JPN	_T("メニュー")
#define DEF_STR_MENU_ENG	_T("Menu")
#define DEF_STR_SELECTALL_JPN	_T("すべて選択")
#define DEF_STR_SELECTALL_ENG	_T("Select All")
#define DEF_STR_KEYCTRL_JPN		_T("タップで選択")
#define DEF_STR_KEYCTRL_ENG		_T("Select on Tap")

__inline BOOL IsJapanese()
{
	return PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_JAPANESE ? TRUE : FALSE;
}

__inline int CopyDefString(LPTSTR pszSrc, LPTSTR pszDst, int nDst)
{
	if (_tcslen(pszSrc) + 1 > nDst) {
		_tcsncpy(pszDst, pszSrc, nDst - 1);
		pszDst[nDst - 1] = _T('\0');
		return nDst - 1;
	}
	else {
		_tcscpy(pszDst, pszSrc);
		return _tcslen(pszSrc);
	}
}

__inline int GetDefString(UINT uID, LPTSTR lpBuffer, int cchBufferMax)
{
	int nRet = 0;
	
	switch(uID) {
	case IDS_MENU_UP:
		nRet = CopyDefString(IsJapanese() ? DEF_STR_UP_JPN : DEF_STR_UP_ENG, lpBuffer, cchBufferMax);
		break;
	case IDS_MENU_MENU:
		nRet = CopyDefString(IsJapanese() ? DEF_STR_MENU_JPN : DEF_STR_MENU_ENG, lpBuffer, cchBufferMax);
		break;
	case IDS_MENU_SELECTALL:
		nRet = CopyDefString(IsJapanese() ? DEF_STR_SELECTALL_JPN : DEF_STR_SELECTALL_ENG, lpBuffer, cchBufferMax);
		break;
	case IDS_MENU_KEYCTRL:
		nRet = CopyDefString(IsJapanese() ? DEF_STR_KEYCTRL_JPN : DEF_STR_KEYCTRL_ENG, lpBuffer, cchBufferMax);
		break;
	default:
		nRet = 0;
	}
	
	return nRet;
}

#endif // __DEFSTRING_H__

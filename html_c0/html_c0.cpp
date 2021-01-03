
/*
Copyright (c) 2010 Igor Yudincev
Copyright (c) 2010 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <shlwapi.h>
#include "Plugin.hpp"
#include "CRT/crt.hpp"

#define BUF_SIZE 4096

typedef struct tagExtension {
    int length;
	const wchar_t *text;
} Extension;

const Extension extensions[] = {
	{4, L".htm"},
	{5, L".html"},
	{6, L".phtml"},
	{6, L".shtml"},
	{6, L".xhtml"}
};

typedef struct tagCharset {
    UINT code;
	const char *name;
} Charset;

const Charset charsets[] = {
	{1251, "windows-1251"},
	{20866, "koi8-r"},
	{21866, "koi8-u"},
	{28595, "iso-8859-5"},
	{65001, "utf-8"}
};

/* FAR */
PluginStartupInfo psi;
FarStandardFunctions fsf;
char buf[BUF_SIZE];

BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    return TRUE;
}


#if defined(__GNUC__)

extern
BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     );

#ifdef __cplusplus
extern "C"{
#endif
  BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved);
  int WINAPI GetMinFarVersionW(void);
  void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info);
  void WINAPI GetPluginInfoW(struct PluginInfo *Info);
  int WINAPI GetCustomDataW(const wchar_t *FilePath, wchar_t **CustomData);
  void WINAPI FreeCustomDataW(wchar_t *CustomData);
#ifdef __cplusplus
};
#endif

BOOL WINAPI DllMainCRTStartup(HANDLE hDll,DWORD dwReason,LPVOID lpReserved)
{
  DllMain(hDll, dwReason, lpReserved);
  return TRUE;
}
#endif


int WINAPI GetMinFarVersionW(void)
{
    return MAKEFARVERSION(2,0,1588);
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info)
{
    ::psi = *Info;
    ::fsf = *(Info->FSF);
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info)
{
	Info->Flags = PF_PRELOAD;
}

static inline char tolower(char c)
{
	if (c>='A' && c<='Z') return c+32;
	return c;
}

static bool HasAllowedExtension(const wchar_t *filename)
{
	int sz = wcslen(filename);
	for (size_t i=0; i<ArraySize(extensions); i++) {
		int szExt = extensions[i].length;
		if (sz>szExt &&
			StrCmpIW(&filename[sz-szExt], extensions[i].text)==0)
			return true;
	}
	return false;
}

static UINT DecodeCharset(const char *charset)
{
	for (size_t i=0; i<ArraySize(charsets); i++) {
		if (StrCmpNIA(charset, charsets[i].name, strlen(charsets[i].name))==0)
			return charsets[i].code;
	}
	return CP_ACP;
}

static int ExtractTitle(wchar_t **CustomData)
{
	char *p, *pTitle, *pTitleEnd, *pCharset, *pFirstChar, *pLastChar;
	bool bCharFound;
	int nTitle;
	int charset;

	p = StrStrIA(buf,"<html");
	if (p==NULL) return FALSE;

	pTitle = StrStrIA(p,"<title>");
	if (pTitle==NULL) return FALSE;
	pTitle += 7;

	pTitleEnd = StrStrIA(pTitle,"</title>");
	if (pTitleEnd==NULL) return FALSE;

	pCharset = strstr(p,"charset=");
	if (pCharset!=NULL) {
		pCharset += 8;
		charset = DecodeCharset(pCharset);
	}
	else {
		charset=CP_ACP;
	}

	nTitle = pTitleEnd-pTitle;
	pFirstChar = pLastChar = pTitle;
	bCharFound = false;
	for (int i=0; i<nTitle; i++) {
		if (pTitle[i]>' ') {
			pLastChar = &pTitle[i];
			if (!bCharFound) pFirstChar = &pTitle[i];
			bCharFound = true;
		}
		else {
			pTitle[i] = ' ';
		}
	}
	if (!bCharFound) return FALSE;

	nTitle = pLastChar-pFirstChar+1;
	*CustomData = (wchar_t*)malloc((nTitle+1)*sizeof(wchar_t));
	if (MultiByteToWideChar(charset,0,pFirstChar,nTitle,*CustomData,nTitle+1)==0) {
		**CustomData = 0;
	}
	return TRUE;
}

int WINAPI GetCustomDataW(const wchar_t *FilePath, wchar_t **CustomData)
{
	*CustomData = NULL;

	wchar_t *pSlash = wcsrchr(FilePath,'\\');
	if (pSlash==NULL) return FALSE;
	pSlash++;
	if (!HasAllowedExtension(pSlash)) return FALSE;

	int result = FALSE;
	HANDLE hFile = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0,0);
	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD nRead;
		if (ReadFile(hFile, buf, BUF_SIZE, &nRead, NULL)) {
			buf[nRead] = 0;
			result = ExtractTitle(CustomData);
		}
		CloseHandle(hFile);
	}

	return result;
}

void WINAPI FreeCustomDataW(wchar_t *CustomData)
{
	if (CustomData)
		free(CustomData);
}

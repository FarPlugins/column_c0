
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


#include <windows.h>
#include <shlwapi.h>
#include <TCHAR.H>
#include "Plugin.hpp"

#define BUF_SIZE 4096
#define CDIR_SIG 0x06054b50

#pragma pack(push,1)
typedef struct {
	DWORD signature;	//End of central directory signature = 0x06054b50
	WORD disk;			//Number of this disk
	WORD cdir_disk;		//Disk where central directory starts
	WORD cdir_num_on_disk;	//Number of central directory records on this disk
	WORD cdir_num;		//Total number of central directory records
	DWORD cdir_size;	//Size of central directory (bytes)
	DWORD cdir_ofs;		//Offset of start of central directory, relative to start of archive
	WORD comment_len;	//ZIP file comment length (n)
	char comment[];		//ZIP file comment
} TZipCentralDirectory;
#pragma pack(pop)

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

char tolower(char c) {
	if (c>='A' && c<='Z') return c+32;
	return c;
}

int ExtractComment(const char *pBuf, size_t nBuf, wchar_t **CustomData) {
	for (const char *p=pBuf+nBuf-sizeof(TZipCentralDirectory); p>=pBuf; p--) {
		TZipCentralDirectory *pSig = (TZipCentralDirectory *)p;
		if (pSig->signature==CDIR_SIG &&
			pSig->comment+pSig->comment_len==pBuf+nBuf)
		{
			*CustomData = (wchar_t*)malloc((pSig->comment_len+1)*sizeof(wchar_t));
			if (MultiByteToWideChar(CP_ACP, 0, pSig->comment, pSig->comment_len, *CustomData, pSig->comment_len+1)==0)
				**CustomData = 0;
			return TRUE;
		}
	}
	return FALSE;
}

int WINAPI GetCustomDataW(const wchar_t *FilePath, wchar_t **CustomData)
{
	*CustomData = NULL;

	wchar_t *pSlash = wcsrchr(FilePath,'\\');
	if (pSlash==NULL) return FALSE;
	pSlash++;
	int nFileName = wcslen(pSlash);
	if (nFileName<4 || StrCmpIW(pSlash+nFileName-4, L".zip")!=0) return FALSE;

	int result = FALSE;
	HANDLE hFile = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0,0);
	if (hFile != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER nSize = {{0}};
		if (GetFileSizeEx(hFile, &nSize) &&
			nSize.QuadPart > sizeof(TZipCentralDirectory))
		{
			int sz = (nSize.QuadPart<BUF_SIZE) ? (int)nSize.QuadPart : BUF_SIZE;
			DWORD nRead;
			if (SetFilePointer(hFile, -sz, NULL, FILE_END)!=0xFFFFFFFF &&
				ReadFile(hFile, buf, sz, &nRead, NULL))
			{
				result = ExtractComment(buf, nRead, CustomData);
			}
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

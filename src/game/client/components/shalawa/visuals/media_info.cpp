#include "media_info.h"
#include <cstring>

#ifdef _WIN32

#include <windows.h>

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    SMediaInfo *info = (SMediaInfo*)lParam;
    if(info->valid) return FALSE;
    if(!IsWindowVisible(hwnd)) return TRUE;

    char title[512] = {0};
    GetWindowTextA(hwnd, title, sizeof(title));
    if(title[0] == '\0') return TRUE;

    const char *sep = strstr(title, " - ");
    if(!sep || strlen(title) <= 5) return TRUE;

    char className[256] = {0};
    GetClassNameA(hwnd, className, sizeof(className));

    if(strcmp(className, "Chrome_WidgetWin_1") != 0 &&
       strcmp(className, "MozillaWindowClass") != 0)
        return TRUE;

    int artistLen = (int)(sep - title);
    if(artistLen > 0 && artistLen < 255)
    {
        strncpy(info->artist, title, artistLen);
        info->artist[artistLen] = '\0';
    }
    strncpy(info->title, sep + 3, sizeof(info->title) - 1);
    info->playing = true;
    info->valid = true;
    return FALSE;
}

bool MediaInfo_Query(SMediaInfo *info)
{
    memset(info, 0, sizeof(*info));
    EnumWindows(EnumWindowsProc, (LPARAM)info);
    if(!info->valid)
        strncpy(info->title, "No Media", sizeof(info->title) - 1);
    return info->valid;
}

#else

bool MediaInfo_Query(SMediaInfo *info)
{
    memset(info, 0, sizeof(*info));
    return false;
}

#endif
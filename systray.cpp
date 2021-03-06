﻿#include "stdafx.h"

#include "systray.h"

/* Flags to pass when there aren’t any extra flags to pass. */
#define NO_ADDITIONAL_FLAGS 0

/* Notifies our systray entry about some change. */
static BOOL
SystrayNotify(DWORD message, HWND window, UINT id, HICON icon, LPCTSTR tip, UINT flags)
{
        NOTIFYICONDATA nid;

        INITSTRUCT(nid, TRUE);
        nid.hWnd = window;
        nid.uID = id;
        nid.hIcon = icon;
        nid.uFlags = flags | ((icon == NULL) ? 0 : NIF_ICON) | ((tip == NULL) ? 0 : NIF_TIP);
        if (nid.uFlags & NIF_MESSAGE)
                nid.uCallbackMessage = WM_TRAYNOTIFY;
        if (tip != NULL)
                if (FAILED(StringCchCopy(nid.szTip, _countof(nid.szTip), tip)))
                        nid.szTip[0] = L'\0';

        return Shell_NotifyIcon(message, &nid);
}

/* Adds an ICON to the systray for WINDOW with ID and TIP. */
BOOL 
SystrayAddIcon(HWND window, UINT id, HICON icon, LPCTSTR tip)
{
        return SystrayNotify(NIM_ADD, window, id, icon, tip, NIF_MESSAGE);
}

/* Modifies WINDOW’s systray icon ID with ICON and/or TIP. */
BOOL 
SystrayModifyIcon(HWND window, UINT id, HICON icon, LPCTSTR tip)
{
        return SystrayNotify(NIM_MODIFY, window, id, icon, tip, NO_ADDITIONAL_FLAGS);
}

/* Removes WINDOW’s icon ID from the systray. */
BOOL 
SystrayRemoveIcon(HWND window, UINT id)
{
        return SystrayNotify(NIM_DELETE, window, id, NULL, NULL, NO_ADDITIONAL_FLAGS);
}

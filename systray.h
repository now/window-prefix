﻿BOOL SystrayAddIcon(HWND window, UINT id, HICON icon, LPCTSTR tip);
BOOL SystrayModifyIcon(HWND window, UINT id, HICON icon, LPCTSTR tip);
BOOL SystrayRemoveIcon(HWND window, UINT id);

/* Message sent whenever the system-tray icon triggers some event. */
#define WM_TRAYNOTIFY       (WM_USER + 345)

/* void Cls_OnTrayNotify(HWND hwnd, UINT idCtl, UINT codeNotify) */
#define HANDLE_WM_TRAYNOTIFY(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (UINT)(wParam), (UINT)(lParam)), 0L)
#define FORWARD_WM_TRAYNOTIFY(hwnd, idCtl, codeNotify, fn) \
        (void)(fn)((hwnd), WM_TRAYNOTIFY, (WPARAM)(idCtl), (LPARAM)(codeNotify))

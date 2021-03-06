﻿#include "stdafx.h"
#define LIBRARY_BUILD
#include "hook.h"

#pragma data_seg(".HOOKDATA")
#if 0
HHOOK s_shell_hook = NULL;
#endif
HHOOK s_window_proc_hook = NULL;
HWND s_window = NULL;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.HOOKDATA,RWS")

HINSTANCE s_module;

BOOL APIENTRY DllMain(HINSTANCE module, DWORD reason_for_call, LPVOID reserved)
{
        if (reason_for_call == DLL_PROCESS_ATTACH)
                s_module = module;
        return TRUE;
} 

#if 0
static BOOL PostMessageIfInteresting(int code, WPARAM wParam, LPARAM lParam)
{
        static struct {
                int code;
                UINT message;
        } codes[] = {
                { HSHELL_WINDOWACTIVATED, WM_WPHOOK_WINDOW_ACTIVATED },
                { HSHELL_WINDOWCREATED, WM_WPHOOK_WINDOW_CREATED },
                { HSHELL_WINDOWDESTROYED, WM_WPHOOK_WINDOW_DESTROYED },
                { HSHELL_WINDOWREPLACED, WM_WPHOOK_WINDOW_REPLACED },
        };

        for (int i = 0; i < _countof(codes); i++)
                if (codes[i].code == code)
                        return PostMessage(s_window, codes[i].message, wParam, lParam);

        return FALSE;
}

static LRESULT CALLBACK ShellProc(int code, WPARAM wParam, LPARAM lParam)
{
        if (code >= 0)
                PostMessageIfInteresting(code, wParam, lParam);

        return CallNextHookEx(s_shell_hook, code, wParam, lParam);
}
#endif

static BOOL PostMessageIfSetIcon(PCWPRETSTRUCT message)
{
        if (message->message != WM_SETICON ||
            !IsWindowVisible(message->hwnd) ||
            (GetParent(message->hwnd) != NULL &&
             !(GetWindowLongPtr(message->hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)))
                return FALSE;

        return PostMessage(s_window, WM_WPHOOK_WINDOW_ICON_CHANGED, (WPARAM)message->hwnd, 0L);
}

static LRESULT CALLBACK CallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
{
        if (code == HC_ACTION)
                PostMessageIfSetIcon((PCWPRETSTRUCT)lParam);

        return CallNextHookEx(s_window_proc_hook, code, wParam, lParam);
}


BOOL WPHookRegister(HWND window)
{
        s_window = window;
#if 0
        s_shell_hook = SetWindowsHookEx(WH_SHELL, ShellProc, s_module, 0);
        if (s_shell_hook == NULL)
                return FALSE;
#endif
        s_window_proc_hook = SetWindowsHookEx(WH_CALLWNDPROCRET, CallWndRetProc, s_module, 0);
        return s_window_proc_hook != NULL;
}

BOOL WPHookUnregister()
{
        BOOL unhooked_all = TRUE;

        if (!UnhookWindowsHookEx(s_window_proc_hook))
                unhooked_all = FALSE;

#if 0
        if (!UnhookWindowsHookEx(s_shell_hook))
                unhooked_all = FALSE;
#endif

        return unhooked_all;
}

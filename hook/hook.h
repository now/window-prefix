#ifdef LIBRARY_BUILD
#  define DLL_API __declspec (dllexport) 
#else
#  define DLL_API __declspec (dllimport) 
#endif

#define WM_WPHOOK_BASE					(WM_USER + 600)
#define WM_WPHOOK_WINDOW_ACTIVATED		(WM_WPHOOK_BASE + 1)
#define WM_WPHOOK_WINDOW_CREATED		(WM_WPHOOK_BASE + 2)
#define WM_WPHOOK_WINDOW_DESTROYED		(WM_WPHOOK_BASE + 3)
#define WM_WPHOOK_WINDOW_REPLACED		(WM_WPHOOK_BASE + 4)
#define WM_WPHOOK_WINDOW_ICON_CHANGED	(WM_WPHOOK_BASE + 5)
#define WM_WPHOOK_WINDOW_TITLE_CHANGED	(WM_WPHOOK_BASE + 6)

/* void Cls_OnWPHookWindowActivated(HWND window, HWND activated_window, BOOL fullscreen) */
#define HANDLE_WM_WPHOOK_WINDOW_ACTIVATED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam), (BOOL)(lParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_ACTIVATED(window, activated_window, fullscreen, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_ACTIVATED, (WPARAM)(activated_window), (LPARAM)(fullscreen))

/* void Cls_OnWPHookWindowCreated(HWND window, HWND created_window) */
#define HANDLE_WM_WPHOOK_WINDOW_CREATED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_CREATED(window, created_window, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_CREATED, (WPARAM)(Created_window), (LPARAM)0L)

/* void Cls_OnWPHookWindowDestroyed(HWND window, HWND destroyed_window) */
#define HANDLE_WM_WPHOOK_WINDOW_DESTROYED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_DESTROYED(window, destroyed_window, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_DESTROYED, (WPARAM)(destroyed_window), (LPARAM)0L)

/* void Cls_OnWPHookWindowReplaced(HWND window, HWND replaced_window, HWND new_window) */
#define HANDLE_WM_WPHOOK_WINDOW_REPLACED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam), (HWND)(lParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_REPLACED(window, replaced_window, new_window, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_DESTROYED, (WPARAM)(replaced_window), (LPARAM)(new_window))

/* void Cls_OnWPHookWindowIconChanged(HWND window, HWND changed_window) */
#define HANDLE_WM_WPHOOK_WINDOW_ICON_CHANGED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_ICON_CHANGED(window, changed_window, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_ICON_CHANGED, (WPARAM)(changed_window), (LPARAM)0L)

/* void Cls_OnWPHookWindowTitleChanged(HWND window, HWND changed_window) */
#define HANDLE_WM_WPHOOK_WINDOW_TITLE_CHANGED(window, wParam, lParam, fn) \
    ((fn)((window), (HWND)(wParam)), 0L)
#define FORWARD_WM_WPHOOK_WINDOW_TITLE_CHANGED(window, changed_window, fn) \
    (void)(fn)((window), WM_WPHOOK_WINDOW_TITLE_CHANGED, (WPARAM)(changed_window), (LPARAM)0L)

DLL_API BOOL WPHookRegister(HWND window);
DLL_API BOOL WPHookUnregister(void);

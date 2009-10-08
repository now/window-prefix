#include "stdafx.h"

/* The class of the system tray window. */
#define SYSTEM_TRAY_WINDOW_CLASS    L"Shell_TrayWnd"

/* Number of points in an inch. */
#define N_POINTS_IN_AN_INCH         72

typedef BOOL (WINAPI *IsHungAppWindowFunc)(HWND);

/* A FreeFunc that does nothing. */
void
NullFreeFunc(void *data)
{
        UNREFERENCED_PARAMETER(data);
}

/* A simple EqualityFunc that compares the pointers passed to it. */
BOOL
PointerEqualityFunc(void *p1, void *p2)
{
        return p1 == p2;
}

/* Determines if the last Windows error was due to a timeout. */
BOOL 
LastErrorWasTimeout(VOID)
{
        DWORD error = GetLastError();
        return (error == ERROR_SUCCESS || error == ERROR_TIMEOUT);
}

/* Checks if WINDOW is hung by trying to send it a timed-out message. */
BOOL 
MyIsHungAppWindow(HWND window)
{
        static IsHungAppWindowFunc is_hung_app_window;
        static BOOL tried_loading_is_hung_app_window;

        if (!tried_loading_is_hung_app_window) {
                HINSTANCE user32dll = LoadLibrary(L"user32.dll");
                is_hung_app_window = (IsHungAppWindowFunc)GetProcAddress(user32dll, "IsHungAppWindow");
                tried_loading_is_hung_app_window = TRUE;
        }

        if (is_hung_app_window != NULL && is_hung_app_window(window))
                return TRUE;

        DWORD dw;
        LRESULT result = SendMessageTimeout(window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, HUNG_TIMEOUT, &dw);
        if (result)
                return FALSE;

        return LastErrorWasTimeout();
}

/* Enqueues two <Alt>-key presses in the keyboard input buffer and then try
 * calling SetForegroundWindow(). */
static BOOL 
SendAltKeyAndThenSetForegroundWindow(HWND window)
{
        INPUT keys[4];
        INITSTRUCT(keys, FALSE);
        keys[0].type = INPUT_KEYBOARD;
        keys[0].ki.wVk = VK_MENU;
        keys[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
        keys[1] = keys[2] = keys[0];
        keys[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        keys[3] = keys[1];
        SendInput(4, keys, sizeof(*keys));

        return SetForegroundWindow(window);
}

/* Checks if WINDOW can be used for attaching input or try using the
 * system-tray window if not. */
static BOOL 
CanUseWindowOrSystemTray(HWND *window)
{
        if (*window != NULL)
                return TRUE;

        HWND system_tray = FindWindow(SYSTEM_TRAY_WINDOW_CLASS, NULL);
        if (system_tray == NULL)
                return FALSE;

        *window = system_tray;
        return TRUE;
}

/* Attaches input between FOREGROUND_WINDOW and WINDOW and then try
 * SetForegroundWindow() and finally SendAltKeyAndThenSetForegroundWindow(). */
static BOOL 
AttachAndSetForegroundWindow(HWND foreground_window, HWND window)
{
        if (!CanUseWindowOrSystemTray(&foreground_window))
                return FALSE;

        DWORD id_foreground = GetWindowThreadProcessId(foreground_window, NULL);
        DWORD id_new = GetWindowThreadProcessId(window, NULL);

        AttachThreadInput(id_foreground, id_new, TRUE);

        BOOL success = TRUE;
        if (!SetForegroundWindow(window))
                success = SendAltKeyAndThenSetForegroundWindow(window);

        AttachThreadInput(id_foreground, id_new, FALSE);

        return success;
}

/* Tries to make WINDOW the topmost foreground window. */
static BOOL 
SetTopForegroundWindow(HWND window)
{
        HWND foreground_window = GetForegroundWindow();
        if (window == foreground_window)
                return TRUE;

        BringWindowToTop(window);
        SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        if (SetForegroundWindow(window))
                return TRUE;

        return AttachAndSetForegroundWindow(foreground_window, window);
}


BOOL 
IsToolWindow(HWND window)
{
        return GetWindowLongPtr(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW;
}

/* Tries every method of making WINDOW (or its last-active popup)
 * the foreground window. */
BOOL 
MySwitchToThisWindow(HWND window)
{
        HWND popup = GetLastActivePopup(window);
        if (popup != NULL && window != popup && !IsToolWindow(popup))
                return MySwitchToThisWindow(popup);

        if (MyIsHungAppWindow(window))
                return FALSE;

        BOOL success = SetTopForegroundWindow(window);

        if (IsIconic(window))
                PostMessage(window, WM_SYSCOMMAND, SC_RESTORE, 0);

        return success;
}

/* Gets WINDOW’s title-string and stores it in TITLE, allocating fresh memory
 * for it.
 *
 * Returns FALSE if the title couldn’t be retrieved or memory couldn’t be
 * allocated for it. */
BOOL 
GetWindowTitle(HWND window, LPTSTR *title)
{
        int size = ZERO_TERMINATE(GetWindowTextLength(window));
        *title = ALLOC_N(TCHAR, size);
        if (*title == NULL)
                return FALSE;

        int length = GetWindowText(window, *title, size);
        if (length > 0)
                return TRUE;

        FREE(*title);
        *title = NULL;

        return FALSE;
}

/* Unloads font if it isn’t usable. Checks for NULL. */
static BOOL 
UnloadUnusableFont(Font *font)
{
        if (font == NULL)
                return TRUE;

        if (font->IsAvailable())
                return FALSE;

        delete font;

        return TRUE;
}

/* Tries to load a single generic Font. */
static BOOL 
LoadSpecificGenericFont(WCHAR const *family_name, REAL em_size, Font **font)
{
        *font = new Font(family_name, em_size, FontStyleBold);

        return !UnloadUnusableFont(*font);
}

/* Tries its hardest to create a Font by going through a list of standard
 * fonts that should be available on any Windows system. */
static Status 
LoadGenericFont(Font **font)
{
        static struct {
                WCHAR const *family_name;
                REAL em_size;
        } const fonts[] = {
                { L"Segoe UI", 9 },
                { L"Tahoma", 8 },
                { L"Verdana", 8 },
                { L"Arial", 8 },
                { L"Microsoft Sans Serif", 8 },
                { L"Courier New", 8 },
                { L"Times New Roman", 9 },
        };

        for (size_t i = 0; i < _countof(fonts); i++)
                if (LoadSpecificGenericFont(fonts[i].family_name, fonts[i].em_size, font))
                        return Ok;

        return FontFamilyNotFound;
}

/* Loads the Font used for window captions, falling back on a generic font
 * if this fails. */
Status 
LoadWindowCaptionFont(Font **font)
{
        NONCLIENTMETRICS metrics;
        metrics.cbSize = sizeof(metrics);
        if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, FALSE))
                return LoadGenericFont(font);

        HDC screen_dc = GetDC(NULL);
        if (screen_dc == NULL)
                return LoadGenericFont(font);

        *font = new Font(screen_dc, &metrics.lfCaptionFont);

        ReleaseDC(NULL, screen_dc);

        if (UnloadUnusableFont(*font))
                return LoadGenericFont(font);

        return Ok;
}

/* Gets a system metric INDEX, returning DEFAULT_DIMENSION if it can’t be
 * retrieved. */
int 
GetSystemMetricsDefault(int index, int default_dimension)
{
        int dimension = GetSystemMetrics(index);
        if (dimension != 0)
                return dimension;

        return default_dimension;
}

/* Turns a GDI+ Status into an error message. */
BOOL 
StatusToString(Status status, LPTSTR buffer, size_t size)
{
#define CASE_STATUS(status, message)    \
        case status:                    \
        return SUCCEEDED(StringCchCopy(buffer, size, message));

        switch (status) {
                CASE_STATUS(Ok, L"No error occurred");
                CASE_STATUS(GenericError, L"A generic error occurred");
                CASE_STATUS(InvalidParameter, L"An invalid parameter was used");
                CASE_STATUS(OutOfMemory, L"Out of memory");
                CASE_STATUS(ObjectBusy, L"Object was currently busy");
                CASE_STATUS(InsufficientBuffer, L"A buffer was too small");
                CASE_STATUS(NotImplemented, L"A method has not been implemented");
                CASE_STATUS(Win32Error, L"A Microsoft Win32 error occurred");
                CASE_STATUS(WrongState, L"An object was in an invalid state");
                CASE_STATUS(Aborted, L"A method was prematurely terminated");
                CASE_STATUS(FileNotFound, L"A specified image file or metafile could not be found");
                CASE_STATUS(ValueOverflow, L"An arithmetic operation produced a numeric overflow");
                CASE_STATUS(AccessDenied, L"Writing to a specified file was denied");
                CASE_STATUS(UnknownImageFormat, L"An image file was in an unknown format");
                CASE_STATUS(FontFamilyNotFound, L"A specified font family could not be found");
                CASE_STATUS(FontStyleNotFound, L"A specified font was not available in a specified font style");
                CASE_STATUS(NotTrueTypeFont, L"A font coming from a HDC or LOGFONT was not a TrueType font");
                CASE_STATUS(UnsupportedGdiplusVersion, L"An unsupported GDI+ version is installed on this system");
                CASE_STATUS(GdiplusNotInitialized, L"The GDI+ API was not initialized properly before use");
                CASE_STATUS(PropertyNotFound, L"A specified property was not found in a specified image");
                CASE_STATUS(PropertyNotSupported, L"A specified property was not supported by the format a specified image");
#if I_FIGURED_OUT_WHY_THIS_SHOULD_BE_INCLUDED
                CASE_STATUS(ProfileNotFound, L"A color profile used when saving an image in CMYK format was not found");
#endif
        }

        return SUCCEEDED(StringCchCopy(buffer, size, L"An unknown error occured"));

#undef CASE_STATUS
}

/* Determines whether PREFIX is a case-insensitive prefix to STRING. */
BOOL
IsPrefixIgnoringCase(LPCTSTR string, LPCTSTR prefix)
{
        size_t string_length;
        if (FAILED(StringCchLength(string, STRSAFE_MAX_CCH, &string_length)))
                return FALSE;

        size_t prefix_length;
        if (FAILED(StringCchLength(prefix, STRSAFE_MAX_CCH, &prefix_length)))
                return FALSE;

        return CompareString(LOCALE_INVARIANT, NORM_IGNORECASE,
                             string, (int)min(string_length, prefix_length),
                             prefix, (int)prefix_length) == CSTR_EQUAL;
}

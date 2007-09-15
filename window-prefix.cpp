#include "stdafx.h"
#include <vld.h>
#include <stdarg.h>
#include <gdiplus.h>
#include "window-prefix.h"
#include "list.h"
#include "windowlistitem.h"
#include "windowlist.h"
#include "buffer.h"
#include "textfield.h"
#include "systray.h"
#include "hook/hook.h"

#define CORNER_SIZE 40

#define IDC_TRAYICON    1

#define IDK_BASE                1000
#define IDK_SHOW_WINDOWLIST     (IDK_BASE + 1)

#define BACKGROUND_ALPHA        0xbf

#define RIGHT_ANGLE (90.0f)

#define CORNER_PADDING          (CORNER_SIZE / 2)
#define SUB_AREA_Y_PADDING      (CORNER_PADDING / 2)

static HINSTANCE s_instance;
static LPCWSTR g_window_name;

static WindowList *g_list;
static TextField *g_buffer;
static REAL g_buffer_height;

static RectF g_title_area;
static RectF g_buffer_area;
static RectF g_window_list_area;

static Font *g_caption_font;

static Status
GraphicsSetup(Graphics *canvas)
{
        RETURN_GDI_FAILURE(canvas->SetSmoothingMode(SmoothingModeHighQuality));
        RETURN_GDI_FAILURE(canvas->SetCompositingQuality(CompositingQualityHighQuality));
        RETURN_GDI_FAILURE(canvas->SetCompositingMode(CompositingModeSourceOver));
        RETURN_GDI_FAILURE(canvas->SetTextRenderingHint(TextRenderingHintAntiAliasGridFit));
        return Ok;
}

static inline void 
RECTToRect(RECT *gdi_rect, Rect *gdiplus_rect)
{
        gdiplus_rect->X = gdi_rect->left;
        gdiplus_rect->Y = gdi_rect->top;
        gdiplus_rect->Width = RectWidth(gdi_rect);
        gdiplus_rect->Height = RectHeight(gdi_rect);
}

static Status 
GetClientPlusRect(HWND window, Rect *canvas_area)
{
        RECT rc;
        if (!GetClientRect(window, &rc))
                return Win32Error;

        RECTToRect(&rc, canvas_area);

        return Ok;
}

typedef struct _MemoryDC MemoryDC;

struct _MemoryDC
{
        HDC dc;
        HBITMAP bitmap;
        HBITMAP saved_bitmap;
};

static void 
MemoryDCFree(MemoryDC *memory_dc)
{
        if (memory_dc->dc == NULL)
                return;

        if (memory_dc->saved_bitmap != NULL)
                SelectObject(memory_dc->dc, memory_dc->saved_bitmap);

        if (memory_dc->bitmap != NULL)
                DeleteObject(memory_dc->bitmap);

        DeleteDC(memory_dc->dc);
}

static Status 
LastErrorAsStatus(VOID)
{
        switch (GetLastError()) {
        case ERROR_SUCCESS:
                return Ok;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
                return OutOfMemory;
        default:
                return Win32Error;
        }
}

static Status 
CreateMemoryDC(Rect const *canvas_area, MemoryDC *memory_dc)
{
        memory_dc->dc = CreateCompatibleDC(HDC_OF_SCREEN);
        if (memory_dc->dc == NULL)
                return LastErrorAsStatus();

        BITMAPINFO bmi;
        INITSTRUCT(bmi, TRUE);
        bmi.bmiHeader.biWidth = canvas_area->Width;
        bmi.bmiHeader.biHeight = canvas_area->Height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = canvas_area->Width * canvas_area->Height * 4;

        VOID *bits;
        memory_dc->bitmap = CreateDIBSection(memory_dc->dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (memory_dc->bitmap == NULL) {
                MemoryDCFree(memory_dc);
                return LastErrorAsStatus();
        }

        memory_dc->saved_bitmap = SelectBitmap(memory_dc->dc, memory_dc->bitmap);
        if (memory_dc->saved_bitmap == NULL) {
                MemoryDCFree(memory_dc);
                return LastErrorAsStatus();
        }

        return Ok;
}

static Status 
BackgroundAddCorner(GraphicsPath *background, INT x, INT y, INT quadrant)
{
        return background->AddArc(x, y, CORNER_SIZE, CORNER_SIZE, RIGHT_ANGLE * quadrant, RIGHT_ANGLE);
}

static Status 
DrawBackground(Graphics *canvas, Rect *area)
{
        SolidBrush background_brush(Color(BACKGROUND_ALPHA, 0, 0, 0));
        RETURN_GDI_FAILURE(background_brush.GetLastStatus());

        GraphicsPath background;
        RETURN_GDI_FAILURE(background.GetLastStatus());

        INT left = area->GetLeft();
        INT right = area->GetRight() - CORNER_SIZE;
        INT bottom = area->GetBottom() - CORNER_SIZE;
        INT top = area->GetTop();

        RETURN_GDI_FAILURE(BackgroundAddCorner(&background, left, bottom, 1));
        RETURN_GDI_FAILURE(BackgroundAddCorner(&background, left, top, 2));
        RETURN_GDI_FAILURE(BackgroundAddCorner(&background, right, top, 3));
        RETURN_GDI_FAILURE(BackgroundAddCorner(&background, right, bottom, 4));

        RETURN_GDI_FAILURE(background.CloseAllFigures());

        /* TODO: This is slow when using high quality rendition. */
        return canvas->FillPath(&background_brush, &background);
}

static Status 
DrawTitle(Graphics *canvas, Font *font, RectF *area)
{
        StringFormat format;
        RETURN_GDI_FAILURE(format.GetLastStatus());

        SolidBrush white_brush(Color::White);
        RETURN_GDI_FAILURE(white_brush.GetLastStatus());

        return canvas->DrawString(_(IDS_LIST_TITLE, L"Switch to…"), -1, font,
                                  *area, &format, &white_brush);
}

#define FREE_AND_RETURN_GDI_FAILURE(call, memory_dc) MACRO_BLOCK_START  \
        Status __return_on_gdiplus_failure_status = call;               \
if (__return_on_gdiplus_failure_status != Ok) {                         \
        MemoryDCFree(&(memory_dc));                                     \
        return __return_on_gdiplus_failure_status;                      \
}                                                                       \
MACRO_BLOCK_END

static Status 
Draw(HWND window)
{
        Rect canvas_area;
        RETURN_GDI_FAILURE(GetClientPlusRect(window, &canvas_area));

        MemoryDC memory_dc;
        RETURN_GDI_FAILURE(CreateMemoryDC(&canvas_area, &memory_dc));

        Graphics canvas(memory_dc.dc);
        FREE_AND_RETURN_GDI_FAILURE(canvas.GetLastStatus(), memory_dc);

        FREE_AND_RETURN_GDI_FAILURE(GraphicsSetup(&canvas), memory_dc);

        FREE_AND_RETURN_GDI_FAILURE(DrawBackground(&canvas, &canvas_area), memory_dc);

        FREE_AND_RETURN_GDI_FAILURE(DrawTitle(&canvas, g_caption_font, &g_title_area), memory_dc);

        FREE_AND_RETURN_GDI_FAILURE(TextFieldDraw(g_buffer, &canvas, &g_buffer_area), memory_dc);

        FREE_AND_RETURN_GDI_FAILURE(WindowListDraw(g_list, &canvas, &g_window_list_area), memory_dc);

#ifdef _DEBUG
        canvas.DrawRectangle(&Pen(Color::Red, 1), g_title_area);
        canvas.DrawRectangle(&Pen(Color::Blue, 1), g_buffer_area);
        canvas.DrawRectangle(&Pen(Color::Green, 1), g_window_list_area);
#endif

        POINT origin;
        origin.x = 0;
        origin.y = 0;

        SIZE size;
        size.cx = canvas_area.Width;
        size.cy = canvas_area.Height;

        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 0xff;
        bf.AlphaFormat = AC_SRC_ALPHA;

        HDC canvas_dc = canvas.GetHDC();
        UpdateLayeredWindow(window, NULL, NULL, &size, canvas_dc, &origin, 0, &bf, ULW_ALPHA);
        canvas.ReleaseHDC(canvas_dc);

        MemoryDCFree(&memory_dc);

        return Ok;
}

static Status
CalculateSubAreas(Graphics *canvas, PointF *origin, Rect *constraint_area)
{
        SizeF buffer_size;
        TextFieldSize(g_buffer, canvas, &buffer_size);

        SizeF window_list_size;
        RETURN_GDI_FAILURE(WindowListSize(g_list, canvas, &window_list_size));

        g_title_area.X = origin->X;
        g_title_area.Y = origin->Y;
        g_title_area.Width = min(max(buffer_size.Width, window_list_size.Height), constraint_area->Width);
        g_title_area.Height = g_caption_font->GetHeight(canvas);

        /* The buffer area’s width is set to be the same as that of the
         * window list, as at the time this procedure is called, the
         * buffer will always be empty, thus having a width of zero. */
        g_buffer_area.X = g_title_area.X;
        g_buffer_area.Y = g_title_area.GetBottom() + SUB_AREA_Y_PADDING;
        g_buffer_area.Width = window_list_size.Width;
        g_buffer_area.Height = buffer_size.Height;

        g_window_list_area.X = g_title_area.X;
        g_window_list_area.Y = g_buffer_area.GetBottom() + SUB_AREA_Y_PADDING;
        g_window_list_area.Width = window_list_size.Width;
        g_window_list_area.Height = window_list_size.Height;

        return Ok;
}

static void 
AdjustWindowSize(HWND window)
{
        Graphics canvas(window);
        GraphicsSetup(&canvas);

        RECT work_area;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);

        INT half_screen_width = RectWidth(&work_area) / 2;
        INT quarter_screen_width = half_screen_width / 2;

        Rect constraint_area(quarter_screen_width, work_area.top, half_screen_width, RectHeight(&work_area));

        CalculateSubAreas(&canvas, &PointF(CORNER_PADDING, CORNER_PADDING), &constraint_area);

        INT width = (INT)(max(min(max(max(g_title_area.Width,
                                          g_buffer_area.Width),
                                      g_window_list_area.Width),
                                  half_screen_width),
                              quarter_screen_width)
                          + 2 * CORNER_SIZE);
        INT height = (INT)(g_window_list_area.GetBottom() + CORNER_PADDING);

        g_title_area.Width = min(g_title_area.Width, width - CORNER_SIZE);
        g_buffer_area.Width = min(g_buffer_area.Width, width - CORNER_SIZE);
        g_window_list_area.Width = min(g_window_list_area.Width, width - CORNER_SIZE);

        RECT current_area;
        GetWindowRect(window, &current_area);
        if (RectWidth(&current_area) == width && RectHeight(&current_area) == height)
                return;

        MoveWindow(window,
                   half_screen_width - width / 2,
                   (RectHeight(&work_area) - height) / 2,
                   width, height, FALSE);
}

static void
SwitchToAndHide(WindowListItem *item, HWND window)
{
        if (item == NULL)
                return;

        WindowListItemSwitchTo(item);
        HideWindow(window);
}

static BOOL 
CharDigitValue(TCHAR c, int *value)
{
        TCHAR folded[1];
        if (FoldString(MAP_FOLDDIGITS, &c, 1, folded, 1) == 0)
                return FALSE;

        *value = folded[0] - L'0';

        return TRUE;
}

static LRESULT 
HandleDigits(HWND window, TCHAR digit)
{
#if 0
        int n = digit - L'0';
#endif
        int n;
        if (!CharDigitValue(digit, &n))
                return FALSE;
        if (n == 0)
                n = 10;

        SwitchToAndHide(WindowListNthShown(g_list, n), window);
        return 0;
}


static LRESULT 
OnSysChar(HWND window, TCHAR c, int cRepeat)
{
        BOOL control = GetAsyncKeyState(VK_CONTROL);

        return TextFieldOnChar(g_buffer, c, cRepeat, control);
}

static BOOL 
IsDigit(TCHAR c)
{
        WORD type;
        if (!GetStringTypeEx(LOCALE_USER_DEFAULT, CT_CTYPE1, &c, 1, &type))
                return FALSE;

        return type & C1_DIGIT;
}

static LRESULT 
OnChar(HWND window, TCHAR c, int cRepeat)
{
        if (IsDigit(c))
                return HandleDigits(window, c);

        BOOL control = GetAsyncKeyState(VK_CONTROL);

        if (c == VK_CANCEL || c == VK_ESCAPE) {
                HideWindow(window);
                return 0;
        } else if (c == VK_RETURN) { // TODO: this needs to be modified once we can move between items.
                SwitchToAndHide(WindowListNthShown(g_list, 1), window);
                return 0;
        }

        return TextFieldOnChar(g_buffer, c, cRepeat, control);
}

static LRESULT 
OnKey(HWND window, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
        if (!fDown)
                return 0;

        BOOL control = GetAsyncKeyState(VK_CONTROL);

        if (control && IsDigit(vk))
                return HandleDigits(window, vk);

        return 0;
}

static LRESULT 
OnKillFocus(HWND window, HWND window_focus)
{
        UNREFERENCED_PARAMETER(window_focus);

        BufferReset(TextFieldBuffer(g_buffer));
        HideWindow(window);

        return 0;
}

static void 
MyBufferEventHandler(Buffer *buffer, BufferEvent event, VOID *closure)
{
        if (event & BUFFER_ON_CHANGE) {
                HWND main_window = (HWND)closure;
                WindowListFilter(g_list, BufferContents(buffer));
                if (WindowListLengthShown(g_list) == 1) {
                        SwitchToAndHide(WindowListNthShown(g_list, 1), main_window);
                        return;
                }
                RedrawWindow(main_window, NULL, NULL, RDW_INTERNALPAINT);
        }
}

static void 
DisplayWindowListIfNotAlreadyDisplayed(HWND window)
{
        if (g_list != NULL)
                WindowListFree(g_list);
        g_list = WindowListNew(g_caption_font);

        if (GetForegroundWindow() == window && g_list != NULL && WindowListLength(g_list) > 1) {
                SwitchToAndHide(WindowListNthShown(g_list, 2), window);
                return;
        }

        BufferReset(TextFieldBuffer(g_buffer));
        WindowListFilter(g_list, BufferContents(TextFieldBuffer(g_buffer)));
        AdjustWindowSize(window);
        /* TODO: Call RedrawWindow instead? */
        Draw(window);

        SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
        SetForegroundWindow(window);
}

static LRESULT 
OnHotKey(HWND window, int id, UINT modifiers, UINT vkey)
{
        UNREFERENCED_PARAMETER(id);
        UNREFERENCED_PARAMETER(modifiers);
        UNREFERENCED_PARAMETER(vkey);

        DisplayWindowListIfNotAlreadyDisplayed(window);

        return 0;
}

static LRESULT 
OnDestroy(HWND window)
{
        SystrayRemoveIcon(window, IDC_TRAYICON);
        PostQuitMessage(EXIT_SUCCESS);

        return 0;
}

static LRESULT 
DisplayTrayMenu(HWND window)
{
        HMENU menu_container = LoadMenu(s_instance, MAKEINTRESOURCE(IDR_TRAYMENU));
        if (menu_container == NULL)
                goto cleanup;

        HMENU tray_menu = GetSubMenu(menu_container, 0);
        if (tray_menu == NULL)
                goto cleanup;

        SetMenuDefaultItem(tray_menu, IDM_TRAYMENU_EXIT, FALSE);

        POINT cursor;
        if (!GetCursorPos(&cursor))
                goto cleanup;

        SetForegroundWindow(window);
        TrackPopupMenu(tray_menu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                       cursor.x, cursor.y, 0, window, NULL);

cleanup:
        if (menu_container != NULL)
                DestroyMenu(menu_container);

        return 0;
}

static LRESULT 
OnTrayNotify(HWND window, UINT id, UINT notify_code) 
{
        switch (notify_code) {
        case WM_RBUTTONUP:
                return DisplayTrayMenu(window);
        default:
                return 0;
        }
}

static LRESULT 
OnCommand(HWND window, int id, HWND control, UINT notify_code)
{
        UNREFERENCED_PARAMETER(control);
        UNREFERENCED_PARAMETER(notify_code);

        switch (id) {
        case IDM_TRAYMENU_EXIT:
                DestroyWindow(window);
                break;
        case IDK_SHOW_WINDOWLIST:
                DisplayWindowListIfNotAlreadyDisplayed(window);
                break;
        default:
                break;
        }

        return 0;
}

static LRESULT 
OnFontChange(HWND window)
{
        Font *new_caption_font;
        if (LoadWindowCaptionFont(&new_caption_font) != Ok)
                return 0;

        delete g_caption_font;
        g_caption_font = new_caption_font;
        TextFieldSetFont(g_buffer, g_caption_font);
        WindowListSetFont(g_list, g_caption_font);

        return 0;
}

static LRESULT 
OnWPHookWindowIconChanged(HWND window, HWND changed_window)
{
        UNREFERENCED_PARAMETER(window);

        WindowIconIconChanged(changed_window);
        return 0;
}

static LRESULT 
OnPaint(HWND window)
{
        Draw(window);

        return 0;
}

static LRESULT CALLBACK 
WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message) {
                HANDLE_MSG(window, WM_TRAYNOTIFY, OnTrayNotify);
                HANDLE_MSG(window, WM_COMMAND, OnCommand);
                HANDLE_MSG(window, WM_KILLFOCUS, OnKillFocus);
                HANDLE_MSG(window, WM_CHAR, OnChar);
                HANDLE_MSG(window, WM_SYSCHAR, OnSysChar);
                HANDLE_MSG(window, WM_KEYDOWN, OnKey);
                HANDLE_MSG(window, WM_HOTKEY, OnHotKey);
                HANDLE_MSG(window, WM_DESTROY, OnDestroy);
                HANDLE_MSG(window, WM_FONTCHANGE, OnFontChange);
                HANDLE_MSG(window, WM_WPHOOK_WINDOW_ICON_CHANGED, OnWPHookWindowIconChanged);
                HANDLE_MSG(window, WM_PAINT, OnPaint);
        default: return DefWindowProc(window, message, wParam, lParam);
        }
}

void 
ReportErrorMessage(LPCTSTR message)
{
        MessageBox(NULL, message, g_window_name, MB_OK | MB_ICONERROR);
}

void 
ReportError(Error *error)
{
        ReportErrorMessage(ErrorMessage(error));
        ErrorFree(error);
}

static BOOL 
GdiInitialize(ULONG_PTR *token, Error **error)
{
        GdiplusStartupInput gdiplusStartupInput;
        Status status = GdiplusStartup(token, &gdiplusStartupInput, NULL);
        if (status == Ok)
                return TRUE;

        *error = GdiErrorNew(status, 
                             L"Unable to initialize GDI+.\r\n"
                             L"\r\n"
                             L"Make sure that GDI+ is installed on your system.");
        return FALSE;
}

static BOOL 
WindowCaptionFontInitialize(Error **error)
{
        Status status = LoadWindowCaptionFont(&g_caption_font);
        if (status == Ok)
                return TRUE;

        *error = GdiErrorNew(status,
                             L"Unable to load any font to typeset with.\r\n"
                             L"Make sure you haven’t removed your font folder and that\r\n"
                             L"fonts like “Segoe UI”, “Tahoma”, or “Times New Roman” are available.\r\n"
                             L"\r\n"
                             L"Exiting, as there’s no way to continue without a font.");
        return FALSE;
}

static BOOL 
RegisterMainWindowClass(HINSTANCE instance, ATOM *window_class, Error **error)
{
        WNDCLASSEX wclass;

        INITSTRUCT(wclass, TRUE);
        wclass.style = CS_HREDRAW | CS_VREDRAW;
        wclass.lpfnWndProc = WndProc;
        wclass.hInstance = instance;
        wclass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_WINDOWPREFIX));
        wclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wclass.lpszClassName = _(IDS_WINDOW_CLASS, L"WINDOW-PREFIX");
        wclass.hIconSm = wclass.hIcon;

        *window_class = RegisterClassEx(&wclass);
        if (*window_class == 0)
                return TRUE;

        *error = ErrorNew(L"Unable to register main window’s window-class.\r\n"
                          L"\r\n"
                          L"The window class is needed so that Microsoft Windows knows\r\n"
                          L"how to deal with windows of its kind.\r\n"
                          L"\r\n"
                          L"Exiting, as there’s no way to continue without the window class.");
        return FALSE;
}

static BOOL 
CreateMainWindow(HINSTANCE instance, ATOM window_class, HWND *window, Error **error)
{
        *window = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                 (LPCTSTR)(LONG_PTR)MAKELONG(window_class, 0),
                                 g_window_name, WS_POPUP, 0, 0, 0, 0, NULL,
                                 NULL, instance, NULL);
        if (*window != NULL)
                return TRUE;

        *error = ErrorNew(L"Unable to create main window.\r\n"
                          L"\r\n"
                          L"Exiting, as there’s no way to continue without the main window.");
        return FALSE;
}

#ifdef _DEBUG
#  define MY_HOTKEY_KEY L'5'
#else
#  define MY_HOTKEY_KEY L'4'
#endif

static BOOL 
InstallHotkey(HWND window, Error **error)
{
        /* TODO: Is this a fatal error?  I mean, a user could theoretically still
         * use the application, as we do respond to messages and they could post
         * the message to display the main window anyway.  Still, the only reason
         * for this failing is a misconfiguration or if we’re already running an
         * instance. */
        RegisterHotKey(window, 0, MOD_CONTROL, VkKeyScan(MY_HOTKEY_KEY));

        return TRUE;
}

static BOOL 
InstallSystrayIcon(HWND window, HINSTANCE instance, Error **error)
{
        /* NOTE: This may fail, but we don’t care.  It’s not a reason to exit the application. */
        SystrayAddIcon(window, IDC_TRAYICON, LoadIcon(instance, MAKEINTRESOURCE(IDI_WINDOWPREFIX)), g_window_name);

        return TRUE;
}

int 
MainLoop(VOID)
{
        MSG message;
        while (GetMessage(&message, NULL, 0, 0)) {
                TranslateMessage(&message);
                DispatchMessage(&message);
        }

        return (int)message.wParam;
}

int APIENTRY 
_tWinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
        UNREFERENCED_PARAMETER(hPrevInstance);
        UNREFERENCED_PARAMETER(lpCmdLine);
        UNREFERENCED_PARAMETER(nCmdShow);

        s_instance = instance;

        TranslationInitialize(instance);

        g_window_name = _(IDS_WINDOW_NAME, L"Window*");

        int exit_value = EXIT_FAILURE;
        ULONG_PTR gdiplus_token = NULL;
        Error *error = NULL;
        if (!GdiInitialize(&gdiplus_token, &error))
                goto cleanup;

        if (!WindowCaptionFontInitialize(&error))
                goto cleanup;

        if (!WindowIconInitialize(&error))
                goto cleanup;

        ATOM window_class;
        if (!RegisterMainWindowClass(instance, &window_class, &error))
                goto cleanup;

        HWND main_window;
        if (!CreateMainWindow(instance, window_class, &main_window, &error))
                goto cleanup;

        if (!InstallHotkey(main_window, &error))
                goto cleanup;

        if (!InstallSystrayIcon(main_window, instance, &error))
                goto cleanup;

        g_buffer = TextFieldNew(g_caption_font);
        if (g_buffer == NULL)
                goto cleanup;

        BufferRegisterListener(TextFieldBuffer(g_buffer), BUFFER_ON_CHANGE, MyBufferEventHandler, main_window);

        if (!WPHookRegister(main_window))
                goto cleanup;

        exit_value = MainLoop();

cleanup:
        WPHookUnregister();

        if (g_list != NULL)
                WindowListFree(g_list);

        if (g_buffer != NULL)
                TextFieldFree(g_buffer);

        WindowIconFinalize();

        if (g_caption_font != NULL)
                delete g_caption_font;

        if (gdiplus_token != NULL)
                GdiplusShutdown(gdiplus_token);

        if (error != NULL)
                ReportError(error);

        return exit_value;
}

#include "stdafx.h"
#include <vld.h>
#include <stdarg.h>
#include <gdiplus.h>
#include <shlobj.h>
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

#define TITLE_STRING _(IDS_LIST_TITLE, L"Switch to…")

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
        RETURN_GDI_FAILURE(canvas->SetTextRenderingHint(TextRenderingHintClearTypeGridFit));
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

        return canvas->DrawString(TITLE_STRING, -1, font, *area, &format, &white_brush);
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

        RectF title_area;
        RETURN_GDI_FAILURE(canvas->MeasureString(TITLE_STRING,
                                                   -1,
                                                   g_caption_font,
                                                   PointF(0.0f, 0.0f),
                                                   &title_area));

        g_title_area.X = origin->X;
        g_title_area.Y = origin->Y;
        g_title_area.Width = title_area.Width; //window_list_size.Width;
        REAL height = g_caption_font->GetHeight(canvas);
        RETURN_GDI_FAILURE(g_caption_font->GetLastStatus());
        g_title_area.Height = height;

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

        INT width = (INT)(min(max(max(g_title_area.Width,
                                          g_buffer_area.Width),
                                      g_window_list_area.Width),
                                  half_screen_width)
                          + CORNER_SIZE);
        INT height = (INT)(g_window_list_area.GetBottom() + CORNER_PADDING);

        g_title_area.Width = min(g_title_area.Width, width - CORNER_SIZE / 2);
        g_buffer_area.Width = min(g_buffer_area.Width, width - CORNER_SIZE / 2);
        g_window_list_area.Width = min(g_window_list_area.Width, width - CORNER_SIZE / 2);

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

        if (WindowListLength(g_list) < 2) {
                Sleep(1000);

                BLENDFUNCTION blend;

                blend.BlendOp = AC_SRC_OVER;
                blend.BlendFlags = 0;
                blend.SourceConstantAlpha = 255;
                blend.AlphaFormat = AC_SRC_ALPHA;

                while (blend.SourceConstantAlpha > 25) {
                        blend.SourceConstantAlpha -= 25;
                        UpdateLayeredWindow(window, NULL, NULL, NULL, NULL, NULL, NULL, &blend, ULW_ALPHA);
                        Sleep(12);
                }
                if (WindowListLength(g_list) > 0)
                        SwitchToAndHide(WindowListNthShown(g_list, 0), window);
                else
                        HideWindow(window);
        }
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
        if (*window_class != 0)
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
#  define MY_HOTKEY_KEY L'd'
#else
#  define MY_HOTKEY_KEY L'c'
#endif

static LPCTSTR
ParseModifier(LPCTSTR input, UINT *modifier)
{
        static struct {
                LPCTSTR string;
                UINT modifier;
        } const strings[] = {
                { L"Alt", MOD_ALT },
                { L"Ctrl", MOD_CONTROL },
                { L"Shift", MOD_SHIFT },
                { L"Win", MOD_WIN }
        };

        int i;
        for (i = 0; i < _countof(strings); i++)
                if (IsPrefixIgnoringCase(input, strings[i].string))
                        break;

        if (i == _countof(strings))
                return NULL;

        *modifier = strings[i].modifier;
        size_t length;
        if (FAILED(StringCchLength(strings[i].string, STRSAFE_MAX_CCH, &length)))
                return NULL;

        return input + length;
}

static BOOL
ParseKey(LPCTSTR input, UINT *key)
{
        static struct {
                LPCTSTR string;
                UINT key;
        } const strings[] = {
                { L"Backspace", VK_BACK },
                { L"BS", VK_BACK },
                { L"Tab", VK_TAB },
                { L"Enter", VK_RETURN },
                { L"Return", VK_RETURN },
                { L"Pause", VK_PAUSE },
                { L"Break", VK_PAUSE },
                { L"Escape", VK_ESCAPE },
                { L"Esc", VK_ESCAPE },
                { L"Space", VK_SPACE },
                { L"Page Up", VK_PRIOR },
                { L"Page Down", VK_NEXT },
                { L"End", VK_END },
                { L"Home", VK_HOME },
                { L"Left", VK_LEFT },
                { L"Up", VK_UP },
                { L"Right", VK_RIGHT },
                { L"Down", VK_DOWN },
                { L"Select", VK_SELECT },
                { L"Print Screen", VK_PRINT },
                { L"PrintScr", VK_PRINT },
                { L"Insert", VK_INSERT },
                { L"Ins", VK_INSERT },
                { L"Delete", VK_DELETE },
                { L"Del", VK_DELETE },
                { L"Help", VK_HELP },
                { L"Numpad 0", VK_NUMPAD0 },
                { L"Numpad 1", VK_NUMPAD1 },
                { L"Numpad 2", VK_NUMPAD2 },
                { L"Numpad 3", VK_NUMPAD3 },
                { L"Numpad 4", VK_NUMPAD4 },
                { L"Numpad 5", VK_NUMPAD5 },
                { L"Numpad 6", VK_NUMPAD6 },
                { L"Numpad 7", VK_NUMPAD7 },
                { L"Numpad 8", VK_NUMPAD8 },
                { L"Numpad 9", VK_NUMPAD9 },
                { L"Numpad *", VK_MULTIPLY },
                { L"Numpad +", VK_MULTIPLY },
                { L"Numpad 1000 Separator", VK_SEPARATOR },
                { L"Numpad -", VK_SUBTRACT },
                { L"Numpad .", VK_DECIMAL },
                { L"Numpad ,", VK_DECIMAL },
                { L"Numpad /", VK_DIVIDE },
                { L"F1", VK_F1 },
                { L"F2", VK_F2 },
                { L"F3", VK_F3 },
                { L"F4", VK_F4 },
                { L"F5", VK_F5 },
                { L"F6", VK_F6 },
                { L"F7", VK_F7 },
                { L"F8", VK_F8 },
                { L"F9", VK_F9 },
                { L"F10", VK_F10 },
                { L"F11", VK_F11 },
                { L"F12", VK_F12 },
                { L"F13", VK_F13 },
                { L"F14", VK_F14 },
                { L"F15", VK_F15 },
                { L"F16", VK_F16 },
                { L"F17", VK_F17 },
                { L"F18", VK_F18 },
                { L"F19", VK_F19 },
                { L"F20", VK_F20 },
                { L"F21", VK_F21 },
                { L"F22", VK_F22 },
                { L"F23", VK_F23 },
                { L"F24", VK_F24 },
                { L"Scroll Lock", VK_SCROLL },
                { L"Browser Back", VK_BROWSER_BACK },
                { L"Browser Forward", VK_BROWSER_FORWARD },
                { L"Browser Refresh", VK_BROWSER_REFRESH },
                { L"Browser Stop", VK_BROWSER_STOP },
                { L"Browser Search", VK_BROWSER_SEARCH },
                { L"Browser Favorites", VK_BROWSER_FAVORITES },
                { L"Browser Home", VK_BROWSER_HOME },
                { L"Volume Mute", VK_VOLUME_MUTE },
                { L"Volume Down", VK_VOLUME_DOWN },
                { L"Volume Up", VK_VOLUME_UP },
                { L"Media Next Track", VK_MEDIA_NEXT_TRACK },
                { L"Media Previous Track", VK_MEDIA_PREV_TRACK },
                { L"Media Play/Pause", VK_MEDIA_PLAY_PAUSE },
                { L"Launch Mail", VK_LAUNCH_MAIL },
                { L"Launch Media Select", VK_LAUNCH_MEDIA_SELECT },
                { L"Launch Application 1", VK_LAUNCH_APP1 },
                { L"Launch Application 2", VK_LAUNCH_APP2 },
        };

        size_t remaining;
        if (FAILED(StringCchLength(input, STRSAFE_MAX_CCH, &remaining)))
                return FALSE;

        if (remaining == 0)
                return FALSE;

        /* Check names */
        int i = 0;
        for (i = 0; i < _countof(strings); i++) {
                if (CompareString(LOCALE_INVARIANT, NORM_IGNORECASE,
                                  strings[i].string, remaining,
                                  input, -1) == CSTR_EQUAL) {
                        *key = strings[i].key;
                        return TRUE;
                }
        }

        /* Check for a C-like number */
        if (*input == L'<' && remaining > 1) {
                LPTSTR end;
                int key_scan = _tcstol(CharNext(input), &end, 0);
                if (end != input && *end == L'>') {
                        *key = key_scan;
                        return TRUE;
                }
        }

        if (remaining > 1)
                return FALSE;

        *key = VkKeyScan(*input);
        return TRUE;
}

static void
DetermineHotkey(UINT *modifiers, UINT *key)
{
        *modifiers = MOD_WIN;
        *key = VkKeyScan(MY_HOTKEY_KEY);

        TCHAR settings[MAX_PATH];
        if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA |
                                      CSIDL_FLAG_DONT_UNEXPAND |
                                      CSIDL_FLAG_DONT_VERIFY, NULL,
                                      SHGFP_TYPE_CURRENT, settings)))
                return;

        if (FAILED(StringCchCat(settings, MAX_PATH, L"\\Window Prefix\\settings.ini")))
                return;

        int length = MAX_PATH;
        TCHAR *specification = new TCHAR[length];
        while (GetPrivateProfileString(L"Settings", L"Key", NULL, specification, length, settings) == length - 1) {
                length *= 2;
                delete specification;
                specification = new TCHAR[length];
        }

        LPCTSTR p = specification;
        UINT parsed_modifiers = 0;
        while (true) {
                UINT parsed_modifier;
                LPCTSTR q = ParseModifier(p, &parsed_modifier);
                if (q == NULL)
                        break;
                parsed_modifiers |= parsed_modifier;
                if (!IsPrefixIgnoringCase(q, L"+"))
                        goto cleanup;
                p = q + 1;
        }

        UINT parsed_key;
        if (!ParseKey(p, &parsed_key))
                goto cleanup;

        *modifiers = parsed_modifiers;
        *key = parsed_key;

cleanup:
        delete specification;
}

static BOOL 
InstallHotkey(HWND window, Error **error)
{
        UINT modifiers, key;
        DetermineHotkey(&modifiers, &key);

#if _DEBUG
        modifiers = MOD_CONTROL;
        key = MY_HOTKEY_KEY;
#endif

        /* TODO: Is this a fatal error?  I mean, a user could theoretically still
         * use the application, as we do respond to messages and they could post
         * the message to display the main window anyway.  Still, the only reason
         * for this failing is a misconfiguration or if we’re already running an
         * instance. */
        RegisterHotKey(window, 0, modifiers, key);

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

        /* TODO: Load hook.dll dynamically and fail gracefully? */
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

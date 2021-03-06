﻿#include <stdafx.h>

#include "bitmap.h"
#include "windowicon.h"
#include "list.h"

/* The default icon to use when no other icon can be provided. */
static Bitmap *s_default_icon;

static List *s_cache;
static int s_cache_size;
static int s_cache_next_compression_size = 20;

typedef struct _CacheEntry CacheEntry;

struct _CacheEntry
{
        HWND window;
        Bitmap *icon;
};

static CacheEntry *
CacheEntryNew(HWND window, Bitmap *icon)
{
        CacheEntry *entry = ALLOC_STRUCT(CacheEntry);

        entry->window = window;
        entry->icon = icon;

        return entry;
}

static void 
CacheEntryFree(CacheEntry *entry)
{
        delete entry->icon;
        FREE(entry);
}

static BOOL 
CacheEntryWindowEqual(CacheEntry *entry, HWND window)
{
        return entry->window == window;
}

static CacheEntry *
CacheLookup(HWND window)
{
        return (CacheEntry *)ListFindItem(s_cache, (EqualityFunc)CacheEntryWindowEqual, window);
}

static BOOL 
CacheEntryInvalid(CacheEntry *entry)
{
        return !IsWindow(entry->window);
}

static void 
CacheCompress(void)
{
        if (s_cache_size < s_cache_next_compression_size)
                return;

        s_cache = ListRemoveIf(s_cache, (PredicateFunc)CacheEntryInvalid, (FreeFunc)CacheEntryFree);
        s_cache_size = ListLength(s_cache);
        s_cache_next_compression_size = max(s_cache_size * 2, 20);
}

static void 
CacheAdd(HWND window, Bitmap *icon)
{
        CacheEntry *entry = CacheEntryNew(window, icon);
        if (entry == NULL)
                return;

        if (!ListCons(&s_cache, entry))
                CacheEntryFree(entry);
        else
                s_cache_size++;
}

static BOOL 
CacheUpdate(HWND window, Bitmap *icon)
{
        CacheEntry *entry = CacheLookup(window);
        if (entry == NULL)
                return FALSE;

        delete entry->icon;
        entry->icon = icon;

        return TRUE;
}

static VOID 
CachePut(HWND window, Bitmap *icon)
{
        if (CacheUpdate(window, icon))
                return;

        CacheAdd(window, icon);
        CacheCompress();
}

static BOOL 
CacheGet(HWND window, Bitmap **icon)
{
        CacheEntry *entry = CacheLookup(window);
        if (entry == NULL)
                return FALSE;

        *icon = entry->icon;

        return TRUE;
}

/* Gets the HICON for ICON_ID ({ICON_BIG, ICON_SMALL, ICON_SMALL2}) associated
 * with WINDOW.  Returns FALSE if unable to retrieve this icon and sets
 * WAS_HUNG to true if the reason it fail was because the message timed out. */
static BOOL 
GetWindowHIconByMessage(HWND window, WPARAM icon_id, HICON *icon, BOOL *was_hung)
{
        *icon = NULL;
        *was_hung = FALSE;

        if (SendMessageTimeout(window, WM_GETICON, icon_id, 0, SMTO_ABORTIFHUNG, HUNG_TIMEOUT, (PDWORD_PTR)icon))
                return *icon != NULL;

        *was_hung = LastErrorWasTimeout();

        return FALSE;
}

/* Gets the HICON for ICON_ID ({ICON_BIG, ICON_SMALL, ICON_SMALL2}) associated
 * with WINDOW, also trying to get it by calling GetClassLongPtr(WINDOW, CLASS_LONG_ICON).
 * Returns FALSE if unable to retrieve both of these icons. */
static BOOL 
GetWindowHIcon(HWND window, WPARAM icon_id, int class_long_icon, HICON *icon)
{
        BOOL was_hung;

        if (GetWindowHIconByMessage(window, icon_id, icon, &was_hung))
                return TRUE;

        if (was_hung)
                return FALSE;

        *icon = (HICON)(UINT_PTR)GetClassLongPtr(window, class_long_icon);
        return *icon != NULL;
}

/* Iterator setting the alpha channel’s value by checking the bitmap mask’s pixels.
 * If the mask’s pixel is black, set the alpha channel to opaque and to transparent
 * otherwise. */
static Status 
BitmapCopyWithMaskIterator(Point point, ARGB pixel, VOID *closure, IterationState *state)
{
        UNREFERENCED_PARAMETER(state);

        BitmapIterateForCopyClosure *copy_closure = (BitmapIterateForCopyClosure *)closure;
        Bitmap *mask = (Bitmap *)copy_closure->inner_closure;

        Color mask_pixel;
        RETURN_GDI_FAILURE(mask->GetPixel(point.X, point.Y, &mask_pixel));
        BOOL visible = mask_pixel.GetValue() == Color::Black;
        ARGB fixed_pixel = ARGBSetAlpha(pixel, visible ? ALPHA_OPAQUE : ALPHA_TRANSPARENT);
        BitmapDataRow(&copy_closure->copy_data, point.Y)[point.X] = fixed_pixel;

        return Ok;
}

/* Creates a Bitmap from a Bitmap SOURCE created from a HBITMAP of an icon.
 * If SOURCE has less than 32 bits of information per pixel, use the
 * non-alpha-bitmap copy-routine.  Otherwise, if SOURCE has pixels with the
 * alpha channel set, do a simple per-pixel copy of it.  Otherwise, create
 * the copy by copying pixels and setting the alpha channel based on HB_MASK,
 * which should be a 1-bit-per-pixel HBITMAP where pixels that should be
 * transparent are set to 1 and pixels that should be opaque are sot to 0. */
static Status
BitmapFromIconBitmap(Bitmap *source, HBITMAP hb_mask, Bitmap **icon)
{
        if (GetPixelFormatSize(source->GetPixelFormat()) < 32)
                return NonAlphaBitmapCopy(source, icon);

        BOOL has_alpha;
        RETURN_GDI_FAILURE(BitmapHasAlpha(source, &has_alpha));
        if (has_alpha)
                return BitmapCopy(source, icon);

        Bitmap mask(hb_mask, NULL);
        RETURN_GDI_FAILURE(mask.GetLastStatus());

        return BitmapIterateForCopy(source, BitmapIterate, BitmapCopyWithMaskIterator, &mask, icon);
}

/* Scales BITMAP to the relevant system metrics for displaying small icons
 * (SM_CXSMICON, SMCYSMICON). BITMAP may be freed, a new bitmap being returned
 * instead. */
static Status 
BitmapScaleToSystemMetrics(Bitmap **bitmap)
{
        UINT width, height;
        RETURN_GDI_FAILURE(BitmapGetDimensions(*bitmap, &width, &height));

        UINT desired_width = (UINT)GetSystemMetricsDefault(SM_CXSMICON, DEFAULT_ICON_DELTA);
        UINT desired_height = (UINT)GetSystemMetricsDefault(SM_CYSMICON, DEFAULT_ICON_DELTA);

        if (width == desired_width && height == desired_height)
                return Ok;

        Bitmap *scaled_bitmap = new Bitmap(desired_width, desired_height, (*bitmap)->GetPixelFormat());
        if (scaled_bitmap == NULL) {
                delete *bitmap;

                return OutOfMemory;
        }

        Status status = scaled_bitmap->GetLastStatus();
        if (status != Ok) {
                delete scaled_bitmap;
                delete *bitmap;

                return status;
        }

        Graphics canvas(scaled_bitmap);
        status = canvas.GetLastStatus();
        if (status == Ok)
                status = canvas.SetInterpolationMode(InterpolationModeHighQualityBilinear);
        if (status == Ok)
                status = canvas.DrawImage(*bitmap, 0, 0, desired_width, desired_height);

        if (status != Ok) {
                delete scaled_bitmap;
                delete *bitmap;

                return status;
        }

        delete *bitmap;
        *bitmap = scaled_bitmap;

        return Ok;
}

/* Creates a Bitmap from ICON.  See BitmapFromIconBitmap() for more details. */
static Status 
BitmapFromHIcon(HICON icon, Bitmap **icon_bitmap)
{
        ICONINFO info;
        if (!GetIconInfo(icon, &info))
                return Win32Error;

        Bitmap bitmap(info.hbmColor, NULL);
        RETURN_GDI_FAILURE(bitmap.GetLastStatus());

        Status status = BitmapFromIconBitmap(&bitmap, info.hbmMask, icon_bitmap);

        DeleteObject(info.hbmColor);
        DeleteObject(info.hbmMask);

        return status;
}

/* Loads the default icon used for windows. */
static inline BOOL 
LoadDefaultWindowIcon(HICON *icon)
{
        *icon = LoadIcon(NULL, IDI_APPLICATION);
        return *icon != NULL;
}

/* Creates a transparent icon of iconic dimensions. */
static Status 
CreateTransparentIcon(Bitmap **icon)
{
        *icon = new Bitmap(GetSystemMetricsDefault(SM_CXSMICON, DEFAULT_ICON_DELTA),
                           GetSystemMetricsDefault(SM_CYSMICON, DEFAULT_ICON_DELTA));
        if (*icon == NULL)
                return OutOfMemory;

        Status status = (*icon)->GetLastStatus();
        if (status != Ok) {
                delete *icon;
                return status;
        }

        return Ok;
}

/* A function setting an input parameter to an icon. */
typedef Status (*IconSetterFunc)(Bitmap **);

/* Set-ups a window icon from a HICON, using SET.  Frees the HICON, and
 * scales the resulting window icon to system metrics. */
static Status 
SetupWindowIconFromHIcon(HWND window, HICON hicon, IconSetterFunc set, Bitmap **icon)
{
        Status status = BitmapFromHIcon(hicon, icon);

        DeleteObject(hicon);

        if (status != Ok)
                return set(icon);

        status = BitmapScaleToSystemMetrics(icon);
        if (status != Ok)
                return set(icon);

        if (window != NULL)
                CachePut(window, *icon);

        return Ok;
}

/* Sets ICON to the default window-icon. */
static Status 
SetToDefaultIcon(Bitmap **icon)
{
        *icon = s_default_icon;

        return Ok;
}

/* Sets up the default icon. */
static Status 
SetupDefaultIcon(VOID)
{
        HICON application_icon;
        if (!LoadDefaultWindowIcon(&application_icon))
                return CreateTransparentIcon(&s_default_icon);

        return SetupWindowIconFromHIcon(NULL, application_icon, CreateTransparentIcon, &s_default_icon);
}

/* Sets up the window-icon management code.  Currently, this only creates
 * a transparent window-icon to return whenever a window icon can’t be
 * otherwise created. */
BOOL 
WindowIconInitialize(Error **error)
{
        Status status = SetupDefaultIcon();
        if (status == Ok)
                return TRUE;

        *error = GdiErrorNew(status,
                             L"Failed to load an icon to fall back upon when drawing.\r\n"
                             L"\r\n"
                             L"Exiting, as an icon is needed for drawing the window list.");
        return FALSE;
}

void 
WindowIconFinalize(VOID)
{
        if (s_default_icon != NULL)
                delete s_default_icon;

        ListFree(s_cache, (FreeFunc)CacheEntryFree);
}

static Status 
WindowIconNewCached(HWND window, Bitmap **icon)
{
        if (s_default_icon == NULL)
                return WrongState;

        HICON small_icon;
        BOOL was_hung;

        if (!GetWindowHIconByMessage(window, ICON_SMALL, &small_icon, &was_hung))
                if (was_hung || !GetWindowHIcon(window, ICON_SMALL2, GCLP_HICONSM, &small_icon))
                        return SetToDefaultIcon(icon);

        return SetupWindowIconFromHIcon(window, small_icon, SetToDefaultIcon, icon);
}

/* Gets WINDOW’s (small) icon.  Always succeeds, unless WindowIconInitialize() hasn’t
 * been called, in which case WrongState is returned. */
Status 
WindowIconNew(HWND window, Bitmap **icon)
{
        if (CacheGet(window, icon))
                return Ok;

        return WindowIconNewCached(window, icon);
}

void 
WindowIconIconChanged(HWND window)
{
        Bitmap *icon;
        WindowIconNewCached(window, &icon);
}

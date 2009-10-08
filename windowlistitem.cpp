#include "stdafx.h"

#include "windowlistitem.h"

/* The amount of padding of icons on the x-axis. */
#define ICON_X_PADDING 2

#define DEFAULT_ITEM_HEIGHT 20
#define NO_TITLE_TITLE L"<No title>"

/* An item of the window list.
 *
 * WINDOW is the window this item deals with.
 * TITLE is the item’s window’s title.
 * ICON is the item’s window’s icon.
 * SIZE is the size of the item.
 * SHOWN determines whether this item is currently being displayed. */
struct _WindowListItem
{
        HWND window;
        LPTSTR title;
        Bitmap *icon;
        SizeF size;
        BOOL shown;
};


/* Creates a new window-list item for WINDOW, which is owned by OWNER. */
WindowListItem *
WindowListItemNew(HWND window, HWND owner)
{
        WindowListItem *item = ALLOC_STRUCT(WindowListItem);
        if (item == NULL)
                return NULL;

        item->window = window;
        if (!GetWindowTitle(item->window, &item->title) &&
            !GetWindowTitle(owner, &item->title))
                item->title = NO_TITLE_TITLE;
        WindowIconNew(owner, &item->icon);
        item->size.Width = item->size.Height = INVALID_CXY;
        item->shown = TRUE;

        return item;
}

/* Frees a window-list item. */
void 
WindowListItemFree(WindowListItem *item)
{
        if (item->title != NO_TITLE_TITLE)
                FREE(item->title);
        FREE(item);
}

/* Determines whether ITEM is currently being shown in the window list. */
BOOL 
WindowListItemShown(WindowListItem const *item)
{
        return item->shown;
}

/* Switches to the given ITEM’s window. */
BOOL 
WindowListItemSwitchTo(WindowListItem const *item)
{
        return MySwitchToThisWindow(item->window);
}

static BOOL
IsFlexibleMatch(LPCTSTR string, LPCTSTR chars)
{
        LPCTSTR string_p = string, chars_p = chars;
        while (*string_p != L'\0' && *chars_p != L'\0') {
                if (CompareString(LOCALE_USER_DEFAULT,
                                  IsCharUpper(*chars_p) ? 0 : NORM_IGNORECASE,
                                  string_p, 1, chars_p, 1) == CSTR_EQUAL)
                        chars_p = CharNext(chars_p);
                string_p = CharNext(string_p);
        }

        return *chars_p == L'\0';
}

static BOOL
IsSubMatch(LPCTSTR string, LPCTSTR chars)
{
        LPCTSTR string_p = string;

        while (*string_p != L'\0') {
                LPCTSTR test_p = string_p, chars_p = chars;

                while (*test_p != L'\0' && *chars_p != L'\0') {
                        if (CompareString(LOCALE_USER_DEFAULT,
                                          IsCharUpper(*chars_p) ? 0 : NORM_IGNORECASE,
                                          test_p, 1, chars_p, 1) != CSTR_EQUAL)
                                break;

                        test_p = CharNext(test_p);
                        chars_p = CharNext(chars_p);
                }

                if (*chars_p == L'\0')
                        return TRUE;

                string_p = CharNext(string_p);
        }

        return *chars == L'\0';
}

/* Updates whether ITEM should be displayed, given PREFIX as a filter. */
IterationState 
WindowListItemFilter(WindowListItem *item, LPCTSTR prefix)
{
        item->shown = IsSubMatch(item->title, prefix);

        return IterationContinue;
}

/* Validates ITEM’s size on CANVAS. */
static Status 
WindowListItemValidateSize(WindowListItem *item, Canvas const *canvas)
{
        if (item->size.Width != INVALID_CXY && item->size.Height != INVALID_CXY)
                return Ok;

        RectF title_area;
        RETURN_GDI_FAILURE(canvas->graphics->MeasureString(item->title, -1,
                                                           canvas->font,
                                                           PointF(0.0f, 0.0f),
                                                           &title_area));

        /* TODO: Why don’t we just use title_area.Height? */
        REAL font_height = canvas->font->GetHeight(canvas->graphics);
        RETURN_GDI_FAILURE(canvas->font->GetLastStatus());

        REAL default_height = (REAL)(GetSystemMetricsDefault(SM_CYICON, DEFAULT_ICON_DELTA));
        default_height = max(default_height, font_height);
        default_height += ICON_X_PADDING * 2;

        UINT icon_width = item->icon->GetWidth();
        RETURN_GDI_FAILURE(item->icon->GetLastStatus());

        item->size.Width = icon_width + ICON_X_PADDING + title_area.Width;
        item->size.Height = (REAL)GetSystemMetricsDefault(SM_CYCAPTION, (INT)default_height);

        return Ok;
}

/* Gets the SizeF of ITEM on CANVAS. */
Status 
WindowListItemSize(WindowListItem *item, Canvas const *canvas, SizeF *size)
{
        RETURN_GDI_FAILURE(WindowListItemValidateSize(item, canvas));

        *size = item->size;

        return Ok;
}

/* Gets the y-axis PADDING of ITEM’s icon on CANVAS. */
static Status 
WindowListItemIconYPadding(WindowListItem *item, Canvas const *canvas, REAL *padding)
{
        RETURN_GDI_FAILURE(WindowListItemValidateSize(item, canvas));

        UINT icon_height = item->icon->GetHeight();
        RETURN_GDI_FAILURE(item->icon->GetLastStatus());

        *padding = (item->size.Height - icon_height) / 2;

        return Ok;
}

/* Gets the y-axis PADDING of ITEM’s text on CANVAS. */
Status 
WindowListItemTextYPadding(WindowListItem *item, Canvas const *canvas, REAL *padding)
{
        RETURN_GDI_FAILURE(WindowListItemValidateSize(item, canvas));

        REAL icon_y_padding;
        RETURN_GDI_FAILURE(WindowListItemIconYPadding(item, canvas, &icon_y_padding));

        REAL font_height = canvas->font->GetHeight(canvas->graphics);
        RETURN_GDI_FAILURE(canvas->font->GetLastStatus());

        *padding = (item->size.Height - icon_y_padding - font_height) / 2;

        return Ok;
}

static Status 
ItemDrawIcon(WindowListItem *item, Canvas const *canvas, RectF const *area)
{
        REAL icon_y_padding;
        RETURN_GDI_FAILURE(WindowListItemIconYPadding(item, canvas, &icon_y_padding));

        Point icon_origo((INT)area->X, (INT)area->Y + (INT)icon_y_padding);

        return canvas->graphics->DrawImage(item->icon, icon_origo);
}

static Status 
ItemDrawString(WindowListItem *item, Canvas const *canvas, RectF const *area)
{
        UINT dx_icon = item->icon->GetWidth() + ICON_X_PADDING;
        RETURN_GDI_FAILURE(item->icon->GetLastStatus());

        REAL text_y_padding;
        RETURN_GDI_FAILURE(WindowListItemTextYPadding(item, canvas, &text_y_padding));
        RectF title_area(area->X + dx_icon, area->Y + text_y_padding,
                         area->Width - dx_icon, area->Height - text_y_padding);

        StringFormat format(StringFormatFlagsNoWrap);
        RETURN_GDI_FAILURE(format.GetLastStatus());
        RETURN_GDI_FAILURE(format.SetTrimming(StringTrimmingEllipsisCharacter));

        SolidBrush white_brush(Color::White);
        RETURN_GDI_FAILURE(white_brush.GetLastStatus());

        return canvas->graphics->DrawString(item->title, -1, canvas->font,
                                            title_area, &format, &white_brush);
}

/* Draws ITEM on CANVAS inside AREA. */
Status 
WindowListItemDraw(WindowListItem *item, Canvas const *canvas, RectF const *area)
{
        RETURN_GDI_FAILURE(WindowListItemValidateSize(item, canvas));

        RETURN_GDI_FAILURE(ItemDrawIcon(item, canvas, area));

        return ItemDrawString(item, canvas, area);
}

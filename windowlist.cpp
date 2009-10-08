#include "stdafx.h"
#include "list.h"
#include "windowlistitem.h"
#include "windowlist.h"
#include "resource.h"

/* A LIST of windows to display using FONT, numbering items inside an
 * area of width NUMBER_WIDTH. */
struct _WindowList
{
        List *list;
        Font *font;
        REAL number_width;
};

/* Numbers drawn for the first ten items in the list for fast access using
 * number keys. */
static LPCTSTR s_numbers[] = { L"1. ", L"2. ", L"3. ", L"4. ",
        L"5. ", L"6. ", L"7. ", L"8. ", L"9. ", L"0. " };
#define N_NUMBERS    _countof(s_numbers)

#define STRING_EMPTY_WARNING _(IDS_WINDOW_LIST_EMPTY_WARNING, L"No windows to switch to!")
#define STRING_EMPTY_SHOWN_WARNING _(IDS_WINDOW_LIST_EMPTY_SHOWN_WARNING, L"No window titles match the given input!")

/* A window-list item that has yet to be added to the window list.
 *
 * This is needed, as not all top-level windows should be kept in
 * the window list, rather their parents are enough.  Also, this
 * is a good place to get the appropriate WINDOW’s icon, i.e., that
 * of the owner of the window.
 *
 * ITEM is the window-list item we’ve semi-added.
 * OWNER is the owner of the window referenced by ITEM.
 * KEEP determines whether this window-list item is to be kept in
 * the window list. */
typedef struct _WindowListSemiAddedItem WindowListSemiAddedItem;

struct _WindowListSemiAddedItem
{
        HWND window;
        HWND owner;
        BOOL keep;
};

/* Creates a new WindowListSemiAddedItem for WINDOW, being owned by
 * OWNER, kept if KEEP is TRUE. */
static WindowListSemiAddedItem *
WindowListSemiAddedItemNew(HWND window, HWND owner, BOOL keep)
{
        WindowListSemiAddedItem *semi_item = ALLOC_STRUCT(WindowListSemiAddedItem);
        if (semi_item == NULL)
                return NULL;

        semi_item->window = window;
        semi_item->owner = owner;
        semi_item->keep = keep;

        return semi_item;
}

/* Frees a WindowListeSemiAddedItem. */
void 
WindowListSemiAddedItemFree(WindowListSemiAddedItem *semi_item)
{
        FREE(semi_item);
}

/* Gets the top-most window that owns WINDOW. */
static HWND 
GetTopmostOwner(HWND window)
{
        HWND shell = GetShellWindow();
        HWND owner = window;

        while (TRUE) {
                HWND owner_owner = GetWindow(owner, GW_OWNER);

                if (owner_owner == NULL || owner_owner == shell)
                        break;

                owner = owner_owner;
        }

        return owner;
}

/* A closure for determining if the window list already contains a window
 * owned by the same window as another one.
 *
 * OWNER is the window */
typedef struct _WindowListFindSameOwnerClosure WindowListFindSameOwnerClosure;

struct _WindowListFindSameOwnerClosure
{
        HWND owner;
        WindowListSemiAddedItem *item;
};

static IterationState 
WindowListFindSameOwnerIterator(void *list_item, void *v_closure)
{
        WindowListSemiAddedItem *item = (WindowListSemiAddedItem *)list_item;
        WindowListFindSameOwnerClosure *closure = (WindowListFindSameOwnerClosure *)v_closure;

        if (item->owner != closure->owner)
                return IterationContinue;

        closure->item = item;

        return IterationStop;
}

static BOOL 
IsAppWindow(HWND window)
{
        return GetWindowLongPtr(window, GWL_EXSTYLE) & WS_EX_APPWINDOW;
}

static BOOL 
IsControlParent(HWND window)
{
        return GetWindowLongPtr(window, GWL_EXSTYLE) & WS_EX_CONTROLPARENT;
}

static BOOL 
HasSameOwnerAsAnotherWindow(List *windows, HWND window, HWND owner)
{
        WindowListFindSameOwnerClosure closure = { owner, NULL };
        ListItemsIterate(windows, WindowListFindSameOwnerIterator, &closure);

        if (closure.item == NULL)
                return FALSE;

        if (!IsToolWindow(window)) {
                /* If this window isn’t a tool window and we actually own the window that’s already in the list and that window isn’t already kept, replace
		 * it with this window, given that it would pass all the tests.  (The algorithm previously assumed that an appwindow would be in the list
		 * before any of its tool windows, but that’s not always the case. An example is Adobe Illustrator. */
                if (closure.item->owner == window && !(IsToolWindow(owner) && !IsAppWindow(window) && (IsToolWindow(window) || !IsControlParent(window)))) {
                        closure.item->window = window;
                        closure.item->owner = owner;
                }
                if (!IsToolWindow(closure.item->window))
                        closure.item->keep = TRUE;
        }

        return TRUE;
}

static BOOL CALLBACK 
WindowListConsProc(HWND window, LPARAM lParam)
{
        if (!IsWindowVisible(window))
                return TRUE;

        List **list = (List **)lParam;

        HWND owner = GetTopmostOwner(window);
        HWND saved_owner = owner;

        /* IsAppWindow windows should appear in the window list even though they are owned. */
        if (window != owner && IsAppWindow(window))
                owner = window;
        else if (HasSameOwnerAsAnotherWindow(*list, window, owner))
                return TRUE;

        if (IsToolWindow(saved_owner) && !IsAppWindow(window) && (IsToolWindow(window) || !IsControlParent(window)))
                return TRUE;

        BOOL keep = IsToolWindow(saved_owner) || !IsToolWindow(window);
        WindowListSemiAddedItem *semi_item = WindowListSemiAddedItemNew(window, owner, keep);
        if (semi_item == NULL)
                return TRUE;

        if (!ListCons(list, semi_item))
                WindowListSemiAddedItemFree(semi_item);

        return TRUE;
}

/* Converts a single WindowListSemiAddedItem to a WindowListItem and
 * adds it to the WindowList.  Only items we want to KEEP are added to
 * the list. */
static IterationState 
SemiAddedWindowListToWindowListIterator(void *list_item, void *closure)
{
        WindowListSemiAddedItem *semi_item = (WindowListSemiAddedItem *)list_item;
        if (!semi_item->keep)
                return IterationContinue;

        WindowListItem *item = WindowListItemNew(semi_item->window, semi_item->owner);
        if (item == NULL)
                return IterationContinue;

        if (!ListCons((List **)closure, item))
                WindowListItemFree(item);

        return IterationContinue;
}

/* Converts a list of WindowListSemiAddedItems to a list of WindowListItems. */
static List *
SemiAddedWindowListToWindowList(List *list)
{
        List *window_list = ListNew();
        ListItemsIterate(list, SemiAddedWindowListToWindowListIterator, &window_list);
        return window_list;
}

/* Creates a new WindowList, using FONT for drawing. */
WindowList *
WindowListNew(Font *font)
{
        WindowList *list = ALLOC_STRUCT(WindowList);

        List *semi_added_list = ListNew();
        EnumDesktopWindows(NULL, WindowListConsProc, (LPARAM)&semi_added_list);
        list->list = SemiAddedWindowListToWindowList(semi_added_list);
        ListFree(semi_added_list, (FreeFunc)WindowListSemiAddedItemFree);

        list->font = font;
        list->number_width = -1;

        return list;
}

/* Frees a WindowList LIST. */
void 
WindowListFree(WindowList *list)
{
        ListFree(list->list, (FreeFunc)WindowListItemFree);
        FREE(list);
}

/* Iterates over the WindowList LIST, calling ITERATOR with CLOSURE. */
static void 
WindowListIterate(WindowList *list, WindowListIterator iterator, void *closure)
{
        ListItemsIterate(list->list, (ListItemsIterator)iterator, closure);
}

/* Determines the length of the WindowList LIST. */
int 
WindowListLength(WindowList *list)
{
        return ListLength(list->list);
}

/* Iterator calculating the number of shown WindowListItems in a WindowList. */
static IterationState 
WindowListLengthShownIterator(WindowListItem *item, void *closure)
{
        if (WindowListItemShown(item))
                (*(int *)closure)++;

        return IterationContinue;
}

/* Determines the number of WindowListItems shown in the WindowList list. */
int 
WindowListLengthShown(WindowList *list)
{
        int n = 0;

        WindowListIterate(list, WindowListLengthShownIterator, &n);

        return n;
}

static LPCTSTR
WindowListEmpty(WindowList *list)
{
        if (WindowListLength(list) == 0)
                return STRING_EMPTY_WARNING;

        if (WindowListLengthShown(list) == 0)
                return STRING_EMPTY_SHOWN_WARNING;

        return NULL;
}

/* Closure used when calculating the Size of a WindowList.
 *
 * CANVAS is the Canvas drawing will be done on.
 * SIZE is where we store the calculated Size.
 * STATUS is used to pass any failure Status we encounter. */
typedef struct _WindowListSizeClosure WindowListSizeClosure;

struct _WindowListSizeClosure
{
        Canvas canvas;
        SizeF *size;
        Status status;
};

/* Iterator calculating the size of a WindowList. */
static IterationState
WindowListSizeIterator(WindowListItem *item, void *v_closure)
{
        if (!WindowListItemShown(item))
                return IterationContinue;

        WindowListSizeClosure *closure = (WindowListSizeClosure *)v_closure;
        SizeF size;

        closure->status = WindowListItemSize(item, &closure->canvas, &size);
        if (closure->status != Ok)
                return IterationStop;

        closure->size->Width = max(closure->size->Width, size.Width);
        closure->size->Height += size.Height;

        return IterationContinue;
}

/* Measures the WIDTH of NUMBER on GRAPHICS, using FONT. */ 
static Status 
MeasureNumber(LPCTSTR number, Graphics *graphics, Font const *font, REAL *width)
{
        RectF number_area;
        StringFormat format(StringFormatFlagsMeasureTrailingSpaces);
        RETURN_GDI_FAILURE(format.GetLastStatus());
        RETURN_GDI_FAILURE(graphics->MeasureString(number, -1, font, PointF(),
                                                   &format, &number_area));

        *width = number_area.Width;

        return Ok;
}

/* Calculates the SIZE of LIST on GRAPHICS. */
Status 
WindowListSize(WindowList *list, Graphics *graphics, SizeF *size)
{
        size->Width = -1.0f;
        size->Height = 0.0f;

        LPCTSTR message = WindowListEmpty(list);
        if (message) {
                RectF message_area;
                RETURN_GDI_FAILURE(graphics->MeasureString(message, -1,
                                                           list->font,
                                                           PointF(0.0f, 0.0f),
                                                           &message_area));
                size->Width = message_area.Width;
                size->Height = message_area.Height;
                return Ok;
        }

        WindowListSizeClosure closure = { { graphics, list->font}, size, Ok };
        WindowListIterate(list, WindowListSizeIterator, &closure);
        RETURN_GDI_FAILURE(closure.status);

        list->number_width = -1.0f;
        for (int i = 0; i < N_NUMBERS; i++) {
                REAL number_width;
                RETURN_GDI_FAILURE(MeasureNumber(s_numbers[i], graphics, list->font, &number_width));
                list->number_width = max(list->number_width, number_width);
        }

        size->Width += list->number_width;

        /* Also need to measure the “there are no windows matching…” message,
         * as it may be displayed at any time. */
        RectF message_area;
        RETURN_GDI_FAILURE(graphics->MeasureString(STRING_EMPTY_SHOWN_WARNING,
                                                   -1,
                                                   list->font,
                                                   PointF(0.0f, 0.0f),
                                                   &message_area));
        size->Width = max(size->Width, message_area.Width);

        return Ok;
}

/* Filters the shown items of LIST based on PREFIX. */ 
void 
WindowListFilter(WindowList *list, LPCTSTR prefix)
{
        WindowListIterate(list, (WindowListIterator)WindowListItemFilter, (void *)prefix);
}

/* Sets the FONT used to draw LIST. */
void 
WindowListSetFont(WindowList *list, Font *font)
{
        list->font = font;
}

/* Draws the number associated with ROW in the window list inside AREA on CANVAS. */
static Status 
WindowListDrawNumber(WindowListItem *item, Canvas *canvas, int row, RectF const *area)
{
        if (row >= N_NUMBERS)
                return Ok;

        REAL text_y_padding;
        RETURN_GDI_FAILURE(WindowListItemTextYPadding(item, canvas, &text_y_padding));
        RectF number_area(area->X, area->Y + text_y_padding,
                          area->Width, area->Height - text_y_padding); 

        StringFormat format(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
        RETURN_GDI_FAILURE(format.GetLastStatus());
        RETURN_GDI_FAILURE(format.SetAlignment(StringAlignmentFar));

        SolidBrush white_brush(Color::White);
        RETURN_GDI_FAILURE(white_brush.GetLastStatus());

        return canvas->graphics->DrawString(s_numbers[row], -1, canvas->font,
                                            number_area, &format, &white_brush);
}

/* Closure used when drawing a WindowList.
 *
 * CANVAS is the Canvas to draw upon.
 * NUMBER_AREA is the area to draw numbers on.
 * ITEM_AREA is the area to draw items on.
 * ROW is the number of the current row being drawn.
 * STATUS contains any failure Status returned while drawing. */
typedef struct _WindowListDrawClosure WindowListDrawClosure;

struct _WindowListDrawClosure
{
        Canvas canvas;
        RectF *number_area;
        RectF *item_area;
        int row;
        Status status;
};

/* Draws a single row of a WindowList for ITEM, drawing numbers if appropriate. */
static IterationState 
WindowListDrawIterator(WindowListItem *item, void *v_closure)
{
        if (!WindowListItemShown(item))
                return IterationContinue;

        WindowListDrawClosure *closure = (WindowListDrawClosure *)v_closure;

        closure->status = WindowListDrawNumber(item, &closure->canvas,
                                               closure->row, closure->number_area);
        if (closure->status != Ok)
                return IterationStop;

        closure->status = WindowListItemDraw(item, &closure->canvas, closure->item_area);
        if (closure->status != Ok)
                return IterationStop;

        SizeF size;
        closure->status = WindowListItemSize(item, &closure->canvas, &size);
        if (closure->status != Ok)
                return IterationStop;

        closure->number_area->Y += size.Height;
        closure->item_area->Y += size.Height;
        closure->row++;

        return IterationContinue;
}

/* Draws the text displayed when the WindowList is empty. */
Status 
WindowListDrawEmpty(WindowList *list, Graphics *graphics, RectF const *area, LPCTSTR message)
{
        REAL font_height = list->font->GetHeight(graphics);
        RETURN_GDI_FAILURE(list->font->GetLastStatus());
        REAL quarter_area_height = area->Height / 2 - font_height / 2;
        RectF y_centered_area(area->X, area->Y + quarter_area_height,
                              area->Width, area->Height - quarter_area_height);

        StringFormat format;
        RETURN_GDI_FAILURE(format.GetLastStatus());
        RETURN_GDI_FAILURE(format.SetAlignment(StringAlignmentCenter));

        /* A bug in GDI+ makes brushes for text come out white when drawn on a
         * non-opaque background when using TextRenderingHintAntiAliasGridFit. */
        SolidBrush red_brush(Color(255, 225, 50, 50));
        RETURN_GDI_FAILURE(red_brush.GetLastStatus());

        return graphics->DrawString(message, -1,
                                    list->font, y_centered_area, &format,
                                    &red_brush);
}

/* Draws LIST on GRAPHICS inside AREA. */
Status 
WindowListDraw(WindowList *list, Graphics *graphics, RectF const *area)
{
        LPCTSTR message = WindowListEmpty(list);
        if (message)
                return WindowListDrawEmpty(list, graphics, area, message);

        RectF number_area(area->X, area->Y, list->number_width, area->Height);
        RectF item_area(area->X + list->number_width, area->Y,
                        area->Width - list->number_width, area->Height);
        WindowListDrawClosure closure = { { graphics, list->font }, &number_area, &item_area, 0, Ok };

        WindowListIterate(list, WindowListDrawIterator, &closure);

        return closure.status;
}

/* Closure used when finding the Nth shown WindowListItem.
 *
 * N_LEFT is the number of items that remain to find before the Nth has been found.
 * ITEM is used to return the found WindowListItem once found. */
typedef struct _WindowListNthShownClosure WindowListNthShownClosure;

struct _WindowListNthShownClosure
{
        int n_left;
        WindowListItem *item;
};

/* Iterator used for finding the Nth shown WindowListItem. */
static IterationState
WindowListNthShownIterator(WindowListItem *item, void *v_closure)
{
        if (!WindowListItemShown(item))
                return IterationContinue;

        WindowListNthShownClosure *closure = (WindowListNthShownClosure *)v_closure;
        closure->n_left--;
        if (closure->n_left > 0)
                return IterationContinue;

        closure->item = item;
        return IterationStop;
}

/* Gets the Nth shown WindowListItem in LIST. */
WindowListItem *
WindowListNthShown(WindowList *list, int n)
{
        WindowListNthShownClosure closure = { n, NULL };

        WindowListIterate(list, WindowListNthShownIterator, &closure);

        return closure.item;
}

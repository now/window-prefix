﻿typedef struct _WindowListItem WindowListItem;

WindowListItem *WindowListItemNew(HWND window, HWND owner);
void WindowListItemFree(WindowListItem *item);
Status WindowListItemSize(WindowListItem *item, Canvas const *canvas, SizeF *size);
BOOL WindowListItemShown(WindowListItem const *item);
BOOL WindowListItemSwitchTo(WindowListItem const *item);
IterationState WindowListItemFilter(WindowListItem *item, LPCTSTR prefix);
Status WindowListItemTextYPadding(WindowListItem *item, Canvas const *canvas, REAL *padding);
Status WindowListItemDraw(WindowListItem *item, Canvas const *canvas, RectF const *rc);

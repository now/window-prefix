﻿typedef struct _WindowList WindowList;

typedef IterationState (*WindowListIterator)(WindowListItem *, void *);

WindowList *WindowListNew(Font *font);
void WindowListFree(WindowList *list);
int WindowListLength(WindowList *list);
int WindowListLengthShown(WindowList *list);
Status WindowListSize(WindowList *list, Graphics *g, SizeF *size);
void WindowListFilter(WindowList *list, LPCTSTR prefix);
void WindowListSetFont(WindowList *list, Font *font);
WindowListItem *WindowListNthShown(WindowList *list, int n);
Status WindowListDraw(WindowList *list, Graphics *g, RectF const *rc);

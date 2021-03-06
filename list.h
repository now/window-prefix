﻿typedef struct _List List;

struct _List
{
        void *item;
        List *next;
};

typedef IterationState (*ListIterator)(List *, void *);
typedef IterationState (*ListItemsIterator)(void *, void *);

List *ListNew(void);
BOOL ListCons(List **list, void *item);
List *ListAppend(List *list, void *item);
List *ListRemove(List *list, void *item, EqualityFunc equal, FreeFunc f);
int ListLength(List *list);
List *ListReverse(List *list);
void ListIterate(List *list, ListIterator f, void *closure);
void ListItemsIterate(List *list, ListItemsIterator f, void *closure);
void ListFree(List *list, FreeFunc f);
void *ListFindItem(List *list, EqualityFunc equal, void *other);
List *ListRemoveNode(List *list, List *node, List *previous, FreeFunc f);
List *ListRemoveIf(List *list, PredicateFunc remove, FreeFunc f);

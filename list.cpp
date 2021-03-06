﻿#include "stdafx.h"

#include "list.h"

static List *
ListNewItem(void *item)
{
        List *l = ALLOC_STRUCT(List);
        l->item = item;
        l->next = NULL;
        return l;
}

void 
ListIterate(List *list, ListIterator f, void *closure)
{
        List *iter = list;
        while (iter != NULL) {
                List *next = iter->next;
                if (f(iter, closure) == IterationStop)
                        break;
                iter = next;
        }
}

typedef struct _ListItemsIteratorClosure ListItemsIteratorClosure;

struct _ListItemsIteratorClosure
{
        ListItemsIterator f;
        void *closure;
};

static IterationState
ListItemsIteratorFunc(List *list, void *closure)
{
        ListItemsIteratorClosure *iter_closure = (ListItemsIteratorClosure *)closure;
        return iter_closure->f(list->item, iter_closure->closure);
}

void 
ListItemsIterate(List *list, ListItemsIterator f, void *closure)
{
        ListItemsIteratorClosure iter_closure = { f, closure };
        ListIterate(list, ListItemsIteratorFunc, &iter_closure);
}

List *
ListNew(void)
{
        return (List *)NULL;
}

BOOL 
ListCons(List **list, void *item)
{
        List *l = ListNewItem(item);
        if (l == NULL)
                return FALSE;

        l->next = *list;
        *list = l;

        return TRUE;
}

List *
ListAppend(List *list, void *item)
{
        List *l = ListNewItem(item);
        if (l == NULL)
                return NULL;

        if (list == NULL)
                return l;

        List *iter = list;
        while (iter->next != NULL)
                iter = iter->next;

        iter->next = l;

        return list;
}

static void 
ListFreeNode(List *node, FreeFunc free_item)
{
        free_item(node->item);
        FREE(node);
}

List *
ListRemoveNode(List *list, List *node, List *previous, FreeFunc free_item)
{
        List *next = node->next;

        ListFreeNode(node, free_item);

        if (previous == NULL)
                return next;

        previous->next = next;

        return list;
}

List *
ListRemove(List *list, void *item, EqualityFunc equal, FreeFunc free_item)
{
        List *previous = NULL;

        for (List *iter = list; iter != NULL; iter = iter->next) {
                if (equal(iter->item, item))
                        return ListRemoveNode(list, iter, previous, free_item);

                previous = iter;
        }

        return list;
}

List *
ListRemoveIf(List *list, PredicateFunc remove, FreeFunc free_item)
{
        List *previous = NULL;
        List *iter = list;

        while (iter != NULL) {
                List *next = iter->next;

                if (remove(iter->item))
                        list = ListRemoveNode(list, iter, previous, free_item);
                else
                        previous = iter;

                iter = next;
        }

        return list;
}

static IterationState 
ListLengthIterator(List *node, void *closure)
{
        UNREFERENCED_PARAMETER(node);

        (*(int *)closure)++;

        return IterationContinue;
}

int 
ListLength(List *list)
{
        int n = 0;
        ListIterate(list, ListLengthIterator, &n);
        return n;
}

static IterationState 
ListReverseIterator(List *node, void *closure)
{
        List **preceding = (List **)closure;
        node->next = *preceding;
        *preceding = node;

        return IterationContinue;
}

List *
ListReverse(List *list)
{
        List *preceding = NULL;
        ListIterate(list, ListReverseIterator, &preceding);

        return preceding;
}

static IterationState 
ListFreeIterator(List *node, void *closure)
{
        ListFreeNode(node, (FreeFunc)closure);

        return IterationContinue;
}

void 
ListFree(List *list, FreeFunc f)
{
        ListIterate(list, ListFreeIterator, f);
}

typedef struct _ListFindItemIteratorClosure ListFindItemIteratorClosure;

struct _ListFindItemIteratorClosure
{
        EqualityFunc equal;
        void *other;
        void *item;
};

static IterationState 
ListFindIterator(void *item, void *closure)
{
        ListFindItemIteratorClosure *iter_closure = (ListFindItemIteratorClosure *)closure;

        if (!iter_closure->equal(item, iter_closure->other))
                return IterationContinue;

        iter_closure->item = item;

        return IterationStop;
}

void *
ListFindItem(List *list, EqualityFunc equal, void *other)
{
        ListFindItemIteratorClosure closure = { equal, other, NULL };

        ListItemsIterate(list, ListFindIterator, &closure);

        return closure.item;
}

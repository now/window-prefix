﻿#include "stdafx.h"

#include "buffer.h"
#include "list.h"

/* A Buffer contains the text being entered into a TextField.
 * A Buffer can be monitored for changes so that users will know
 * when to update themselves as a response.

/* Default size of a Buffer. */
#define DEFAULT_BUFFER_SIZE 16

/* A Buffer for text.
 *
 * CONTENTS stores the text.
 * ALLOCATED is the number of TCHARs allocated in CONTENTS.
 * OFFSET is the number of TCHARs used in CONTENTS.
 * LISTENERS is a list of BufferListeners for event callbacks. */
struct _Buffer
{
        LPTSTR contents;
        size_t allocated;
        size_t offset;
        List *listeners;
};

/* A BufferListener represents a listener of BufferEvents.
 *
 * EVENT is the events that this BufferListener is interested in.
 * CALLBACK is a callback function called when EVENT is produced.
 * CLOSURE is a closure sent to CALLBACK whenever it is invoked. */
struct _BufferListener
{
        BufferEvent event;
        BufferListenerCallback callback;
        VOID *closure;
};

static BufferListener *
BufferListenerNew(BufferEvent event, BufferListenerCallback callback, VOID *closure)
{
        BufferListener *listener = ALLOC_STRUCT(BufferListener);
        if (listener == NULL)
                return NULL;

        listener->event = event;
        listener->callback = callback;
        listener->closure = closure;

        return listener;
}

static void
BufferListenerFree(BufferListener *listener)
{
        FREE(listener);
}

BOOL
BufferRegisterListener(Buffer *buffer, BufferEvent event, BufferListenerCallback callback, VOID *closure)
{
        BufferListener *listener = BufferListenerNew(event, callback, closure);
        if (listener == NULL)
                return FALSE;

        if (ListCons(&buffer->listeners, listener))
                return TRUE;

        BufferListenerFree(listener);

        return FALSE;
}

typedef struct _BufferListenerEqualityInfo BufferListenerEqualityInfo;

struct _BufferListenerEqualityInfo
{
        BufferEvent event;
        BufferListenerCallback callback;
};

static BOOL BufferListenerEquality(BufferListener *listener, BufferListenerEqualityInfo *info)
{
        if (listener->callback != info->callback)
                return FALSE;

        UINT events = listener->event;
        events &= ~info->event;
        listener->event = (BufferEvent)events;
        if (listener->event != 0)
                return FALSE;

        return TRUE;

}

void
BufferUnregisterListener(Buffer *buffer, BufferEvent event, BufferListenerCallback callback)
{
        BufferListenerEqualityInfo info = { event, callback };
        ListRemove(buffer->listeners, &info, (EqualityFunc)BufferListenerEquality, (FreeFunc)BufferListenerFree);
}

typedef struct _BufferSendEventClosure BufferSendEventClosure;

struct _BufferSendEventClosure
{
        Buffer *buffer;
        BufferEvent event;
};

static IterationState
BufferSendEventIterator(BufferListener *listener, BufferSendEventClosure *closure)
{
        if (listener->event & closure->event)
                listener->callback(closure->buffer, closure->event, listener->closure);

        return IterationContinue;
}

static void
BufferSendEvent(Buffer *buffer, BufferEvent event)
{
        BufferSendEventClosure closure = { buffer, event };
        ListItemsIterate(buffer->listeners, (ListItemsIterator)BufferSendEventIterator, &closure);
}

static BOOL
BufferResize(Buffer *buffer, size_t new_size)
{
        LPTSTR new_contents = REALLOC_N(TCHAR, buffer->contents, new_size);
        if (new_contents == NULL)
                return FALSE;

        buffer->contents = new_contents;
        buffer->allocated = new_size;
        return TRUE;
}

Buffer *
BufferNew()
{
        Buffer *buffer = ALLOC_STRUCT(Buffer);
        if (buffer == NULL)
                return NULL;

        if (!BufferResize(buffer, DEFAULT_BUFFER_SIZE)) {
                BufferFree(buffer);
                return NULL;
        }

        buffer->listeners = ListNew();

        BufferReset(buffer);

        return buffer;
}

void
BufferFree(Buffer *buffer)
{
        if (buffer->contents != NULL)
                FREE(buffer->contents);
        ListFree(buffer->listeners, (FreeFunc)BufferListenerFree);
        FREE(buffer);
}

static void
BufferSetOffsetSilently(Buffer *buffer, size_t new_offset)
{
        buffer->offset = max(new_offset, 0);
        buffer->contents[buffer->offset] = L'\0';
}

static void
BufferSetOffset(Buffer *buffer, size_t new_offset)
{
        BufferSetOffsetSilently(buffer, new_offset);
        BufferSendEvent(buffer, BUFFER_ON_CHANGE);
}

UINT
BufferLength(Buffer const *buffer)
{
        return (UINT)buffer->offset;
}

void
BufferReset(Buffer *buffer)
{
        BufferSetOffsetSilently(buffer, 0);
}

void
BufferClear(Buffer *buffer)
{
        BufferSetOffset(buffer, 0);
}

/* Assert that BUFFER is big enough to contain N_ADDITIONAL characters.
 * If the required number of characters is less than half of the
 * number allocated, shrink the buffer to half (with a minimum of
 * DEFAULT_BUFFER_SIZE).  If the requried number of characters is more
 * then try doubling the buffer size, checking for size_t overflows. */
static BOOL
BufferAssertBigEnough(Buffer *buffer, size_t n_additional)
{
        size_t required = buffer->offset + n_additional;

        size_t half_allocated = buffer->allocated / 2;
        if (required < half_allocated && half_allocated > DEFAULT_BUFFER_SIZE)
                return BufferResize(buffer, half_allocated);

        if (required < buffer->allocated - 1)
                return TRUE;

        size_t double_allocated = buffer->allocated * 2;
        if (double_allocated < buffer->allocated)
                return FALSE;

        return BufferResize(buffer, double_allocated);
}

BOOL
BufferPushChar(Buffer *buffer, TCHAR c, UINT n)
{
        if (!BufferAssertBigEnough(buffer, (size_t)n))
                return FALSE;

        for (UINT i = 0; i < n; i++)
                buffer->contents[buffer->offset + i] = c;

        BufferSetOffset(buffer, buffer->offset + n);

        return TRUE;
}

BOOL
BufferPopChar(Buffer *buffer, UINT n)
{
        if (buffer->offset == 0)
                return FALSE;

        BufferSetOffset(buffer, buffer->offset - n);

        return TRUE;
}

LPCTSTR
BufferContents(Buffer const *buffer)
{
        return buffer->contents;
}

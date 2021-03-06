﻿typedef struct _Buffer Buffer;
typedef struct _BufferListener BufferListener;

/* Events sent by a Buffer.
 *
 * BUFFER_ON_CHANGE is sent whenever the contents of the buffer changes. */
typedef enum {
        BUFFER_ON_CHANGE = 1 << 0,
} BufferEvent;

/* A callback for Buffer events. */
typedef void (*BufferListenerCallback)(Buffer *, BufferEvent, VOID *closure);

Buffer *BufferNew();
void BufferFree(Buffer *buffer);
void BufferReset(Buffer *buffer);
void BufferClear(Buffer *buffer);
UINT BufferLength(Buffer const *buffer);
BOOL BufferPopChar(Buffer *buffer, UINT n);
BOOL BufferPushChar(Buffer *buffer, TCHAR c, UINT n);
LPCTSTR BufferContents(Buffer const *buffer);
BOOL BufferRegisterListener(Buffer *buffer, BufferEvent event, BufferListenerCallback callback, VOID *closure);
void BufferUnregisterListener(Buffer *buffer, BufferEvent event, BufferListenerCallback callback);

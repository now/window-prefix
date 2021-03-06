﻿/* The default width and height of an icon. */
#define DEFAULT_ICON_DELTA          16

BOOL WindowIconInitialize(Error **error);
void WindowIconFinalize(VOID);
Status WindowIconNew(HWND window, Bitmap **icon);
void WindowIconFree(Bitmap *icon);
void WindowIconIconChanged(HWND window);

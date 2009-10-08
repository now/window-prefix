// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// TODO: move Makefile switches here instead (#pragma linker(...))
#ifndef UNICODE
#  define UNICODE
#endif // UNICODE

#ifndef _UNICODE
#  define _UNICODE
#endif // _UNICODE

#ifndef STRICT
#  define STRICT
#endif

//#define WIN32_LEAN_AND_MEAN           // Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x0501
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>

// C RunTime Header Files
#if 0
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#endif
#include <tchar.h>
#include <strsafe.h>
#include <gdiplus.h>

using namespace Gdiplus;

typedef struct _Canvas Canvas;

struct _Canvas
{
        Graphics *graphics;
        Font const *font;
};

// TODO: reference additional headers your program requires here
#include "error.h"
#include "windowicon.h"
#include "generic.h"
#include "translation.h"

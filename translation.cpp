﻿#include "stdafx.h"

#include "translation.h"

#define STRING_DEFAULT_LANGUAGE MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)

static inline UINT
CStringResourceLength(LPCWSTR string)
{
        return (UINT)*string;
}

static LPCWSTR 
FindCStringResourceEx(HINSTANCE instance, UINT id, UINT language)
{
        HRSRC resource = FindResourceEx(instance, RT_STRING,
                                        MAKEINTRESOURCE(id / 16 + 1), language);
        if (resource == NULL)
                return NULL;

        HGLOBAL global = LoadResource(instance, resource);
        if (global == NULL)
                return NULL;

        LPCWSTR string = (LPCWSTR)LockResource(global);
        if (string == NULL)
                return NULL;

        for (UINT i = 0; i < (id & 0xf); i++)
                string += CStringResourceLength(string) + 1;

        return string;
}

static LPCWSTR
FindCStringResource(HINSTANCE instance, UINT id)
{
        return FindCStringResourceEx(instance, id, STRING_DEFAULT_LANGUAGE);
}

static UINT 
GetStringResourceLengthEx(HINSTANCE instance, UINT id, UINT language)
{
        LPCWSTR string = FindCStringResourceEx(instance, id, language);

        return (string != NULL ? CStringResourceLength(string) : 0);
}

static UINT 
GetStringResourceLength(HINSTANCE instance, UINT id)
{
        return GetStringResourceLengthEx(instance, id, STRING_DEFAULT_LANGUAGE);
}

static LPWSTR 
AllocStringFromResourceEx(HINSTANCE instance, UINT id, UINT language)
{
        LPCWSTR resource_string = FindCStringResourceEx(instance, id, language);
        if (resource_string == NULL)
                return NULL;

        UINT length = CStringResourceLength(resource_string);

        LPWSTR string = new WCHAR[ZERO_TERMINATE(length)];
        if (string == NULL)
                return NULL;

        CopyMemory(string, resource_string + 1, length * sizeof(WCHAR));
        string[length] = L'\0';

        return string;
}

static LPWSTR 
AllocStringFromResource(HINSTANCE instance, UINT id)
{
        return AllocStringFromResourceEx(instance, id, STRING_DEFAULT_LANGUAGE);
}

/* Frees a string previously allocated with AllocStringFromResourceEx()
 * or AllocStringFromResourceEx(). */
static void 
FreeStringResource(LPWSTR string)
{
        delete string;
}

static LPCWSTR 
FindStringResourceEx(HINSTANCE instance, UINT id, UINT language)
{
        LPCWSTR string = FindCStringResourceEx(instance, id, language);
        if (string == NULL)
                return NULL;

        return string + 1;
}

static LPCWSTR 
FindStringResource(HINSTANCE instance, UINT id)
{
        return FindStringResourceEx(instance, id, STRING_DEFAULT_LANGUAGE);
}

static HINSTANCE s_instance;

void 
TranslationInitialize(HINSTANCE instance)
{
        s_instance = instance;
}

LPCWSTR 
Translate(UINT id, LPCWSTR neutral)
{
        LPCWSTR string = FindStringResourceEx(s_instance, id, LANGIDFROMLCID(GetThreadLocale()));
        return (string != NULL) ? string : neutral;
}

﻿#include "stdafx.h"

#include "error.h"

/* An error, currently only maintaining an error message. */
struct _Error
{
        LPWSTR message;
};

/* Fallback Error to return when we fail to return anything else. */
static Error s_fallback_error = {
        L"Failed to allocate memory for an error.\r\n"
        L"\r\n"
        L"This is the best I can do, sorry.  My guess is"
        L" that main memory is completely full.  I’m"
        L" surprised that you’re even seeing this message."
};

/* Determines the length of a formatted error-message. */
static int
ErrorMessageLength(LPCTSTR format, va_list args)
{
        return ZERO_TERMINATE(_vsctprintf(format, args));
}

/* Creates a formatted error-message. */
static LPWSTR
CreateErrorMessageV(LPCTSTR format, va_list args)
{
        va_list saved_args = args;
        int length = ErrorMessageLength(format, args);
        args = saved_args;

        LPWSTR message = ALLOC_N(WCHAR, length);
        if (message == NULL)
                return NULL;

        if (SUCCEEDED(StringCchVPrintf(message, length, format, args)))
                return message;

        FREE(message);

        return NULL;
}

/* Creates a formatted error-message. */
static LPWSTR
CreateErrorMessage(LPCTSTR format, ...)
{
        va_list args;
        va_start(args, format);
        LPWSTR string = CreateErrorMessageV(format, args);
        va_end(args);

        return string;
}

/* Creates a new Error. */
Error *
ErrorNewV(LPCTSTR format, va_list args)
{
        Error *error = ALLOC_STRUCT(Error);
        if (error == NULL)
                return &s_fallback_error;

        error->message = CreateErrorMessageV(format, args);
        if (error->message != NULL)
                return error;

        FREE(error);
        return &s_fallback_error;
}

/* Creates a new Error. */
Error *
ErrorNew(LPCTSTR format, ...)
{
        va_list args;
        va_start(args, format);
        Error *error = ErrorNewV(format, args);
        va_end(args);

        return error;
}

/* Creates a new Error based on a Gdi Status. */
Error *
GdiErrorNew(Status status, LPCTSTR format, ...)
{
        va_list args;
        va_start(args, format);
        Error *error = ErrorNewV(format, args);
        va_end(args);

        if (error == &s_fallback_error)
                return error;

        TCHAR status_message[MAX_STATUS_STRING_LENGTH];
        StatusToString(status, status_message, _countof(status_message));

        LPWSTR full_message = CreateErrorMessage(L"%s\r\n"
                                                 L"\r\n"
                                                 L"The error reported by GDI+ was:\r\n"
                                                 L"\r\n"
                                                 L"%s",
                                                 error->message, status_message);
        if (full_message == NULL)
                return error;

        FREE(error->message);
        error->message = full_message;

        return error;
}

/* Retrieves an Error’s error message. */
LPTSTR
ErrorMessage(Error *error)
{
        return error->message;
}

/* Frees an Error. */
void
ErrorFree(Error *error)
{
        if (error == &s_fallback_error)
                return;

        FREE(error->message);
        FREE(error);
}

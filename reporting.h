﻿#define MAX_ERROR_MESSAGE_LENGTH        (MAX_STATUS_STRING_LENGTH + 512)

typedef void (*ErrorHandler)(LPCTSTR);

void RegisterErrorHandler(ErrorHandler handler);
void RegisterFatalErrorHandler(ErrorHandler handler);

void ReportError(LPTSTR format, va_list args);
void ReportErrorV(LPTSTR format, va_list args);
void ReportFatalError(LPTSTR format, ...);
void ReportFatalErrorV(LPTSTR format, va_list args);

BOOL NoisyFindStringResourceExF(HINSTANCE instance, UINT id, LPTSTR id_as_string, UINT language, LPCWSTR *string, Error **error);
BOOL NoisyFindStringResourceF(HINSTANCE instance, UINT id, LPTSTR id_as_string, LPCWSTR *string, Error **error);
#define NoisyFindStringResourceEx(instance, id, string, language, error)        \
        NoisyFindStringResourceExF(instance, id, L#id, string, language, error)
#define NoisyFindStringResource(instance, id, string, error)    \
        NoisyFindStringResourceF(instance, id, L#id, string, error)

void FatalFindStringResourceExF(HINSTANCE instance, UINT id, LPTSTR id_as_string, UINT language, LPCWSTR *string);
void FatalFindStringResourceF(HINSTANCE instance, UINT id, LPTSTR id_as_string, LPCWSTR *string);
#define FatalFindStringResourceEx(instance, id, string, language)       \
        FatalFindStringResourceExF(instance, id, L#id, string, language)
#define FatalFindStringResource(instance, id, string)   \
        FatalFindStringResourceF(instance, id, L#id, string)

﻿static ErrorHandler s_errorhandler;
static ErrorHandler s_fatalerrorhandler;

void
RegisterErrorHandler(ErrorHandler handler)
{
        s_errorhandler = handler;
}

void 
RegisterFatalErrorHandler(ErrorHandler handler)
{
        s_fatalerrorhandler = handler;
}

static void 
ReportErrorToHandlerV(ErrorHandler handler, LPTSTR format, va_list args)
{
        if (handler == NULL)
                return;

        TCHAR message[MAX_ERROR_MESSAGE_LENGTH];

        _vsntprintf_s(message, _countof(message), _TRUNCATE, format, args);

        handler(message);
}

static void 
ReportErrorToHandler(ErrorHandler handler, LPTSTR format, ...)
{
        va_list args;
        
        va_start(args, format);
        ReportErrorToHandlerV(handler, format, args);
        va_end(args);
}

void 
ReportErrorV(LPTSTR format, va_list args)
{
        ReportErrorToHandlerV(s_errorhandler, format, args);
}

void 
ReportError(LPTSTR format, ...)
{
        va_list args;
        
        va_start(args, format);
        ReportErrorV(format, args);
        va_end(args);
}

void 
ReportFatalErrorV(LPTSTR format, va_list args)
{
        ReportErrorToHandlerV(s_fatalerrorhandler, format, args);
}

void 
ReportFatalError(LPTSTR format, ...)
{
        va_list args;
        
        va_start(args, format);
        ReportFatalErrorV(format, args);
        va_end(args);
}

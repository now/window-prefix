﻿typedef struct _Error Error;

Error *ErrorNew(LPCTSTR format, ...);
Error *GdiErrorNew(Status status, LPCTSTR format, ...);
LPTSTR ErrorMessage(Error *error);
void ErrorFree(Error *error);

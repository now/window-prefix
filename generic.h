static inline INT RectWidth(LPRECT rectangle)
{
        return rectangle->right - rectangle->left;
}

static inline INT RectHeight(LPRECT rectangle)
{
        return rectangle->bottom - rectangle->top;
}

#define RECTW(r) ((r).right - (r).left)
#define RECTH(r) ((r).bottom - (r).top)

#define INVALID_CXY (-1)

/* The maximum amount of time in microseconds we’ll wait until we
 * consider an application to be hung. */
#define HUNG_TIMEOUT                    100

#define MAX_STATUS_STRING_LENGTH        60

static inline BOOL HideWindow(HWND window)
{
        return ShowWindow(window, SW_HIDE);
}

static inline BOOL DisplayWindow(HWND window)
{
        return ShowWindow(window, SW_SHOW);
}

#define INITSTRUCT(structure, fInitSize)                \
        (ZeroMemory(&(structure), sizeof(structure)),   \
         fInitSize ? (*(int*) &(structure) = sizeof(structure)) : 0)

#define ALLOC_STRUCT(type)      \
        (type *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(type))

#define ALLOC_N(type, n)        \
        (type *)HeapAlloc(GetProcessHeap(), 0, sizeof(type) * (n))

#define REALLOC_N(type, pointer, n)     \
        (type *)(((pointer) == NULL) ? ALLOC_N(type, (n)) : HeapReAlloc(GetProcessHeap(), 0, (pointer), sizeof(type) * (n)))


#define FREE(pointer)   \
        HeapFree(GetProcessHeap(), 0, (pointer));

#define ZERO_TERMINATE(size)    \
        ((size) + 1)

#define HDC_OF_SCREEN           (NULL)

#define MACRO_BLOCK_START       \
        do {

#define MACRO_BLOCK_END         \
        } while (0)

#define RETURN_GDI_FAILURE(call) MACRO_BLOCK_START              \
        Status __return_on_gdiplus_failure_status = call;       \
if (__return_on_gdiplus_failure_status != Ok)                   \
return __return_on_gdiplus_failure_status;                      \
MACRO_BLOCK_END

typedef void (*FreeFunc)(void *);
typedef BOOL (*EqualityFunc)(void *, void *);
typedef BOOL (*PredicateFunc)(void *);

typedef enum
{
        IterationContinue,
        IterationStop
} IterationState;

void NullFreeFunc(void *data);
BOOL PointerEqualityFunc(void *p1, void *p2);
BOOL LastErrorWasTimeout(VOID);
BOOL MyIsHungAppWindow(HWND window);
BOOL MySwitchToThisWindow(HWND window);
BOOL IsToolWindow(HWND window);
BOOL EnumTaskBarWindows(WNDENUMPROC f, LPARAM lParam);
BOOL GetWindowTitle(HWND window, LPTSTR *title);
Status LoadWindowCaptionFont(Font **font);
int GetSystemMetricsDefault(int index, int default_dimension);
BOOL StatusToString(Status status, LPTSTR buffer, size_t size);
BOOL IsPrefixIgnoringCase(LPCTSTR string, LPCTSTR prefix);

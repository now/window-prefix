﻿#include <stdafx.h>

#include "bitmap.h"

/* Gets the width and height of BITMAP, returning any Status reported
 * while doing so. */
Status
BitmapGetDimensions(Bitmap *bitmap, UINT *width, UINT *height)
{
        *width = bitmap->GetWidth();
        RETURN_GDI_FAILURE(bitmap->GetLastStatus());

        *height = bitmap->GetHeight();
        RETURN_GDI_FAILURE(bitmap->GetLastStatus());

        return Ok;
}

/* Locks all bits in BITMAP into LOCKED_DATA. */
static inline Status
BitmapLockAll(Bitmap *bitmap, BitmapData *locked_data)
{
        UINT width, height;
        RETURN_GDI_FAILURE(BitmapGetDimensions(bitmap, &width, &height));

        PixelFormat format = bitmap->GetPixelFormat();
        RETURN_GDI_FAILURE(bitmap->GetLastStatus());

        return bitmap->LockBits(&Rect(0, 0, width, height), ImageLockModeRead, format, locked_data);
}

/* Iterates over a non-alpha, i.e., < 32-bit, BITMAP.  We do this by using
 * GetPixel(), as it will do the right thing.  Returns a GDI+ Status of how the iteration
 * fared.  WAS_STOPPED will be set to TRUE if iteration ended before all pixels of the bitmap
 * were iterated over. */
Status
NonAlphaBitmapIterate(Bitmap *bitmap, BitmapIterator iterator, VOID *closure, BOOL *was_stopped)
{
        *was_stopped = FALSE;

        UINT width, height;
        RETURN_GDI_FAILURE(BitmapGetDimensions(bitmap, &width, &height));

        IterationState state = IterationContinue;

        for (UINT y = 0; y < height; y++) {
                for (UINT x = 0; x < width; x++) {
                        Color pixel;
                        RETURN_GDI_FAILURE(bitmap->GetPixel(x, y, &pixel));
                        RETURN_GDI_FAILURE(iterator(Point(x, y), pixel.GetValue(), closure, &state));
                        if (state == IterationStop) {
                                *was_stopped = TRUE;
                                return Ok;
                        }
                }
        }

        return Ok;
}

/* Iterates over an alpha, i.e., 32-bit, bitmap’s data.  Here we can use the
 * raw data easily. */
static Status
BitmapDataIterate(BitmapData *data, BitmapIterator iterator, VOID *closure, BOOL *was_stopped)
{
        *was_stopped = FALSE;

        IterationState state = IterationContinue;

        for (UINT y = 0; y < data->Height; y++) {
                ARGB *row = BitmapDataRow(data, y);

                for (UINT x = 0; x < data->Width; x++) {
                        RETURN_GDI_FAILURE(iterator(Point(x, y), row[x], closure, &state));
                        if (state == IterationStop) {
                                *was_stopped = TRUE;
                                return Ok;
                        }
                }
        }

        return Ok;
}

/* Iterates over an alpha, i.e., 32-bit, bitmap.  This can be a lot faster
 * than the non-alpha case, as we can access the raw bitmap-data. */
Status
BitmapIterate(Bitmap *bitmap, BitmapIterator iterator, VOID *closure, BOOL *was_stopped)
{
        BitmapData locked_data;
        RETURN_GDI_FAILURE(BitmapLockAll(bitmap, &locked_data));

        Status iteration_status = BitmapDataIterate(&locked_data, iterator, closure, was_stopped);

        Status unlock_status = bitmap->UnlockBits(&locked_data);

        if (iteration_status != Ok)
                return iteration_status;

        return unlock_status;
}

/* Iterator checking for pixels in the bitmap with the alpha-channel set to
 * something other than completely transparent) or completely opaque.
 * Iteration is stopped as soon as a pixel with an alpha-channel is found. */
static Status
BitmapHasAlphaIterator(Point point, ARGB pixel, VOID *closure, IterationState *state)
{
        UNREFERENCED_PARAMETER(point);

        ARGB alpha = pixel & ALPHA_MASK;
        if (alpha == ALPHA_TRANSPARENT || alpha == ALPHA_OPAQUE)
                return Ok;

        *(BOOL *)closure = TRUE;

        *state = IterationStop;

        return Ok;
}

/* Checks if BITMAP has any pixels with an alpha-channel. */
Status
BitmapHasAlpha(Bitmap *bitmap, BOOL *has_alpha)
{
        *has_alpha = FALSE;

        BOOL was_stopped;

        return BitmapIterate(bitmap, BitmapHasAlphaIterator, has_alpha, &was_stopped);
}

/* Iterates over SOURCE, creating a copy by calling BLOCK to generate the copy. */
Status
BitmapIterateForCopy(Bitmap *source, BitmapIterateFunc block, BitmapIterator iterator,
                     VOID *inner_closure, Bitmap **copy)
{
        UINT width, height;
        RETURN_GDI_FAILURE(BitmapGetDimensions(source, &width, &height));

        *copy = new Bitmap(width, height);
        if (*copy == NULL)
                return OutOfMemory;

        BitmapIterateForCopyClosure closure;
        closure.inner_closure = inner_closure;

        Status status = BitmapLockAll(*copy, &closure.copy_data);
        if (status != Ok) {
                delete *copy;
                return status;
        }

        BOOL was_stopped;
        status = block(source, iterator, &closure, &was_stopped);

        Status unlock_status = (*copy)->UnlockBits(&closure.copy_data);

        if (status != Ok || unlock_status != Ok)
                delete *copy;

        if (status != Ok)
                return status;

        return unlock_status;
}

/* Iterator copying a bitmap into another. */
static Status
BitmapCopyGenericIterator(Point point, ARGB pixel, VOID *closure, IterationState *state)
{
        UNREFERENCED_PARAMETER(state);

        BitmapDataRow(&((BitmapIterateForCopyClosure *)closure)->copy_data, point.Y)[point.X] = pixel;

        return Ok;
}

/* Copies SOURCE through BLOCK. */
static Status
BitmapCopyGeneric(Bitmap *source, BitmapIterateFunc block, Bitmap **copy)
{
        return BitmapIterateForCopy(source, block, BitmapCopyGenericIterator, NULL, copy);
}

/* Creates an alpha bitmap copy of the non-alpha bitmap SOURCE. */
Status
NonAlphaBitmapCopy(Bitmap *source, Bitmap **copy)
{
        return BitmapCopyGeneric(source, NonAlphaBitmapIterate, copy);
}

/* Creates a copy of the alpha-bitmap SOURCE. */
Status
BitmapCopy(Bitmap *source, Bitmap **copy)
{
        return BitmapCopyGeneric(source, BitmapIterate, copy);
}

﻿/* Alpha value for total transparency. */
#define ALPHA_TRANSPARENT       (0x00)

/* Alpha value for no transparency at all. */
#define ALPHA_OPAQUE            (0xff)

/* An iterator over the pixels of a bitmap.  Returns a GDI+ Status of how the current
 * iteration fared.  POINT is the Point currently being iterated over, PIXEL is its color
 * value, CLOSURE is the iterations closure, and STATE controls how
 * iteration should proceed. */
typedef Status (*BitmapIterator)(Point, ARGB, VOID *, IterationState *);

/* Closure used for copying a bitmap into another.
 * COPY_DATA is the BitmapData for the bitmap we’re copying into.
 * INNER_CLOSURE is the closure we’re going to use for the inner iterator. */
typedef struct _BitmapIterateForCopyClosure BitmapIterateForCopyClosure;

struct _BitmapIterateForCopyClosure
{
        BitmapData copy_data;
        VOID *inner_closure;
};

/* A bitmap-iteration function for use with BitmapIterateForCopy(). */
typedef Status (*BitmapIterateFunc)(Bitmap *, BitmapIterator, VOID *, BOOL *);

/* Gets an ARGB pointer into DATA starting at the first pixel in ROW. */
static inline ARGB *BitmapDataRow(BitmapData *data, UINT row)
{
        return (ARGB *)((BYTE *)data->Scan0 + (row * data->Stride));
}

/* Gets the RGB value from an ARGB value, i.e., removes the alpha channel’s value. */
static inline ARGB ARGBGetRGB(ARGB pixel)
{
        return (pixel & ~ALPHA_MASK);
}

/* Sets the value of the alpha channel of an ARGB value. */
static inline ARGB ARGBSetAlpha(ARGB pixel, BYTE alpha)
{
        return ARGBGetRGB(pixel) | (alpha << ALPHA_SHIFT);
}

Status BitmapGetDimensions(Bitmap *bitmap, UINT *width, UINT *height);
Status NonAlphaBitmapIterate(Bitmap *bitmap, BitmapIterator iterator, VOID *closure, BOOL *was_aborted);
Status BitmapIterate(Bitmap *bitmap, BitmapIterator iterator, VOID *closure, BOOL *was_aborted);
Status BitmapHasAlpha(Bitmap *bitmap, BOOL *has_alpha);
Status BitmapIterateForCopy(Bitmap *source, BitmapIterateFunc block, BitmapIterator iterator,
                            VOID *inner_closure, Bitmap **copy);
Status NonAlphaBitmapCopy(Bitmap *source, Bitmap **copy);
Status BitmapCopy(Bitmap *source, Bitmap **copy);

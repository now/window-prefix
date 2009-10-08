#include "stdafx.h"

#include "buffer.h"
#include "textfield.h"

/* The TextField is responsible for maintaining and displaying the
 * user’s input.  As we only care about a prefix and only about
 * simple filtering of the window list, it only needs to be able
 * to add and remove characters at the end.  A cursor is still
 * displayed, though, to make it easier for the user to see what’s
 * going on.  The actual buffer handling is done in a Buffer. */

/* The leading of a TextField. */
#define TEXTFIELD_LEADING               2.0f

/* The amount of padding to the left of the cursor. */
#define TEXTFIELD_CURSOR_LEFT_PADDING   2.0f

/* The TextField consists of a BUFFER being drawn in FONT and has a
 * cached size of SIZE. */
struct _TextField
{
        Buffer *buffer;
        Font const *font;
        SizeF size;
};

static void
TextFieldInvalidateSize(TextField *field)
{
        field->size.Width = field->size.Height = INVALID_CXY;
}

/* We need to know when the Buffer changes, so that we can update the
 * TextField’s size. */
static void 
TextFieldBufferEventHandler(Buffer *buffer, BufferEvent event, VOID *closure)
{
        TextField *field = (TextField *)closure;

        if (event & BUFFER_ON_CHANGE)
                TextFieldInvalidateSize(field);
}

TextField *
TextFieldNew(Font const *font)
{
        TextField *field = ALLOC_STRUCT(TextField);
        if (field == NULL)
                return NULL;

        field->buffer = BufferNew();
        if (field->buffer == NULL)
                goto cleanup;

        if (!BufferRegisterListener(field->buffer, BUFFER_ON_CHANGE, TextFieldBufferEventHandler, field))
                goto cleanup;

        field->font = font;

        TextFieldInvalidateSize(field);

        return field;

cleanup:
        TextFieldFree(field);

        return NULL;
}

void 
TextFieldFree(TextField *field)
{
        if (field->buffer != NULL)
                BufferFree(field->buffer);
        FREE(field);
}

void 
TextFieldSetFont(TextField *field, Font const *font)
{
        field->font = font;
        TextFieldInvalidateSize(field);
}

/* We need to use the same settings for string formatting for when we
 * measure the TextField as when we actually draw the contents of it. */
static Status 
SetupStringFormat(StringFormat *format)
{
        /* TODO: Is there a way to set the trimming to the left? */
        RETURN_GDI_FAILURE(format->SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces));
        return format->SetTrimming(StringTrimmingEllipsisCharacter);
}

static Status 
UpdateHeight(TextField *field, Graphics const *graphics)
{
        REAL font_height = field->font->GetHeight(graphics);
        RETURN_GDI_FAILURE(field->font->GetLastStatus());
        field->size.Height = font_height + TEXTFIELD_LEADING;

        return Ok;
}

static Status 
UpdateWidth(TextField *field, Graphics const *graphics)
{
        if (BufferLength(field->buffer) == 0) {
                field->size.Width = 0.0f;
                return Ok;
        }

        StringFormat format;
        RETURN_GDI_FAILURE(format.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces));
        RETURN_GDI_FAILURE(format.SetTrimming(StringTrimmingEllipsisCharacter));
        CharacterRange ranges[] = { CharacterRange(0, BufferLength(field->buffer)) };
        RETURN_GDI_FAILURE(format.SetMeasurableCharacterRanges(1, ranges));

        RectF area(0.0f, 0.0f, 10000.0f, 10000.0f);
        Region regions[1];
        RETURN_GDI_FAILURE(graphics->MeasureCharacterRanges(BufferContents(field->buffer),
                                                            BufferLength(field->buffer),
                                                            field->font, area,
                                                            &format, 1, regions));
        RETURN_GDI_FAILURE(regions[0].GetBounds(&area, graphics));

        field->size.Width = area.Width;

        return Ok;
}

static Status 
TextFieldValidateSize(TextField *field, Graphics const *graphics)
{
        if (field->size.Width != INVALID_CXY && field->size.Height != INVALID_CXY)
                return Ok;

        RETURN_GDI_FAILURE(UpdateHeight(field, graphics));
        return UpdateWidth(field, graphics);
}

Status 
TextFieldSize(TextField *field, Graphics const *graphics, SizeF *size)
{
        RETURN_GDI_FAILURE(TextFieldValidateSize(field, graphics));

        *size = field->size;

        return Ok;
}

Buffer *
TextFieldBuffer(TextField const *field)
{
        return field->buffer;
}

static Status 
DrawBuffer(TextField *field, Graphics *graphics, RectF const *area)
{
        StringFormat format;
        RETURN_GDI_FAILURE(SetupStringFormat(&format));

        SolidBrush white_brush(Color::White);
        RETURN_GDI_FAILURE(white_brush.GetLastStatus());

        return graphics->DrawString(BufferContents(field->buffer),
                                    BufferLength(field->buffer),
                                    field->font, *area, &format, &white_brush);
}

static Status 
DrawCursor(TextField *field, Graphics *graphics, RectF const *area)
{
        Pen white_pen(Color::White);
        RETURN_GDI_FAILURE(white_pen.GetLastStatus());

        /* TODO: Should be Point so we don’t get fuzzy line-endings. */
        PointF top(area->GetLeft() + field->size.Width + TEXTFIELD_CURSOR_LEFT_PADDING, area->GetTop());
        PointF bottom(top.X, top.Y + field->size.Height - TEXTFIELD_LEADING);
        return graphics->DrawLine(&white_pen, top, bottom);
}

Status 
TextFieldDraw(TextField *field, Graphics *graphics, RectF const *area)
{
        TextFieldValidateSize(field, graphics);

        RETURN_GDI_FAILURE(DrawBuffer(field, graphics, area));
        return DrawCursor(field, graphics, area);
}

#define VK_CONTROL_U            VK_KANA

typedef BOOL (*KeyHandler)(TextField *field, int repetitions);

static BOOL 
ControlU(TextField *field, int repetitions)
{
        UNREFERENCED_PARAMETER(repetitions);

        BufferClear(field->buffer);

        return TRUE;
}

static BOOL 
Backspace(TextField *field, int repetitions)
{
        BufferPopChar(field->buffer, repetitions);

        return TRUE;
}

static BOOL 
DefaultKeyHandler(TextField *field, TCHAR c, int repetitions, BOOL control)
{
        if (control)
                return FALSE;

        BufferPushChar(field->buffer, c, repetitions);

        return TRUE;
}

BOOL 
TextFieldOnChar(TextField *field, TCHAR c, int repetitions, BOOL control)
{
        static struct {
                TCHAR key;
                BYTE control : 1;
                KeyHandler handler;
        } handlers[] = {
                { VK_CONTROL_U, TRUE, ControlU },
                { VK_BACK, FALSE, Backspace },
        };

        for (int i = 0; i < _countof(handlers); i++)
                if (handlers[i].key == c && (!handlers[i].control || control))
                        return handlers[i].handler(field, repetitions);

        return DefaultKeyHandler(field, c, repetitions, control);
}

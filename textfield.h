﻿typedef struct _TextField TextField;

TextField *TextFieldNew(Font const *font);
void TextFieldFree(TextField *field);
void TextFieldSetFont(TextField *field, Font const *font);
Status TextFieldSize(TextField *field, Graphics const *graphics, SizeF *size);
Buffer *TextFieldBuffer(TextField const *field);
Status TextFieldDraw(TextField *field, Graphics *graphics, RectF const *area);
BOOL TextFieldOnChar(TextField *field, TCHAR c, int n, BOOL control);

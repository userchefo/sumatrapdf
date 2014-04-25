/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusUtil_h
#define GdiPlusUtil_h

// used for communicating with DrawCloseButton()
#define BUTTON_HOVER_TEXT L"1"

#define COL_CLOSE_X RGB(0xa0, 0xa0, 0xa0)
#define COL_CLOSE_X_HOVER RGB(0xf9, 0xeb, 0xeb)  // white-ish
#define COL_CLOSE_HOVER_BG RGB(0xC1, 0x35, 0x35) // red-ish

// note: must write "using namespace Gdiplus;" before #include "GdiPlusUtil.h"
// this is to make sure we don't accidentally do that just by including this file

typedef RectF (* TextMeasureAlgorithm)(Graphics *g, Font *f, const WCHAR *s, int len);

RectF    MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, int len);
RectF    MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, int len);
RectF    MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, int len);
RectF    MeasureTextQuick(Graphics *g, Font *f, const WCHAR *s, int len);
RectF    MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len=-1, TextMeasureAlgorithm algo=NULL);
REAL     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=NULL);
size_t   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm algo=NULL);
void     DrawCloseButton(DRAWITEMSTRUCT *dis);

void     GetBaseTransform(Matrix& m, RectF pageRect, float zoom, int rotation);

const WCHAR * GfxFileExtFromData(const char *data, size_t len);
bool          IsGdiPlusNativeFormat(const char *data, size_t len);
Bitmap *      BitmapFromData(const char *data, size_t len);
Size          BitmapSizeFromData(const char *data, size_t len);
CLSID         GetEncoderClsid(const WCHAR *format);

#endif

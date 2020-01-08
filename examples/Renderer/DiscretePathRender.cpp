#include "DiscretePathRender.h"

#include "core/SkCanvas.h"
#include "core/SkPaint.h"
#include "core/SkPath.h"
#include "effects/SkDiscretePathEffect.h"
static SkPath star()
{
    const SkScalar R = 115.2f, C = 128.0f;
    SkPath path;
    path.moveTo(C + R, C);
    for (int i = 1; i < 8; ++i) {
        SkScalar a = 2.6927937f * i;
        path.lineTo(C + R * cos(a), C + R * sin(a));
    }
    return path;
}
void DiscretePathRender::draw(SkCanvas* canvas, int elapsed, int w, int h)
{
    SkPaint paint;
    paint.setPathEffect(SkDiscretePathEffect::Make(10.0f, 4.0f));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    paint.setAntiAlias(true);
    paint.setColor(0xff4285F4);
    canvas->clear(SK_ColorWHITE);
    SkPath path(star());
    canvas->drawPath(path, paint);
    canvas->flush();
}

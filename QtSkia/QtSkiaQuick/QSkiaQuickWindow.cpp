#include "QSkiaQuickWindow.h"
#include "InnerItem_p.h"

#include "core/SkImageInfo.h"
#include "core/SkSurface.h"
#include "gpu/GrContext.h"
#include "src/gpu/gl/GrGLUtil.h"

#include <QtQuick/private/qsgrenderloop_p.h>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPropertyAnimation>
#include <QQuickItem>
#include <QResizeEvent>
#include <QTime>
#include <QTimer>
#include <QRunnable>
#include <QCoreApplication>
class CleanUp : public QRunnable {
public:
    CleanUp(sk_sp<GrContext> ctx, sk_sp<SkSurface> surface): pCtx(ctx), pSurface(surface){}
    virtual void run() override
    {
        qWarning() << __FUNCTION__;
        pCtx.reset();
        pSurface.reset();
    }
private:
    sk_sp<GrContext> pCtx;
    sk_sp<SkSurface> pSurface;
};

class QSkiaQuickWindowPrivate {
public:
    sk_sp<GrContext> context = nullptr;
    sk_sp<SkSurface> gpuSurface = nullptr;
    QTime lastTimeA;
    QTime lastTimeB;
    InnerItem innerItem;
};

QSkiaQuickWindow::QSkiaQuickWindow(QWindow* parent)
    : QQuickWindow(parent)
    , m_dptr(new QSkiaQuickWindowPrivate)
{
    connect(this, &QQuickWindow::sceneGraphInitialized, this, &QSkiaQuickWindow::onSGInited, Qt::DirectConnection);
    connect(this, &QQuickWindow::sceneGraphInvalidated, this, &QSkiaQuickWindow::onSGUninited, Qt::DirectConnection);
    setClearBeforeRendering(false);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [&](){
        qWarning() << "aboutToQuit";
        auto pCleanUp = new CleanUp(m_dptr->context, m_dptr->gpuSurface);
        m_dptr->context = nullptr;
        m_dptr->gpuSurface = nullptr;
        QSGRenderLoop::instance()->postJob(this, pCleanUp);
    });
}

QSkiaQuickWindow::~QSkiaQuickWindow()
{
    qWarning() << __FUNCTION__;
    delete m_dptr;
    m_dptr = nullptr;
}

void QSkiaQuickWindow::onSGInited()
{
    qWarning() << __FUNCTION__;
    connect(this, &QQuickWindow::beforeRendering, this, &QSkiaQuickWindow::onBeforeRendering, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
    connect(this, &QQuickWindow::afterRendering, this, &QSkiaQuickWindow::onAfterRendering, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
//    connect(openglContext(), &QOpenGLContext::aboutToBeDestroyed, this, [&](){
//            qWarning() << "aboutToBeDestroyed";
//            s_context.clear();
//            s_gpuSurface.clear();
//        }, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));

    //设置innerItem,驱动渲染循环
    m_dptr->innerItem.setParentItem(contentItem());

    m_dptr->context = GrContext::MakeGL();
    SkASSERT(m_dptr->context);
    init(this->width(), this->height());
    m_dptr->lastTimeA = QTime::currentTime();
    m_dptr->lastTimeB = QTime::currentTime();
    onInit(this->width(), this->height());
}

void QSkiaQuickWindow::onSGUninited()
{
    qWarning() << __FUNCTION__;
    disconnect(this, &QQuickWindow::beforeRendering, this, &QSkiaQuickWindow::onBeforeRendering);
    disconnect(this, &QQuickWindow::afterRendering, this, &QSkiaQuickWindow::onAfterRendering);
}
void QSkiaQuickWindow::init(int w, int h)
{
    GrGLFramebufferInfo info;
    info.fFBOID = openglContext()->defaultFramebufferObject();
    SkColorType colorType;
    colorType = kRGBA_8888_SkColorType;
    if (format().renderableType() == QSurfaceFormat::RenderableType::OpenGLES) {
        info.fFormat = GR_GL_BGRA8;
    } else {
        info.fFormat = GR_GL_RGBA8;
    }
    GrBackendRenderTarget backend(w, h, format().samples(), format().stencilBufferSize(), info);
    // setup SkSurface
    // To use distance field text, use commented out SkSurfaceProps instead
    // SkSurfaceProps props(SkSurfaceProps::kUseDeviceIndependentFonts_Flag,
    //                      SkSurfaceProps::kLegacyFontHost_InitType);
    SkSurfaceProps props(SkSurfaceProps::kLegacyFontHost_InitType);

    m_dptr->gpuSurface = SkSurface::MakeFromBackendRenderTarget(m_dptr->context.get(), backend, kBottomLeft_GrSurfaceOrigin, colorType, nullptr, &props);

    if (!m_dptr->gpuSurface) {
        SkDebugf("SkSurface::MakeRenderTarget return null\n");
        return;
    }
    if (openglContext() && openglContext()->functions()) {
        openglContext()->functions()->glViewport(0, 0, w, h);
    }
}

void QSkiaQuickWindow::resizeEvent(QResizeEvent* e)
{
    if (e->size() == e->oldSize()) {
        e->accept();
        return;
    }
    if (isSceneGraphInitialized()) {
        init(e->size().width(), e->size().height());
        onResize(e->size().width(), e->size().height());
    }
}

void QSkiaQuickWindow::timerEvent(QTimerEvent*)
{
    if (isVisible() && isSceneGraphInitialized()) {
        update();
    }
}

void QSkiaQuickWindow::onBeforeRendering()
{
    if (!this->isVisible() || !isSceneGraphInitialized()) {
        return;
    }
    if (!m_dptr->gpuSurface) {
        return;
    }
    auto canvas = m_dptr->gpuSurface->getCanvas();
    if (!canvas) {
        return;
    }
    qWarning() << __FUNCTION__;
    const auto elapsed = m_dptr->lastTimeB.elapsed();
    m_dptr->lastTimeB = QTime::currentTime();
    canvas->save();
    this->drawBeforeSG(canvas, elapsed);
    canvas->restore();
}

void QSkiaQuickWindow::onAfterRendering()
{
    if (!this->isVisible()|| !isSceneGraphInitialized()) {
        return;
    }
    if (!m_dptr->gpuSurface) {
        return;
    }
    auto canvas = m_dptr->gpuSurface->getCanvas();
    if (!canvas) {
        return;
    }
    qWarning() << __FUNCTION__;
    const auto elapsed = m_dptr->lastTimeA.elapsed();
    m_dptr->lastTimeA = QTime::currentTime();
    canvas->save();
    this->drawAfterSG(canvas, elapsed);
    canvas->restore();
}
#include "moc_QSkiaQuickWindow.cpp"

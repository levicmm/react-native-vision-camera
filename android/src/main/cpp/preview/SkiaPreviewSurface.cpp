//
// Created by Marc Rousavy on 05.05.23.
//

#include "SkiaPreviewSurface.h"

#include <android/log.h>

#include "SkImage.h"
#include "SkSurface.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

namespace vision {

#define EGL_RECORDABLE_ANDROID 0x3142

SkiaPreviewSurface::SkiaPreviewSurface(jni::alias_ref<SkiaPreviewSurface::jhybridobject> jThis,
                                       jint textureId)
        : _javaPart(jni::make_global(jThis)) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "Creating SkiaPreviewSurface...");

    _inputSurface = createSurfaceFromNativeWindow(textureId, 480, 640);
}

void SkiaPreviewSurface::registerNatives() {
    registerHybrid({
       makeNativeMethod("initHybrid", SkiaPreviewSurface::initHybrid),
       makeNativeMethod("setOutputSize", SkiaPreviewSurface::setOutputSize),
       makeNativeMethod("onFrame", SkiaPreviewSurface::onFrame),
    });
}

void SkiaPreviewSurface::setOutputSize(jint width, jint height) {
    _outputWidth = width;
    _outputHeight = height;
}


void SkiaPreviewSurface::setOutputSurface(jni::alias_ref<jobject> surface) {
    // _renderer = std::make_unique<RNSkia::SkiaOpenGLRenderer>(surface.get());
}

sk_sp<SkSurface> SkiaPreviewSurface::createSurfaceFromNativeWindow(int eglTextureId, int width, int height) {
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        throw std::runtime_error("Failed to get EGL Display! " + std::to_string(glGetError()));
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        throw std::runtime_error("Failed to initialize EGL! " + std::to_string(glGetError()));
    }

    EGLint att[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RECORDABLE_ANDROID, 1,
                    EGL_ALPHA_SIZE, 8,
                    EGL_BLUE_SIZE, 8,
                    EGL_GREEN_SIZE, 8,
                    EGL_RED_SIZE, 8,
                    EGL_NONE};

    EGLint numConfigs;
    EGLConfig eglConfig;
    if (!eglChooseConfig(eglDisplay, att, &eglConfig, 1, &numConfigs) ||
        numConfigs == 0) {
        throw std::runtime_error("Failed to choose a GL Config! " + std::to_string(glGetError()));
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext eglContext = eglCreateContext(eglDisplay,
                                             eglConfig,
                                             EGL_NO_CONTEXT,
                                             contextAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        throw std::runtime_error("Failed to create a GL Context! " + std::to_string(glGetError()));
    }

    glBindTexture(0x00008d65, eglTextureId);

    GLint buffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
    GLint stencil;
    glGetIntegerv(GL_STENCIL_BITS, &stencil);
    GLint samples;
    glGetIntegerv(GL_SAMPLES, &samples);

    // Create the Skia backend context
    auto backendInterface = GrGLMakeNativeInterface();
    auto grContext = GrDirectContext::MakeGL(backendInterface);
    if (grContext == nullptr) {
        throw std::runtime_error("Failed to create Skia Context from GL Context! " + std::to_string(glGetError()));
    }

    auto maxSamples = grContext->maxSurfaceSampleCountForColorType(kRGBA_8888_SkColorType);
    if (samples > maxSamples)
        samples = maxSamples;

    GrGLFramebufferInfo fbInfo;
    fbInfo.fFBOID = buffer;
    fbInfo.fFormat = 0x8058; // GR_GL_RGBA8

    auto renderTarget = GrBackendRenderTarget(width, height, samples, stencil, fbInfo);

    struct RenderContext {
        EGLDisplay display;
    };
    auto ctx = new RenderContext({eglDisplay});

    auto surface = SkSurface::MakeFromBackendRenderTarget(
            grContext.get(), renderTarget, kBottomLeft_GrSurfaceOrigin,
            kRGBA_8888_SkColorType, nullptr, nullptr,
            [](void *addr) {
                auto ctx = reinterpret_cast<RenderContext *>(addr);
                delete ctx;
            },
            reinterpret_cast<void *>(ctx));
    return surface;
}

void SkiaPreviewSurface::onFrame() {
    __android_log_write(ANDROID_LOG_INFO, TAG,
                        "drawFrame(...)");

    SkPaint paint;
    paint.setColor(SkColors::kRed);
    auto rect = SkRect::MakeXYWH(100, 120, 180, 140);
    _inputSurface->getCanvas()->drawRect(rect, paint);
    _inputSurface->flushAndSubmit();

    /*
    auto image = _inputSurface->makeImageSnapshot();
    SkPixmap pixmap;
    bool success = image->readPixels(pixmap, 0, 0);
    auto color = pixmap.getColor(100, 100);

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Frame %i x %i Pixel: %i | SUCCESS: %i", image->width(), image->height(), color, success);

    _inputSurface->flushAndSubmit();*/

    /*_renderer->run([](SkCanvas* canvas) {
        SkPaint paint;
        paint.setColor(SkColors::kRed);
        auto rect = SkRect::MakeXYWH(100, 120, 180, 140);
        canvas->drawRect(rect, paint);

        //canvas->drawImage(image, 0, 0);
    }, _outputWidth, _outputHeight);*/
}

jint SkiaPreviewSurface::getSurfaceId() {
    auto backend = _inputSurface->getBackendRenderTarget(SkSurface::BackendHandleAccess::kFlushWrite_BackendHandleAccess);
    GrGLFramebufferInfo info;
    backend.getGLFramebufferInfo(&info);
    return static_cast<jint>(info.fFBOID);
}

} // namespace vision

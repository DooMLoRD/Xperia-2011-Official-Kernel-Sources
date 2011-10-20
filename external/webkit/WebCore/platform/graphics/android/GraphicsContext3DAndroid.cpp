/*
 * Copyright (C) 2011, Sony Ericsson Mobile Communications AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Sony Ericsson Mobile Communications AB nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SONY ERICSSON MOBILE COMMUNICATIONS AB BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "PlatformGraphicsContext.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"

#include "BitmapImage.h"
#include "CString.h"
#include "Extensions3DAndroid.h"
#include "Float32Array.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageDecoder.h"
#include "ImageSource.h"
#include "Int32Array.h"
#include "Int8Array.h"
#include "RenderBox.h"
#include "Uint8Array.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebViewCore.h"

#include "NativeImageSkia.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkImageDecoder.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wtf/OwnPtr.h>

#include <utils/Log.h>
//#define LOGMSG(...) ((void)android_printLog(ANDROID_LOG_DEBUG, "WebGL", __VA_ARGS__))
#define LOGMSG(...)

#undef  USE_FBO
#define NUMBER_OF_BITMAPS   1

using namespace std;
using namespace android;

namespace WebCore {

bool GraphicsContext3D::isGLES2Compliant() const
{
    return true;
}

/**********************************************************************
 * GraphicsContext3DInternal
 **********************************************************************/

#define CLAMP(x) GraphicsContext3DInternal::clampValue(x)

class GraphicsContext3DInternal
{
    friend class GraphicsContext3D;

public:
    GraphicsContext3DInternal(HTMLCanvasElement *canvas, GraphicsContext3D::Attributes attrs);
    ~GraphicsContext3DInternal();

    void makeContextCurrent();
    GraphicsContext3D::Attributes getContextAttributes();
    PlatformGraphicsContext3D platformGraphicsContext3D() const;

    void reshape(int width, int height);

    void paintRenderingResultsToCanvas(CanvasRenderingContext* context);

    unsigned long getError();
    void synthesizeGLError(unsigned long error);
    void checkGlError(const char* op);
    void checkEglError(const char* s);

    void viewport(long x, long y, unsigned long width, unsigned long height);

    bool refreshCanvas();
    void paint(SkCanvas* canvas);
    int  getContextId() { return m_contextId; }

    void recreateSurface();
    void releaseSurface();

    static GC3Dclampf clampValue(GC3Dclampf x)  {
        GC3Dclampf tmp = x;
        if (tmp < 0.0f) {
            tmp = 0.0f;
        }
        if (tmp > 1.0f) {
            tmp = 1.0f;
        }
        return tmp;
    }

private:
    // These routines are used for logging purposes
    void printGLString(const char *name, GLenum s);
    void printEGLConfiguration(EGLDisplay dpy, EGLConfig config);

    GraphicsContext3D::Attributes m_attrs;

    bool createContext();
    void createPbufferSurface(int width, int height);
    void createBitmaps(int width, int height);

    int m_width;
    int m_height;
    int m_maxwidth;
    int m_maxheight;

    struct {
        int x, y, width, height;
    } m_savedViewport;

    EGLDisplay m_dpy;
    EGLSurface m_surface;
    EGLContext m_context;
    EGLConfig  m_config;
    HTMLCanvasElement *m_canvas;
    android::Mutex     m_mutex;
    SkBitmap*  m_currentBitmap;
    SkBitmap*  m_bitmap[NUMBER_OF_BITMAPS];
    int        m_bitmapNumber;

    // Errors raised by synthesizeGLError().
    ListHashSet<unsigned long> m_syntheticErrors;

#ifdef USE_FBO
    void createFBO(int width, int height);

    unsigned int m_texture;
    unsigned int m_fbo;
    unsigned int m_depthBuffer;
    unsigned int m_boundFBO;
#endif

    bool m_layerComposited;
    OwnPtr<Extensions3DAndroid> m_extensions;

    int m_contextId;
};


void GraphicsContext3DInternal::checkGlError(const char* op)
{
    GLint error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGMSG("after %s() glError (0x%x)\n", op, error);
    }
    else {
        LOGMSG("%s() OK\n", op);
    }
}

void GraphicsContext3DInternal::checkEglError(const char* s)
{
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        LOGMSG("after %s() eglError = 0x%x\n", s, error);
    }
}

void GraphicsContext3DInternal::printGLString(const char *name, GLenum s)
{
    const char *v = (const char *)glGetString(s);
    LOGMSG("GL %s = %s\n", name, v);
}

void GraphicsContext3DInternal::printEGLConfiguration(EGLDisplay dpy, EGLConfig config)
{
#define X(VAL) {VAL, #VAL}
    struct {EGLint attribute; const char* name;} names[] = {
    X(EGL_BUFFER_SIZE),
    X(EGL_ALPHA_SIZE),
    X(EGL_BLUE_SIZE),
    X(EGL_GREEN_SIZE),
    X(EGL_RED_SIZE),
    X(EGL_DEPTH_SIZE),
    X(EGL_STENCIL_SIZE),
    X(EGL_CONFIG_CAVEAT),
    X(EGL_CONFIG_ID),
    X(EGL_LEVEL),
    X(EGL_MAX_PBUFFER_HEIGHT),
    X(EGL_MAX_PBUFFER_PIXELS),
    X(EGL_MAX_PBUFFER_WIDTH),
    X(EGL_NATIVE_RENDERABLE),
    X(EGL_NATIVE_VISUAL_ID),
    X(EGL_NATIVE_VISUAL_TYPE),
    //    X(EGL_PRESERVED_RESOURCES),
    X(EGL_SAMPLES),
    X(EGL_SAMPLE_BUFFERS),
    X(EGL_SURFACE_TYPE),
    X(EGL_TRANSPARENT_TYPE),
    X(EGL_TRANSPARENT_RED_VALUE),
    X(EGL_TRANSPARENT_GREEN_VALUE),
    X(EGL_TRANSPARENT_BLUE_VALUE),
    X(EGL_BIND_TO_TEXTURE_RGB),
    X(EGL_BIND_TO_TEXTURE_RGBA),
    X(EGL_MIN_SWAP_INTERVAL),
    X(EGL_MAX_SWAP_INTERVAL),
    X(EGL_LUMINANCE_SIZE),
    X(EGL_ALPHA_MASK_SIZE),
    X(EGL_COLOR_BUFFER_TYPE),
    X(EGL_RENDERABLE_TYPE),
    X(EGL_CONFORMANT),
   };
#undef X

    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
        EGLint value = -1;
        EGLint returnVal = eglGetConfigAttrib(dpy, config, names[j].attribute, &value);
        EGLint error = eglGetError();
        if (returnVal && error == EGL_SUCCESS) {
            LOGMSG(" %s: ", names[j].name);
            LOGMSG("%d (0x%x)\n", value, value);
        }
    }
    LOGMSG("\n");
}

GraphicsContext3DInternal::GraphicsContext3DInternal(HTMLCanvasElement *canvas, GraphicsContext3D::Attributes attrs)
    : m_attrs(attrs)
    , m_dpy(EGL_NO_DISPLAY)
    , m_surface(EGL_NO_SURFACE)
    , m_context(EGL_NO_CONTEXT)
    , m_canvas(canvas)
#ifdef USE_FBO
    , m_texture(0)
    , m_fbo(0)
    , m_depthBuffer(0)
    , m_boundFBO(0)
#endif
    , m_layerComposited(false)
    , m_extensions(0)
    , m_contextId(0)
{
    m_currentBitmap = NULL;
    for (int i = 0; i < NUMBER_OF_BITMAPS; i++) {
        m_bitmap[i] = NULL;
    }
    m_bitmapNumber = 0;

    EGLBoolean returnValue;

    EGLint majorVersion;
    EGLint minorVersion;

    m_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_dpy == EGL_NO_DISPLAY) {
        LOGMSG("eglGetDisplay returned EGL_NO_DISPLAY.\n");
        return;
    }

    returnValue = eglInitialize(m_dpy, &majorVersion, &minorVersion);
    if (returnValue != EGL_TRUE) {
        return;
    }
    LOGMSG("EGL version %d.%d\n", majorVersion, minorVersion);
    const char *s = eglQueryString(m_dpy, EGL_VENDOR);
    LOGMSG("EGL_VENDOR = %s\n", s);
    s = eglQueryString(m_dpy, EGL_VERSION);
    LOGMSG("EGL_VERSION = %s\n", s);
    s = eglQueryString(m_dpy, EGL_EXTENSIONS);
    LOGMSG("EGL_EXTENSIONS = %s\n", s);
    s = eglQueryString(m_dpy, EGL_CLIENT_APIS);
    LOGMSG("EGL_CLIENT_APIS = %s\n", s);

    EGLint* config_attribs = new EGLint[17];
    int p = 0;
    config_attribs[p++] = EGL_BLUE_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_GREEN_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_RED_SIZE;
    config_attribs[p++] = 8;
    config_attribs[p++] = EGL_SURFACE_TYPE;
    config_attribs[p++] = EGL_PBUFFER_BIT;
    config_attribs[p++] = EGL_RENDERABLE_TYPE;
    config_attribs[p++] = EGL_OPENGL_ES2_BIT;

    config_attribs[p++] = EGL_ALPHA_SIZE;
    config_attribs[p++] = m_attrs.alpha ? 8 : 0;
    if (m_attrs.depth) {
        config_attribs[p++] = EGL_DEPTH_SIZE;
        config_attribs[p++] = 16;
    }
    if (m_attrs.stencil) {
        config_attribs[p++] = EGL_STENCIL_SIZE;
        config_attribs[p++] = 8;
    }
    config_attribs[p] = EGL_NONE;

    // No support for other values for these:
    m_attrs.antialias = false;
    m_attrs.premultipliedAlpha = false;
    m_attrs.preserveDrawingBuffer = true;

    EGLint num_configs = 0;
    returnValue = eglChooseConfig(m_dpy, config_attribs, &m_config, 1, &num_configs);
    LOGMSG("num_configs = %d\n", num_configs);

    EGLint value;
    eglGetConfigAttrib(m_dpy, m_config, EGL_MAX_PBUFFER_WIDTH, &value);
    m_maxwidth = value;
    eglGetConfigAttrib(m_dpy, m_config, EGL_MAX_PBUFFER_HEIGHT, &value);
    m_maxheight = value;
    delete config_attribs;

    m_width = canvas->width() > m_maxwidth ? m_maxwidth : canvas->width();
    m_height = canvas->height() > m_maxheight ? m_maxheight : canvas->height();

    m_savedViewport.x = 0;
    m_savedViewport.y = 0;
    m_savedViewport.width = m_width;
    m_savedViewport.height = m_height;

    LOGMSG("Chose configuration: %d\n", m_config);
    printEGLConfiguration(m_dpy, m_config);

    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    m_extensions.set(new Extensions3DAndroid(ext));

    if (!createContext()) {
        return;
    }
    createPbufferSurface(m_width, m_height);
    createBitmaps(m_width, m_height);

    returnValue = eglMakeCurrent(m_dpy, m_surface, m_surface, m_context);
    if (returnValue != EGL_TRUE) {
        LOGMSG("eglMakeCurrent failed\n");
        return;
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);
    int v;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &v);
    LOGMSG("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS = %d\n", v);

    static int contextCounter = 1;
    m_contextId = contextCounter++;
}

GraphicsContext3DInternal::~GraphicsContext3DInternal()
{
    LOGMSG("+~GraphicsContext3DInternal(%d)\n", m_contextId);

#ifdef USE_FBO
    makeContextCurrent();
    glDeleteTextures(1, &m_texture);
    glDeleteRenderbuffers(1, &m_depthBuffer);
    glDeleteFramebuffers(1, &m_fbo);
    m_texture = 0;
    m_depthBuffer = 0;
    m_fbo = 0;
#endif

    eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_dpy, m_surface);
        m_surface = EGL_NO_SURFACE;
    }
    if (m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_dpy, m_context);
        m_context = EGL_NO_CONTEXT;
    }

    m_mutex.lock();
    for (int i = 0; i < NUMBER_OF_BITMAPS; i++) {
        if (m_bitmap[i]) {
            delete m_bitmap[i];
        }
    }
    m_mutex.unlock();

    LOGMSG("-~GraphicsContext3DInternal()\n");
}

bool GraphicsContext3DInternal::createContext()
{
    LOGMSG("createContext()\n");
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};

    m_context = eglCreateContext(m_dpy, m_config, EGL_NO_CONTEXT, context_attribs);
    EGLint error = eglGetError();
    if (error == EGL_BAD_ALLOC) {
        // Probably too many contexts. Force a JS garbage collection, and then try again.
        // This typically only happens in Khronos Conformance tests.
        LOGMSG(" error == EGL_BAD_ALLOC: try again after GC\n");
        m_canvas->document()->frame()->script()->lowMemoryNotification();
        m_context = eglCreateContext(m_dpy, m_config, EGL_NO_CONTEXT, context_attribs);
    }
    checkEglError("createContext");

    return (m_context != EGL_NO_CONTEXT);
}

void GraphicsContext3DInternal::createPbufferSurface(int width, int height)
{
    LOGMSG("+createPbufferSurface(%d, %d)\n", width, height);
    EGLint surface_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE};

    surface_attribs[1] = width;
    surface_attribs[3] = height;

    m_surface = eglCreatePbufferSurface(m_dpy, m_config, surface_attribs);
    checkEglError("eglCreatePbufferSurface");
    if (m_surface == EGL_NO_SURFACE) {
        LOGMSG("eglCreatePbufferSurface failed.\n");
        return;
    }

    LOGMSG("-createPbufferSurface()\n");
}

void GraphicsContext3DInternal::createBitmaps(int width, int height)
{
    m_mutex.lock();
    for (int i = 0; i < NUMBER_OF_BITMAPS; i++) {
        if (m_bitmap[i]) {
            delete m_bitmap[i];
        }
        m_bitmap[i] = new SkBitmap();
        if (m_bitmap[i]) {
            m_bitmap[i]->setConfig(SkBitmap::kARGB_8888_Config, width, height);
            m_bitmap[i]->allocPixels();
        }
    }
    m_bitmapNumber = 0;
    m_currentBitmap = NULL;
    m_mutex.unlock();
}

#ifdef USE_FBO
void GraphicsContext3DInternal::createFBO(int width, int height)
{
    if (!m_texture) {
        glGenTextures(1, &m_texture);
        checkGlError("glGenTextures");
        glBindTexture(GL_TEXTURE_2D, m_texture);
        checkGlError("glBindTexture");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        checkGlError("glTexParameterf");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        checkGlError("glTexParameterf");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        checkGlError("glTexParameterf");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        checkGlError("glTexParameterf");
        glBindTexture(GL_TEXTURE_2D, 0);
        checkGlError("glBindTexture");
        glGenFramebuffers(1, &m_fbo);
        checkGlError("glGenFramebuffers");
        glGenRenderbuffers(1, &m_depthBuffer);
        checkGlError("glGenRenderbuffers");
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    checkGlError("glTexImage2D");
    glBindTexture(GL_TEXTURE_2D, 0);
    checkGlError("glBindTexture");

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    checkGlError("glBindFramebuffer");
    m_boundFBO = m_fbo;
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
    checkGlError("glBindRenderbuffer");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    checkGlError("glRenderbufferStorage");
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    checkGlError("glBindRenderbuffer");

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    checkGlError("glFramebufferTexture2D");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
    checkGlError("glFramebufferRenderbuffer");
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGMSG("Framebuffer incomplete: %d\n", status);
    }
}
#endif

void GraphicsContext3DInternal::makeContextCurrent()
{
    if (eglGetCurrentContext() != m_context) {
        eglMakeCurrent(m_dpy, m_surface, m_surface, m_context);
    }
}

GraphicsContext3D::Attributes GraphicsContext3DInternal::getContextAttributes()
{
    return m_attrs;
}

PlatformGraphicsContext3D GraphicsContext3DInternal::platformGraphicsContext3D() const
{
    return m_context;
}

void GraphicsContext3DInternal::reshape(int width, int height)
{
    LOGMSG("reshape(%d, %d)\n", width, height);
    m_width = width;
    m_height = height;

    eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    checkEglError("reshape::eglMakeCurrent");
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_dpy, m_surface);
        checkEglError("reshape::eglDestroySurface");
    }

    createPbufferSurface(m_width, m_height);
    createBitmaps(m_width, m_height);

    eglMakeCurrent(m_dpy, m_surface, m_surface, m_context);
    checkEglError("reshape::eglMakeCurrent");

#ifdef USE_FBO
    createFBO(m_width, m_height);
#endif
}

bool GraphicsContext3DInternal::refreshCanvas()
{
    LOGMSG("+GraphicsContext3DInternal::refreshCanvas()\n");
#ifdef PROFILE_GFX_CONTEXT
    nsecs_t startTime = systemTime();
    static nsecs_t lastTime_refreshCanvas = 0;
    nsecs_t delta = startTime - lastTime_refreshCanvas;
    lastTime_refreshCanvas = startTime;
#endif
#ifdef USE_FBO
    bool mustRestoreFBO = (m_boundFBO != m_fbo);
    if (mustRestoreFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
#endif

    unsigned int idx = m_bitmapNumber++ % NUMBER_OF_BITMAPS;
    SkBitmap* bitmap = m_bitmap[idx];
    if (bitmap) {
        SkAutoLockPixels bitmapLock(*bitmap);
        unsigned char* pixels = static_cast<unsigned char*>(bitmap->getPixels());
        makeContextCurrent();
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    m_mutex.lock();
    m_currentBitmap = bitmap;
    m_mutex.unlock();

#ifdef USE_FBO
    if (mustRestoreFBO) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_boundFBO);
    }
#endif

#ifdef PROFILE_GFX_CONTEXT
    nsecs_t elapsed = systemTime() - startTime;
    TBENCHLOG("refreshCanvas()\t\t%7.4f ms | %7.4f ms\n", elapsed/1000000.0, delta/1000000.0);
#endif
    LOGMSG("-GraphicsContext3DInternal::refreshCanvas()\n");

    return true;
}

void GraphicsContext3DInternal::paint(SkCanvas* canvas)
{
#ifdef PROFILE_GFX_CONTEXT
    nsecs_t startTime = systemTime();
    static nsecs_t lastTime_paint = 0;
    nsecs_t delta = startTime - lastTime_paint;
    lastTime_paint = startTime;
#endif
    m_mutex.lock();
    if (m_currentBitmap == NULL) {
        m_mutex.unlock();
        return;
    }
    SkBitmap bitmap(*m_currentBitmap);
    SkAutoLockPixels bitmapLock(bitmap);
    m_mutex.unlock();

    int x = 0;
    int y = 0;
    IntRect isrc(0, 0, m_width, m_height);
    SkIRect src = isrc;
    IntRect idst;
    SkRect  dst;
    RenderObject* renderer = m_canvas->renderer();
    if (renderer && renderer->isBox()) {
        x = ((RenderBox*)renderer)->borderLeft() + ((RenderBox*)renderer)->paddingLeft();
        y = ((RenderBox*)renderer)->borderTop() + ((RenderBox*)renderer)->paddingTop();
        idst = ((RenderBox*)renderer)->contentBoxRect();
        dst = idst;
    }
    canvas->save();
    canvas->translate(0, SkIntToScalar(idst.height() + 2 * y));
    canvas->scale(SK_Scalar1, -SK_Scalar1);

    // Don't paint with alpha blending unless requested by attributes
    if (!m_attrs.alpha) {
        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        canvas->drawBitmapRect(bitmap, &src, dst, &paint);
    }
    else {
        canvas->drawBitmapRect(bitmap, &src, dst);
    }
    canvas->restore();

#ifdef PROFILE_GFX_CONTEXT
    nsecs_t elapsed = systemTime() - startTime;
    TBENCHLOG("paint()\t\t%7.4f ms | %7.4f ms\n", elapsed/1000000.0, delta/1000000.0);
#endif
}

void GraphicsContext3DInternal::recreateSurface()
{
    LOGMSG("GraphicsContext3DInternal::recreateSurface()\n");
    if (!createContext()) {
        LOGMSG("eglCreateContext failed\n");
        return;
    }
    reshape(m_width, m_height);
    glViewport(m_savedViewport.x, m_savedViewport.y, m_savedViewport.width, m_savedViewport.height);
}

void GraphicsContext3DInternal::releaseSurface()
{
    LOGMSG("+GraphicsContext3DInternal::releaseSurface(%d)\n", m_contextId);
    if (m_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(m_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(m_dpy, m_surface);
        eglDestroyContext(m_dpy, m_context);

        m_context = EGL_NO_CONTEXT;
        m_surface = EGL_NO_SURFACE;

        m_mutex.lock();
        for (int i = 0; i < NUMBER_OF_BITMAPS; i++) {
            if (m_bitmap[i]) {
                delete m_bitmap[i];
                m_bitmap[i] = NULL;
            }
        }
        m_bitmapNumber = 0;
        m_currentBitmap = NULL;
        m_mutex.unlock();
    }
    LOGMSG("-GraphicsContext3DInternal::releaseSurface()\n");
}

void GraphicsContext3DInternal::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    LOGMSG("+GraphicsContext3DInternal::paintRenderingResultsToCanvas()\n");
    ImageBuffer* imageBuffer = context->canvas()->buffer();
    const SkBitmap* canvasBitmap = imageBuffer->context()->platformContext()->bitmap();
    SkCanvas canvas(*canvasBitmap);

    // TODO: add a dirty flag so we can determine if this call is necessary
    refreshCanvas();

    m_mutex.lock();
    if (m_currentBitmap == NULL) {
        m_mutex.unlock();
        return;
    }
    SkBitmap bitmap(*m_currentBitmap);
    SkAutoLockPixels bitmapLock(bitmap);
    m_mutex.unlock();

    canvas.translate(0, SkIntToScalar(m_height));
    canvas.scale(SK_Scalar1, -SK_Scalar1);
    SkRect  dstRect;
    dstRect.iset(0, 0, imageBuffer->size().width(), imageBuffer->size().height());
    canvas.drawBitmapRect(bitmap, 0, dstRect);
    LOGMSG("-GraphicsContext3DInternal::paintRenderingResultsToCanvas()\n");
}

unsigned long GraphicsContext3DInternal::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }
    makeContextCurrent();
    return glGetError();
}

void GraphicsContext3DInternal::synthesizeGLError(unsigned long error)
{
    m_syntheticErrors.add(error);
}

void GraphicsContext3DInternal::viewport(long x, long y, unsigned long width, unsigned long height)
{
    LOGMSG("glViewport(%d, %d, %d, %d)\n", x, y, width, height);
    glViewport(x, y, width, height);
    m_savedViewport.x = x;
    m_savedViewport.y = y;
    m_savedViewport.width = width;
    m_savedViewport.height = height;
}

/**********************************************************************
 * GraphicsContext3D
 **********************************************************************/

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(HTMLCanvasElement *canvas, GraphicsContext3D::Attributes attrs)
{
    LOGMSG("GraphicsContext3D::create()\n");
    GraphicsContext3D* context = new GraphicsContext3D(canvas, attrs);
    if (context->m_internal->m_contextId == 0) {
        // Something failed during initialization
        delete context;
        return 0;
    }
    return adoptRef(context);
}

GraphicsContext3D::GraphicsContext3D(HTMLCanvasElement *canvas, GraphicsContext3D::Attributes attrs)
    : m_internal(new GraphicsContext3DInternal(canvas, attrs))
    , m_canvas(canvas)
{
    m_currentWidth = m_internal->m_width;
    m_currentHeight = m_internal->m_height;
}

GraphicsContext3D::~GraphicsContext3D() {
    LOGMSG("~GraphicsContext3D()\n");
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_internal->platformGraphicsContext3D();
}

void GraphicsContext3D::makeContextCurrent() {
    m_internal->makeContextCurrent();
}

IntSize GraphicsContext3D::getInternalFramebufferSize()
{
    return IntSize(m_currentWidth, m_currentHeight);
}

bool GraphicsContext3D::getImageData(Image* image,
                                     GC3Denum format,
                                     GC3Denum type,
                                     bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile,
                                     Vector<uint8_t>& outputVector)
{
    LOGMSG("getImageData(format = %d, type = %d)\n", format, type);
    if (!image)
        return false;

    AlphaOp neededAlphaOp = AlphaDoNothing;
    bool hasAlpha = (image->data() && image->isBitmapImage()) ?
        static_cast<BitmapImage*>(image)->frameHasAlphaAtIndex(0) : true;
    ImageDecoder* decoder = 0;
    RGBA32Buffer* buf = 0;

    if ((ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && image->data()) {
        // Attempt to get raw unpremultiplied image data
        decoder = ImageDecoder::create(*(image->data()), premultiplyAlpha, ignoreGammaAndColorProfile);
        if (decoder) {
            decoder->setData(image->data(), true);
            buf = decoder->frameBufferAtIndex(0);
            if (buf && buf->hasAlpha() && premultiplyAlpha)
                neededAlphaOp = AlphaDoPremultiply;
        }
    }

    SkBitmapRef* bitmapRef = 0;
    if (!buf) {
        bitmapRef = image->nativeImageForCurrentFrame();
        if (!bitmapRef)
            return false;
        if (!premultiplyAlpha && hasAlpha)
            neededAlphaOp = AlphaDoUnmultiply;
    }

    SkBitmap& bitmap = buf ? buf->bitmap() : bitmapRef->bitmap();
    unsigned char* pixels = 0;
    int rowBytes = 0;
    uint32_t* tmpPixels = 0;

    int width = bitmap.width();
    int height = bitmap.height();
    int iwidth = image->width();
    int iheight = image->height();
    LOGMSG("  bitmap.width() = %d, image->width() = %d, bitmap.height = %d, image->height() = %d\n",
           width, iwidth, height, iheight);
    if (width != iwidth || height != iheight) {
        // This image has probably been subsampled because it was too big.
        // Currently, we cannot handle this in WebGL: give up.
        return false;
    }

    SkBitmap::Config skiaConfig = bitmap.getConfig();

    bitmap.lockPixels();
    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        LOGMSG("  skiaConfig = kARGB_8888_Config\n");
        pixels = reinterpret_cast<unsigned char*>(bitmap.getPixels());
        rowBytes = bitmap.rowBytes();
        if (!pixels)
            return false;
    }
    else if (skiaConfig == SkBitmap::kIndex8_Config) {
        LOGMSG("  skiaConfig = kIndex8_Config\n");
        rowBytes = width * 4;
        tmpPixels = (uint32_t*)fastMalloc(width * height * 4);
        if (!tmpPixels)
            return false;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                SkPMColor c = bitmap.getIndex8Color(j, i);
                tmpPixels[i * width + j] = c;//SkExpand_8888(c);
            }
        }
        pixels = (unsigned char*)tmpPixels;
    }

    outputVector.resize(rowBytes * height);
    LOGMSG("rowBytes() = %d, width() = %d, height() = %d\n", rowBytes, width, height);

    bool res = packPixels(pixels,
                          SourceFormatRGBA8, width, height, 0,
                          format, type, neededAlphaOp, outputVector.data());
    bitmap.unlockPixels();

    if (decoder)
        delete decoder;

    if (tmpPixels)
        fastFree(tmpPixels);

    return res;
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    LOGMSG("glActiveTexture(%ld)\n", texture);
    makeContextCurrent();
    glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    LOGMSG("glAttachShader()\n");
    makeContextCurrent();
    glAttachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    LOGMSG("glBindAttribLocation()\n");
    if (!program) {
        return;
    }
    makeContextCurrent();
    glBindAttribLocation(program, index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    LOGMSG("glBindBuffer()\n");
    makeContextCurrent();
    glBindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    LOGMSG("glBindFrameBuffer()\n");
    makeContextCurrent();
#ifdef USE_FBO
    if (!buffer) {
        buffer = m_fbo;
    }
#endif
    glBindFramebuffer(target, buffer);
#ifdef USE_FBO
    m_boundFBO = buffer;
#endif
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    LOGMSG("glBindRenderBuffer()\n");
    makeContextCurrent();
    glBindRenderbuffer(target, renderbuffer);
}

void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    LOGMSG("glBindTexture()\n");
    makeContextCurrent();
    glBindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    LOGMSG("glBlendColor()\n");
    makeContextCurrent();
    glBlendColor(CLAMP(red), CLAMP(green), CLAMP(blue), CLAMP(alpha));
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    LOGMSG("glBlendEquation()\n");
    makeContextCurrent();
    glBlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    LOGMSG("glBlendEquationSeparate()\n");
    makeContextCurrent();
    glBlendEquationSeparate(modeRGB, modeAlpha);
}

void GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    LOGMSG("glBlendFunc()\n");
    makeContextCurrent();
    glBlendFunc(sfactor, dfactor);
}

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    LOGMSG("glBlendFuncSeparate()\n");
    makeContextCurrent();
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    LOGMSG("glBufferData()\n");
    makeContextCurrent();
    glBufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    LOGMSG("glBufferData(size = %d)\n", size);
    makeContextCurrent();
    glBufferData(target, size, data, usage);
    int err = glGetError();
    LOGMSG(" err = 0x%x\n", err);
    // Adreno OpenGL ES incorrectly sets err = GL_OUT_OF_MEMORY if size == 0
    if (err != GL_NO_ERROR && !(err == GL_OUT_OF_MEMORY && size == 0)) {
        synthesizeGLError(err);
    }
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    LOGMSG("glBufferSubData()\n");
    makeContextCurrent();
    glBufferSubData(target, offset, size, data);
}

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    LOGMSG("glCheckFramebufferStatus()\n");
    makeContextCurrent();
    return glCheckFramebufferStatus(target);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    LOGMSG("glClear()\n");
    makeContextCurrent();
    glClear(mask);
}

void GraphicsContext3D::clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    LOGMSG("glClearColor(%.2lf, %.2lf, %.2lf, %.2lf\n", red, green, blue, alpha);
    makeContextCurrent();
    glClearColor(CLAMP(red), CLAMP(green), CLAMP(blue), CLAMP(alpha));
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    LOGMSG("glClearDepthf()\n");
    makeContextCurrent();
    glClearDepthf(CLAMP(depth));
}

void GraphicsContext3D::clearStencil(GC3Dint s)
{
    LOGMSG("glClearStencil()\n");
    makeContextCurrent();
    glClearStencil(s);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    LOGMSG("glColorMask()\n");
    makeContextCurrent();
    glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    LOGMSG("glCompileShader()\n");
    makeContextCurrent();
    GLuint s = shader;
    glCompileShader(s);
}

//void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
//void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
                                       GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    LOGMSG("glCopyTexImage2D()\n");
    makeContextCurrent();
    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                          GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    LOGMSG("glCopyTexSubImage2D()\n");
    makeContextCurrent();
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    LOGMSG("glCullFace()\n");
    makeContextCurrent();
    glCullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    LOGMSG("glDepthFunc()\n");
    makeContextCurrent();
    glDepthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    LOGMSG("glDepthMask()\n");
    makeContextCurrent();
    glDepthMask(flag);
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    LOGMSG("glDepthRangef()\n");
    makeContextCurrent();
    glDepthRangef(CLAMP(zNear), CLAMP(zFar));
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    LOGMSG("glDetachShader()\n");
    makeContextCurrent();
    glDetachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    LOGMSG("glDisable()\n");
    makeContextCurrent();
    glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    LOGMSG("glDisableVertexAttribArray()\n");
    makeContextCurrent();
    glDisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    LOGMSG("glDrawArrays()\n");
    makeContextCurrent();
    glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    LOGMSG("glDrawElements()\n");
    makeContextCurrent();
    glDrawElements(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}


void GraphicsContext3D::enable(GC3Denum cap)
{
    LOGMSG("glEnable(0x%04x)\n", cap);
    makeContextCurrent();
    glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    LOGMSG("glEnableVertexAttribArray()\n");
    makeContextCurrent();
    glEnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    LOGMSG("glFinish()\n");
    makeContextCurrent();
    glFinish();
}

void GraphicsContext3D::flush()
{
    LOGMSG("glFlush()\n");
    makeContextCurrent();
    glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment,
                                                GC3Denum renderbuffertarget, Platform3DObject renderbuffer)
{
    LOGMSG("glFramebufferRenderbuffer()\n");
    makeContextCurrent();
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget,
                                             Platform3DObject texture, GC3Dint level)
{
    LOGMSG("glFramebufferTexture2D()\n");
    makeContextCurrent();
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    LOGMSG("glFrontFace()\n");
    makeContextCurrent();
    glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    LOGMSG("glGenerateMipmap()\n");
    makeContextCurrent();
    glGenerateMipmap(target);
}


bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    LOGMSG("glGetActiveAttrib()\n");
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxAttributeSize = 0;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    GLchar name[maxAttributeSize]; // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    LOGMSG("glGetActiveUniform()\n");
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxUniformSize = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize]; // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount,
                                           GC3Dsizei* count, Platform3DObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    makeContextCurrent();
    glGetAttachedShaders(program, maxCount, count, shaders);
}

GC3Dint GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    LOGMSG("glGetAttribLocation()\n");
    if (!program) {
        return -1;
    }
    makeContextCurrent();

    return glGetAttribLocation(program, name.utf8().data());
}

void GraphicsContext3D::getBooleanv(GC3Denum pname, GC3Dboolean* value)
{
    LOGMSG("glGetBooleanv()\n");
    makeContextCurrent();
    glGetBooleanv(pname, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetBufferParameteriv()\n");
    makeContextCurrent();
    glGetBufferParameteriv(target, pname, value);
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    LOGMSG("glGetContextAttributes()\n");
    return m_internal->getContextAttributes();
}

GC3Denum GraphicsContext3D::getError()
{
    LOGMSG("glGetError()\n");
    return m_internal->getError();
}

void GraphicsContext3D::getFloatv(GC3Denum pname, GC3Dfloat* value)
{
    LOGMSG("glGetFloatv()\n");
    makeContextCurrent();
    glGetFloatv(pname, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment,
                                                            GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetFramebufferAttachmentParameteriv()\n");
    makeContextCurrent();
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, value);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetIntegerv()\n");
    makeContextCurrent();
    glGetIntegerv(pname, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetProgramiv()\n");
    makeContextCurrent();
    glGetProgramiv(program, pname, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    LOGMSG("glGetProgramInfoLog()\n");
    makeContextCurrent();
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return "";

    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);

    glGetProgramInfoLog(program, length, &size, info);
    String s(info);
    fastFree(info);
    return s;
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetRenderbufferParameteriv()\n");
    makeContextCurrent();
    glGetRenderbufferParameteriv(target, pname, value);
    GLint err = glGetError();
    LOGMSG("  value = %d, error = 0x%x\n", *value, err);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetShaderiv()\n");
    makeContextCurrent();
    glGetShaderiv(shader, pname, value);
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    LOGMSG("glGetShaderInfoLog(%d)\n", shader);
    makeContextCurrent();
    GLuint shaderID = shader;
    GLint logLength;
    glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
    LOGMSG("  shader info log has length = %d\n", logLength);
    if (!logLength) {
        return "";
    }
    char* log = 0;
    if ((log = (char *)fastMalloc(logLength * sizeof(char))) == 0) {
        LOGMSG("fastMalloc(&d) failed\n", logLength * sizeof(char));
        return "";
    }
    GLsizei returnedLogLength;
    glGetShaderInfoLog(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String(log, returnedLogLength);
    LOGMSG("  shader = %d, msg = %s\n", shader, log);
    fastFree(log);

    return res;
}

// TBD
// void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    LOGMSG("glGetShaderSource()\n");
    makeContextCurrent();
    GLuint shaderID = shader;
    GLint logLength;
    glGetShaderiv(shaderID, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (!logLength) {
        return String();
    }
    char* log = 0;
    if ((log = (char *)fastMalloc(logLength * sizeof(char))) == 0) {
        return String();
    }
    GLsizei returnedLogLength;
    glGetShaderSource(shaderID, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    String res = String(log, returnedLogLength);
    fastFree(log);

    return res;
}

String GraphicsContext3D::getString(GC3Denum name)
{
    LOGMSG("glGetString()\n");
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(glGetString(name)));
}

void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value)
{
    LOGMSG("glGetTexParameterfv()\n");
    makeContextCurrent();
    glGetTexParameterfv(target, pname, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetTexParameteriv()\n");
    makeContextCurrent();
    glGetTexParameteriv(target, pname, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    LOGMSG("glGetUniformfv()\n");
    makeContextCurrent();
    glGetUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    LOGMSG("glGetUniformiv()\n");
    makeContextCurrent();
    glGetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    LOGMSG("glGetUniformLocation()\n");
    makeContextCurrent();
    return glGetUniformLocation(program, name.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value)
{
    LOGMSG("glGetVertexAttribfv()\n");
    makeContextCurrent();
    glGetVertexAttribfv(index, pname, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value)
{
    LOGMSG("glGetVertexAttribiv()\n");
    makeContextCurrent();
    glGetVertexAttribiv(index, pname, value);
}

GC3Dsizeiptr GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    LOGMSG("glGetVertexAttribOffset()\n");
    GLvoid* pointer = 0;
    glGetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    LOGMSG("glHint()\n");
    makeContextCurrent();
    glHint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    LOGMSG("glIsBuffer()\n");
    if (!buffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsBuffer(buffer);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    LOGMSG("glIsEnabled()\n");
    makeContextCurrent();
    return glIsEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    LOGMSG("glIsFramebuffer()\n");
    if (!framebuffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsFramebuffer(framebuffer);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject program)
{
    LOGMSG("glIsProgram()\n");
    if (!program)
        return GL_FALSE;
    makeContextCurrent();
    return glIsProgram(program);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    LOGMSG("glIsRenderbuffer()\n");
    if (!renderbuffer)
        return GL_FALSE;
    makeContextCurrent();
    return glIsRenderbuffer(renderbuffer);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject shader)
{
    LOGMSG("glIsShader()\n");
    if (!shader)
        return GL_FALSE;
    makeContextCurrent();
    return glIsShader(shader);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject texture)
{
    LOGMSG("glIsTexture()\n");
    if (!texture)
        return GL_FALSE;
    makeContextCurrent();
    return glIsTexture(texture);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    LOGMSG("glLineWidth()\n");
    makeContextCurrent();
    glLineWidth(width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    LOGMSG("glLinkProgram()\n");
    makeContextCurrent();
    glLinkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    LOGMSG("glPixelStorei()\n");
    makeContextCurrent();
    glPixelStorei(pname, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    LOGMSG("glPolygonOffset()\n");
    makeContextCurrent();
    glPolygonOffset(factor, units);
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height,
                                   GC3Denum format, GC3Denum type, void* data)
{
    LOGMSG("glReadPixels(width = %d, height = %d)\n", width, height);
    makeContextCurrent();
    glReadPixels(x, y, width, height, format, type, data);
    int err = glGetError();
    LOGMSG(" err = 0x%x\n", err);
    // Adreno OpenGL ES incorrectly sets err = GL_INVALID_VALUE if width == 0 || height == 0
    if (err != GL_NO_ERROR && (err != GL_INVALID_VALUE || (width != 0 && height != 0))) {
        synthesizeGLError(err);
    }
}

void GraphicsContext3D::releaseShaderCompiler()
{
    LOGMSG("glReleaseShaderCompiler()\n");
    makeContextCurrent();
    glReleaseShaderCompiler();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat,
                                            GC3Dsizei width, GC3Dsizei height)
{
    LOGMSG("glRenderbufferStorage(target = %d, internalformat = %d, width = %d, height = %d)\n",
           target, internalformat, width, height);
    makeContextCurrent();
    glRenderbufferStorage(target, internalformat, width, height);
    GLint error = glGetError();
    LOGMSG("  glRenderbufferStorage() => glError (0x%x)\n", error);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    LOGMSG("glSampleCoverage()\n");
    makeContextCurrent();
    glSampleCoverage(CLAMP(value), invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    LOGMSG("glScissor()\n");
    makeContextCurrent();
    glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& source)
{
    LOGMSG("glShaderSource()\n");
    makeContextCurrent();
    //    String prefixedSource;
    //    prefixedSource.append("precision mediump float;\n");
    //    prefixedSource.append(source);

    CString str = source.utf8();
    const char* data = str.data();
    GLint length = str.length();
    glShaderSource(shader, 1, &data, &length);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    LOGMSG("glStencilFunc()\n");
    makeContextCurrent();
    glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    LOGMSG("glStencilFuncSeparate()\n");
    makeContextCurrent();
    glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    LOGMSG("glStencilMask()\n");
    makeContextCurrent();
    glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    LOGMSG("glStencilMaskSeparate()\n");
    makeContextCurrent();
    glStencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    LOGMSG("glStencilOp()\n");
    makeContextCurrent();
    glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    LOGMSG("glStencilOpSeparate()\n");
    makeContextCurrent();
    glStencilOpSeparate(face, fail, zfail, zpass);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat,
                                   GC3Dsizei width, GC3Dsizei height, GC3Dint border,
                                   GC3Denum format, GC3Denum type, const void* pixels)
{
    LOGMSG("glTexImage2D()\n");
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param)
{
    LOGMSG("glTexParameterf()\n");
    makeContextCurrent();
    glTexParameterf(target, pname, param);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param)
{
    LOGMSG("glTexParameteri()\n");
    makeContextCurrent();
    glTexParameteri(target, pname, param);
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset,
                                      GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height,
                                      GC3Denum format, GC3Denum type, const void* pixels)
{
    LOGMSG("glTexSubImage2D()\n");
    makeContextCurrent();
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat x)
{
    LOGMSG("glUniform1f()\n");
    makeContextCurrent();
    glUniform1f(location, x);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGMSG("glUniform1fv()\n");
    makeContextCurrent();
    glUniform1fv(location, size, v);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint x)
{
    LOGMSG("glUniform1i()\n");
    makeContextCurrent();
    glUniform1i(location, x);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGMSG("glUniform1iv()\n");
    makeContextCurrent();
    glUniform1iv(location, size, v);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat x, float y)
{
    LOGMSG("glUniform2f()\n");
    makeContextCurrent();
    glUniform2f(location, x, y);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGMSG("glUniform2fv()\n");
    makeContextCurrent();
    glUniform2fv(location, size, v);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint x, GC3Dint y)
{
    LOGMSG("glUniform2i()\n");
    makeContextCurrent();
    glUniform2i(location, x, y);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGMSG("glUniform2iv()\n");
    makeContextCurrent();
    glUniform2iv(location, size, v);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    LOGMSG("glUniform3f()\n");
    makeContextCurrent();
    glUniform3f(location, x, y, z);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGMSG("glUniform3fv()\n");
    makeContextCurrent();
    glUniform3fv(location, size, v);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z)
{
    LOGMSG("glUniform3i()\n");
    makeContextCurrent();
    glUniform3i(location, x, y, z);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGMSG("glUniform3iv()\n");
    makeContextCurrent();
    glUniform3iv(location, size, v);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    LOGMSG("glUniform4f()\n");
    makeContextCurrent();
    glUniform4f(location, x, y, z, w);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dfloat* v, GC3Dsizei size)
{
    LOGMSG("glUniform4fv()\n");
    makeContextCurrent();
    glUniform4fv(location, size, v);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w)
{
    LOGMSG("glUniform4i()\n");
    makeContextCurrent();
    glUniform4i(location, x, y, z, w);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dint* v, GC3Dsizei size)
{
    LOGMSG("glUniform4iv()\n");
    makeContextCurrent();
    glUniform4iv(location, size, v);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    LOGMSG("glUniformMatrix2fv()\n");
    makeContextCurrent();
    glUniformMatrix2fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    LOGMSG("glUniformMatrix3fv()\n");
    makeContextCurrent();
    glUniformMatrix3fv(location, size, transpose, value);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dboolean transpose, GC3Dfloat* value, GC3Dsizei size)
{
    LOGMSG("glUniformMatrix4fv()\n");
    makeContextCurrent();
    glUniformMatrix4fv(location, size, transpose, value);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    LOGMSG("glUseProgram()\n");
    makeContextCurrent();
    glUseProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    LOGMSG("glValidateProgram()\n");
    makeContextCurrent();
    glValidateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat x)
{
    LOGMSG("glVertexAttrib1f()\n");
    makeContextCurrent();
    glVertexAttrib1f(index, x);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, GC3Dfloat* values)
{
    LOGMSG("glVertexAttrib1fv()\n");
    makeContextCurrent();
    glVertexAttrib1fv(index, values);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y)
{
    LOGMSG("glVertexAttrib2f()\n");
    makeContextCurrent();
    glVertexAttrib2f(index, x, y);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, GC3Dfloat* values)
{
    LOGMSG("glVertexAttrib2fv()\n");
    makeContextCurrent();
    glVertexAttrib2fv(index, values);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z)
{
    LOGMSG("glVertexAttrib3f()\n");
    makeContextCurrent();
    glVertexAttrib3f(index, x, y, z);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, GC3Dfloat* values)
{
    LOGMSG("glVertexAttrib3fv()\n");
    makeContextCurrent();
    glVertexAttrib3fv(index, values);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w)
{
    LOGMSG("glVertexAttrib4f()\n");
    makeContextCurrent();
    glVertexAttrib4f(index, x, y, z, w);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, GC3Dfloat* values)
{
    LOGMSG("glVertexAttrib4fv()\n");
    makeContextCurrent();
    glVertexAttrib4fv(index, values);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type,
                                            GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    LOGMSG("glVertexAttribPointer()\n");
    makeContextCurrent();
    glVertexAttribPointer(index, size, type, normalized, stride,
                          reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    LOGMSG("glViewport(%d, %d, %d, %d)\n", x, y, width, height);
    makeContextCurrent();
    m_internal->viewport(x, y, width, height);
}

void GraphicsContext3D::reshape(int width, int height)
{
    LOGMSG("GraphicsContext3D::reshape(%d, %d)\n", width, height);
    if ((width == m_currentWidth) && (height == m_currentHeight)) {
        return;
    }
    m_currentWidth = width > m_internal->m_maxwidth ? m_internal->m_maxwidth : width;
    m_currentHeight = height > m_internal->m_maxheight ? m_internal->m_maxheight : height;
    makeContextCurrent();
    m_internal->reshape(m_currentWidth, m_currentHeight);
}

// Support for buffer creation and deletion
Platform3DObject GraphicsContext3D::createBuffer()
{
    LOGMSG("glCreateBuffer()\n");
    makeContextCurrent();
    GLuint o;
    glGenBuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    LOGMSG("glCreateFramebuffer()\n");
    makeContextCurrent();
    GLuint o = 0;
    glGenFramebuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createProgram()
{
    LOGMSG("glCreateProgram()\n");
    makeContextCurrent();
    return glCreateProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    LOGMSG("glCreateRenderbuffer()\n");
    makeContextCurrent();
    GLuint o;
    glGenRenderbuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    LOGMSG("glCreateShader()\n");
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    LOGMSG("glCreateTexture()\n");
    makeContextCurrent();
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    LOGMSG("glDeleteBuffer()\n");
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject framebuffer)
{
    LOGMSG("glDeleteFramebuffer()\n");
    makeContextCurrent();
    glDeleteFramebuffers(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    LOGMSG("glDeleteProgram()\n");
    makeContextCurrent();
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject renderbuffer)
{
    LOGMSG("glDeleteRenderbuffer()\n");
    makeContextCurrent();
    glDeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    LOGMSG("glDeleteShader()\n");
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    LOGMSG("glDeleteTexture()\n");
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

int GraphicsContext3D::getContextId()
{
    return m_internal->getContextId();
}

bool GraphicsContext3D::refreshCanvas()
{
    makeContextCurrent();
    return m_internal->refreshCanvas();
}

void GraphicsContext3D::paint(SkCanvas* canvas)
{
    m_internal->paint(canvas);
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    makeContextCurrent();
    m_internal->paintRenderingResultsToCanvas(context);
}

PassRefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData()
{
    // FIXME: implement
    return 0;
}

IntRect GraphicsContext3D::canvasRect()
{
    // Return the absolute position of the canvas
    IntRect rect;
    RenderObject* renderer = m_canvas->renderer();

    if (renderer && renderer->isBox()) {
        rect = ((RenderBox*)renderer)->absoluteContentBox();
    }

    return rect;
}

void GraphicsContext3D::markContextChanged()
{
    m_internal->m_layerComposited = false;
}

void GraphicsContext3D::markLayerComposited()
{
    m_internal->m_layerComposited = true;
}

bool GraphicsContext3D::layerComposited() const
{
    return m_internal->m_layerComposited;
}

void GraphicsContext3D::recreateSurface()
{
    LOGMSG("recreateSurface()\n");
    m_internal->recreateSurface();
}

void GraphicsContext3D::releaseSurface()
{
    LOGMSG("releaseSurface()\n");
    m_internal->releaseSurface();
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    m_internal->synthesizeGLError(error);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    return m_internal->m_extensions.get();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGL)

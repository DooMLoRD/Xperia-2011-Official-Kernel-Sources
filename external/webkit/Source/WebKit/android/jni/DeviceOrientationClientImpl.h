/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeviceOrientationClientImpl_h
#define DeviceOrientationClientImpl_h

#include <DeviceOrientation.h>
#include <DeviceOrientationClient.h>
#include <JNIUtility.h>
#include <PassRefPtr.h>
#include <RefPtr.h>

using namespace WebCore;

namespace android {

class DeviceMotionAndOrientationManager;
class WebViewCore;

class DeviceOrientationClientImpl : public DeviceOrientationClient {
public:
    DeviceOrientationClientImpl(WebViewCore*);
    virtual ~DeviceOrientationClientImpl();

    void onOrientationChange(PassRefPtr<DeviceOrientation>);
    void suspend();
    void resume();

    // DeviceOrientationClient methods
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual DeviceOrientation* lastOrientation() const { return m_lastOrientation.get(); }
    virtual void setController(DeviceOrientationController* controller) { m_controller = controller; }
    virtual void deviceOrientationControllerDestroyed() { }

private:
    jobject getJavaInstance();
    void releaseJavaInstance();

    WebViewCore* m_webViewCore;
    jobject m_javaDeviceOrientationServiceObject;
    DeviceOrientationController* m_controller;
    RefPtr<DeviceOrientation> m_lastOrientation;
};

} // namespace android

#endif // DeviceOrientationClientImpl_h

/*
 * Copyright 2006, The Android Open Source Project
 * Portions created by Sony Ericsson are Copyright (C) 2011,
 * 2012 Sony Ericsson Mobile Communications AB.
 * All Rights Reserved.
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
/*
 * This file has been modified by Sony Ericsson on 2011-11-25, 2012-02-04.
 */

// TODO: change name to WebFrame.h

#ifndef WebCoreFrameBridge_h
#define WebCoreFrameBridge_h

#include "FrameLoaderClient.h"
#include "PlatformBridge.h"
#include "PlatformString.h"
#include "WebCoreRefObject.h"
#include <jni.h>
#include <string>
#include <wtf/RefCounted.h>

namespace WebCore {
    class HTMLFormElement;
    class Frame;
    class HistoryItem;
    class Image;
    class Page;
    class RenderPart;
    class RenderSkinAndroid;
    class ResourceHandle;
    class ResourceLoaderAndroid;
    class ResourceRequest;
}

namespace android {

class WebViewCore;
class WebUrlLoaderClient;
class UrlInterceptResponse;

// one instance of WebFrame per Page for calling into Java's BrowserFrame
class WebFrame : public WebCoreRefObject {
  public:
    WebFrame(JNIEnv* env, jobject obj, jobject historyList, WebCore::Page* page);
    ~WebFrame();

    // helper function
    static WebFrame* getWebFrame(const WebCore::Frame* frame);

    virtual PassRefPtr<WebCore::ResourceLoaderAndroid> startLoadingResource(WebCore::ResourceHandle*,
            const WebCore::ResourceRequest& request, bool mainResource,
            bool synchronous);

    UrlInterceptResponse* shouldInterceptRequest(const WTF::String& url);

    void reportError(int errorCode, const WTF::String& description,
            const WTF::String& failingUrl);

    void loadStarted(WebCore::Frame* frame);

    void transitionToCommitted(WebCore::Frame* frame);

    void didFinishLoad(WebCore::Frame* frame);

    void addHistoryItem(WebCore::HistoryItem* item);

    void removeHistoryItem(int index);

    void updateHistoryIndex(int newIndex);

    void setTitle(const WTF::String& title);

    void windowObjectCleared(WebCore::Frame* frame);

    void setProgress(float newProgress);

    const WTF::String userAgentForURL(const WebCore::KURL* url);

    void didReceiveIcon(WebCore::Image* icon);

    void didReceiveTouchIconURL(const WTF::String& url, bool precomposed);
    
    void updateVisitedHistory(const WebCore::KURL& url, bool reload);

    // Used to determine whether the WebView should handle the given request.
    // Returns true if it should handle it, otherwise false.
    virtual bool canHandleRequest(const WebCore::ResourceRequest& request);

    WebCore::Frame* createWindow(bool dialog, bool userGesture);

    void requestFocus() const;

    void closeWindow(WebViewCore* webViewCore);

    void decidePolicyForFormResubmission(WebCore::FramePolicyFunction func);

    void setUserAgent(WTF::String userAgent) { mUserAgent = userAgent; }

    WTF::String getRawResourceFilename(WebCore::PlatformBridge::rawResId) const;

    float density() const;

#if USE(CHROME_NETWORK_STACK)
    const WTF::String userAgentProfile();
    void setUserAgentProfile(WTF::String userAgentProfile) { mUserAgentProfile = userAgentProfile; }
    void didReceiveAuthenticationChallenge(WebUrlLoaderClient*, const std::string& host, const std::string& realm, bool useCachedCredentials, bool suppressDialog);
    void reportSslCertError(WebUrlLoaderClient* client, int cert_error, const std::string& cert, const std::string& url);
    void requestClientCert(WebUrlLoaderClient* client, const std::string& hostAndPort);
    void downloadStart(const std::string& url, const std::string& userAgent, const std::string& contentDisposition, const std::string& mimetype, long long contentLength);
    void didReceiveData(const char* data, int size);
    void didFinishLoading();
    void setCertificate(const std::string& cert);
#endif

    void maybeSavePassword(WebCore::Frame* frame, const WebCore::ResourceRequest& request);


    // Parse the x-auto-login header and propagate the parameters to the
    // application.
    void autoLogin(const std::string& loginHeader);

    /**
     * When the user initiates a click, we set mUserInitiatedAction to true.
     * If a load happens due to this click, then we ask the application if it wants
     * to override the load. Otherwise, we attempt to load the resource internally.
     */
    void setUserInitiatedAction(bool userInitiatedAction) { mUserInitiatedAction = userInitiatedAction; }

    WebCore::Page* page() const { return mPage; }

    // Currently used only by the chrome net stack.  A similar field is used by
    // FrameLoader.java to block java network loads.
    void setBlockNetworkLoads(bool block) { mBlockNetworkLoads = block; }
    bool blockNetworkLoads() const { return mBlockNetworkLoads; }

    /**
     * Helper methods. These are typically chunks of code that are called in
     * slightly different ways by the Apache and Chrome HTTP stacks.
     */
    bool getUsernamePasswordFromDom(WebCore::Frame* frame, WTF::String& username, WTF::String& password);
    jobject getPostData(const WebCore::ResourceRequest& request);

    bool shouldSaveFormData();
    void saveFormData(WebCore::HTMLFormElement*);
    WebCore::RenderSkinAndroid* renderSkins() const { return m_renderSkins; }
    void setRenderSkins(WebCore::RenderSkinAndroid* skins) { m_renderSkins = skins; }

    // Convert a URL from potential punycode I18nDomainName to safe to-be-displayed Unicode.
    static WTF::String convertIDNToUnicode(const WebCore::KURL& kurl);

  private:
    struct JavaBrowserFrame;
    JavaBrowserFrame* mJavaFrame;
    WebCore::Page* mPage;
    WTF::String mUserAgent;
    bool mBlockNetworkLoads;
    bool mUserInitiatedAction;
    WebCore::RenderSkinAndroid* m_renderSkins;
#if USE(CHROME_NETWORK_STACK)
    WTF::String mUserAgentProfile;
#endif
};

}   // namespace android

#endif // WebCoreFrameBridge_h

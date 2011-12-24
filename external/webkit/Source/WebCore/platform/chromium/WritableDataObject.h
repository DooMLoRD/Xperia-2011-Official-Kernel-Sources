/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WritableDataObject_h
#define WritableDataObject_h

#include "Clipboard.h"
#include "KURL.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// Used for one way communication of drag/drop and copy/paste data from the
// renderer to the browser. This is intenteded to be used in dragstart/copy/cut
// events. Due to shortcomings, writes to the pasteboard cannot be performed
// atomically.
class WritableDataObject : public RefCounted<WritableDataObject> {
public:
    static PassRefPtr<WritableDataObject> create(Clipboard::ClipboardType);

    void clearData(const String& type);
    void clearAllExceptFiles();
    void clearAll();
    bool setData(const String& type, const String& data);

    void setUrlTitle(const String& title) { m_urlTitle = title; }
    void setHtmlBaseUrl(const KURL& baseURL) { m_htmlBaseURL = baseURL; }

    // Used for transferring drag data from the renderer to the browser.
    HashMap<String, String> dataMap() const { return m_dataMap; }
    String urlTitle() const { return m_urlTitle; }
    KURL htmlBaseURL() const { return m_htmlBaseURL; }

    String fileExtension() const { return m_fileExtension; }
    String fileContentFilename() const { return m_fileContentFilename; }
    PassRefPtr<SharedBuffer> fileContent() const { return m_fileContent; }
    void setFileExtension(const String& fileExtension) { m_fileExtension = fileExtension; }
    void setFileContentFilename(const String& fileContentFilename) { m_fileContentFilename = fileContentFilename; }
    void setFileContent(PassRefPtr<SharedBuffer> fileContent) { m_fileContent = fileContent; }

private:
    explicit WritableDataObject(Clipboard::ClipboardType);

    Clipboard::ClipboardType m_clipboardType;

    HashMap<String, String> m_dataMap;
    String m_urlTitle;
    KURL m_htmlBaseURL;
    String m_fileExtension;
    String m_fileContentFilename;
    RefPtr<SharedBuffer> m_fileContent;
};

} // namespace WebCore

#endif

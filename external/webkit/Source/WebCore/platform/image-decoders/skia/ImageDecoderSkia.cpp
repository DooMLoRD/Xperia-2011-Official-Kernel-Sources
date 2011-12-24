/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDecoder.h"
#if PLATFORM(ANDROID)
#include "SkBitmapRef.h"
#endif

#include "NotImplemented.h"

namespace WebCore {

ImageFrame::ImageFrame()
    : m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
    , m_premultiplyAlpha(true)
{
}

ImageFrame& ImageFrame::operator=(const ImageFrame& other)
{
    if (this == &other)
        return *this;

    m_bitmap = other.m_bitmap;
    // Keep the pixels locked since we will be writing directly into the
    // bitmap throughout this object's lifetime.
    m_bitmap.lockPixels();
    setOriginalFrameRect(other.originalFrameRect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    setPremultiplyAlpha(other.premultiplyAlpha());
    return *this;
}

void ImageFrame::clearPixelData()
{
    m_bitmap.reset();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache()
    // calls this to free the bitmap data, but other functions like
    // initFrameBuffer() and frameComplete() may still need to read
    // other metadata out of this frame later.
}

void ImageFrame::zeroFillPixelData()
{
    m_bitmap.eraseARGB(0, 0, 0, 0);
}

bool ImageFrame::copyBitmapData(const ImageFrame& other)
{
    if (this == &other)
        return true;

    m_bitmap.reset();
    const NativeImageSkia& otherBitmap = other.m_bitmap;
    return otherBitmap.copyTo(&m_bitmap, otherBitmap.config());
}

bool ImageFrame::setSize(int newWidth, int newHeight)
{
    // This function should only be called once, it will leak memory
    // otherwise.
    ASSERT(width() == 0 && height() == 0);
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, newWidth, newHeight);
    if (!m_bitmap.allocPixels())
        return false;

    zeroFillPixelData();

    return true;
}

NativeImagePtr ImageFrame::asNewNativeImage() const
{
#if PLATFORM(ANDROID)
    return new SkBitmapRef(m_bitmap);
#else
    return new NativeImageSkia(m_bitmap);
#endif
}

bool ImageFrame::hasAlpha() const
{
    return !m_bitmap.isOpaque();
}

void ImageFrame::setHasAlpha(bool alpha)
{
    m_bitmap.setIsOpaque(!alpha);
}

void ImageFrame::setColorProfile(const ColorProfile& colorProfile)
{
    notImplemented();
}

void ImageFrame::setStatus(FrameStatus status)
{
    m_status = status;
    if (m_status == FrameComplete)
        m_bitmap.setDataComplete();  // Tell the bitmap it's done.
}

int ImageFrame::width() const
{
    return m_bitmap.width();
}

int ImageFrame::height() const
{
    return m_bitmap.height();
}

} // namespace WebCore

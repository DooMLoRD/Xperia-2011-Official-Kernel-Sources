/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB
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

#if ENABLE(WEBGL)

#include "Uint16Array.h"

namespace WebCore {

PassRefPtr<Uint16Array> Uint16Array::create(unsigned length)
{
    return TypedArrayBase<unsigned short>::create<Uint16Array>(length);
}

PassRefPtr<Uint16Array> Uint16Array::create(unsigned short* array, unsigned length)
{
    return TypedArrayBase<unsigned short>::create<Uint16Array>(array, length);
}

PassRefPtr<Uint16Array> Uint16Array::create(PassRefPtr<ArrayBuffer> buffer, unsigned byteOffset, unsigned length)
{
    return TypedArrayBase<unsigned short>::create<Uint16Array>(buffer, byteOffset, length);
}

Uint16Array::Uint16Array(PassRefPtr<ArrayBuffer> buffer, unsigned byteOffset, unsigned length)
    : IntegralTypedArrayBase<unsigned short>(buffer, byteOffset, length)
{
}

PassRefPtr<Uint16Array> Uint16Array::subarray(int begin) const
{
    return subarray(begin, length());
}

PassRefPtr<Uint16Array> Uint16Array::subarray(int begin, int end) const
{
    return sliceImpl<Uint16Array>(begin, end);
}

PassRefPtr<ArrayBufferView> Uint16Array::slice(int start, int end) const
{
    return sliceImpl<Uint16Array>(start, end);
}

}

#endif // ENABLE(WEBGL)

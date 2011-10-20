/*
 * keyDeriveTkip.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Texas Instruments nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/** \file keyDeriveTkip.h
 *  \brief TKIP key derivation API
 *
 *  \see keyDeriveTkip.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:	WEP broadcast key derivation                                *
 *   PURPOSE:   WEP broadcast key derivation API                            *
 *                                                                          *
 ****************************************************************************/

#ifndef _KEY_DERIVE_TKIP_H
#define _KEY_DERIVE_TKIP_H

#include "rsnApi.h"
#include "keyTypes.h"

#include "keyDerive.h"

#define KEY_DERIVE_TKIP_ENC_LEN 16
#define KEY_DERIVE_TKIP_MIC_LEN 8

typedef struct {
	TI_UINT8   macAddress[MAC_ADDR_LEN];
	TI_UINT8   keyRSC[KEY_RSC_LEN];
	TI_UINT8   encKey[KEY_DERIVE_TKIP_ENC_LEN];
	TI_UINT8   micRxKey[KEY_DERIVE_TKIP_MIC_LEN];
	TI_UINT8   micTxKey[KEY_DERIVE_TKIP_MIC_LEN];
} keyMaterialTkip_t;

/* WEP broadcast key derivation init function. */
TI_STATUS keyDeriveTkip_config(struct _keyDerive_t *pKeyDerive);

TI_STATUS keyDeriveTkip_derive(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);
TI_STATUS keyDeriveTkip_remove(struct _keyDerive_t *pKeyDerive, encodedKeyMaterial_t *pEncodedKey);

#endif /*  __INCLUDE_WEP_BROADCAST_KEY_DERIVATION_H*/

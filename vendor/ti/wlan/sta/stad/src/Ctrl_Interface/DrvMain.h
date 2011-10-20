/*
 * DrvMain.h
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



#ifndef DRVMAIN_H
#define DRVMAIN_H


#include "paramOut.h"
#include "WlanDrvCommon.h"


/* Driver-Main module external functions */

TI_STATUS drvMain_Create 			(TI_HANDLE  hOs,
                             TI_HANDLE *pDrvMainHndl,
                             TI_HANDLE *pCmdHndlr,
                             TI_HANDLE *pContext,
                             TI_HANDLE *pTxDataQ,
                             TI_HANDLE *pTxMgmtQ,
                             TI_HANDLE *pTxCtrl,
                             TI_HANDLE *pTwd,
                             TI_HANDLE *pEvHandler,
                             TI_HANDLE *pCmdDispatch,
                             TI_HANDLE *pReport);
TI_STATUS drvMain_Destroy           (TI_HANDLE  hDrvMain);
TI_STATUS drvMain_InsertAction      (TI_HANDLE  hDrvMain, EActionType eAction);
TI_STATUS drvMain_Recovery          (TI_HANDLE  hDrvMain);
void      drvMain_SmeStop           (TI_HANDLE hDrvMain);
void      drvMain_PowerMgrSuspended (TI_HANDLE hDrvMain, TI_BOOL success);
#endif

/*
 * ipc_sta.h
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/****************************************************************************/
/*                                                                          */
/*    MODULE:   IPC_STA.h                                                   */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/
#ifndef _IPC_STA_H_
#define _IPC_STA_H_

#include "oserr.h"

/* defines */
/***********/

/* types */
/*********/

/* functions */
/*************/
THandle IpcSta_Create(const PS8 device_name);
VOID IpcSta_Destroy(THandle hIpcSta);
S32 IPC_STA_Private_Send(THandle hIpcSta, U32 ioctl_cmd, PVOID bufIn, U32 sizeIn, PVOID bufOut, U32 sizeOut);
S32 IPC_STA_Wext_Send(THandle hIpcSta, U32 wext_request_id, PVOID p_iwreq_data, U32 len);
#endif  /* _IPC_STA_H_ */


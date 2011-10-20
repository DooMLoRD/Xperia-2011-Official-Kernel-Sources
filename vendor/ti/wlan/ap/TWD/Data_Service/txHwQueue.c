/*
 * txHwQueue.c
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


/****************************************************************************
 *
 *   MODULE:  txHwQueue.c
 *
 *   PURPOSE: manage the wlan hardware Tx memory blocks allocation per queue.
 *
 *   DESCRIPTION:
 *   ============
 *      This module is responsible for the HW Tx data-blocks and descriptors allocation.
 *      The HW Tx blocks are allocated in the driver by rough calculations without
 *        accessing the FW.
 *      They are freed according to FW counters that are provided by the FwEvent module
 *          on every FW interrupt.
 ****************************************************************************/
#define __FILE_ID__  FILE_ID_100
#include "osApi.h"
#include "report.h"
#include "TWDriver.h"
#include "txCtrlBlk_api.h"
#include "txHwQueue_api.h"
#include "DrvMainModules.h"
#include "wlanLinks.h"

/* Translate input TID to AC */
/* Note: This structure is shared with other modules */
const EAcTrfcType WMEQosTagToACTable[MAX_NUM_OF_802_1d_TAGS] =
{QOS_AC_BE, QOS_AC_BK, QOS_AC_BK, QOS_AC_BE, QOS_AC_VI, QOS_AC_VI, QOS_AC_VO, QOS_AC_VO};

/*
 *  Local definitions:
 */

/* Spare blocks written in extraMemBlks field in TxDescriptor for HW use */
#define BLKS_HW_ALLOC_SPARE             2

/* Set queue's backpressure bit (indicates queue state changed from ready to busy or inversely). */
#define SET_QUEUE_BACKPRESSURE(pBackpressure, uQueueId)   (*pBackpressure |= (1 << uQueueId))

/* Callback function definition for UpdateBusyMap */
typedef void (* tUpdateBusyMapCb)(TI_HANDLE hCbHndl, TI_UINT32 uBackpressure, TI_UINT32 uLinkBackpressure);

#ifdef AP_MODE_ENABLED
/* Set/Reset Link's backpressure bit (indicates link state changed from ready to busy or inversely). */
#define SET_LINK_BACKPRESSURE(pLinkBackpressure, uHlid)   (*pLinkBackpressure |= (1 << uHlid))
#define RESET_LINK_BACKPRESSURE(pLinkBackpressure, uHlid)   (*pLinkBackpressure &= ~(1 << uHlid))

/* Per Link blocks count */
#define HW_RESOURCES_MAX_FW_LINK_BLOCKS     (8*9)  /* MAX blocks per link - for 8 packets */
#define HW_RESOURCES_MIN_FW_LINK_BLOCKS     (2*9)  /* MIN blocks per link - for 2 packets, used in power save */
typedef struct {
	TI_UINT32       uFwMax;             /* Max mem blocks per link */
	TI_UINT8        uFwFree;            /* Total mem blocks freed by firmware - must be 8 bits as in firmware */
	TI_UINT8        uHostAlloc;         /* Total mem blocks allocated by host driver - must be 8 bits as in firmware */
	TI_UINT32       uLastAllocNumBlks;  /* Number of HW blocks that was last allocateed. - used in freeResources to stop link if FwMax changhed */
	TI_BOOL         bBusy;              /* Link is busy */
	TI_UINT32       uNumBlksCausedBusy; /* Number of HW blocks that caused link busy state. - used in freeResources to restart the link */
	TI_BOOL         bEnabled;           /* Link is enabled */

} TTxHwLinkInfo;
#endif

/* Per Queue HW blocks accounting data: */
typedef struct {
	TI_UINT32  uNumBlksThresh;          /* Minimum HW blocks that must be reserved for this Queue. */
	TI_UINT32  uNumBlksUsed;            /* Number of HW blocks that are currently allocated for this Queue. */
	TI_UINT32  uNumBlksReserved;        /* Number of HW blocks currently reserved for this Queue (to guarentee the low threshold). */
	TI_UINT32  uAllocatedBlksCntr;      /* Accumulates allocated blocks for FW freed-blocks counter coordination. */
	TI_UINT32  uFwFreedBlksCntr;        /* Accumulated freed blocks in FW. */
	TI_UINT32  uNumBlksCausedBusy;      /* Number of HW blocks that caused queue busy state. */
	TI_BOOL    bQueueBusy;              /* If TI_TRUE, this queue is currently stopped. */
	TI_UINT16  uPercentOfBlkLowThresh;  /* Configured percentage of blocks to use as the queue's low allocation threshold */
	TI_UINT16  uPercentOfBlkHighThresh; /* Configured percentage of blocks to use as the queue's high allocation threshold */

} TTxHwQueueInfo;

typedef struct {
	TI_HANDLE  hOs;
	TI_HANDLE  hReport;

	tUpdateBusyMapCb fUpdateBusyMapCb;  /* The upper layers UpdateBusyMap callback */
	TI_HANDLE        hUpdateBusyMapHndl;/* The handle for the fUpdateBusyMapCb */

	TI_UINT32  uNumTotalBlks;           /* The total number of Tx blocks        */
	TI_UINT32  uNumTotalBlksFree;       /* Total number of free HW blocks       */
	TI_UINT32  uNumTotalBlksReserved;   /* Total number of free but reserved HW blocks */
	TI_UINT32  uNumUsedDescriptors;     /* Total number of packets in the FW. */
	TI_UINT8   uFwTxResultsCntr;        /* Accumulated freed descriptors in FW. */
	TI_UINT8   uDrvTxPacketsCntr;       /* Accumulated allocated descriptors in driver. */

	TTxHwQueueInfo  aTxHwQueueInfo[MAX_NUM_OF_AC]; /* The per queue variables */
	TTxHwLinkInfo   aTxHwLinkInfo[WLANLINKS_MAX_LINKS]; /* The per link variables */
	TI_UINT32  uNumTotalLinks;                   /* Number of currently connected stations*/

} TTxHwQueue;


static void      txHwQueue_UpdateFreeBlocks (TTxHwQueue *pTxHwQueue, TI_UINT32 uQueueId, TI_UINT32 uFreeBlocks);
static TI_UINT32 txHwQueue_CheckResources (TTxHwQueue *pTxHwQueue, TTxHwQueueInfo *pQueueInfo);
#ifdef AP_MODE_ENABLED
static TI_UINT32 txHwQueue_CheckLinkResources (TTxHwQueue *pTxHwQueue, TTxHwLinkInfo *pLinkInfo);
#endif


/****************************************************************************
 *                      txHwQueue_Create()
 ****************************************************************************
 * DESCRIPTION: Create the Tx buffers pool object
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: The Created object
 ****************************************************************************/
TI_HANDLE txHwQueue_Create (TI_HANDLE hOs)
{
	TTxHwQueue *pTxHwQueue;

	pTxHwQueue = os_memoryAlloc(hOs, sizeof(TTxHwQueue));
	if (pTxHwQueue == NULL) {
		return NULL;
	}

	os_memoryZero(hOs, pTxHwQueue, sizeof(TTxHwQueue));

	pTxHwQueue->hOs = hOs;

	return (TI_HANDLE)pTxHwQueue;
}

/****************************************************************************
 *                      txHwQueue_Destroy()
 ****************************************************************************
 * DESCRIPTION: Destroy the Tx buffers pool object
 *
 * INPUTS:  hTxHwQueue - The object to free
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS txHwQueue_Destroy (TI_HANDLE hTxHwQueue)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;

	if (pTxHwQueue) {
		os_memoryFree(pTxHwQueue->hOs, pTxHwQueue, sizeof(TTxHwQueue));
	}
	return TI_OK;
}




/****************************************************************************
 *               txHwQueue_Init()
 ****************************************************************************

  DESCRIPTION:  Initialize module handles.

 ****************************************************************************/
TI_STATUS txHwQueue_Init (TI_HANDLE hTxHwQueue, TI_HANDLE hReport)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;

	pTxHwQueue->hReport = hReport;

	return TI_OK;
}


/****************************************************************************
 *                      txHwQueue_Config()
 ****************************************************************************
 * DESCRIPTION: Configure the Tx buffers pool object
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS:
 ****************************************************************************/
TI_STATUS txHwQueue_Config (TI_HANDLE hTxHwQueue, TTwdInitParams *pInitParams)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	TI_UINT32   TxQid;

	/* Configure queue parameters to Tx-HW queue module */
	for (TxQid = 0; TxQid < MAX_NUM_OF_AC; TxQid++) {
		pTxHwQueue->aTxHwQueueInfo[TxQid].uNumBlksThresh = pInitParams->tGeneral.TxBlocksThresholdPerAc[TxQid];
	}

	return TI_OK;
}



/****************************************************************************
 *                  txHwQueue_SetHwInfo()
 ****************************************************************************

  DESCRIPTION:

    Called after the HW configuration in the driver init or recovery process.
    Configure Tx HW information, including Tx-HW-blocks number, and per queue
      Tx-descriptors number. Than, restart the module variables.

    Two thresholds are defined per queue:
    a)  TxBlocksLowPercentPerQueue[queue] - The lower threshold is the minimal number of
        Tx blocks guaranteed for each queue.
        The sum of all low thresholds should be less than 100%.
    b)  TxBlocksHighPercentPerQueue[queue] - The higher threshold is the maximal number of
        Tx blocks that may be allocated to the queue.
        The extra blocks above the low threshold can be allocated when needed only
        if they are currently available and are not needed in order to guarantee
        the other queues low threshold.
        The sum of all high thresholds should be more than 100%.
 ****************************************************************************/
TI_STATUS txHwQueue_SetHwInfo (TI_HANDLE hTxHwQueue, TDmaParams *pDmaParams)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;

	pTxHwQueue->uNumTotalBlks = pDmaParams->NumTxBlocks - 1; /* One block must be always free for FW use. */

	/* Restart the module variables. */
	txHwQueue_Restart (hTxHwQueue);

	return TI_OK;
}


/****************************************************************************
 *               txHwQueue_Restart()
 ****************************************************************************
   DESCRIPTION:
   ============
     Called after the HW configuration in the driver init or recovery process.
     Restarts the Tx-HW-Queue module.
 ****************************************************************************/
TI_STATUS txHwQueue_Restart (TI_HANDLE hTxHwQueue)
{
	TTxHwQueue     *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	TTxHwQueueInfo *pQueueInfo;
	TI_UINT32       TxQid;
	TTxHwLinkInfo  *pLinkInfo;
	TI_UINT32       uHlid;


	/*
	 * All blocks are free at restart.
	 * Note that free means all blocks that are currently not in use, while reserved are
	 *   a part of the free blocks that are the summary of all queues reserved blocks.
	 * Each queue may take from the reserved part only up to its own reservation (according to
	 *   its low threshold).
	 */
	pTxHwQueue->uNumTotalBlksFree = pTxHwQueue->uNumTotalBlks;
	pTxHwQueue->uNumTotalBlksReserved = 0;
	pTxHwQueue->uNumUsedDescriptors = 0;
	pTxHwQueue->uFwTxResultsCntr = 0;
	pTxHwQueue->uDrvTxPacketsCntr = 0;
	pTxHwQueue->uNumTotalLinks = 0;

	/* Info per queue */
	for (TxQid = 0; TxQid < MAX_NUM_OF_AC; TxQid++) {
		pQueueInfo = &pTxHwQueue->aTxHwQueueInfo[TxQid];

		pQueueInfo->uNumBlksUsed = 0;
		pQueueInfo->uAllocatedBlksCntr = 0;
		pQueueInfo->uFwFreedBlksCntr = 0;
		pQueueInfo->uNumBlksCausedBusy = 0;
		pQueueInfo->bQueueBusy = TI_FALSE;

		/* Since no blocks are used yet, reserved blocks number equals to the low threshold. */
		pQueueInfo->uNumBlksReserved = pQueueInfo->uNumBlksThresh;

		/* Accumulate total reserved blocks. */
		pTxHwQueue->uNumTotalBlksReserved += pQueueInfo->uNumBlksReserved;
	}

	/* Info per link */
	for (uHlid = 0; uHlid < WLANLINKS_MAX_LINKS; uHlid++) {
		pLinkInfo = &pTxHwQueue->aTxHwLinkInfo[uHlid];

		pLinkInfo->bBusy = TI_FALSE;
		pLinkInfo->uNumBlksCausedBusy = 0;
		pLinkInfo->bEnabled = TI_FALSE;
		pLinkInfo->uHostAlloc = 0;
		pLinkInfo->uNumBlksCausedBusy = 0;
		pLinkInfo->uFwFree = 0;
		pLinkInfo->uFwMax = HW_RESOURCES_MAX_FW_LINK_BLOCKS; /* default is MAX mem blocks, in power save it will be MIN - 18 mem blocks for 2 packets */
	}

	return TI_OK;
}


/****************************************************************************
 *                  txHwQueue_AllocResources()
 ****************************************************************************
 * DESCRIPTION:
   ============
    1.  Estimate required HW-blocks number.
    2.1  If the required blocks are not available for the link or no free descriptor,
            return  link STOP_CURRENT  (to stop current link and requeue the packet).
    2.2  If the required blocks are not available for the queue or no free descriptor,
            return  queue STOP_CURRENT  (to stop current queue and requeue the packet).
    3.  Resources are available so update allocated blocks and descriptors counters.
    4.1  If no link resources for another similar packet, set STOP_NEXT (to stop current link).
    4.2  If no queue resources for another similar packet, set STOP_NEXT (to stop current queue).
        Else, return SUCCESS
 ****************************************************************************/
ETxHwQueStatus txHwQueue_AllocResources (TI_HANDLE hTxHwQueue, TTxCtrlBlk *pTxCtrlBlk)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	TI_UINT32 uNumBlksToAlloc; /* The number of blocks required for the current packet. */
	TI_UINT32 uExcludedLength; /* The data length not included in the rough blocks calculation */
	TI_UINT32 uAvailableBlks;  /* Max blocks that are currently available for this queue. */
	TI_UINT32 uReservedBlks;   /* How many blocks are reserved for this queue before this allocation. */
	TI_UINT32 uQueueId = WMEQosTagToACTable[pTxCtrlBlk->tTxDescriptor.tid];
	TTxHwQueueInfo *pQueueInfo = &(pTxHwQueue->aTxHwQueueInfo[uQueueId]);
	ETxHwQueStatus retStatus = TX_HW_QUE_STATUS_SUCCESS;
	TI_UINT32 uHlid = pTxCtrlBlk->tTxDescriptor.hlid;
#ifdef AP_MODE_ENABLED
	TTxHwLinkInfo *pLinkInfo = &(pTxHwQueue->aTxHwLinkInfo[uHlid]);
	TI_UINT32 uAvailableLinkBlks;  /* Max blocks that are currently available for this link. */
#endif

	/***********************************************************************/
	/*  Calculate packet required HW blocks.                               */
	/***********************************************************************/

	/* Divide length by 256 instead of 252 (block size) to save CPU */
	uNumBlksToAlloc = ( pTxCtrlBlk->tTxDescriptor.length + 20 ) >> 8;

	/* The length not yet included in the uNumBlksToAlloc is the sum of:
	    1) 4 bytes per block as a result of using 256 instead of 252 block size.
	    2) The remainder of the division by 256.
	    3) Overhead due to header translation, security and LLC header (subtracting ethernet header).
	*/
	uExcludedLength = (uNumBlksToAlloc << 2) + ((pTxCtrlBlk->tTxDescriptor.length + 20) & 0xFF) + MAX_HEADER_SIZE - 14;

	/* Add 1 or 2 blocks for the excluded length, according to its size */
	uNumBlksToAlloc += (uExcludedLength > 252) ? 2 : 1;

	/* Add extra blocks needed in case of fragmentation */
	uNumBlksToAlloc += BLKS_HW_ALLOC_SPARE;


#ifdef AP_MODE_ENABLED
	/*
	 * Check LINK resources and then AC resources
	 * In case of no resources for the link, return STOP CURRENT and don't continue to check AC resources
	 */

	/***********************************************************************/
	/*            Check link resources                                     */
	/***********************************************************************/

	/* Find max available blocks for this link (0 could indicate no descriptors). */
	uAvailableLinkBlks = txHwQueue_CheckLinkResources (pTxHwQueue, pLinkInfo);

	/* If we need more blocks than available, return  STOP_CURRENT (stop current link and requeue packet). */
	if (uNumBlksToAlloc > uAvailableLinkBlks) {
		TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": No link resources, Link=%d, ReqBlks=%d, FreeBlks=%d, UsedBlks=%d, uFwMax=%d, uAvailableLinkBlks=%d\n",
		uHlid, uNumBlksToAlloc, pLinkInfo->uFwFree, pLinkInfo->uHostAlloc, pLinkInfo->uFwMax, uAvailableLinkBlks);
		pLinkInfo->uNumBlksCausedBusy = uNumBlksToAlloc; /* used as prediction in free resources */
		pLinkInfo->bBusy = TI_TRUE;
		return TX_HW_LINK_STATUS_STOP_CURRENT;  /**** Exit! (we should stop link and requeue packet) ****/
	}
#endif

	/***********************************************************************/
	/*            Check if the required resources are available            */
	/***********************************************************************/

	/* Find max available blocks for this queue (0 could indicate no descriptors). */
	uAvailableBlks = txHwQueue_CheckResources (pTxHwQueue, pQueueInfo);

	/* If we need more blocks than available, return  STOP_CURRENT (stop current queue and requeue packet). */
	if (uNumBlksToAlloc > uAvailableBlks) {
		TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": No resources, Queue=%d, ReqBlks=%d, FreeBlks=%d, UsedBlks=%d, AvailBlks=%d, UsedPkts=%d\n", uQueueId, uNumBlksToAlloc, pTxHwQueue->uNumTotalBlksFree, pQueueInfo->uNumBlksUsed, uAvailableBlks, pTxHwQueue->uNumUsedDescriptors);
		pQueueInfo->uNumBlksCausedBusy = uNumBlksToAlloc;
		pQueueInfo->bQueueBusy = TI_TRUE;

		return TX_HW_QUE_STATUS_STOP_CURRENT;  /**** Exit! (we should stop queue and requeue packet) ****/
	}

	/***********************************************************************/
	/*                    Allocate required resources                      */
	/***********************************************************************/

	/* Update blocks numbers in Tx descriptor */
	pTxCtrlBlk->tTxDescriptor.extraMemBlks = BLKS_HW_ALLOC_SPARE;
	pTxCtrlBlk->tTxDescriptor.totalMemBlks = uNumBlksToAlloc;

	/* Update packet allocation info:  */
	pTxHwQueue->uNumUsedDescriptors++; /* Update number of packets in FW (for descriptors allocation check). */
	pTxHwQueue->uDrvTxPacketsCntr++;
	pQueueInfo->uAllocatedBlksCntr += uNumBlksToAlloc; /* For FW counter coordination. */
	uReservedBlks = pQueueInfo->uNumBlksReserved;

	/* If we are currently using less than the low threshold (i.e. we have some reserved blocks),
	    blocks allocation should reduce the reserved blocks number as follows:
	*/
	if (uReservedBlks) {

		/* If adding the allocated blocks to the used blocks will pass the low-threshold,
		    only the part up to the low-threshold is subtracted from the reserved blocks.
		    This is because blocks are reserved for the Queue only up to its low-threshold.

		      0   old used                    low      new used       high
		      |######|                         |          |            |
		      |######|                         |          |            |
		              <------------ allocated ----------->
		              <----- old reserved ---->
		                     new reserved = 0     (we passed the low threshold)
		*/
		if (uNumBlksToAlloc > uReservedBlks) {
			pQueueInfo->uNumBlksReserved = 0;
			pTxHwQueue->uNumTotalBlksReserved -= uReservedBlks; /* reduce change from total reserved.*/
		}


		/* Else, if allocating less than reserved,
		    the allocated blocks are subtracted from the reserved blocks:

		      0   old used       new used               low      high
		      |######|               |                   |        |
		      |######|               |                   |        |
		              <- allocated ->
		              <--------- old reserved ---------->
		                             <-- new reserved -->
		*/
		else {
			pQueueInfo->uNumBlksReserved -= uNumBlksToAlloc;
			pTxHwQueue->uNumTotalBlksReserved -= uNumBlksToAlloc; /* reduce change from total reserved.*/
		}
	}


	/* Update total free blocks and Queue used blocks with the allocated blocks number. */
	pTxHwQueue->uNumTotalBlksFree -= uNumBlksToAlloc;
	pQueueInfo->uNumBlksUsed += uNumBlksToAlloc;

	TRACE7(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": SUCCESS,  Link=%d, Queue=%d, Req-blks=%d , Free=%d, Used=%d, Reserved=%d, Accumulated=%d\n", uHlid, uQueueId, uNumBlksToAlloc, pTxHwQueue->uNumTotalBlksFree, pQueueInfo->uNumBlksUsed, pQueueInfo->uNumBlksReserved, pQueueInfo->uAllocatedBlksCntr);

#ifdef AP_MODE_ENABLED

	/* Update Link used blocks with the allocated blocks number. */
	pLinkInfo->uLastAllocNumBlks = uNumBlksToAlloc; /* used as prediction in free resources */
	pLinkInfo->uHostAlloc += uNumBlksToAlloc;
	/*
	 * At this stage, the resources for this LINK and AC are allocated successfully
	 * Need to check if there is a place for anothoer similar packet (LINK and AC resources)
	 * return bitmap of STOP NEXT, for link if no link resources and for AC if no queue resources
	 */

	/* If no resources for another similar packet, return STOP_NEXT (to stop current link). */
	/* Note: Current packet transmission is continued */
	if ( (uNumBlksToAlloc << 1) > uAvailableLinkBlks ) {
		TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": No link resources for next packet, Link=%d, ReqBlks=%d, FreeBlks=%d, UsedBlks=%d, uFwMax=%d, uAvailableLinkBlks=%d\n",
		uHlid, uNumBlksToAlloc, pLinkInfo->uFwFree, pLinkInfo->uHostAlloc, pLinkInfo->uFwMax, uAvailableLinkBlks);
		pLinkInfo->bBusy = TI_TRUE;
		retStatus |= TX_HW_LINK_STATUS_STOP_NEXT;
	}
#endif

	/* If no resources for another similar packet, return STOP_NEXT (to stop current queue). */
	/* Note: Current packet transmission is continued */
	if ( (uNumBlksToAlloc << 1) > uAvailableBlks ) {
		TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": No resources for next pkt, Queue=%d, ReqBlks=%d, FreeBlks=%d, UsedBlks=%d, AvailBlks=%d, UsedPkts=%d\n", uQueueId, uNumBlksToAlloc, pTxHwQueue->uNumTotalBlksFree, pQueueInfo->uNumBlksUsed, uAvailableBlks, pTxHwQueue->uNumUsedDescriptors);
		pQueueInfo->uNumBlksCausedBusy = uNumBlksToAlloc;
		pQueueInfo->bQueueBusy = TI_TRUE;
		retStatus |= TX_HW_QUE_STATUS_STOP_NEXT;
	}

	return retStatus;
}


/****************************************************************************
 *                  txHwQueue_UpdateFreeBlocks()
 ****************************************************************************
 * DESCRIPTION:
   ===========
    This function is called per queue after reading the freed blocks counters from the FwStatus.
    It updates the queue's blocks status according to the freed blocks.
 ****************************************************************************/
static void txHwQueue_UpdateFreeBlocks (TTxHwQueue *pTxHwQueue, TI_UINT32 uQueueId, TI_UINT32 uFreeBlocks)
{
	TTxHwQueueInfo *pQueueInfo = &(pTxHwQueue->aTxHwQueueInfo[uQueueId]);
	TI_UINT32 lowThreshold;  /* Minimum blocks that are guaranteed for this Queue. */
	TI_UINT32 newUsedBlks;   /* Blocks that are used by this Queue after updating free blocks. */
	TI_UINT32 newReserved;   /* How many blocks are reserved to this Queue after freeing. */
	TI_UINT32 numBlksToFree; /* The number of blocks freed in the current queue. */

	/* If the FW free blocks counter didn't change, exit */
	uFreeBlocks = ENDIAN_HANDLE_LONG(uFreeBlocks);
	if (uFreeBlocks == pQueueInfo->uFwFreedBlksCntr) {
		return;
	}

	pQueueInfo->uFwFreedBlksCntr = uFreeBlocks;

	/* The uFreeBlocks is the accumulated number of blocks freed by the FW for the uQueueId.
	 * Subtracting it from the accumulated number of blocks allocated by the driver should
	 *   give the current number of used blocks in this queue.
	 * Since the difference is always a small positive number, a simple subtraction should work
	 *   also for wrap around.
	 */
	newUsedBlks = pQueueInfo->uAllocatedBlksCntr - uFreeBlocks;

	numBlksToFree = pQueueInfo->uNumBlksUsed - newUsedBlks;

#ifdef TI_DBG   /* Sanity check: make sure we don't free more than is allocated. */
	if (numBlksToFree > pQueueInfo->uNumBlksUsed) {
		TRACE5(pTxHwQueue->hReport, REPORT_SEVERITY_ERROR, ":  Try to free more blks than used: Queue %d, ToFree %d, Used %d, HostAlloc=0x%x, FwFree=0x%x\n", uQueueId, numBlksToFree, pQueueInfo->uNumBlksUsed, pQueueInfo->uAllocatedBlksCntr, uFreeBlocks);
	}
#endif

	/* Update total free blocks and Queue used blocks with the freed blocks number. */
	pTxHwQueue->uNumTotalBlksFree += numBlksToFree;
	pQueueInfo->uNumBlksUsed = newUsedBlks;

	lowThreshold = pQueueInfo->uNumBlksThresh;

	/* If after freeing the blocks we are using less than the low threshold,
	    update total reserved blocks number as follows:
	   (note: if we are above the low threshold after freeing the blocks we still have no reservation.)
	*/
	if (newUsedBlks < lowThreshold) {
		newReserved = lowThreshold - newUsedBlks;
		pQueueInfo->uNumBlksReserved = newReserved;


		/* If freeing the blocks reduces the used blocks from above to below the low-threshold,
		    only the part from the low-threshold to the new used number is added to the
		    reserved blocks (because blocks are reserved for the Queue only up to its low-threshold):

		      0        new used               low            old used         high
		      |###########|####################|################|             |
		      |###########|####################|################|             |
		                   <-------------- freed -------------->
		                   <-- new reserved -->
		                     old reserved = 0
		*/
		if (numBlksToFree > newReserved)
			pTxHwQueue->uNumTotalBlksReserved += newReserved; /* Add change to total reserved.*/


		/* Else, if we were under the low-threshold before freeing these blocks,
		    all freed blocks are added to the reserved blocks:

		      0             new used          old used             low               high
		      |################|#################|                  |                  |
		      |################|#################|                  |                  |
		                        <---- freed ---->
		                                          <- old reserved ->
		                        <---------- new reserved ---------->
		*/
		else
			pTxHwQueue->uNumTotalBlksReserved += numBlksToFree; /* Add change to total reserved.*/
	}

	TRACE5(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ":  Queue %d, ToFree %d, Used %d, HostAlloc=0x%x, FwFree=0x%x\n", uQueueId, numBlksToFree, pQueueInfo->uNumBlksUsed, pQueueInfo->uAllocatedBlksCntr, uFreeBlocks);
}


/****************************************************************************
 *                  txHwQueue_UpdateFreeResources()
 ****************************************************************************
 * DESCRIPTION:
   ===========
   Called by FwEvent upon Data interrupt to update freed HW-Queue resources as follows:
    1) For all queues, update blocks and descriptors numbers according to FwStatus information.
    2) For each busy queue, if now available indicate it in the backpressure bitmap.
 ****************************************************************************/
ETxnStatus txHwQueue_UpdateFreeResources (TI_HANDLE hTxHwQueue, FwStatus_t *pFwStatus)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	TTxHwQueueInfo *pQueueInfo;
	TI_UINT32 uQueueId;
	TI_UINT32 uAvailableBlks; /* Max blocks available for current queue. */
	TI_UINT32 uNewNumUsedDescriptors;
	TI_UINT32 uBackpressure = 0;
	TI_UINT32 *pFreeQueueBlocks = (TI_UINT32 *)pFwStatus->txReleasedBlks;
	TI_UINT32 uTempFwCounters;
	FwStatCntrs_t *pFwStatusCounters;
	TI_BOOL uLinkInfoChanged = TI_FALSE;
	TI_UINT32 uLinkBackpressure = 0;
#ifdef AP_MODE_ENABLED
	TI_UINT32 uHlid;
#endif

	/*
	 * If TxResults counter changed in FwStatus, update descriptors number according to  information
	 */
	uTempFwCounters = (ENDIAN_HANDLE_LONG(pFwStatus->counters));
	pFwStatusCounters = (FwStatCntrs_t *)&uTempFwCounters;
	if (pFwStatusCounters->txResultsCntr != pTxHwQueue->uFwTxResultsCntr) {
		pTxHwQueue->uFwTxResultsCntr = pFwStatusCounters->txResultsCntr;

		/* Calculate new number of used descriptors (the else is for wrap around case) */
		if (pTxHwQueue->uFwTxResultsCntr <= pTxHwQueue->uDrvTxPacketsCntr) {
			uNewNumUsedDescriptors = (TI_UINT32)(pTxHwQueue->uDrvTxPacketsCntr - pTxHwQueue->uFwTxResultsCntr);
		} else {
			uNewNumUsedDescriptors = 0x100 - (TI_UINT32)(pTxHwQueue->uFwTxResultsCntr - pTxHwQueue->uDrvTxPacketsCntr);
		}

#ifdef TI_DBG   /* Sanity check: make sure we don't free more descriptors than allocated. */
		if (uNewNumUsedDescriptors >= pTxHwQueue->uNumUsedDescriptors) {
			TRACE2(pTxHwQueue->hReport, REPORT_SEVERITY_ERROR, ":  Used descriptors number should decrease: UsedDesc %d, NewUsedDesc %d\n", pTxHwQueue->uNumUsedDescriptors, uNewNumUsedDescriptors);
		}
#endif

		/* Update number of packets left in FW (for descriptors allocation check). */
		pTxHwQueue->uNumUsedDescriptors = uNewNumUsedDescriptors;
	}

#ifdef AP_MODE_ENABLED
	/*
	 * In case of change in FwFree or FwMax
	 * NO_LINK_RESOURCES(hlid) =  (LinksTxBlks[hlid].HostAlloc - LinksTxBlks[hlid].FwFree) > LinksTxBlks[hlid].FwMax
	 * Check if now there are resources and LinkBusy was TRUE and set backpressure bitmap for the link to FALSE
	 * Check if now there is no rssources and LinkBusy was FALSE and set backpressure bitmap for the link to TRUE
	 * Call the updateBusyMap callback with AC backpressure and Link backpressure bitmaps as parameters to start/stop links queue
	 */
	for (uHlid = 0; uHlid < WLANLINKS_MAX_LINKS; uHlid++) {
		TTxHwLinkInfo *pLinkInfo = &(pTxHwQueue->aTxHwLinkInfo[uHlid]);
		TI_UINT8 new_FwFree = pFwStatus->txLinkFreeBlks[uHlid];
		TI_UINT8 new_FwMax;
		TI_UINT32 uAvailableLinkBlks;

		if((pFwStatus->linkPsBitmap >> uHlid) & 1) {
			if(pTxHwQueue->uNumTotalLinks == 1) {
				new_FwMax = HW_RESOURCES_MAX_FW_LINK_BLOCKS;
			} else {
				new_FwMax = HW_RESOURCES_MIN_FW_LINK_BLOCKS;
			}
		} else {
			new_FwMax = HW_RESOURCES_MAX_FW_LINK_BLOCKS;
		}

		/* set initial backbressure value */
		if (pLinkInfo->bBusy)
			RESET_LINK_BACKPRESSURE(&uLinkBackpressure, uHlid); /* Init link as stopped. */
		else
			SET_LINK_BACKPRESSURE(&uLinkBackpressure, uHlid); /* Init link as started. */

		/* Check if there is a change in link info */
		if ((pLinkInfo->uFwFree != new_FwFree) || (pLinkInfo->uFwMax != new_FwMax)) {
			uLinkInfoChanged = TI_TRUE;
			pLinkInfo->uFwFree = new_FwFree;
			pLinkInfo->uFwMax = new_FwMax;

			/* Find max available blocks for this link */
			uAvailableLinkBlks = txHwQueue_CheckLinkResources (pTxHwQueue, pLinkInfo);

			/* If the link was stopped */
			if (pLinkInfo->bBusy) {
				/* Link was stopped and now, there are resources, set the link's backpressure bit to indicate NOT-BUSY */
				if (uAvailableLinkBlks > pLinkInfo->uNumBlksCausedBusy) {
					TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": Link Available, Link=%d, ReqBlks=%d, FwFree=%d, HostAlloc=%d, AvailBlks=%d, FwMax=%d\n", uHlid, pLinkInfo->uNumBlksCausedBusy, pLinkInfo->uFwFree, pLinkInfo->uHostAlloc, uAvailableLinkBlks, pLinkInfo->uFwMax);
					SET_LINK_BACKPRESSURE(&uLinkBackpressure, uHlid); /* Start link. */
					pLinkInfo->bBusy = TI_FALSE;
				}
			} else {
				/* Link was not busy and now, there are no resources(FwMax was changed), set the link's backpressure bit to indicate BUSY */
				if (pLinkInfo->uLastAllocNumBlks > uAvailableLinkBlks) {
					TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": Link NOT Available, Link=%d, ReqBlks=%d, FwFree=%d, HostAlloc=%d, AvailBlks=%d, FwMax=%d\n", uHlid, pLinkInfo->uNumBlksCausedBusy, pLinkInfo->uFwFree, pLinkInfo->uHostAlloc, uAvailableLinkBlks, pLinkInfo->uFwMax);
					RESET_LINK_BACKPRESSURE(&uLinkBackpressure, uHlid); /* Stop link. */
					pLinkInfo->uNumBlksCausedBusy = uAvailableLinkBlks;
					pLinkInfo->bBusy = TI_TRUE;
				}
			}
		}
	}
#endif

	/*
	 * For all queues, update blocks numbers according to FwStatus information
	 */
	for (uQueueId = 0; uQueueId < MAX_NUM_OF_AC; uQueueId++) {
		/* Update per queue number of used, free and reserved blocks. */
		txHwQueue_UpdateFreeBlocks (pTxHwQueue, uQueueId, pFreeQueueBlocks[uQueueId]);
	}

	/*
	 * For each busy queue, if now available indicate it in the backpressure bitmap
	 */
	for (uQueueId = 0; uQueueId < MAX_NUM_OF_AC; uQueueId++) {
		pQueueInfo = &(pTxHwQueue->aTxHwQueueInfo[uQueueId]);

		/* If the queue was stopped */
		if (pQueueInfo->bQueueBusy) {
			/* Find max available blocks for this queue (0 could indicate no descriptors). */
			uAvailableBlks = txHwQueue_CheckResources (pTxHwQueue, pQueueInfo);

			/* If the required blocks and a descriptor are available,
			     set the queue's backpressure bit to indicate NOT-busy! */
			if (pQueueInfo->uNumBlksCausedBusy <= uAvailableBlks) {
				TRACE6(pTxHwQueue->hReport, REPORT_SEVERITY_INFORMATION, ": Queue Available, Queue=%d, ReqBlks=%d, FreeBlks=%d, UsedBlks=%d, AvailBlks=%d, UsedPkts=%d\n", uQueueId, pQueueInfo->uNumBlksCausedBusy, pTxHwQueue->uNumTotalBlksFree, pQueueInfo->uNumBlksUsed, uAvailableBlks, pTxHwQueue->uNumUsedDescriptors);
				SET_QUEUE_BACKPRESSURE(&uBackpressure, uQueueId); /* Start queue. */
				pQueueInfo->bQueueBusy = TI_FALSE;
			}
		}
	}

	/* If released queues map is not 0 or link backpressure changed, send it to the upper layers (if CB available) */
	if (((uBackpressure > 0) || uLinkInfoChanged) && (pTxHwQueue->fUpdateBusyMapCb != NULL)) {
		pTxHwQueue->fUpdateBusyMapCb (pTxHwQueue->hUpdateBusyMapHndl, uBackpressure, uLinkBackpressure);
	}

	return TXN_STATUS_COMPLETE;
}

/****************************************************************************
 *                  txHwQueue_AddLink()
 ****************************************************************************
 * DESCRIPTION:
 ===========
 Increment counter of currently active data links
 ****************************************************************************/
void  txHwQueue_AddLink(TI_HANDLE hTxHwQueue)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	pTxHwQueue->uNumTotalLinks++;
}

/****************************************************************************
 *                  txHwQueue_RemoveLink()
 ****************************************************************************
 * DESCRIPTION:
 ===========
 Deccrement counter of currently active data links
 ****************************************************************************/
void  txHwQueue_RemoveLink(TI_HANDLE hTxHwQueue)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;

	if(pTxHwQueue->uNumTotalLinks) { /* Sanity check */
		pTxHwQueue->uNumTotalLinks--;
	}

}

/****************************************************************************
 *                  txHwQueue_CheckResources()
 ****************************************************************************
 * DESCRIPTION:
   ============
    Return the given queue's available blocks.
    If no descriptors available, return 0.
 ****************************************************************************/
static TI_UINT32 txHwQueue_CheckResources (TTxHwQueue *pTxHwQueue, TTxHwQueueInfo *pQueueInfo)
{
	/* If descriptors are available: */
	if (pTxHwQueue->uNumUsedDescriptors < NUM_TX_DESCRIPTORS) {
		/* Calculate how many buffers are available for this Queue: the total free buffers minus the buffers
		     that are reserved for other Queues (all reserved minus this Queue's reserved). */
		return (pTxHwQueue->uNumTotalBlksFree - (pTxHwQueue->uNumTotalBlksReserved - pQueueInfo->uNumBlksReserved));
	}

	/* If no descriptors are available, return 0 (can't transmit anything). */
	else {
		return 0;
	}
}

#ifdef AP_MODE_ENABLED
/****************************************************************************
 *                  txHwQueue_CheckLinkResources()
 ****************************************************************************
 * DESCRIPTION:
   ============
    Return the given link's available blocks.
    If no descriptors available, return 0.
 ****************************************************************************/
static TI_UINT32 txHwQueue_CheckLinkResources (TTxHwQueue *pTxHwQueue, TTxHwLinkInfo *pLinkInfo)
{
	int availabaleLinkBlocks;

	/* If descriptors are available: */
	if (pTxHwQueue->uNumUsedDescriptors < NUM_TX_DESCRIPTORS) {
		availabaleLinkBlocks = pLinkInfo->uFwMax - (pLinkInfo->uHostAlloc - pLinkInfo->uFwFree);
		if (availabaleLinkBlocks <= 0) {
			availabaleLinkBlocks = 0;
		}
	}

	/* If no descriptors are available, return 0 (can't transmit anything). */
	else {
		availabaleLinkBlocks = 0;
	}

	return availabaleLinkBlocks;
}
#endif


/****************************************************************************
 *                      txHwQueue_RegisterCb()
 ****************************************************************************
 * DESCRIPTION:  Register the upper driver TxHwQueue callback functions.
 ****************************************************************************/
void txHwQueue_RegisterCb (TI_HANDLE hTxHwQueue, TI_UINT32 uCallBackId, void *fCbFunc, TI_HANDLE hCbHndl)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;

	switch (uCallBackId) {
	case TWD_INT_UPDATE_BUSY_MAP:
		pTxHwQueue->fUpdateBusyMapCb   = (tUpdateBusyMapCb)fCbFunc;
		pTxHwQueue->hUpdateBusyMapHndl = hCbHndl;
		break;

	default:
		TRACE1(pTxHwQueue->hReport, REPORT_SEVERITY_ERROR, " - Illegal parameter = %d\n", uCallBackId);
		return;
	}
}


/****************************************************************************
 *                      txHwQueue_PrintInfo()
 ****************************************************************************
 * DESCRIPTION: Print the Hw Queue module current information
 ****************************************************************************/
#ifdef TI_DBG
void txHwQueue_PrintInfo (TI_HANDLE hTxHwQueue)
{
	TTxHwQueue *pTxHwQueue = (TTxHwQueue *)hTxHwQueue;
	TI_INT32 TxQid;
	TI_UINT32 uHlid;

	/* Print the Tx-HW-Queue information: */
	WLAN_OS_REPORT(("Hw-Queues Information:\n"));
	WLAN_OS_REPORT(("======================\n"));
	WLAN_OS_REPORT(("Total Blocks:           %d\n", pTxHwQueue->uNumTotalBlks));
	WLAN_OS_REPORT(("Total Free Blocks:      %d\n", pTxHwQueue->uNumTotalBlksFree));
	WLAN_OS_REPORT(("Total Reserved Blocks:  %d\n", pTxHwQueue->uNumTotalBlksReserved));
	WLAN_OS_REPORT(("Total Used Descriptors: %d\n", pTxHwQueue->uNumUsedDescriptors));
	WLAN_OS_REPORT(("FwTxResultsCntr:        %d\n", pTxHwQueue->uFwTxResultsCntr));
	WLAN_OS_REPORT(("DrvTxPacketsCntr:       %d\n", pTxHwQueue->uDrvTxPacketsCntr));

	for(TxQid = 0; TxQid < MAX_NUM_OF_AC; TxQid++) {
		WLAN_OS_REPORT(("Q=%d: Used=%d, Reserve=%d, Threshold=%d\n",
		TxQid,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uNumBlksUsed,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uNumBlksReserved,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uNumBlksThresh));
	}

	WLAN_OS_REPORT(("\n"));

	for(TxQid = 0; TxQid < MAX_NUM_OF_AC; TxQid++) {
		WLAN_OS_REPORT(("Queue=%d: HostAllocCount=%d, FwFreeCount=%d, uNumBlksCausedBusy=%d, Busy=%d\n",
		TxQid,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uAllocatedBlksCntr,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uFwFreedBlksCntr,
		pTxHwQueue->aTxHwQueueInfo[TxQid].uNumBlksCausedBusy,
		pTxHwQueue->aTxHwQueueInfo[TxQid].bQueueBusy));
	}
	WLAN_OS_REPORT(("\n"));
	WLAN_OS_REPORT(("Number of Active Links = %d\n", pTxHwQueue->uNumTotalLinks));

	for(uHlid = 0; uHlid < WLANLINKS_MAX_LINKS; uHlid++) {
		WLAN_OS_REPORT(("Link=%d: HostAllocCount=%3d, FwFreeCount=%3d, FwMax=%3d, Busy=%d, uNumBlksCausedBusy=%d, uLastAllocNumBlks=%d\n",
		uHlid,
		pTxHwQueue->aTxHwLinkInfo[uHlid].uHostAlloc,
		pTxHwQueue->aTxHwLinkInfo[uHlid].uFwFree,
		pTxHwQueue->aTxHwLinkInfo[uHlid].uFwMax,
		pTxHwQueue->aTxHwLinkInfo[uHlid].bBusy,
		pTxHwQueue->aTxHwLinkInfo[uHlid].uNumBlksCausedBusy,
		pTxHwQueue->aTxHwLinkInfo[uHlid].uLastAllocNumBlks));
	}
}


#endif /* TI_DBG */


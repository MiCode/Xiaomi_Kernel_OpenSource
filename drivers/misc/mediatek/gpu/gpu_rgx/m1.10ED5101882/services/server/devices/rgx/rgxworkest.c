/*************************************************************************/ /*!
@File           rgxworkest.c
@Title          RGX Workload Estimation Functionality
@Codingstyle    IMG
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Kernel mode workload estimation functionality.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "rgxworkest.h"
#include "rgxfwutils.h"
#include "rgxdevice.h"
#include "rgxpdvfs.h"
#include "rgx_options.h"
#include "device.h"
#include "pvr_debug.h"

#define ROUND_DOWN_TO_NEAREST_1024(number) (((number) >> 10) << 10)

static inline IMG_BOOL _WorkEstEnabled(void)
{
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	if (psPVRSRVData->sDriverInfo.sKMBuildInfo.ui32BuildOptions &
	    psPVRSRVData->sDriverInfo.sUMBuildInfo.ui32BuildOptions &
	    OPTIONS_WORKLOAD_ESTIMATION_MASK)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static inline IMG_UINT32 _WorkEstDoHash(IMG_UINT32 ui32Input)
{
	IMG_UINT32 ui32HashPart;

	 /* Hash function borrowed from hash.c */
	ui32HashPart = ui32Input;
	ui32HashPart += (ui32HashPart << 12);
	ui32HashPart ^= (ui32HashPart >> 22);
	ui32HashPart += (ui32HashPart << 4);
	ui32HashPart ^= (ui32HashPart >> 9);
	ui32HashPart += (ui32HashPart << 10);
	ui32HashPart ^= (ui32HashPart >> 2);
	ui32HashPart += (ui32HashPart << 7);
	ui32HashPart ^= (ui32HashPart >> 12);

	return ui32HashPart;
}

IMG_BOOL WorkEstHashCompareTA3D(size_t uKeySize, void *pKey1, void *pKey2)
{
	RGX_WORKLOAD_TA3D *psWorkload1;
	RGX_WORKLOAD_TA3D *psWorkload2;
	PVR_UNREFERENCED_PARAMETER(uKeySize);

	if (pKey1 && pKey2)
	{
		psWorkload1 = *((RGX_WORKLOAD_TA3D **)pKey1);
		psWorkload2 = *((RGX_WORKLOAD_TA3D **)pKey2);

		PVR_ASSERT(psWorkload1);
		PVR_ASSERT(psWorkload2);

		if (psWorkload1->ui32RenderTargetSize == psWorkload2->ui32RenderTargetSize
		    && psWorkload1->ui32NumberOfDrawCalls == psWorkload2->ui32NumberOfDrawCalls
		    && psWorkload1->ui32NumberOfIndices == psWorkload2->ui32NumberOfIndices
		    && psWorkload1->ui32NumberOfMRTs == psWorkload2->ui32NumberOfMRTs)
		{
			/* This is added to allow this memory to be freed */
			*(uintptr_t*)pKey2 = *(uintptr_t*)pKey1;
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

IMG_UINT32 WorkEstHashFuncTA3D(size_t uKeySize, void *pKey, IMG_UINT32 uHashTabLen)
{
	RGX_WORKLOAD_TA3D *psWorkload = *((RGX_WORKLOAD_TA3D**)pKey);
	IMG_UINT32 ui32HashKey = 0;
	PVR_UNREFERENCED_PARAMETER(uHashTabLen);
	PVR_UNREFERENCED_PARAMETER(uKeySize);

	/* Hash key predicated on multiple render target attributes */
	ui32HashKey += _WorkEstDoHash(psWorkload->ui32RenderTargetSize);
	ui32HashKey += _WorkEstDoHash(psWorkload->ui32NumberOfDrawCalls);
	ui32HashKey += _WorkEstDoHash(psWorkload->ui32NumberOfIndices);
	ui32HashKey += _WorkEstDoHash(psWorkload->ui32NumberOfMRTs);

	return ui32HashKey;
}

void WorkEstHashLockCreate(POS_LOCK *ppsHashLock)
{
	if (*ppsHashLock == NULL)
	{
		OSLockCreate(ppsHashLock, LOCK_TYPE_DISPATCH);
	}
}

void WorkEstHashLockDestroy(POS_LOCK psWorkEstHashLock)
{
	if (psWorkEstHashLock != NULL)
	{
		OSLockDestroy(psWorkEstHashLock);
		psWorkEstHashLock = NULL;
	}
}

void WorkEstCheckFirmwareCCB(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_WORKEST_FWCCB_CMD *psFwCCBCmd;
	IMG_UINT8 *psFWCCB = psDevInfo->psWorkEstFirmwareCCB;
	RGXFWIF_CCB_CTL *psFWCCBCtl = psDevInfo->psWorkEstFirmwareCCBCtl;

	while (psFWCCBCtl->ui32ReadOffset != psFWCCBCtl->ui32WriteOffset)
	{
		PVRSRV_ERROR eError;

		/* Point to the next command */
		psFwCCBCmd = ((RGXFWIF_WORKEST_FWCCB_CMD *)psFWCCB) + psFWCCBCtl->ui32ReadOffset;

		eError = WorkEstRetire(psDevInfo, psFwCCBCmd);
		PVR_LOG_IF_ERROR(eError, "WorkEstCheckFirmwareCCB: WorkEstRetire failed");

		/* Update read offset */
		psFWCCBCtl->ui32ReadOffset = (psFWCCBCtl->ui32ReadOffset + 1) & psFWCCBCtl->ui32WrapMask;
	}
}

PVRSRV_ERROR WorkEstPrepare(PVRSRV_RGXDEV_INFO        *psDevInfo,
                            WORKEST_HOST_DATA         *psWorkEstHostData,
                            WORKLOAD_MATCHING_DATA    *psWorkloadMatchingData,
                            IMG_UINT32                ui32RenderTargetSize,
                            IMG_UINT32                ui32NumberOfDrawCalls,
                            IMG_UINT32                ui32NumberOfIndices,
                            IMG_UINT32                ui32NumberOfMRTs,
                            IMG_UINT64                ui64DeadlineInus,
                            RGXFWIF_WORKEST_KICK_DATA *psWorkEstKickData)
{
	RGX_WORKLOAD_TA3D     *psWorkloadCharacteristics;
	IMG_UINT64            *pui64CyclePrediction;
	IMG_UINT64            ui64CurrentTime;
	WORKEST_RETURN_DATA   *psReturnData;
	IMG_UINT32            ui32ReturnDataWO;
#if defined(SUPPORT_SOC_TIMER)
	PVRSRV_DEVICE_CONFIG  *psDevConfig;
	IMG_UINT64            ui64CurrentSoCTime;
#endif
	PVRSRV_ERROR          eError = PVRSRV_ERROR_INVALID_PARAMS;

	if (!_WorkEstEnabled())
	{
		/* No error message to avoid excessive messages */
		return PVRSRV_OK;
	}

	/* Validate all required objects required for preparing work estimation */
	PVR_LOGR_IF_FALSE(psDevInfo, "WorkEstPrepare: Device info not available", eError);
	PVR_LOGR_IF_FALSE(psWorkEstHostData, "WorkEstPrepare: Host data not available", eError);
	PVR_LOGR_IF_FALSE(psWorkloadMatchingData, "WorkEstPrepare: Workload Matching Data not available", eError);
	PVR_LOGR_IF_FALSE(psWorkloadMatchingData->psHashLock, "WorkEstPrepare: Hash lock not available", eError);
	PVR_LOGR_IF_FALSE(psWorkloadMatchingData->psHashTable, "WorkEstPrepare: Hash table not available", eError);

#if defined(SUPPORT_SOC_TIMER)
	psDevConfig = psDevInfo->psDeviceNode->psDevConfig;
	ui64CurrentSoCTime = psDevConfig->pfnSoCTimerRead(psDevConfig->hSysData);
#endif

	eError = OSClockMonotonicus64(&ui64CurrentTime);
	PVR_LOGR_IF_ERROR(eError, "WorkEstPrepare: Unable to access System Monotonic clock");

#if defined(SUPPORT_PDVFS)
	psDevInfo->psDeviceNode->psDevConfig->sDVFS.sPDVFSData.bWorkInFrame = IMG_TRUE;
#endif

	/* Select the next index for the return data and update it (is this thread safe?) */
	ui32ReturnDataWO = psDevInfo->ui32ReturnDataWO;
	psDevInfo->ui32ReturnDataWO = (ui32ReturnDataWO + 1) & RETURN_DATA_ARRAY_WRAP_MASK;

	/* Index for the return data passed to/from the firmware. */
	psWorkEstKickData->ui64ReturnDataIndex = ui32ReturnDataWO;
	if (ui64DeadlineInus > ui64CurrentTime)
	{
		/* Rounding is done to reduce multiple deadlines with minor spread flooding the fw workload array. */
#if defined(SUPPORT_SOC_TIMER)
		IMG_UINT64 ui64TimeDelta = (ui64DeadlineInus - ui64CurrentTime) * SOC_TIMER_FREQ;
		psWorkEstKickData->ui64Deadline = ROUND_DOWN_TO_NEAREST_1024(ui64CurrentSoCTime + ui64TimeDelta);
#else
		psWorkEstKickData->ui64Deadline = ROUND_DOWN_TO_NEAREST_1024(ui64DeadlineInus);
#endif
	}
	else
	{
		/* If deadline has already passed, assign zero to suggest full frequency */
		psWorkEstKickData->ui64Deadline = 0;
	}

	/* Set up data for the return path to process the workload; the matching data is needed
	   as it holds the hash data, the host data is needed for completion updates */
	psReturnData = &psDevInfo->asReturnData[ui32ReturnDataWO];
	psReturnData->psWorkloadMatchingData = psWorkloadMatchingData;
	psReturnData->psWorkEstHostData = psWorkEstHostData;

	/* The workload characteristic is needed in the return data for the matching
	   of future workloads via the hash. */
	psWorkloadCharacteristics = &psReturnData->sWorkloadCharacteristics;
	psWorkloadCharacteristics->ui32RenderTargetSize = ui32RenderTargetSize;
	psWorkloadCharacteristics->ui32NumberOfDrawCalls = ui32NumberOfDrawCalls;
	psWorkloadCharacteristics->ui32NumberOfIndices = ui32NumberOfIndices;
	psWorkloadCharacteristics->ui32NumberOfMRTs = ui32NumberOfMRTs;

	/* Acquire the lock to access hash */
	OSLockAcquire(psWorkloadMatchingData->psHashLock);

	/* Check if there is a prediction for this workload */
	pui64CyclePrediction = (IMG_UINT64*) HASH_Retrieve(psWorkloadMatchingData->psHashTable,
													   (uintptr_t)psWorkloadCharacteristics);

	/* Release lock */
	OSLockRelease(psWorkloadMatchingData->psHashLock);

	if (pui64CyclePrediction != NULL)
	{
		/* Cycle prediction is available, store this prediction */
		psWorkEstKickData->ui64CyclesPrediction = *pui64CyclePrediction;
	}
	else
	{
		/* There is no prediction */
		psWorkEstKickData->ui64CyclesPrediction = 0;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR WorkEstRetire(PVRSRV_RGXDEV_INFO *psDevInfo,
						   RGXFWIF_WORKEST_FWCCB_CMD *psReturnCmd)
{
	RGX_WORKLOAD_TA3D      *psWorkloadCharacteristics;
	WORKLOAD_MATCHING_DATA *psWorkloadMatchingData;
	IMG_UINT64             *paui64WorkloadHashData;
	RGX_WORKLOAD_TA3D      *pasWorkloadHashKeys;
	IMG_UINT32             ui32HashArrayWO;
	IMG_UINT64             *pui64CyclesTaken;
	WORKEST_RETURN_DATA    *psReturnData;
	WORKEST_HOST_DATA      *psWorkEstHostData;
	PVRSRV_ERROR           eError = PVRSRV_ERROR_INVALID_PARAMS;

	if (!_WorkEstEnabled())
	{
		/* No error message to avoid excessive messages */
		return PVRSRV_OK;
	}

	PVR_LOGR_IF_FALSE(psReturnCmd, "WorkEstRetire: Missing return command", eError);
	PVR_LOGR_IF_FALSE((psReturnCmd->ui64ReturnDataIndex < RETURN_DATA_ARRAY_SIZE), "WorkEstRetire: Handle reference out-of-bounds", eError);

	/* Retrieve/validate the return data from this completed workload */
	psReturnData = &psDevInfo->asReturnData[psReturnCmd->ui64ReturnDataIndex];
 	psWorkloadCharacteristics = &psReturnData->sWorkloadCharacteristics;
	psWorkEstHostData = psReturnData->psWorkEstHostData;
	PVR_LOGR_IF_FALSE(psWorkEstHostData, "WorkEstRetire: Missing host data", eError);

	/* Retrieve/validate completed workload matching data */
	psWorkloadMatchingData = psReturnData->psWorkloadMatchingData;
	PVR_LOGG_IF_FALSE(psWorkloadMatchingData, "WorkEstRetire: Missing matching data", hasherror);
	PVR_LOGG_IF_FALSE(psWorkloadMatchingData->psHashTable, "WorkEstRetire: Missing hash", hasherror);
	PVR_LOGG_IF_FALSE(psWorkloadMatchingData->psHashLock, "WorkEstRetire: Missing hash/lock", hasherror);
	paui64WorkloadHashData = psWorkloadMatchingData->aui64HashData;
	pasWorkloadHashKeys = psWorkloadMatchingData->asHashKeys;
	ui32HashArrayWO = psWorkloadMatchingData->ui32HashArrayWO;

	OSLockAcquire(psWorkloadMatchingData->psHashLock);

	/* Update workload prediction by removing old hash entry (if any) & inserting new hash entry */
	pui64CyclesTaken = (IMG_UINT64*) HASH_Remove_Extended(psWorkloadMatchingData->psHashTable,
														  (uintptr_t*)&psWorkloadCharacteristics);

	if (paui64WorkloadHashData[ui32HashArrayWO] > 0)
	{
		/* Out-of-space so remove the oldest hash data before it becomes overwritten */
		RGX_WORKLOAD_TA3D *psWorkloadHashKey = &pasWorkloadHashKeys[ui32HashArrayWO];
		(void) HASH_Remove_Extended(psWorkloadMatchingData->psHashTable, (uintptr_t*)&psWorkloadHashKey);
	}

	if (pui64CyclesTaken == NULL)
	{
		/* There is no existing entry for this workload characteristics, store it */
		paui64WorkloadHashData[ui32HashArrayWO] = psReturnCmd->ui64CyclesTaken;
		pasWorkloadHashKeys[ui32HashArrayWO] = *psWorkloadCharacteristics;
	}
	else
	{
		/* Found prior entry for workload characteristics, average with completed; also reset the
		   old value to 0 so it is known to be invalid */
		paui64WorkloadHashData[ui32HashArrayWO] = (*pui64CyclesTaken + psReturnCmd->ui64CyclesTaken)/2;
		pasWorkloadHashKeys[ui32HashArrayWO] = *psWorkloadCharacteristics;
		*pui64CyclesTaken = 0;
	}

	/* Hash insertion should not fail but if it does best we can do is to exit gracefully and not
	   update the FW received counter */
	if (IMG_TRUE != HASH_Insert((HASH_TABLE*)psWorkloadMatchingData->psHashTable,
								(uintptr_t)&pasWorkloadHashKeys[ui32HashArrayWO],
								(uintptr_t)&paui64WorkloadHashData[ui32HashArrayWO]))
	{
		PVR_ASSERT(0);
		PVR_LOG(("WorkEstRetire: HASH_Insert failed"));
	}

	psWorkloadMatchingData->ui32HashArrayWO = (ui32HashArrayWO + 1) & WORKLOAD_HASH_WRAP_MASK;

	OSLockRelease(psWorkloadMatchingData->psHashLock);

hasherror:
	/* Update the received counter so that the FW is able to check as to whether all
	   the workloads connected to a render context are finished. */
	psWorkEstHostData->ui32WorkEstCCBReceived++;

	return eError;
}

void WorkEstInit(PVRSRV_RGXDEV_INFO *psDevInfo, WORKEST_HOST_DATA *psWorkEstData)
{
	HASH_TABLE *psWorkloadHashTable;
	PVR_UNREFERENCED_PARAMETER(psDevInfo);

	/* Create a lock to protect the TA hash table */
	WorkEstHashLockCreate(&psWorkEstData->sWorkloadMatchingDataTA.psHashLock);

	/* Create hash table for TA workload matching */
	psWorkloadHashTable = HASH_Create_Extended(WORKLOAD_HASH_SIZE,
											  sizeof(RGX_WORKLOAD_TA3D *),
											  WorkEstHashFuncTA3D,
											  (HASH_KEY_COMP *)WorkEstHashCompareTA3D);
	psWorkEstData->sWorkloadMatchingDataTA.psHashTable = psWorkloadHashTable;

	/* Create a lock to protect the 3D hash tables */
	WorkEstHashLockCreate(&psWorkEstData->sWorkloadMatchingData3D.psHashLock);

	/* Create hash table for 3D workload matching */
	psWorkloadHashTable = HASH_Create_Extended(WORKLOAD_HASH_SIZE,
											  sizeof(RGX_WORKLOAD_TA3D *),
											  WorkEstHashFuncTA3D,
											  (HASH_KEY_COMP *)WorkEstHashCompareTA3D);
	psWorkEstData->sWorkloadMatchingData3D.psHashTable = psWorkloadHashTable;
}

void WorkEstDeInit(PVRSRV_RGXDEV_INFO *psDevInfo, WORKEST_HOST_DATA *psWorkEstData)
{
	HASH_TABLE        *psWorkloadHashTable;
	RGX_WORKLOAD_TA3D *pasWorkloadHashKeys;
	RGX_WORKLOAD_TA3D *psWorkloadHashKey;
	IMG_UINT64        *paui64WorkloadCycleData;
	IMG_UINT32        ui32Itr;

	/* Tear down TA hash */
	pasWorkloadHashKeys = psWorkEstData->sWorkloadMatchingDataTA.asHashKeys;
	paui64WorkloadCycleData = psWorkEstData->sWorkloadMatchingDataTA.aui64HashData;
	psWorkloadHashTable = psWorkEstData->sWorkloadMatchingDataTA.psHashTable;

	if (psWorkloadHashTable)
	{
		for (ui32Itr = 0; ui32Itr < WORKLOAD_HASH_SIZE; ui32Itr++)
		{
			if (paui64WorkloadCycleData[ui32Itr] > 0)
			{
				psWorkloadHashKey = &pasWorkloadHashKeys[ui32Itr];
				HASH_Remove_Extended(psWorkloadHashTable, (uintptr_t*)&psWorkloadHashKey);
			}
		}

		HASH_Delete(psWorkloadHashTable);
	}

	/* Remove the hash lock */
	WorkEstHashLockDestroy(psWorkEstData->sWorkloadMatchingDataTA.psHashLock);

	/* Tear down 3D hash */
	pasWorkloadHashKeys = psWorkEstData->sWorkloadMatchingData3D.asHashKeys;
	paui64WorkloadCycleData = psWorkEstData->sWorkloadMatchingData3D.aui64HashData;
	psWorkloadHashTable = psWorkEstData->sWorkloadMatchingData3D.psHashTable;

	if (psWorkloadHashTable)
	{
		for (ui32Itr = 0; ui32Itr < WORKLOAD_HASH_SIZE; ui32Itr++)
		{
			if (paui64WorkloadCycleData[ui32Itr] > 0)
			{
				psWorkloadHashKey = &pasWorkloadHashKeys[ui32Itr];
				HASH_Remove_Extended(psWorkloadHashTable, (uintptr_t*)&psWorkloadHashKey);
			}
		}

		HASH_Delete(psWorkloadHashTable);
	}

	/* Remove the hash lock */
	WorkEstHashLockDestroy(psWorkEstData->sWorkloadMatchingData3D.psHashLock);

	return;
}

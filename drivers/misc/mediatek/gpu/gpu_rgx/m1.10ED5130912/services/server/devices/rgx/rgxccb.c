/*************************************************************************/ /*!
@File
@Title          RGX CCB routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX CCB routines
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

#include "pvr_debug.h"
#include "rgxdevice.h"
#include "pdump_km.h"
#include "allocmem.h"
#include "devicemem.h"
#include "rgxfwutils.h"
#include "osfunc.h"
#include "rgxccb.h"
#include "rgx_memallocflags.h"
#include "devicemem_pdump.h"
#include "dllist.h"
#include "rgx_fwif_shared.h"
#include "rgxtimerquery.h"
#if defined(LINUX)
#include "trace_events.h"
#endif
#include "sync_checkpoint_external.h"
#include "sync_checkpoint.h"
#include "rgxutils.h"

/*
*  Defines the number of fence updates to record so that future fences in the CCB
*  can be checked to see if they are already known to be satisfied.
*/
#define RGX_CCCB_FENCE_UPDATE_LIST_SIZE  (32)

#define RGX_UFO_PTR_ADDR(ufoptr)			(((ufoptr)->puiAddrUFO.ui32Addr) & 0xFFFFFFFC)

#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO)

#define PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_THRESHOLD 0x1
#define PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_ACQUIRE_FAILED 0x2

typedef struct _RGX_CLIENT_CCB_UTILISATION_
{
	/* the threshold in bytes.
	 * when the CCB utilisation hits the threshold then we will print
	 * a warning message.
	 */
	IMG_UINT32 ui32ThresholdBytes;
	/* Maximum cCCB usage at some point in time */
	IMG_UINT32 ui32HighWaterMark;
	/* keep track of the warnings already printed.
	 * bit mask of PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_xyz
	 */
	IMG_UINT32 ui32Warnings;
} RGX_CLIENT_CCB_UTILISATION;

#endif /* PVRSRV_ENABLE_CCCB_UTILISATION_INFO */

struct _RGX_CLIENT_CCB_ {
	volatile RGXFWIF_CCCB_CTL	*psClientCCBCtrl;				/*!< CPU mapping of the CCB control structure used by the fw */
	IMG_UINT8					*pui8ClientCCB;					/*!< CPU mapping of the CCB */
	DEVMEM_MEMDESC 				*psClientCCBMemDesc;			/*!< MemDesc for the CCB */
	DEVMEM_MEMDESC 				*psClientCCBCtrlMemDesc;		/*!< MemDesc for the CCB control */
	IMG_UINT32					ui32HostWriteOffset;			/*!< CCB write offset from the driver side */
	IMG_UINT32					ui32LastPDumpWriteOffset;		/*!< CCB write offset from the last time we submitted a command in capture range */
	IMG_UINT32					ui32FinishedPDumpWriteOffset;     /*!< Trails LastPDumpWriteOffset for last finished command, used for HW CB driven DMs */
	IMG_BOOL					bStateOpen;						/*!< Commands will be appended to a non finished CCB */
	IMG_UINT32					ui32LastROff;					/*!< Last CCB Read offset to help detect any CCB wedge */
	IMG_UINT32					ui32LastWOff;					/*!< Last CCB Write offset to help detect any CCB wedge */
	IMG_UINT32					ui32ByteCount;					/*!< Count of the number of bytes written to CCCB */
	IMG_UINT32					ui32LastByteCount;				/*!< Last value of ui32ByteCount to help detect any CCB wedge */
	IMG_UINT32					ui32Size;						/*!< Size of the CCB */
	DLLIST_NODE					sNode;							/*!< Node used to store this CCB on the per connection list */
	PDUMP_CONNECTION_DATA		*psPDumpConnectionData;			/*!< Pointer to the per connection data in which we reside */
	void						*hTransition;					/*!< Handle for Transition callback */
	IMG_CHAR					szName[MAX_CLIENT_CCB_NAME];	/*!< Name of this client CCB */
	RGX_SERVER_COMMON_CONTEXT   *psServerCommonContext;     	/*!< Parent server common context that this CCB belongs to */
#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO)
	RGX_CCB_REQUESTOR_TYPE				eRGXCCBRequestor;
	RGX_CLIENT_CCB_UTILISATION		sUtilisation;				/*!< CCB utilisation data */
#endif
#if defined(DEBUG)
	IMG_UINT32					ui32UpdateEntries;				/*!< Number of Fence Updates in asFenceUpdateList */
	RGXFWIF_UFO					asFenceUpdateList[RGX_CCCB_FENCE_UPDATE_LIST_SIZE];  /*!< List of recent updates written in this CCB */
#endif
};

/* Forms a table, with array of strings for each requestor type (listed in RGX_CCB_REQUESTORS X macro), to be used for
   DevMemAllocation comments and PDump comments. Each tuple in the table consists of 3 strings:
	{ "FwClientCCB:" <requestor_name>, "FwClientCCBControl:" <requestor_name>, <requestor_name> },
   The first string being used as comment when allocating ClientCCB for the given requestor, the second for CCBControl
   structure, and the 3rd one for use in PDUMP comments. The number of tuples in the table must adhere to the following
   build assert. */
IMG_CHAR *const aszCCBRequestors[][3] =
{
#define REQUESTOR_STRING(prefix,req) #prefix ":" #req
#define FORM_REQUESTOR_TUPLE(req) { REQUESTOR_STRING(FwClientCCB,req), REQUESTOR_STRING(FwClientCCBControl,req), #req },
	RGX_CCB_REQUESTORS(FORM_REQUESTOR_TUPLE)
#undef FORM_REQUESTOR_TUPLE
};

/* The number of tuples in the above table is always equal to those provided in the RGX_CCB_REQUESTORS X macro list.
   In an event of change in value of DPX_MAX_RAY_CONTEXTS to say 'n', appropriate entry/entries up to FC[n-1] must be added to
   the RGX_CCB_REQUESTORS list. */
static_assert((sizeof(aszCCBRequestors)/(3*sizeof(aszCCBRequestors[0][0]))) == (REQ_TYPE_FIXED_COUNT + DPX_MAX_RAY_CONTEXTS + 1),
			  "Mismatch between aszCCBRequestors table and DPX_MAX_RAY_CONTEXTS");

PVRSRV_ERROR RGXCCBPDumpDrainCCB(RGX_CLIENT_CCB *psClientCCB,
						IMG_UINT32 ui32PDumpFlags)
{

	IMG_UINT32 ui32PollOffset;

	if (psClientCCB->bStateOpen)
	{
		/* Draining CCB on a command that hasn't finished, and FW isn't expected
		 * to have updated Roff up to Woff. Only drain to the first
		 * finished command prior to this. The Roff for this
		 * is stored in ui32FinishedPDumpWriteOffset. 
		 */
		ui32PollOffset = psClientCCB->ui32FinishedPDumpWriteOffset;

		PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags,
							  "cCCB(%s@%p): Draining open CCB rgxfw_roff < woff (%d)",
							  psClientCCB->szName,
							  psClientCCB,
							  ui32PollOffset);
	}
	else
	{
		/* Command to a finished CCB stream and FW is drained to empty
		 * out remaining commands until R==W.
		 */
		ui32PollOffset = psClientCCB->ui32LastPDumpWriteOffset;

		PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags,
							  "cCCB(%s@%p): Draining CCB rgxfw_roff == woff (%d)",
							  psClientCCB->szName,
							  psClientCCB,
							  ui32PollOffset);
	}

	return DevmemPDumpDevmemPol32(psClientCCB->psClientCCBCtrlMemDesc,
									offsetof(RGXFWIF_CCCB_CTL, ui32ReadOffset),
									ui32PollOffset,
									0xffffffff,
									PDUMP_POLL_OPERATOR_EQUAL,
									ui32PDumpFlags);
}

static PVRSRV_ERROR _RGXCCBPDumpTransition(void **pvData, IMG_BOOL bInto, IMG_UINT32 ui32PDumpFlags)
{
	RGX_CLIENT_CCB *psClientCCB = (RGX_CLIENT_CCB *) pvData;
	
	/* We're about to transition into capture range and we've submitted
	 * new commands since the last time we entered capture range so drain
	 * the live CCB and simulation (sim) CCB as required, i.e. leave CCB
	 * idle in both live and sim contexts.
	 * This requires the host driver to ensure the live FW & the sim FW
	 * have both emptied out the remaining commands until R==W (CCB empty).
	 */
	if (bInto)
	{
		volatile RGXFWIF_CCCB_CTL *psCCBCtl = psClientCCB->psClientCCBCtrl;
		PVRSRV_ERROR eError;

		/* Wait for the live FW to catch up/empty CCB. This is done by returning
		 * retry which will get pushed back out to Services client where it
		 * waits on the event object and then resubmits the command.
		 */
		if (psClientCCB->psClientCCBCtrl->ui32ReadOffset != psClientCCB->ui32HostWriteOffset)
		{
			return PVRSRV_ERROR_RETRY;
		}

		/* Wait for the sim FW to catch up/empty sim CCB.
		 * We drain whenever capture range is entered, even if no commands
		 * have been issued on this CCB when out of capture range. We have to
		 * wait for commands that might have been issued in the last capture
		 * range to finish so the connection's sync block snapshot dumped after
		 * all the PDumpTransition callbacks have been execute doesn't clobber
		 * syncs which the sim FW is currently working on.
		 *
		 * Although this is sub-optimal for play-back - while out of capture
		 * range for every continuous operation we synchronise the sim
		 * play-back processing the script and the sim FW, there is no easy
		 * solution. Not all modules that work with syncs register a
		 * PDumpTransition callback and thus we have no way of knowing if we
		 * can skip this sim CCB drain and sync block dump or not.
		*/

		eError = RGXCCBPDumpDrainCCB(psClientCCB, ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_WARNING, "_RGXCCBPDumpTransition: problem pdumping POL for cCCBCtl (%d)", eError));
		}
		PVR_ASSERT(eError == PVRSRV_OK);

		/* Live CCB and simulation CCB now empty, FW idle on CCB in both
		 * contexts.
		 */

		if (psClientCCB->ui32LastPDumpWriteOffset != psClientCCB->ui32HostWriteOffset)
		{
			/* If new commands have been written when out of capture range in
			 * the live CCB then we need to fast forward the sim CCBCtl
			 * offsets past uncaptured commands. This is done by PDUMPing
			 * the CCBCtl memory to align sim values with the live CCBCtl
			 * values. Both live and sim FWs can start with the 1st command
			 * which is in the new capture range.
			 */
			psCCBCtl->ui32ReadOffset = psClientCCB->ui32HostWriteOffset;
			psCCBCtl->ui32DepOffset = psClientCCB->ui32HostWriteOffset;
			psCCBCtl->ui32WriteOffset = psClientCCB->ui32HostWriteOffset;
	
			PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags,
								  "cCCB(%s@%p): Fast-forward from %d to %d",
								  psClientCCB->szName,
								  psClientCCB,
								  psClientCCB->ui32LastPDumpWriteOffset,
								  psClientCCB->ui32HostWriteOffset);
	
			DevmemPDumpLoadMem(psClientCCB->psClientCCBCtrlMemDesc,
							   0,
							   sizeof(RGXFWIF_CCCB_CTL),
							   ui32PDumpFlags);
			
			/* Although we've entered capture range for this process connection
			 * we might not do any work	on this CCB so update the
			 * ui32LastPDumpWriteOffset to reflect where we got to for next
			 * time so we start the drain from where we got to last time.
			 */
			psClientCCB->ui32LastPDumpWriteOffset = psClientCCB->ui32HostWriteOffset;
		}
	}
	return PVRSRV_OK;
}

#if defined (PVRSRV_ENABLE_CCCB_UTILISATION_INFO)

static INLINE void _RGXInitCCBUtilisation(RGX_CLIENT_CCB *psClientCCB)
{
	psClientCCB->sUtilisation.ui32HighWaterMark = 0; /* initialize ui32HighWaterMark level to zero */
	psClientCCB->sUtilisation.ui32ThresholdBytes = (psClientCCB->ui32Size *
							PVRSRV_ENABLE_CCCB_UTILISATION_INFO_THRESHOLD)	/ 100;
	psClientCCB->sUtilisation.ui32Warnings = 0;
}

static INLINE void _RGXPrintCCBUtilisationWarning(RGX_CLIENT_CCB *psClientCCB,
									IMG_UINT32 ui32WarningType,
									IMG_UINT32 ui32CmdSize)
{
#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO_VERBOSE)
	if(ui32WarningType == PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_ACQUIRE_FAILED)
	{
		PVR_LOG(("Failed to acquire CCB space for %u byte command:", ui32CmdSize));
	}

	PVR_LOG(("%s: Client CCB (%s) watermark (%u) hit %d%% of its allocation size (%u)",
								__FUNCTION__,
								psClientCCB->szName,
								psClientCCB->sUtilisation.ui32HighWaterMark,
								psClientCCB->sUtilisation.ui32HighWaterMark * 100 / psClientCCB->ui32Size,
								psClientCCB->ui32Size));
#else
	PVR_UNREFERENCED_PARAMETER(ui32WarningType);
	PVR_UNREFERENCED_PARAMETER(ui32CmdSize);

	PVR_LOG(("GPU %s command buffer usage high (%u). This is not an error but the application may not run optimally.",
							aszCCBRequestors[psClientCCB->eRGXCCBRequestor][REQ_PDUMP_COMMENT],
							psClientCCB->sUtilisation.ui32HighWaterMark * 100 / psClientCCB->ui32Size));
#endif
}

static INLINE void _RGXCCBUtilisationEvent(RGX_CLIENT_CCB *psClientCCB,
						IMG_UINT32 ui32WarningType,
						IMG_UINT32 ui32CmdSize)
{
	/* in VERBOSE mode we will print a message for each different
	 * event type as they happen.
	 * but by default we will only issue one message
	 */
#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO_VERBOSE)
	if(!(psClientCCB->sUtilisation.ui32Warnings & ui32WarningType))
#else
	if(!psClientCCB->sUtilisation.ui32Warnings)
#endif
	{
		_RGXPrintCCBUtilisationWarning(psClientCCB,
						ui32WarningType,
						ui32CmdSize);
		/* record that we have issued a warning of this type */
		psClientCCB->sUtilisation.ui32Warnings |= ui32WarningType;
	}
}

/* Check the current CCB utilisation. Print a one-time warning message if it is above the
 * specified threshold
 */
static INLINE void _RGXCheckCCBUtilisation(RGX_CLIENT_CCB *psClientCCB)
{
	/* Print a warning message if the cCCB watermark is above the threshold value */
	if(psClientCCB->sUtilisation.ui32HighWaterMark >= psClientCCB->sUtilisation.ui32ThresholdBytes)
	{
		_RGXCCBUtilisationEvent(psClientCCB,
					PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_THRESHOLD,
					0);
	}
}

/* Update the cCCB high watermark level if necessary */
static void _RGXUpdateCCBUtilisation(RGX_CLIENT_CCB *psClientCCB)
{
	IMG_UINT32 ui32FreeSpace, ui32MemCurrentUsage;

	ui32FreeSpace = GET_CCB_SPACE(psClientCCB->ui32HostWriteOffset,
									  psClientCCB->psClientCCBCtrl->ui32ReadOffset,
									  psClientCCB->ui32Size);
	ui32MemCurrentUsage = psClientCCB->ui32Size - ui32FreeSpace;

	if (ui32MemCurrentUsage > psClientCCB->sUtilisation.ui32HighWaterMark)
	{
		psClientCCB->sUtilisation.ui32HighWaterMark = ui32MemCurrentUsage;

		/* The high water mark has increased. Check if it is above the
		 * threshold so we can print a warning if necessary.
		 */
		_RGXCheckCCBUtilisation(psClientCCB);
	}
}

#endif /* PVRSRV_ENABLE_CCCB_UTILISATION_INFO */

PVRSRV_ERROR RGXCreateCCB(PVRSRV_RGXDEV_INFO	*psDevInfo,
						  IMG_UINT32			ui32CCBSizeLog2,
						  CONNECTION_DATA		*psConnectionData,
						  RGX_CCB_REQUESTOR_TYPE		eRGXCCBRequestor,
						  RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
						  RGX_CLIENT_CCB		**ppsClientCCB,
						  DEVMEM_MEMDESC 		**ppsClientCCBMemDesc,
						  DEVMEM_MEMDESC 		**ppsClientCCBCtrlMemDesc)
{
	PVRSRV_ERROR	eError;
	DEVMEM_FLAGS_T	uiClientCCBMemAllocFlags, uiClientCCBCtlMemAllocFlags;
	IMG_UINT32		ui32AllocSize = (1U << ui32CCBSizeLog2);
	RGX_CLIENT_CCB	*psClientCCB;

	/* All client CCBs should be at-least of the "minimum" size declared by the API */
	PVR_ASSERT (ui32CCBSizeLog2 >= MIN_SAFE_CCB_SIZE_LOG2);

	psClientCCB = OSAllocMem(sizeof(*psClientCCB));
	if (psClientCCB == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc;
	}
	psClientCCB->psServerCommonContext = psServerCommonContext;

	uiClientCCBMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
								PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
								PVRSRV_MEMALLOCFLAG_GPU_READABLE |
								PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
								PVRSRV_MEMALLOCFLAG_CPU_WRITE_COMBINE |
								PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
								PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE;

	uiClientCCBCtlMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
								PVRSRV_MEMALLOCFLAG_GPU_READABLE |
								PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
								PVRSRV_MEMALLOCFLAG_UNCACHED |
								PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
								PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE;

	PDUMPCOMMENT("Allocate RGXFW cCCB");
	eError = DevmemFwAllocate(psDevInfo,
										ui32AllocSize,
										uiClientCCBMemAllocFlags,
										aszCCBRequestors[eRGXCCBRequestor][REQ_RGX_FW_CLIENT_CCB_STRING],
										&psClientCCB->psClientCCBMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateCCBKM: Failed to allocate RGX client CCB (%s)",
				PVRSRVGetErrorStringKM(eError)));
		goto fail_alloc_ccb;
	}


	eError = DevmemAcquireCpuVirtAddr(psClientCCB->psClientCCBMemDesc,
									  (void **) &psClientCCB->pui8ClientCCB);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateCCBKM: Failed to map RGX client CCB (%s)",
				PVRSRVGetErrorStringKM(eError)));
		goto fail_map_ccb;
	}

	PDUMPCOMMENT("Allocate RGXFW cCCB control");
	eError = DevmemFwAllocate(psDevInfo,
										sizeof(RGXFWIF_CCCB_CTL),
										uiClientCCBCtlMemAllocFlags,
										aszCCBRequestors[eRGXCCBRequestor][REQ_RGX_FW_CLIENT_CCB_CONTROL_STRING],
										&psClientCCB->psClientCCBCtrlMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateCCBKM: Failed to allocate RGX client CCB control (%s)",
				PVRSRVGetErrorStringKM(eError)));
		goto fail_alloc_ccbctrl;
	}


	eError = DevmemAcquireCpuVirtAddr(psClientCCB->psClientCCBCtrlMemDesc,
									  (void **) &psClientCCB->psClientCCBCtrl);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PVRSRVRGXCreateCCBKM: Failed to map RGX client CCB (%s)",
				PVRSRVGetErrorStringKM(eError)));
		goto fail_map_ccbctrl;
	}

	psClientCCB->psClientCCBCtrl->ui32WriteOffset = 0;
	psClientCCB->psClientCCBCtrl->ui32ReadOffset = 0;
	psClientCCB->psClientCCBCtrl->ui32DepOffset = 0;
	psClientCCB->psClientCCBCtrl->ui32WrapMask = ui32AllocSize - 1;
	OSSNPrintf(psClientCCB->szName, MAX_CLIENT_CCB_NAME, "%s-P%lu-T%lu-%s",
									aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
									(unsigned long) OSGetCurrentClientProcessIDKM(),
									(unsigned long) OSGetCurrentClientThreadIDKM(),
									OSGetCurrentClientProcessNameKM());

	PDUMPCOMMENT("cCCB control");
	DevmemPDumpLoadMem(psClientCCB->psClientCCBCtrlMemDesc,
					   0,
					   sizeof(RGXFWIF_CCCB_CTL),
					   PDUMP_FLAGS_CONTINUOUS);
	PVR_ASSERT(eError == PVRSRV_OK);

	psClientCCB->ui32HostWriteOffset = 0;
	psClientCCB->ui32LastPDumpWriteOffset = 0;
	psClientCCB->ui32FinishedPDumpWriteOffset = 0;
	psClientCCB->ui32Size = ui32AllocSize;
	psClientCCB->ui32LastROff = ui32AllocSize - 1;
	psClientCCB->ui32ByteCount = 0;
	psClientCCB->ui32LastByteCount = 0;
	psClientCCB->bStateOpen = IMG_FALSE;

#if defined(DEBUG)
	psClientCCB->ui32UpdateEntries = 0;
#endif

#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO)
	_RGXInitCCBUtilisation(psClientCCB);
	psClientCCB->eRGXCCBRequestor = eRGXCCBRequestor;
#endif
	eError = PDumpRegisterTransitionCallback(psConnectionData->psPDumpConnectionData,
											  _RGXCCBPDumpTransition,
											  psClientCCB,
											  &psClientCCB->hTransition);
	if (eError != PVRSRV_OK)
	{
		goto fail_pdumpreg;
	}

	/*
	 * Note:
	 * Save the PDump specific structure, which is ref counted unlike
	 * the connection data, to ensure it's not freed too early
	 */
	psClientCCB->psPDumpConnectionData = psConnectionData->psPDumpConnectionData;
	PDUMPCOMMENT("New RGXFW cCCB(%s@%p) created",
				 psClientCCB->szName,
				 psClientCCB);

	*ppsClientCCB = psClientCCB;
	*ppsClientCCBMemDesc = psClientCCB->psClientCCBMemDesc;
	*ppsClientCCBCtrlMemDesc = psClientCCB->psClientCCBCtrlMemDesc;
	return PVRSRV_OK;

fail_pdumpreg:
	DevmemReleaseCpuVirtAddr(psClientCCB->psClientCCBCtrlMemDesc);
fail_map_ccbctrl:
	DevmemFwFree(psDevInfo, psClientCCB->psClientCCBCtrlMemDesc);
fail_alloc_ccbctrl:
	DevmemReleaseCpuVirtAddr(psClientCCB->psClientCCBMemDesc);
fail_map_ccb:
	DevmemFwFree(psDevInfo, psClientCCB->psClientCCBMemDesc);
fail_alloc_ccb:
	OSFreeMem(psClientCCB);
fail_alloc:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

void RGXDestroyCCB(PVRSRV_RGXDEV_INFO *psDevInfo, RGX_CLIENT_CCB *psClientCCB)
{
	PDumpUnregisterTransitionCallback(psClientCCB->hTransition);
	DevmemReleaseCpuVirtAddr(psClientCCB->psClientCCBCtrlMemDesc);
	DevmemFwFree(psDevInfo, psClientCCB->psClientCCBCtrlMemDesc);
	DevmemReleaseCpuVirtAddr(psClientCCB->psClientCCBMemDesc);
	DevmemFwFree(psDevInfo, psClientCCB->psClientCCBMemDesc);
	OSFreeMem(psClientCCB);
}

/******************************************************************************
 FUNCTION	: RGXAcquireCCB

 PURPOSE	: Obtains access to write some commands to a CCB

 PARAMETERS	: psClientCCB		- The client CCB
			  ui32CmdSize		- How much space is required
			  ppvBufferSpace	- Pointer to space in the buffer
			  ui32PDumpFlags - Should this be PDump continuous?

 RETURNS	: PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR RGXAcquireCCB(RGX_CLIENT_CCB *psClientCCB,
										IMG_UINT32		ui32CmdSize,
										void			**ppvBufferSpace,
										IMG_UINT32		ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	IMG_BOOL	bInCaptureRange;
	IMG_BOOL	bPdumpEnabled;
	IMG_UINT64	ui64PDumpState = 0;

	PDumpGetStateKM(&ui64PDumpState);
	PDumpIsCaptureFrameKM(&bInCaptureRange);
	bPdumpEnabled = (ui64PDumpState & PDUMP_STATE_CONNECTED) != 0
		&& (bInCaptureRange || PDUMP_IS_CONTINUOUS(ui32PDumpFlags));

	/*
		PDumpSetFrame will detect as we Transition into capture range for
		frame based data but if we are PDumping continuous data then we
		need to inform the PDump layer ourselves
	*/
	if ((ui64PDumpState & PDUMP_STATE_CONNECTED) != 0
		&& PDUMP_IS_CONTINUOUS(ui32PDumpFlags)
		&& !bInCaptureRange)
	{
		eError = PDumpTransition(psClientCCB->psPDumpConnectionData, IMG_TRUE, ui32PDumpFlags);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}
	}

	/* Check that the CCB can hold this command + padding */
	if ((ui32CmdSize + PADDING_COMMAND_SIZE + 1) > psClientCCB->ui32Size)
	{
		PVR_DPF((PVR_DBG_ERROR, "Command size (%d bytes) too big for CCB (%d bytes)",
								ui32CmdSize, psClientCCB->ui32Size));
		return PVRSRV_ERROR_CMD_TOO_BIG;
	}

	/*
		Check we don't overflow the end of the buffer and make sure we have
		enough space for the padding command. We don't have enough space (including the
		minimum amount for the padding command) we will need to make sure we insert a
		padding command now and wrap before adding the main command.
	*/
	if ((psClientCCB->ui32HostWriteOffset + ui32CmdSize + PADDING_COMMAND_SIZE) <= psClientCCB->ui32Size)
	{
		/*
			The command can fit without wrapping...
		*/
		IMG_UINT32 ui32FreeSpace;

#if defined(PDUMP)
		/* Wait for sufficient CCB space to become available */
		PDUMPCOMMENTWITHFLAGS(0, "Wait for %u bytes to become available according cCCB Ctl (woff=%x) for %s",
								ui32CmdSize, psClientCCB->ui32HostWriteOffset,
								psClientCCB->szName);
		DevmemPDumpCBP(psClientCCB->psClientCCBCtrlMemDesc,
					   offsetof(RGXFWIF_CCCB_CTL, ui32ReadOffset),
					   psClientCCB->ui32HostWriteOffset,
					   ui32CmdSize,
					   psClientCCB->ui32Size);
#endif

		ui32FreeSpace = GET_CCB_SPACE(psClientCCB->ui32HostWriteOffset,
									  psClientCCB->psClientCCBCtrl->ui32ReadOffset,
									  psClientCCB->ui32Size);

		/* Don't allow all the space to be used */
		if (ui32FreeSpace > ui32CmdSize)
		{
			*ppvBufferSpace = (void *) (psClientCCB->pui8ClientCCB +
										psClientCCB->ui32HostWriteOffset);
			return PVRSRV_OK;
		}

		goto e_retry;
	}
	else
	{
		/*
			We're at the end of the buffer without enough contiguous space.
			The command cannot fit without wrapping, we need to insert a
			padding command and wrap. We need to do this in one go otherwise
			we would be leaving unflushed commands and forcing the client to
			deal with flushing the padding command but not the command they
			wanted to write. Therefore we either do all or nothing.
		*/
		RGXFWIF_CCB_CMD_HEADER *psHeader;
		IMG_UINT32 ui32FreeSpace;
		IMG_UINT32 ui32Remain = psClientCCB->ui32Size - psClientCCB->ui32HostWriteOffset;

#if defined(PDUMP)
		/* Wait for sufficient CCB space to become available */
		PDUMPCOMMENTWITHFLAGS(0, "Wait for %u bytes to become available according cCCB Ctl (woff=%x) for %s",
								ui32Remain, psClientCCB->ui32HostWriteOffset,
								psClientCCB->szName);
		DevmemPDumpCBP(psClientCCB->psClientCCBCtrlMemDesc,
					   offsetof(RGXFWIF_CCCB_CTL, ui32ReadOffset),
					   psClientCCB->ui32HostWriteOffset,
					   ui32Remain,
					   psClientCCB->ui32Size);
		PDUMPCOMMENTWITHFLAGS(0, "Wait for %u bytes to become available according cCCB Ctl (woff=%x) for %s",
								ui32CmdSize, 0 /*ui32HostWriteOffset after wrap */,
								psClientCCB->szName);
		DevmemPDumpCBP(psClientCCB->psClientCCBCtrlMemDesc,
					   offsetof(RGXFWIF_CCCB_CTL, ui32ReadOffset),
					   0 /*ui32HostWriteOffset after wrap */,
					   ui32CmdSize,
					   psClientCCB->ui32Size);
#endif

		ui32FreeSpace = GET_CCB_SPACE(psClientCCB->ui32HostWriteOffset,
									  psClientCCB->psClientCCBCtrl->ui32ReadOffset,
									  psClientCCB->ui32Size);

		/* Don't allow all the space to be used */
		if (ui32FreeSpace > ui32Remain + ui32CmdSize)
		{
			psHeader = (void *) (psClientCCB->pui8ClientCCB + psClientCCB->ui32HostWriteOffset);
			psHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_PADDING;
			psHeader->ui32CmdSize = ui32Remain - sizeof(RGXFWIF_CCB_CMD_HEADER);

			PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags, "cCCB(%p): Padding cmd %d", psClientCCB, psHeader->ui32CmdSize);
			if (bPdumpEnabled)
			{
				DevmemPDumpLoadMem(psClientCCB->psClientCCBMemDesc,
								   psClientCCB->ui32HostWriteOffset,
								   ui32Remain,
								   ui32PDumpFlags);
			}
			
			*ppvBufferSpace = (void *) (psClientCCB->pui8ClientCCB +
										0 /*ui32HostWriteOffset after wrap */);
			return PVRSRV_OK;
		}

		goto e_retry;
	}
e_retry:
#if defined(PVRSRV_ENABLE_CCCB_UTILISATION_INFO)
	_RGXCCBUtilisationEvent(psClientCCB,
				PVRSRV_CLIENT_CCCB_UTILISATION_WARNING_ACQUIRE_FAILED,
				ui32CmdSize);
#endif  /* PVRSRV_ENABLE_CCCB_UTILISATION_INFO */
	return PVRSRV_ERROR_RETRY;
}

/******************************************************************************
 FUNCTION	: RGXReleaseCCB

 PURPOSE	: Release a CCB that we have been writing to.

 PARAMETERS	: psDevData			- device data
  			  psCCB				- the CCB

 RETURNS	: None
******************************************************************************/
void RGXReleaseCCB(RGX_CLIENT_CCB *psClientCCB,
								IMG_UINT32		ui32CmdSize,
								IMG_UINT32		ui32PDumpFlags)
{
	IMG_BOOL	bInCaptureRange;
	IMG_BOOL	bPdumpEnabled;
	IMG_UINT64	ui64PDumpState = 0;

	PDumpGetStateKM(&ui64PDumpState);
	PDumpIsCaptureFrameKM(&bInCaptureRange);
	bPdumpEnabled = (ui64PDumpState & PDUMP_STATE_CONNECTED) != 0
		&& (bInCaptureRange || PDUMP_IS_CONTINUOUS(ui32PDumpFlags));

	/*
	 *  If a padding command was needed then we should now move ui32HostWriteOffset
	 *  forward. The command has already be dumped (if bPdumpEnabled).
	 */
	if ((psClientCCB->ui32HostWriteOffset + ui32CmdSize + PADDING_COMMAND_SIZE) > psClientCCB->ui32Size)
	{
		IMG_UINT32 ui32Remain = psClientCCB->ui32Size - psClientCCB->ui32HostWriteOffset;

		UPDATE_CCB_OFFSET(psClientCCB->ui32HostWriteOffset,
						  ui32Remain,
						  psClientCCB->ui32Size);
		psClientCCB->ui32ByteCount += ui32Remain;
	}

	/* Dump the CCB data */
	if (bPdumpEnabled)
	{
		DevmemPDumpLoadMem(psClientCCB->psClientCCBMemDesc,
						   psClientCCB->ui32HostWriteOffset,
						   ui32CmdSize,
						   ui32PDumpFlags);
	}
	
	/*
	 *  Check if there any fences being written that will already be
	 *  satisfied by the last written update command in this CCB. At the
	 *  same time we can ASSERT that all sync addresses are not NULL.
	 */
#if defined(DEBUG)
	{
		IMG_UINT8  *pui8BufferStart = (void *)((uintptr_t)psClientCCB->pui8ClientCCB + psClientCCB->ui32HostWriteOffset);
		IMG_UINT8  *pui8BufferEnd   = (void *)((uintptr_t)psClientCCB->pui8ClientCCB + psClientCCB->ui32HostWriteOffset + ui32CmdSize);
		IMG_BOOL   bMessagePrinted  = IMG_FALSE;

		/* Walk through the commands in this section of CCB being released... */
		while (pui8BufferStart < pui8BufferEnd)
		{
			RGXFWIF_CCB_CMD_HEADER  *psCmdHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8BufferStart;

			if (psCmdHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_UPDATE)
			{
				/* If an UPDATE then record the values incase an adjacent fence uses it. */
				IMG_UINT32   ui32NumUFOs = psCmdHeader->ui32CmdSize / sizeof(RGXFWIF_UFO);
				RGXFWIF_UFO  *psUFOPtr   = (RGXFWIF_UFO*)(pui8BufferStart + sizeof(RGXFWIF_CCB_CMD_HEADER));
				
				psClientCCB->ui32UpdateEntries = 0;
				while (ui32NumUFOs-- > 0)
				{
					PVR_ASSERT(psUFOPtr->puiAddrUFO.ui32Addr != 0);
					if (psClientCCB->ui32UpdateEntries < RGX_CCCB_FENCE_UPDATE_LIST_SIZE)
					{
						psClientCCB->asFenceUpdateList[psClientCCB->ui32UpdateEntries++] = *psUFOPtr++;
					}
				}
			}
			else if (psCmdHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_FENCE)
			{
				/* If a FENCE then check the values against the last UPDATE issued. */
				IMG_UINT32   ui32NumUFOs = psCmdHeader->ui32CmdSize / sizeof(RGXFWIF_UFO);
				RGXFWIF_UFO  *psUFOPtr   = (RGXFWIF_UFO*)(pui8BufferStart + sizeof(RGXFWIF_CCB_CMD_HEADER));
				
				while (ui32NumUFOs-- > 0)
				{
					PVR_ASSERT(psUFOPtr->puiAddrUFO.ui32Addr != 0);

					if (bMessagePrinted == IMG_FALSE)
					{
						RGXFWIF_UFO  *psUpdatePtr = psClientCCB->asFenceUpdateList;
						IMG_UINT32  ui32UpdateIndex;

						for (ui32UpdateIndex = 0;  ui32UpdateIndex < psClientCCB->ui32UpdateEntries;  ui32UpdateIndex++)
						{
							if (PVRSRV_UFO_IS_SYNC_CHECKPOINT(psUFOPtr))
							{
								if (RGX_UFO_PTR_ADDR(psUFOPtr) == RGX_UFO_PTR_ADDR(psUpdatePtr))
								{
									PVR_DPF((PVR_DBG_MESSAGE, "Redundant sync checkpoint check found in cCCB(%p) - 0x%x -> 0x%x",
											psClientCCB, RGX_UFO_PTR_ADDR(psUFOPtr), psUFOPtr->ui32Value));
									bMessagePrinted = IMG_TRUE;
									break;
								}
							}
							else
							{
								if (psUFOPtr->puiAddrUFO.ui32Addr == psUpdatePtr->puiAddrUFO.ui32Addr  &&
									psUFOPtr->ui32Value == psUpdatePtr->ui32Value)
								{
									PVR_DPF((PVR_DBG_MESSAGE, "Redundant fence check found in cCCB(%p) - 0x%x -> 0x%x",
											psClientCCB, psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value));
									bMessagePrinted = IMG_TRUE;
									break;
								}
							}
							psUpdatePtr++;
						}
					}

					psUFOPtr++;
				}
			}
			else if (psCmdHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_FENCE_PR  ||
					 psCmdHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_UNFENCED_UPDATE)
			{
				/* For all other UFO ops check the UFO address is not NULL. */
				IMG_UINT32   ui32NumUFOs = psCmdHeader->ui32CmdSize / sizeof(RGXFWIF_UFO);
				RGXFWIF_UFO  *psUFOPtr   = (RGXFWIF_UFO*)(pui8BufferStart + sizeof(RGXFWIF_CCB_CMD_HEADER));

				while (ui32NumUFOs-- > 0)
				{
					PVR_ASSERT(psUFOPtr->puiAddrUFO.ui32Addr != 0);
					psUFOPtr++;
				}
			}

			/* Move to the next command in this section of CCB being released... */
			pui8BufferStart += sizeof(RGXFWIF_CCB_CMD_HEADER) + psCmdHeader->ui32CmdSize;
		}
	}
#endif /* REDUNDANT_SYNCS_DEBUG */

	/*
	 * Update the CCB write offset.
	 */
	UPDATE_CCB_OFFSET(psClientCCB->ui32HostWriteOffset,
					  ui32CmdSize,
					  psClientCCB->ui32Size);
	psClientCCB->ui32ByteCount += ui32CmdSize;

#if defined (PVRSRV_ENABLE_CCCB_UTILISATION_INFO)
	_RGXUpdateCCBUtilisation(psClientCCB);
#endif
	/*
		PDumpSetFrame will detect as we Transition out of capture range for
		frame based data but if we are PDumping continuous data then we
		need to inform the PDump layer ourselves
	*/
	if ((ui64PDumpState & PDUMP_STATE_CONNECTED) != 0
		&& PDUMP_IS_CONTINUOUS(ui32PDumpFlags)
		&& !bInCaptureRange)
	{
		PVRSRV_ERROR eError;

		/* Only Transitioning into capture range can cause an error */
		eError = PDumpTransition(psClientCCB->psPDumpConnectionData, IMG_FALSE, ui32PDumpFlags);
		PVR_ASSERT(eError == PVRSRV_OK);
	}

	if (bPdumpEnabled)
	{
		if (!psClientCCB->bStateOpen)
		{
			/* Store offset to last finished CCB command. This offset can 
			 * be needed when appending commands to a non finished CCB. 
			 */
			psClientCCB->ui32FinishedPDumpWriteOffset = psClientCCB->ui32LastPDumpWriteOffset;
		}

		/* Update the PDump write offset to show we PDumped this command */
		psClientCCB->ui32LastPDumpWriteOffset = psClientCCB->ui32HostWriteOffset;
	}

#if defined(NO_HARDWARE)
	/*
		The firmware is not running, it cannot update these; we do here instead.
	*/
	psClientCCB->psClientCCBCtrl->ui32ReadOffset = psClientCCB->ui32HostWriteOffset;
	psClientCCB->psClientCCBCtrl->ui32DepOffset = psClientCCB->ui32HostWriteOffset;
#endif
}

IMG_UINT32 RGXGetHostWriteOffsetCCB(RGX_CLIENT_CCB *psClientCCB)
{
	return psClientCCB->ui32HostWriteOffset;
}

#define SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL PVR_DBG_ERROR
#define CHECK_COMMAND(cmd, fenceupdate) \
				case RGXFWIF_CCB_CMD_TYPE_##cmd: \
						PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, #cmd " command (%d bytes)", psHeader->ui32CmdSize)); \
						bFenceUpdate = fenceupdate; \
						break

static void _RGXClientCCBDumpCommands(RGX_CLIENT_CCB *psClientCCB,
									  IMG_UINT32 ui32Offset,
									  IMG_UINT32 ui32ByteCount)
{
#if defined(SUPPORT_DUMP_CLIENT_CCB_COMMANDS)
	IMG_UINT8 *pui8Ptr = psClientCCB->pui8ClientCCB + ui32Offset;
	IMG_UINT32 ui32ConsumeSize = ui32ByteCount;

	while (ui32ConsumeSize)
	{
		RGXFWIF_CCB_CMD_HEADER *psHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8Ptr;
		IMG_BOOL bFenceUpdate = IMG_FALSE;

		PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "@offset 0x%08lx", pui8Ptr - psClientCCB->pui8ClientCCB));
		switch(psHeader->eCmdType)
		{
			CHECK_COMMAND(TA, IMG_FALSE);
			CHECK_COMMAND(3D, IMG_FALSE);
			CHECK_COMMAND(CDM, IMG_FALSE);
			CHECK_COMMAND(TQ_3D, IMG_FALSE);
			CHECK_COMMAND(TQ_2D, IMG_FALSE);
			CHECK_COMMAND(3D_PR, IMG_FALSE);
			CHECK_COMMAND(NULL, IMG_FALSE);
			CHECK_COMMAND(SHG, IMG_FALSE);
			CHECK_COMMAND(RTU, IMG_FALSE);
			CHECK_COMMAND(RTU_FC, IMG_FALSE);
			CHECK_COMMAND(PRE_TIMESTAMP, IMG_FALSE);
			CHECK_COMMAND(POST_TIMESTAMP, IMG_FALSE);
			CHECK_COMMAND(FENCE, IMG_TRUE);
			CHECK_COMMAND(UPDATE, IMG_TRUE);
			CHECK_COMMAND(UNFENCED_UPDATE, IMG_FALSE);
			CHECK_COMMAND(RMW_UPDATE, IMG_TRUE);
			CHECK_COMMAND(FENCE_PR, IMG_TRUE);
			CHECK_COMMAND(UNFENCED_RMW_UPDATE, IMG_FALSE);
			CHECK_COMMAND(PADDING, IMG_FALSE);
			CHECK_COMMAND(TQ_TDM, IMG_FALSE);
			default:
				PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "Unknown command!"));
				break;
		}
		pui8Ptr += sizeof(*psHeader);
		if (bFenceUpdate)
		{
			IMG_UINT32 j;
			RGXFWIF_UFO *psUFOPtr = (RGXFWIF_UFO *) pui8Ptr;
			for (j=0;j<psHeader->ui32CmdSize/sizeof(RGXFWIF_UFO);j++)
			{
				PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "Addr = 0x%08x, value = 0x%08x",
							psUFOPtr[j].puiAddrUFO.ui32Addr, psUFOPtr[j].ui32Value));
			}
		}
		else
		{
			IMG_UINT32 *pui32Ptr = (IMG_UINT32 *) pui8Ptr;
			IMG_UINT32 ui32Remain = psHeader->ui32CmdSize/sizeof(IMG_UINT32);
			while(ui32Remain)
			{
				if (ui32Remain >= 4)
				{
					PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "0x%08x 0x%08x 0x%08x 0x%08x",
							pui32Ptr[0], pui32Ptr[1], pui32Ptr[2], pui32Ptr[3]));
					pui32Ptr += 4;
					ui32Remain -= 4;
				}
				if (ui32Remain == 3)
				{
					PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "0x%08x 0x%08x 0x%08x",
							pui32Ptr[0], pui32Ptr[1], pui32Ptr[2]));
					pui32Ptr += 3;
					ui32Remain -= 3;
				}
				if (ui32Remain == 2)
				{
					PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "0x%08x 0x%08x",
							pui32Ptr[0], pui32Ptr[1]));
					pui32Ptr += 2;
					ui32Remain -= 2;
				}
				if (ui32Remain == 1)
				{
					PVR_DPF((SUPPORT_DUMP_CLIENT_CCB_COMMANDS_DBG_LEVEL, "0x%08x",
							pui32Ptr[0]));
					pui32Ptr += 1;
					ui32Remain -= 1;
				}
			}
		}
		pui8Ptr += psHeader->ui32CmdSize;
		ui32ConsumeSize -= sizeof(*psHeader) + psHeader->ui32CmdSize;
	}
#else
	PVR_UNREFERENCED_PARAMETER(psClientCCB);
	PVR_UNREFERENCED_PARAMETER(ui32Offset);
	PVR_UNREFERENCED_PARAMETER(ui32ByteCount);
#endif
}

/*
	Workout how much space this command will require
*/
PVRSRV_ERROR RGXCmdHelperInitCmdCCB(RGX_CLIENT_CCB            *psClientCCB,
                                    IMG_UINT32                ui32ClientFenceCount,
                                    PRGXFWIF_UFO_ADDR         *pauiFenceUFOAddress,
                                    IMG_UINT32                *paui32FenceValue,
                                    IMG_UINT32                ui32ClientUpdateCount,
                                    PRGXFWIF_UFO_ADDR         *pauiUpdateUFOAddress,
                                    IMG_UINT32                *paui32UpdateValue,
                                    IMG_UINT32                ui32ServerSyncCount,
                                    IMG_UINT32                *paui32ServerSyncFlags,
                                    IMG_UINT32                ui32ServerSyncFlagMask,
                                    SERVER_SYNC_PRIMITIVE     **papsServerSyncs,
                                    IMG_UINT32                ui32CmdSize,
                                    IMG_PBYTE                 pui8DMCmd,
                                    PRGXFWIF_TIMESTAMP_ADDR   *ppPreAddr,
                                    PRGXFWIF_TIMESTAMP_ADDR   *ppPostAddr,
                                    PRGXFWIF_UFO_ADDR         *ppRMWUFOAddr,
                                    RGXFWIF_CCB_CMD_TYPE      eType,
                                    IMG_UINT32                ui32ExtJobRef,
                                    IMG_UINT32                ui32IntJobRef,
                                    IMG_UINT32                ui32PDumpFlags,
                                    RGXFWIF_WORKEST_KICK_DATA *psWorkEstKickData,
                                    IMG_CHAR                  *pszCommandName,
                                    IMG_BOOL                  bCCBStateOpen,
                                    RGX_CCB_CMD_HELPER_DATA   *psCmdHelperData,
									IMG_DEV_VIRTADDR		  sRobustnessResetReason)
{
	IMG_UINT32 ui32FenceCount;
	IMG_UINT32 ui32UpdateCount;
	IMG_UINT32 i;

	/* Job reference values */
	psCmdHelperData->ui32ExtJobRef = ui32ExtJobRef;
	psCmdHelperData->ui32IntJobRef = ui32IntJobRef;

	/* Save the data we require in the submit call */
	psCmdHelperData->psClientCCB = psClientCCB;
	psCmdHelperData->ui32PDumpFlags = ui32PDumpFlags;
	psCmdHelperData->pszCommandName = pszCommandName;
	psCmdHelperData->psClientCCB->bStateOpen = bCCBStateOpen;

	/* Client sync data */
	psCmdHelperData->ui32ClientFenceCount = ui32ClientFenceCount;
	psCmdHelperData->pauiFenceUFOAddress = pauiFenceUFOAddress;
	psCmdHelperData->paui32FenceValue = paui32FenceValue;
	psCmdHelperData->ui32ClientUpdateCount = ui32ClientUpdateCount;
	psCmdHelperData->pauiUpdateUFOAddress = pauiUpdateUFOAddress;
	psCmdHelperData->paui32UpdateValue = paui32UpdateValue;

	/* Server sync data */
	psCmdHelperData->ui32ServerSyncCount = ui32ServerSyncCount;
	psCmdHelperData->paui32ServerSyncFlags = paui32ServerSyncFlags;
	psCmdHelperData->ui32ServerSyncFlagMask = ui32ServerSyncFlagMask;
	psCmdHelperData->papsServerSyncs = papsServerSyncs;

	/* Command data */
	psCmdHelperData->ui32CmdSize = ui32CmdSize;
	psCmdHelperData->pui8DMCmd = pui8DMCmd;
	psCmdHelperData->eType = eType;

	/* Robustness reset reason address */
	psCmdHelperData->sRobustnessResetReason = sRobustnessResetReason;

	PDUMPCOMMENTWITHFLAGS(ui32PDumpFlags,
			"%s Command Server Init on FWCtx %08x", pszCommandName,
			FWCommonContextGetFWAddress(psClientCCB->psServerCommonContext).ui32Addr);

	/* Init the generated data members */
	psCmdHelperData->ui32ServerFenceCount = 0;
	psCmdHelperData->ui32ServerUpdateCount = 0;
	psCmdHelperData->ui32ServerUnfencedUpdateCount = 0;
	psCmdHelperData->ui32PreTimeStampCmdSize = 0;
	psCmdHelperData->ui32PostTimeStampCmdSize = 0;
	psCmdHelperData->ui32RMWUFOCmdSize = 0;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Workload Data added */
	psCmdHelperData->psWorkEstKickData = psWorkEstKickData;
#endif

	if (ppPreAddr && (ppPreAddr->ui32Addr != 0))
	{

		psCmdHelperData->pPreTimestampAddr = *ppPreAddr;
		psCmdHelperData->ui32PreTimeStampCmdSize = sizeof(RGXFWIF_CCB_CMD_HEADER)
			+ ((sizeof(RGXFWIF_DEV_VIRTADDR) + RGXFWIF_FWALLOC_ALIGN - 1) & ~(RGXFWIF_FWALLOC_ALIGN  - 1));
	}

	if (ppPostAddr && (ppPostAddr->ui32Addr != 0))
	{
		psCmdHelperData->pPostTimestampAddr = *ppPostAddr;
		psCmdHelperData->ui32PostTimeStampCmdSize = sizeof(RGXFWIF_CCB_CMD_HEADER)
			+ ((sizeof(RGXFWIF_DEV_VIRTADDR) + RGXFWIF_FWALLOC_ALIGN - 1) & ~(RGXFWIF_FWALLOC_ALIGN  - 1));
	}

	if (ppRMWUFOAddr && (ppRMWUFOAddr->ui32Addr != 0))
	{
		psCmdHelperData->pRMWUFOAddr       = * ppRMWUFOAddr;
		psCmdHelperData->ui32RMWUFOCmdSize = sizeof(RGXFWIF_CCB_CMD_HEADER) + sizeof(RGXFWIF_UFO);
	}


	/* Workout how many fences and updates this command will have */
	for (i = 0; i < ui32ServerSyncCount; i++)
	{
		IMG_UINT32 ui32Flag = paui32ServerSyncFlags[i] & ui32ServerSyncFlagMask;

		if (ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK)
		{
			/* Server syncs must fence */
			psCmdHelperData->ui32ServerFenceCount++;
		}

		/* If it is an update */
		if (ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE)
		{
			/* is it a fenced update or a progress update (a.k.a unfenced update) ?*/
			if ((ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_UNFENCED_UPDATE) == PVRSRV_CLIENT_SYNC_PRIM_OP_UNFENCED_UPDATE)
			{
				/* it is a progress update */
				psCmdHelperData->ui32ServerUnfencedUpdateCount++;
			}
			else
			{
				/* it is a fenced update */
				psCmdHelperData->ui32ServerUpdateCount++;
			}
		}
	}


	/* Total fence command size (header plus command data) */
	ui32FenceCount = ui32ClientFenceCount + psCmdHelperData->ui32ServerFenceCount;
	if (ui32FenceCount)
	{
		psCmdHelperData->ui32FenceCmdSize = RGX_CCB_FWALLOC_ALIGN((ui32FenceCount * sizeof(RGXFWIF_UFO)) +
																  sizeof(RGXFWIF_CCB_CMD_HEADER));
	}
	else
	{
		psCmdHelperData->ui32FenceCmdSize = 0;
	}

	/* Total DM command size (header plus command data) */
	psCmdHelperData->ui32DMCmdSize = RGX_CCB_FWALLOC_ALIGN(ui32CmdSize +
														   sizeof(RGXFWIF_CCB_CMD_HEADER));

	/* Total update command size (header plus command data) */
	ui32UpdateCount = ui32ClientUpdateCount + psCmdHelperData->ui32ServerUpdateCount;
	if (ui32UpdateCount)
	{
		psCmdHelperData->ui32UpdateCmdSize = RGX_CCB_FWALLOC_ALIGN((ui32UpdateCount * sizeof(RGXFWIF_UFO)) +
																   sizeof(RGXFWIF_CCB_CMD_HEADER));
	}
	else
	{
		psCmdHelperData->ui32UpdateCmdSize = 0;
	}

	/* Total unfenced update command size (header plus command data) */
	if (psCmdHelperData->ui32ServerUnfencedUpdateCount != 0)
	{
		psCmdHelperData->ui32UnfencedUpdateCmdSize = RGX_CCB_FWALLOC_ALIGN((psCmdHelperData->ui32ServerUnfencedUpdateCount * sizeof(RGXFWIF_UFO)) +
																		   sizeof(RGXFWIF_CCB_CMD_HEADER));
	}
	else
	{
		psCmdHelperData->ui32UnfencedUpdateCmdSize = 0;
	}

	return PVRSRV_OK;
}

/*
	Reserve space in the CCB and fill in the command and client sync data
*/
PVRSRV_ERROR RGXCmdHelperAcquireCmdCCB(IMG_UINT32 ui32CmdCount,
									   RGX_CCB_CMD_HELPER_DATA *asCmdHelperData)
{
	IMG_UINT32 ui32AllocSize = 0;
	IMG_UINT32 i;
	IMG_UINT8 *pui8StartPtr;
	PVRSRV_ERROR eError;

	/*
		Workout how much space we need for all the command(s)
	*/
	ui32AllocSize = RGXCmdHelperGetCommandSize(ui32CmdCount, asCmdHelperData);


	for (i = 0; i < ui32CmdCount; i++)
	{
		if ((asCmdHelperData[0].ui32PDumpFlags ^ asCmdHelperData[i].ui32PDumpFlags) & PDUMP_FLAGS_CONTINUOUS)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: PDump continuous is not consistent (%s != %s) for command %d",
					 __FUNCTION__,
					 PDUMP_IS_CONTINUOUS(asCmdHelperData[0].ui32PDumpFlags)?"IMG_TRUE":"IMG_FALSE",
					 PDUMP_IS_CONTINUOUS(asCmdHelperData[i].ui32PDumpFlags)?"IMG_TRUE":"IMG_FALSE",
					 ui32CmdCount));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	/*
		Acquire space in the CCB for all the command(s).
	*/
	eError = RGXAcquireCCB(asCmdHelperData[0].psClientCCB,
						   ui32AllocSize,
						   (void **)&pui8StartPtr,
						   asCmdHelperData[0].ui32PDumpFlags);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/*
		For each command fill in the fence, DM, and update command

		Note:
		We only fill in the client fences here, the server fences (and updates)
		will be filled in together at the end. This is because we might fail the
		kernel CCB alloc and would then have to rollback the server syncs if
		we took the operation here
	*/
	for (i = 0; i < ui32CmdCount; i++)
	{
		RGX_CCB_CMD_HELPER_DATA *psCmdHelperData = & asCmdHelperData[i];
		IMG_UINT8 *pui8CmdPtr;
		IMG_UINT8 *pui8ServerFenceStart = NULL;
		IMG_UINT8 *pui8ServerUpdateStart = NULL;
#if defined(PDUMP)
		IMG_UINT32 ui32CtxAddr = FWCommonContextGetFWAddress(asCmdHelperData->psClientCCB->psServerCommonContext).ui32Addr;
		IMG_UINT32 ui32CcbWoff = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(asCmdHelperData->psClientCCB->psServerCommonContext));
#endif

		if (psCmdHelperData->ui32ClientFenceCount+psCmdHelperData->ui32ClientUpdateCount != 0)
		{
			PDUMPCOMMENT("Start of %s client syncs for cmd[%d] on FWCtx %08x Woff 0x%x bytes",
					psCmdHelperData->psClientCCB->szName, i, ui32CtxAddr, ui32CcbWoff);
		}

		/*
			Create the fence command.
		*/
		if (psCmdHelperData->ui32FenceCmdSize)
		{
			RGXFWIF_CCB_CMD_HEADER *psHeader;
			IMG_UINT k, uiNextValueIndex;
			PVRSRV_RGXDEV_INFO *psDevInfo = FWCommonContextGetRGXDevInfo(psCmdHelperData->psClientCCB->psServerCommonContext);

			/* Fences are at the start of the command */
			pui8CmdPtr = pui8StartPtr;

			psHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8CmdPtr;
			psHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_FENCE;
			/* Assign this Fence a 'timestamp' (and increment it) */
			psHeader->ui32SubmissionOrdinal = OSAtomicIncrement(&psDevInfo->iCCBSubmissionOrdinal);

			psHeader->ui32CmdSize = psCmdHelperData->ui32FenceCmdSize - sizeof(RGXFWIF_CCB_CMD_HEADER);
			psHeader->ui32ExtJobRef = psCmdHelperData->ui32ExtJobRef;
			psHeader->ui32IntJobRef = psCmdHelperData->ui32IntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
			psHeader->sWorkEstKickData.ui64ReturnDataIndex = 0;
			psHeader->sWorkEstKickData.ui64Deadline = 0;
			psHeader->sWorkEstKickData.ui64CyclesPrediction = 0;
#endif

			pui8CmdPtr += sizeof(RGXFWIF_CCB_CMD_HEADER);

			/* Fill in the client fences */
			uiNextValueIndex = 0;
			for (k = 0; k < psCmdHelperData->ui32ClientFenceCount; k++)
			{
				RGXFWIF_UFO *psUFOPtr = (RGXFWIF_UFO *) pui8CmdPtr;
	
				psUFOPtr->puiAddrUFO = psCmdHelperData->pauiFenceUFOAddress[k];

				if (PVRSRV_UFO_IS_SYNC_CHECKPOINT(psUFOPtr))
				{
					psUFOPtr->ui32Value = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;
				}
				else
				{
					/* Only increment uiNextValueIndex for non sync checkpoints
					 * (as paui32FenceValue only contains values for sync prims)
					 */
					psUFOPtr->ui32Value = psCmdHelperData->paui32FenceValue[uiNextValueIndex++];
				}
				pui8CmdPtr += sizeof(RGXFWIF_UFO);

#if defined SYNC_COMMAND_DEBUG
				PVR_DPF((PVR_DBG_ERROR, "%s client sync fence - 0x%x -> 0x%x",
						psCmdHelperData->psClientCCB->szName, psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value));
#endif
				PDUMPCOMMENT(".. %s client sync fence - 0x%x -> 0x%x",
						psCmdHelperData->psClientCCB->szName, psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value);


			}
			pui8ServerFenceStart = pui8CmdPtr;
		}

		/* jump over the Server fences */
		pui8CmdPtr = pui8StartPtr + psCmdHelperData->ui32FenceCmdSize;


		/*
		  Create the pre DM timestamp commands. Pre and Post timestamp commands are supposed to
		  sandwich the DM cmd. The padding code with the CCB wrap upsets the FW if we don't have
		  the task type bit cleared for POST_TIMESTAMPs. That's why we have 2 different cmd types.
		*/
		if (psCmdHelperData->ui32PreTimeStampCmdSize != 0)
		{
			RGXWriteTimestampCommand(& pui8CmdPtr,
			                         RGXFWIF_CCB_CMD_TYPE_PRE_TIMESTAMP,
			                         psCmdHelperData->pPreTimestampAddr);
		}

		/*
			Create the DM command
		*/
		if (psCmdHelperData->ui32DMCmdSize)
		{
			RGXFWIF_CCB_CMD_HEADER *psHeader;

			psHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8CmdPtr;
			psHeader->eCmdType = psCmdHelperData->eType;
			{
				PVRSRV_RGXDEV_INFO *psDevInfo = FWCommonContextGetRGXDevInfo(psCmdHelperData->psClientCCB->psServerCommonContext);

				if (psCmdHelperData->eType == RGXFWIF_CCB_CMD_TYPE_FENCE_PR)
				{
					/* Assign this PR Fence a timestamp (and increment it) */
					psHeader->ui32SubmissionOrdinal = OSAtomicIncrement(&psDevInfo->iCCBSubmissionOrdinal);
				}
			}

			psHeader->ui32CmdSize = psCmdHelperData->ui32DMCmdSize - sizeof(RGXFWIF_CCB_CMD_HEADER);
			psHeader->ui32ExtJobRef = psCmdHelperData->ui32ExtJobRef;
			psHeader->ui32IntJobRef = psCmdHelperData->ui32IntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
			if (psCmdHelperData->psWorkEstKickData != NULL)
			{
				PVR_ASSERT(psCmdHelperData->eType == RGXFWIF_CCB_CMD_TYPE_TA ||
				           psCmdHelperData->eType == RGXFWIF_CCB_CMD_TYPE_3D);
				psHeader->sWorkEstKickData = *psCmdHelperData->psWorkEstKickData;
			}
			else
			{
				psHeader->sWorkEstKickData.ui64ReturnDataIndex = 0;
				psHeader->sWorkEstKickData.ui64Deadline = 0;
				psHeader->sWorkEstKickData.ui64CyclesPrediction = 0;
			}
#endif

			psHeader->sRobustnessResetReason = psCmdHelperData->sRobustnessResetReason;

			pui8CmdPtr += sizeof(RGXFWIF_CCB_CMD_HEADER);

			/* The buffer is write-combine, so no special device memory treatment required. */
			OSCachedMemCopy(pui8CmdPtr, psCmdHelperData->pui8DMCmd, psCmdHelperData->ui32CmdSize);
			pui8CmdPtr += psCmdHelperData->ui32CmdSize;
		}

		if (psCmdHelperData->ui32PostTimeStampCmdSize != 0)
		{
			RGXWriteTimestampCommand(& pui8CmdPtr,
			                         RGXFWIF_CCB_CMD_TYPE_POST_TIMESTAMP,
			                         psCmdHelperData->pPostTimestampAddr);
		}


		if (psCmdHelperData->ui32RMWUFOCmdSize != 0)
		{
			RGXFWIF_CCB_CMD_HEADER * psHeader;
			RGXFWIF_UFO            * psUFO;

			psHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8CmdPtr;
			psHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_RMW_UPDATE;
			psHeader->ui32CmdSize = psCmdHelperData->ui32RMWUFOCmdSize - sizeof(RGXFWIF_CCB_CMD_HEADER);
			psHeader->ui32ExtJobRef = psCmdHelperData->ui32ExtJobRef;
			psHeader->ui32IntJobRef = psCmdHelperData->ui32IntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
			psHeader->sWorkEstKickData.ui64ReturnDataIndex = 0;
			psHeader->sWorkEstKickData.ui64Deadline = 0;
			psHeader->sWorkEstKickData.ui64CyclesPrediction = 0;
#endif
			pui8CmdPtr += sizeof(RGXFWIF_CCB_CMD_HEADER);

			psUFO = (RGXFWIF_UFO *) pui8CmdPtr;
			psUFO->puiAddrUFO = psCmdHelperData->pRMWUFOAddr;
			
			pui8CmdPtr += sizeof(RGXFWIF_UFO);
		}
	

		/*
			Create the update command.
			
			Note:
			We only fill in the client updates here, the server updates (and fences)
			will be filled in together at the end
		*/
		if (psCmdHelperData->ui32UpdateCmdSize)
		{
			RGXFWIF_CCB_CMD_HEADER *psHeader;
			IMG_UINT k, uiNextValueIndex;

			psHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8CmdPtr;
			psHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_UPDATE;
			psHeader->ui32CmdSize = psCmdHelperData->ui32UpdateCmdSize - sizeof(RGXFWIF_CCB_CMD_HEADER);
			psHeader->ui32ExtJobRef = psCmdHelperData->ui32ExtJobRef;
			psHeader->ui32IntJobRef = psCmdHelperData->ui32IntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
			psHeader->sWorkEstKickData.ui64ReturnDataIndex = 0;
			psHeader->sWorkEstKickData.ui64Deadline = 0;
			psHeader->sWorkEstKickData.ui64CyclesPrediction = 0;
#endif
			pui8CmdPtr += sizeof(RGXFWIF_CCB_CMD_HEADER);

			/* Fill in the client updates */
			uiNextValueIndex = 0;
			for (k = 0; k < psCmdHelperData->ui32ClientUpdateCount; k++)
			{
				RGXFWIF_UFO *psUFOPtr = (RGXFWIF_UFO *) pui8CmdPtr;
	
				psUFOPtr->puiAddrUFO = psCmdHelperData->pauiUpdateUFOAddress[k];
				if (PVRSRV_UFO_IS_SYNC_CHECKPOINT(psUFOPtr))
				{
					psUFOPtr->ui32Value = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;
				}
				else
				{
					/* Only increment uiNextValueIndex for non sync checkpoints
					 * (as paui32UpdateValue only contains values for sync prims)
					 */
					psUFOPtr->ui32Value = psCmdHelperData->paui32UpdateValue[uiNextValueIndex++];
				}
				pui8CmdPtr += sizeof(RGXFWIF_UFO);

#if defined SYNC_COMMAND_DEBUG
				PVR_DPF((PVR_DBG_ERROR, "%s client sync update - 0x%x -> 0x%x",
						psCmdHelperData->psClientCCB->szName, psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value));
#endif
				PDUMPCOMMENT(".. %s client sync update - 0x%x -> 0x%x",
						psCmdHelperData->psClientCCB->szName, psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value);

			}
			pui8ServerUpdateStart = pui8CmdPtr;
		}
	
		/* Save the server sync fence & update offsets for submit time */
		psCmdHelperData->pui8ServerFenceStart  = pui8ServerFenceStart;
		psCmdHelperData->pui8ServerUpdateStart = pui8ServerUpdateStart;

		/* jump over the fenced update */
		if (psCmdHelperData->ui32UnfencedUpdateCmdSize != 0)
		{
			RGXFWIF_CCB_CMD_HEADER * const psHeader = (RGXFWIF_CCB_CMD_HEADER * ) psCmdHelperData->pui8ServerUpdateStart + psCmdHelperData->ui32UpdateCmdSize;
			/* set up the header for unfenced updates,  */
			PVR_ASSERT(psHeader); /* Could be zero if ui32UpdateCmdSize is 0 which is never expected */
			psHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_UNFENCED_UPDATE;
			psHeader->ui32CmdSize = psCmdHelperData->ui32UnfencedUpdateCmdSize - sizeof(RGXFWIF_CCB_CMD_HEADER);
			psHeader->ui32ExtJobRef = psCmdHelperData->ui32ExtJobRef;
			psHeader->ui32IntJobRef = psCmdHelperData->ui32IntJobRef;
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
			psHeader->sWorkEstKickData.ui64ReturnDataIndex = 0;
			psHeader->sWorkEstKickData.ui64Deadline = 0;
			psHeader->sWorkEstKickData.ui64CyclesPrediction = 0;
#endif

			/* jump over the header */
			psCmdHelperData->pui8ServerUnfencedUpdateStart = ((IMG_UINT8*) psHeader) + sizeof(RGXFWIF_CCB_CMD_HEADER);
		}
		else
		{
			psCmdHelperData->pui8ServerUnfencedUpdateStart = NULL;
		}
		
		/* Save start for sanity checking at submit time */
		psCmdHelperData->pui8StartPtr = pui8StartPtr;

		/* Set the start pointer for the next iteration around the loop */
		pui8StartPtr +=
			psCmdHelperData->ui32FenceCmdSize         +
			psCmdHelperData->ui32PreTimeStampCmdSize  +
			psCmdHelperData->ui32DMCmdSize            +
			psCmdHelperData->ui32PostTimeStampCmdSize +
			psCmdHelperData->ui32RMWUFOCmdSize        +
			psCmdHelperData->ui32UpdateCmdSize        +
			psCmdHelperData->ui32UnfencedUpdateCmdSize;

		if (psCmdHelperData->ui32ClientFenceCount+psCmdHelperData->ui32ClientUpdateCount != 0)
		{
			PDUMPCOMMENT("End of %s client syncs for cmd[%d] on FWCtx %08x Woff 0x%x bytes",
					psCmdHelperData->psClientCCB->szName, i, ui32CtxAddr, ui32CcbWoff);
		}
		else
		{
			PDUMPCOMMENT("No %s client syncs for cmd[%d] on FWCtx %08x Woff 0x%x bytes",
					psCmdHelperData->psClientCCB->szName, i, ui32CtxAddr, ui32CcbWoff);
		}
	}

	return PVRSRV_OK;
}

/*
	Fill in the server syncs data and release the CCB space
*/
void RGXCmdHelperReleaseCmdCCB(IMG_UINT32 ui32CmdCount,
							   RGX_CCB_CMD_HELPER_DATA *asCmdHelperData,
							   const IMG_CHAR *pcszDMName,
							   IMG_UINT32 ui32CtxAddr)
{
	IMG_UINT32 ui32AllocSize = 0;
	IMG_UINT32 i;
#if defined(LINUX)
	IMG_BOOL bTraceChecks = trace_rogue_are_fence_checks_traced();
	IMG_BOOL bTraceUpdates = trace_rogue_are_fence_updates_traced();
#endif

	/*
		Workout how much space we need for all the command(s)
	*/
	ui32AllocSize = RGXCmdHelperGetCommandSize(ui32CmdCount, asCmdHelperData);
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	PVRSRVLockServerSync();
#endif

   /*
		For each command fill in the server sync info
	*/
	for (i=0;i<ui32CmdCount;i++)
	{
		RGX_CCB_CMD_HELPER_DATA *psCmdHelperData = &asCmdHelperData[i];
		IMG_UINT8 *pui8ServerFenceStart = psCmdHelperData->pui8ServerFenceStart;
		IMG_UINT8 *pui8ServerUpdateStart = psCmdHelperData->pui8ServerUpdateStart;
		IMG_UINT8 *pui8ServerUnfencedUpdateStart = psCmdHelperData->pui8ServerUnfencedUpdateStart;
		IMG_UINT32 j;

		/* Now fill in the server fence and updates together */
		for (j = 0; j < psCmdHelperData->ui32ServerSyncCount; j++)
		{
			RGXFWIF_UFO *psUFOPtr;
			IMG_UINT32 ui32UpdateValue;
			IMG_UINT32 ui32FenceValue;
			IMG_UINT32 ui32SyncAddr;
			PVRSRV_ERROR eError;
			IMG_UINT32 ui32Flag = psCmdHelperData->paui32ServerSyncFlags[j] & psCmdHelperData->ui32ServerSyncFlagMask;
			IMG_BOOL bFence = ((ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK)!=0)?IMG_TRUE:IMG_FALSE;
			IMG_BOOL bUpdate = ((ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE)!=0)?IMG_TRUE:IMG_FALSE;
			const IMG_BOOL bUnfencedUpdate = ((ui32Flag & PVRSRV_CLIENT_SYNC_PRIM_OP_UNFENCED_UPDATE) == PVRSRV_CLIENT_SYNC_PRIM_OP_UNFENCED_UPDATE)
				? IMG_TRUE
				: IMG_FALSE;

			eError = PVRSRVServerSyncQueueHWOpKM_NoGlobalLock(psCmdHelperData->papsServerSyncs[j],
												 bUpdate,
												 &ui32FenceValue,
												 &ui32UpdateValue);
			/* This function can't fail */
			PVR_ASSERT(eError == PVRSRV_OK);
	
			/*
				As server syncs always fence (we have a check in RGXCmcdHelperInitCmdCCB
				which ensures the client is playing ball) the filling in of the fence
				is unconditional.
			*/
			eError = ServerSyncGetFWAddr(psCmdHelperData->papsServerSyncs[j], &ui32SyncAddr);
			if (PVRSRV_OK != eError)
			{
				PVR_DPF((PVR_DBG_ERROR,
					"%s: Failed to read Server Sync FW address (%d)",
					__FUNCTION__, eError));
				PVR_ASSERT(eError == PVRSRV_OK);
			}
			if (bFence)
			{
				PVR_ASSERT(pui8ServerFenceStart != NULL);

				psUFOPtr = (RGXFWIF_UFO *) pui8ServerFenceStart;
				psUFOPtr->puiAddrUFO.ui32Addr = ui32SyncAddr;
				psUFOPtr->ui32Value = ui32FenceValue;
				pui8ServerFenceStart += sizeof(RGXFWIF_UFO);

#if defined(LINUX)
				if (bTraceChecks)
				{
					trace_rogue_fence_checks(psCmdHelperData->pszCommandName,
											 pcszDMName,
											 ui32CtxAddr,
											 psCmdHelperData->psClientCCB->ui32HostWriteOffset + ui32AllocSize,
											 1,
											 &psUFOPtr->puiAddrUFO,
											 &psUFOPtr->ui32Value);
				}
#endif
			}
	
			/* If there is an update then fill that in as well */
			if (bUpdate)
			{
				if (bUnfencedUpdate)
				{
					PVR_ASSERT(pui8ServerUnfencedUpdateStart != NULL);

					psUFOPtr = (RGXFWIF_UFO *) pui8ServerUnfencedUpdateStart;
					psUFOPtr->puiAddrUFO.ui32Addr = ui32SyncAddr;
					psUFOPtr->ui32Value = ui32UpdateValue;
					pui8ServerUnfencedUpdateStart += sizeof(RGXFWIF_UFO);
				}
				else
				{
					/* fenced update */
					PVR_ASSERT(pui8ServerUpdateStart != NULL);

					psUFOPtr = (RGXFWIF_UFO *) pui8ServerUpdateStart;
					psUFOPtr->puiAddrUFO.ui32Addr = ui32SyncAddr;
					psUFOPtr->ui32Value = ui32UpdateValue;
					pui8ServerUpdateStart += sizeof(RGXFWIF_UFO);
				}
#if defined(LINUX)
				if (bTraceUpdates)
				{
					trace_rogue_fence_updates(psCmdHelperData->pszCommandName,
											  pcszDMName,
											  ui32CtxAddr,
											  psCmdHelperData->psClientCCB->ui32HostWriteOffset + ui32AllocSize,
											  1,
											  &psUFOPtr->puiAddrUFO,
											  &psUFOPtr->ui32Value);
				}
#endif
				
#if defined(NO_HARDWARE)
				/*
				  There is no FW so the host has to do any Sync updates
				  (client sync updates are done in the client
				*/
				PVRSRVServerSyncPrimSetKM(psCmdHelperData->papsServerSyncs[j], ui32UpdateValue);
#endif
			}
		}

#if defined(LINUX)
		if (bTraceChecks)
		{
			trace_rogue_fence_checks(psCmdHelperData->pszCommandName,
									 pcszDMName,
									 ui32CtxAddr,
									 psCmdHelperData->psClientCCB->ui32HostWriteOffset + ui32AllocSize,
									 psCmdHelperData->ui32ClientFenceCount,
									 psCmdHelperData->pauiFenceUFOAddress,
									 psCmdHelperData->paui32FenceValue);
		}
		if (bTraceUpdates)
		{
			trace_rogue_fence_updates(psCmdHelperData->pszCommandName,
									  pcszDMName,
									  ui32CtxAddr,
									  psCmdHelperData->psClientCCB->ui32HostWriteOffset + ui32AllocSize,
									  psCmdHelperData->ui32ClientUpdateCount,
									  psCmdHelperData->pauiUpdateUFOAddress,
									  psCmdHelperData->paui32UpdateValue);
		}
#endif

		if (psCmdHelperData->ui32ServerSyncCount)
		{
			/*
				Do some sanity checks to ensure we did the point math right
			*/
			if (pui8ServerFenceStart != NULL)
			{
				PVR_ASSERT(pui8ServerFenceStart ==
						   (psCmdHelperData->pui8StartPtr +
						   psCmdHelperData->ui32FenceCmdSize));
			}

			if (pui8ServerUpdateStart != NULL)
			{
				PVR_ASSERT(pui8ServerUpdateStart ==
				           psCmdHelperData->pui8StartPtr             +
				           psCmdHelperData->ui32FenceCmdSize         +
				           psCmdHelperData->ui32PreTimeStampCmdSize  +
				           psCmdHelperData->ui32DMCmdSize            +
				           psCmdHelperData->ui32RMWUFOCmdSize        +
				           psCmdHelperData->ui32PostTimeStampCmdSize +
				           psCmdHelperData->ui32UpdateCmdSize);
			}

			if (pui8ServerUnfencedUpdateStart != NULL)
			{
				PVR_ASSERT(pui8ServerUnfencedUpdateStart ==
				           psCmdHelperData->pui8StartPtr             +
				           psCmdHelperData->ui32FenceCmdSize         +
				           psCmdHelperData->ui32PreTimeStampCmdSize  +
				           psCmdHelperData->ui32DMCmdSize            +
				           psCmdHelperData->ui32RMWUFOCmdSize        +
				           psCmdHelperData->ui32PostTimeStampCmdSize +
				           psCmdHelperData->ui32UpdateCmdSize        +
				           psCmdHelperData->ui32UnfencedUpdateCmdSize);
			}
		}
	
		/*
			All the commands have been filled in so release the CCB space.
			The FW still won't run this command until we kick it
		*/
		PDUMPCOMMENTWITHFLAGS(psCmdHelperData->ui32PDumpFlags,
				"%s Command Server Release on FWCtx %08x",
				psCmdHelperData->pszCommandName, ui32CtxAddr);
	}
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	PVRSRVUnlockServerSync();
#endif

	_RGXClientCCBDumpCommands(asCmdHelperData[0].psClientCCB,
							  asCmdHelperData[0].psClientCCB->ui32HostWriteOffset,
							  ui32AllocSize);

	RGXReleaseCCB(asCmdHelperData[0].psClientCCB,
				  ui32AllocSize,
				  asCmdHelperData[0].ui32PDumpFlags);

	asCmdHelperData[0].psClientCCB->bStateOpen = IMG_FALSE;
}

IMG_UINT32 RGXCmdHelperGetCommandSize(IMG_UINT32              ui32CmdCount,
                                      RGX_CCB_CMD_HELPER_DATA *asCmdHelperData)
{
	IMG_UINT32 ui32AllocSize = 0;
	IMG_UINT32 i;

	/*
		Workout how much space we need for all the command(s)
	*/
	for (i = 0; i < ui32CmdCount; i++)
	{
		ui32AllocSize +=
			asCmdHelperData[i].ui32FenceCmdSize          +
			asCmdHelperData[i].ui32DMCmdSize             +
			asCmdHelperData[i].ui32UpdateCmdSize         +
			asCmdHelperData[i].ui32UnfencedUpdateCmdSize +
			asCmdHelperData[i].ui32PreTimeStampCmdSize   +
			asCmdHelperData[i].ui32PostTimeStampCmdSize  +
			asCmdHelperData[i].ui32RMWUFOCmdSize;
	}

	return ui32AllocSize;
}

/* Work out how much of an offset there is to a specific command. */
IMG_UINT32 RGXCmdHelperGetCommandOffset(RGX_CCB_CMD_HELPER_DATA *asCmdHelperData,
                                        IMG_UINT32              ui32Cmdindex)
{
	IMG_UINT32 ui32Offset = 0;
	IMG_UINT32 i;

	for (i = 0; i < ui32Cmdindex; i++)
	{
		ui32Offset +=
			asCmdHelperData[i].ui32FenceCmdSize          +
			asCmdHelperData[i].ui32DMCmdSize             +
			asCmdHelperData[i].ui32UpdateCmdSize         +
			asCmdHelperData[i].ui32UnfencedUpdateCmdSize +
			asCmdHelperData[i].ui32PreTimeStampCmdSize   +
			asCmdHelperData[i].ui32PostTimeStampCmdSize  +
			asCmdHelperData[i].ui32RMWUFOCmdSize;
	}

	return ui32Offset;
}

/* Returns the offset of the data master command from a write offset */
IMG_UINT32 RGXCmdHelperGetDMCommandHeaderOffset(RGX_CCB_CMD_HELPER_DATA *psCmdHelperData)
{
	return psCmdHelperData->ui32FenceCmdSize + psCmdHelperData->ui32PreTimeStampCmdSize;
}

static const char *_CCBCmdTypename(RGXFWIF_CCB_CMD_TYPE cmdType)
{
	switch (cmdType)
	{
		case RGXFWIF_CCB_CMD_TYPE_TA: return "TA";
		case RGXFWIF_CCB_CMD_TYPE_3D: return "3D";
		case RGXFWIF_CCB_CMD_TYPE_CDM: return "CDM";
		case RGXFWIF_CCB_CMD_TYPE_TQ_3D: return "TQ_3D";
		case RGXFWIF_CCB_CMD_TYPE_TQ_2D: return "TQ_2D";
		case RGXFWIF_CCB_CMD_TYPE_3D_PR: return "3D_PR";
		case RGXFWIF_CCB_CMD_TYPE_NULL: return "NULL";
		case RGXFWIF_CCB_CMD_TYPE_SHG: return "SHG";
		case RGXFWIF_CCB_CMD_TYPE_RTU: return "RTU";
		case RGXFWIF_CCB_CMD_TYPE_RTU_FC: return "RTU_FC";
		case RGXFWIF_CCB_CMD_TYPE_PRE_TIMESTAMP: return "PRE_TIMESTAMP";
		case RGXFWIF_CCB_CMD_TYPE_TQ_TDM: return "TQ_TDM";

		case RGXFWIF_CCB_CMD_TYPE_FENCE: return "FENCE";
		case RGXFWIF_CCB_CMD_TYPE_UPDATE: return "UPDATE";
		case RGXFWIF_CCB_CMD_TYPE_RMW_UPDATE: return "RMW_UPDATE";
		case RGXFWIF_CCB_CMD_TYPE_FENCE_PR: return "FENCE_PR";
		case RGXFWIF_CCB_CMD_TYPE_PRIORITY: return "PRIORITY";

		case RGXFWIF_CCB_CMD_TYPE_POST_TIMESTAMP: return "POST_TIMESTAMP";
		case RGXFWIF_CCB_CMD_TYPE_UNFENCED_UPDATE: return "UNFENCED_UPDATE";
		case RGXFWIF_CCB_CMD_TYPE_UNFENCED_RMW_UPDATE: return "UNFENCED_RMW_UPDATE";

		case RGXFWIF_CCB_CMD_TYPE_PADDING: return "PADDING";

		default:
			PVR_ASSERT(IMG_FALSE);
		break;
	}
	
	return "INVALID";
}

PVRSRV_ERROR CheckForStalledCCB(PVRSRV_DEVICE_NODE *psDevNode, RGX_CLIENT_CCB  *psCurrentClientCCB, RGX_KICK_TYPE_DM eKickTypeDM)
{
	volatile RGXFWIF_CCCB_CTL	*psClientCCBCtrl;
	IMG_UINT32 					ui32SampledRdOff, ui32SampledDpOff, ui32SampledWrOff;
	PVRSRV_ERROR				eError = PVRSRV_OK;

	if (psCurrentClientCCB == NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "CheckForStalledCCB: CCCB is NULL"));
		return  PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	psClientCCBCtrl = psCurrentClientCCB->psClientCCBCtrl;
	ui32SampledRdOff = psClientCCBCtrl->ui32ReadOffset;
	ui32SampledDpOff = psClientCCBCtrl->ui32DepOffset;
	ui32SampledWrOff = psCurrentClientCCB->ui32HostWriteOffset;

	if (ui32SampledRdOff > psClientCCBCtrl->ui32WrapMask  ||
		ui32SampledDpOff > psClientCCBCtrl->ui32WrapMask  ||
		ui32SampledWrOff > psClientCCBCtrl->ui32WrapMask)
	{
		PVR_DPF((PVR_DBG_WARNING, "CheckForStalledCCB: CCCB has invalid offset (ROFF=%d DOFF=%d WOFF=%d)",
				ui32SampledRdOff, ui32SampledDpOff, ui32SampledWrOff));
		return  PVRSRV_ERROR_INVALID_OFFSET;
	}

	if (ui32SampledRdOff != ui32SampledWrOff &&
				psCurrentClientCCB->ui32LastROff != psCurrentClientCCB->ui32LastWOff &&
				ui32SampledRdOff == psCurrentClientCCB->ui32LastROff &&
				(psCurrentClientCCB->ui32ByteCount - psCurrentClientCCB->ui32LastByteCount) < psCurrentClientCCB->ui32Size)
	{
		PVRSRV_RGXDEV_INFO *psDevInfo = (PVRSRV_RGXDEV_INFO*)psDevNode->pvDevice;

		/* Only log a stalled CCB if GPU is idle (any state other than POW_ON is considered idle) */
		if (psDevInfo->psRGXFWIfTraceBuf->ePowState != RGXFWIF_POW_ON)
		{
			static __maybe_unused const char *pszStalledAction =
#if defined(PVRSRV_STALLED_CCB_ACTION)
					"force";
#else
					"warn";
#endif
			/* Don't log this by default unless debugging since a higher up
			 * function will log the stalled condition. Helps avoid double
			 * messages in the log.
			 */
			PVR_DPF((PVR_DBG_ERROR, "%s (%s): CCCB has not progressed (ROFF=%d DOFF=%d WOFF=%d) for \"%s\"",
					__func__, pszStalledAction, ui32SampledRdOff,
					ui32SampledDpOff, ui32SampledWrOff,
					(IMG_PCHAR)&psCurrentClientCCB->szName));
			eError = PVRSRV_ERROR_CCCB_STALLED;

			{
				IMG_UINT8				*pui8ClientCCBBuff = psCurrentClientCCB->pui8ClientCCB;
				RGXFWIF_CCB_CMD_HEADER  *psCommandHeader = (RGXFWIF_CCB_CMD_HEADER *)(pui8ClientCCBBuff + ui32SampledRdOff);
				PVRSRV_RGXDEV_INFO		*psDevInfo = FWCommonContextGetRGXDevInfo(psCurrentClientCCB->psServerCommonContext);

				/* Only try to recover a 'stalled' context (ie one waiting on a fence), as some work (eg compute) could
				 * take a long time to complete, during which time the CCB ptrs would not advance.
				 */
				if ((psCommandHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_FENCE) ||
				    (psCommandHeader->eCmdType == RGXFWIF_CCB_CMD_TYPE_FENCE_PR))
				{
					/* Acquire the cCCB recovery lock */
					OSLockAcquire(psDevInfo->hCCBRecoveryLock);

					if (!psDevInfo->pvEarliestStalledClientCCB)
					{

						psDevInfo->pvEarliestStalledClientCCB = (void*)psCurrentClientCCB;
						psDevInfo->ui32OldestSubmissionOrdinal = psCommandHeader->ui32SubmissionOrdinal;
					}
					else
					{
						/* Check if this fence cmd header has an older submission stamp than the one we are currently considering unblocking
						 * (account for submission stamp wrap by checking diff is less than 0x80000000) - if it is older, then this becomes
						 * our preferred fence to be unblocked/
						 */
						if ((psCommandHeader->ui32SubmissionOrdinal < psDevInfo->ui32OldestSubmissionOrdinal) &&
						    ((psDevInfo->ui32OldestSubmissionOrdinal - psCommandHeader->ui32SubmissionOrdinal) < 0x8000000))
						{
							psDevInfo->pvEarliestStalledClientCCB = (void*)psCurrentClientCCB;
							psDevInfo->ui32OldestSubmissionOrdinal = psCommandHeader->ui32SubmissionOrdinal;
						}
					}

					/* Release the cCCB recovery lock */
					OSLockRelease(psDevInfo->hCCBRecoveryLock);
				}
			}
		}
	}

	psCurrentClientCCB->ui32LastROff = ui32SampledRdOff;
	psCurrentClientCCB->ui32LastWOff = ui32SampledWrOff;
	psCurrentClientCCB->ui32LastByteCount = psCurrentClientCCB->ui32ByteCount;

	return eError;
}

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING) || defined(PVRSRV_ENABLE_FULL_CCB_DUMP)
void DumpCCB(PVRSRV_RGXDEV_INFO *psDevInfo,
			PRGXFWIF_FWCOMMONCONTEXT sFWCommonContext,
			RGX_CLIENT_CCB  *psCurrentClientCCB,
			DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
			void *pvDumpDebugFile)
{
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	PVRSRV_DEVICE_NODE *psDeviceNode = psDevInfo->psDeviceNode;
#endif
	volatile RGXFWIF_CCCB_CTL *psClientCCBCtrl = psCurrentClientCCB->psClientCCBCtrl;
	IMG_UINT8 *pui8ClientCCBBuff = psCurrentClientCCB->pui8ClientCCB;
	IMG_UINT32 ui32Offset = psClientCCBCtrl->ui32ReadOffset;
	IMG_UINT32 ui32DepOffset = psClientCCBCtrl->ui32DepOffset;
	IMG_UINT32 ui32EndOffset = psCurrentClientCCB->ui32HostWriteOffset;
	IMG_UINT32 ui32WrapMask = psClientCCBCtrl->ui32WrapMask;
	IMG_CHAR * pszState = "Ready";

	PVR_DUMPDEBUG_LOG("FWCtx 0x%08X (%s)", sFWCommonContext.ui32Addr,
		(IMG_PCHAR)&psCurrentClientCCB->szName);
	if (ui32Offset == ui32EndOffset)
	{
		PVR_DUMPDEBUG_LOG("  `--<Empty>");
	}

	while (ui32Offset != ui32EndOffset)
	{
		RGXFWIF_CCB_CMD_HEADER *psCmdHeader = (RGXFWIF_CCB_CMD_HEADER*)(pui8ClientCCBBuff + ui32Offset);
		IMG_UINT32 ui32NextOffset = (ui32Offset + psCmdHeader->ui32CmdSize + sizeof(RGXFWIF_CCB_CMD_HEADER)) & ui32WrapMask;
		IMG_BOOL bLastCommand = (ui32NextOffset == ui32EndOffset)? IMG_TRUE: IMG_FALSE;
		IMG_BOOL bLastUFO;
		#define CCB_SYNC_INFO_LEN 80
		IMG_CHAR pszSyncInfo[CCB_SYNC_INFO_LEN];
		IMG_UINT32 ui32NoOfUpdates, i;
		RGXFWIF_UFO *psUFOPtr;

		ui32NoOfUpdates = psCmdHeader->ui32CmdSize / sizeof(RGXFWIF_UFO);
		psUFOPtr = (RGXFWIF_UFO*)(pui8ClientCCBBuff + ui32Offset + sizeof(RGXFWIF_CCB_CMD_HEADER));
		pszSyncInfo[0] = '\0';

		if (ui32Offset == ui32DepOffset)
		{
			pszState = "Waiting";
		}

		PVR_DUMPDEBUG_LOG("  %s--%s %s @ %u Int=%u Ext=%u",
			bLastCommand? "`": "|",
			pszState, _CCBCmdTypename(psCmdHeader->eCmdType),
			ui32Offset, psCmdHeader->ui32IntJobRef, psCmdHeader->ui32ExtJobRef
			);

		/* switch on type and write checks and updates */
		switch (psCmdHeader->eCmdType)
		{
			case RGXFWIF_CCB_CMD_TYPE_UPDATE:
			case RGXFWIF_CCB_CMD_TYPE_UNFENCED_UPDATE:
			case RGXFWIF_CCB_CMD_TYPE_FENCE:
			case RGXFWIF_CCB_CMD_TYPE_FENCE_PR:
			{
				for (i = 0; i < ui32NoOfUpdates; i++, psUFOPtr++)
				{
					bLastUFO = (ui32NoOfUpdates-1 == i)? IMG_TRUE: IMG_FALSE;
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
					if (PVRSRV_UFO_IS_SYNC_CHECKPOINT(psUFOPtr))
					{
						SyncCheckpointRecordLookup(psDeviceNode, psUFOPtr->puiAddrUFO.ui32Addr,
									 pszSyncInfo, CCB_SYNC_INFO_LEN);
					}
					else
					{
						SyncRecordLookup(psDeviceNode, psUFOPtr->puiAddrUFO.ui32Addr,
									 pszSyncInfo, CCB_SYNC_INFO_LEN);
					}
#endif
					PVR_DUMPDEBUG_LOG("  %s  %s--Addr:0x%08x Val=0x%08x %s",
						bLastCommand? " ": "|",
						bLastUFO? "`": "|",
						psUFOPtr->puiAddrUFO.ui32Addr, psUFOPtr->ui32Value,
						pszSyncInfo
						);
				}
				break;
			}

			case RGXFWIF_CCB_CMD_TYPE_RMW_UPDATE:
			case RGXFWIF_CCB_CMD_TYPE_UNFENCED_RMW_UPDATE:
			{
				for (i = 0; i < ui32NoOfUpdates; i++, psUFOPtr++)
				{
					bLastUFO = (ui32NoOfUpdates-1 == i)? IMG_TRUE: IMG_FALSE;
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
					if (PVRSRV_UFO_IS_SYNC_CHECKPOINT(psUFOPtr))
					{
						SyncCheckpointRecordLookup(psDeviceNode, psUFOPtr->puiAddrUFO.ui32Addr,
									 pszSyncInfo, CCB_SYNC_INFO_LEN);
					}
					else
					{
						SyncRecordLookup(psDeviceNode, psUFOPtr->puiAddrUFO.ui32Addr,
									 pszSyncInfo, CCB_SYNC_INFO_LEN);
					}
#endif
					PVR_DUMPDEBUG_LOG("  %s  %s--Addr:0x%08x Val++ %s",
						bLastCommand? " ": "|",
						bLastUFO? "`": "|",
						psUFOPtr->puiAddrUFO.ui32Addr,
						pszSyncInfo
						);
				}
				break;
			}

			default:
				break;
		}
		ui32Offset = ui32NextOffset;
	}

}
#endif /* defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING) || defined(PVRSRV_ENABLE_FULL_CCB_DUMP) */

void DumpStalledCCBCommand(PRGXFWIF_FWCOMMONCONTEXT sFWCommonContext,
				RGX_CLIENT_CCB *psCurrentClientCCB,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	volatile RGXFWIF_CCCB_CTL	  *psClientCCBCtrl = psCurrentClientCCB->psClientCCBCtrl;
	IMG_UINT8					  *pui8ClientCCBBuff = psCurrentClientCCB->pui8ClientCCB;
	volatile IMG_UINT8		   	  *pui8Ptr;
	IMG_UINT32 					  ui32SampledRdOff = psClientCCBCtrl->ui32ReadOffset;
	IMG_UINT32 					  ui32SampledDepOff = psClientCCBCtrl->ui32DepOffset;
	IMG_UINT32 					  ui32SampledWrOff = psCurrentClientCCB->ui32HostWriteOffset;

	pui8Ptr = pui8ClientCCBBuff + ui32SampledRdOff;

	if ((ui32SampledRdOff == ui32SampledDepOff) &&
		(ui32SampledRdOff != ui32SampledWrOff))
	{
		volatile RGXFWIF_CCB_CMD_HEADER *psCommandHeader = (RGXFWIF_CCB_CMD_HEADER *)(pui8ClientCCBBuff + ui32SampledRdOff);
		RGXFWIF_CCB_CMD_TYPE 	eCommandType = psCommandHeader->eCmdType;
		volatile IMG_UINT8				*pui8Ptr = (IMG_UINT8 *)psCommandHeader;

		/* CCB is stalled on a fence... */
		if ((eCommandType == RGXFWIF_CCB_CMD_TYPE_FENCE) || (eCommandType == RGXFWIF_CCB_CMD_TYPE_FENCE_PR))
		{
#if defined(SUPPORT_EXTRA_METASP_DEBUG)
			PVRSRV_RGXDEV_INFO *psDevInfo = FWCommonContextGetRGXDevInfo(psCurrentClientCCB->psServerCommonContext);
			IMG_UINT32 ui32Val;
#endif
			RGXFWIF_UFO *psUFOPtr = (RGXFWIF_UFO *)(pui8Ptr + sizeof(*psCommandHeader));
			IMG_UINT32 jj;

			/* Display details of the fence object on which the context is pending */
			PVR_DUMPDEBUG_LOG("FWCtx 0x%08X @ %d (%s) pending on %s:",
							   sFWCommonContext.ui32Addr,
							   ui32SampledRdOff,
							   (IMG_PCHAR)&psCurrentClientCCB->szName,
							   _CCBCmdTypename(eCommandType));
			for (jj=0; jj<psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO); jj++)
			{
#if !defined(SUPPORT_EXTRA_METASP_DEBUG)
				PVR_DUMPDEBUG_LOG("  Addr:0x%08x  Value=0x%08x",psUFOPtr[jj].puiAddrUFO.ui32Addr, psUFOPtr[jj].ui32Value);
#else
				ui32Val = 0;
				RGXReadWithSP(psDevInfo, psUFOPtr[jj].puiAddrUFO.ui32Addr, &ui32Val);
				PVR_DUMPDEBUG_LOG("  Addr:0x%08x Value(Host)=0x%08x Value(FW)=0x%08x",
				                   psUFOPtr[jj].puiAddrUFO.ui32Addr,
				                   psUFOPtr[jj].ui32Value, ui32Val);
#endif
			}

			/* Advance psCommandHeader past the FENCE to the next command header (this will be the TA/3D command that is fenced) */
			pui8Ptr = (IMG_UINT8 *)psUFOPtr + psCommandHeader->ui32CmdSize;
			psCommandHeader = (RGXFWIF_CCB_CMD_HEADER *)pui8Ptr;
			if( (uintptr_t)psCommandHeader != ((uintptr_t)pui8ClientCCBBuff + ui32SampledWrOff))
			{
				PVR_DUMPDEBUG_LOG(" FWCtx 0x%08X fenced command is of type %s",sFWCommonContext.ui32Addr, _CCBCmdTypename(psCommandHeader->eCmdType));
				/* Advance psCommandHeader past the TA/3D to the next command header (this will possibly be an UPDATE) */
				pui8Ptr += sizeof(*psCommandHeader) + psCommandHeader->ui32CmdSize;
				psCommandHeader = (RGXFWIF_CCB_CMD_HEADER *)pui8Ptr;
				/* If the next command is an update, display details of that so we can see what would then become unblocked */
				if( (uintptr_t)psCommandHeader != ((uintptr_t)pui8ClientCCBBuff + ui32SampledWrOff))
				{
					eCommandType = psCommandHeader->eCmdType;

					if (eCommandType == RGXFWIF_CCB_CMD_TYPE_UPDATE)
					{
						psUFOPtr = (RGXFWIF_UFO *)((IMG_UINT8 *)psCommandHeader + sizeof(*psCommandHeader));
						PVR_DUMPDEBUG_LOG(" preventing %s:",_CCBCmdTypename(eCommandType));
						for (jj=0; jj<psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO); jj++)
						{
#if !defined(SUPPORT_EXTRA_METASP_DEBUG)
							PVR_DUMPDEBUG_LOG("  Addr:0x%08x  Value=0x%08x",psUFOPtr[jj].puiAddrUFO.ui32Addr, psUFOPtr[jj].ui32Value);
#else
							ui32Val = 0;
							RGXReadWithSP(psDevInfo, psUFOPtr[jj].puiAddrUFO.ui32Addr, &ui32Val);
							PVR_DUMPDEBUG_LOG("  Addr:0x%08x Value(Host)=0x%08x Value(FW)=0x%08x",
							                   psUFOPtr[jj].puiAddrUFO.ui32Addr,
							                   psUFOPtr[jj].ui32Value,
							                   ui32Val);
#endif
						}
					}
				}
				else
				{
					PVR_DUMPDEBUG_LOG(" FWCtx 0x%08X has no further commands",sFWCommonContext.ui32Addr);
				}
			}
			else
			{
				PVR_DUMPDEBUG_LOG(" FWCtx 0x%08X has no further commands",sFWCommonContext.ui32Addr);
			}
		}
	}
}

void DumpStalledContextInfo(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGX_CLIENT_CCB *psStalledClientCCB;

	PVR_ASSERT(psDevInfo);

	psStalledClientCCB = (RGX_CLIENT_CCB *)psDevInfo->pvEarliestStalledClientCCB;

	if (psStalledClientCCB)
	{
		volatile RGXFWIF_CCCB_CTL *psClientCCBCtrl = psStalledClientCCB->psClientCCBCtrl;
		IMG_UINT32 ui32SampledReadOffset = psClientCCBCtrl->ui32ReadOffset;
		IMG_UINT8                 *pui8Ptr = (psStalledClientCCB->pui8ClientCCB + ui32SampledReadOffset);
		RGXFWIF_CCB_CMD_HEADER    *psCommandHeader = (RGXFWIF_CCB_CMD_HEADER *)(pui8Ptr);
		RGXFWIF_CCB_CMD_TYPE      eCommandType = psCommandHeader->eCmdType;

		if ((eCommandType == RGXFWIF_CCB_CMD_TYPE_FENCE) || (eCommandType == RGXFWIF_CCB_CMD_TYPE_FENCE_PR))
		{
			RGXFWIF_UFO *psUFOPtr = (RGXFWIF_UFO *)(pui8Ptr + sizeof(*psCommandHeader));
			IMG_UINT32 jj;
			IMG_UINT32 ui32NumUnsignalledUFOs = 0;
			IMG_UINT32 ui32UnsignalledUFOVaddrs[PVRSRV_MAX_SYNC_PRIMS];

			PVR_LOG(("Fence found on context 0x%x '%s' has %d UFOs", FWCommonContextGetFWAddress(psStalledClientCCB->psServerCommonContext).ui32Addr, psStalledClientCCB->szName, (IMG_UINT32)(psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO))));
			for (jj=0; jj<psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO); jj++)
			{
				if (PVRSRV_UFO_IS_SYNC_CHECKPOINT((RGXFWIF_UFO *)&psUFOPtr[jj]))
				{
					IMG_UINT32 ui32ReadValue = SyncCheckpointStateFromUFO(psDevInfo->psDeviceNode,
					                                           psUFOPtr[jj].puiAddrUFO.ui32Addr);
					PVR_LOG(("  %d/%d FWAddr 0x%x requires 0x%x (currently 0x%x)", jj+1,
							   (IMG_UINT32)(psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO)),
							   psUFOPtr[jj].puiAddrUFO.ui32Addr,
							   psUFOPtr[jj].ui32Value,
							   ui32ReadValue));
					/* If fence is unmet, dump debug info on it */
					if (ui32ReadValue != psUFOPtr[jj].ui32Value)
					{
						/* Add to our list to pass to pvr_sync */
						ui32UnsignalledUFOVaddrs[ui32NumUnsignalledUFOs] = psUFOPtr[jj].puiAddrUFO.ui32Addr;
						ui32NumUnsignalledUFOs++;
					}
				}
				else
				{
					PVR_LOG(("  %d/%d FWAddr 0x%x requires 0x%x", jj+1,
							   (IMG_UINT32)(psCommandHeader->ui32CmdSize/sizeof(RGXFWIF_UFO)),
							   psUFOPtr[jj].puiAddrUFO.ui32Addr,
							   psUFOPtr[jj].ui32Value));
				}
			}
#if defined(SUPPORT_NATIVE_FENCE_SYNC) || defined (SUPPORT_FALLBACK_FENCE_SYNC)
			if (ui32NumUnsignalledUFOs > 0)
			{
				IMG_UINT32 ui32NumSyncsOwned;
				PVRSRV_ERROR eErr = SyncCheckpointDumpInfoOnStalledUFOs(ui32NumUnsignalledUFOs, &ui32UnsignalledUFOVaddrs[0], &ui32NumSyncsOwned);

				PVR_LOG_IF_ERROR(eErr, "SyncCheckpointDumpInfoOnStalledUFOs() call failed.");
				PVR_LOG(("%d sync checkpoint%s owned by pvr_sync in stalled context", ui32NumSyncsOwned, ui32NumSyncsOwned==1 ? "" : "s"));
			}
#endif
#if defined(PVRSRV_STALLED_CCB_ACTION)
			if (ui32NumUnsignalledUFOs > 0)
			{
				RGXFWIF_KCCB_CMD sSignalFencesCmd;

				sSignalFencesCmd.eCmdType = RGXFWIF_KCCB_CMD_FORCE_UPDATE;
				sSignalFencesCmd.eDM = RGXFWIF_DM_GP;
				sSignalFencesCmd.uCmdData.sForceUpdateData.psContext = FWCommonContextGetFWAddress(psStalledClientCCB->psServerCommonContext);
				sSignalFencesCmd.uCmdData.sForceUpdateData.ui32CCBFenceOffset = ui32SampledReadOffset;

				PVR_LOG(("Forced update command issued for FWCtx 0x%08X", sSignalFencesCmd.uCmdData.sForceUpdateData.psContext.ui32Addr));

				RGXScheduleCommand(FWCommonContextGetRGXDevInfo(psStalledClientCCB->psServerCommonContext),
								   RGXFWIF_DM_GP,
								   &sSignalFencesCmd,
								   sizeof(sSignalFencesCmd),
								   0,
								   PDUMP_FLAGS_CONTINUOUS);
			}
#endif
		}
		psDevInfo->pvEarliestStalledClientCCB = NULL;
	}
}

/******************************************************************************
 End of file (rgxccb.c)
******************************************************************************/

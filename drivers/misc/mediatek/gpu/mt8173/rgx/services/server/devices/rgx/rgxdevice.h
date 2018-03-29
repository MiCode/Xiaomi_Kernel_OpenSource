/*************************************************************************/ /*!
@File
@Title          RGX device node header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX device node
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

#if !defined(__RGXDEVICE_H__)
#define __RGXDEVICE_H__

#include "img_types.h"
#include "pvrsrv_device_types.h"
#include "mmu_common.h"
#include "rgx_fwif_km.h"
#include "rgx_fwif.h"
#include "rgxscript.h"
#include "cache_ops.h"
#include "device.h"
#include "osfunc.h"
#include "rgxlayer_km_impl.h"

typedef struct _RGX_SERVER_COMMON_CONTEXT_ RGX_SERVER_COMMON_CONTEXT;

typedef struct {
	DEVMEM_MEMDESC		*psFWFrameworkMemDesc;
	IMG_DEV_VIRTADDR	*psMCUFenceAddr;
#if RGX_FEATURE_CDM_CONTROL_STREAM_FORMAT == 2 && defined(RGX_FEATURE_SIGNAL_SNOOPING)
	IMG_DEV_VIRTADDR	*psResumeSignalAddr;
#endif
} RGX_COMMON_CONTEXT_INFO;


/*!
 ******************************************************************************
 * Device state flags
 *****************************************************************************/
#define RGXKM_DEVICE_STATE_ZERO_FREELIST			(0x1 << 0)		/*!< Zeroing the physical pages of reconstructed free lists */
#define RGXKM_DEVICE_STATE_FTRACE_EN				(0x1 << 1)		/*!< Used to enable device FTrace thread to consume HWPerf data */
#define RGXKM_DEVICE_STATE_DISABLE_DW_LOGGING_EN 	(0x1 << 2)		/*!< Used to disable the Devices Watchdog logging */
#define RGXKM_DEVICE_STATE_DUST_REQUEST_INJECT_EN		(0x1 << 3)		/*!< Used for validation to inject dust requests every TA/3D kick */

/*!
 ******************************************************************************
 * GPU DVFS Table
 *****************************************************************************/

#define RGX_GPU_DVFS_TABLE_SIZE            100                      /* DVFS Table size */
#define RGX_GPU_DVFS_GET_INDEX(clockfreq)  ((clockfreq) / 10000000) /* Assuming different GPU clocks are separated by at least 10MHz
                                                                     * WARNING: this macro must be used only with nominal values of
                                                                     * the GPU clock speed (the ones provided by the customer code) */
#define RGX_GPU_DVFS_FIRST_CALIBRATION_TIME_US       25000          /* Time required to calibrate a clock frequency the first time */
#define RGX_GPU_DVFS_TRANSITION_CALIBRATION_TIME_US  150000         /* Time required for a recalibration after a DVFS transition */
#define RGX_GPU_DVFS_PERIODIC_CALIBRATION_TIME_US    10000000       /* Time before the next periodic calibration and correlation */

typedef struct _RGX_GPU_DVFS_TABLE_
{
	IMG_UINT64 ui64CalibrationCRTimestamp;              /*!< CR timestamp used to calibrate GPU frequencies (beginning of a calibration period) */
	IMG_UINT64 ui64CalibrationOSTimestamp;              /*!< OS timestamp used to calibrate GPU frequencies (beginning of a calibration period) */
	IMG_UINT64 ui64CalibrationCRTimediff;               /*!< CR timediff used to calibrate GPU frequencies (calibration period) */
	IMG_UINT64 ui64CalibrationOSTimediff;               /*!< OS timediff used to calibrate GPU frequencies (calibration period) */
	IMG_UINT32 ui32CalibrationPeriod;                   /*!< Threshold used to determine whether the current GPU frequency should be calibrated */
	IMG_UINT32 ui32CurrentDVFSId;                       /*!< Current table entry index */
	IMG_BOOL   bAccumulatePeriod;                       /*!< Accumulate many consecutive periods to get a better calibration at the end */
	IMG_UINT32 aui32DVFSClock[RGX_GPU_DVFS_TABLE_SIZE]; /*!< DVFS clocks table (clocks in Hz) */
} RGX_GPU_DVFS_TABLE;


/*!
 ******************************************************************************
 * GPU utilisation statistics
 *****************************************************************************/

typedef struct _RGXFWIF_GPU_UTIL_STATS_
{
	IMG_BOOL   bValid;                /* If TRUE, statistics are valid.
	                                     FALSE if the driver couldn't get reliable stats. */
	IMG_UINT64 ui64GpuStatActiveHigh; /* GPU active high statistic */
	IMG_UINT64 ui64GpuStatActiveLow;  /* GPU active low (i.e. TLA active only) statistic */
	IMG_UINT64 ui64GpuStatBlocked;    /* GPU blocked statistic */
	IMG_UINT64 ui64GpuStatIdle;       /* GPU idle statistic */
	IMG_UINT64 ui64GpuStatCumulative; /* Sum of active/blocked/idle stats */
} RGXFWIF_GPU_UTIL_STATS;


typedef struct _RGX_REG_CONFIG_
{
	IMG_BOOL               bEnabled;
	RGXFWIF_REG_CFG_TYPE   eRegCfgTypeToPush;
	IMG_UINT32             ui32NumRegRecords;
} RGX_REG_CONFIG;

typedef struct _PVRSRV_STUB_PBDESC_ PVRSRV_STUB_PBDESC;

typedef struct
{
	IMG_UINT32			ui32DustCount1;
	IMG_UINT32			ui32DustCount2;
	IMG_BOOL			bToggle;
} RGX_DUST_STATE;

/* there is a corresponding define in rgxapi.h */
#define RGX_MAX_TIMER_QUERIES 16

/*!
 ******************************************************************************
 * RGX Device info
 *****************************************************************************/

typedef struct _PVRSRV_RGXDEV_INFO_
{
	PVRSRV_DEVICE_NODE		*psDeviceNode;

	/* FIXME: This is a workaround due to having 2 inits but only 1 deinit */
	IMG_BOOL				bDevInit2Done;

	IMG_BOOL                bFirmwareInitialised;
	IMG_BOOL				bPDPEnabled;

	IMG_HANDLE				hDbgReqNotify;

	/* Kernel mode linear address of device registers */
	void					*pvRegsBaseKM;

	/* FIXME: The alloc for this should go through OSAllocMem in future */
	IMG_HANDLE				hRegMapping;

	/* System physical address of device registers*/
	IMG_CPU_PHYADDR			sRegsPhysBase;
	/*  Register region size in bytes */
	IMG_UINT32				ui32RegSize;

	PVRSRV_STUB_PBDESC		*psStubPBDescListKM;

	/* Firmware memory context info */
	DEVMEM_CONTEXT			*psKernelDevmemCtx;
	DEVMEM_HEAP				*psFirmwareHeap;
	MMU_CONTEXT				*psKernelMMUCtx;

	void					*pvDeviceMemoryHeap;

	/* Kernel CCB */
	DEVMEM_MEMDESC			*psKernelCCBCtlMemDesc;    /*!< memdesc for Kernel CCB control */
	RGXFWIF_CCB_CTL			*psKernelCCBCtl;           /*!< kernel mapping for Kernel CCB control */
	DEVMEM_MEMDESC			*psKernelCCBMemDesc;       /*!< memdesc for Kernel CCB */
	IMG_UINT8				*psKernelCCB;              /*!< kernel mapping for Kernel CCB */

	/* Firmware CCB */
	DEVMEM_MEMDESC			*psFirmwareCCBCtlMemDesc;   /*!< memdesc for Firmware CCB control */
	RGXFWIF_CCB_CTL			*psFirmwareCCBCtl;          /*!< kernel mapping for Firmware CCB control */
	DEVMEM_MEMDESC			*psFirmwareCCBMemDesc;      /*!< memdesc for Firmware CCB */
	IMG_UINT8				*psFirmwareCCB;             /*!< kernel mapping for Firmware CCB */

	/*
		if we don't preallocate the pagetables we must 
		insert newly allocated page tables dynamically 
	*/
	void					*pvMMUContextList;

	IMG_UINT32				ui32ClkGateStatusReg;
	IMG_UINT32				ui32ClkGateStatusMask;
	RGX_SCRIPTS				*psScripts;

	DEVMEM_MEMDESC			*psRGXFWCodeMemDesc;
	IMG_DEV_VIRTADDR		sFWCodeDevVAddrBase;
	DEVMEM_MEMDESC			*psRGXFWDataMemDesc;
	IMG_DEV_VIRTADDR		sFWDataDevVAddrBase;

	DEVMEM_MEMDESC			*psRGXFWCorememMemDesc;
	IMG_DEV_VIRTADDR		sFWCorememCodeDevVAddrBase;
	RGXFWIF_DEV_VIRTADDR	sFWCorememCodeFWAddr;

#if defined(RGXFW_ALIGNCHECKS)
	DEVMEM_MEMDESC			*psRGXFWAlignChecksMemDesc;
#endif

	DEVMEM_MEMDESC			*psRGXFWSigTAChecksMemDesc;
	IMG_UINT32				ui32SigTAChecksSize;

	DEVMEM_MEMDESC			*psRGXFWSig3DChecksMemDesc;
	IMG_UINT32				ui32Sig3DChecksSize;

#if defined(RGX_FEATURE_RAY_TRACING)
	DEVMEM_MEMDESC			*psRGXFWSigRTChecksMemDesc;
	IMG_UINT32				ui32SigRTChecksSize;
	
	DEVMEM_MEMDESC			*psRGXFWSigSHChecksMemDesc;
	IMG_UINT32				ui32SigSHChecksSize;
#endif

#if defined (PDUMP)
	IMG_BOOL				bDumpedKCCBCtlAlready;
#endif

#if defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* 
		Guest drivers do not support these functionalities:
			- H/W perf & device power management
			- F/W initialization & management
			- GPU dvfs, trace & utilization
	 */
	DEVMEM_MEMDESC			*psRGXFWIfInitMemDesc;
	RGXFWIF_DEV_VIRTADDR	sFWInitFWAddr;
#else
	DEVMEM_MEMDESC			*psRGXFWIfTraceBufCtlMemDesc;	/*!< memdesc of trace buffer control structure */
	DEVMEM_MEMDESC			*psRGXFWIfTraceBufferMemDesc[RGXFW_THREAD_NUM];	/*!< memdesc of actual FW trace (log) buffer(s) */
	RGXFWIF_TRACEBUF		*psRGXFWIfTraceBuf;		/* structure containing trace control data and actual trace buffer */

	DEVMEM_MEMDESC			*psRGXFWIfHWRInfoBufCtlMemDesc;
	RGXFWIF_HWRINFOBUF		*psRGXFWIfHWRInfoBuf;

	DEVMEM_MEMDESC			*psRGXFWIfGpuUtilFWCbCtlMemDesc;
	RGXFWIF_GPU_UTIL_FWCB	*psRGXFWIfGpuUtilFWCb;

	DEVMEM_MEMDESC			*psRGXFWIfHWPerfBufMemDesc;
	IMG_BYTE				*psRGXFWIfHWPerfBuf;
	IMG_UINT32				ui32RGXFWIfHWPerfBufSize; /* in bytes */

	DEVMEM_MEMDESC			*psRGXFWIfCorememDataStoreMemDesc;

	DEVMEM_MEMDESC			*psRGXFWIfRegCfgMemDesc;

	DEVMEM_MEMDESC			*psRGXFWIfHWPerfCountersMemDesc;
	DEVMEM_MEMDESC			*psRGXFWIfInitMemDesc;
	RGXFWIF_DEV_VIRTADDR	sFWInitFWAddr;

	DEVMEM_MEMDESC			*psRGXFWIfRuntimeCfgMemDesc;
	RGXFWIF_RUNTIME_CFG		*psRGXFWIfRuntimeCfg;

#if defined(SUPPORT_PVRSRV_GPUVIRT)
	/* Additional guest firmware memory context info */
	DEVMEM_HEAP				*psGuestFirmwareHeap[RGXFW_NUM_OS-1];
	DEVMEM_MEMDESC			*psGuestFirmwareMemDesc[RGXFW_NUM_OS-1];
#endif
	DEVMEM_MEMDESC			*psMETAT1StackMemDesc;

#if defined (SUPPORT_PDVFS)
	/**
	 * Host memdesc and pointer to memory containing core clock rate in Hz.
	 * Firmware (PDVFS) updates the memory on changing the core clock rate over
	 * GPIO.
	 * Note: Shared memory needs atomic access from Host driver and firmware,
	 * hence size should not be greater than memory transaction granularity.
	 * Currently it's is chosen to be 32 bits.
	 */
	DEVMEM_MEMDESC			*psRGXFWIFCoreClkRateMemDesc;
	volatile IMG_UINT32		*pui32RGXFWIFCoreClkRate;
	/**
	 * Last sampled core clk rate.
	 */
	volatile IMG_UINT32		ui32CoreClkRateSnapshot;
#endif
	/*
	   HWPerf data for the RGX device
	 */

	POS_LOCK    hHWPerfLock;  /*! Critical section lock that protects HWPerf code
	                           *  from multiple thread duplicate init/deinit
	                           *  and loss/freeing of FW & Host resources while in
	                           *  use in another thread e.g. MSIR. */

	IMG_UINT64  ui64HWPerfFilter; /*! Event filter for FW events (settable by AppHint) */
	IMG_HANDLE  hHWPerfStream;    /*! TL Stream buffer (L2) for firmware event stream */

	IMG_UINT32  ui32HWPerfHostFilter;      /*! Event filter for HWPerfHost stream (settable by AppHint) */
	POS_LOCK    hLockHWPerfHostStream;     /*! Lock guarding access to HWPerfHost stream from multiple threads */
	IMG_HANDLE  hHWPerfHostStream;         /*! TL Stream buffer for host only event stream */
	IMG_UINT32  ui32HWPerfHostBufSize;     /*! Host side buffer size in bytes */
	IMG_UINT32  ui32HWPerfHostNextOrdinal; /*! Ordinal number for HWPerfHost events */

#if defined(SUPPORT_GPUTRACE_EVENTS)
	void        *pvGpuFtraceData;
#endif
	
	/* Poll data for detecting firmware fatal errors */
	IMG_UINT32				aui32CrLastPollAddr[RGXFW_THREAD_NUM];
	IMG_UINT32				ui32KCCBCmdsExecutedLastTime;
	IMG_BOOL				bKCCBCmdsWaitingLastTime;
	IMG_UINT32				ui32GEOTimeoutsLastTime;

	/* Client stall detection */
	IMG_BOOL				bStalledClient;
#endif

	IMG_BOOL				bWorkEstEnabled;
	IMG_BOOL				bPDVFSEnabled;

	void					*pvLISRData;
	void					*pvMISRData;
	void					*pvAPMISRData;

	volatile IMG_UINT32		aui32SampleIRQCount[RGXFW_THREAD_NUM];

	DEVMEM_MEMDESC			*psRGXFaultAddressMemDesc;

#if defined(FIX_HW_BRN_37200)
	DEVMEM_MEMDESC			*psRGXFWHWBRN37200MemDesc;
#endif

#if defined(RGX_FEATURE_SLC_VIVT)
	DEVMEM_MEMDESC			*psSLC3FenceMemDesc;
#endif

	/* If we do 10 deferred memory allocations per second, then the ID would warp around after 13 years */
	IMG_UINT32				ui32ZSBufferCurrID;	/*!< ID assigned to the next deferred devmem allocation */
	IMG_UINT32				ui32FreelistCurrID;	/*!< ID assigned to the next freelist */
	IMG_UINT32				ui32RPMFreelistCurrID;	/*!< ID assigned to the next RPM freelist */

	POS_LOCK 				hLockZSBuffer;		/*!< Lock to protect simultaneous access to ZSBuffers */
	DLLIST_NODE				sZSBufferHead;		/*!< List of on-demand ZSBuffers */
	POS_LOCK 				hLockFreeList;		/*!< Lock to protect simultaneous access to Freelists */
	DLLIST_NODE				sFreeListHead;		/*!< List of growable Freelists */
	POS_LOCK 				hLockRPMFreeList;	/*!< Lock to protect simultaneous access to RPM Freelists */
	DLLIST_NODE				sRPMFreeListHead;	/*!< List of growable RPM Freelists */
	POS_LOCK				hLockRPMContext;	/*!< Lock to protect simultaneous access to RPM contexts */
	PSYNC_PRIM_CONTEXT		hSyncPrimContext;
	PVRSRV_CLIENT_SYNC_PRIM *psPowSyncPrim;

	IMG_UINT32				ui32ActivePMReqOk;
	IMG_UINT32				ui32ActivePMReqDenied;
	IMG_UINT32				ui32ActivePMReqTotal;
	
	IMG_HANDLE				hProcessQueuesMISR;

	IMG_UINT32 				ui32DeviceFlags;		/*!< Flags to track general device state  */

	/* Timer Queries */
	IMG_UINT32				ui32ActiveQueryId;		/*!< id of the active line */
	IMG_BOOL				bSaveStart;				/*!< save the start time of the next kick on the device*/
	IMG_BOOL				bSaveEnd;				/*!< save the end time of the next kick on the device*/

	DEVMEM_MEMDESC			*psStartTimeMemDesc;    /*!< memdesc for Start Times */
	IMG_UINT64				*pui64StartTimeById;    /*!< CPU mapping of the above */

	DEVMEM_MEMDESC			*psEndTimeMemDesc;      /*!< memdesc for End Timer */
	IMG_UINT64				*pui64EndTimeById;      /*!< CPU mapping of the above */

	IMG_UINT32				aui32ScheduledOnId[RGX_MAX_TIMER_QUERIES];	/*!< kicks Scheduled on QueryId */
	DEVMEM_MEMDESC			*psCompletedMemDesc;	/*!< kicks Completed on QueryId */
	IMG_UINT32				*pui32CompletedById;	/*!< CPU mapping of the above */

#if !defined(PVRSRV_GPUVIRT_GUESTDRV)
	/* GPU DVFS Table */
	RGX_GPU_DVFS_TABLE  *psGpuDVFSTable;

	/* Pointer to function returning the GPU utilisation statistics since the last
	 * time the function was called. Supports different users at the same time.
	 *
	 * psReturnStats [out]: GPU utilisation statistics (active high/active low/idle/blocked)
	 *                      in microseconds since the last time the function was called
	 *                      by a specific user (identified by hGpuUtilUser)
	 *
	 * Returns PVRSRV_OK in case the call completed without errors,
	 * some other value otherwise.
	 */
	PVRSRV_ERROR (*pfnGetGpuUtilStats) (PVRSRV_DEVICE_NODE *psDeviceNode,
	                                    IMG_HANDLE hGpuUtilUser,
	                                    RGXFWIF_GPU_UTIL_STATS *psReturnStats);
#endif

	POS_LOCK    			hGPUUtilLock;

	/* Register configuration */
	RGX_REG_CONFIG			sRegCongfig;

	IMG_BOOL				bRGXPowered;
	DLLIST_NODE				sMemoryContextList;

	POSWR_LOCK				hRenderCtxListLock;
	POSWR_LOCK				hComputeCtxListLock;
	POSWR_LOCK				hTransferCtxListLock;
	POSWR_LOCK				hRaytraceCtxListLock;
	POSWR_LOCK				hMemoryCtxListLock;
	POSWR_LOCK				hKickSyncCtxListLock;

	/* Linked list of deferred KCCB commands due to a full KCCB */
	DLLIST_NODE 			sKCCBDeferredCommandsListHead;

	/* Linked lists of contexts on this device */
	DLLIST_NODE 			sRenderCtxtListHead;
	DLLIST_NODE 			sComputeCtxtListHead;
	DLLIST_NODE 			sTransferCtxtListHead;
	DLLIST_NODE 			sRaytraceCtxtListHead;
	DLLIST_NODE 			sKickSyncCtxtListHead;

	DLLIST_NODE 			sCommonCtxtListHead;
	IMG_UINT32				ui32CommonCtxtCurrentID;	/*!< ID assigned to the next common context */

#if defined(SUPPORT_PAGE_FAULT_DEBUG)
	POS_LOCK 				hDebugFaultInfoLock;		/*!< Lock to protect the debug fault info list */
	POS_LOCK 				hMMUCtxUnregLock;	/*!< Lock to protect list of unregistered MMU contexts */
#endif

#if defined(RGX_FEATURE_MIPS)
	POS_LOCK				hNMILock; /*!< Lock to protect NMI operations */
#endif

	RGX_DUST_STATE			sDustReqState;

	RGX_POWER_LAYER_PARAMS	sPowerParams;

	RGXFWIF_DM				eBPDM;					/*!< Current breakpoint data master */
	IMG_BOOL				bBPSet;					/*!< A Breakpoint has been set */

	IMG_BOOL				bCoherencyTestDone;
} PVRSRV_RGXDEV_INFO;



typedef struct _RGX_TIMING_INFORMATION_
{
	/*! GPU default core clock speed in Hz */
	IMG_UINT32			ui32CoreClockSpeed;

	/*! Active Power Management: GPU actively requests the host driver to be powered off */
	IMG_BOOL			bEnableActivePM;

	/*! Enable the GPU to power off internal Power Islands independently from the host driver */
	IMG_BOOL			bEnableRDPowIsland;
	
	/*! Active Power Management: Delay between the GPU idle and the request to the host */
	IMG_UINT32			ui32ActivePMLatencyms;

} RGX_TIMING_INFORMATION;

typedef struct _RGX_DATA_
{
	/*! Timing information */
	RGX_TIMING_INFORMATION	*psRGXTimingInfo;
	IMG_BOOL bHasTDFWCodePhysHeap;
	IMG_UINT32 uiTDFWCodePhysHeapID;
	IMG_BOOL bHasTDSecureBufPhysHeap;
	IMG_UINT32 uiTDSecureBufPhysHeapID;
} RGX_DATA;


/*
	RGX PDUMP register bank name (prefix)
*/
#define RGX_PDUMPREG_NAME		"RGXREG"

#endif /* __RGXDEVICE_H__ */

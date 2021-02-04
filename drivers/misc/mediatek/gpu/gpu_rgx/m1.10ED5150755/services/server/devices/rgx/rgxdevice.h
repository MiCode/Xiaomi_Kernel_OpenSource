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
#include "cache_ops.h"
#include "device.h"
#include "osfunc.h"
#include "rgxlayer_impl.h"
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#include "hash.h"
#endif
typedef struct _RGX_SERVER_COMMON_CONTEXT_ RGX_SERVER_COMMON_CONTEXT;

typedef struct {
	DEVMEM_MEMDESC		*psFWFrameworkMemDesc;
	IMG_DEV_VIRTADDR	*psResumeSignalAddr;
} RGX_COMMON_CONTEXT_INFO;


/*!
 ******************************************************************************
 * Device state flags
 *****************************************************************************/
#define RGXKM_DEVICE_STATE_ZERO_FREELIST			(0x1 << 0)		/*!< Zeroing the physical pages of reconstructed free lists */
#define RGXKM_DEVICE_STATE_FTRACE_EN				(0x1 << 1)		/*!< Used to enable device FTrace thread to consume HWPerf data */
#define RGXKM_DEVICE_STATE_DISABLE_DW_LOGGING_EN 	(0x1 << 2)		/*!< Used to disable the Devices Watchdog logging */
#define RGXKM_DEVICE_STATE_DUST_REQUEST_INJECT_EN	(0x1 << 3)		/*!< Used for validation to inject dust requests every TA/3D kick */

/*!
 ******************************************************************************
 * GPU DVFS Table
 *****************************************************************************/

#define RGX_GPU_DVFS_TABLE_SIZE                      16
#define RGX_GPU_DVFS_FIRST_CALIBRATION_TIME_US       25000     /* Time required to calibrate a clock frequency the first time */
#define RGX_GPU_DVFS_TRANSITION_CALIBRATION_TIME_US  150000    /* Time required for a recalibration after a DVFS transition */
#define RGX_GPU_DVFS_PERIODIC_CALIBRATION_TIME_US    10000000  /* Time before the next periodic calibration and correlation */

typedef struct _GPU_FREQ_TRACKING_DATA_
{
	/* Core clock speed estimated by the driver */
	IMG_UINT32 ui32EstCoreClockSpeed;

	/* Amount of successful calculations of the estimated core clock speed */
	IMG_UINT32 ui32CalibrationCount;
} GPU_FREQ_TRACKING_DATA;

typedef struct _RGX_GPU_DVFS_TABLE_
{
	/* Beginning of current calibration period (in us) */
	IMG_UINT64 ui64CalibrationCRTimestamp;
	IMG_UINT64 ui64CalibrationOSTimestamp;

	/* Calculated calibration period (in us) */
	IMG_UINT64 ui64CalibrationCRTimediff;
	IMG_UINT64 ui64CalibrationOSTimediff;

	/* Current calibration period (in us) */
	IMG_UINT32 ui32CalibrationPeriod;

	/* System layer frequency table and frequency tracking data */
	IMG_UINT32 ui32FreqIndex;
	IMG_UINT32 aui32GPUFrequency[RGX_GPU_DVFS_TABLE_SIZE];
	GPU_FREQ_TRACKING_DATA asTrackingData[RGX_GPU_DVFS_TABLE_SIZE];
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
	IMG_UINT64 ui64TimeStamp;         /* Timestamp of the most recent sample of the GPU stats */
} RGXFWIF_GPU_UTIL_STATS;


typedef struct _RGX_REG_CONFIG_
{
	IMG_BOOL               bEnabled;
	RGXFWIF_REG_CFG_TYPE   eRegCfgTypeToPush;
	IMG_UINT32             ui32NumRegRecords;
#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK               hLock;
#endif
} RGX_REG_CONFIG;

typedef struct _PVRSRV_STUB_PBDESC_ PVRSRV_STUB_PBDESC;

typedef struct
{
	IMG_UINT32			ui32DustCount1;
	IMG_UINT32			ui32DustCount2;
	IMG_BOOL			bToggle;
} RGX_DUST_STATE;

typedef struct _PVRSRV_DEVICE_FEATURE_CONFIG_
{
	IMG_UINT64 ui64ErnsBrns;
	IMG_UINT64 ui64Features;
	IMG_UINT32 ui32B;
	IMG_UINT32 ui32V;
	IMG_UINT32 ui32N;
	IMG_UINT32 ui32C;
	IMG_UINT32 ui32FeaturesValues[RGX_FEATURE_WITH_VALUES_MAX_IDX];
	IMG_UINT32 ui32MAXDMCount;
	IMG_UINT32 ui32MAXDMMTSCount;
	IMG_UINT32 ui32MAXDustCount;
#define 	MAX_BVNC_STRING_LEN		(50)
	IMG_PCHAR  pszBVNCString;
}PVRSRV_DEVICE_FEATURE_CONFIG;

/* This is used to get the value of a specific feature.
 * Note that it will assert if the feature is disabled or value is invalid. */
#define RGX_GET_FEATURE_VALUE(psDevInfo, Feature) \
			( psDevInfo->sDevFeatureCfg.ui32FeaturesValues[RGX_FEATURE_##Feature##_IDX] )

/* This is used to check if the feature with value is available for the currently running bvnc or not */
#define RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, Feature) \
			( psDevInfo->sDevFeatureCfg.ui32FeaturesValues[RGX_FEATURE_##Feature##_IDX] < RGX_FEATURE_VALUE_DISABLED )

/* This is used to check if the feature WITHOUT value is available for the currently running bvnc or not */
#define RGX_IS_FEATURE_SUPPORTED(psDevInfo, Feature) \
			( psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_##Feature##_BIT_MASK)

/* This is used to check if the ERN is available for the currently running bvnc or not */
#define RGX_IS_ERN_SUPPORTED(psDevInfo, ERN) \
			( psDevInfo->sDevFeatureCfg.ui64ErnsBrns & HW_ERN_##ERN##_BIT_MASK)

/* This is used to check if the BRN is available for the currently running bvnc or not */
#define RGX_IS_BRN_SUPPORTED(psDevInfo, BRN) \
			( psDevInfo->sDevFeatureCfg.ui64ErnsBrns & FIX_HW_BRN_##BRN##_BIT_MASK)

/* there is a corresponding define in rgxapi.h */
#define RGX_MAX_TIMER_QUERIES 16

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
/*
   For the workload estimation return data array, the max amount of commands the
   MTS can have is 255, therefore 512 (LOG2 = 9) is large enough to account for
   all corner cases
*/
#define RETURN_DATA_ARRAY_SIZE_LOG2 (9)
#define RETURN_DATA_ARRAY_SIZE      ((1UL) << RETURN_DATA_ARRAY_SIZE_LOG2)
#define RETURN_DATA_ARRAY_WRAP_MASK (RETURN_DATA_ARRAY_SIZE - 1)

#define WORKLOAD_HASH_SIZE_LOG2		6
#define WORKLOAD_HASH_SIZE 			((1UL) << WORKLOAD_HASH_SIZE_LOG2)
#define WORKLOAD_HASH_WRAP_MASK		(WORKLOAD_HASH_SIZE - 1)

typedef struct _RGX_WORKLOAD_TA3D_
{
	IMG_UINT32				ui32RenderTargetSize;
	IMG_UINT32				ui32NumberOfDrawCalls;
	IMG_UINT32				ui32NumberOfIndices;
	IMG_UINT32				ui32NumberOfMRTs;
} RGX_WORKLOAD_TA3D;

typedef struct _WORKLOAD_MATCHING_DATA_
{
	POS_LOCK				psHashLock;
	HASH_TABLE				*psHashTable;
	RGX_WORKLOAD_TA3D		asHashKeys[WORKLOAD_HASH_SIZE];
	IMG_UINT64				aui64HashData[WORKLOAD_HASH_SIZE];
	IMG_UINT32				ui32HashArrayWO;

} WORKLOAD_MATCHING_DATA;

typedef struct _WORKEST_HOST_DATA_
{
	WORKLOAD_MATCHING_DATA	sWorkloadMatchingDataTA;
	WORKLOAD_MATCHING_DATA	sWorkloadMatchingData3D;
	IMG_UINT32				ui32WorkEstCCBReceived;
} WORKEST_HOST_DATA;

typedef struct _WORKEST_RETURN_DATA_
{
	WORKEST_HOST_DATA		*psWorkEstHostData;
	WORKLOAD_MATCHING_DATA	*psWorkloadMatchingData;
	RGX_WORKLOAD_TA3D		sWorkloadCharacteristics;
} WORKEST_RETURN_DATA;
#endif


typedef struct
{
#if defined(PDUMP)
	IMG_HANDLE      hPdumpPages;
#endif
	PG_HANDLE       sPages;
	IMG_DEV_PHYADDR sPhysAddr;
} RGX_MIPS_ADDRESS_TRAMPOLINE;


/*!
 ******************************************************************************
 * RGX Device info
 *****************************************************************************/

typedef struct _PVRSRV_RGXDEV_INFO_
{
	PVRSRV_DEVICE_NODE		*psDeviceNode;

	PVRSRV_DEVICE_FEATURE_CONFIG	sDevFeatureCfg;

	/* FIXME: This is a workaround due to having 2 inits but only 1 deinit */
	IMG_BOOL				bDevInit2Done;

	IMG_BOOL                bFirmwareInitialised;
	IMG_BOOL				bPDPEnabled;

	IMG_HANDLE				hDbgReqNotify;

	/* Kernel mode linear address of device registers */
	void __iomem			*pvRegsBaseKM;

	/* FIXME: The alloc for this should go through OSAllocMem in future */
	IMG_HANDLE				hRegMapping;

	/* System physical address of device registers*/
	IMG_CPU_PHYADDR			sRegsPhysBase;
	/*  Register region size in bytes */
	IMG_UINT32				ui32RegSize;

	PVRSRV_STUB_PBDESC		*psStubPBDescListKM;

	/* Firmware memory context info */
	DEVMEM_CONTEXT			*psKernelDevmemCtx;
	DEVMEM_HEAP				*psFirmwareMainHeap;
	DEVMEM_HEAP				*psFirmwareConfigHeap;
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

	/* Workload Estimation Firmware CCB */
	DEVMEM_MEMDESC			*psWorkEstFirmwareCCBCtlMemDesc;   /*!< memdesc for Workload Estimation Firmware CCB control */
	RGXFWIF_CCB_CTL			*psWorkEstFirmwareCCBCtl;          /*!< kernel mapping for Workload Estimation Firmware CCB control */
	DEVMEM_MEMDESC			*psWorkEstFirmwareCCBMemDesc;      /*!< memdesc for Workload Estimation Firmware CCB */
	IMG_UINT8				*psWorkEstFirmwareCCB;             /*!< kernel mapping for Workload Estimation Firmware CCB */

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	/* Counter dumping */
	DEVMEM_MEMDESC 			*psCounterBufferMemDesc;      /*!< mem desc for counter dumping buffer */
	POS_LOCK				hCounterDumpingLock;          /*!< Lock for guarding access to counter dumping buffer */
#endif

	IMG_BOOL				bEnableFWPoisonOnFree;             /*!< Enable poisoning of FW allocations when freed */
	IMG_BYTE				ubFWPoisonOnFreeValue;             /*!< Byte value used when poisoning FW allocations */

	/*
		if we don't preallocate the pagetables we must
		insert newly allocated page tables dynamically
	*/
	void					*pvMMUContextList;

	IMG_UINT32				ui32ClkGateStatusReg;
	IMG_UINT32				ui32ClkGateStatusMask;

	DEVMEM_MEMDESC			*psRGXFWCodeMemDesc;
	IMG_DEV_VIRTADDR		sFWCodeDevVAddrBase;
	DEVMEM_MEMDESC			*psRGXFWDataMemDesc;
	IMG_DEV_VIRTADDR		sFWDataDevVAddrBase;
	RGX_MIPS_ADDRESS_TRAMPOLINE	sTrampoline;

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

	DEVMEM_MEMDESC			*psRGXFWSigRTChecksMemDesc;
	IMG_UINT32				ui32SigRTChecksSize;

	DEVMEM_MEMDESC			*psRGXFWSigSHChecksMemDesc;
	IMG_UINT32				ui32SigSHChecksSize;

#if defined (PDUMP)
	IMG_BOOL				bDumpedKCCBCtlAlready;
#endif

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK				hRGXFWIfBufInitLock;			/*!< trace buffer lock for initialisation phase */
#endif

	DEVMEM_MEMDESC			*psRGXFWIfTraceBufCtlMemDesc;	/*!< memdesc of trace buffer control structure */
	DEVMEM_MEMDESC			*psRGXFWIfTraceBufferMemDesc[RGXFW_THREAD_NUM];	/*!< memdesc of actual FW trace (log) buffer(s) */
	RGXFWIF_TRACEBUF		*psRGXFWIfTraceBuf;		/* structure containing trace control data and actual trace buffer */

	DEVMEM_MEMDESC			*psRGXFWIfGuestTraceBufCtlMemDesc;	/*!< memdesc of trace buffer control structure */
	RGXFWIF_TRACEBUF		*psRGXFWIfGuestTraceBuf;

	DEVMEM_MEMDESC			*psRGXFWIfTBIBufferMemDesc;	/*!< memdesc of actual FW TBI buffer */
	RGXFWIF_DEV_VIRTADDR		sRGXFWIfTBIBuffer;		/* TBI buffer data */

	DEVMEM_MEMDESC			*psRGXFWIfGuestHWRInfoBufCtlMemDesc;
	RGXFWIF_HWRINFOBUF		*psRGXFWIfGuestHWRInfoBuf;

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
	DEVMEM_MEMDESC			*psRGXFWIfOSConfigDesc;
	RGXFWIF_OS_CONFIG		*psFWIfOSConfig;
	RGXFWIF_DEV_VIRTADDR	sFWInitFWAddr;

	DEVMEM_MEMDESC			*psRGXFWIfRuntimeCfgMemDesc;
	RGXFWIF_RUNTIME_CFG		*psRGXFWIfRuntimeCfg;

	/* Additional guest firmware memory context info */
	DEVMEM_HEAP				*psGuestFirmwareRawHeap[RGXFW_NUM_OS];
	DEVMEM_MEMDESC			*psGuestFirmwareRawMemDesc[RGXFW_NUM_OS];
	DEVMEM_MEMDESC			*psGuestFirmwareMainMemDesc[RGXFW_NUM_OS];
	DEVMEM_MEMDESC			*psGuestFirmwareConfigMemDesc[RGXFW_NUM_OS];

	DEVMEM_MEMDESC			*psMETAT1StackMemDesc;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Array to store data needed for workload estimation when a workload
	   has finished and its cycle time is returned to the host.	 */
	WORKEST_RETURN_DATA		asReturnData[RETURN_DATA_ARRAY_SIZE];
	IMG_UINT32				ui32ReturnDataWO;
#endif

#if defined (SUPPORT_PDVFS)
	/**
	 * Host memdesc and pointer to memory containing core clock rate in Hz.
	 * Firmware updates the memory on changing the core clock rate over GPIO.
	 * Note: Shared memory needs atomic access from Host driver and firmware,
	 * hence size should not be greater than memory transaction granularity.
	 * Currently it is chosen to be 32 bits.
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
	IMG_UINT32  ui32MaxPacketSize;/*!< Max allowed packet size */

	IMG_UINT32  ui32HWPerfHostFilter;      /*! Event filter for HWPerfHost stream (settable by AppHint) */
	POS_LOCK    hLockHWPerfHostStream;     /*! Lock guarding access to HWPerfHost stream from multiple threads */
	IMG_HANDLE  hHWPerfHostStream;         /*! TL Stream buffer for host only event stream */
	IMG_UINT32  ui32HWPerfHostBufSize;     /*! Host side buffer size in bytes */
	IMG_UINT32  ui32HWPerfHostLastOrdinal; /*! Ordinal of the last packet emitted in HWPerfHost TL stream.
	                                        *  Guarded by hLockHWPerfHostStream */
	IMG_UINT32  ui32HWPerfHostNextOrdinal; /*! Ordinal number for HWPerfHost events. Guarded by hHWPerfHostSpinLock */
	IMG_UINT8   *pui8DeferredEvents;       /*! List of HWPerfHost events yet to be emitted in the TL stream.
	                                        *  Events generated from atomic context are deferred "emitted"
											*  as the "emission" code can sleep */
	IMG_UINT16  ui16DEReadIdx;             /*! Read index in the above deferred events buffer */
	IMG_UINT16  ui16DEWriteIdx;            /*! Write index in the above deferred events buffer */
	void        *pvHostHWPerfMISR;         /*! MISR to emit pending/deferred events in HWPerfHost TL stream */
	POS_SPINLOCK hHWPerfHostSpinLock;      /*! Guards data shared between an atomic & sleepable-context */
#if defined (PVRSRV_HWPERF_HOST_DEBUG_DEFERRED_EVENTS)
	IMG_UINT32  ui32DEHighWatermark;       /*! High watermark of deferred events buffer usage. Protected by
	                                        *! hHWPerfHostSpinLock*/
	/* Max number of times DeferredEmission waited for an atomic-context to "finish" packet write */
	IMG_UINT32  ui32WaitForAtomicCtxPktHighWatermark; /*! Protected by hLockHWPerfHostStream */
	/* Whether warning has been logged about an atomic-context packet loss (due to too long wait for "write" finish) */
	IMG_BOOL    bWarnedAtomicCtxPktLost;
	/* Max number of times DeferredEmission scheduled-out to give a chance to the right-ordinal packet to be emitted */
	IMG_UINT32  ui32WaitForRightOrdPktHighWatermark; /*! Protected by hLockHWPerfHostStream */
	/* Whether warning has been logged about an packet loss (due to too long wait for right ordinal to emit) */
	IMG_BOOL    bWarnedPktOrdinalBroke;
#endif

#if defined(SUPPORT_GPUTRACE_EVENTS)
	void        *pvGpuFtraceData;
#endif

	/* Poll data for detecting firmware fatal errors */
	IMG_UINT32				aui32CrLastPollAddr[RGXFW_THREAD_NUM];
	IMG_UINT32				ui32KCCBCmdsExecutedLastTime;
	IMG_BOOL				bKCCBCmdsWaitingLastTime;
	IMG_UINT32				ui32GEOTimeoutsLastTime;

	/* Client stall detection */
	IMG_UINT32				ui32StalledClientMask;

	IMG_BOOL				bWorkEstEnabled;
	IMG_BOOL				bPDVFSEnabled;

	void					*pvLISRData;
	void					*pvMISRData;
	void					*pvAPMISRData;
	RGX_ACTIVEPM_CONF		eActivePMConf;

	volatile IMG_UINT32		aui32SampleIRQCount[RGXFW_THREAD_NUM];
	volatile IMG_UINT32		aui32SampleIRQTime[RGXFW_THREAD_NUM];

	DEVMEM_MEMDESC			*psRGXFaultAddressMemDesc;

	DEVMEM_MEMDESC			*psSLC3FenceMemDesc;

	/* If we do 10 deferred memory allocations per second, then the ID would wrap around after 13 years */
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
	IMG_UINT32				ui32ActivePMReqNonIdle;
	IMG_UINT32				ui32ActivePMReqTotal;

	IMG_HANDLE				hProcessQueuesMISR;

	IMG_UINT32 				ui32DeviceFlags;		/*!< Flags to track general device state */

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

#if !defined(PVRSRV_USE_BRIDGE_LOCK)
	POS_LOCK				hTimerQueryLock;		/*!< lock to protect simultaneous access to timer query members */
#endif

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

#ifndef ENABLE_COMMON_DVFS
	POS_LOCK				hGPUUtilLock;
#else
	spinlock_t				sGPUUtilLock;
#endif

	/* Register configuration */
	RGX_REG_CONFIG			sRegCongfig;

	IMG_BOOL				bRGXPowered;
	DLLIST_NODE				sMemoryContextList;

	POSWR_LOCK				hRenderCtxListLock;
	POSWR_LOCK				hComputeCtxListLock;
	POSWR_LOCK				hTransferCtxListLock;
	POSWR_LOCK				hTDMCtxListLock;
	POSWR_LOCK				hRaytraceCtxListLock;
	POSWR_LOCK				hMemoryCtxListLock;
	POSWR_LOCK				hKickSyncCtxListLock;

	/* Linked list of deferred KCCB commands due to a full KCCB */
	POS_LOCK 				hLockKCCBDeferredCommandsList;
	DLLIST_NODE				sKCCBDeferredCommandsListHead;

	/* Linked lists of contexts on this device */
	DLLIST_NODE				sRenderCtxtListHead;
	DLLIST_NODE				sComputeCtxtListHead;
	DLLIST_NODE				sTransferCtxtListHead;
	DLLIST_NODE				sTDMCtxtListHead;
	DLLIST_NODE				sRaytraceCtxtListHead;
	DLLIST_NODE				sKickSyncCtxtListHead;

	DLLIST_NODE 			sCommonCtxtListHead;
	POSWR_LOCK				hCommonCtxtListLock;
	IMG_UINT32				ui32CommonCtxtCurrentID;	/*!< ID assigned to the next common context */

#if defined(SUPPORT_PAGE_FAULT_DEBUG)
	POS_LOCK 				hDebugFaultInfoLock;	/*!< Lock to protect the debug fault info list */
	POS_LOCK 				hMMUCtxUnregLock;		/*!< Lock to protect list of unregistered MMU contexts */
#endif

	POS_LOCK				hNMILock; /*!< Lock to protect NMI operations */

	RGX_DUST_STATE			sDustReqState;

	RGX_LAYER_PARAMS		sLayerParams;

	RGXFWIF_DM				eBPDM;					/*!< Current breakpoint data master */
	IMG_BOOL				bBPSet;					/*!< A Breakpoint has been set */
	POS_LOCK				hBPLock;				/*!< Lock for break point operations */

	IMG_UINT32				ui32CoherencyTestsDone;

	ATOMIC_T				iCCBSubmissionOrdinal; /* Rolling count used to indicate CCB submission order (all CCBs) */
	POS_LOCK				hCCBRecoveryLock;      /* Lock to protect pvEarliestStalledClientCCB and ui32OldestSubmissionOrdinal variables*/
	void					*pvEarliestStalledClientCCB; /* Will point to cCCB command to unblock in the event of a stall */
	IMG_UINT32				ui32OldestSubmissionOrdinal; /* Earliest submission ordinal of CCB entry found so far */

	POS_LOCK				hCCBStallCheckLock; /* Lock used to guard against multiple threads simultaneously checking for stalled CCBs */
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

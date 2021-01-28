/*************************************************************************/ /*!
@File
@Title          RGX firmware interface structures used by pvrsrvkm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX firmware interface structures used by pvrsrvkm
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

#if !defined(RGX_FWIF_KM_H)
#define RGX_FWIF_KM_H

#include "img_types.h"
#include "rgx_fwif_shared.h"
#include "rgxdefs_km.h"
#include "dllist.h"
#include "rgx_hwperf.h"
#include "rgx_hw_errors.h"


/*************************************************************************/ /*!
 Logging type
*/ /**************************************************************************/
#define RGXFWIF_LOG_TYPE_NONE			0x00000000U
#define RGXFWIF_LOG_TYPE_TRACE			0x00000001U
#define RGXFWIF_LOG_TYPE_GROUP_MAIN		0x00000002U
#define RGXFWIF_LOG_TYPE_GROUP_MTS		0x00000004U
#define RGXFWIF_LOG_TYPE_GROUP_CLEANUP	0x00000008U
#define RGXFWIF_LOG_TYPE_GROUP_CSW		0x00000010U
#define RGXFWIF_LOG_TYPE_GROUP_BIF		0x00000020U
#define RGXFWIF_LOG_TYPE_GROUP_PM		0x00000040U
#define RGXFWIF_LOG_TYPE_GROUP_RTD		0x00000080U
#define RGXFWIF_LOG_TYPE_GROUP_SPM		0x00000100U
#define RGXFWIF_LOG_TYPE_GROUP_POW		0x00000200U
#define RGXFWIF_LOG_TYPE_GROUP_HWR		0x00000400U
#define RGXFWIF_LOG_TYPE_GROUP_HWP		0x00000800U
#define RGXFWIF_LOG_TYPE_GROUP_RPM		0x00001000U
#define RGXFWIF_LOG_TYPE_GROUP_DMA		0x00002000U
#define RGXFWIF_LOG_TYPE_GROUP_MISC		0x00004000U
#define RGXFWIF_LOG_TYPE_GROUP_DEBUG	0x80000000U
#define RGXFWIF_LOG_TYPE_GROUP_MASK		0x80007FFEU
#define RGXFWIF_LOG_TYPE_MASK			0x80007FFFU

/* String used in pvrdebug -h output */
#define RGXFWIF_LOG_GROUPS_STRING_LIST   "main,mts,cleanup,csw,bif,pm,rtd,spm,pow,hwr,hwp,rpm,dma,misc,debug"

/* Table entry to map log group strings to log type value */
typedef struct {
	const IMG_CHAR* pszLogGroupName;
	IMG_UINT32      ui32LogGroupType;
} RGXFWIF_LOG_GROUP_MAP_ENTRY;

/*
  Macro for use with the RGXFWIF_LOG_GROUP_MAP_ENTRY type to create a lookup
  table where needed. Keep log group names short, no more than 20 chars.
*/
#define RGXFWIF_LOG_GROUP_NAME_VALUE_MAP { "none",    RGXFWIF_LOG_TYPE_NONE }, \
                                         { "main",    RGXFWIF_LOG_TYPE_GROUP_MAIN }, \
                                         { "mts",     RGXFWIF_LOG_TYPE_GROUP_MTS }, \
                                         { "cleanup", RGXFWIF_LOG_TYPE_GROUP_CLEANUP }, \
                                         { "csw",     RGXFWIF_LOG_TYPE_GROUP_CSW }, \
                                         { "bif",     RGXFWIF_LOG_TYPE_GROUP_BIF }, \
                                         { "pm",      RGXFWIF_LOG_TYPE_GROUP_PM }, \
                                         { "rtd",     RGXFWIF_LOG_TYPE_GROUP_RTD }, \
                                         { "spm",     RGXFWIF_LOG_TYPE_GROUP_SPM }, \
                                         { "pow",     RGXFWIF_LOG_TYPE_GROUP_POW }, \
                                         { "hwr",     RGXFWIF_LOG_TYPE_GROUP_HWR }, \
                                         { "hwp",     RGXFWIF_LOG_TYPE_GROUP_HWP }, \
                                         { "rpm",     RGXFWIF_LOG_TYPE_GROUP_RPM }, \
                                         { "dma",     RGXFWIF_LOG_TYPE_GROUP_DMA }, \
                                         { "misc",    RGXFWIF_LOG_TYPE_GROUP_MISC }, \
                                         { "debug",   RGXFWIF_LOG_TYPE_GROUP_DEBUG }


/* Used in print statements to display log group state, one %s per group defined */
#define RGXFWIF_LOG_ENABLED_GROUPS_LIST_PFSPEC  "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"

/* Used in a print statement to display log group state, one per group */
#define RGXFWIF_LOG_ENABLED_GROUPS_LIST(types)  (((types) & RGXFWIF_LOG_TYPE_GROUP_MAIN)	?("main ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_MTS)		?("mts ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_CLEANUP)	?("cleanup ")	:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_CSW)		?("csw ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_BIF)		?("bif ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_PM)		?("pm ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_RTD)		?("rtd ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_SPM)		?("spm ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_POW)		?("pow ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_HWR)		?("hwr ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_HWP)		?("hwp ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_RPM)		?("rpm ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_DMA)		?("dma ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_MISC)	?("misc ")		:("")),		\
                                                (((types) & RGXFWIF_LOG_TYPE_GROUP_DEBUG)	?("debug ")		:(""))


/************************************************************************
* RGX FW signature checks
************************************************************************/
#define RGXFW_SIG_BUFFER_SIZE_MIN       (8192)

/*!
 ******************************************************************************
 * HWPERF
 *****************************************************************************/
/* Size of the Firmware L1 HWPERF buffer in bytes (2MB). Accessed by the
 * Firmware and host driver. */
#define RGXFW_HWPERF_L1_SIZE_MIN        (16U)
#define RGXFW_HWPERF_L1_SIZE_DEFAULT    PVRSRV_APPHINT_HWPERFFWBUFSIZEINKB
#define RGXFW_HWPERF_L1_SIZE_MAX        (12288U)

/* This padding value must always be large enough to hold the biggest
 * variable sized packet. */
#define RGXFW_HWPERF_L1_PADDING_DEFAULT (RGX_HWPERF_MAX_PACKET_SIZE)


#define RGXFWIF_TIMEDIFF_ID			((0x1UL << 28) | RGX_CR_TIMER)

/*!
 ******************************************************************************
 * Trace Buffer
 *****************************************************************************/

/*! Default size of RGXFWIF_TRACEBUF_SPACE in DWords */
#define RGXFW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS 12000U
#define RGXFW_TRACE_BUFFER_ASSERT_SIZE 200U
#if defined(RGXFW_META_SUPPORT_2ND_THREAD)
#define RGXFW_THREAD_NUM 2U
#else
#define RGXFW_THREAD_NUM 1U
#endif

#define RGXFW_POLL_TYPE_SET 0x80000000U

typedef struct
{
	IMG_CHAR	szPath[RGXFW_TRACE_BUFFER_ASSERT_SIZE];
	IMG_CHAR	szInfo[RGXFW_TRACE_BUFFER_ASSERT_SIZE];
	IMG_UINT32	ui32LineNum;
} UNCACHED_ALIGN RGXFWIF_FILE_INFO_BUF;

typedef struct
{
	IMG_UINT32			ui32TracePointer;

#if defined(RGX_FIRMWARE)
	IMG_UINT32 *pui32RGXFWIfTraceBuffer;		/* To be used by firmware for writing into trace buffer */
#else
	RGXFWIF_DEV_VIRTADDR pui32RGXFWIfTraceBuffer;
#endif
	IMG_PUINT32             pui32TraceBuffer;	/* To be used by host when reading from trace buffer */

	RGXFWIF_FILE_INFO_BUF	sAssertBuf;
} UNCACHED_ALIGN RGXFWIF_TRACEBUF_SPACE;

#define RGXFWIF_FWFAULTINFO_MAX		(8U)			/* Total number of FW fault logs stored */

typedef struct
{
	IMG_UINT64 RGXFW_ALIGN	ui64CRTimer;
	IMG_UINT64 RGXFW_ALIGN	ui64OSTimer;
	IMG_UINT32 RGXFW_ALIGN	ui32Data;
	IMG_UINT32 ui32Reserved;
	RGXFWIF_FILE_INFO_BUF	sFaultBuf;
} UNCACHED_ALIGN RGX_FWFAULTINFO;


#define RGXFWIF_POW_STATES \
  X(RGXFWIF_POW_OFF)			/* idle and handshaked with the host (ready to full power down) */ \
  X(RGXFWIF_POW_ON)				/* running HW commands */ \
  X(RGXFWIF_POW_FORCED_IDLE)	/* forced idle */ \
  X(RGXFWIF_POW_IDLE)			/* idle waiting for host handshake */

typedef enum
{
#define X(NAME) NAME,
	RGXFWIF_POW_STATES
#undef X
} RGXFWIF_POW_STATE;

/* Firmware HWR states */
#define RGXFWIF_HWR_HARDWARE_OK			(0x1U << 0U)	/*!< The HW state is ok or locked up */
#define RGXFWIF_HWR_RESET_IN_PROGRESS	(0x1U << 1U)	/*!< Tells if a HWR reset is in progress */
#define RGXFWIF_HWR_ANALYSIS_DONE		(0x1U << 2U)	/*!< The analysis of a GPU lockup has been performed */
#define RGXFWIF_HWR_GENERAL_LOCKUP		(0x1U << 3U)	/*!< A DM unrelated lockup has been detected */
#define RGXFWIF_HWR_DM_RUNNING_OK		(0x1U << 4U)	/*!< At least one DM is running without being close to a lockup */
#define RGXFWIF_HWR_DM_STALLING			(0x1U << 5U)	/*!< At least one DM is close to lockup */
#define RGXFWIF_HWR_FW_FAULT			(0x1U << 6U)	/*!< The FW has faulted and needs to restart */
#define RGXFWIF_HWR_RESTART_REQUESTED	(0x1U << 7U)	/*!< The FW has requested the host to restart it */

#define RGXFWIF_PHR_STATE_SHIFT			(8U)
#define RGXFWIF_PHR_RESTART_REQUESTED	(IMG_UINT32_C(1) << RGXFWIF_PHR_STATE_SHIFT)	/*!< The FW has requested the host to restart it, per PHR configuration */
#define RGXFWIF_PHR_RESTART_FINISHED	(IMG_UINT32_C(2) << RGXFWIF_PHR_STATE_SHIFT)	/*!< A PHR triggered GPU reset has just finished */
#define RGXFWIF_PHR_RESTART_MASK		(RGXFWIF_PHR_RESTART_REQUESTED | RGXFWIF_PHR_RESTART_FINISHED)

typedef IMG_UINT32 RGXFWIF_HWR_STATEFLAGS;

/* Firmware per-DM HWR states */
#define RGXFWIF_DM_STATE_WORKING					(0x00U)		/*!< DM is working if all flags are cleared */
#define RGXFWIF_DM_STATE_READY_FOR_HWR				(IMG_UINT32_C(0x1) << 0)	/*!< DM is idle and ready for HWR */
#define RGXFWIF_DM_STATE_NEEDS_SKIP					(IMG_UINT32_C(0x1) << 2)	/*!< DM need to skip to next cmd before resuming processing */
#define RGXFWIF_DM_STATE_NEEDS_PR_CLEANUP			(IMG_UINT32_C(0x1) << 3)	/*!< DM need partial render cleanup before resuming processing */
#define RGXFWIF_DM_STATE_NEEDS_TRACE_CLEAR			(IMG_UINT32_C(0x1) << 4)	/*!< DM need to increment Recovery Count once fully recovered */
#define RGXFWIF_DM_STATE_GUILTY_LOCKUP				(IMG_UINT32_C(0x1) << 5)	/*!< DM was identified as locking up and causing HWR */
#define RGXFWIF_DM_STATE_INNOCENT_LOCKUP			(IMG_UINT32_C(0x1) << 6)	/*!< DM was innocently affected by another lockup which caused HWR */
#define RGXFWIF_DM_STATE_GUILTY_OVERRUNING			(IMG_UINT32_C(0x1) << 7)	/*!< DM was identified as over-running and causing HWR */
#define RGXFWIF_DM_STATE_INNOCENT_OVERRUNING		(IMG_UINT32_C(0x1) << 8)	/*!< DM was innocently affected by another DM over-running which caused HWR */
#define RGXFWIF_DM_STATE_HARD_CONTEXT_SWITCH		(IMG_UINT32_C(0x1) << 9)	/*!< DM was forced into HWR as it delayed more important workloads */
#define RGXFWIF_DM_STATE_GPU_ECC_HWR				(IMG_UINT32_C(0x1) << 10)	/*!< DM was forced into HWR due to an uncorrected GPU ECC error */

/* Firmware's connection state */
typedef enum
{
	RGXFW_CONNECTION_FW_OFFLINE = 0,	/*!< Firmware is offline */
	RGXFW_CONNECTION_FW_READY,			/*!< Firmware is initialised */
	RGXFW_CONNECTION_FW_ACTIVE,			/*!< Firmware connection is fully established */
	RGXFW_CONNECTION_FW_OFFLOADING,		/*!< Firmware is clearing up connection data */
	RGXFW_CONNECTION_FW_STATE_COUNT
} RGXFWIF_CONNECTION_FW_STATE;

/* OS' connection state */
typedef enum
{
	RGXFW_CONNECTION_OS_OFFLINE = 0,	/*!< OS is offline */
	RGXFW_CONNECTION_OS_READY,			/*!< OS's KM driver is setup and waiting */
	RGXFW_CONNECTION_OS_ACTIVE,			/*!< OS connection is fully established */
	RGXFW_CONNECTION_OS_STATE_COUNT
} RGXFWIF_CONNECTION_OS_STATE;

typedef struct
{
	IMG_UINT			bfOsState		: 3;
	IMG_UINT			bfFLOk			: 1;
	IMG_UINT			bfFLGrowPending	: 1;
	IMG_UINT			bfIsolatedOS	: 1;
	IMG_UINT			bfReserved		: 26;
} RGXFWIF_OS_RUNTIME_FLAGS;

typedef IMG_UINT32 RGXFWIF_HWR_RECOVERYFLAGS;

#if defined(PVRSRV_STALLED_CCB_ACTION)
#define PVR_SLR_LOG_ENTRIES 10
#define PVR_SLR_LOG_STRLEN  30 /*!< MAX_CLIENT_CCB_NAME not visible to this header */

typedef struct
{
	IMG_UINT64 RGXFW_ALIGN	ui64Timestamp;
	IMG_UINT32				ui32FWCtxAddr;
	IMG_UINT32				ui32NumUFOs;
	IMG_CHAR				aszCCBName[PVR_SLR_LOG_STRLEN];
} UNCACHED_ALIGN RGXFWIF_SLR_ENTRY;
#endif

/* firmware trace control data */
typedef struct
{
	IMG_UINT32              ui32LogType;
	RGXFWIF_TRACEBUF_SPACE  sTraceBuf[RGXFW_THREAD_NUM];
	IMG_UINT32              ui32TraceBufSizeInDWords; /*!< Member initialised only when sTraceBuf is actually allocated
                                                       * (in RGXTraceBufferInitOnDemandResources) */
	IMG_UINT32              ui32TracebufFlags;        /*!< Compatibility and other flags */
} UNCACHED_ALIGN RGXFWIF_TRACEBUF;

/* firmware system data shared with the Host driver */
typedef struct
{
	IMG_UINT32                 ui32ConfigFlags;                       /*!< Configuration flags from host */
	IMG_UINT32                 ui32ConfigFlagsExt;                    /*!< Extended configuration flags from host */
	volatile RGXFWIF_POW_STATE ePowState;
	volatile IMG_UINT32        ui32HWPerfRIdx;
	volatile IMG_UINT32        ui32HWPerfWIdx;
	volatile IMG_UINT32        ui32HWPerfWrapCount;
	IMG_UINT32                 ui32HWPerfSize;                        /*!< Constant after setup, needed in FW */
	IMG_UINT32                 ui32HWPerfDropCount;                   /*!< The number of times the FW drops a packet due to buffer full */

	/* ui32HWPerfUt, ui32FirstDropOrdinal, ui32LastDropOrdinal only valid when FW is built with
	 * RGX_HWPERF_UTILIZATION & RGX_HWPERF_DROP_TRACKING defined in rgxfw_hwperf.c */
	IMG_UINT32                 ui32HWPerfUt;                          /*!< Buffer utilisation, high watermark of bytes in use */
	IMG_UINT32                 ui32FirstDropOrdinal;                  /*!< The ordinal of the first packet the FW dropped */
	IMG_UINT32                 ui32LastDropOrdinal;                   /*!< The ordinal of the last packet the FW dropped */
	RGXFWIF_OS_RUNTIME_FLAGS   asOsRuntimeFlagsMirror[RGXFW_MAX_NUM_OS];/*!< State flags for each Operating System mirrored from Fw coremem */
	IMG_UINT32                 aui32OSidPrioMirror[RGXFW_MAX_NUM_OS]; /*!< Priority for each Operating System mirrored from Fw coremem */
	IMG_UINT32                 ui32PHRModeMirror;                     /*!< Periodic Hardware Reset Mode mirrored from Fw coremem */
	RGX_FWFAULTINFO            sFaultInfo[RGXFWIF_FWFAULTINFO_MAX];
	IMG_UINT32                 ui32FWFaults;
	IMG_UINT32                 aui32CrPollAddr[RGXFW_THREAD_NUM];
	IMG_UINT32                 aui32CrPollMask[RGXFW_THREAD_NUM];
	IMG_UINT32                 aui32CrPollCount[RGXFW_THREAD_NUM];
	IMG_UINT64 RGXFW_ALIGN     ui64StartIdleTime;
#if defined(SUPPORT_POWMON_COMPONENT)
#if defined(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS)
	RGXFWIF_TRACEBUF_SPACE     sPowerMonBuf;
	IMG_UINT32                 ui32PowerMonBufSizeInDWords;
#endif
	IMG_UINT32                 ui32PowMonEnergy;                      /*!< Non-volatile power monitoring results:
	                                                                   * static power (by default)
	                                                                   * energy count (PVR_POWER_MONITOR_DYNAMIC_ENERGY) */
#endif

#if defined(SUPPORT_VALIDATION)
	IMG_UINT32                 ui32KillingCtl;                        /*!< DM Killing Configuration from host */
#endif
#if defined(SUPPORT_RGXFW_STATS_FRAMEWORK)
#define RGXFWIF_STATS_FRAMEWORK_LINESIZE    (8)
#define RGXFWIF_STATS_FRAMEWORK_MAX         (2048*RGXFWIF_STATS_FRAMEWORK_LINESIZE)
	IMG_UINT32 RGXFW_ALIGN     aui32FWStatsBuf[RGXFWIF_STATS_FRAMEWORK_MAX];
#endif
	RGXFWIF_HWR_STATEFLAGS     ui32HWRStateFlags;
	RGXFWIF_HWR_RECOVERYFLAGS  aui32HWRRecoveryFlags[RGXFWIF_DM_DEFAULT_MAX];
	IMG_UINT32                 ui32FwSysDataFlags;                      /*!< Compatibility and other flags */
} UNCACHED_ALIGN RGXFWIF_SYSDATA;

/* per-os firmware shared data */
typedef struct
{
	IMG_UINT32                 ui32FwOsConfigFlags;                   /*!< Configuration flags from an OS */
	IMG_UINT32                 ui32FWSyncCheckMark;                   /*!< Markers to signal that the host should perform a full sync check */
	IMG_UINT32                 ui32HostSyncCheckMark;
#if defined(PVRSRV_STALLED_CCB_ACTION)
	IMG_UINT32                 ui32ForcedUpdatesRequested;
	IMG_UINT8                  ui8SLRLogWp;
	RGXFWIF_SLR_ENTRY          sSLRLogFirst;
	RGXFWIF_SLR_ENTRY          sSLRLog[PVR_SLR_LOG_ENTRIES];
	IMG_UINT64 RGXFW_ALIGN     ui64LastForcedUpdateTime;
#endif
	volatile IMG_UINT32        aui32InterruptCount[RGXFW_THREAD_NUM]; /*!< Interrupt count from Threads > */
	IMG_UINT32                 ui32KCCBCmdsExecuted;
	RGXFWIF_DEV_VIRTADDR       sPowerSync;
	IMG_UINT32                 ui32FwOsDataFlags;                       /*!< Compatibility and other flags */
} UNCACHED_ALIGN RGXFWIF_OSDATA;

/* Firmware trace time-stamp field breakup */

/* RGX_CR_TIMER register read (48 bits) value*/
#define RGXFWT_TIMESTAMP_TIME_SHIFT                   (0U)
#define RGXFWT_TIMESTAMP_TIME_CLRMSK                  (IMG_UINT64_C(0xFFFF000000000000))

/* Extra debug-info (16 bits) */
#define RGXFWT_TIMESTAMP_DEBUG_INFO_SHIFT             (48U)
#define RGXFWT_TIMESTAMP_DEBUG_INFO_CLRMSK            ~RGXFWT_TIMESTAMP_TIME_CLRMSK


/* Debug-info sub-fields */
/* Bit 0: RGX_CR_EVENT_STATUS_MMU_PAGE_FAULT bit from RGX_CR_EVENT_STATUS register */
#define RGXFWT_DEBUG_INFO_MMU_PAGE_FAULT_SHIFT        (0U)
#define RGXFWT_DEBUG_INFO_MMU_PAGE_FAULT_SET          (1U << RGXFWT_DEBUG_INFO_MMU_PAGE_FAULT_SHIFT)

/* Bit 1: RGX_CR_BIF_MMU_ENTRY_PENDING bit from RGX_CR_BIF_MMU_ENTRY register */
#define RGXFWT_DEBUG_INFO_MMU_ENTRY_PENDING_SHIFT     (1U)
#define RGXFWT_DEBUG_INFO_MMU_ENTRY_PENDING_SET       (1U << RGXFWT_DEBUG_INFO_MMU_ENTRY_PENDING_SHIFT)

/* Bit 2: RGX_CR_SLAVE_EVENT register is non-zero */
#define RGXFWT_DEBUG_INFO_SLAVE_EVENTS_SHIFT          (2U)
#define RGXFWT_DEBUG_INFO_SLAVE_EVENTS_SET            (1U << RGXFWT_DEBUG_INFO_SLAVE_EVENTS_SHIFT)

/* Bit 3-15: Unused bits */

#define RGXFWT_DEBUG_INFO_STR_MAXLEN                  64
#define RGXFWT_DEBUG_INFO_STR_PREPEND                 " (debug info: "
#define RGXFWT_DEBUG_INFO_STR_APPEND                  ")"

/* Table of debug info sub-field's masks and corresponding message strings
 * to be appended to firmware trace
 *
 * Mask     : 16 bit mask to be applied to debug-info field
 * String   : debug info message string
 */

#define RGXFWT_DEBUG_INFO_MSKSTRLIST \
/*Mask,                                           String*/ \
X(RGXFWT_DEBUG_INFO_MMU_PAGE_FAULT_SET,      "mmu pf") \
X(RGXFWT_DEBUG_INFO_MMU_ENTRY_PENDING_SET,   "mmu pending") \
X(RGXFWT_DEBUG_INFO_SLAVE_EVENTS_SET,        "slave events")

/*!
 ******************************************************************************
 * HWR Data
 *****************************************************************************/
typedef enum
{
	RGX_HWRTYPE_UNKNOWNFAILURE = 0,
	RGX_HWRTYPE_OVERRUN        = 1,
	RGX_HWRTYPE_POLLFAILURE    = 2,
	RGX_HWRTYPE_BIF0FAULT      = 3,
	RGX_HWRTYPE_BIF1FAULT      = 4,
	RGX_HWRTYPE_TEXASBIF0FAULT = 5,
	RGX_HWRTYPE_MMUFAULT       = 6,
	RGX_HWRTYPE_MMUMETAFAULT   = 7,
	RGX_HWRTYPE_MIPSTLBFAULT   = 8,
	RGX_HWRTYPE_ECCFAULT       = 9,
	RGX_HWRTYPE_MMURISCVFAULT  = 10,
} RGX_HWRTYPE;

#define RGXFWIF_HWRTYPE_BIF_BANK_GET(eHWRType) (((eHWRType) == RGX_HWRTYPE_BIF0FAULT) ? 0 : 1)

#define RGXFWIF_HWRTYPE_PAGE_FAULT_GET(eHWRType) ((((eHWRType) == RGX_HWRTYPE_BIF0FAULT)      ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_BIF1FAULT)      ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_TEXASBIF0FAULT) ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_MMUFAULT)       ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_MMUMETAFAULT)   ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_MIPSTLBFAULT)   ||       \
                                                   ((eHWRType) == RGX_HWRTYPE_MMURISCVFAULT)) ? true : false)

typedef struct
{
	IMG_UINT32 ui32FaultGPU;
} RGX_ECCINFO;

typedef struct
{
	IMG_UINT64	RGXFW_ALIGN		aui64MMUStatus[2];
	IMG_UINT64	RGXFW_ALIGN		ui64PCAddress; /*!< phys address of the page catalogue */
	IMG_UINT64	RGXFW_ALIGN		ui64Reserved;
} RGX_MMUINFO;

typedef struct
{
	IMG_UINT32	ui32ThreadNum;
	IMG_UINT32	ui32CrPollAddr;
	IMG_UINT32	ui32CrPollMask;
	IMG_UINT32	ui32CrPollLastValue;
	IMG_UINT64	RGXFW_ALIGN ui64Reserved;
} UNCACHED_ALIGN RGX_POLLINFO;

typedef struct
{
	union
	{
		RGX_MMUINFO  sMMUInfo;
		RGX_POLLINFO sPollInfo;
		RGX_ECCINFO sECCInfo;
	} uHWRData;

	IMG_UINT64 RGXFW_ALIGN ui64CRTimer;
	IMG_UINT64 RGXFW_ALIGN ui64OSTimer;
	IMG_UINT32             ui32FrameNum;
	IMG_UINT32             ui32PID;
	IMG_UINT32             ui32ActiveHWRTData;
	IMG_UINT32             ui32HWRNumber;
	IMG_UINT32             ui32EventStatus;
	IMG_UINT32             ui32HWRRecoveryFlags;
	RGX_HWRTYPE            eHWRType;
	RGXFWIF_DM             eDM;
	IMG_UINT32             ui32CoreID;
	RGX_HW_ERR             eHWErrorCode;			/*!< Error code used to determine HW fault */
	IMG_UINT64 RGXFW_ALIGN ui64CRTimeOfKick;
	IMG_UINT64 RGXFW_ALIGN ui64CRTimeHWResetStart;
	IMG_UINT64 RGXFW_ALIGN ui64CRTimeHWResetFinish;
	IMG_UINT64 RGXFW_ALIGN ui64CRTimeFreelistReady;
	IMG_UINT64 RGXFW_ALIGN ui64Reserved;			/*!< Pad to 16 64-bit words */
} UNCACHED_ALIGN RGX_HWRINFO;

#define RGXFWIF_HWINFO_MAX_FIRST 8U							/* Number of first HWR logs recorded (never overwritten by newer logs) */
#define RGXFWIF_HWINFO_MAX_LAST 8U							/* Number of latest HWR logs (older logs are overwritten by newer logs) */
#define RGXFWIF_HWINFO_MAX (RGXFWIF_HWINFO_MAX_FIRST + RGXFWIF_HWINFO_MAX_LAST)	/* Total number of HWR logs stored in a buffer */
#define RGXFWIF_HWINFO_LAST_INDEX (RGXFWIF_HWINFO_MAX - 1U)	/* Index of the last log in the HWR log buffer */

typedef struct
{
	RGX_HWRINFO sHWRInfo[RGXFWIF_HWINFO_MAX];
	IMG_UINT32  ui32HwrCounter;
	IMG_UINT32  ui32WriteIndex;
	IMG_UINT32  ui32DDReqCount;
	IMG_UINT32  ui32HWRInfoBufFlags; /* Compatibility and other flags */
	IMG_UINT32  aui32HwrDmLockedUpCount[RGXFWIF_DM_DEFAULT_MAX];
	IMG_UINT32  aui32HwrDmOverranCount[RGXFWIF_DM_DEFAULT_MAX];
	IMG_UINT32  aui32HwrDmRecoveredCount[RGXFWIF_DM_DEFAULT_MAX];
	IMG_UINT32  aui32HwrDmFalseDetectCount[RGXFWIF_DM_DEFAULT_MAX];
} UNCACHED_ALIGN RGXFWIF_HWRINFOBUF;

#define RGXFWIF_CTXSWITCH_PROFILE_FAST_EN		(1U)
#define RGXFWIF_CTXSWITCH_PROFILE_MEDIUM_EN		(2U)
#define RGXFWIF_CTXSWITCH_PROFILE_SLOW_EN		(3U)
#define RGXFWIF_CTXSWITCH_PROFILE_NODELAY_EN	(4U)

#define RGXFWIF_CDM_ARBITRATION_TASK_DEMAND_EN	(0x1)
#define RGXFWIF_CDM_ARBITRATION_ROUND_ROBIN_EN	(0x2)

#define RGXFWIF_ISP_SCHEDMODE_VER1_IPP			(0x1)
#define RGXFWIF_ISP_SCHEDMODE_VER2_ISP			(0x2)
/*!
 ******************************************************************************
 * RGX firmware Init Config Data
 * NOTE: Please be careful to keep backwards compatibility with DDKv1 for the
 * CTXSWITCH controls.
 *****************************************************************************/

/* Flag definitions affecting the firmware globally */
#define RGXFWIF_INICFG_CTXSWITCH_MODE_RAND					(IMG_UINT32_C(0x1) << 0)	/*!< Randomise context switch requests */
#define RGXFWIF_INICFG_CTXSWITCH_SRESET_EN					(IMG_UINT32_C(0x1) << 1)
#define RGXFWIF_INICFG_HWPERF_EN							(IMG_UINT32_C(0x1) << 2)
#define RGXFWIF_INICFG_DM_KILL_MODE_RAND_EN					(IMG_UINT32_C(0x1) << 3)	/*!< Randomise DM-killing requests */
#define RGXFWIF_INICFG_POW_RASCALDUST						(IMG_UINT32_C(0x1) << 4)
/* 5 unused */
#define RGXFWIF_INICFG_FBCDC_V3_1_EN						(IMG_UINT32_C(0x1) << 6)
#define RGXFWIF_INICFG_CHECK_MLIST_EN						(IMG_UINT32_C(0x1) << 7)
#define RGXFWIF_INICFG_DISABLE_CLKGATING_EN					(IMG_UINT32_C(0x1) << 8)
#define RGXFWIF_INICFG_POLL_COUNTERS_EN						(IMG_UINT32_C(0x1) << 9)
/* 10 unused */
/* 11 unused */
#define RGXFWIF_INICFG_REGCONFIG_EN							(IMG_UINT32_C(0x1) << 12)
#define RGXFWIF_INICFG_ASSERT_ON_OUTOFMEMORY				(IMG_UINT32_C(0x1) << 13)
#define RGXFWIF_INICFG_HWP_DISABLE_FILTER					(IMG_UINT32_C(0x1) << 14)
#define RGXFWIF_INICFG_CUSTOM_PERF_TIMER_EN					(IMG_UINT32_C(0x1) << 15)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT				(16)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_FAST				(RGXFWIF_CTXSWITCH_PROFILE_FAST_EN << RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_MEDIUM				(RGXFWIF_CTXSWITCH_PROFILE_MEDIUM_EN << RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_SLOW				(RGXFWIF_CTXSWITCH_PROFILE_SLOW_EN << RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_NODELAY			(RGXFWIF_CTXSWITCH_PROFILE_NODELAY_EN << RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT)
#define RGXFWIF_INICFG_CTXSWITCH_PROFILE_MASK				(IMG_UINT32_C(0x7) << RGXFWIF_INICFG_CTXSWITCH_PROFILE_SHIFT)
#define RGXFWIF_INICFG_DISABLE_DM_OVERLAP					(IMG_UINT32_C(0x1) << 19)
#define RGXFWIF_INICFG_ASSERT_ON_HWR_TRIGGER				(IMG_UINT32_C(0x1) << 20)
#define RGXFWIF_INICFG_FABRIC_COHERENCY_ENABLED				(IMG_UINT32_C(0x1) << 21)
#define RGXFWIF_INICFG_VALIDATE_IRQ							(IMG_UINT32_C(0x1) << 22)
#define RGXFWIF_INICFG_DISABLE_PDP_EN						(IMG_UINT32_C(0x1) << 23)
#define RGXFWIF_INICFG_SPU_POWER_STATE_MASK_CHANGE_EN		(IMG_UINT32_C(0x1) << 24)
#define RGXFWIF_INICFG_WORKEST								(IMG_UINT32_C(0x1) << 25)
#define RGXFWIF_INICFG_PDVFS								(IMG_UINT32_C(0x1) << 26)
#define RGXFWIF_INICFG_CDM_ARBITRATION_SHIFT				(27)
#define RGXFWIF_INICFG_CDM_ARBITRATION_TASK_DEMAND			(RGXFWIF_CDM_ARBITRATION_TASK_DEMAND_EN << RGXFWIF_INICFG_CDM_ARBITRATION_SHIFT)
#define RGXFWIF_INICFG_CDM_ARBITRATION_ROUND_ROBIN			(RGXFWIF_CDM_ARBITRATION_ROUND_ROBIN_EN << RGXFWIF_INICFG_CDM_ARBITRATION_SHIFT)
#define RGXFWIF_INICFG_CDM_ARBITRATION_MASK					(IMG_UINT32_C(0x3) << RGXFWIF_INICFG_CDM_ARBITRATION_SHIFT)
#define RGXFWIF_INICFG_ISPSCHEDMODE_SHIFT					(29)
#define RGXFWIF_INICFG_ISPSCHEDMODE_NONE					(0)
#define RGXFWIF_INICFG_ISPSCHEDMODE_VER1_IPP				(RGXFWIF_ISP_SCHEDMODE_VER1_IPP << RGXFWIF_INICFG_ISPSCHEDMODE_SHIFT)
#define RGXFWIF_INICFG_ISPSCHEDMODE_VER2_ISP				(RGXFWIF_ISP_SCHEDMODE_VER2_ISP << RGXFWIF_INICFG_ISPSCHEDMODE_SHIFT)
#define RGXFWIF_INICFG_ISPSCHEDMODE_MASK					(RGXFWIF_INICFG_ISPSCHEDMODE_VER1_IPP |\
                                                             RGXFWIF_INICFG_ISPSCHEDMODE_VER2_ISP)
#define RGXFWIF_INICFG_VALIDATE_SOCUSC_TIMER				(IMG_UINT32_C(0x1) << 31)
#define RGXFWIF_INICFG_ALL									(0xFFFFF3FFU)

/* Extended Flag definitions affecting the firmware globally */
#define RGXFWIF_INICFG_EXT_ALL								(0x0U)

#define RGXFWIF_INICFG_SYS_CTXSWITCH_CLRMSK					~(RGXFWIF_INICFG_CTXSWITCH_MODE_RAND | \
															  RGXFWIF_INICFG_CTXSWITCH_SRESET_EN)

/* Flag definitions affecting only workloads submitted by a particular OS */
#define RGXFWIF_INICFG_OS_CTXSWITCH_TDM_EN					(IMG_UINT32_C(0x1) << 0)
#define RGXFWIF_INICFG_OS_CTXSWITCH_GEOM_EN					(IMG_UINT32_C(0x1) << 1)	/*!< Enables GEOM-TA and GEOM-SHG context switch */
#define RGXFWIF_INICFG_OS_CTXSWITCH_3D_EN					(IMG_UINT32_C(0x1) << 2)
#define RGXFWIF_INICFG_OS_CTXSWITCH_CDM_EN					(IMG_UINT32_C(0x1) << 3)
/* #define RGXFWIF_INICFG_OS_CTXSWITCH_RTU_EN_DEPRECATED	(IMG_UINT32_C(0x1) << 4)	!< Used for RTU DM-kill only. The RTU does not context switch */

#define RGXFWIF_INICFG_OS_LOW_PRIO_CS_TDM					(IMG_UINT32_C(0x1) << 4)
#define RGXFWIF_INICFG_OS_LOW_PRIO_CS_GEOM					(IMG_UINT32_C(0x1) << 5)
#define RGXFWIF_INICFG_OS_LOW_PRIO_CS_3D					(IMG_UINT32_C(0x1) << 6)
#define RGXFWIF_INICFG_OS_LOW_PRIO_CS_CDM					(IMG_UINT32_C(0x1) << 7)

#define RGXFWIF_INICFG_OS_CTXSWITCH_DM_ALL					(RGXFWIF_INICFG_OS_CTXSWITCH_GEOM_EN | \
															 RGXFWIF_INICFG_OS_CTXSWITCH_3D_EN | \
															 RGXFWIF_INICFG_OS_CTXSWITCH_CDM_EN | \
															 RGXFWIF_INICFG_OS_CTXSWITCH_TDM_EN)

#define RGXFWIF_INICFG_OS_ALL								(0xFFU)

#define RGXFWIF_INICFG_OS_CTXSWITCH_CLRMSK					~(RGXFWIF_INICFG_OS_CTXSWITCH_DM_ALL)

#define RGXFWIF_FILTCFG_TRUNCATE_HALF		(0x1U << 3)
#define RGXFWIF_FILTCFG_TRUNCATE_INT		(0x1U << 2)
#define RGXFWIF_FILTCFG_NEW_FILTER_MODE		(0x1U << 1)

typedef enum
{
	RGX_ACTIVEPM_FORCE_OFF = 0,
	RGX_ACTIVEPM_FORCE_ON = 1,
	RGX_ACTIVEPM_DEFAULT = 2
} RGX_ACTIVEPM_CONF;

typedef enum
{
	RGX_RD_POWER_ISLAND_FORCE_OFF = 0,
	RGX_RD_POWER_ISLAND_FORCE_ON = 1,
	RGX_RD_POWER_ISLAND_DEFAULT = 2
} RGX_RD_POWER_ISLAND_CONF;

/*!
 ******************************************************************************
 * Querying DM state
 *****************************************************************************/

typedef struct
{
	IMG_UINT16 ui16RegNum;				/*!< Register number */
	IMG_UINT16 ui16IndirectRegNum;		/*!< Indirect register number (or 0 if not used) */
	IMG_UINT16 ui16IndirectStartVal;	/*!< Start value for indirect register */
	IMG_UINT16 ui16IndirectEndVal;		/*!< End value for indirect register */
} RGXFW_REGISTER_LIST;


#if defined(RGX_FIRMWARE)
typedef DLLIST_NODE							RGXFWIF_DLLIST_NODE;
#else
typedef struct {RGXFWIF_DEV_VIRTADDR p;
                RGXFWIF_DEV_VIRTADDR n;}	RGXFWIF_DLLIST_NODE;
#endif

typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_SIGBUFFER;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_TRACEBUF;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_SYSDATA;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_OSDATA;
#if defined(SUPPORT_TBI_INTERFACE)
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_TBIBUF;
#endif
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_HWPERFBUF;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_HWRINFOBUF;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_RUNTIME_CFG;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_GPU_UTIL_FWCB;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_REG_CFG;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_HWPERF_CTL;
typedef RGXFWIF_DEV_VIRTADDR  PRGX_HWPERF_CONFIG_CNTBLK;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CCB_CTL;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CCB;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CCB_RTN_SLOTS;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_FWMEMCONTEXT;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_FWCOMMONCONTEXT;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_ZSBUFFER;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_COMMONCTX_STATE;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CORE_CLK_RATE;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_FIRMWAREGCOVBUFFER;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CCCB;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_CCCB_CTL;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_FREELIST;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_HWRTDATA;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_TIMESTAMP_ADDR;
typedef RGXFWIF_DEV_VIRTADDR  PRGXFWIF_RF_CMD;

/*!
 * This number is used to represent an invalid page catalogue physical address
 */
#define RGXFWIF_INVALID_PC_PHYADDR 0xFFFFFFFFFFFFFFFFLLU

/*!
 * This number is used to represent unallocated page catalog base register
 */
#define RGXFW_BIF_INVALID_PCREG 0xFFFFFFFFU

/*!
    Firmware memory context.
*/
typedef struct
{
	IMG_DEV_PHYADDR			RGXFW_ALIGN sPCDevPAddr;	/*!< device physical address of context's page catalogue */
	IMG_UINT32				uiPageCatBaseRegID;	/*!< associated page catalog base register (RGXFW_BIF_INVALID_PCREG == unallocated) */
	IMG_UINT32				uiBreakpointAddr; /*!< breakpoint address */
	IMG_UINT32				uiBPHandlerAddr; /*!< breakpoint handler address */
	IMG_UINT32				uiBreakpointCtl; /*!< DM and enable control for BP */
	IMG_UINT64				RGXFW_ALIGN ui64FBCStateIDMask;	/*!< FBCDC state descriptor IDs (non-zero means defer on mem context activation) */
	IMG_UINT32				ui32FwMemCtxFlags; /*!< Compatibility and other flags */

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	IMG_UINT32              ui32OSid;
	IMG_BOOL                bOSidAxiProt;
#endif

} UNCACHED_ALIGN RGXFWIF_FWMEMCONTEXT;

/*!
 * FW context state flags
 */
#define RGXFWIF_CONTEXT_FLAGS_NEED_RESUME			(0x00000001U)
#define RGXFWIF_CONTEXT_FLAGS_TDM_HEADER_STALE		(0x00000002U)

typedef struct
{
	/* FW-accessible TA state which must be written out to memory on context store */
	IMG_UINT64	RGXFW_ALIGN uTAReg_DCE_CMD0;
	IMG_UINT64	RGXFW_ALIGN uTAReg_DCE_CMD1;
	IMG_UINT64	RGXFW_ALIGN uTAReg_DCE_DRAW0;
	IMG_UINT64	RGXFW_ALIGN uTAReg_DCE_DRAW1;
	IMG_UINT64	RGXFW_ALIGN uTAReg_DCE_WRITE;
	IMG_UINT32	RGXFW_ALIGN uTAReg_GTA_SO_PRIM[4];
	IMG_UINT16	ui16TACurrentIdx;
} UNCACHED_ALIGN RGXFWIF_TACTX_STATE;

/* The following defines need to be auto generated using the HW defines
 * rather than hard coding it */
#define RGXFWIF_ISP_PIPE_COUNT_MAX		(20)
#define RGXFWIF_PIPE_COUNT_PER_ISP		(2)
#define RGXFWIF_IPP_RESUME_REG_COUNT	(1)

#if !defined(__KERNEL__)
#define RGXFWIF_ISP_COUNT		(RGX_FEATURE_NUM_SPU * RGX_FEATURE_NUM_ISP_PER_SPU)
#define RGXFWIF_ISP_PIPE_COUNT	(RGXFWIF_ISP_COUNT * RGXFWIF_PIPE_COUNT_PER_ISP)
#if RGXFWIF_ISP_PIPE_COUNT > RGXFWIF_ISP_PIPE_COUNT_MAX
#error RGXFWIF_ISP_PIPE_COUNT should not be greater than RGXFWIF_ISP_PIPE_COUNT_MAX
#endif
#endif /* !defined(__KERNEL__) */

typedef struct
{
#if defined(PM_INTERACTIVE_MODE)
	IMG_UINT64	RGXFW_ALIGN u3DReg_PM_DEALLOCATED_MASK_STATUS;	/*!< Managed by PM HW in the non-interactive mode */
#endif
	IMG_UINT32	ui32CtxStateFlags;	/*!< Compatibility and other flags */

	/* FW-accessible ISP state which must be written out to memory on context store */
	/* au3DReg_ISP_STORE should be the last element of the structure
	 * as this is an array whose size is determined at runtime
	 * after detecting the RGX core */
	IMG_UINT64	RGXFW_ALIGN au3DReg_ISP_STORE[];
} UNCACHED_ALIGN RGXFWIF_3DCTX_STATE;

#define RGXFWIF_CTX_USING_BUFFER_A		(0)
#define RGXFWIF_CTX_USING_BUFFER_B		(1U)

typedef struct
{
	IMG_UINT32  ui32CtxStateFlags;		/*!< Target buffer and other flags */
} RGXFWIF_COMPUTECTX_STATE;


typedef struct RGXFWIF_FWCOMMONCONTEXT_
{
	/* CCB details for this firmware context */
	PRGXFWIF_CCCB_CTL		psCCBCtl;				/*!< CCB control */
	PRGXFWIF_CCCB			psCCB;					/*!< CCB base */
	RGXFWIF_DMA_ADDR		sCCBMetaDMAAddr;

	RGXFWIF_DLLIST_NODE		RGXFW_ALIGN sWaitingNode;			/*!< List entry for the waiting list */
	RGXFWIF_DLLIST_NODE		RGXFW_ALIGN sRunNode;				/*!< List entry for the run list */
	RGXFWIF_UFO				sLastFailedUFO;						/*!< UFO that last failed (or NULL) */

	PRGXFWIF_FWMEMCONTEXT	psFWMemContext;			/*!< Memory context */

	/* Context suspend state */
	PRGXFWIF_COMMONCTX_STATE	RGXFW_ALIGN psContextState;		/*!< TA/3D context suspend state, read/written by FW */

	/*
	 * Flags e.g. for context switching
	 */
	IMG_UINT32				ui32FWComCtxFlags;
	IMG_UINT32				ui32Priority;
	IMG_UINT32				ui32PrioritySeqNum;

	/* Framework state */
	PRGXFWIF_RF_CMD				RGXFW_ALIGN psRFCmd;		/*!< Register updates for Framework */

	/* References to the host side originators */
	IMG_UINT32				ui32ServerCommonContextID;			/*!< the Server Common Context */
	IMG_UINT32				ui32PID;							/*!< associated process ID */

	/* Statistic updates waiting to be passed back to the host... */
	IMG_BOOL				bStatsPending;						/*!< True when some stats are pending */
	IMG_INT32				i32StatsNumStores;					/*!< Number of stores on this context since last update */
	IMG_INT32				i32StatsNumOutOfMemory;				/*!< Number of OOMs on this context since last update */
	IMG_INT32				i32StatsNumPartialRenders;			/*!< Number of PRs on this context since last update */
	RGXFWIF_DM				eDM;								/*!< Data Master type */
	IMG_UINT64				RGXFW_ALIGN  ui64WaitSignalAddress;	/*!< Device Virtual Address of the signal the context is waiting on */
	RGXFWIF_DLLIST_NODE		RGXFW_ALIGN  sWaitSignalNode;		/*!< List entry for the wait-signal list */
	RGXFWIF_DLLIST_NODE		RGXFW_ALIGN  sBufStalledNode;		/*!< List entry for the buffer stalled list */
	IMG_UINT64				RGXFW_ALIGN  ui64CBufQueueCtrlAddr;	/*!< Address of the circular buffer queue pointers */

	IMG_UINT64				RGXFW_ALIGN  ui64RobustnessAddress;
	IMG_UINT32				ui32MaxDeadlineMS;					/*!< Max HWR deadline limit in ms */
	bool					bReadOffsetNeedsReset;				/*!< Following HWR circular buffer read-offset needs resetting */
} UNCACHED_ALIGN RGXFWIF_FWCOMMONCONTEXT;

/*!
	Firmware render context.
*/
typedef struct
{
	RGXFWIF_FWCOMMONCONTEXT	sTAContext;				/*!< Firmware context for the TA */
	RGXFWIF_FWCOMMONCONTEXT	s3DContext;				/*!< Firmware context for the 3D */

	IMG_UINT32			ui32TotalNumPartialRenders; /*!< Total number of partial renders */
	IMG_UINT32			ui32TotalNumOutOfMemory;	/*!< Total number of OOMs */
	IMG_UINT32			ui32WorkEstCCBSubmitted; /*!< Number of commands submitted to the WorkEst FW CCB */
	IMG_UINT32			ui32FwRenderCtxFlags; /*!< Compatibility and other flags */

	RGXFWIF_STATIC_RENDERCONTEXT_STATE sStaticRenderContextState;

} UNCACHED_ALIGN RGXFWIF_FWRENDERCONTEXT;

/*!
	Firmware compute context.
*/
typedef struct
{
	RGXFWIF_FWCOMMONCONTEXT sCDMContext;				/*!< Firmware context for the CDM */

	RGXFWIF_STATIC_COMPUTECONTEXT_STATE sStaticComputeContextState;

	IMG_UINT32			ui32WorkEstCCBSubmitted; /*!< Number of commands submitted to the WorkEst FW CCB */

	IMG_UINT32		ui32KZState;
	IMG_UINT32		aui32KZChecksum[RGX_KZ_MAX_NUM_CORES];
} UNCACHED_ALIGN RGXFWIF_FWCOMPUTECONTEXT;

typedef IMG_UINT64 RGXFWIF_TRP_CHECKSUM_2D[RGX_TRP_MAX_NUM_CORES][2];
typedef IMG_UINT64 RGXFWIF_TRP_CHECKSUM_3D[RGX_TRP_MAX_NUM_CORES][4];
typedef IMG_UINT64 RGXFWIF_TRP_CHECKSUM_GEOM[RGX_TRP_MAX_NUM_CORES][2];

/*!
	Firmware TDM context.
*/
typedef struct
{
	RGXFWIF_FWCOMMONCONTEXT	sTDMContext;				/*!< Firmware context for the TDM */

	IMG_UINT32			ui32WorkEstCCBSubmitted; /*!< Number of commands submitted to the WorkEst FW CCB */
#if defined(SUPPORT_TRP)
	/* TRP state
	 *
	 * Stored state is used to kick the same geometry or 3D twice,
	 * State is stored before first kick and restored before second to rerun the same data.
	 */
	IMG_UINT32				ui32TRPState;
	RGXFWIF_TRP_CHECKSUM_2D	RGXFW_ALIGN	aui64TRPChecksums2D;
#endif

} UNCACHED_ALIGN RGXFWIF_FWTDMCONTEXT;

/*!
 ******************************************************************************
 * Defines for CMD_TYPE corruption detection and forward compatibility check
 *****************************************************************************/

/* CMD_TYPE 32bit contains:
 * 31:16	Reserved for magic value to detect corruption (16 bits)
 * 15		Reserved for RGX_CCB_TYPE_TASK (1 bit)
 * 14:0		Bits available for CMD_TYPEs (15 bits) */


/* Magic value to detect corruption */
#define RGX_CMD_MAGIC_DWORD			IMG_UINT32_C(0x2ABC)
#define RGX_CMD_MAGIC_DWORD_MASK	(0xFFFF0000U)
#define RGX_CMD_MAGIC_DWORD_SHIFT	(16U)
#define RGX_CMD_MAGIC_DWORD_SHIFTED	(RGX_CMD_MAGIC_DWORD << RGX_CMD_MAGIC_DWORD_SHIFT)


/*!
 ******************************************************************************
 * Kernel CCB control for RGX
 *****************************************************************************/
typedef struct
{
	volatile IMG_UINT32		ui32WriteOffset;		/*!< write offset into array of commands (MUST be aligned to 16 bytes!) */
	volatile IMG_UINT32		ui32ReadOffset;			/*!< read offset into array of commands */
	IMG_UINT32				ui32WrapMask;			/*!< Offset wrapping mask (Total capacity of the CCB - 1) */
	IMG_UINT32				ui32CmdSize;			/*!< size of each command in bytes */
} UNCACHED_ALIGN RGXFWIF_CCB_CTL;

/*!
 ******************************************************************************
 * Kernel CCB command structure for RGX
 *****************************************************************************/
#define RGXFWIF_MMUCACHEDATA_FLAGS_PT      (0x1U) /* MMU_CTRL_INVAL_PT_EN */
#define RGXFWIF_MMUCACHEDATA_FLAGS_PD      (0x2U) /* MMU_CTRL_INVAL_PD_EN */
#define RGXFWIF_MMUCACHEDATA_FLAGS_PC      (0x4U) /* MMU_CTRL_INVAL_PC_EN */
#define RGXFWIF_MMUCACHEDATA_FLAGS_CTX_ALL (0x800U) /* MMU_CTRL_INVAL_ALL_CONTEXTS_EN */

#define RGXFWIF_MMUCACHEDATA_FLAGS_INTERRUPT (0x4000000U) /* indicates FW should interrupt the host */

typedef struct
{
	IMG_UINT32            ui32CacheFlags;
	RGXFWIF_DEV_VIRTADDR  sMMUCacheSync;
	IMG_UINT32            ui32MMUCacheSyncUpdateValue;
} RGXFWIF_MMUCACHEDATA;

#define RGXFWIF_BPDATA_FLAGS_ENABLE (1U << 0)
#define RGXFWIF_BPDATA_FLAGS_WRITE  (1U << 1)
#define RGXFWIF_BPDATA_FLAGS_CTL    (1U << 2)
#define RGXFWIF_BPDATA_FLAGS_REGS   (1U << 3)

typedef struct
{
	PRGXFWIF_FWMEMCONTEXT	psFWMemContext;			/*!< Memory context */
	IMG_UINT32		ui32BPAddr;			/*!< Breakpoint address */
	IMG_UINT32		ui32HandlerAddr;		/*!< Breakpoint handler */
	IMG_UINT32		ui32BPDM;			/*!< Breakpoint control */
	IMG_UINT32		ui32BPDataFlags;
	IMG_UINT32		ui32TempRegs;		/*!< Number of temporary registers to overallocate */
	IMG_UINT32		ui32SharedRegs;		/*!< Number of shared registers to overallocate */
	RGXFWIF_DM      eDM;                /*!< DM associated with the breakpoint */
} RGXFWIF_BPDATA;

#define RGXFWIF_KCCB_CMD_KICK_DATA_MAX_NUM_CLEANUP_CTLS (RGXFWIF_PRBUFFER_MAXSUPPORTED + 1U) /* +1 is RTDATASET cleanup */

typedef struct
{
	PRGXFWIF_FWCOMMONCONTEXT	psContext;			/*!< address of the firmware context */
	IMG_UINT32					ui32CWoffUpdate;	/*!< Client CCB woff update */
	IMG_UINT32					ui32CWrapMaskUpdate; /*!< Client CCB wrap mask update after CCCB growth */
	IMG_UINT32					ui32NumCleanupCtl;		/*!< number of CleanupCtl pointers attached */
	PRGXFWIF_CLEANUP_CTL		apsCleanupCtl[RGXFWIF_KCCB_CMD_KICK_DATA_MAX_NUM_CLEANUP_CTLS]; /*!< CleanupCtl structures associated with command */
	IMG_UINT32					ui32WorkEstCmdHeaderOffset; /*!< offset to the CmdHeader which houses the workload estimation kick data. */
} RGXFWIF_KCCB_CMD_KICK_DATA;

typedef struct
{
	RGXFWIF_KCCB_CMD_KICK_DATA	sTACmdKickData;
	RGXFWIF_KCCB_CMD_KICK_DATA	s3DCmdKickData;
} RGXFWIF_KCCB_CMD_COMBINED_TA_3D_KICK_DATA;

typedef struct
{
	PRGXFWIF_FWCOMMONCONTEXT	psContext;			/*!< address of the firmware context */
	IMG_UINT32					ui32CCBFenceOffset;	/*!< Client CCB fence offset */
} RGXFWIF_KCCB_CMD_FORCE_UPDATE_DATA;

typedef struct
{
	IMG_UINT32					ui32PHRMode;		/*!< Variable containing PHR configuration values */
} RGXFWIF_KCCB_CMD_PHR_CFG_DATA;

#define RGXIF_PHR_MODE_OFF						(0UL)
#define RGXIF_PHR_MODE_RD_RESET					(1UL)
#define RGXIF_PHR_MODE_FULL_RESET				(2UL)

typedef struct
{
	IMG_UINT32					ui32WdgPeriodUs;		/*!< Variable containing the requested watchdog period in microseconds */
} RGXFWIF_KCCB_CMD_WDG_CFG_DATA;

typedef enum
{
	RGXFWIF_CLEANUP_FWCOMMONCONTEXT,		/*!< FW common context cleanup */
	RGXFWIF_CLEANUP_HWRTDATA,				/*!< FW HW RT data cleanup */
	RGXFWIF_CLEANUP_FREELIST,				/*!< FW freelist cleanup */
	RGXFWIF_CLEANUP_ZSBUFFER,				/*!< FW ZS Buffer cleanup */
} RGXFWIF_CLEANUP_TYPE;

typedef struct
{
	RGXFWIF_CLEANUP_TYPE			eCleanupType;			/*!< Cleanup type */
	union {
		PRGXFWIF_FWCOMMONCONTEXT	psContext;				/*!< FW common context to cleanup */
		PRGXFWIF_HWRTDATA			psHWRTData;				/*!< HW RT to cleanup */
		PRGXFWIF_FREELIST			psFreelist;				/*!< Freelist to cleanup */
		PRGXFWIF_ZSBUFFER			psZSBuffer;				/*!< ZS Buffer to cleanup */
	} uCleanupData;
} RGXFWIF_CLEANUP_REQUEST;

typedef enum
{
	RGXFWIF_POW_OFF_REQ = 1,
	RGXFWIF_POW_FORCED_IDLE_REQ,
	RGXFWIF_POW_NUM_UNITS_CHANGE,
	RGXFWIF_POW_APM_LATENCY_CHANGE
} RGXFWIF_POWER_TYPE;

typedef enum
{
	RGXFWIF_POWER_FORCE_IDLE = 1,
	RGXFWIF_POWER_CANCEL_FORCED_IDLE,
	RGXFWIF_POWER_HOST_TIMEOUT,
} RGXFWIF_POWER_FORCE_IDLE_TYPE;

typedef struct
{
	RGXFWIF_POWER_TYPE					ePowType;					/*!< Type of power request */
	union
	{
		IMG_UINT32						ui32PowUnitsStateMask;/*!< New power units state mask*/
		IMG_BOOL						bForced;				/*!< If the operation is mandatory */
		RGXFWIF_POWER_FORCE_IDLE_TYPE	ePowRequestType;		/*!< Type of Request. Consolidating Force Idle, Cancel Forced Idle, Host Timeout */
		IMG_UINT32						ui32ActivePMLatencyms;	/*!< Number of milliseconds to set APM latency */
	} uPowerReqData;
} RGXFWIF_POWER_REQUEST;

typedef struct
{
	PRGXFWIF_FWCOMMONCONTEXT psContext; /*!< Context to fence on (only useful when bDMContext == TRUE) */
	IMG_BOOL    bInval;                 /*!< Invalidate the cache as well as flushing */
	IMG_BOOL    bDMContext;             /*!< The data to flush/invalidate belongs to a specific DM context */
	IMG_UINT64	RGXFW_ALIGN ui64Address;	/*!< Optional address of range (only useful when bDMContext == FALSE) */
	IMG_UINT64	RGXFW_ALIGN ui64Size;		/*!< Optional size of range (only useful when bDMContext == FALSE) */
} RGXFWIF_SLCFLUSHINVALDATA;

typedef struct
{
	IMG_UINT32  ui32HCSDeadlineMS;  /* New number of milliseconds C/S is allowed to last */
} RGXFWIF_HCS_CTL;

typedef enum{
	RGXFWIF_HWPERF_CTRL_TOGGLE = 0,
	RGXFWIF_HWPERF_CTRL_SET    = 1,
	RGXFWIF_HWPERF_CTRL_EMIT_FEATURES_EV = 2
} RGXFWIF_HWPERF_UPDATE_CONFIG;

typedef struct
{
	RGXFWIF_HWPERF_UPDATE_CONFIG eOpCode; /*!< Control operation code */
	IMG_UINT64	RGXFW_ALIGN	ui64Mask;   /*!< Mask of events to toggle */
} RGXFWIF_HWPERF_CTRL;

typedef enum {
	RGXFWIF_HWPERF_CNTR_NOOP    = 0,   /* No-Op */
	RGXFWIF_HWPERF_CNTR_ENABLE  = 1,   /* Enable Counters */
	RGXFWIF_HWPERF_CNTR_DISABLE = 2    /* Disable Counters */
} RGXFWIF_HWPERF_CNTR_CONFIG;

typedef struct
{
	IMG_UINT32                ui32CtrlWord;
	IMG_UINT32                ui32NumBlocks;    /*!< Number of RGX_HWPERF_CONFIG_CNTBLK in the array */
	PRGX_HWPERF_CONFIG_CNTBLK sBlockConfigs;    /*!< Address of the RGX_HWPERF_CONFIG_CNTBLK array */
} RGXFWIF_HWPERF_CONFIG_ENABLE_BLKS;

typedef struct
{
	IMG_UINT32	ui32NewClockSpeed;			/*!< New clock speed */
} RGXFWIF_CORECLKSPEEDCHANGE_DATA;

#define RGXFWIF_HWPERF_CTRL_BLKS_MAX	16

typedef struct
{
	bool        bEnable;
	IMG_UINT32	ui32NumBlocks;                              /*!< Number of block IDs in the array */
	IMG_UINT16	aeBlockIDs[RGXFWIF_HWPERF_CTRL_BLKS_MAX];   /*!< Array of RGX_HWPERF_CNTBLK_ID values */
} RGXFWIF_HWPERF_CTRL_BLKS;

typedef struct
{
	RGXFWIF_DEV_VIRTADDR	sZSBufferFWDevVAddr;				/*!< ZS-Buffer FW address */
	IMG_BOOL				bDone;								/*!< action backing/unbacking succeeded */
} RGXFWIF_ZSBUFFER_BACKING_DATA;

#if defined(SUPPORT_VALIDATION)
typedef struct
{
	IMG_UINT32 ui32RegWidth;
	IMG_BOOL   bWriteOp;
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 RGXFW_ALIGN ui64RegVal;
} RGXFWIF_RGXREG_DATA;
#endif

typedef struct
{
	RGXFWIF_DEV_VIRTADDR	sFreeListFWDevVAddr;				/*!< Freelist FW address */
	IMG_UINT32				ui32DeltaPages;						/*!< Amount of the Freelist change */
	IMG_UINT32				ui32NewPages;						/*!< New amount of pages on the freelist (including ready pages) */
	IMG_UINT32              ui32ReadyPages;                     /*!< Number of ready pages to be held in reserve until OOM */
} RGXFWIF_FREELIST_GS_DATA;

#define RGXFWIF_MAX_FREELISTS_TO_RECONSTRUCT         (MAX_HW_TA3DCONTEXTS * RGXFW_MAX_FREELISTS * 2U)
#define RGXFWIF_FREELISTS_RECONSTRUCTION_FAILED_FLAG 0x80000000U

typedef struct
{
	IMG_UINT32			ui32FreelistsCount;
	IMG_UINT32			aui32FreelistIDs[RGXFWIF_MAX_FREELISTS_TO_RECONSTRUCT];
} RGXFWIF_FREELISTS_RECONSTRUCTION_DATA;


typedef struct
{
	IMG_DEV_VIRTADDR RGXFW_ALIGN       sDevSignalAddress; /*!< device virtual address of the updated signal */
	PRGXFWIF_FWMEMCONTEXT              psFWMemContext; /*!< Memory context */
} UNCACHED_ALIGN RGXFWIF_SIGNAL_UPDATE_DATA;


typedef struct
{
	PRGXFWIF_FWCOMMONCONTEXT  psContext; /*!< Context to that may need to be resumed following write offset update */
} UNCACHED_ALIGN RGXFWIF_WRITE_OFFSET_UPDATE_DATA;

/*!
 ******************************************************************************
 * Proactive DVFS Structures
 *****************************************************************************/
#define NUM_OPP_VALUES 16

typedef struct
{
	IMG_UINT32			ui32Volt; /* V  */
	IMG_UINT32			ui32Freq; /* Hz */
} UNCACHED_ALIGN PDVFS_OPP;

typedef struct
{
	PDVFS_OPP		asOPPValues[NUM_OPP_VALUES];
#if defined(DEBUG)
	IMG_UINT32		ui32MinOPPPoint;
#endif
	IMG_UINT32		ui32MaxOPPPoint;
} UNCACHED_ALIGN RGXFWIF_PDVFS_OPP;

typedef struct
{
	IMG_UINT32 ui32MaxOPPPoint;
} UNCACHED_ALIGN RGXFWIF_PDVFS_MAX_FREQ_DATA;

typedef struct
{
	IMG_UINT32 ui32MinOPPPoint;
} UNCACHED_ALIGN RGXFWIF_PDVFS_MIN_FREQ_DATA;

/*!
 ******************************************************************************
 * Register configuration structures
 *****************************************************************************/

#define RGXFWIF_REG_CFG_MAX_SIZE 512

typedef enum
{
	RGXFWIF_REGCFG_CMD_ADD				= 101,
	RGXFWIF_REGCFG_CMD_CLEAR			= 102,
	RGXFWIF_REGCFG_CMD_ENABLE			= 103,
	RGXFWIF_REGCFG_CMD_DISABLE			= 104
} RGXFWIF_REGDATA_CMD_TYPE;

typedef IMG_UINT32 RGXFWIF_REG_CFG_TYPE;
#define RGXFWIF_REG_CFG_TYPE_PWR_ON			0U	/* Sidekick power event */
#define RGXFWIF_REG_CFG_TYPE_DUST_CHANGE	1U	/* Rascal / dust power event */
#define RGXFWIF_REG_CFG_TYPE_TA				2U	/* TA kick */
#define RGXFWIF_REG_CFG_TYPE_3D				3U	/* 3D kick */
#define RGXFWIF_REG_CFG_TYPE_CDM			4U	/* Compute kick */
#define RGXFWIF_REG_CFG_TYPE_TDM			5U	/* TDM kick */
#define RGXFWIF_REG_CFG_TYPE_ALL			6U	/* Applies to all types. Keep as last element */

typedef struct
{
	IMG_UINT64		ui64Addr;
	IMG_UINT64		ui64Mask;
	IMG_UINT64		ui64Value;
} RGXFWIF_REG_CFG_REC;

typedef struct
{
	RGXFWIF_REGDATA_CMD_TYPE         eCmdType;
	RGXFWIF_REG_CFG_TYPE             eRegConfigType;
	RGXFWIF_REG_CFG_REC RGXFW_ALIGN  sRegConfig;

} RGXFWIF_REGCONFIG_DATA;

typedef struct
{
	/**
	 * PDump WRW command write granularity is 32 bits.
	 * Add padding to ensure array size is 32 bit granular.
	 */
	IMG_UINT8           RGXFW_ALIGN  aui8NumRegsType[PVR_ALIGN(RGXFWIF_REG_CFG_TYPE_ALL,sizeof(IMG_UINT32))];
	RGXFWIF_REG_CFG_REC RGXFW_ALIGN  asRegConfigs[RGXFWIF_REG_CFG_MAX_SIZE];
} UNCACHED_ALIGN RGXFWIF_REG_CFG;

/* OSid Scheduling Priority Change */
typedef struct
{
	IMG_UINT32			ui32OSidNum;
	IMG_UINT32			ui32Priority;
} RGXFWIF_OSID_PRIORITY_DATA;

typedef enum
{
	RGXFWIF_OS_ONLINE = 1,
	RGXFWIF_OS_OFFLINE
} RGXFWIF_OS_STATE_CHANGE;

typedef struct
{
	IMG_UINT32 ui32OSid;
	RGXFWIF_OS_STATE_CHANGE eNewOSState;
} UNCACHED_ALIGN RGXFWIF_OS_STATE_CHANGE_DATA;

typedef enum
{
	/* Common commands */
	RGXFWIF_KCCB_CMD_KICK								= 101U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	RGXFWIF_KCCB_CMD_MMUCACHE							= 102U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	RGXFWIF_KCCB_CMD_BP									= 103U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	RGXFWIF_KCCB_CMD_SLCFLUSHINVAL						= 105U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< SLC flush and invalidation request */
	RGXFWIF_KCCB_CMD_CLEANUP							= 106U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Requests cleanup of a FW resource (type specified in the command data) */
	RGXFWIF_KCCB_CMD_POW								= 107U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Power request */
	RGXFWIF_KCCB_CMD_ZSBUFFER_BACKING_UPDATE			= 108U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Backing for on-demand ZS-Buffer done */
	RGXFWIF_KCCB_CMD_ZSBUFFER_UNBACKING_UPDATE			= 109U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Unbacking for on-demand ZS-Buffer done */
	RGXFWIF_KCCB_CMD_FREELIST_GROW_UPDATE				= 110U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Freelist Grow done */
	RGXFWIF_KCCB_CMD_FREELISTS_RECONSTRUCTION_UPDATE	= 112U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Freelists Reconstruction done */
	RGXFWIF_KCCB_CMD_NOTIFY_SIGNAL_UPDATE				= 113U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Informs the firmware that the host has performed a signal update */
	RGXFWIF_KCCB_CMD_NOTIFY_WRITE_OFFSET_UPDATE			= 114U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Informs the firmware that the host has added more data to a CDM2 Circular Buffer */
	RGXFWIF_KCCB_CMD_HEALTH_CHECK						= 115U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Health check request */
	RGXFWIF_KCCB_CMD_FORCE_UPDATE						= 116U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Forcing signalling of all unmet UFOs for a given CCB offset */

	RGXFWIF_KCCB_CMD_COMBINED_TA_3D_KICK				= 117U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< There is a TA and a 3D command in this single kick */

	/* Commands only permitted to the native or host OS */
	RGXFWIF_KCCB_CMD_REGCONFIG							= 200U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	RGXFWIF_KCCB_CMD_HWPERF_UPDATE_CONFIG              = 201U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Configure HWPerf events (to be generated) and HWPerf buffer address (if required) */
	RGXFWIF_KCCB_CMD_HWPERF_CONFIG_BLKS                = 202U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Configure, clear and enable multiple HWPerf blocks */
	RGXFWIF_KCCB_CMD_HWPERF_CTRL_BLKS                  = 203U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Enable or disable multiple HWPerf blocks (reusing existing configuration) */
	RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE                = 204U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Core clock speed change event */
	RGXFWIF_KCCB_CMD_HWPERF_CONFIG_ENABLE_BLKS_DIRECT  = 205U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Configure, clear and enable multiple HWPerf blocks during the init process */
	RGXFWIF_KCCB_CMD_LOGTYPE_UPDATE                    = 206U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Ask the firmware to update its cached ui32LogType value from the (shared) tracebuf control structure */
	RGXFWIF_KCCB_CMD_PDVFS_LIMIT_MAX_FREQ              = 207U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	RGXFWIF_KCCB_CMD_OSID_PRIORITY_CHANGE              = 208U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Changes the relative scheduling priority for a particular OSID. It can only be serviced for the Host DDK */
	RGXFWIF_KCCB_CMD_STATEFLAGS_CTRL                   = 209U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Set or clear firmware state flags */
	RGXFWIF_KCCB_CMD_HCS_SET_DEADLINE                  = 210U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Set hard context switching deadline */
	RGXFWIF_KCCB_CMD_OS_ONLINE_STATE_CONFIGURE         = 211U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Informs the FW that a Guest OS has come online / offline. It can only be serviced for the Host DDK */
	RGXFWIF_KCCB_CMD_PDVFS_LIMIT_MIN_FREQ              = 212U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Set a minimum frequency/OPP point */
	RGXFWIF_KCCB_CMD_PHR_CFG                           = 213U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Configure Periodic Hardware Reset behaviour */
#if defined(SUPPORT_VALIDATION)
	RGXFWIF_KCCB_CMD_RGXREG                            = 214U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Read RGX Register from FW */
#endif
	RGXFWIF_KCCB_CMD_WDG_CFG							= 215U | RGX_CMD_MAGIC_DWORD_SHIFTED, /*!< Configure Safety Firmware Watchdog */
} RGXFWIF_KCCB_CMD_TYPE;

#define RGXFWIF_LAST_ALLOWED_GUEST_KCCB_CMD (RGXFWIF_KCCB_CMD_REGCONFIG - 1)

/* Kernel CCB command packet */
typedef struct
{
	RGXFWIF_KCCB_CMD_TYPE  eCmdType;      /*!< Command type */
	IMG_UINT32             ui32KCCBFlags; /*!< Compatibility and other flags */

	/* NOTE: Make sure that uCmdData is the last member of this struct
	 * This is to calculate actual command size for device mem copy.
	 * (Refer RGXGetCmdMemCopySize())
	 * */
	union
	{
		RGXFWIF_KCCB_CMD_KICK_DATA			sCmdKickData;			/*!< Data for Kick command */
		RGXFWIF_KCCB_CMD_COMBINED_TA_3D_KICK_DATA	sCombinedTA3DCmdKickData;	/*!< Data for combined TA/3D Kick command */
		RGXFWIF_MMUCACHEDATA				sMMUCacheData;			/*!< Data for MMU cache command */
		RGXFWIF_BPDATA						sBPData;				/*!< Data for Breakpoint Commands */
		RGXFWIF_SLCFLUSHINVALDATA			sSLCFlushInvalData;		/*!< Data for SLC Flush/Inval commands */
		RGXFWIF_CLEANUP_REQUEST				sCleanupData;			/*!< Data for cleanup commands */
		RGXFWIF_POWER_REQUEST				sPowData;				/*!< Data for power request commands */
		RGXFWIF_HWPERF_CTRL					sHWPerfCtrl;			/*!< Data for HWPerf control command */
		RGXFWIF_HWPERF_CONFIG_ENABLE_BLKS	sHWPerfCfgEnableBlks;	/*!< Data for HWPerf configure, clear and enable performance counter block command */
		RGXFWIF_HWPERF_CTRL_BLKS			sHWPerfCtrlBlks;		/*!< Data for HWPerf enable or disable performance counter block commands */
		RGXFWIF_CORECLKSPEEDCHANGE_DATA		sCoreClkSpeedChangeData;/*!< Data for core clock speed change */
		RGXFWIF_ZSBUFFER_BACKING_DATA		sZSBufferBackingData;	/*!< Feedback for Z/S Buffer backing/unbacking */
		RGXFWIF_FREELIST_GS_DATA			sFreeListGSData;		/*!< Feedback for Freelist grow/shrink */
		RGXFWIF_FREELISTS_RECONSTRUCTION_DATA	sFreeListsReconstructionData;	/*!< Feedback for Freelists reconstruction */
		RGXFWIF_REGCONFIG_DATA				sRegConfigData;			/*!< Data for custom register configuration */
		RGXFWIF_SIGNAL_UPDATE_DATA          sSignalUpdateData;      /*!< Data for informing the FW about the signal update */
		RGXFWIF_WRITE_OFFSET_UPDATE_DATA    sWriteOffsetUpdateData; /*!< Data for informing the FW about the write offset update */
#if defined(SUPPORT_PDVFS)
		RGXFWIF_PDVFS_MAX_FREQ_DATA			sPDVFSMaxFreqData;
		RGXFWIF_PDVFS_MIN_FREQ_DATA			sPDVFSMinFreqData;		/*!< Data for setting the min frequency/OPP */
#endif
		RGXFWIF_OSID_PRIORITY_DATA			sCmdOSidPriorityData;	/*!< Data for updating an OSid priority */
		RGXFWIF_HCS_CTL						sHCSCtrl;				/*!< Data for Hard Context Switching */
		RGXFWIF_OS_STATE_CHANGE_DATA        sCmdOSOnlineStateData;  /*!< Data for updating the Guest Online states */
		RGXFWIF_DEV_VIRTADDR                sTBIBuffer;             /*!< Dev address for TBI buffer allocated on demand */
		RGXFWIF_KCCB_CMD_FORCE_UPDATE_DATA  sForceUpdateData;       /*!< Data for signalling all unmet fences for a given CCB */
		RGXFWIF_KCCB_CMD_PHR_CFG_DATA       sPeriodicHwResetCfg;    /*!< Data for configuring the Periodic Hw Reset behaviour */
#if defined(SUPPORT_VALIDATION)
		RGXFWIF_RGXREG_DATA                 sFwRgxData;             /*!< Data for reading off an RGX register */
#endif
		RGXFWIF_KCCB_CMD_WDG_CFG_DATA       sSafetyWatchDogCfg;     /*!< Data for configuring the Safety Firmware Watchdog */
	} UNCACHED_ALIGN uCmdData;
} UNCACHED_ALIGN RGXFWIF_KCCB_CMD;

RGX_FW_STRUCT_SIZE_ASSERT(RGXFWIF_KCCB_CMD);

/*!
 ******************************************************************************
 * Firmware CCB command structure for RGX
 *****************************************************************************/

typedef struct
{
	IMG_UINT32				ui32ZSBufferID;
} RGXFWIF_FWCCB_CMD_ZSBUFFER_BACKING_DATA;

typedef struct
{
	IMG_UINT32				ui32FreelistID;
} RGXFWIF_FWCCB_CMD_FREELIST_GS_DATA;

typedef struct
{
	IMG_UINT32			ui32FreelistsCount;
	IMG_UINT32			ui32HwrCounter;
	IMG_UINT32			aui32FreelistIDs[RGXFWIF_MAX_FREELISTS_TO_RECONSTRUCT];
} RGXFWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION_DATA;

#define RGXFWIF_FWCCB_CMD_CONTEXT_RESET_FLAG_PF			(1U<<0)	/*!< 1 if a page fault happened */
#define RGXFWIF_FWCCB_CMD_CONTEXT_RESET_FLAG_ALL_CTXS	(1U<<1)	/*!< 1 if applicable to all contexts */

typedef struct
{
	IMG_UINT32						ui32ServerCommonContextID;	/*!< Context affected by the reset */
	RGX_CONTEXT_RESET_REASON		eResetReason;				/*!< Reason for reset */
	RGXFWIF_DM						eDM;						/*!< Data Master affected by the reset */
	IMG_UINT32						ui32ResetJobRef;			/*!< Job ref running at the time of reset */
	IMG_UINT32						ui32Flags;					/*!< RGXFWIF_FWCCB_CMD_CONTEXT_RESET_FLAG bitfield  */
	IMG_UINT64 RGXFW_ALIGN			ui64PCAddress;				/*!< At what page catalog address */
	IMG_DEV_VIRTADDR RGXFW_ALIGN	sFaultAddress;				/*!< Page fault address (only when applicable) */
} RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA;

typedef struct
{
	IMG_DEV_VIRTADDR sFWFaultAddr;	/*!< Page fault address */
} RGXFWIF_FWCCB_CMD_FW_PAGEFAULT_DATA;

typedef enum
{
	RGXFWIF_FWCCB_CMD_ZSBUFFER_BACKING				= 101U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests ZSBuffer to be backed with physical pages */
	RGXFWIF_FWCCB_CMD_ZSBUFFER_UNBACKING			= 102U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests ZSBuffer to be unbacked */
	RGXFWIF_FWCCB_CMD_FREELIST_GROW					= 103U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests an on-demand freelist grow/shrink */
	RGXFWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION		= 104U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests freelists reconstruction */
	RGXFWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION	= 105U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Notifies host of a HWR event on a context */
	RGXFWIF_FWCCB_CMD_DEBUG_DUMP					= 106U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests an on-demand debug dump */
	RGXFWIF_FWCCB_CMD_UPDATE_STATS					= 107U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Requests an on-demand update on process stats */
    /* Unused FWCCB CMD ID 108 */
    /* Unused FWCCB CMD ID 109 */
#if defined(SUPPORT_PDVFS)
	RGXFWIF_FWCCB_CMD_CORE_CLK_RATE_CHANGE			= 110U | RGX_CMD_MAGIC_DWORD_SHIFTED,
	/* Unused FWCCB CMD ID 111 */
#endif
	RGXFWIF_FWCCB_CMD_REQUEST_GPU_RESTART			= 112U | RGX_CMD_MAGIC_DWORD_SHIFTED,
#if defined(SUPPORT_VALIDATION)
	RGXFWIF_FWCCB_CMD_REG_READ					= 113U | RGX_CMD_MAGIC_DWORD_SHIFTED,
#if defined(SUPPORT_SOC_TIMER)
	RGXFWIF_FWCCB_CMD_SAMPLE_TIMERS				= 114U | RGX_CMD_MAGIC_DWORD_SHIFTED,
#endif
#endif
	RGXFWIF_FWCCB_CMD_CONTEXT_FW_PF_NOTIFICATION    = 115U | RGX_CMD_MAGIC_DWORD_SHIFTED,	/*!< Notifies host of a FW pagefault */
} RGXFWIF_FWCCB_CMD_TYPE;

typedef enum
{
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_PARTIAL_RENDERS=1,		/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32TotalNumPartialRenders stat */
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_OUT_OF_MEMORY,			/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32TotalNumOutOfMemory stat */
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_TA_STORES,				/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32NumTAStores stat */
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_3D_STORES,				/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32Num3DStores stat */
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_CDM_STORES,			/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32NumCDMStores stat */
	RGXFWIF_FWCCB_CMD_UPDATE_NUM_TDM_STORES				/*!< PVRSRVStatsUpdateRenderContextStats should increase the value of the ui32NumTDMStores stat */
} RGXFWIF_FWCCB_CMD_UPDATE_STATS_TYPE;

typedef struct
{
	RGXFWIF_FWCCB_CMD_UPDATE_STATS_TYPE		eElementToUpdate;			/*!< Element to update */
	IMG_PID									pidOwner;					/*!< The pid of the process whose stats are being updated */
	IMG_INT32								i32AdjustmentValue;			/*!< Adjustment to be made to the statistic */
} RGXFWIF_FWCCB_CMD_UPDATE_STATS_DATA;

typedef struct
{
	IMG_UINT32 ui32CoreClkRate;
} UNCACHED_ALIGN RGXFWIF_FWCCB_CMD_CORE_CLK_RATE_CHANGE_DATA;

#if defined(SUPPORT_VALIDATION)
typedef struct
{
	IMG_UINT64 ui64RegValue;
} RGXFWIF_FWCCB_CMD_RGXREG_READ_DATA;

#if defined(SUPPORT_SOC_TIMER)
typedef struct
{
	IMG_UINT64 ui64timerGray;
	IMG_UINT64 ui64timerBinary;
	IMG_UINT64 aui64uscTimers[RGX_FEATURE_NUM_CLUSTERS];
}  RGXFWIF_FWCCB_CMD_SAMPLE_TIMERS_DATA;
#endif
#endif

typedef struct
{
	RGXFWIF_FWCCB_CMD_TYPE  eCmdType;       /*!< Command type */
	IMG_UINT32              ui32FWCCBFlags; /*!< Compatibility and other flags */

	union
	{
		RGXFWIF_FWCCB_CMD_ZSBUFFER_BACKING_DATA				sCmdZSBufferBacking;			/*!< Data for Z/S-Buffer on-demand (un)backing*/
		RGXFWIF_FWCCB_CMD_FREELIST_GS_DATA					sCmdFreeListGS;					/*!< Data for on-demand freelist grow/shrink */
		RGXFWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION_DATA		sCmdFreeListsReconstruction;	/*!< Data for freelists reconstruction */
		RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA				sCmdContextResetNotification;	/*!< Data for context reset notification */
		RGXFWIF_FWCCB_CMD_UPDATE_STATS_DATA					sCmdUpdateStatsData;			/*!< Data for updating process stats */
		RGXFWIF_FWCCB_CMD_CORE_CLK_RATE_CHANGE_DATA			sCmdCoreClkRateChange;
		RGXFWIF_FWCCB_CMD_FW_PAGEFAULT_DATA					sCmdFWPagefault;				/*!< Data for context reset notification */
#if defined(SUPPORT_VALIDATION)
		RGXFWIF_FWCCB_CMD_RGXREG_READ_DATA					sCmdRgxRegReadData;
#if defined(SUPPORT_SOC_TIMER)
		RGXFWIF_FWCCB_CMD_SAMPLE_TIMERS_DATA				sCmdTimers;
#endif
#endif
	} RGXFW_ALIGN uCmdData;
} RGXFW_ALIGN RGXFWIF_FWCCB_CMD;

RGX_FW_STRUCT_SIZE_ASSERT(RGXFWIF_FWCCB_CMD);


/*!
 ******************************************************************************
 * Workload estimation Firmware CCB command structure for RGX
 *****************************************************************************/
typedef struct
{
	IMG_UINT64 RGXFW_ALIGN ui64ReturnDataIndex; /*!< Index for return data array */
	IMG_UINT64 RGXFW_ALIGN ui64CyclesTaken;     /*!< The cycles the workload took on the hardware */
} RGXFWIF_WORKEST_FWCCB_CMD;


/*!
 ******************************************************************************
 * Client CCB commands for RGX
 *****************************************************************************/

/* Required memory alignment for 64-bit variables accessible by Meta
  (The gcc meta aligns 64-bit variables to 64-bit; therefore, memory shared
   between the host and meta that contains 64-bit variables has to maintain
   this alignment) */
#define RGXFWIF_FWALLOC_ALIGN	sizeof(IMG_UINT64)

#define RGX_CCB_TYPE_TASK			(IMG_UINT32_C(1) << 15)
#define RGX_CCB_FWALLOC_ALIGN(size)	(((size) + (RGXFWIF_FWALLOC_ALIGN-1)) & ~(RGXFWIF_FWALLOC_ALIGN - 1))

typedef IMG_UINT32 RGXFWIF_CCB_CMD_TYPE;

#define RGXFWIF_CCB_CMD_TYPE_GEOM			(201U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_TQ_3D			(202U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_3D				(203U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_3D_PR			(204U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_CDM			(205U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_TQ_TDM			(206U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_FBSC_INVALIDATE (207U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_TQ_2D			(208U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_PRE_TIMESTAMP	(209U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)
#define RGXFWIF_CCB_CMD_TYPE_NULL			(210U | RGX_CMD_MAGIC_DWORD_SHIFTED | RGX_CCB_TYPE_TASK)

/* Leave a gap between CCB specific commands and generic commands */
#define RGXFWIF_CCB_CMD_TYPE_FENCE          (211U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_UPDATE         (212U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_RMW_UPDATE     (213U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_FENCE_PR       (214U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_PRIORITY       (215U | RGX_CMD_MAGIC_DWORD_SHIFTED)
/* Pre and Post timestamp commands are supposed to sandwich the DM cmd. The
   padding code with the CCB wrap upsets the FW if we don't have the task type
   bit cleared for POST_TIMESTAMPs. That's why we have 2 different cmd types.
*/
#define RGXFWIF_CCB_CMD_TYPE_POST_TIMESTAMP (216U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_UNFENCED_UPDATE (217U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#define RGXFWIF_CCB_CMD_TYPE_UNFENCED_RMW_UPDATE (218U | RGX_CMD_MAGIC_DWORD_SHIFTED)

#if defined(SUPPORT_VALIDATION)
#define RGXFWIF_CCB_CMD_TYPE_REG_READ (219U | RGX_CMD_MAGIC_DWORD_SHIFTED)
#endif

#define RGXFWIF_CCB_CMD_TYPE_PADDING	(220U | RGX_CMD_MAGIC_DWORD_SHIFTED)

typedef struct
{
	IMG_UINT32				ui32RenderTargetSize;
	IMG_UINT32				ui32NumberOfDrawCalls;
	IMG_UINT32				ui32NumberOfIndices;
	IMG_UINT32				ui32NumberOfMRTs;
} RGXFWIF_WORKLOAD_TA3D;

typedef union
{
	RGXFWIF_WORKLOAD_TA3D	sTA3D;
	/* Created as a union to facilitate other DMs
	 * in the future.
	 * for Example:
	 * RGXFWIF_WORKLOAD_CDM     sCDM;
	 */
}RGXFWIF_WORKLOAD;

typedef struct
{
	/* Index for the KM Workload estimation return data array */
	IMG_UINT64 RGXFW_ALIGN                    ui64ReturnDataIndex;
	/* Deadline for the workload */
	IMG_UINT64 RGXFW_ALIGN                    ui64Deadline;
	/* Predicted time taken to do the work in cycles */
	IMG_UINT64 RGXFW_ALIGN                    ui64CyclesPrediction;
} RGXFWIF_WORKEST_KICK_DATA;

#if defined(SUPPORT_PDVFS)
typedef struct _RGXFWIF_WORKLOAD_LIST_NODE_ RGXFWIF_WORKLOAD_LIST_NODE;
typedef struct _RGXFWIF_DEADLINE_LIST_NODE_ RGXFWIF_DEADLINE_LIST_NODE;

struct _RGXFWIF_WORKLOAD_LIST_NODE_
{
	IMG_UINT64 RGXFW_ALIGN ui64Cycles;
	IMG_UINT64 RGXFW_ALIGN ui64SelfMemDesc;
	IMG_UINT64 RGXFW_ALIGN ui64WorkloadDataMemDesc;
	IMG_BOOL					bReleased;
	RGXFWIF_WORKLOAD_LIST_NODE *psNextNode;
};

struct _RGXFWIF_DEADLINE_LIST_NODE_
{
	IMG_UINT64 RGXFW_ALIGN ui64Deadline;
	RGXFWIF_WORKLOAD_LIST_NODE *psWorkloadList;
	IMG_UINT64 RGXFW_ALIGN ui64SelfMemDesc;
	IMG_UINT64 RGXFW_ALIGN ui64WorkloadDataMemDesc;
	IMG_BOOL					bReleased;
	RGXFWIF_DEADLINE_LIST_NODE *psNextNode;
};
#endif
typedef struct
{
	RGXFWIF_CCB_CMD_TYPE				eCmdType;
	IMG_UINT32							ui32CmdSize;
	IMG_UINT32							ui32ExtJobRef; /*!< external job reference - provided by client and used in debug for tracking submitted work */
	IMG_UINT32							ui32IntJobRef; /*!< internal job reference - generated by services and used in debug for tracking submitted work */
	RGXFWIF_WORKEST_KICK_DATA RGXFW_ALIGN		sWorkEstKickData; /*!< Workload Estimation - Workload Estimation Data */
} RGXFWIF_CCB_CMD_HEADER;

/*!
 ******************************************************************************
 * Client CCB commands which are only required by the kernel
 *****************************************************************************/
typedef struct
{
	IMG_UINT32             ui32Priority;
} RGXFWIF_CMD_PRIORITY;


/*!
 ******************************************************************************
 * Signature and Checksums Buffer
 *****************************************************************************/
typedef struct
{
	PRGXFWIF_SIGBUFFER		sBuffer;			/*!< Ptr to Signature Buffer memory */
	IMG_UINT32				ui32LeftSizeInRegs;	/*!< Amount of space left for storing regs in the buffer */
} UNCACHED_ALIGN RGXFWIF_SIGBUF_CTL;

typedef struct
{
	PRGXFWIF_FIRMWAREGCOVBUFFER	sBuffer;		/*!< Ptr to firmware gcov buffer */
	IMG_UINT32					ui32Size;		/*!< Amount of space for storing in the buffer */
} UNCACHED_ALIGN RGXFWIF_FIRMWARE_GCOV_CTL;

/*!
 *****************************************************************************
 * RGX Compatibility checks
 *****************************************************************************/

/* WARNING: Whenever the layout of RGXFWIF_COMPCHECKS_BVNC is a subject of change,
	following define should be increased by 1 to indicate to compatibility logic,
	that layout has changed */
#define RGXFWIF_COMPCHECKS_LAYOUT_VERSION 3

typedef struct
{
	IMG_UINT32	ui32LayoutVersion; /* WARNING: This field must be defined as first one in this structure */
	IMG_UINT64	RGXFW_ALIGN ui64BVNC;
} UNCACHED_ALIGN RGXFWIF_COMPCHECKS_BVNC;

typedef struct
{
	IMG_UINT8	ui8OsCountSupport;
} UNCACHED_ALIGN RGXFWIF_INIT_OPTIONS;

#define RGXFWIF_COMPCHECKS_BVNC_DECLARE_AND_INIT(name) \
	RGXFWIF_COMPCHECKS_BVNC name = { \
		RGXFWIF_COMPCHECKS_LAYOUT_VERSION, \
		0, \
	}
#define RGXFWIF_COMPCHECKS_BVNC_INIT(name) \
	do { \
		(name).ui32LayoutVersion = RGXFWIF_COMPCHECKS_LAYOUT_VERSION; \
		(name).ui64BVNC = 0; \
	} while (0)

typedef struct
{
	RGXFWIF_COMPCHECKS_BVNC		sHWBVNC;				/*!< hardware BVNC (from the RGX registers) */
	RGXFWIF_COMPCHECKS_BVNC		sFWBVNC;				/*!< firmware BVNC */
	IMG_UINT32					ui32FWProcessorVersion;	/*!< identifier of the FW processor version */
	IMG_UINT32					ui32DDKVersion;			/*!< software DDK version */
	IMG_UINT32					ui32DDKBuild;			/*!< software DDK build no. */
	IMG_UINT32					ui32BuildOptions;		/*!< build options bit-field */
	RGXFWIF_INIT_OPTIONS		sInitOptions;			/*!< initialisation options bit-field */
	IMG_BOOL					bUpdated;				/*!< Information is valid */
} UNCACHED_ALIGN RGXFWIF_COMPCHECKS;

/*!
 ******************************************************************************
 * Updated configuration post FW data init.
 *****************************************************************************/
typedef struct
{
	IMG_UINT32         ui32ActivePMLatencyms;      /* APM latency in ms before signalling IDLE to the host */
	IMG_UINT32         ui32RuntimeCfgFlags;        /* Compatibility and other flags */
	IMG_BOOL           bActivePMLatencyPersistant; /* If set, APM latency does not reset to system default each GPU power transition */
	IMG_UINT32         ui32CoreClockSpeed;         /* Core clock speed, currently only used to calculate timer ticks */
	IMG_UINT32         ui32PowUnitsStateMask;      /* Power Unit state mask set by the host */
	PRGXFWIF_HWPERFBUF sHWPerfBuf;                 /* On-demand allocated HWPerf buffer address, to be passed to the FW */
	RGXFWIF_DMA_ADDR   sHWPerfDMABuf;
} RGXFWIF_RUNTIME_CFG;

/*!
 *****************************************************************************
 * Control data for RGX
 *****************************************************************************/

#define RGXFWIF_HWR_DEBUG_DUMP_ALL (99999U)

#if defined(PDUMP)

#define RGXFWIF_PID_FILTER_MAX_NUM_PIDS 32U

typedef enum
{
	RGXFW_PID_FILTER_INCLUDE_ALL_EXCEPT,
	RGXFW_PID_FILTER_EXCLUDE_ALL_EXCEPT
} RGXFWIF_PID_FILTER_MODE;

typedef struct
{
	IMG_PID uiPID;
	IMG_UINT32 ui32OSID;
} RGXFW_ALIGN RGXFWIF_PID_FILTER_ITEM;

typedef struct
{
	RGXFWIF_PID_FILTER_MODE eMode;
	/* each process in the filter list is specified by a PID and OS ID pair.
	 * each PID and OS pair is an item in the items array (asItems).
	 * if the array contains less than RGXFWIF_PID_FILTER_MAX_NUM_PIDS entries
	 * then it must be terminated by an item with pid of zero.
	 */
	RGXFWIF_PID_FILTER_ITEM asItems[RGXFWIF_PID_FILTER_MAX_NUM_PIDS];
} RGXFW_ALIGN RGXFWIF_PID_FILTER;
#endif

#if defined(SUPPORT_SECURITY_VALIDATION)
#define RGXFWIF_SECURE_ACCESS_TEST_READ_WRITE_FW_DATA  (0x1U << 0)
#define RGXFWIF_SECURE_ACCESS_TEST_READ_WRITE_FW_CODE  (0x1U << 1)
#define RGXFWIF_SECURE_ACCESS_TEST_RUN_FROM_NONSECURE  (0x1U << 2)
#define RGXFWIF_SECURE_ACCESS_TEST_RUN_FROM_SECURE     (0x1U << 3)
#endif

typedef enum
{
	RGXFWIF_USRM_DM_VDM = 0,
	RGXFWIF_USRM_DM_DDM = 1,
	RGXFWIF_USRM_DM_CDM = 2,
	RGXFWIF_USRM_DM_PDM = 3,
	RGXFWIF_USRM_DM_TDM = 4,
	RGXFWIF_USRM_DM_LAST
} RGXFWIF_USRM_DM;

typedef enum
{
	RGXFWIF_UVBRM_DM_VDM = 0,
	RGXFWIF_UVBRM_DM_DDM = 1,
	RGXFWIF_UVBRM_DM_LAST
} RGXFWIF_UVBRM_DM;

typedef enum
{
	RGXFWIF_TPU_DM_PDM = 0,
	RGXFWIF_TPU_DM_VDM = 1,
	RGXFWIF_TPU_DM_CDM = 2,
	RGXFWIF_TPU_DM_TDM = 3,
	RGXFWIF_TPU_DM_LAST
} RGXFWIF_TPU_DM;

typedef enum
{
	RGXFWIF_GPIO_VAL_OFF           = 0, /*!< No GPIO validation */
	RGXFWIF_GPIO_VAL_GENERAL       = 1, /*!< Simple test case that
	                                         initiates by sending data via the
	                                         GPIO and then sends back any data
	                                         received over the GPIO */
	RGXFWIF_GPIO_VAL_AP            = 2, /*!< More complex test case that writes
	                                         and reads data across the entire
	                                         GPIO AP address range.*/
#if defined(SUPPORT_STRIP_RENDERING)
	RGXFWIF_GPIO_VAL_SR_BASIC      = 3, /*!< Strip Rendering AP based basic test.*/
	RGXFWIF_GPIO_VAL_SR_COMPLEX    = 4, /*!< Strip Rendering AP based complex test.*/
#endif
	RGXFWIF_GPIO_VAL_TESTBENCH     = 5, /*!< Validates the GPIO Testbench. */
	RGXFWIF_GPIO_VAL_LAST
} RGXFWIF_GPIO_VAL_MODE;

typedef enum
{
	FW_PERF_CONF_NONE = 0,
	FW_PERF_CONF_ICACHE = 1,
	FW_PERF_CONF_DCACHE = 2,
	FW_PERF_CONF_POLLS = 3,
	FW_PERF_CONF_CUSTOM_TIMER = 4,
	FW_PERF_CONF_JTLB_INSTR = 5,
	FW_PERF_CONF_INSTRUCTIONS = 6
} FW_PERF_CONF;

typedef enum
{
	FW_BOOT_STAGE_TLB_INIT_FAILURE = -2,
	FW_BOOT_STAGE_NOT_AVAILABLE = -1,
	FW_BOOT_NOT_STARTED = 0,
	FW_BOOT_BLDR_STARTED = 1,
	FW_BOOT_CACHE_DONE,
	FW_BOOT_TLB_DONE,
	FW_BOOT_MAIN_STARTED,
	FW_BOOT_ALIGNCHECKS_DONE,
	FW_BOOT_INIT_DONE,
} FW_BOOT_STAGE;

/*
 * Kernel CCB return slot responses. Usage of bit-fields instead of bare integers
 * allows FW to possibly pack-in several responses for each single kCCB command.
 */
#define RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED   (1U << 0) /* Command executed (return status from FW) */
#define RGXFWIF_KCCB_RTN_SLOT_CLEANUP_BUSY   (1U << 1) /* A cleanup was requested but resource busy */
#define RGXFWIF_KCCB_RTN_SLOT_POLL_FAILURE   (1U << 2) /* Poll failed in FW for a HW operation to complete */

#define RGXFWIF_KCCB_RTN_SLOT_NO_RESPONSE            0x0U      /* Reset value of a kCCB return slot (set by host) */

typedef struct
{
	/* Fw-Os connection states */
	volatile RGXFWIF_CONNECTION_FW_STATE eConnectionFwState;
	volatile RGXFWIF_CONNECTION_OS_STATE eConnectionOsState;
	volatile IMG_UINT32                  ui32AliveFwToken;
	volatile IMG_UINT32                  ui32AliveOsToken;
} UNCACHED_ALIGN RGXFWIF_CONNECTION_CTL;

typedef struct
{
	/* Kernel CCB */
	PRGXFWIF_CCB_CTL        psKernelCCBCtl;
	PRGXFWIF_CCB            psKernelCCB;
	PRGXFWIF_CCB_RTN_SLOTS  psKernelCCBRtnSlots;

	/* Firmware CCB */
	PRGXFWIF_CCB_CTL        psFirmwareCCBCtl;
	PRGXFWIF_CCB            psFirmwareCCB;

	/* Workload Estimation Firmware CCB */
	PRGXFWIF_CCB_CTL        RGXFW_ALIGN psWorkEstFirmwareCCBCtl;
	PRGXFWIF_CCB            RGXFW_ALIGN psWorkEstFirmwareCCB;

	PRGXFWIF_HWRINFOBUF     sRGXFWIfHWRInfoBufCtl;

	IMG_UINT32              ui32HWRDebugDumpLimit;

	PRGXFWIF_OSDATA         sFwOsData;

	/* Compatibility checks to be populated by the Firmware */
	RGXFWIF_COMPCHECKS      sRGXCompChecks;

} UNCACHED_ALIGN RGXFWIF_OSINIT;

typedef struct
{
	IMG_DEV_PHYADDR         RGXFW_ALIGN sFaultPhysAddr;

	IMG_DEV_VIRTADDR        RGXFW_ALIGN sPDSExecBase;
	IMG_DEV_VIRTADDR        RGXFW_ALIGN sUSCExecBase;
	IMG_DEV_VIRTADDR        RGXFW_ALIGN sFBCDCStateTableBase;
	IMG_DEV_VIRTADDR        RGXFW_ALIGN sFBCDCLargeStateTableBase;
	IMG_DEV_VIRTADDR        RGXFW_ALIGN sTextureHeapBase;
	IMG_DEV_VIRTADDR        RGXFW_ALIGN sPDSIndirectHeapBase;

	IMG_UINT32              ui32FilterFlags;

	RGXFWIF_SIGBUF_CTL      asSigBufCtl[RGXFWIF_DM_DEFAULT_MAX];
#if defined(SUPPORT_VALIDATION)
	RGXFWIF_SIGBUF_CTL      asValidationSigBufCtl[RGXFWIF_DM_DEFAULT_MAX];
#endif

	PRGXFWIF_RUNTIME_CFG    sRuntimeCfg;

	PRGXFWIF_TRACEBUF       sTraceBufCtl;
	PRGXFWIF_SYSDATA        sFwSysData;
#if defined(SUPPORT_TBI_INTERFACE)
	PRGXFWIF_TBIBUF         sTBIBuf;
#endif
	IMG_UINT64              RGXFW_ALIGN ui64HWPerfFilter;

	PRGXFWIF_GPU_UTIL_FWCB  sGpuUtilFWCbCtl;
	PRGXFWIF_REG_CFG        sRegCfg;
	PRGXFWIF_HWPERF_CTL     sHWPerfCtl;

	RGXFWIF_FIRMWARE_GCOV_CTL sFirmwareGcovCtl;

	RGXFWIF_DEV_VIRTADDR    sAlignChecks;

	/* Core clock speed at FW boot time */
	IMG_UINT32              ui32InitialCoreClockSpeed;

	/* APM latency in ms before signalling IDLE to the host */
	IMG_UINT32              ui32ActivePMLatencyms;

	/* Flag to be set by the Firmware after successful start */
	IMG_BOOL                bFirmwareStarted;

	IMG_UINT32              ui32MarkerVal;

	IMG_UINT32              ui32FirmwareStartedTimeStamp;

	IMG_UINT32              ui32JonesDisableMask;

	RGXFWIF_DMA_ADDR        sCorememDataStore;

	FW_PERF_CONF            eFirmwarePerf;

	IMG_DEV_VIRTADDR        RGXFW_ALIGN sSLC3FenceDevVAddr;

#if defined(SUPPORT_PDVFS)
	RGXFWIF_PDVFS_OPP       RGXFW_ALIGN sPDVFSOPPInfo;

	/**
	 * FW Pointer to memory containing core clock rate in Hz.
	 * Firmware (PDVFS) updates the memory when running on non primary FW thread
	 * to communicate to host driver.
	 */
	PRGXFWIF_CORE_CLK_RATE  RGXFW_ALIGN sCoreClockRate;
#endif

#if defined(PDUMP)
	RGXFWIF_PID_FILTER      sPIDFilter;
#endif

	RGXFWIF_GPIO_VAL_MODE   eGPIOValidationMode;
	IMG_UINT32              RGXFW_ALIGN aui32TPUTrilinearFracMask[RGXFWIF_TPU_DM_LAST];
	IMG_UINT32				RGXFW_ALIGN aui32USRMNumRegions[RGXFWIF_USRM_DM_LAST];
	IMG_UINT64				RGXFW_ALIGN aui64UVBRMNumRegions[RGXFWIF_UVBRM_DM_LAST];

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	/*
	 * Used when validation is enabled to allow the host to check
	 * that MTS sent the correct sideband in response to a kick
	 * from a given OSes schedule register.
	 * Testing is enabled if RGXFWIF_KICK_TEST_ENABLED_BIT is set
	 *
	 * Set by the host to:
	 * (osid << RGXFWIF_KICK_TEST_OSID_SHIFT) | RGXFWIF_KICK_TEST_ENABLED_BIT
	 * reset to 0 by FW when kicked by the given OSid
	 */
	IMG_UINT32              ui32OSKickTest;
#endif
	RGX_HWPERF_BVNC         sBvncKmFeatureFlags;

#if defined(SUPPORT_SECURITY_VALIDATION)
	IMG_UINT32              ui32SecurityTestFlags;
	RGXFWIF_DEV_VIRTADDR    pbSecureBuffer;
	RGXFWIF_DEV_VIRTADDR    pbNonSecureBuffer;
#endif

} UNCACHED_ALIGN RGXFWIF_SYSINIT;

#if defined(SUPPORT_GPUVIRT_VALIDATION)
#define RGXFWIF_KICK_TEST_ENABLED_BIT  0x1
#define RGXFWIF_KICK_TEST_OSID_SHIFT   0x1
#endif

/*!
 *****************************************************************************
 * Timer correlation shared data and defines
 *****************************************************************************/

typedef struct
{
	IMG_UINT64 RGXFW_ALIGN ui64OSTimeStamp;
	IMG_UINT64 RGXFW_ALIGN ui64OSMonoTimeStamp;
	IMG_UINT64 RGXFW_ALIGN ui64CRTimeStamp;

	/* Utility variable used to convert CR timer deltas to OS timer deltas (nS),
	 * where the deltas are relative to the timestamps above:
	 * deltaOS = (deltaCR * K) >> decimal_shift, see full explanation below */
	IMG_UINT64 RGXFW_ALIGN ui64CRDeltaToOSDeltaKNs;

	IMG_UINT32             ui32CoreClockSpeed;
	IMG_UINT32             ui32Reserved;
} UNCACHED_ALIGN RGXFWIF_TIME_CORR;


/* The following macros are used to help converting FW timestamps to the Host
 * time domain. On the FW the RGX_CR_TIMER counter is used to keep track of
 * time; it increments by 1 every 256 GPU clock ticks, so the general
 * formula to perform the conversion is:
 *
 * [ GPU clock speed in Hz, if (scale == 10^9) then deltaOS is in nS,
 *   otherwise if (scale == 10^6) then deltaOS is in uS ]
 *
 *             deltaCR * 256                                   256 * scale
 *  deltaOS = --------------- * scale = deltaCR * K    [ K = --------------- ]
 *             GPUclockspeed                                  GPUclockspeed
 *
 * The actual K is multiplied by 2^20 (and deltaCR * K is divided by 2^20)
 * to get some better accuracy and to avoid returning 0 in the integer
 * division 256000000/GPUfreq if GPUfreq is greater than 256MHz.
 * This is the same as keeping K as a decimal number.
 *
 * The maximum deltaOS is slightly more than 5hrs for all GPU frequencies
 * (deltaCR * K is more or less a constant), and it's relative to the base
 * OS timestamp sampled as a part of the timer correlation data.
 * This base is refreshed on GPU power-on, DVFS transition and periodic
 * frequency calibration (executed every few seconds if the FW is doing
 * some work), so as long as the GPU is doing something and one of these
 * events is triggered then deltaCR * K will not overflow and deltaOS will be
 * correct.
 */

#define RGXFWIF_CRDELTA_TO_OSDELTA_ACCURACY_SHIFT  (20)

#define RGXFWIF_GET_DELTA_OSTIME_NS(deltaCR, K) \
	(((deltaCR) * (K)) >> RGXFWIF_CRDELTA_TO_OSDELTA_ACCURACY_SHIFT)


/*!
 ******************************************************************************
 * GPU Utilisation
 *****************************************************************************/

/* See rgx_common.h for a list of GPU states */
#define RGXFWIF_GPU_UTIL_TIME_MASK       (IMG_UINT64_C(0xFFFFFFFFFFFFFFFF) & ~RGXFWIF_GPU_UTIL_STATE_MASK)

#define RGXFWIF_GPU_UTIL_GET_TIME(word)  ((word) & RGXFWIF_GPU_UTIL_TIME_MASK)
#define RGXFWIF_GPU_UTIL_GET_STATE(word) ((word) & RGXFWIF_GPU_UTIL_STATE_MASK)

/* The OS timestamps computed by the FW are approximations of the real time,
 * which means they could be slightly behind or ahead the real timer on the Host.
 * In some cases we can perform subtractions between FW approximated
 * timestamps and real OS timestamps, so we need a form of protection against
 * negative results if for instance the FW one is a bit ahead of time.
 */
#define RGXFWIF_GPU_UTIL_GET_PERIOD(newtime,oldtime) \
	(((newtime) > (oldtime)) ? ((newtime) - (oldtime)) : 0U)

#define RGXFWIF_GPU_UTIL_MAKE_WORD(time,state) \
	(RGXFWIF_GPU_UTIL_GET_TIME(time) | RGXFWIF_GPU_UTIL_GET_STATE(state))


/* The timer correlation array must be big enough to ensure old entries won't be
 * overwritten before all the HWPerf events linked to those entries are processed
 * by the MISR. The update frequency of this array depends on how fast the system
 * can change state (basically how small the APM latency is) and perform DVFS transitions.
 *
 * The minimum size is 2 (not 1) to avoid race conditions between the FW reading
 * an entry while the Host is updating it. With 2 entries in the worst case the FW
 * will read old data, which is still quite ok if the Host is updating the timer
 * correlation at that time.
 */
#define RGXFWIF_TIME_CORR_ARRAY_SIZE            256U
#define RGXFWIF_TIME_CORR_CURR_INDEX(seqcount)  ((seqcount) % RGXFWIF_TIME_CORR_ARRAY_SIZE)

/* Make sure the timer correlation array size is a power of 2 */
static_assert((RGXFWIF_TIME_CORR_ARRAY_SIZE & (RGXFWIF_TIME_CORR_ARRAY_SIZE - 1U)) == 0U,
			  "RGXFWIF_TIME_CORR_ARRAY_SIZE must be a power of two");

typedef struct
{
	RGXFWIF_TIME_CORR sTimeCorr[RGXFWIF_TIME_CORR_ARRAY_SIZE];
	IMG_UINT32        ui32TimeCorrSeqCount;

	/* Last GPU state + OS time of the last state update */
	IMG_UINT64 RGXFW_ALIGN ui64LastWord;

	/* Counters for the amount of time the GPU was active/idle/blocked */
	IMG_UINT64 RGXFW_ALIGN aui64StatsCounters[RGXFWIF_GPU_UTIL_STATE_NUM];

	IMG_UINT32 ui32GpuUtilFlags; /* Compatibility and other flags */
} UNCACHED_ALIGN RGXFWIF_GPU_UTIL_FWCB;


typedef struct
{
	IMG_UINT32           ui32RenderTargetIndex;		//Render number
	IMG_UINT32           ui32CurrentRenderTarget;	//index in RTA
	IMG_UINT32           ui32ActiveRenderTargets;	//total active RTs
	RGXFWIF_DEV_VIRTADDR sValidRenderTargets;  //Array of valid RT indices
	RGXFWIF_DEV_VIRTADDR sRTANumPartialRenders;  //Array of number of occurred partial renders per render target
	IMG_UINT32           ui32MaxRTs;   //Number of render targets in the array
	IMG_UINT32           ui32RTACtlFlags; /* Compatibility and other flags */
} UNCACHED_ALIGN RGXFWIF_RTA_CTL;

typedef struct
{
	IMG_DEV_VIRTADDR		RGXFW_ALIGN sFreeListBaseDevVAddr;		/*!< Free list device base address */
	IMG_DEV_VIRTADDR		RGXFW_ALIGN	sFreeListStateDevVAddr;		/*!< Free list state buffer */
	IMG_DEV_VIRTADDR		RGXFW_ALIGN sFreeListLastGrowDevVAddr;	/*!< Free list base address at last grow */

#if defined(PM_INTERACTIVE_MODE)
	IMG_UINT64				RGXFW_ALIGN ui64CurrentDevVAddr;
	IMG_UINT32				ui32CurrentStackTop;
#endif

	IMG_UINT32				ui32MaxPages;
	IMG_UINT32				ui32GrowPages;
	IMG_UINT32				ui32CurrentPages; /* HW pages */
#if defined(PM_INTERACTIVE_MODE)
	IMG_UINT32				ui32AllocatedPageCount;
	IMG_UINT32				ui32AllocatedMMUPageCount;
#endif
#if defined(SUPPORT_SHADOW_FREELISTS)
	IMG_UINT32				ui32HWRCounter;
	PRGXFWIF_FWMEMCONTEXT	psFWMemContext;
#endif
	IMG_UINT32				ui32FreeListID;
	IMG_BOOL				bGrowPending;
	IMG_UINT32				ui32ReadyPages; /* Pages that should be used only when OOM is reached */
	IMG_UINT32				ui32FreelistFlags; /* Compatibility and other flags */

	IMG_BOOL				bUpdatePending;
	IMG_UINT32				ui32UpdateNewPages;
	IMG_UINT32				ui32UpdateNewReadyPages;
} UNCACHED_ALIGN RGXFWIF_FREELIST;


/*!
 ******************************************************************************
 * HWRTData
 *****************************************************************************/

/* HWRTData flags */
/* Deprecated flags 1:0 */
#define HWRTDATA_HAS_LAST_TA              (1U << 2)
#define HWRTDATA_PARTIAL_RENDERED         (1U << 3)
#define HWRTDATA_KILLED                   (1U << 4)
#define HWRTDATA_KILL_AFTER_TARESTART     (1U << 5)

#if defined(SUPPORT_SW_TRP)
#define SW_TRP_SIGNATURE_FIRST_KICK 0U
#define SW_TRP_SIGNATURE_SECOND_KICK 1U
#define SW_TRP_SIGNATURE_COUNT 2U
#define SW_TRP_GEOMETRY_SIGNATURE_SIZE 8U
#define SW_TRP_FRAGMENT_SIGNATURE_SIZE 8U
/* Space for tile usage bitmap, one bit per tile on screen */
#define RGX_FEATURE_TILE_SIZE_X (32U)
#define RGX_FEATURE_TILE_SIZE_Y (32U)
#define SW_TRP_TILE_USED_SIZE ((ROGUE_RENDERSIZE_MAXX / RGX_FEATURE_TILE_SIZE_X + ROGUE_RENDERSIZE_MAXY / RGX_FEATURE_TILE_SIZE_Y) / (8U * sizeof(IMG_UINT32)))
#endif

/*!
 ******************************************************************************
 * Parameter Management (PM) control data for RGX
 *****************************************************************************/
typedef enum
{
	RGXFW_SPM_STATE_NONE = 0,
	RGXFW_SPM_STATE_PR_BLOCKED,
	RGXFW_SPM_STATE_WAIT_FOR_GROW,
	RGXFW_SPM_STATE_WAIT_FOR_HW,
	RGXFW_SPM_STATE_PR_RUNNING,
	RGXFW_SPM_STATE_PR_AVOIDED,
	RGXFW_SPM_STATE_PR_EXECUTED,
	RGXFW_SPM_STATE_PR_FORCEFREE,
} RGXFW_SPM_STATE;

typedef struct
{
	/* Make sure the structure is aligned to the dcache line */
	IMG_CHAR RGXFW_ALIGN_DCACHEL align[1];

	RGXFW_SPM_STATE			eSPMState;							/*!< current owner of this PM data structure */
	RGXFWIF_UFO				sPartialRenderTA3DFence;			/*!< TA/3D fence object holding the value to let through the 3D partial command */
#if defined(RGX_FIRMWARE)
	RGXFWIF_FWCOMMONCONTEXT	*ps3dContext;
	RGXFWIF_CCB_CMD_HEADER	*psCmdHeader;						/*!< Pointer to the header of the command holding the partial render */
	struct RGXFWIF_CMD3D_STRUCT			*ps3DCmd;							/*!< Pointer to the 3D command holding the partial render register info*/
	RGXFWIF_PRBUFFER		*apsPRBuffer[RGXFWIF_PRBUFFER_MAXSUPPORTED];	/*!< Array of pointers to PR Buffers which may be used if partial render is needed */
#else
	RGXFWIF_DEV_VIRTADDR	ps3dContext;
	RGXFWIF_DEV_VIRTADDR	psCmdHeader;						/*!< Pointer to the header of the command holding the partial render */
	RGXFWIF_DEV_VIRTADDR	ps3DCmd;							/*!< Pointer to the 3D command holding the partial render register info*/
	RGXFWIF_DEV_VIRTADDR	apsPRBuffer[RGXFWIF_PRBUFFER_MAXSUPPORTED];		/*!< Array of pointers to PR Buffers which may be used if partial render is needed */
#endif
	RGXFW_FREELIST_TYPE		eOOMFreeListType;					/*!< Indicates the freelist type that went out of memory */
	bool					b3DMemFreeDetected;					/*!< Indicates if a 3D Memory Free has been detected, which resolves OOM */
} RGXFW_SPMCTL;

typedef enum
{
	RGXFWIF_RTDATA_STATE_NONE = 0,
	RGXFWIF_RTDATA_STATE_KICKTA,
	RGXFWIF_RTDATA_STATE_KICKTAFIRST,
	RGXFWIF_RTDATA_STATE_TAFINISHED,
	RGXFWIF_RTDATA_STATE_KICK3D,
	RGXFWIF_RTDATA_STATE_3DFINISHED,
	RGXFWIF_RTDATA_STATE_TAOUTOFMEM,
	RGXFWIF_RTDATA_STATE_PARTIALRENDERFINISHED,
	/* In case of HWR, we can't set the RTDATA state to NONE,
	 * as this will cause any TA to become a first TA.
	 * To ensure all related TA's are skipped, we use the HWR state */
	RGXFWIF_RTDATA_STATE_HWR,
	RGXFWIF_RTDATA_STATE_UNKNOWN = 0x7FFFFFFFU
} RGXFWIF_RTDATA_STATE;

typedef struct
{
	IMG_UINT32							ui32HWRTDataFlags;
	RGXFWIF_RTDATA_STATE				eState;


	IMG_UINT64							RGXFW_ALIGN ui64VCECatBase[4];
	IMG_UINT64							RGXFW_ALIGN ui64VCELastCatBase[4];
	IMG_UINT64							RGXFW_ALIGN ui64TECatBase[4];
	IMG_UINT64							RGXFW_ALIGN ui64TELastCatBase[4];
	IMG_UINT64							RGXFW_ALIGN ui64AlistCatBase;
	IMG_UINT64							RGXFW_ALIGN ui64AlistLastCatBase;

#if defined(PM_INTERACTIVE_MODE)
	IMG_DEV_VIRTADDR					RGXFW_ALIGN psVHeapTableDevVAddr;
	IMG_DEV_VIRTADDR					RGXFW_ALIGN sPMMListDevVAddr;
#else
	/* Series8 PM State buffers */
	IMG_DEV_VIRTADDR					RGXFW_ALIGN sPMRenderStateDevVAddr;
	IMG_DEV_VIRTADDR					RGXFW_ALIGN sPMSecureRenderStateDevVAddr;
#endif

	PRGXFWIF_FREELIST					RGXFW_ALIGN apsFreeLists[RGXFW_MAX_FREELISTS];
	IMG_UINT32							aui32FreeListHWRSnapshot[RGXFW_MAX_FREELISTS];
	IMG_BOOL							bRenderStateNeedsReset;

	RGXFWIF_CLEANUP_CTL					sCleanupState;

	RGXFWIF_RTA_CTL						sRTACtl;

	IMG_UINT32							ui32ScreenPixelMax;
	IMG_UINT64							RGXFW_ALIGN ui64PPPMultiSampleCtl;
	IMG_UINT32							ui32TEStride;
	IMG_DEV_VIRTADDR					RGXFW_ALIGN sTailPtrsDevVAddr;
	IMG_UINT32							ui32TPCSize;
	IMG_UINT32							ui32TEScreen;
	IMG_UINT32							ui32TEAA;
	IMG_UINT32							ui32TEMTILE1;
	IMG_UINT32							ui32TEMTILE2;
	IMG_UINT32							ui32RgnStride;
	IMG_UINT32							ui32ISPMergeLowerX;
	IMG_UINT32							ui32ISPMergeLowerY;
	IMG_UINT32							ui32ISPMergeUpperX;
	IMG_UINT32							ui32ISPMergeUpperY;
	IMG_UINT32							ui32ISPMergeScaleX;
	IMG_UINT32							ui32ISPMergeScaleY;
#if defined(RGX_FIRMWARE)
	struct RGXFWIF_FWCOMMONCONTEXT_*	psOwnerGeom;
#else
	RGXFWIF_DEV_VIRTADDR				pui32OwnerGeomNotUsedByHost;
#endif

#if defined(PM_INTERACTIVE_MODE)
	IMG_UINT64							RGXFW_ALIGN ui64PMAListStackPointer;
	IMG_UINT32							ui32PMMListStackPointer;
#endif
#if defined(SUPPORT_SW_TRP)
	/* SW-TRP state and signature data
	 *
	 * Stored state is used to kick the same geometry or 3D twice,
	 * State is stored before first kick and restored before second to rerun the same data.
	 * Signatures from both kicks are stored and compared */
    IMG_UINT32							aaui32GeometrySignature[SW_TRP_SIGNATURE_COUNT][SW_TRP_GEOMETRY_SIGNATURE_SIZE];
    IMG_UINT32							aaui32FragmentSignature[SW_TRP_SIGNATURE_COUNT][SW_TRP_FRAGMENT_SIGNATURE_SIZE];
	IMG_UINT32							ui32KickFlagsCopy;
	IMG_UINT32							ui32SW_TRPState;
	IMG_UINT32							aui32TileUsed[SW_TRP_TILE_USED_SIZE];
	RGXFW_SPMCTL						sSPMCtlCopy;
#endif
#if defined(SUPPORT_TRP)
	/* TRP state
	 *
	 * Stored state is used to kick the same geometry or 3D twice,
	 * State is stored before first kick and restored before second to rerun the same data.
	 */
	IMG_UINT32							ui32KickFlagsCopy;
	IMG_UINT32							ui32TRPState;
	RGXFWIF_TRP_CHECKSUM_3D				aui64TRPChecksums3D;
	RGXFWIF_TRP_CHECKSUM_GEOM			aui64TRPChecksumsGeom;
#endif
} UNCACHED_ALIGN RGXFWIF_HWRTDATA;

#endif /* RGX_FWIF_KM_H */

/******************************************************************************
 End of file (rgx_fwif_km.h)
******************************************************************************/

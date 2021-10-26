/*************************************************************************/ /*!
@File
@Title          RGX HWPerf and Debug Types and Defines Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Common data types definitions for hardware performance API
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
#ifndef RGX_HWPERF_H_
#define RGX_HWPERF_H_

#if defined(__cplusplus)
extern "C" {
#endif

/* These structures are used on both GPU and CPU and must be a size that is a
 * multiple of 64 bits, 8 bytes to allow the FW to write 8 byte quantities at
 * 8 byte aligned addresses. RGX_FW_STRUCT_*_ASSERT() is used to check this.
 */

/******************************************************************************
 * Includes and Defines
 *****************************************************************************/

#include "img_types.h"
#include "img_defs.h"

#include "rgx_common.h"
#include "pvrsrv_tlcommon.h"
#include "pvrsrv_sync_km.h"


#if defined(RGX_BVNC_CORE_KM_HEADER) && defined(RGX_BNC_CONFIG_KM_HEADER)
/* HWPerf interface assumption checks */
static_assert(RGX_FEATURE_NUM_CLUSTERS <= 16U, "Cluster count too large for HWPerf protocol definition");


#if !defined(__KERNEL__)
/* User-mode and Firmware definitions only */

/*! The number of indirectly addressable TPU_MSC blocks in the GPU */
# define RGX_HWPERF_PHANTOM_INDIRECT_BY_DUST MAX(((IMG_UINT32)RGX_FEATURE_NUM_CLUSTERS >> 1), 1U)

/*! The number of indirectly addressable USC blocks in the GPU */
# define RGX_HWPERF_PHANTOM_INDIRECT_BY_CLUSTER (RGX_FEATURE_NUM_CLUSTERS)

# if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)

 /*! Defines the number of performance counter blocks that are directly
  * addressable in the RGX register map for S. */
#  define RGX_HWPERF_MAX_DIRECT_ADDR_BLKS      1 /* JONES */
#  define RGX_HWPERF_INDIRECT_BY_PHANTOM       (RGX_NUM_PHANTOMS)
#  define RGX_HWPERF_PHANTOM_NONDUST_BLKS      1 /* BLACKPEARL */
#  define RGX_HWPERF_PHANTOM_DUST_BLKS         2 /* TPU, TEXAS */
#  define RGX_HWPERF_PHANTOM_DUST_CLUSTER_BLKS 2 /* USC, PBE */
#  define RGX_HWPERF_DOPPLER_BX_TU_BLKS        0

# elif defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)

  /*! Defines the number of performance counter blocks that are directly
   * addressable in the RGX register map. */
#   define RGX_HWPERF_MAX_DIRECT_ADDR_BLKS    2 /* TORNADO, TA */
#   define RGX_HWPERF_DOPPLER_BX_TU_BLKS      0

#  define RGX_HWPERF_INDIRECT_BY_PHANTOM       (RGX_NUM_PHANTOMS)
#  define RGX_HWPERF_PHANTOM_NONDUST_BLKS      2 /* RASTER, TEXAS */
#  define RGX_HWPERF_PHANTOM_DUST_BLKS         1 /* TPU */
#  define RGX_HWPERF_PHANTOM_DUST_CLUSTER_BLKS 1 /* USC */

# else /* !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE) && !defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE) i.e. S6 */

 /*! Defines the number of performance counter blocks that are
  * addressable in the RGX register map for Series 6. */
#  define RGX_HWPERF_MAX_DIRECT_ADDR_BLKS      3 /* TA, RASTER, HUB */
#  define RGX_HWPERF_INDIRECT_BY_PHANTOM       0 /* PHANTOM is not there in Rogue1. Just using it to keep naming same as later series (RogueXT n Rogue XT+) */
#  define RGX_HWPERF_PHANTOM_NONDUST_BLKS      0
#  define RGX_HWPERF_PHANTOM_DUST_BLKS         1 /* TPU */
#  define RGX_HWPERF_PHANTOM_DUST_CLUSTER_BLKS 1 /* USC */
#  define RGX_HWPERF_DOPPLER_BX_TU_BLKS        0

# endif

/*! The number of performance counters in each layout block defined for UM/FW code */
#if defined(RGX_FEATURE_CLUSTER_GROUPING)
  #define RGX_HWPERF_CNTRS_IN_BLK 6
 #else
  #define RGX_HWPERF_CNTRS_IN_BLK 4
#endif

#else /* defined(__KERNEL__) */
/* Kernel/server definitions - not used, hence invalid definitions */

# define RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC 0xFF

# define RGX_HWPERF_PHANTOM_INDIRECT_BY_DUST    RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC
# define RGX_HWPERF_PHANTOM_INDIRECT_BY_CLUSTER RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC

# define RGX_HWPERF_MAX_DIRECT_ADDR_BLKS      RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC
# define RGX_HWPERF_INDIRECT_BY_PHANTOM       RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC
# define RGX_HWPERF_PHANTOM_NONDUST_BLKS      RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC
# define RGX_HWPERF_PHANTOM_DUST_BLKS         RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC
# define RGX_HWPERF_PHANTOM_DUST_CLUSTER_BLKS RGX_HWPERF_NUM_BLOCK_UNITS_RUNTIME_CALC

# define RGX_HWPERF_DOPPLER_BX_TU_BLKS       0U

#endif
#endif

/*! The number of custom non-mux counter blocks supported */
#define RGX_HWPERF_MAX_CUSTOM_BLKS 5U

/*! The number of counters supported in each non-mux counter block */
#define RGX_HWPERF_MAX_CUSTOM_CNTRS 8U


/******************************************************************************
 * Packet Event Type Enumerations
 *****************************************************************************/

/*! Type used to encode the event that generated the packet.
 * NOTE: When this type is updated the corresponding hwperfbin2json tool
 * source needs to be updated as well. The RGX_HWPERF_EVENT_MASK_* macros will
 * also need updating when adding new types.
 */
typedef IMG_UINT32 RGX_HWPERF_EVENT_TYPE;

#define RGX_HWPERF_INVALID				0x00U

/* FW types 0x01..0x06 */
#define RGX_HWPERF_FW_EVENT_RANGE_FIRST_TYPE	0x01U

#define RGX_HWPERF_FW_BGSTART			0x01U
#define RGX_HWPERF_FW_BGEND				0x02U
#define RGX_HWPERF_FW_IRQSTART			0x03U

#define RGX_HWPERF_FW_IRQEND			0x04U
#define RGX_HWPERF_FW_DBGSTART			0x05U
#define RGX_HWPERF_FW_DBGEND			0x06U

#define RGX_HWPERF_FW_EVENT_RANGE_LAST_TYPE		0x06U

/* HW types 0x07..0x19 */
#define RGX_HWPERF_HW_EVENT_RANGE0_FIRST_TYPE	0x07U

#define RGX_HWPERF_HW_PMOOM_TAPAUSE		0x07U

#define RGX_HWPERF_HW_TAKICK			0x08U
#define RGX_HWPERF_HW_TAFINISHED		0x09U
#define RGX_HWPERF_HW_3DTQKICK			0x0AU
#define RGX_HWPERF_HW_3DKICK			0x0BU
#define RGX_HWPERF_HW_3DFINISHED		0x0CU
#define RGX_HWPERF_HW_CDMKICK			0x0DU
#define RGX_HWPERF_HW_CDMFINISHED		0x0EU
#define RGX_HWPERF_HW_TLAKICK			0x0FU
#define RGX_HWPERF_HW_TLAFINISHED		0x10U
#define RGX_HWPERF_HW_3DSPMKICK			0x11U
#define RGX_HWPERF_HW_PERIODIC			0x12U
#define RGX_HWPERF_HW_RTUKICK			0x13U
#define RGX_HWPERF_HW_RTUFINISHED		0x14U
#define RGX_HWPERF_HW_SHGKICK			0x15U
#define RGX_HWPERF_HW_SHGFINISHED		0x16U
#define RGX_HWPERF_HW_3DTQFINISHED		0x17U
#define RGX_HWPERF_HW_3DSPMFINISHED		0x18U

#define RGX_HWPERF_HW_PMOOM_TARESUME	0x19U

/* HW_EVENT_RANGE0 used up. Use next empty range below to add new hardware events */
#define RGX_HWPERF_HW_EVENT_RANGE0_LAST_TYPE	0x19U

	/* other types 0x1A..0x1F */
#define RGX_HWPERF_CLKS_CHG				0x1AU
#define RGX_HWPERF_GPU_STATE_CHG		0x1BU

/* power types 0x20..0x27 */
#define RGX_HWPERF_PWR_EST_RANGE_FIRST_TYPE	0x20U
#define RGX_HWPERF_PWR_EST_REQUEST		0x20U
#define RGX_HWPERF_PWR_EST_READY		0x21U
#define RGX_HWPERF_PWR_EST_RESULT		0x22U
#define RGX_HWPERF_PWR_EST_RANGE_LAST_TYPE	0x22U

#define RGX_HWPERF_PWR_CHG				0x23U

/* HW_EVENT_RANGE1 0x28..0x2F, for accommodating new hardware events */
#define RGX_HWPERF_HW_EVENT_RANGE1_FIRST_TYPE	0x28U

#define RGX_HWPERF_HW_TDMKICK			0x28U
#define RGX_HWPERF_HW_TDMFINISHED		0x29U
#define RGX_HWPERF_HW_NULLKICK			0x2AU

#define RGX_HWPERF_HW_EVENT_RANGE1_LAST_TYPE 0x2AU

/* context switch types 0x30..0x31 */
#define RGX_HWPERF_CSW_START			0x30U
#define RGX_HWPERF_CSW_FINISHED			0x31U

/* DVFS events */
#define RGX_HWPERF_DVFS					0x32U

/* firmware misc 0x38..0x39 */
#define RGX_HWPERF_UFO					0x38U
#define RGX_HWPERF_FWACT				0x39U

/* last */
#define RGX_HWPERF_LAST_TYPE			0x3BU

/* This enumeration must have a value that is a power of two as it is
 * used in masks and a filter bit field (currently 64 bits long).
 */
#define RGX_HWPERF_MAX_TYPE				0x40U


/* The event type values are incrementing integers for use as a shift ordinal
 * in the event filtering process at the point events are generated.
 * This scheme thus implies a limit of 63 event types.
 */
static_assert(RGX_HWPERF_LAST_TYPE < RGX_HWPERF_MAX_TYPE, "Too many HWPerf event types");

/* Macro used to check if an event type ID is present in the known set of hardware type events */
#define HWPERF_PACKET_IS_HW_TYPE(_etype)	(((_etype) >= RGX_HWPERF_HW_EVENT_RANGE0_FIRST_TYPE && (_etype) <= RGX_HWPERF_HW_EVENT_RANGE0_LAST_TYPE) || \
											 ((_etype) >= RGX_HWPERF_HW_EVENT_RANGE1_FIRST_TYPE && (_etype) <= RGX_HWPERF_HW_EVENT_RANGE1_LAST_TYPE))

#define HWPERF_PACKET_IS_FW_TYPE(_etype)					\
	((_etype) >= RGX_HWPERF_FW_EVENT_RANGE_FIRST_TYPE &&	\
	 (_etype) <= RGX_HWPERF_FW_EVENT_RANGE_LAST_TYPE)


typedef enum {
	RGX_HWPERF_HOST_INVALID   = 0x00,
	RGX_HWPERF_HOST_ENQ       = 0x01,
	RGX_HWPERF_HOST_UFO       = 0x02,
	RGX_HWPERF_HOST_ALLOC     = 0x03,
	RGX_HWPERF_HOST_CLK_SYNC  = 0x04,
	RGX_HWPERF_HOST_FREE      = 0x05,
	RGX_HWPERF_HOST_MODIFY    = 0x06,
	RGX_HWPERF_HOST_DEV_INFO  = 0x07,
	RGX_HWPERF_HOST_INFO      = 0x08,
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT = 0x09,
	RGX_HWPERF_HOST_SYNC_SW_TL_ADVANCE  = 0x0A,

	/* last */
	RGX_HWPERF_HOST_LAST_TYPE,

	/* This enumeration must have a value that is a power of two as it is
	 * used in masks and a filter bit field (currently 32 bits long).
	 */
	RGX_HWPERF_HOST_MAX_TYPE  = 0x20
} RGX_HWPERF_HOST_EVENT_TYPE;

/* The event type values are incrementing integers for use as a shift ordinal
 * in the event filtering process at the point events are generated.
 * This scheme thus implies a limit of 31 event types.
 */
static_assert(RGX_HWPERF_HOST_LAST_TYPE < RGX_HWPERF_HOST_MAX_TYPE, "Too many HWPerf host event types");


/******************************************************************************
 * Packet Header Format Version 2 Types
 *****************************************************************************/

/*! Major version number of the protocol in operation
 */
#define RGX_HWPERF_V2_FORMAT 2

/*! Signature ASCII pattern 'HWP2' found in the first word of a HWPerfV2 packet
 */
#define HWPERF_PACKET_V2_SIG		0x48575032

/*! Signature ASCII pattern 'HWPA' found in the first word of a HWPerfV2a packet
 */
#define HWPERF_PACKET_V2A_SIG		0x48575041

/*! Signature ASCII pattern 'HWPB' found in the first word of a HWPerfV2b packet
 */
#define HWPERF_PACKET_V2B_SIG		0x48575042

#define HWPERF_PACKET_ISVALID(_ptr) (((_ptr) == HWPERF_PACKET_V2_SIG) || ((_ptr) == HWPERF_PACKET_V2A_SIG)|| ((_ptr) == HWPERF_PACKET_V2B_SIG))

/*! Type defines the HWPerf packet header common to all events. */
typedef struct
{
	IMG_UINT32  ui32Sig;        /*!< Always the value HWPERF_PACKET_SIG */
	IMG_UINT32  ui32Size;       /*!< Overall packet size in bytes */
	IMG_UINT32  eTypeId;        /*!< Event type information field */
	IMG_UINT32  ui32Ordinal;    /*!< Sequential number of the packet */
	IMG_UINT64  ui64Timestamp;  /*!< Event timestamp */
} RGX_HWPERF_V2_PACKET_HDR, *RGX_PHWPERF_V2_PACKET_HDR;

#ifndef __CHECKER__
RGX_FW_STRUCT_OFFSET_ASSERT(RGX_HWPERF_V2_PACKET_HDR, ui64Timestamp);

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_V2_PACKET_HDR);
#endif


/*! Mask for use with the IMG_UINT32 ui32Size header field */
#define RGX_HWPERF_SIZE_MASK         0xFFFFU

/*! This macro defines an upper limit to which the size of the largest variable
 * length HWPerf packet must fall within, currently 3KB. This constant may be
 * used to allocate a buffer to hold one packet.
 * This upper limit is policed by packet producing code.
 */
#define RGX_HWPERF_MAX_PACKET_SIZE   0xC00U

/*! Defines an upper limit to the size of a variable length packet payload.
 */
#define RGX_HWPERF_MAX_PAYLOAD_SIZE	 ((IMG_UINT32)(RGX_HWPERF_MAX_PACKET_SIZE-\
	sizeof(RGX_HWPERF_V2_PACKET_HDR)))


/*! Macro which takes a structure name and provides the packet size for
 * a fixed size payload packet, rounded up to 8 bytes to align packets
 * for 64 bit architectures. */
#define RGX_HWPERF_MAKE_SIZE_FIXED(_struct)       ((IMG_UINT32)(RGX_HWPERF_SIZE_MASK&(sizeof(RGX_HWPERF_V2_PACKET_HDR)+PVR_ALIGN(sizeof(_struct), PVRSRVTL_PACKET_ALIGNMENT))))

/*! Macro which takes the number of bytes written in the data payload of a
 * packet for a variable size payload packet, rounded up to 8 bytes to
 * align packets for 64 bit architectures. */
#define RGX_HWPERF_MAKE_SIZE_VARIABLE(_size)      ((IMG_UINT32)(RGX_HWPERF_SIZE_MASK&(sizeof(RGX_HWPERF_V2_PACKET_HDR)+PVR_ALIGN((_size), PVRSRVTL_PACKET_ALIGNMENT))))

/*! Macro to obtain the size of the packet */
#define RGX_HWPERF_GET_SIZE(_packet_addr)         ((IMG_UINT16)(((_packet_addr)->ui32Size) & RGX_HWPERF_SIZE_MASK))

/*! Macro to obtain the size of the packet data */
#define RGX_HWPERF_GET_DATA_SIZE(_packet_addr)    (RGX_HWPERF_GET_SIZE(_packet_addr) - sizeof(RGX_HWPERF_V2_PACKET_HDR))



/*! Masks for use with the IMG_UINT32 eTypeId header field */
#define RGX_HWPERF_TYPEID_MASK			0x7FFFFU
#define RGX_HWPERF_TYPEID_EVENT_MASK	0x07FFFU
#define RGX_HWPERF_TYPEID_THREAD_MASK	0x08000U
#define RGX_HWPERF_TYPEID_STREAM_MASK	0x70000U
#define RGX_HWPERF_TYPEID_META_DMA_MASK	0x80000U
#define RGX_HWPERF_TYPEID_OSID_MASK		0xFF000000U

/*! Meta thread macros for encoding the ID into the type field of a packet */
#define RGX_HWPERF_META_THREAD_SHIFT	15U
#define RGX_HWPERF_META_THREAD_ID0		0x0U
#define RGX_HWPERF_META_THREAD_ID1		0x1U
/*! Obsolete, kept for source compatibility */
#define RGX_HWPERF_META_THREAD_MASK		0x1U
/*! Stream ID macros for encoding the ID into the type field of a packet */
#define RGX_HWPERF_STREAM_SHIFT			16U
/*! Meta DMA macro for encoding how the packet was generated into the type field of a packet */
#define RGX_HWPERF_META_DMA_SHIFT		19U
/*! OSID bit-shift macro used for encoding OSID into type field of a packet */
#define RGX_HWPERF_OSID_SHIFT			24U
typedef enum {
	RGX_HWPERF_STREAM_ID0_FW,     /*!< Events from the Firmware/GPU */
	RGX_HWPERF_STREAM_ID1_HOST,   /*!< Events from the Server host driver component */
	RGX_HWPERF_STREAM_ID2_CLIENT, /*!< Events from the Client host driver component */
	RGX_HWPERF_STREAM_ID_LAST,
} RGX_HWPERF_STREAM_ID;

/* Checks if all stream IDs can fit under RGX_HWPERF_TYPEID_STREAM_MASK. */
static_assert(((IMG_UINT32)RGX_HWPERF_STREAM_ID_LAST - 1U) < (RGX_HWPERF_TYPEID_STREAM_MASK >> RGX_HWPERF_STREAM_SHIFT),
		"Too many HWPerf stream IDs.");

/*! Macros used to set the packet type and encode meta thread ID (0|1), HWPerf stream ID, and OSID within */
#define RGX_HWPERF_MAKE_TYPEID(_stream, _type, _thread, _metadma, _osid)\
		((IMG_UINT32) ((RGX_HWPERF_TYPEID_STREAM_MASK&((IMG_UINT32)(_stream) << RGX_HWPERF_STREAM_SHIFT)) | \
		(RGX_HWPERF_TYPEID_THREAD_MASK & ((IMG_UINT32)(_thread) << RGX_HWPERF_META_THREAD_SHIFT)) | \
		(RGX_HWPERF_TYPEID_EVENT_MASK & (IMG_UINT32)(_type)) | \
		(RGX_HWPERF_TYPEID_META_DMA_MASK & ((IMG_UINT32)(_metadma) << RGX_HWPERF_META_DMA_SHIFT)) | \
		(RGX_HWPERF_TYPEID_OSID_MASK & ((IMG_UINT32)(_osid) << RGX_HWPERF_OSID_SHIFT))))

/*! Obtains the event type that generated the packet */
#define RGX_HWPERF_GET_TYPE(_packet_addr)            (((_packet_addr)->eTypeId) & RGX_HWPERF_TYPEID_EVENT_MASK)

/*! Obtains the META Thread number that generated the packet */
#define RGX_HWPERF_GET_THREAD_ID(_packet_addr)       (((((_packet_addr)->eTypeId) & RGX_HWPERF_TYPEID_THREAD_MASK) >> RGX_HWPERF_META_THREAD_SHIFT))

/*! Obtains the guest OSID which resulted in packet generation */
#define RGX_HWPERF_GET_OSID(_packet_addr)            (((_packet_addr)->eTypeId & RGX_HWPERF_TYPEID_OSID_MASK) >> RGX_HWPERF_OSID_SHIFT)

/*! Obtain stream id */
#define RGX_HWPERF_GET_STREAM_ID(_packet_addr)       (((((_packet_addr)->eTypeId) & RGX_HWPERF_TYPEID_STREAM_MASK) >> RGX_HWPERF_STREAM_SHIFT))

/*! Obtain information about how the packet was generated, which might affect payload total size */
#define RGX_HWPERF_GET_META_DMA_INFO(_packet_addr)   (((((_packet_addr)->eTypeId) & RGX_HWPERF_TYPEID_META_DMA_MASK) >> RGX_HWPERF_META_DMA_SHIFT))

/*! Macros to obtain a typed pointer to a packet or data structure given a packet address */
#define RGX_HWPERF_GET_PACKET(_buffer_addr)            ((RGX_HWPERF_V2_PACKET_HDR *)(void *)  (_buffer_addr))
#define RGX_HWPERF_GET_PACKET_DATA_BYTES(_packet_addr) (IMG_OFFSET_ADDR((_packet_addr), sizeof(RGX_HWPERF_V2_PACKET_HDR)))
#define RGX_HWPERF_GET_NEXT_PACKET(_packet_addr)       ((RGX_HWPERF_V2_PACKET_HDR *)  (IMG_OFFSET_ADDR((_packet_addr), RGX_HWPERF_SIZE_MASK&((_packet_addr)->ui32Size))))

/*! Obtains a typed pointer to a packet header given the packed data address */
#define RGX_HWPERF_GET_PACKET_HEADER(_packet_addr)     ((RGX_HWPERF_V2_PACKET_HDR *)  (IMG_OFFSET_ADDR((_packet_addr), -sizeof(RGX_HWPERF_V2_PACKET_HDR))))


/******************************************************************************
 * Other Common Defines
 *****************************************************************************/

/* This macro is not a real array size, but indicates the array has a variable
 * length only known at run-time but always contains at least 1 element. The
 * final size of the array is deduced from the size field of a packet header.
 */
#define RGX_HWPERF_ONE_OR_MORE_ELEMENTS  1U

/* This macro is not a real array size, but indicates the array is optional
 * and if present has a variable length only known at run-time. The final
 * size of the array is deduced from the size field of a packet header. */
#define RGX_HWPERF_ZERO_OR_MORE_ELEMENTS 1U


/*! Masks for use with the IMG_UINT32 ui32BlkInfo field */
#define RGX_HWPERF_BLKINFO_BLKCOUNT_MASK	0xFFFF0000U
#define RGX_HWPERF_BLKINFO_BLKOFFSET_MASK	0x0000FFFFU

/*! Shift for the NumBlocks and counter block offset field in ui32BlkInfo */
#define RGX_HWPERF_BLKINFO_BLKCOUNT_SHIFT	16U
#define RGX_HWPERF_BLKINFO_BLKOFFSET_SHIFT 0U

/*! Macro used to set the block info word as a combination of two 16-bit integers */
#define RGX_HWPERF_MAKE_BLKINFO(_numblks, _blkoffset) ((IMG_UINT32) ((RGX_HWPERF_BLKINFO_BLKCOUNT_MASK&((_numblks) << RGX_HWPERF_BLKINFO_BLKCOUNT_SHIFT)) | (RGX_HWPERF_BLKINFO_BLKOFFSET_MASK&((_blkoffset) << RGX_HWPERF_BLKINFO_BLKOFFSET_SHIFT))))

/*! Macro used to obtain get the number of counter blocks present in the packet */
#define RGX_HWPERF_GET_BLKCOUNT(_blkinfo)            (((_blkinfo) & RGX_HWPERF_BLKINFO_BLKCOUNT_MASK) >> RGX_HWPERF_BLKINFO_BLKCOUNT_SHIFT)

/*! Obtains the offset of the counter block stream in the packet */
#define RGX_HWPERF_GET_BLKOFFSET(_blkinfo)           (((_blkinfo) & RGX_HWPERF_BLKINFO_BLKOFFSET_MASK) >> RGX_HWPERF_BLKINFO_BLKOFFSET_SHIFT)

/* This macro gets the number of blocks depending on the packet version */
#define RGX_HWPERF_GET_NUMBLKS(_sig, _packet_data, _numblocks) \
	if (HWPERF_PACKET_V2B_SIG == (_sig)) \
	{ \
		(_numblocks) = RGX_HWPERF_GET_BLKCOUNT((_packet_data)->ui32BlkInfo); \
	} \
	else \
	{ \
		IMG_UINT32 ui32VersionOffset = (((_sig) == HWPERF_PACKET_V2_SIG) ? 1 : 3); \
		(_numblocks) = *(IMG_UINT16 *)(IMG_OFFSET_ADDR(&(_packet_data)->ui32WorkTarget, ui32VersionOffset)); \
	}

/* This macro gets the counter stream pointer depending on the packet version */
#define RGX_HWPERF_GET_CNTSTRM(_sig, _hw_packet_data, _cntstream_ptr) \
{ \
	if (HWPERF_PACKET_V2B_SIG == (_sig)) \
	{ \
		(_cntstream_ptr) = (IMG_UINT32 *)(IMG_OFFSET_ADDR((_hw_packet_data), RGX_HWPERF_GET_BLKOFFSET((_hw_packet_data)->ui32BlkInfo))); \
	} \
	else \
	{ \
		IMG_UINT32 ui32BlkStreamOffsetInWords = (((_sig) == HWPERF_PACKET_V2_SIG) ? 6 : 8); \
		(_cntstream_ptr) = (IMG_UINT32 *)(IMG_OFFSET_ADDR((_hw_packet_data), ui32BlkStreamOffsetInWords)); \
	} \
}

/* This is the maximum frame contexts that are supported in the driver at the moment */
#define RGX_HWPERF_HW_MAX_WORK_CONTEXT               2

/*! Masks for use with the RGX_HWPERF_UFO_EV eEvType field */
#define RGX_HWPERF_UFO_STREAMSIZE_MASK 0xFFFF0000U
#define RGX_HWPERF_UFO_STREAMOFFSET_MASK 0x0000FFFFU

/*! Shift for the UFO count and data stream fields */
#define RGX_HWPERF_UFO_STREAMSIZE_SHIFT 16U
#define RGX_HWPERF_UFO_STREAMOFFSET_SHIFT 0U

/*! Macro used to set UFO stream info word as a combination of two 16-bit integers */
#define RGX_HWPERF_MAKE_UFOPKTINFO(_ssize, _soff) \
        ((IMG_UINT32) ((RGX_HWPERF_UFO_STREAMSIZE_MASK&((_ssize) << RGX_HWPERF_UFO_STREAMSIZE_SHIFT)) | \
        (RGX_HWPERF_UFO_STREAMOFFSET_MASK&((_soff) << RGX_HWPERF_UFO_STREAMOFFSET_SHIFT))))

/*! Macro used to obtain UFO count*/
#define RGX_HWPERF_GET_UFO_STREAMSIZE(_streaminfo) \
        (((_streaminfo) & RGX_HWPERF_UFO_STREAMSIZE_MASK) >> RGX_HWPERF_UFO_STREAMSIZE_SHIFT)

/*! Obtains the offset of the UFO stream in the packet */
#define RGX_HWPERF_GET_UFO_STREAMOFFSET(_streaminfo) \
        (((_streaminfo) & RGX_HWPERF_UFO_STREAMOFFSET_MASK) >> RGX_HWPERF_UFO_STREAMOFFSET_SHIFT)



/******************************************************************************
 * Data Stream Common Types
 *****************************************************************************/

/* All the Data Masters HWPerf is aware of. When a new DM is added to this
 * list, it should be appended at the end to maintain backward compatibility
 * of HWPerf data */
typedef enum {

	RGX_HWPERF_DM_GP,
	RGX_HWPERF_DM_2D,
	RGX_HWPERF_DM_TA,
	RGX_HWPERF_DM_3D,
	RGX_HWPERF_DM_CDM,
	RGX_HWPERF_DM_RTU,
	RGX_HWPERF_DM_SHG,
	RGX_HWPERF_DM_TDM,

	RGX_HWPERF_DM_LAST,

	RGX_HWPERF_DM_INVALID = 0x1FFFFFFF
} RGX_HWPERF_DM;

/* Enum containing bit pos for 32bit feature flags used in hwperf and api */
typedef enum {
	RGX_HWPERF_FEATURE_PERFBUS_FLAG                = 0x001,
	RGX_HWPERF_FEATURE_S7_TOP_INFRASTRUCTURE_FLAG  = 0x002,
	RGX_HWPERF_FEATURE_XT_TOP_INFRASTRUCTURE_FLAG  = 0x004,
	RGX_HWPERF_FEATURE_PERF_COUNTER_BATCH_FLAG     = 0x008,
	RGX_HWPERF_FEATURE_ROGUEXE_FLAG                = 0x010,
	RGX_HWPERF_FEATURE_DUST_POWER_ISLAND_S7_FLAG   = 0x020,
	RGX_HWPERF_FEATURE_PBE2_IN_XE_FLAG             = 0x040,
	RGX_HWPERF_FEATURE_WORKLOAD_ESTIMATION         = 0x080
} RGX_HWPERF_FEATURE_FLAGS;

/*! This structure holds the data of a firmware packet. */
typedef struct
{
	RGX_HWPERF_DM eDM;				/*!< DataMaster identifier, see RGX_HWPERF_DM */
	IMG_UINT32 ui32TxtActCyc;		/*!< Meta TXTACTCYC register value */
	IMG_UINT32 ui32FWPerfCount0;	/*!< Meta/MIPS PERF_COUNT0 register */
	IMG_UINT32 ui32FWPerfCount1;	/*!< Meta/MIPS PERF_COUNT1 register */
	IMG_UINT32 ui32TimeCorrIndex;
	IMG_UINT32 ui32Padding;
} RGX_HWPERF_FW_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_FW_DATA);

/*! This structure holds the data of a hardware packet, including counters. */
typedef struct
{
	IMG_UINT32 ui32DMCyc;         /*!< DataMaster cycle count register, 0 if none */
	IMG_UINT32 ui32FrameNum;      /*!< Frame number, undefined on some DataMasters */
	IMG_UINT32 ui32PID;           /*!< Process identifier */
	IMG_UINT32 ui32DMContext;     /*!< GPU Data Master (FW) Context */
	IMG_UINT32 ui32WorkTarget;    /*!< RenderTarget for a TA,3D; Frame context for RTU, 0x0 otherwise */
	IMG_UINT32 ui32ExtJobRef;     /*!< Client driver context job reference used for tracking/debugging */
	IMG_UINT32 ui32IntJobRef;     /*!< RGX Data master context job reference used for tracking/debugging */
	IMG_UINT32 ui32TimeCorrIndex; /*!< Index to the time correlation at the time the packet was generated */
	IMG_UINT32 ui32BlkInfo;       /*!< <31..16> NumBlocks <15..0> Counter block stream offset */
	IMG_UINT32 ui32WorkCtx;       /*!< Work context: Render Context for TA/3D; RayTracing Context for RTU/SHG; 0x0 otherwise */
	IMG_UINT32 ui32CtxPriority;   /*!< Context priority */
	IMG_UINT32 ui32Padding1;      /* To ensure correct alignment */
	IMG_UINT32 aui32CountBlksStream[RGX_HWPERF_ZERO_OR_MORE_ELEMENTS]; /*!< Counter data */
	IMG_UINT32 ui32Padding2;      /* To ensure correct alignment */
} RGX_HWPERF_HW_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_HW_DATA);

/*! Mask for use with the aui32CountBlksStream field when decoding the
 * counter block ID and mask word. */
#define RGX_HWPERF_CNTBLK_ID_MASK	0xFFFF0000U
#define RGX_HWPERF_CNTBLK_ID_SHIFT	16U

/*! Obtains the counter block ID from the supplied RGX_HWPERF_HW_DATA address
 * and stream index. May be used in decoding the counter block stream words of
 * a RGX_HWPERF_HW_DATA structure. */
#define RGX_HWPERF_GET_CNTBLK_ID(_data_addr, _idx) ((IMG_UINT16)(((_data_addr)->aui32CountBlksStream[(_idx)]&RGX_HWPERF_CNTBLK_ID_MASK)>>RGX_HWPERF_CNTBLK_ID_SHIFT))
#define RGX_HWPERF_GET_CNTBLK_IDW(_word)           ((IMG_UINT16)(((_word)&RGX_HWPERF_CNTBLK_ID_MASK)>>RGX_HWPERF_CNTBLK_ID_SHIFT))

/*! Obtains the counter mask from the supplied RGX_HWPERF_HW_DATA address
 * and stream index. May be used in decoding the counter block stream words
 * of a RGX_HWPERF_HW_DATA structure. */
#define RGX_HWPERF_GET_CNT_MASK(_data_addr, _idx) ((IMG_UINT16)((_data_addr)->aui32CountBlksStream[(_idx)]&((1<<RGX_CNTBLK_COUNTERS_MAX)-1)))
#define RGX_HWPERF_GET_CNT_MASKW(_word)           ((IMG_UINT16)((_word)&((1<<RGX_CNTBLK_COUNTERS_MAX)-1)))


typedef struct
{
	RGX_HWPERF_DM	eDM;					/*!< DataMaster identifier, see RGX_HWPERF_DM */
	IMG_UINT32		ui32DMContext;			/*!< GPU Data Master (FW) Context */
	IMG_UINT32		ui32FrameNum;			/*!< Frame number */
	IMG_UINT32		ui32TxtActCyc;			/*!< Meta TXTACTCYC register value */
	IMG_UINT32		ui32PerfCycle;			/*!< Cycle count. Used to measure HW context store latency */
	IMG_UINT32		ui32PerfPhase;			/*!< Phase. Used to determine geometry content */
	IMG_UINT32		ui32Padding[2];			/*!< Padding to 8 DWords */
} RGX_HWPERF_CSW_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_CSW_DATA);

/*! Enumeration of clocks supporting this event */
typedef enum
{
	RGX_HWPERF_CLKS_CHG_INVALID = 0,

	RGX_HWPERF_CLKS_CHG_NAME_CORE = 1,

	RGX_HWPERF_CLKS_CHG_LAST,
} RGX_HWPERF_CLKS_CHG_NAME;

/*! This structure holds the data of a clocks change packet. */
typedef struct
{
	IMG_UINT64                ui64NewClockSpeed;         /*!< New Clock Speed (in Hz) */
	RGX_HWPERF_CLKS_CHG_NAME  eClockName;                /*!< Clock name */
	IMG_UINT32                ui32CalibratedClockSpeed;  /*!< Calibrated new GPU clock speed (in Hz) */
	IMG_UINT64                ui64OSTimeStamp;           /*!< OSTimeStamp sampled by the host */
	IMG_UINT64                ui64CRTimeStamp;           /*!< CRTimeStamp sampled by the host and
	                                                          correlated to OSTimeStamp */
} RGX_HWPERF_CLKS_CHG_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_CLKS_CHG_DATA);

/*! Enumeration of GPU utilisation states supported by this event */
typedef IMG_UINT32 RGX_HWPERF_GPU_STATE;

/*! This structure holds the data of a GPU utilisation state change packet. */
typedef struct
{
	RGX_HWPERF_GPU_STATE	eState;		/*!< New GPU utilisation state */
	IMG_UINT32				uiUnused1;	/*!< Padding */
	IMG_UINT32				uiUnused2;	/*!< Padding */
	IMG_UINT32				uiUnused3;	/*!< Padding */
} RGX_HWPERF_GPU_STATE_CHG_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_GPU_STATE_CHG_DATA);


/*! Signature pattern 'HPE1' found in the first word of a PWR_EST packet data */
#define HWPERF_PWR_EST_V1_SIG	0x48504531

/*! Macros to obtain a component field from a counter ID word */
#define RGX_HWPERF_GET_PWR_EST_HIGH_FLAG(_word) (((_word)&0x80000000)>>31)
#define RGX_HWPERF_GET_PWR_EST_UNIT(_word)      (((_word)&0x0F000000)>>24)
#define RGX_HWPERF_GET_PWR_EST_NUMBER(_word)    ((_word)&0x0000FFFF)

/*! This macro constructs a counter ID for a power estimate data stream from
 * the component parts of: high word flag, unit id, counter number */
#define RGX_HWPERF_MAKE_PWR_EST_COUNTERID(_high, _unit, _number) \
			((IMG_UINT32)(((IMG_UINT32)((IMG_UINT32)(_high)&0x1U)<<31) | ((IMG_UINT32)((IMG_UINT32)(_unit)&0xFU)<<24) | \
			              ((_number)&0x0000FFFFU)))

/*! This structure holds the data for a power estimate packet. */
typedef struct
{
	IMG_UINT32  ui32StreamVersion;  /*!< HWPERF_PWR_EST_V1_SIG */
	IMG_UINT32  ui32StreamSize;     /*!< Size of array in bytes of stream data
	                                     held in the aui32StreamData member */
	IMG_UINT32  aui32StreamData[RGX_HWPERF_ONE_OR_MORE_ELEMENTS]; /*!< Counter data */
	IMG_UINT32  ui32Padding; /* To ensure correct alignment */
} RGX_HWPERF_PWR_EST_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_PWR_EST_DATA);

/*! Enumeration of the kinds of power change events that can occur */
typedef enum
{
	RGX_HWPERF_PWR_UNDEFINED   = 0,
	RGX_HWPERF_PWR_ON          = 1, /*!< Whole device powered on */
	RGX_HWPERF_PWR_OFF         = 2, /*!< Whole device powered off */
	RGX_HWPERF_PWR_UP          = 3, /*!< Power turned on to a HW domain */
	RGX_HWPERF_PWR_DOWN        = 4, /*!< Power turned off to a HW domain */
	RGX_HWPERF_PWR_PHR_PARTIAL = 5, /*!< Periodic HW partial Rascal/Dust(S6) Reset */
	RGX_HWPERF_PWR_PHR_FULL    = 6, /*!< Periodic HW full GPU Reset */

	RGX_HWPERF_PWR_LAST,
} RGX_HWPERF_PWR;

/*! This structure holds the data of a power packet. */
typedef struct
{
	RGX_HWPERF_PWR eChange;                  /*!< Defines the type of power change */
	IMG_UINT32     ui32Domains;              /*!< HW Domains affected */
	IMG_UINT64     ui64OSTimeStamp;          /*!< OSTimeStamp sampled by the host */
	IMG_UINT64     ui64CRTimeStamp;          /*!< CRTimeStamp sampled by the host and
	                                              correlated to OSTimeStamp */
	IMG_UINT32     ui32CalibratedClockSpeed; /*!< GPU clock speed (in Hz) at the time
	                                              the two timers were correlated */
	IMG_UINT32     ui32Unused1;              /*!< Padding */
} RGX_HWPERF_PWR_CHG_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_PWR_CHG_DATA);


/*
 * PDVFS, GPU clock frequency changes and workload estimation profiling
 * data.
 */
/*! DVFS and work estimation events. */
typedef enum
{
	RGX_HWPERF_DVFS_EV_INVALID,                 /*! Invalid value. */
	RGX_HWPERF_DVFS_EV_PROACTIVE_EST_START,     /*! Proactive DVFS estimate start */
	RGX_HWPERF_DVFS_EV_PROACTIVE_EST_FINISHED,  /*! Proactive DVFS estimate finished */
	RGX_HWPERF_DVFS_EV_REACTIVE_EST_START,      /*! Reactive DVFS estimate start */
	RGX_HWPERF_DVFS_EV_REACTIVE_EST_FINISHED,   /*! Reactive DVFS estimate finished */
	/* workload estimation */
	RGX_HWPERF_DVFS_EV_WORK_EST_START,          /*! Workload estimation start */
	RGX_HWPERF_DVFS_EV_WORK_EST_FINISHED,       /*! Workload estimation finished */
	RGX_HWPERF_DVFS_EV_FREQ_CHG,                /*! DVFS OPP/clock frequency change */

	RGX_HWPERF_DVFS_EV_LAST               /*! Number of element. */
} RGX_HWPERF_DVFS_EV;

/*! Enumeration of DVFS transitions that can occur */
typedef enum
{
	RGX_HWPERF_DVFS_OPP_NONE        = 0x0,  /*!< No OPP change, already operating at required freq */
#if defined(SUPPORT_PDVFS_IDLE)
	RGX_HWPERF_DVFS_OPP_IDLE        = 0x1,  /*!< GPU is idle, defer the OPP change */
#endif
	/* 0x2 to 0xF reserved */
	RGX_HWPERF_DVFS_OPP_UPDATE      = 0x10, /*!< OPP change, new point is encoded in bits [3:0] */
	RGX_HWPERF_DVFS_OPP_LAST        = 0x20,
} RGX_HWPERF_DVFS_OPP;

typedef union
{
	/*! This structure holds the data of a proactive DVFS calculation packet. */
	struct
	{
		IMG_UINT64     ui64DeadlineInus;         /*!< Next deadline in microseconds */
		IMG_UINT32     ui32Frequency;            /*!< Required freq to meet deadline at 90% utilisation */
		IMG_UINT32     ui32WorkloadCycles;       /*!< Current workload estimate in cycles */
		IMG_UINT32     ui32TxtActCyc;            /*!< Meta TXTACTCYC register value */
	} sProDVFSCalc;

	/*! This structure holds the data of a reactive DVFS calculation packet. */
	struct
	{
		IMG_UINT32     ui32Frequency;            /*!< Required freq to achieve average 90% utilisation */
		IMG_UINT32     ui32Utilisation;          /*!< GPU utilisation since last update */
		IMG_UINT32     ui32TxtActCyc;            /*!< Meta TXTACTCYC register value */
	} sDVFSCalc;

	/*! This structure holds the data of a work estimation packet. */
	struct
	{
		IMG_UINT64     ui64CyclesPrediction;     /*!< Predicted cycle count for this workload */
		IMG_UINT64     ui64CyclesTaken;          /*!< Actual cycle count for this workload */
		RGXFWIF_DM     eDM;                      /*!< Target DM */
		IMG_UINT32     ui32ReturnDataIndex;      /*!< Index into workload estimation table */
		IMG_UINT32     ui32TxtActCyc;            /*!< Meta TXTACTCYC register value */
	} sWorkEst;

	/*! This structure holds the data of an OPP clock frequency transition packet. */
	struct
	{
		IMG_UINT32     ui32OPPData;              /*!< OPP transition */
	} sOPP;

} RGX_HWPERF_DVFS_DETAIL;

typedef struct {
	RGX_HWPERF_DVFS_EV      eEventType;          /*!< DVFS sub-event type */
	RGX_HWPERF_DVFS_DETAIL  uData;               /*!< DVFS sub-event data */
} RGX_HWPERF_DVFS_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_DVFS_DATA);


/*! Firmware Activity event. */
typedef enum
{
	RGX_HWPERF_FWACT_EV_INVALID,            /*! Invalid value. */
	RGX_HWPERF_FWACT_EV_REGS_SET,           /*! Registers set. */
	RGX_HWPERF_FWACT_EV_HWR_DETECTED,       /*! HWR detected. */
	RGX_HWPERF_FWACT_EV_HWR_RESET_REQUIRED, /*! Reset required. */
	RGX_HWPERF_FWACT_EV_HWR_RECOVERED,      /*! HWR recovered. */
	RGX_HWPERF_FWACT_EV_HWR_FREELIST_READY, /*! Freelist ready. */
	RGX_HWPERF_FWACT_EV_FEATURES,           /*! Features present */

	RGX_HWPERF_FWACT_EV_LAST                /*! Number of element. */
} RGX_HWPERF_FWACT_EV;

/*! Cause of the HWR event. */
typedef enum
{
	RGX_HWPERF_HWR_REASON_INVALID,              /*! Invalid value. */
	RGX_HWPERF_HWR_REASON_LOCKUP,               /*! Lockup. */
	RGX_HWPERF_HWR_REASON_PAGEFAULT,            /*! Page fault. */
	RGX_HWPERF_HWR_REASON_POLLFAIL,             /*! Poll fail. */
	RGX_HWPERF_HWR_REASON_DEADLINE_OVERRUN,     /*! Deadline overrun. */
	RGX_HWPERF_HWR_REASON_CSW_DEADLINE_OVERRUN, /*! Hard Context Switch deadline overrun. */

	RGX_HWPERF_HWR_REASON_LAST                  /*! Number of elements. */
} RGX_HWPERF_HWR_REASON;


/* Fixed size for BVNC string so it does not alter packet data format
 * Check it is large enough against official BVNC string length maximum
 */
#define RGX_HWPERF_MAX_BVNC_LEN (24)
static_assert((RGX_HWPERF_MAX_BVNC_LEN >= RGX_BVNC_STR_SIZE_MAX),
			  "Space inside HWPerf packet data for BVNC string insufficient");

#define RGX_HWPERF_MAX_BVNC_BLOCK_LEN (16U)

/*! BVNC Features */
typedef struct
{
	/*! Counter block ID, see RGX_HWPERF_CNTBLK_ID */
	IMG_UINT16 ui16BlockID;

	/*! Number of counters in this block type */
	IMG_UINT16 ui16NumCounters;

	/*! Number of blocks of this type */
	IMG_UINT16 ui16NumBlocks;

	IMG_UINT16 ui16Reserved;
} RGX_HWPERF_BVNC_BLOCK;

/*! BVNC Features */
typedef struct
{
	IMG_CHAR aszBvncString[RGX_HWPERF_MAX_BVNC_LEN]; /*! BVNC string */
	IMG_UINT32 ui32BvncKmFeatureFlags;               /*! See RGX_HWPERF_FEATURE_FLAGS */
	IMG_UINT16 ui16BvncBlocks;                       /*! Number of blocks described in aBvncBlocks */
	IMG_UINT16 ui16Reserved1;                        /*! Align to 32bit */
	RGX_HWPERF_BVNC_BLOCK aBvncBlocks[RGX_HWPERF_MAX_BVNC_BLOCK_LEN]; /*! Supported Performance Blocks for BVNC */
} RGX_HWPERF_BVNC;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_BVNC);

/*! Sub-event's data. */
typedef union
{
	struct
	{
		RGX_HWPERF_DM eDM;				/*!< Data Master ID. */
		RGX_HWPERF_HWR_REASON eReason;	/*!< Reason of the HWR. */
		IMG_UINT32 ui32DMContext;		/*!< FW render context */
	} sHWR;								/*!< HWR sub-event data. */

	RGX_HWPERF_BVNC sBVNC;				/*!< BVNC Features */
} RGX_HWPERF_FWACT_DETAIL;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_FWACT_DETAIL);

/*! This structure holds the data of a FW activity event packet */
typedef struct
{
	RGX_HWPERF_FWACT_EV eEvType;           /*!< Event type. */
	RGX_HWPERF_FWACT_DETAIL uFwActDetail;  /*!< Data of the sub-event. */
	IMG_UINT32 ui32Padding;
} RGX_HWPERF_FWACT_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_FWACT_DATA);



typedef enum {
	RGX_HWPERF_UFO_EV_UPDATE,
	RGX_HWPERF_UFO_EV_CHECK_SUCCESS,
	RGX_HWPERF_UFO_EV_PRCHECK_SUCCESS,
	RGX_HWPERF_UFO_EV_CHECK_FAIL,
	RGX_HWPERF_UFO_EV_PRCHECK_FAIL,
	RGX_HWPERF_UFO_EV_FORCE_UPDATE,

	RGX_HWPERF_UFO_EV_LAST
} RGX_HWPERF_UFO_EV;

/*! Data stream tuple. */
typedef union
{
	struct
	{
		IMG_UINT32 ui32FWAddr;
		IMG_UINT32 ui32Value;
	} sCheckSuccess;
	struct
	{
		IMG_UINT32 ui32FWAddr;
		IMG_UINT32 ui32Value;
		IMG_UINT32 ui32Required;
	} sCheckFail;
	struct
	{
		IMG_UINT32 ui32FWAddr;
		IMG_UINT32 ui32OldValue;
		IMG_UINT32 ui32NewValue;
	} sUpdate;
} RGX_HWPERF_UFO_DATA_ELEMENT;

/*! This structure holds the packet payload data for UFO event. */
typedef struct
{
	RGX_HWPERF_UFO_EV eEvType;
	IMG_UINT32 ui32TimeCorrIndex;
	IMG_UINT32 ui32PID;
	IMG_UINT32 ui32ExtJobRef;
	IMG_UINT32 ui32IntJobRef;
	IMG_UINT32 ui32DMContext;      /*!< GPU Data Master (FW) Context */
	IMG_UINT32 ui32StreamInfo;
	RGX_HWPERF_DM eDM;
	IMG_UINT32 ui32Padding;
	IMG_UINT32 aui32StreamData[RGX_HWPERF_ONE_OR_MORE_ELEMENTS];
} RGX_HWPERF_UFO_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_UFO_DATA);



typedef enum
{
	RGX_HWPERF_KICK_TYPE_TA3D, /*!< Replaced by separate TA and 3D types */
	RGX_HWPERF_KICK_TYPE_TQ2D,
	RGX_HWPERF_KICK_TYPE_TQ3D,
	RGX_HWPERF_KICK_TYPE_CDM,
	RGX_HWPERF_KICK_TYPE_RS,
	RGX_HWPERF_KICK_TYPE_VRDM,
	RGX_HWPERF_KICK_TYPE_TQTDM,
	RGX_HWPERF_KICK_TYPE_SYNC,
	RGX_HWPERF_KICK_TYPE_TA,
	RGX_HWPERF_KICK_TYPE_3D,
	RGX_HWPERF_KICK_TYPE_LAST,

	RGX_HWPERF_KICK_TYPE_FORCE_32BIT = 0x7fffffff
} RGX_HWPERF_KICK_TYPE;

typedef struct
{
	RGX_HWPERF_KICK_TYPE ui32EnqType;
	IMG_UINT32 ui32PID;
	IMG_UINT32 ui32ExtJobRef;
	IMG_UINT32 ui32IntJobRef;
	IMG_UINT32 ui32DMContext;        /*!< GPU Data Master (FW) Context */
	IMG_UINT32 ui32Padding;
	IMG_UINT64 ui64CheckFence_UID;
	IMG_UINT64 ui64UpdateFence_UID;
	IMG_UINT64 ui64DeadlineInus;     /*!< Workload deadline in system monotonic time */
	IMG_UINT64 ui64CycleEstimate;    /*!< Estimated cycle time for the workload */
	PVRSRV_FENCE hCheckFence;        /*!< Fence this enqueue task waits for, before starting */
	PVRSRV_FENCE hUpdateFence;       /*!< Fence this enqueue task signals, on completion */
	PVRSRV_TIMELINE hUpdateTimeline; /*!< Timeline on which the above hUpdateFence is created */

	IMG_UINT32 ui32Pad;              /* Align structure size to 8 bytes */
} RGX_HWPERF_HOST_ENQ_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_ENQ_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_ENQ_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef struct
{
	RGX_HWPERF_UFO_EV eEvType;
	IMG_UINT32 ui32StreamInfo;
	IMG_UINT32 aui32StreamData[RGX_HWPERF_ONE_OR_MORE_ELEMENTS];
	IMG_UINT32 ui32Padding;      /* Align structure size to 8 bytes */
} RGX_HWPERF_HOST_UFO_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_UFO_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_UFO_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef enum
{
	RGX_HWPERF_HOST_RESOURCE_TYPE_INVALID,
	RGX_HWPERF_HOST_RESOURCE_TYPE_SYNC, /* PRIM */
	RGX_HWPERF_HOST_RESOURCE_TYPE_TIMELINE_DEPRECATED, /* Timeline resource packets are now
	                                                      emitted in client hwperf buffer */
	RGX_HWPERF_HOST_RESOURCE_TYPE_FENCE_PVR, /* Fence for use on GPU (SYNC_CP backed) */
	RGX_HWPERF_HOST_RESOURCE_TYPE_SYNC_CP,
	RGX_HWPERF_HOST_RESOURCE_TYPE_FENCE_SW, /* Fence created on SW timeline */

	RGX_HWPERF_HOST_RESOURCE_TYPE_LAST
} RGX_HWPERF_HOST_RESOURCE_TYPE;

typedef union
{
	struct
	{
		IMG_UINT32 uiPid;
		IMG_UINT64 ui64Timeline_UID1;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
		IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	} sTimelineAlloc;

	struct
	{
		IMG_PID uiPID;
		PVRSRV_FENCE hFence;
		IMG_UINT32 ui32CheckPt_FWAddr;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
	} sFenceAlloc;

	struct
	{
		IMG_UINT32 ui32CheckPt_FWAddr;
		PVRSRV_TIMELINE hTimeline;
		IMG_PID uiPID;
		PVRSRV_FENCE hFence;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
	} sSyncCheckPointAlloc;

	struct
	{
		IMG_PID uiPID;
		PVRSRV_FENCE hSWFence;
		PVRSRV_TIMELINE hSWTimeline;
		IMG_UINT64 ui64SyncPtIndex;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
	} sSWFenceAlloc;

	struct
	{
		IMG_UINT32 ui32FWAddr;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
	} sSyncAlloc;
} RGX_HWPERF_HOST_ALLOC_DETAIL;

typedef struct
{
	RGX_HWPERF_HOST_RESOURCE_TYPE ui32AllocType;
	RGX_HWPERF_HOST_ALLOC_DETAIL RGXFW_ALIGN uAllocDetail;
} RGX_HWPERF_HOST_ALLOC_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_ALLOC_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_ALLOC_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef union
{
	struct
	{
		IMG_UINT32 uiPid;
		IMG_UINT64 ui64Timeline_UID1;
		IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	} sTimelineDestroy;

	struct
	{
		IMG_UINT64 ui64Fence_UID;
		IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	} sFenceDestroy;

	struct
	{
		IMG_UINT32 ui32CheckPt_FWAddr;
	} sSyncCheckPointFree;

	struct
	{
		IMG_UINT32 ui32FWAddr;
	} sSyncFree;
} RGX_HWPERF_HOST_FREE_DETAIL;

typedef struct
{
	RGX_HWPERF_HOST_RESOURCE_TYPE ui32FreeType;
	RGX_HWPERF_HOST_FREE_DETAIL uFreeDetail;
	IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
} RGX_HWPERF_HOST_FREE_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_FREE_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_FREE_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef struct
{
	IMG_UINT64 ui64CRTimestamp;
	IMG_UINT64 ui64OSTimestamp;
	IMG_UINT32 ui32ClockSpeed;
	IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
} RGX_HWPERF_HOST_CLK_SYNC_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_CLK_SYNC_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_CLK_SYNC_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef union
{
	struct
	{
		IMG_UINT64 ui64NewFence_UID;
		IMG_UINT64 ui64InFence1_UID;
		IMG_UINT64 ui64InFence2_UID;
		IMG_CHAR acName[PVRSRV_SYNC_NAME_LENGTH];
		IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	} sFenceMerge;
} RGX_HWPERF_HOST_MODIFY_DETAIL;

typedef struct
{
	RGX_HWPERF_HOST_RESOURCE_TYPE ui32ModifyType;
	RGX_HWPERF_HOST_MODIFY_DETAIL uModifyDetail;
} RGX_HWPERF_HOST_MODIFY_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_MODIFY_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_MODIFY_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef enum
{
	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_UNDEFINED = 0,
	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_OK,
	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_RESPONDING,
	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_DEAD,
	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_FAULT,

	RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS_LAST
} RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS;

typedef enum
{
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_UNDEFINED = 0,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_NONE,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_ASSERTED,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_POLL_FAILING,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_TIMEOUTS,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_QUEUE_CORRUPT,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_QUEUE_STALLED,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_IDLING,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_RESTARTING,
	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_MISSING_INTERRUPTS,

	RGX_HWPERF_HOST_DEVICE_HEALTH_REASON_LAST
} RGX_HWPERF_HOST_DEVICE_HEALTH_REASON;

typedef enum
{
	RGX_HWPERF_DEV_INFO_EV_HEALTH,

	RGX_HWPERF_DEV_INFO_EV_LAST
} RGX_HWPERF_DEV_INFO_EV;

typedef union
{
	struct
	{
		RGX_HWPERF_HOST_DEVICE_HEALTH_STATUS eDeviceHealthStatus;
		RGX_HWPERF_HOST_DEVICE_HEALTH_REASON eDeviceHealthReason;
	} sDeviceStatus;
} RGX_HWPERF_HOST_DEV_INFO_DETAIL;

typedef struct
{
	IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	RGX_HWPERF_DEV_INFO_EV eEvType;
	RGX_HWPERF_HOST_DEV_INFO_DETAIL	uDevInfoDetail;
} RGX_HWPERF_HOST_DEV_INFO_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_DEV_INFO_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			 "sizeof(RGX_HWPERF_HOST_DEV_INFO_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef enum
{
	RGX_HWPERF_INFO_EV_MEM_USAGE,

	RGX_HWPERF_INFO_EV_LAST
} RGX_HWPERF_INFO_EV;

typedef union
{
	struct
	{
		IMG_UINT32 ui32TotalMemoryUsage;
		struct
		{
			IMG_UINT32 ui32Pid;
			IMG_UINT32 ui32KernelMemUsage;
			IMG_UINT32 ui32GraphicsMemUsage;
		} sPerProcessUsage[RGX_HWPERF_ZERO_OR_MORE_ELEMENTS];
	} sMemUsageStats;
} RGX_HWPERF_HOST_INFO_DETAIL;

typedef struct
{
	IMG_UINT32 ui32Padding;       /* Align structure size to 8 bytes */
	RGX_HWPERF_INFO_EV eEvType;
	RGX_HWPERF_HOST_INFO_DETAIL uInfoDetail;
} RGX_HWPERF_HOST_INFO_DATA;

/* Payload size must be multiple of 8 bytes to align start of next packet. */
static_assert((sizeof(RGX_HWPERF_HOST_INFO_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_INFO_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef enum
{
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_TYPE_BEGIN = 0,
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_TYPE_END,

	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_TYPE_LAST,
} RGX_HWPERF_HOST_SYNC_FENCE_WAIT_TYPE;

typedef enum
{
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT_INVALID = 0,
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT_TIMEOUT,
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT_PASSED,
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT_ERROR,

	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT_LAST,
} RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT;

typedef union
{
	struct
	{
		IMG_UINT32 ui32TimeoutInMs;
	} sBegin;

	struct
	{
		RGX_HWPERF_HOST_SYNC_FENCE_WAIT_RESULT eResult;
	} sEnd;
} RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DETAIL;

typedef struct
{
	IMG_PID uiPID;
	PVRSRV_FENCE hFence;
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_TYPE eType;
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DETAIL uDetail;

} RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DATA;

static_assert((sizeof(RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef struct
{
	IMG_PID uiPID;
	PVRSRV_TIMELINE hTimeline;
	IMG_UINT64 ui64SyncPtIndex;

} RGX_HWPERF_HOST_SYNC_SW_TL_ADV_DATA;

static_assert((sizeof(RGX_HWPERF_HOST_SYNC_SW_TL_ADV_DATA) & (PVRSRVTL_PACKET_ALIGNMENT-1U)) == 0U,
			  "sizeof(RGX_HWPERF_HOST_SYNC_SW_TL_ADV_DATA) must be a multiple PVRSRVTL_PACKET_ALIGNMENT");

typedef enum
{
	RGX_HWPERF_RESOURCE_CAPTURE_TYPE_NONE,
	RGX_HWPERF_RESOURCE_CAPTURE_TYPE_DEFAULT_FRAMEBUFFER,
	RGX_HWPERF_RESOURCE_CAPTURE_TYPE_OFFSCREEN_FB_ATTACHMENTS,
	RGX_HWPERF_RESOURCE_CAPTURE_TYPE_TILE_LIFETIME_DATA,

	RGX_HWPERF_RESOURCE_TYPE_COUNT
} RGX_HWPERF_RESOURCE_CAPTURE_TYPE;

typedef struct
{
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32BPP;
	IMG_UINT32 ui32PixFormat;
} RGX_RESOURCE_PER_SURFACE_INFO, *PRGX_RESOURCE_PER_SURFACE_INFO;

typedef struct
{
	IMG_INT32  i32XOffset;        /*!< render surface X shift */
	IMG_INT32  i32YOffset;        /*!< render surface Y shift */
	IMG_UINT32 ui32WidthInTiles;  /*!< number of TLT data points in X */
	IMG_UINT32 ui32HeightInTiles; /*!< number of TLT data points in Y */
} RGX_RESOURCE_PER_TLT_BUFFER_INFO, *PRGX_RESOURCE_PER_TLT_BUFFER_INFO;

typedef union
{
	struct RGX_RESOURCE_CAPTURE_RENDER_SURFACES
	{
		IMG_UINT32 ui32RenderSurfaceCount;
		RGX_RESOURCE_PER_SURFACE_INFO sSurface[RGX_HWPERF_ONE_OR_MORE_ELEMENTS];
	} sRenderSurfaces;

	struct RGX_RESOURCE_CAPTURE_TILE_LIFETIME_BUFFERS
	{
		RGX_RESOURCE_PER_TLT_BUFFER_INFO sTLTBufInfo[RGX_HWPERF_ONE_OR_MORE_ELEMENTS];
	} sTLTBuffers;
} RGX_RESOURCE_CAPTURE_DETAIL;

typedef struct
{
	RGX_HWPERF_RESOURCE_CAPTURE_TYPE eType;
	IMG_PID uPID;
	IMG_UINT32 ui32ContextID;
	IMG_UINT32 ui32FrameNum;
	IMG_UINT32 ui32CapturedTaskJobRef;	/* The job ref of the HW task that emitted the data */
	IMG_INT32 eClientModule;			/* RGX_HWPERF_CLIENT_API - ID that the capture is originating from. */
	RGX_RESOURCE_CAPTURE_DETAIL uDetail; /* eType determines the value of the union */
} RGX_RESOURCE_CAPTURE_INFO, *PRGX_RESOURCE_CAPTURE_INFO;

#define RGX_RESOURCE_CAPTURE_INFO_BASE_SIZE() offsetof(RGX_RESOURCE_CAPTURE_INFO, uDetail)

#define RGX_TLT_HARDWARE_HDR_SIZE   (16U)

/* PVRSRVGetHWPerfResourceCaptureResult */
typedef enum
{
	RGX_HWPERF_RESOURCE_CAPTURE_RESULT_NONE = 0,
	RGX_HWPERF_RESOURCE_CAPTURE_RESULT_OK,					/* We got data ok, expect more packets for this request. */
	RGX_HWPERF_RESOURCE_CAPTURE_RESULT_NOT_READY,			/* Signals a timeout on the connection - no data available yet. */
	RGX_HWPERF_RESOURCE_CAPTURE_RESULT_COMPLETE_SUCCESS,	/* The request completed successfully, signals the end of packets for the request. */
	RGX_HWPERF_RESOURCE_CAPTURE_RESULT_COMPLETE_FAILURE		/* The request failed, signals the end of packets for the request. */
} RGX_HWPERF_RESOURCE_CAPTURE_RESULT_STATUS;

typedef struct
{
	IMG_PID uPID;						/* In case of a failed request pass the caller the PID and context ID. */
	IMG_UINT32 ui32CtxID;
	RGX_RESOURCE_CAPTURE_INFO *psInfo;	/* Various meta-data regarding the captured resource which aid the requester when,
											unpacking the resource data, valid if RGX_HWPERF_RESOURCE_CAPTURE_RESULT_OK is returned. */
	IMG_BYTE *pbData;					/* Buffer containing the captured resource data, valid if RGX_HWPERF_RESOURCE_CAPTURE_RESULT_OK is returned. */
} RGX_RESOURCE_CAPTURE_RESULT;

/*! This type is a union of packet payload data structures associated with
 * various FW and Host events */
typedef union
{
	RGX_HWPERF_FW_DATA             sFW;           /*!< Firmware event packet data */
	RGX_HWPERF_HW_DATA             sHW;           /*!< Hardware event packet data */
	RGX_HWPERF_CLKS_CHG_DATA       sCLKSCHG;      /*!< Clock change event packet data */
	RGX_HWPERF_GPU_STATE_CHG_DATA  sGPUSTATECHG;  /*!< GPU utilisation state change event packet data */
	RGX_HWPERF_PWR_EST_DATA        sPWREST;       /*!< Power estimate event packet data */
	RGX_HWPERF_PWR_CHG_DATA        sPWR;          /*!< Power event packet data */
	RGX_HWPERF_CSW_DATA            sCSW;          /*!< Context switch packet data */
	RGX_HWPERF_UFO_DATA            sUFO;          /*!< UFO data */
	RGX_HWPERF_FWACT_DATA          sFWACT;        /*!< Firmware activity event packet data */
	RGX_HWPERF_DVFS_DATA           sDVFS;         /*!< DVFS activity data */
	/* */
	RGX_HWPERF_HOST_ENQ_DATA       sENQ;          /*!< Host ENQ data */
	RGX_HWPERF_HOST_UFO_DATA       sHUFO;         /*!< Host UFO data */
	RGX_HWPERF_HOST_ALLOC_DATA     sHALLOC;       /*!< Host Alloc data */
	RGX_HWPERF_HOST_CLK_SYNC_DATA  sHCLKSYNC;     /*!< Host CLK_SYNC data */
	RGX_HWPERF_HOST_FREE_DATA      sHFREE;        /*!< Host Free data */
	RGX_HWPERF_HOST_MODIFY_DATA    sHMOD;         /*!< Host Modify data */
	RGX_HWPERF_HOST_DEV_INFO_DATA  sHDEVINFO;	  /*!< Host device info data */
	RGX_HWPERF_HOST_INFO_DATA      sHINFO;        /*!< Host info data */
	RGX_HWPERF_HOST_SYNC_FENCE_WAIT_DATA sWAIT;   /*!< Host fence-wait data */
	RGX_HWPERF_HOST_SYNC_SW_TL_ADV_DATA sSWTLADV; /*!< Host SW-timeline advance data */
} RGX_HWPERF_V2_PACKET_DATA_, *RGX_PHWPERF_V2_PACKET_DATA;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_V2_PACKET_DATA_);

#define RGX_HWPERF_GET_PACKET_DATA(_packet_addr) ((RGX_PHWPERF_V2_PACKET_DATA) (IMG_OFFSET_ADDR((_packet_addr), sizeof(RGX_HWPERF_V2_PACKET_HDR))))

#define RGX_HWPERF_GET_DVFS_EVENT_TYPE_PTR(_packet_addr)	\
	((RGX_HWPERF_DVFS_EV*) (IMG_OFFSET_ADDR((_packet_addr), sizeof(RGX_HWPERF_V2_PACKET_HDR) + offsetof(RGX_HWPERF_DVFS_DATA,eEventType))))

/******************************************************************************
 * API Types
 *****************************************************************************/

/*! Counter block IDs for all the hardware blocks with counters.
 * Directly addressable blocks must have a value between 0..15.
 * First hex digit represents a group number and the second hex digit
 * represents the unit within the group. Group 0 is the direct group,
 * all others are indirect groups.
 */
typedef IMG_UINT32 RGX_HWPERF_CNTBLK_ID;

/* Directly addressable counter blocks */
#define	RGX_CNTBLK_ID_TA			 0x0000U
#define	RGX_CNTBLK_ID_RASTER		 0x0001U /* Non-cluster grouping cores */
#define	RGX_CNTBLK_ID_HUB			 0x0002U /* Non-cluster grouping cores */
#define	RGX_CNTBLK_ID_TORNADO		 0x0003U /* XT cores */
#define	RGX_CNTBLK_ID_JONES			 0x0004U /* S7 cores */
#define	RGX_CNTBLK_ID_BF			 0x0005U /* Doppler unit */
#define	RGX_CNTBLK_ID_BT			 0x0006U /* Doppler unit */
#define	RGX_CNTBLK_ID_RT			 0x0007U /* Doppler unit */
#define	RGX_CNTBLK_ID_SH			 0x0008U /* Ray tracing unit */

#define	RGX_CNTBLK_ID_DIRECT_LAST	 0x0009U

/* Indirectly addressable counter blocks */
#define	RGX_CNTBLK_ID_TPU_MCU0		 0x0010U /* Addressable by Dust */
#define	RGX_CNTBLK_ID_TPU_MCU1		 0x0011U
#define	RGX_CNTBLK_ID_TPU_MCU2		 0x0012U
#define	RGX_CNTBLK_ID_TPU_MCU3		 0x0013U
#define	RGX_CNTBLK_ID_TPU_MCU4		 0x0014U
#define	RGX_CNTBLK_ID_TPU_MCU5		 0x0015U
#define	RGX_CNTBLK_ID_TPU_MCU6		 0x0016U
#define	RGX_CNTBLK_ID_TPU_MCU7		 0x0017U
#define	RGX_CNTBLK_ID_TPU_MCU_ALL	 0x4010U

#define	RGX_CNTBLK_ID_USC0			 0x0020U /* Addressable by Cluster */
#define	RGX_CNTBLK_ID_USC1			 0x0021U
#define	RGX_CNTBLK_ID_USC2			 0x0022U
#define	RGX_CNTBLK_ID_USC3			 0x0023U
#define	RGX_CNTBLK_ID_USC4			 0x0024U
#define	RGX_CNTBLK_ID_USC5			 0x0025U
#define	RGX_CNTBLK_ID_USC6			 0x0026U
#define	RGX_CNTBLK_ID_USC7			 0x0027U
#define	RGX_CNTBLK_ID_USC8			 0x0028U
#define	RGX_CNTBLK_ID_USC9			 0x0029U
#define	RGX_CNTBLK_ID_USC10			 0x002AU
#define	RGX_CNTBLK_ID_USC11			 0x002BU
#define	RGX_CNTBLK_ID_USC12			 0x002CU
#define	RGX_CNTBLK_ID_USC13			 0x002DU
#define	RGX_CNTBLK_ID_USC14			 0x002EU
#define	RGX_CNTBLK_ID_USC15			 0x002FU
#define	RGX_CNTBLK_ID_USC_ALL		 0x4020U

#define	RGX_CNTBLK_ID_TEXAS0		 0x0030U /* Addressable by Phantom in XT, Dust in S7 */
#define	RGX_CNTBLK_ID_TEXAS1		 0x0031U
#define	RGX_CNTBLK_ID_TEXAS2		 0x0032U
#define	RGX_CNTBLK_ID_TEXAS3		 0x0033U
#define	RGX_CNTBLK_ID_TEXAS4		 0x0034U
#define	RGX_CNTBLK_ID_TEXAS5		 0x0035U
#define	RGX_CNTBLK_ID_TEXAS6		 0x0036U
#define	RGX_CNTBLK_ID_TEXAS7		 0x0037U
#define	RGX_CNTBLK_ID_TEXAS_ALL		 0x4030U

#define	RGX_CNTBLK_ID_RASTER0		 0x0040U /* Addressable by Phantom, XT only */
#define	RGX_CNTBLK_ID_RASTER1		 0x0041U
#define	RGX_CNTBLK_ID_RASTER2		 0x0042U
#define	RGX_CNTBLK_ID_RASTER3		 0x0043U
#define	RGX_CNTBLK_ID_RASTER_ALL	 0x4040U

#define	RGX_CNTBLK_ID_BLACKPEARL0	 0x0050U /* Addressable by Phantom, S7, only */
#define	RGX_CNTBLK_ID_BLACKPEARL1	 0x0051U
#define	RGX_CNTBLK_ID_BLACKPEARL2	 0x0052U
#define	RGX_CNTBLK_ID_BLACKPEARL3	 0x0053U
#define	RGX_CNTBLK_ID_BLACKPEARL_ALL 0x4050U

#define	RGX_CNTBLK_ID_PBE0			 0x0060U /* Addressable by Cluster in S7 and PBE2_IN_XE */
#define	RGX_CNTBLK_ID_PBE1			 0x0061U
#define	RGX_CNTBLK_ID_PBE2			 0x0062U
#define	RGX_CNTBLK_ID_PBE3			 0x0063U
#define	RGX_CNTBLK_ID_PBE4			 0x0064U
#define	RGX_CNTBLK_ID_PBE5			 0x0065U
#define	RGX_CNTBLK_ID_PBE6			 0x0066U
#define	RGX_CNTBLK_ID_PBE7			 0x0067U
#define	RGX_CNTBLK_ID_PBE8			 0x0068U
#define	RGX_CNTBLK_ID_PBE9			 0x0069U
#define	RGX_CNTBLK_ID_PBE10			 0x006AU
#define	RGX_CNTBLK_ID_PBE11			 0x006BU
#define	RGX_CNTBLK_ID_PBE12			 0x006CU
#define	RGX_CNTBLK_ID_PBE13			 0x006DU
#define	RGX_CNTBLK_ID_PBE14			 0x006EU
#define	RGX_CNTBLK_ID_PBE15			 0x006FU
#define	RGX_CNTBLK_ID_PBE_ALL		 0x4060U

#define	RGX_CNTBLK_ID_BX_TU0		 0x0070U /* Doppler unit, XT only */
#define	RGX_CNTBLK_ID_BX_TU1		 0x0071U
#define	RGX_CNTBLK_ID_BX_TU2		 0x0072U
#define	RGX_CNTBLK_ID_BX_TU3		 0x0073U
#define	RGX_CNTBLK_ID_BX_TU_ALL		 0x4070U

#define	RGX_CNTBLK_ID_LAST			 0x0074U

#define	RGX_CNTBLK_ID_CUSTOM0		 0x7FF0U
#define	RGX_CNTBLK_ID_CUSTOM1		 0x7FF1U
#define	RGX_CNTBLK_ID_CUSTOM2		 0x7FF2U
#define	RGX_CNTBLK_ID_CUSTOM3		 0x7FF3U
#define	RGX_CNTBLK_ID_CUSTOM4_FW	 0x7FF4U /* Custom block used for getting statistics held in the FW */


/* Masks for the counter block ID*/
#define RGX_CNTBLK_ID_GROUP_MASK     (0x00F0U)
#define RGX_CNTBLK_ID_GROUP_SHIFT    (4U)
#define RGX_CNTBLK_ID_UNIT_ALL_MASK  (0x4000U)
#define RGX_CNTBLK_ID_UNIT_MASK		 (0xfU)

#define RGX_CNTBLK_INDIRECT_COUNT(_class, _n) ((IMG_UINT32)(RGX_CNTBLK_ID_ ## _class ## _n) - (IMG_UINT32)(RGX_CNTBLK_ID_ ## _class ## 0) + 1u)

/*! The number of layout blocks defined with configurable multiplexed
 * performance counters, hence excludes custom counter blocks.
 */
#define RGX_HWPERF_MAX_DEFINED_BLKS  (\
	(IMG_UINT32)RGX_CNTBLK_ID_DIRECT_LAST    +\
	RGX_CNTBLK_INDIRECT_COUNT(TPU_MCU,     7)+\
	RGX_CNTBLK_INDIRECT_COUNT(USC,        15)+\
	RGX_CNTBLK_INDIRECT_COUNT(TEXAS,       7)+\
	RGX_CNTBLK_INDIRECT_COUNT(RASTER,      3)+\
	RGX_CNTBLK_INDIRECT_COUNT(BLACKPEARL,  3)+\
	RGX_CNTBLK_INDIRECT_COUNT(PBE,        15)+\
	RGX_CNTBLK_INDIRECT_COUNT(BX_TU,       3) )

static_assert(
	((RGX_CNTBLK_ID_DIRECT_LAST + ((RGX_CNTBLK_ID_LAST & RGX_CNTBLK_ID_GROUP_MASK) >> RGX_CNTBLK_ID_GROUP_SHIFT)) <= RGX_HWPERF_MAX_BVNC_BLOCK_LEN),
	"RGX_HWPERF_MAX_BVNC_BLOCK_LEN insufficient");

#define RGX_HWPERF_EVENT_MASK_VALUE(e)      (IMG_UINT64_C(1) << (e))

#define RGX_CUSTOM_FW_CNTRS	\
                X(TA_LOCAL_FL_SIZE,    0x0, RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_TAKICK) |         \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_PMOOM_TAPAUSE) |  \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_PMOOM_TARESUME) | \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_TAFINISHED))      \
                                                                                                        \
                X(TA_GLOBAL_FL_SIZE,   0x1, RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_TAKICK) |         \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_PMOOM_TAPAUSE) |  \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_PMOOM_TARESUME) | \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_TAFINISHED))      \
                                                                                                        \
                X(3D_LOCAL_FL_SIZE,    0x2, RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DKICK) |         \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DFINISHED))      \
                                                                                                        \
                X(3D_GLOBAL_FL_SIZE,   0x3, RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DKICK) |         \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DFINISHED))      \
                                                                                                        \
                X(ISP_TILES_IN_FLIGHT, 0x4, RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DKICK) |         \
                                            RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_3DSPMKICK))

/*! Counter IDs for the firmware held statistics */
typedef enum
{
#define X(ctr, id, allow_mask)	RGX_CUSTOM_FW_CNTR_##ctr = id,
	RGX_CUSTOM_FW_CNTRS
#undef X

	/* always the last entry in the list */
	RGX_CUSTOM_FW_CNTR_LAST
} RGX_HWPERF_CUSTOM_FW_CNTR_ID;

/*! Identifier for each counter in a performance counting module */
typedef IMG_UINT32 RGX_HWPERF_CNTBLK_COUNTER_ID;

#define	RGX_CNTBLK_COUNTER0_ID 0U
#define	RGX_CNTBLK_COUNTER1_ID 1U
#define	RGX_CNTBLK_COUNTER2_ID 2U
#define	RGX_CNTBLK_COUNTER3_ID 3U
#define	RGX_CNTBLK_COUNTER4_ID 4U
#define	RGX_CNTBLK_COUNTER5_ID 5U
	/* MAX value used in server handling of counter config arrays */
#define	RGX_CNTBLK_COUNTERS_MAX 6U


/* sets all the bits from bit _b1 to _b2, in a IMG_UINT64 type */
#define MASK_RANGE_IMPL(b1, b2)	((IMG_UINT64)((IMG_UINT64_C(1) << ((IMG_UINT32)(b2)-(IMG_UINT32)(b1) + 1U)) - 1U) << (IMG_UINT32)(b1))
#define MASK_RANGE(R)			MASK_RANGE_IMPL(R##_FIRST_TYPE, R##_LAST_TYPE)
#define RGX_HWPERF_HOST_EVENT_MASK_VALUE(e) (IMG_UINT32_C(1) << (e))

/*! Mask macros for use with RGXCtrlHWPerf() API.
 */
#define RGX_HWPERF_EVENT_MASK_NONE          (IMG_UINT64_C(0x0000000000000000))
#define RGX_HWPERF_EVENT_MASK_ALL           (IMG_UINT64_C(0xFFFFFFFFFFFFFFFF))

/*! HWPerf Firmware event masks
 * Next macro covers all FW Start/End/Debug (SED) events.
 */
#define RGX_HWPERF_EVENT_MASK_FW_SED    (MASK_RANGE(RGX_HWPERF_FW_EVENT_RANGE))

#define RGX_HWPERF_EVENT_MASK_FW_UFO    (RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_UFO))
#define RGX_HWPERF_EVENT_MASK_FW_CSW    (RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_CSW_START) |\
                                          RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_CSW_FINISHED))
#define RGX_HWPERF_EVENT_MASK_ALL_FW    (RGX_HWPERF_EVENT_MASK_FW_SED |\
                                          RGX_HWPERF_EVENT_MASK_FW_UFO |\
                                          RGX_HWPERF_EVENT_MASK_FW_CSW)

#define RGX_HWPERF_EVENT_MASK_HW_PERIODIC   (RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_HW_PERIODIC))
#define RGX_HWPERF_EVENT_MASK_HW_KICKFINISH ((MASK_RANGE(RGX_HWPERF_HW_EVENT_RANGE0) |\
                                               MASK_RANGE(RGX_HWPERF_HW_EVENT_RANGE1)) &\
                                              ~(RGX_HWPERF_EVENT_MASK_HW_PERIODIC))

#define RGX_HWPERF_EVENT_MASK_ALL_HW        (RGX_HWPERF_EVENT_MASK_HW_KICKFINISH |\
                                              RGX_HWPERF_EVENT_MASK_HW_PERIODIC)

#define RGX_HWPERF_EVENT_MASK_ALL_PWR_EST   (MASK_RANGE(RGX_HWPERF_PWR_EST_RANGE))

#define RGX_HWPERF_EVENT_MASK_ALL_PWR       (RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_CLKS_CHG) |\
                                              RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_GPU_STATE_CHG) |\
                                              RGX_HWPERF_EVENT_MASK_VALUE(RGX_HWPERF_PWR_CHG))

/*! HWPerf Host event masks
 */
#define RGX_HWPERF_EVENT_MASK_HOST_WORK_ENQ  (RGX_HWPERF_HOST_EVENT_MASK_VALUE(RGX_HWPERF_HOST_ENQ))
#define RGX_HWPERF_EVENT_MASK_HOST_ALL_UFO   (RGX_HWPERF_HOST_EVENT_MASK_VALUE(RGX_HWPERF_HOST_UFO))
#define RGX_HWPERF_EVENT_MASK_HOST_ALL_PWR   (RGX_HWPERF_HOST_EVENT_MASK_VALUE(RGX_HWPERF_HOST_CLK_SYNC))


/*! Type used in the RGX API RGXConfigureAndEnableHWPerfCounters() */
typedef struct
{
	/*! Counter block ID, see RGX_HWPERF_CNTBLK_ID */
	IMG_UINT16 ui16BlockID;

	/*! 4 or 6 LSBs used to select counters to configure in this block. */
	IMG_UINT8  ui8CounterSelect;

	/*! 4 or 6 LSBs used as MODE bits for the counters in the group. */
	IMG_UINT8  ui8Mode;

	/*! 5 or 6 LSBs used as the GROUP_SELECT value for the counter. */
	IMG_UINT8  aui8GroupSelect[RGX_CNTBLK_COUNTERS_MAX];

	/*! 16 LSBs used as the BIT_SELECT value for the counter. */
	IMG_UINT16 aui16BitSelect[RGX_CNTBLK_COUNTERS_MAX];

	/*! 14 LSBs used as the BATCH_MAX value for the counter. */
	IMG_UINT32 aui32BatchMax[RGX_CNTBLK_COUNTERS_MAX];

	/*! 14 LSBs used as the BATCH_MIN value for the counter. */
	IMG_UINT32 aui32BatchMin[RGX_CNTBLK_COUNTERS_MAX];
} UNCACHED_ALIGN RGX_HWPERF_CONFIG_CNTBLK;

RGX_FW_STRUCT_SIZE_ASSERT(RGX_HWPERF_CONFIG_CNTBLK);


#if defined(__cplusplus)
}
#endif

#endif /* RGX_HWPERF_H_ */

/******************************************************************************
 End of file
******************************************************************************/

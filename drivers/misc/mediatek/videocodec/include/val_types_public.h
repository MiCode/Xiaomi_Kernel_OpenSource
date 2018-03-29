/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VAL_TYPES_PUBLIC_H_
#define _VAL_TYPES_PUBLIC_H_

/* #include <sys/types.h> */
/* #include <linux/ion.h> */

#ifdef __cplusplus
extern "C" {
#endif

#define IRQ_STATUS_MAX_NUM 16                   /* /< support max 16 return register values when HW done */

#define VCODEC_THREAD_MAX_NUM 16                /* /< support max 16 multiple thread currently */

/*=============================================================================
 *                              Type definition
 *===========================================================================*/

typedef void                VAL_VOID_T;         /* /< void type definition */
typedef char                VAL_BOOL_T;         /* /< char type definition */
typedef char                VAL_CHAR_T;         /* /< char type definition */
typedef signed char         VAL_INT8_T;         /* /< signed char type definition */
typedef signed short        VAL_INT16_T;        /* /< signed short type definition */
typedef signed int         VAL_INT32_T;        /* /< signed int type definition */
typedef unsigned char       VAL_UCHAR_T;        /* /< unsigned char type definition */
typedef unsigned char       VAL_UINT8_T;        /* /< unsigned char type definition */
typedef unsigned short      VAL_UINT16_T;       /* /< unsigned short definition */
typedef unsigned int       VAL_UINT32_T;       /* /< unsigned int type definition */
typedef unsigned long long  VAL_UINT64_T;       /* /< unsigned long long type definition */
typedef long long           VAL_INT64_T;        /* /< long long type definition */
typedef unsigned long       VAL_HANDLE_T;       /* /< unsigned int (handle) type definition */
typedef signed long         VAL_LONG_T;       /* / */
typedef unsigned long       VAL_ULONG_T;       /* / */

#define VAL_NULL        (0)                     /* /< VAL_NULL = 0 */
#define VAL_TRUE        (1)                     /* /< VAL_TRUE = 1 */
#define VAL_FALSE       (0)                     /* /< VAL_FALSE = 0 */

/* /< VAL_RESOLUTION_CHANGED = 2, used to video resolution changed during playback */
#define VAL_RESOLUTION_CHANGED       (2)

/**
 * @par Enumeration
 *   VAL_MEM_CODEC_T
 * @par Description
 *   This is the item used to memory usage for video encoder or video decoder
 */
typedef enum _VAL_MEM_CODEC_T {
	VAL_MEM_CODEC_FOR_VENC = 0,                 /* /< Memory for Video Encoder */
	VAL_MEM_CODEC_FOR_VDEC,                     /* /< Memory for Video Decoder */
	VAL_MEM_CODEC_MAX = 0xFFFFFFFF              /* /< Max Value */
} VAL_MEM_CODEC_T;


/**
 * @par Enumeration
 *   VAL_CHIP_NAME_T
 * @par Description
 *   This is the item for query chip name for HAL interface
 */
typedef enum _VAL_CHIP_NAME_T {
	VAL_CHIP_NAME_MT6516 = 0,                   /* /< MT6516 */
	VAL_CHIP_NAME_MT6571,                       /* /< MT6571 */
	VAL_CHIP_NAME_MT6572,                       /* /< MT6572 */
	VAL_CHIP_NAME_MT6573,                       /* /< MT6573 */
	VAL_CHIP_NAME_MT6575,                       /* /< MT6575 */
	VAL_CHIP_NAME_MT6577,                       /* /< MT6577 */
	VAL_CHIP_NAME_MT6589,                       /* /< MT6589 */
	VAL_CHIP_NAME_MT6582,                       /* /< MT6582 */
	VAL_CHIP_NAME_MT8135,                       /* /< MT8135 */
	VAL_CHIP_NAME_ROME,                         /* /< ROME */
	VAL_CHIP_NAME_MT6592,                       /* /< MT6592 */
	VAL_CHIP_NAME_MT8127,                       /* /< MT8127 */
	VAL_CHIP_NAME_MT6752,                       /* /< MT6752 */
	VAL_CHIP_NAME_MT6795,                       /* /< MT6795 */
	VAL_CHIP_NAME_DENALI_1,                     /* /< Denali-1 */
	VAL_CHIP_NAME_DENALI_2,                     /* /< Denali-2 */
	VAL_CHIP_NAME_DENALI_3,                     /* /< Denali-3 */
	VAL_CHIP_NAME_MT6570,                       /* /< Rainier (2 core) */
	VAL_CHIP_NAME_MT6580,                       /* /< Rainier (4 core) */
	VAL_CHIP_NAME_MT8163,
	VAL_CHIP_NAME_MT8173,                       /* / <8173 */
	VAL_CHIP_NAME_MT6755,                       /* / <Jade */
	VAL_CHIP_NAME_MT6757,                       /* / <Olympus */
	VAL_CHIP_NAME_MT6797,                       /* / <Everest */
	VAL_CHIP_NAME_MT7623,                       /* / <7623 */
	VAL_CHIP_NAME_MT8167,                       /* / <MT8167 */
	VAL_CHIP_NAME_ELBRUS,                       /* /< ELBRUS */
	VAL_CHIP_NAME_MAX = 0xFFFFFFFF              /* /< Max Value */
} VAL_CHIP_NAME_T;

/**
 * @par Enumeration
 *   VAL_CHIP_VARIANT_T
 * @par Description
 *   This is the item for query chip variant for HAL interface
 */
typedef enum _VAL_CHIP_VARIANT_T {
	VAL_CHIP_VARIANT_MT6571L = 0,       /* /< MT6571L */
	VAL_CHIP_VARIANT_MAX = 0xFFFFFFFF  /* /< Max Value */
} VAL_CHIP_VARIANT_T;


/**
 * @par Enumeration
 *   VAL_CHIP_VERSION_T
 * @par Description
 *   This is the item used to GetChipVersionAPI()
 */
typedef enum _VAL_CHIP_VERSION_T {
	/* /< The data will be "6595" for 6595 series; "6795" for 6795 series, ... */
	VAL_CHIP_VERSION_HW_CODE = 0,
	/* /< The data will be "0000" for E1; "0001" for E2, ... */
	VAL_CHIP_VERSION_SW_VER,
	/* /< Max Value */
	VAL_CHIP_VERSION_MAX = 0xFFFFFFFF
} VAL_CHIP_VERSION_T;


/**
 * @par Enumeration
 *   VAL_DRIVER_TYPE_T
 * @par Description
 *   This is the item for driver type
 */
typedef enum _VAL_DRIVER_TYPE_T {
	VAL_DRIVER_TYPE_NONE = 0,                   /* /< None */
	VAL_DRIVER_TYPE_MP4_ENC,                    /* /< MP4 encoder */
	VAL_DRIVER_TYPE_MP4_DEC,                    /* /< MP4 decoder */
	VAL_DRIVER_TYPE_H263_ENC,                   /* /< H.263 encoder */
	VAL_DRIVER_TYPE_H263_DEC,                   /* /< H.263 decoder */
	VAL_DRIVER_TYPE_H264_ENC,                   /* /< H.264 encoder */
	VAL_DRIVER_TYPE_H264_DEC,                   /* /< H.264 decoder */
	VAL_DRIVER_TYPE_SORENSON_SPARK_DEC,         /* /< Sorenson Spark decoder */
	VAL_DRIVER_TYPE_VC1_SP_DEC,                 /* /< VC-1 simple profile decoder */
	VAL_DRIVER_TYPE_RV9_DEC,                    /* /< RV9 decoder */
	VAL_DRIVER_TYPE_MP1_MP2_DEC,                /* /< MPEG1/2 decoder */
	VAL_DRIVER_TYPE_XVID_DEC,                   /* /< Xvid decoder */
	VAL_DRIVER_TYPE_DIVX4_DIVX5_DEC,            /* /< Divx4/5 decoder */
	VAL_DRIVER_TYPE_VC1_MP_WMV9_DEC,            /* /< VC-1 main profile (WMV9) decoder */
	VAL_DRIVER_TYPE_RV8_DEC,                    /* /< RV8 decoder */
	VAL_DRIVER_TYPE_WMV7_DEC,                   /* /< WMV7 decoder */
	VAL_DRIVER_TYPE_WMV8_DEC,                   /* /< WMV8 decoder */
	VAL_DRIVER_TYPE_AVS_DEC,                    /* /< AVS decoder */
	VAL_DRIVER_TYPE_DIVX_3_11_DEC,              /* /< Divx3.11 decoder */
	VAL_DRIVER_TYPE_H264_DEC_MAIN,              /* /< H.264 main profile decoder (due to different packet) == 20 */
	/* /< H.264 main profile decoder for CABAC type but packet is the same, just for reload. */
	VAL_DRIVER_TYPE_H264_DEC_MAIN_CABAC,
	VAL_DRIVER_TYPE_VP8_DEC,                    /* /< VP8 decoder */
	VAL_DRIVER_TYPE_MP2_DEC,                    /* /< MPEG2 decoder */
	VAL_DRIVER_TYPE_VP9_DEC,                    /* /< VP9 decoder */
	VAL_DRIVER_TYPE_VP8_ENC,                    /* /< VP8 encoder */
	VAL_DRIVER_TYPE_VC1_ADV_DEC,                /* /< VC1 advance decoder */
	VAL_DRIVER_TYPE_VC1_DEC,                    /* /< VC1 simple/main/advance decoder */
	VAL_DRIVER_TYPE_JPEG_ENC,                   /* /< JPEG encoder */
	VAL_DRIVER_TYPE_HEVC_ENC,                   /* /< HEVC encoder */
	VAL_DRIVER_TYPE_HEVC_DEC,                   /* /< HEVC decoder */
	VAL_DRIVER_TYPE_H264_ENC_LIVEPHOTO,         /* LivePhoto type */
	VAL_DRIVER_TYPE_MMDVFS,                     /* /< MMDVFS */
	VAL_DRIVER_TYPE_MAX = 0xFFFFFFFF            /* /< Max driver type */
} VAL_DRIVER_TYPE_T;


/**
 * @par Enumeration
 *   VAL_RESULT_T
 * @par Description
 *   This is the return status of each OSAL function
 */
typedef enum _VAL_RESULT_T {
	VAL_RESULT_NO_ERROR = 0,                    /* /< The function work successfully */
	VAL_RESULT_INVALID_DRIVER,                  /* /< Error due to invalid driver */
	VAL_RESULT_INVALID_PARAMETER,               /* /< Error due to invalid parameter */
	VAL_RESULT_INVALID_MEMORY,                  /* /< Error due to invalid memory */
	VAL_RESULT_INVALID_ISR,                     /* /< Error due to invalid isr request */
	VAL_RESULT_ISR_TIMEOUT,                     /* /< Error due to invalid isr request */
	VAL_RESULT_UNKNOWN_ERROR,                   /* /< Unknown error */
	VAL_RESULT_RESTARTSYS,                      /* /< Restart sys */
	VAL_RESULT_MAX = 0xFFFFFFFF                 /* /< Max result */
} VAL_RESULT_T;


/**
 * @par Enumeration
 *   VAL_MEM_ALIGN_T
 * @par Description
 *   This is the item for allocation memory byte alignment
 */
typedef enum _VAL_MEM_ALIGN_T {
	VAL_MEM_ALIGN_1 = 1,                        /* /< 1 byte alignment */
	VAL_MEM_ALIGN_2 = (1 << 1),                 /* /< 2 byte alignment */
	VAL_MEM_ALIGN_4 = (1 << 2),                 /* /< 4 byte alignment */
	VAL_MEM_ALIGN_8 = (1 << 3),                 /* /< 8 byte alignment */
	VAL_MEM_ALIGN_16 = (1 << 4),                /* /< 16 byte alignment */
	VAL_MEM_ALIGN_32 = (1 << 5),                /* /< 32 byte alignment */
	VAL_MEM_ALIGN_64 = (1 << 6),                /* /< 64 byte alignment */
	VAL_MEM_ALIGN_128 = (1 << 7),               /* /< 128 byte alignment */
	VAL_MEM_ALIGN_256 = (1 << 8),               /* /< 256 byte alignment */
	VAL_MEM_ALIGN_512 = (1 << 9),               /* /< 512 byte alignment */
	VAL_MEM_ALIGN_1K = (1 << 10),               /* /< 1K byte alignment */
	VAL_MEM_ALIGN_2K = (1 << 11),               /* /< 2K byte alignment */
	VAL_MEM_ALIGN_4K = (1 << 12),               /* /< 4K byte alignment */
	VAL_MEM_ALIGN_8K = (1 << 13),               /* /< 8K byte alignment */
	VAL_MEM_ALIGN_MAX = 0xFFFFFFFF              /* /< Max memory byte alignment */
} VAL_MEM_ALIGN_T;


/**
 * @par Enumeration
 *   VAL_MEM_TYPE_T
 * @par Description
 *   This is the item for allocation memory type
 */
typedef enum _VAL_MEM_TYPE_T {
	VAL_MEM_TYPE_FOR_SW = 0,                    /* /< External memory foe SW */
	VAL_MEM_TYPE_FOR_HW_CACHEABLE,              /* /< External memory for HW Cacheable */
	VAL_MEM_TYPE_FOR_HW_CACHEABLE_MCI,          /* /< External memory for HW Cacheable, with MCI port config */
	VAL_MEM_TYPE_FOR_HW_NONCACHEABLE,           /* /< External memory for HW Non-Cacheable */
	VAL_MEM_TYPE_MAX = 0xFFFFFFFF               /* /< Max memory type */
} VAL_MEM_TYPE_T;


/**
 * @par Structure
 *  VAL_MEM_ADDR_T
 * @par Description
 *  This is a structure for memory address
 */
typedef struct _VAL_MEM_ADDR_T {                 /* union extend 64bits for TEE*/
	union {
		VAL_ULONG_T    u4VA;                       /* /< [IN/OUT] virtual address */
		VAL_UINT64_T u4VA_ext64;
	};
	union {
		VAL_ULONG_T    u4PA;                       /* /< [IN/OUT] physical address */
		VAL_UINT64_T u4PA_ext64;
	};
	union {
		VAL_ULONG_T    u4Size;                     /* /< [IN/OUT] size */
		VAL_UINT64_T u4Size_ext64;
	};
} VAL_MEM_ADDR_T;


/**
 * @par Structure
 *  VAL_VCODEC_THREAD_ID_T
 * @par Description
 *  This is a structure for thread info
 */
typedef struct _VAL_VCODEC_THREAD_ID_T {
	VAL_UINT32_T    u4tid1; /* /< [IN/OUT] thread id for single core */
	VAL_UINT32_T    u4tid2; /* /< [IN/OUT] thread id for single core */
	VAL_UINT32_T    u4VCodecThreadNum;                          /* /< [IN/OUT] thread num */
	VAL_UINT32_T    u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];    /* /< [IN/OUT] thread id for each thread */
} VAL_VCODEC_THREAD_ID_T;


/**
 * @par Structure
 *  VAL_VCODEC_CPU_LOADING_INFO_T
 * @par Description
 *  This is a structure for CPU loading info
 */
typedef struct _VAL_VCODEC_CPU_LOADING_INFO_T {
	unsigned long long  _cpu_idle_time;         /* /< [OUT] cpu idle time */
	unsigned long long  _thread_cpu_time;       /* /< [OUT] thread cpu time */
	unsigned long long  _sched_clock;           /* /< [OUT] sched clock */
	unsigned int        _inst_count;            /* /< [OUT] inst count */
} VAL_VCODEC_CPU_LOADING_INFO_T;


/**
 * @par Structure
 *  VAL_VCODEC_CPU_OPP_LIMIT_T
 * @par Description
 *  This is a structure for CPU opp limit info
 */
typedef struct _VAL_VCODEC_CPU_OPP_LIMIT_T {
	int limited_freq;                           /* /< [IN] limited freq */
	int limited_cpu;                            /* /< [IN] limited cpu */
	int enable;                                 /* /< [IN] enable */
} VAL_VCODEC_CPU_OPP_LIMIT_T;


/**
 * @par Structure
 *  VAL_VCODEC_M4U_BUFFER_CONFIG_T
 * @par Description
 *  This is a structure for m4u buffer config
 */
typedef struct _VAL_VCODEC_M4U_BUFFER_CONFIG_T {
	VAL_MEM_CODEC_T eMemCodec;                  /* /< [IN] memory usage for encoder or decoder */
	VAL_UINT32_T    cache_coherent;             /* /< [IN] cache coherent or not */
	VAL_UINT32_T    security;                   /* /< [IN] security or not */
} VAL_VCODEC_M4U_BUFFER_CONFIG_T;


/**
 * @par Structure
 *  VAL_MEMORY_T
 * @par Description
 *  This is a parameter for memory usaged function
 */
typedef struct _VAL_MEMORY_T {                /* union extend 64bits for TEE*/
	VAL_MEM_TYPE_T  eMemType;                   /* /< [IN]     The allocation memory type */
	union {
		VAL_ULONG_T     u4MemSize;              /* /< [IN]     The size of memory allocation */
		VAL_UINT64_T u4MemSize_ext64;
	};
	union {
		VAL_VOID_T *pvMemVa;
		VAL_UINT64_T pvMemVa_ext64;
	};
	union {
		VAL_VOID_T *pvMemPa;
		VAL_UINT64_T pvMemPa_ext64;
	};
	VAL_MEM_ALIGN_T eAlignment;                 /* /< [IN]     The memory byte alignment setting */
	union {
		VAL_VOID_T *pvAlignMemVa;
		VAL_UINT64_T pvAlignMemVa_ext64;
	};
	union {
		VAL_VOID_T *pvAlignMemPa;
		VAL_UINT64_T pvAlignMemPa_ext64;
	};
	VAL_MEM_CODEC_T eMemCodec;                  /* /< [IN]     The memory codec for VENC or VDEC */
	VAL_UINT32_T    i4IonShareFd;

	union {
		struct ion_handle *pIonBufhandle;
		VAL_UINT64_T pIonBufhandle_ext64;
	};
	union {
		VAL_VOID_T      *pvReserved;            /* /< [IN/OUT] The reserved parameter */
		VAL_UINT64_T pvReserved_ext64;
	};
	union {
		VAL_ULONG_T     u4ReservedSize;         /* /< [IN]     The size of reserved parameter structure */
		VAL_UINT64_T u4ReservedSize_ext64;
	};
#ifdef __EARLY_PORTING__
	union {
		VAL_VOID_T      *pvReservedPmem;        /* /< [IN/OUT] The reserved parameter */
		VAL_UINT64_T pvReservedPmem_ext64;
	};
#endif
} VAL_MEMORY_T;

/**
 * @par Structure
 *  VAL_RECORD_SIZE_T
 * @par Description
 *  This is a parameter for setting record size to EMI controller
 */
typedef struct __VAL_RECORD_SIZE_T {
	VAL_UINT32_T    u4FrmWidth;                 /* /< [IN] Frame Width, (may not 16 byte-align) */
	VAL_UINT32_T    u4FrmHeight;                /* /< [IN] Frame Height, (may not 16 byte-align) */
	VAL_UINT32_T    u4BufWidth;                 /* /< [IN] Buffer Width, (must 16 byte-align) */
	VAL_UINT32_T    u4BufHeight;                /* /< [IN] Buffer Height, (must 16 byte-align) */
} VAL_RECORD_SIZE_T;


/**
 * @par Structure
 *  VAL_ATOI_T
 * @par Description
 *  This is a parameter for eVideoAtoi()
 */
typedef struct _VAL_ATOI_T {
	VAL_VOID_T      *pvStr;                     /* /< [IN]     Null-terminated String to be converted */
	/* /< [Out]    returns the int value produced by interpreting the input characters as a number. */
	VAL_INT32_T     i4Result;
	VAL_VOID_T      *pvReserved;                /* /< [IN/OUT] The reserved parameter */
	VAL_UINT32_T    u4ReservedSize;             /* /< [IN]     The size of reserved parameter structure */
} VAL_ATOI_T;


/**
 * @par Structure
 *  VAL_STRSTR_T
 * @par Description
 *  This is a parameter for eVideoStrStr()
 */
typedef struct _VAL_STRSTR_T {
	VAL_VOID_T      *pvStr;                     /* /< [IN]     Null-terminated string to search. */
	VAL_VOID_T      *pvStrSearch;               /* /< [IN]     Null-terminated string to search for */
	/*
	    /< [Out]    Returns a pointer to the first occurrence of strSearch in str,
			or NULL if strSearch does not appear in str.
	*/
	VAL_VOID_T      *pvStrResult;
	VAL_VOID_T      *pvReserved;                /* /< [IN/OUT] The reserved parameter */
	VAL_UINT32_T    u4ReservedSize;             /* /< [IN]     The size of reserved parameter structure */
} VAL_STRSTR_T;


/**
 * @par Structure
 *  VAL_ISR_T
 * @par Description
 *  This is a parameter for ISR related function
 */
typedef struct _VAL_ISR_T {
	VAL_VOID_T          *pvHandle;              /* /< [IN]     The video codec driver handle */
	VAL_UINT32_T        u4HandleSize;           /* /< [IN]     The size of video codec driver handle */
	VAL_DRIVER_TYPE_T   eDriverType;            /* /< [IN]     The driver type */
	VAL_VOID_T          *pvIsrFunction;         /* /< [IN]     The isr function */
	VAL_VOID_T          *pvReserved;            /* /< [IN/OUT] The reserved parameter */
	VAL_UINT32_T        u4ReservedSize;         /* /< [IN]     The size of reserved parameter structure */
	VAL_UINT32_T        u4TimeoutMs;            /* /< [IN]     The timeout in ms */
	/* /< [IN]     The num of return registers when HW done */
	VAL_UINT32_T        u4IrqStatusNum;
	/* /< [IN/OUT] The value of return registers when HW done */
	VAL_UINT32_T        u4IrqStatus[IRQ_STATUS_MAX_NUM];
} VAL_ISR_T;


/**
 * @par Structure
 *  VAL_HW_LOCK_T
 * @par Description
 *  This is a parameter for HW Lock/UnLock related function
 */
typedef struct _VAL_HW_LOCK_T {
	VAL_VOID_T          *pvHandle;              /* /< [IN]     The video codec driver handle */
	VAL_UINT32_T        u4HandleSize;           /* /< [IN]     The size of video codec driver handle */
	VAL_VOID_T          *pvLock;                /* /< [IN/OUT] The Lock discriptor */
	VAL_UINT32_T        u4TimeoutMs;            /* /< [IN]     The timeout ms */
	VAL_VOID_T          *pvReserved;            /* /< [IN/OUT] The reserved parameter */
	VAL_UINT32_T        u4ReservedSize;         /* /< [IN]     The size of reserved parameter structure */
	VAL_DRIVER_TYPE_T   eDriverType;            /* /< [IN]     The driver type */
	/* /< [IN] True if this is a secure instance // MTK_SEC_VIDEO_PATH_SUPPORT */
	VAL_BOOL_T          bSecureInst;
} VAL_HW_LOCK_T;


/**
 * @par Structure
 *  VAL_TIME_T
 * @par Description
 *  This is a structure for system time.
 */
typedef struct _VAL_TIME_T {
	VAL_UINT32_T    u4Sec;                      /* /< [IN/OUT] second */
	VAL_UINT32_T    u4uSec;                     /* /< [IN/OUT] micro second */
} VAL_TIME_T;


/**
 * @par Enumeration
 *   VAL_SET_TYPE_T
 * @par Description
 *   This is the item for setting val parameter
 */
typedef enum _VAL_SET_TYPE_T {
	VAL_SET_TYPE_CURRENT_SCENARIO,              /* /< Set current scenario */
	VAL_SET_TYPE_MCI_PORT_CONFIG,               /* /< Set MCI port config */
	VAL_SET_TYPE_M4U_PORT_CONFIG,               /* /< Set M4U port config */
	VAL_SET_TYPE_SET_TCM_ON,                    /* /< Set TCM on */
	VAL_SET_TYPE_SET_TCM_OFF,                   /* /< Set TCM off */
} VAL_SET_TYPE_T;

/**
 * @par Enumeration
 *   VAL_GET_TYPE_T
 * @par Description
 *   This is the item for getting val parameter
 */
typedef enum _VAL_GET_TYPE_T {
	VAL_GET_TYPE_CURRENT_SCENARIO_CNT,          /* /< Get current scenario reference count */
	VAL_GET_TYPE_LCM_INFO,                      /* /< Get LCM info */
} VAL_GET_TYPE_T;

/**
 * @par Enumeration
 *   VAL_VCODEC_SCENARIO
 * @par Description
 *   This is the item for get/setting current vcodec scenario
 */
typedef enum _VAL_VCODEC_SCENARIO_T {
	VAL_VCODEC_SCENARIO_VENC_1080P  = 0x1,          /* /< Camera recording 1080P */
	VAL_VCODEC_SCENARIO_VDEC_1080P  = 0x2,          /* /< Playback 1080P */
	VAL_VCODEC_SCENARIO_VENC_WFD    = 0x4,          /* /< Wifi-display encoding */
	VAL_VCODEC_SCENARIO_VDEC_60FPS  = 0x8,          /* /< Playback 60fps video */
	VAL_VCODEC_SCENARIO_VDEC_4K     = 0x10,         /* /< Playback 4K */
	VAL_VCODEC_SCENARIO_VDEC_2K     = 0x20,         /* /< Playback 2K */
} VAL_VCODEC_SCENARIO_T;

/**
 * @par Structure
 *  VAL_CURRENT_SCENARIO_T
 * @par Description
 *  This is a structure for set/get current scenario
 */
typedef struct _VAL_CURRENT_SCENARIO_T {
	VAL_UINT32_T    u4Scenario;             /* /< [IN/OUT] set/get current scenario */
	VAL_UINT32_T    u4OnOff;                /* /< [IN] set on/off (increment/decrement) 1 = inc, 0 = dec */
} VAL_CURRENT_SCENARIO_T;

/**
 * @par Structure
 *  VAL_CURRENT_SCENARIO_CNT_T
 * @par Description
 *  This is a structure for set/get current scenario reference count
 */
typedef struct _VAL_CURRENT_SCENARIO_CNT_T {
	VAL_UINT32_T    u4Scenario;             /* /< [IN] current scenario type */
	VAL_UINT32_T    u4ScenarioRefCount;     /* /< [OUT] current scenario reference count */
} VAL_CURRENT_SCENARIO_CNT_T;


/**
 * @par Structure
 *  VAL_MCI_PORT_CONFIG_T
 * @par Description
 *  This is a structure for set/get MCI port config
 */
typedef struct _VAL_MCI_PORT_CONFIG_T {
	VAL_MEM_CODEC_T    eMemCodecType;       /* /< [IN] memory type - decoder/encoder */
	VAL_UINT32_T       u4Config;            /* /< [IN] set port config */
} VAL_MCI_PORT_CONFIG_T;

/**
 * @par Structure
 *  VAL_LCM_INFO_T
 * @par Description
 *  This is a structure for get LCM info
 */
typedef struct _VAL_LCM_INFO_T {
	VAL_UINT32_T        u4Width;            /* /< [OUT] width */
	VAL_UINT32_T        u4Height;           /* /< [OUT] height */
} VAL_LCM_INFO_T;

#define VAL_M4U_PORT_ALL       (-1)        /* /< VAL_M4UPORT_DEFAULT_ALL = 1, config all M4U port for VENC or VDEC */

/**
 * @par Structure
 *  VAL_M4U_MPORT_CONFIG_T
 * @par Description
 *  This is a parameter for eVideoSetParam() input structure
 */
typedef struct _VAL_M4U_MPORT_CONFIG_T {
	/* /< [IN]  The memory codec for VENC or VDEC */
	VAL_MEM_CODEC_T eMemCodec;
	/* /< [IN]  config port ID (VAL_M4U_PORT_ALL[-1] = config all VENC or VDEC) */
	VAL_UINT32_T        i4M4UPortID;
	/* /< [IN]  config port security */
	VAL_BOOL_T          bSecurity;
	/* /< [IN]  config port virtuality */
	VAL_BOOL_T          bVirtuality;
} VAL_M4U_MPORT_CONFIG_T;


/* for DirectLink Meta Mode + */
#define META_HANDLE_LIST_MAX    50

typedef struct _VAL_MetaBufInfo {
	void               *pNativeHandle;
	VAL_ULONG_T         u4VA;
	VAL_ULONG_T         u4PA;
	VAL_UINT32_T        u4BuffSize;
	VAL_BOOL_T          bUseION;
	int                 fd;
	struct ion_handle  *pIonBufhandle;
} VAL_MetaBufInfo;

typedef struct _VAL_MetaHandleList {
	int                 mIonDevFd;
	VAL_MetaBufInfo     rMetaBufInfo[META_HANDLE_LIST_MAX];
	VAL_BOOL_T          fgSeqHdrEncoded;
} VAL_MetaHandleList;

typedef struct _VAL_BufInfo {
	VAL_UINT8_T         fgIsConfigData;
	VAL_ULONG_T         u4BSVA;
	VAL_UINT8_T         fgBSStatus;
	VAL_UINT8_T         fgIsKeyFrame;
	VAL_UINT32_T        u4BSSize;
} VAL_BufInfo;
/* for DirectLink Meta Mode - */


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_TYPES_PUBLIC_H_ */

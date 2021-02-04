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

/* /< support max 16 return register values when HW done */
#define IRQ_STATUS_MAX_NUM 16

/* /< support max 16 multiple thread currently */
#define VCODEC_THREAD_MAX_NUM 16

/*=============================================================================
 *							  Type definition
 *===========================================================================
 */

#define VAL_VOID_T void     /* /< void type definition */
#define VAL_BOOL_T char     /* /< char type definition */
#define VAL_CHAR_T char     /* /< char type definition */
#define VAL_INT8_T signed char      /* /< signed char type definition */
#define VAL_INT16_T signed short    /* /< signed short type definition */
#define VAL_INT32_T signed int      /* /< signed int type definition */
#define VAL_UCHAR_T unsigned char   /* /< unsigned char type definition */
#define VAL_UINT8_T unsigned char   /* /< unsigned char type definition */
#define VAL_UINT16_T unsigned short /* /< unsigned short definition */
#define VAL_UINT32_T unsigned int   /* /< unsigned int type definition */
#define VAL_UINT64_T unsigned long long /* unsigned long long type definition */
#define VAL_INT64_T long long   /* long long type definition */
#define VAL_HANDLE_T unsigned long /* unsigned int (handle) type definition */
#define VAL_LONG_T signed long
#define VAL_ULONG_T unsigned long

#define VAL_NULL (0)  /* /< VAL_NULL = 0 */
#define VAL_TRUE (1)  /* /< VAL_TRUE = 1 */
#define VAL_FALSE (0) /* /< VAL_FALSE = 0 */

/* /< VAL_RESOLUTION_CHANGED = 2, used to video resolution changed during
 * playback
 */
#define VAL_RESOLUTION_CHANGED (2)

/**
 * @par Enumeration
 *	VAL_MEM_CODEC_T
 * @par Description
 *	This is the item used to memory usage for video encoder or video
 *decoder
 */
enum VAL_MEM_CODEC_T {
	VAL_MEM_CODEC_FOR_VENC = 0,    /* /< Memory for Video Encoder */
	VAL_MEM_CODEC_FOR_VDEC,	/* /< Memory for Video Decoder */
	VAL_MEM_CODEC_MAX = 0xFFFFFFFF /* /< Max Value */
};

/**
 * @par Enumeration
 *	VAL_CHIP_NAME_T
 * @par Description
 *	This is the item for query chip name for HAL interface
 */
enum VAL_CHIP_NAME_T {
	VAL_CHIP_NAME_MT6516 = 0, /* /< MT6516 */
	VAL_CHIP_NAME_MT6571,     /* /< MT6571 */
	VAL_CHIP_NAME_MT6572,     /* /< MT6572 */
	VAL_CHIP_NAME_MT6573,     /* /< MT6573 */
	VAL_CHIP_NAME_MT6575,     /* /< MT6575 */
	VAL_CHIP_NAME_MT6577,     /* /< MT6577 */
	VAL_CHIP_NAME_MT6589,     /* /< MT6589 */
	VAL_CHIP_NAME_MT6582,     /* /< MT6582 */
	VAL_CHIP_NAME_MT8135,     /* /< MT8135 */
	VAL_CHIP_NAME_ROME,       /* /< ROME */
	VAL_CHIP_NAME_MT6592,     /* /< MT6592 */
	VAL_CHIP_NAME_MT8127,     /* /< MT8127 */
	VAL_CHIP_NAME_MT6752,     /* /< MT6752 */
	VAL_CHIP_NAME_MT6795,     /* /< MT6795 */
	VAL_CHIP_NAME_DENALI_1,   /* /< MT6737-1 */
	VAL_CHIP_NAME_DENALI_2,   /* /< MT6737-2 */
	VAL_CHIP_NAME_DENALI_3,   /* /< MT6737-3 */
	VAL_CHIP_NAME_MT6570,     /* /< MT6570 (2 core) */
	VAL_CHIP_NAME_MT6580,     /* /< MT6580 (4 core) */
	VAL_CHIP_NAME_MT8163,
	VAL_CHIP_NAME_MT8173,	  /* / <8173 */
	VAL_CHIP_NAME_MT6755,	  /* / <MT6755 */
	VAL_CHIP_NAME_MT6757,	  /* / <MT6757 */
	VAL_CHIP_NAME_MT6797,	  /* / <MT6797 */
	VAL_CHIP_NAME_MT7623,	  /* / <MT7623 */
	VAL_CHIP_NAME_MT8167,	  /* / <MT8167 */
	VAL_CHIP_NAME_ELBRUS,	  /* /< ELBRUS */
	VAL_CHIP_NAME_MT6799,	  /* /< MT6799 */
	VAL_CHIP_NAME_MT6759,	  /* /< MT6759 */
	VAL_CHIP_NAME_MT6758,	  /* / <MT6758 */
	VAL_CHIP_NAME_MT6763,	  /* /< MT6763 */
	VAL_CHIP_NAME_MT6739,	  /* /< MT6739 */
	VAL_CHIP_NAME_MT6771,	  /* /< MT6771 */
	VAL_CHIP_NAME_MT6775,	  /* /< MT6775 */
	VAL_CHIP_NAME_MT6765,	  /* /< MT6765 */
	VAL_CHIP_NAME_MT3967,	  /* /< MT3967 */
	VAL_CHIP_NAME_MT6761,	  /* /< MT6761 */
	VAL_CHIP_NAME_MAX = 0xFFFFFFFF /* /< Max Value */
};

/**
 * @par Enumeration
 *	VAL_CHIP_VARIANT_T
 * @par Description
 *	This is the item for query chip variant for HAL interface
 */
enum VAL_CHIP_VARIANT_T {
	VAL_CHIP_VARIANT_MT6571L = 0,     /* /< MT6571L */
	VAL_CHIP_VARIANT_MAX = 0xFFFFFFFF /* /< Max Value */
};

/**
 * @par Enumeration
 *	VAL_CHIP_VERSION_T
 * @par Description
 *	This is the item used to GetChipVersionAPI()
 */
enum VAL_CHIP_VERSION_T {
	/* /< The data will be "6595" for 6595 series; "6795" for 6795 series,
	 *...
	 */
	VAL_CHIP_VERSION_HW_CODE = 0,
	/* /< The data will be "0000" for E1; "0001" for E2, ... */
	VAL_CHIP_VERSION_SW_VER,
	/* /< Max Value */
	VAL_CHIP_VERSION_MAX = 0xFFFFFFFF
};

/**
 * @par Enumeration
 *	VAL_DRIVER_TYPE_T
 * @par Description
 *	This is the item for driver type
 */
enum VAL_DRIVER_TYPE_T {
	VAL_DRIVER_TYPE_NONE = 0,	   /* /< None */
	VAL_DRIVER_TYPE_MP4_ENC,	    /* /< MP4 encoder */
	VAL_DRIVER_TYPE_MP4_DEC,	    /* /< MP4 decoder */
	VAL_DRIVER_TYPE_H263_ENC,	   /* /< H.263 encoder */
	VAL_DRIVER_TYPE_H263_DEC,	   /* /< H.263 decoder */
	VAL_DRIVER_TYPE_H264_ENC,	   /* /< H.264 encoder */
	VAL_DRIVER_TYPE_H264_DEC,	   /* /< H.264 decoder */
	VAL_DRIVER_TYPE_SORENSON_SPARK_DEC, /* /< Sorenson Spark decoder */
	VAL_DRIVER_TYPE_VC1_SP_DEC,      /* /< VC-1 simple profile decoder */
	VAL_DRIVER_TYPE_RV9_DEC,	 /* /< RV9 decoder */
	VAL_DRIVER_TYPE_MP1_MP2_DEC,     /* /< MPEG1/2 decoder */
	VAL_DRIVER_TYPE_XVID_DEC,	/* /< Xvid decoder */
	VAL_DRIVER_TYPE_DIVX4_DIVX5_DEC, /* /< Divx4/5 decoder */
	/* /< VC-1 main profile (WMV9) decoder */
	VAL_DRIVER_TYPE_VC1_MP_WMV9_DEC,
	VAL_DRIVER_TYPE_RV8_DEC,       /* /< RV8 decoder */
	VAL_DRIVER_TYPE_WMV7_DEC,      /* /< WMV7 decoder */
	VAL_DRIVER_TYPE_WMV8_DEC,      /* /< WMV8 decoder */
	VAL_DRIVER_TYPE_AVS_DEC,       /* /< AVS decoder */
	VAL_DRIVER_TYPE_DIVX_3_11_DEC, /* /< Divx3.11 decoder */
	/* /< H.264 main profile decoder (due to different packet) == 20 */
	VAL_DRIVER_TYPE_H264_DEC_MAIN,
	/* /< H.264 main profile decoder for CABAC type but packet is the same,
	 * just for reload.
	 */
	VAL_DRIVER_TYPE_H264_DEC_MAIN_CABAC,
	VAL_DRIVER_TYPE_VP8_DEC,     /* /< VP8 decoder */
	VAL_DRIVER_TYPE_MP2_DEC,     /* /< MPEG2 decoder */
	VAL_DRIVER_TYPE_VP9_DEC,     /* /< VP9 decoder */
	VAL_DRIVER_TYPE_VP8_ENC,     /* /< VP8 encoder */
	VAL_DRIVER_TYPE_VC1_ADV_DEC, /* /< VC1 advance decoder */
	VAL_DRIVER_TYPE_VC1_DEC,     /* /< VC1 simple/main/advance decoder */
	VAL_DRIVER_TYPE_JPEG_ENC,    /* /< JPEG encoder */
	VAL_DRIVER_TYPE_HEVC_ENC,    /* /< HEVC encoder */
	VAL_DRIVER_TYPE_HEVC_DEC,    /* /< HEVC decoder */
	VAL_DRIVER_TYPE_H264_ENC_LIVEPHOTO, /* LivePhoto type */
	VAL_DRIVER_TYPE_MMDVFS,		    /* /< MMDVFS */
	VAL_DRIVER_TYPE_VP9_ENC,	    /* /< VP9 encoder */
	VAL_DRIVER_TYPE_MAX = 0xFFFFFFFF    /* /< Max driver type */
};

/**
 * @par Enumeration
 *	VAL_RESULT_T
 * @par Description
 *	This is the return status of each OSAL function
 */
enum VAL_RESULT_T {
	VAL_RESULT_NO_ERROR = 0,      /* /< The function work successfully */
	VAL_RESULT_INVALID_DRIVER,    /* /< Error due to invalid driver */
	VAL_RESULT_INVALID_PARAMETER, /* /< Error due to invalid parameter */
	VAL_RESULT_INVALID_MEMORY,    /* /< Error due to invalid memory */
	VAL_RESULT_INVALID_ISR,       /* /< Error due to invalid isr request */
	VAL_RESULT_ISR_TIMEOUT,       /* /< Error due to invalid isr request */
	VAL_RESULT_UNKNOWN_ERROR,     /* /< Unknown error */
	VAL_RESULT_RESTARTSYS,	/* /< Restart sys */
	VAL_RESULT_MAX = 0xFFFFFFFF   /* /< Max result */
};

/**
 * @par Enumeration
 *	VAL_MEM_ALIGN_T
 * @par Description
 *	This is the item for allocation memory byte alignment
 */
enum VAL_MEM_ALIGN_T {
	VAL_MEM_ALIGN_1 = 1,	   /* /< 1 byte alignment */
	VAL_MEM_ALIGN_2 = (1 << 1),    /* /< 2 byte alignment */
	VAL_MEM_ALIGN_4 = (1 << 2),    /* /< 4 byte alignment */
	VAL_MEM_ALIGN_8 = (1 << 3),    /* /< 8 byte alignment */
	VAL_MEM_ALIGN_16 = (1 << 4),   /* /< 16 byte alignment */
	VAL_MEM_ALIGN_32 = (1 << 5),   /* /< 32 byte alignment */
	VAL_MEM_ALIGN_64 = (1 << 6),   /* /< 64 byte alignment */
	VAL_MEM_ALIGN_128 = (1 << 7),  /* /< 128 byte alignment */
	VAL_MEM_ALIGN_256 = (1 << 8),  /* /< 256 byte alignment */
	VAL_MEM_ALIGN_512 = (1 << 9),  /* /< 512 byte alignment */
	VAL_MEM_ALIGN_1K = (1 << 10),  /* /< 1K byte alignment */
	VAL_MEM_ALIGN_2K = (1 << 11),  /* /< 2K byte alignment */
	VAL_MEM_ALIGN_4K = (1 << 12),  /* /< 4K byte alignment */
	VAL_MEM_ALIGN_8K = (1 << 13),  /* /< 8K byte alignment */
	VAL_MEM_ALIGN_MAX = 0xFFFFFFFF /* /< Max memory byte alignment */
};

/**
 * @par Enumeration
 *	VAL_MEM_TYPE_T
 * @par Description
 *	This is the item for allocation memory type
 */
enum VAL_MEM_TYPE_T {
	VAL_MEM_TYPE_FOR_SW = 0, /* /< External memory foe SW */
	/* /< External memory for HW Cacheable */
	VAL_MEM_TYPE_FOR_HW_CACHEABLE,
	/* /< External memory for HW Cacheable, with MCI port config */
	VAL_MEM_TYPE_FOR_HW_CACHEABLE_MCI,
	/* /< External memory for HW Non-Cacheable */
	VAL_MEM_TYPE_FOR_HW_NONCACHEABLE,
	VAL_MEM_TYPE_MAX = 0xFFFFFFFF /* /< Max memory type */
};

/**
 * @par Structure
 *  VAL_MEM_ADDR_T
 * @par Description
 *  This is a structure for memory address
 */
struct VAL_MEM_ADDR_T { /* union extend 64bits for TEE*/
	union {
		unsigned long u4VA; /* /< [IN/OUT] virtual address */
		unsigned long long u4VA_ext64;
	};
	union {
		unsigned long u4PA; /* /< [IN/OUT] physical address */
		unsigned long long u4PA_ext64;
	};
	union {
		unsigned long u4Size; /* /< [IN/OUT] size */
		unsigned long long u4Size_ext64;
	};
};

/**
 * @par Structure
 *  VAL_VCODEC_THREAD_ID_T
 * @par Description
 *  This is a structure for thread info
 *  u4tid1		[IN/OUT] thread id for single core
 *  u4tid2		[IN/OUT] thread id for single core
 *  u4VCodecThreadNum	[IN/OUT] thread num
 *  u4VCodecThreadID	[IN/OUT] thread id for each thread
 */
struct VAL_VCODEC_THREAD_ID_T {
	unsigned int u4tid1;
	unsigned int u4tid2;
	unsigned int u4VCodecThreadNum;
	unsigned int u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
};

/**
 * @par Structure
 *  VAL_VCODEC_CPU_LOADING_INFO_T
 * @par Description
 *  This is a structure for CPU loading info
 */
struct VAL_VCODEC_CPU_LOADING_INFO_T {
	unsigned long long _cpu_idle_time;   /* /< [OUT] cpu idle time */
	unsigned long long _thread_cpu_time; /* /< [OUT] thread cpu time */
	unsigned long long _sched_clock;     /* /< [OUT] sched clock */
	unsigned int _inst_count;	    /* /< [OUT] inst count */
};

/**
 * @par Structure
 *  VAL_VCODEC_CPU_OPP_LIMIT_T
 * @par Description
 *  This is a structure for CPU opp limit info
 */
struct VAL_VCODEC_CPU_OPP_LIMIT_T {
	int limited_freq; /* /< [IN] limited freq */
	int limited_cpu;  /* /< [IN] limited cpu */
	int enable;       /* /< [IN] enable */
};

/**
 * @par Structure
 *  VAL_VCODEC_M4U_BUFFER_CONFIG_T
 * @par Description
 *  This is a structure for m4u buffer config
 *  eMemCodec		[IN] memory usage for encoder or decoder
 *  cache_coherent	[IN] cache coherent or not
 *  security		[IN] security or not
 */
struct VAL_VCODEC_M4U_BUFFER_CONFIG_T {
	enum VAL_MEM_CODEC_T eMemCodec;
	unsigned int cache_coherent;
	unsigned int security;
};

/**
 * @par Structure
 *  VAL_MEMORY_T
 * @par Description
 *  This is a parameter for memory usaged function
 *  u4MemSign		[IN]	 memory signature
 *  eMemType		[IN]	 The allocation memory type
 *  u4MemSize		[IN]	 The size of memory allocation
 *  eAlignment		[IN]	 The memory byte alignment setting
 *  eMemCodec		[IN]	 The memory codec for VENC or VDEC
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]	 The size of reserved parameter structure
 *  pvReservedPmem	[IN/OUT] The reserved parameter
 */
struct VAL_MEMORY_T { /* union extend 64bits for TEE*/
	unsigned int u4MemSign;
	enum VAL_MEM_TYPE_T eMemType;
	union {
		unsigned long u4MemSize;
		unsigned long long u4MemSize_ext64;
	};
	union {
		void *pvMemVa;
		unsigned long long pvMemVa_ext64;
	};
	union {
		void *pvMemPa;
		unsigned long long pvMemPa_ext64;
	};
	enum VAL_MEM_ALIGN_T eAlignment;
	union {
		void *pvAlignMemVa;
		unsigned long long pvAlignMemVa_ext64;
	};
	union {
		void *pvAlignMemPa;
		unsigned long long pvAlignMemPa_ext64;
	};
	enum VAL_MEM_CODEC_T eMemCodec;
	unsigned int i4IonShareFd;

	union {
		struct ion_handle *pIonBufhandle;
		unsigned long long pIonBufhandle_ext64;
	};
	union {
		void *pvReserved;
		unsigned long long pvReserved_ext64;
	};
	union {
		unsigned long u4ReservedSize;
		unsigned long long u4ReservedSize_ext64;
	};
#ifdef __EARLY_PORTING__
	union {
		void *pvReservedPmem;
		unsigned long long pvReservedPmem_ext64;
	};
#endif
	unsigned int i4IonDevFd;
};

/**
 * @par Structure
 *  VAL_RECORD_SIZE_T
 * @par Description
 *  This is a parameter for setting record size to EMI controller
 *  u4FrmWidth		[IN] Frame Width, (may not 16 byte-align)
 *  u4FrmHeight		[IN] Frame Height, (may not 16 byte-align)
 *  u4BufWidth		[IN] Buffer Width, (must 16 byte-align)
 *  u4BufHeight		[IN] Buffer Height, (must 16 byte-align)
 */
struct VAL_RECORD_SIZE_T {
	unsigned int u4FrmWidth;
	unsigned int u4FrmHeight;
	unsigned int u4BufWidth;
	unsigned int u4BufHeight;
};

/**
 * @par Structure
 *  VAL_ATOI_T
 * @par Description
 *  This is a parameter for eVideoAtoi()
 *  pvStr		[IN]	 Null-terminated String to be converted
 *  i4Result		[Out]	returns the int value produced by interpreting
 *					the input characters as a number.
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]	 The size of reserved parameter structure
 */
struct VAL_ATOI_T {
	void *pvStr;
	int i4Result;
	void *pvReserved;
	unsigned int u4ReservedSize;
};

/**
 * @par Structure
 *  VAL_STRSTR_T
 * @par Description
 *  This is a parameter for eVideoStrStr()
 *  pvStr		[IN]	 Null-terminated string to search.
 *  pvStrSearch		[IN]	 Null-terminated string to search for
 *  pvStrResult		[Out]	Returns a pointer to the first occurrence of
 *					strSearch in str or NULL if strSearch
 *					does not appear in str.
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]	 The size of reserved parameter structure
 */
struct VAL_STRSTR_T {
	void *pvStr;
	void *pvStrSearch;
	void *pvStrResult;
	void *pvReserved;
	unsigned int u4ReservedSize;
};

/**
 * @par Structure
 *  VAL_ISR_T
 * @par Description
 *  This is a parameter for ISR related function
 *  pvHandle		[IN]	 The video codec driver handle
 *  u4HandleSize	[IN]	 The size of video codec driver handle
 *  eDriverType		[IN]	 The driver type
 *  pvIsrFunction	[IN]	 The isr function
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]	 The size of reserved parameter structure
 *  u4TimeoutMs		[IN]	 The timeout in ms
 *  u4IrqStatusNum	[IN]	 The num of return registers when HW done
 *  u4IrqStatus		[IN/OUT] The value of return registers when HW done
 */
struct VAL_ISR_T {
	void *pvHandle;
	unsigned int u4HandleSize;
	enum VAL_DRIVER_TYPE_T eDriverType;
	void *pvIsrFunction;
	void *pvReserved;
	unsigned int u4ReservedSize;
	unsigned int u4TimeoutMs;
	unsigned int u4IrqStatusNum;
	unsigned int u4IrqStatus[IRQ_STATUS_MAX_NUM];
};

/**
 * @par Structure
 *  VAL_HW_LOCK_T
 * @par Description
 *  This is a parameter for HW Lock/UnLock related function
 *  pvHandle		[IN]	 The video codec driver handle
 *  u4HandleSize	[IN]	 The size of video codec driver handle
 *  pvLock		[IN/OUT] The Lock discriptor
 *  u4TimeoutMs		[IN]	 The timeout ms
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]	 The size of reserved parameter structure
 *  eDriverType		[IN]	 The driver type
 *  bSecureInst		[IN]	 True if this is a secure instance
 *								//
 *MTK_SEC_VIDEO_PATH_SUPPORT
 */
struct VAL_HW_LOCK_T {
	void *pvHandle;
	unsigned int u4HandleSize;
	void *pvLock;
	unsigned int u4TimeoutMs;
	void *pvReserved;
	unsigned int u4ReservedSize;
	enum VAL_DRIVER_TYPE_T eDriverType;
	char bSecureInst;
};

/**
 * @par Structure
 *  VAL_TIME_T
 * @par Description
 *  This is a structure for system time.
 */
struct VAL_TIME_T {
	unsigned int u4Sec;  /* /< [IN/OUT] second */
	unsigned int u4uSec; /* /< [IN/OUT] micro second */
};

/**
 * @par Enumeration
 *	VAL_SET_TYPE_T
 * @par Description
 *	This is the item for setting val parameter
 */
enum VAL_SET_TYPE_T {
	VAL_SET_TYPE_CURRENT_SCENARIO,  /* /< Set current scenario */
	VAL_SET_TYPE_MCI_PORT_CONFIG,   /* /< Set MCI port config */
	VAL_SET_TYPE_M4U_PORT_CONFIG,   /* /< Set M4U port config */
	VAL_SET_TYPE_SET_TCM_ON,	/* /< Set TCM on */
	VAL_SET_TYPE_SET_TCM_OFF,       /* /< Set TCM off */
	VAL_SET_TYPE_SET_AV_TASK_GROUP, /* /< Set AV task grouping */
	VAL_SET_FRAME_INFO, /* /< Set current frame info for PM QoS */
};

/**
 * @par Enumeration
 *	VAL_GET_TYPE_T
 * @par Description
 *	This is the item for getting val parameter
 *	Get current scenario reference count
 *	Get LCM info
 */
enum VAL_GET_TYPE_T {
	VAL_GET_TYPE_CURRENT_SCENARIO_CNT,
	VAL_GET_TYPE_LCM_INFO,
};

/**
 * @par Enumeration
 *	VAL_VCODEC_SCENARIO
 * @par Description
 *	This is the item for get/setting current vcodec scenario
 */
enum VAL_VCODEC_SCENARIO_T {
	VAL_VCODEC_SCENARIO_VENC_1080P = 0x1, /* /< Camera recording 1080P */
	VAL_VCODEC_SCENARIO_VDEC_1080P = 0x2, /* /< Playback 1080P */
	VAL_VCODEC_SCENARIO_VENC_WFD = 0x4,   /* /< Wifi-display encoding */
	VAL_VCODEC_SCENARIO_VDEC_60FPS = 0x8, /* /< Playback 60fps video */
	VAL_VCODEC_SCENARIO_VDEC_4K = 0x10,   /* /< Playback 4K */
	VAL_VCODEC_SCENARIO_VDEC_2K = 0x20,   /* /< Playback 2K */
	VAL_VCODEC_SCENARIO_VENC_4K = 0x40,   /* /< VR 4K */
};

/**
 * @par Structure
 *  VAL_CURRENT_SCENARIO_T
 * @par Description
 *  This is a structure for set/get current scenario
 *  u4Scenario	[IN/OUT] set/get current scenario
 *  u4OnOff	[IN] set on/off (increment/decrement) 1 = inc, 0 = dec
 */
struct VAL_CURRENT_SCENARIO_T {
	unsigned int u4Scenario;
	unsigned int u4OnOff;
};

/**
 * @par Structure
 *  VAL_CURRENT_SCENARIO_CNT_T
 * @par Description
 *  This is a structure for set/get current scenario reference count
 *  u4Scenario		[IN] current scenario type
 *  u4ScenarioRefCount	[OUT] current scenario reference count
 */
struct VAL_CURRENT_SCENARIO_CNT_T {
	unsigned int u4Scenario;
	unsigned int u4ScenarioRefCount;
};

/**
 * @par Structure
 *  VAL_MCI_PORT_CONFIG_T
 * @par Description
 *  This is a structure for set/get MCI port config
 *  eMemCodecType	[IN] memory type - decoder/encoder
 *  u4Config		[IN] set port config
 */
struct _VAL_MCI_PORT_CONFIG_T {
	enum VAL_MEM_CODEC_T eMemCodecType;
	unsigned int u4Config;
};

/**
 * @par Structure
 *  VAL_LCM_INFO_T
 * @par Description
 *  This is a structure for get LCM info
 */
struct VAL_LCM_INFO_T {
	unsigned int u4Width;  /* /< [OUT] width */
	unsigned int u4Height; /* /< [OUT] height */
};

/* /< VAL_M4UPORT_DEFAULT_ALL = 1, config all M4U port for VENC or VDEC */
#define VAL_M4U_PORT_ALL (-1)

/**
 * @par Structure
 *  VAL_M4U_MPORT_CONFIG_T
 * @par Description
 *  This is a parameter for eVideoSetParam() input structure
 *  eMemCodec	[IN]  The memory codec for VENC or VDEC
 *  i4M4UPortID	[IN]  config port ID
 *			(VAL_M4U_PORT_ALL[-1] = config all VENC or VDEC)
 *  bSecurity	[IN]  config port security
 *  bVirtuality	[IN]  config port virtuality
 */
struct VAL_M4U_MPORT_CONFIG_T {
	enum VAL_MEM_CODEC_T eMemCodec;
	unsigned int i4M4UPortID;
	char bSecurity;
	char bVirtuality;
};

/* for DirectLink Meta Mode + */
#define META_HANDLE_LIST_MAX 50

struct VAL_MetaBufInfo {
	void *pNativeHandle;
	unsigned long u4VA;
	unsigned long u4PA;
	unsigned int u4BuffSize;
	char bUseION;
	int fd;
	struct ion_handle *pIonBufhandle;
};

struct VAL_MetaHandleList {
	int mIonDevFd;
	struct VAL_MetaBufInfo rMetaBufInfo[META_HANDLE_LIST_MAX];
	char fgSeqHdrEncoded;
};

struct VAL_BufInfo {
	unsigned char fgIsConfigData;
	unsigned long u4BSVA;
	unsigned char fgBSStatus;
	unsigned char fgIsKeyFrame;
	unsigned int u4BSSize;
};
/* for DirectLink Meta Mode - */

struct VAL_FRAME_INFO_T {
	void *handle; /* driver handle */
	enum VAL_DRIVER_TYPE_T driver_type;
	unsigned int input_size; /* input bitstream bytes */
	unsigned int frame_width;
	unsigned int frame_height; /* field pic has half height */
	/* 0: intra, 1: inter 1 ref, 2: inter 2 ref, 3: copy */
	unsigned int frame_type;
	unsigned int is_compressed; /* is output buffer compressed */
};

#endif /* #ifndef _VAL_TYPES_PUBLIC_H_ */

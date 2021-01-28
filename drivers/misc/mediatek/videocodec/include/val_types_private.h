/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _VAL_TYPES_PRIVATE_H_
#define _VAL_TYPES_PRIVATE_H_

#include "val_types_public.h"


/* #define __EARLY_PORTING__ */

#define OALMEM_STATUS_NUM 16

/**
 * @par Enumeration
 *   VAL_HW_COMPLETE_T
 * @par Description
 *   This is polling or interrupt for waiting for HW done
 */
enum VAL_HW_COMPLETE_T {
	VAL_POLLING_MODE = 0,                       /* /< polling */
	VAL_INTERRUPT_MODE,                         /* /< interrupt */
	VAL_MODE_MAX = 0xFFFFFFFF                   /* /< Max result */
};


/**
 * @par Enumeration
 *   VAL_CODEC_TYPE_T
 * @par Description
 *   This is the item in VAL_OBJECT_T for open driver type and
 *                    in VAL_CLOCK_T for clock setting and
 *                    in VAL_ISR_T for irq line setting
 */
enum VAL_CODEC_TYPE_T {
	VAL_CODEC_TYPE_NONE = 0,		/* /< None */
	VAL_CODEC_TYPE_MP4_ENC,			/* /< MP4 encoder */
	VAL_CODEC_TYPE_MP4_DEC,			/* /< MP4 decoder */
	VAL_CODEC_TYPE_H263_ENC,		/* /< H.263 encoder */
	VAL_CODEC_TYPE_H263_DEC,		/* /< H.263 decoder */
	VAL_CODEC_TYPE_H264_ENC,		/* /< H.264 encoder */
	VAL_CODEC_TYPE_H264_DEC,		/* /< H.264 decoder */
	VAL_CODEC_TYPE_SORENSON_SPARK_DEC,	/* /< Sorenson Spark decoder */
	VAL_CODEC_TYPE_VC1_SP_DEC,	/* /< VC-1 simple profile decoder */
	VAL_CODEC_TYPE_RV9_DEC,			/* /< RV9 decoder */
	VAL_CODEC_TYPE_MP1_MP2_DEC,		/* /< MPEG1/2 decoder */
	VAL_CODEC_TYPE_XVID_DEC,		/* /< Xvid decoder */
	VAL_CODEC_TYPE_DIVX4_DIVX5_DEC,		/* /< Divx4/5 decoder */
	VAL_CODEC_TYPE_VC1_MP_WMV9_DEC,	/* /< VC-1 main profile(WMV9) decoder */
	VAL_CODEC_TYPE_RV8_DEC,			/* /< RV8 decoder */
	VAL_CODEC_TYPE_WMV7_DEC,		/* /< WMV7 decoder */
	VAL_CODEC_TYPE_WMV8_DEC,		/* /< WMV8 decoder */
	VAL_CODEC_TYPE_AVS_DEC,			/* /< AVS decoder */
	VAL_CODEC_TYPE_DIVX_3_11_DEC,		/* /< Divx3.11 decoder */
	/* /< H.264 main profile decoder (due to different packet) == 20 */
	VAL_CODEC_TYPE_H264_DEC_MAIN,
	VAL_CODEC_TYPE_MAX = 0xFFFFFFFF		/* /< Max driver type */
};


enum VAL_CACHE_TYPE_T {
	VAL_CACHE_TYPE_CACHABLE = 0,
	VAL_CACHE_TYPE_NONCACHABLE,
	VAL_CACHE_TYPE_MAX = 0xFFFFFFFF

};


/**
 * @par Structure
 *  VAL_INTMEM_T
 * @par Description
 *  This is a parameter for eVideoIntMemUsed()
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  u4MemSize		[OUT]    The size of internal memory
 *  pvMemVa		[OUT]    The internal memory start virtual address
 *  pvMemPa		[OUT]    The internal memory start physical address
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct VAL_INTMEM_T {
	void		*pvHandle;
	unsigned int	u4HandleSize;
	unsigned int	u4MemSize;
	void		*pvMemVa;
	void		*pvMemPa;
	void		*pvReserved;
	unsigned int    u4ReservedSize;
};


/**
 * @par Structure
 *  VAL_EVENT_T
 * @par Description
 *  This is a parameter for eVideoWaitEvent() and eVideoSetEvent()
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  pvWaitQueue		[IN]     The waitqueue discription
 *  pvEvent		[IN]     The event discription
 *  u4TimeoutMs		[IN]     The timeout ms
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct VAL_EVENT_T {
	void		*pvHandle;
	unsigned int	u4HandleSize;
	void		*pvWaitQueue;
	void		*pvEvent;
	unsigned int	u4TimeoutMs;
	void		*pvReserved;
	unsigned int	u4ReservedSize;
};


/**
 * @par Structure
 *  VAL_MUTEX_T
 * @par Description
 *  This is a parameter for eVideoWaitMutex() and eVideoReleaseMutex()
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  pvMutex		[IN]     The Mutex discriptor
 *  u4TimeoutMs		[IN]     The timeout ms
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct VAL_MUTEX_T {
	void		*pvHandle;
	unsigned int	u4HandleSize;
	void		*pvMutex;
	unsigned int	u4TimeoutMs;
	void		*pvReserved;
	unsigned int	u4ReservedSize;
};


/**
 * @par Structure
 *  VAL_POWER_T
 * @par Description
 *  This is a parameter for eVideoHwPowerCtrl()
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  eDriverType		[IN]     The driver type
 *  fgEnable		[IN]     Enable or not.
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct VAL_POWER_T {
	void		*pvHandle;
	unsigned int	u4HandleSize;
	enum VAL_DRIVER_TYPE_T	eDriverType;
	char		fgEnable;
	void		*pvReserved;
	unsigned int	u4ReservedSize;
};


/**
 * @par Structure
 *  VAL_MMAP_T
 * @par Description
 *  This is a parameter for eVideoMMAP() and eVideoUNMAP()
 *  pvHandle		[IN]     The video codec driver handle
 *  u4HandleSize	[IN]     The size of video codec driver handle
 *  pvMemPa		[IN]     The physical memory address
 *  u4MemSize		[IN]     The memory size
 *  pvMemVa		[IN]     The mapped virtual memory address
 *  pvReserved		[IN/OUT] The reserved parameter
 *  u4ReservedSize	[IN]     The size of reserved parameter structure
 */
struct VAL_MMAP_T {
	void		*pvHandle;
	unsigned int	u4HandleSize;
	void		*pvMemPa;
	unsigned int	u4MemSize;
	void		*pvMemVa;
	void		*pvReserved;
	unsigned int	u4ReservedSize;
};

/**
 *   u4ReadAddr		[IN]  memory source address in VA
 *   u4ReadData		[OUT] memory data
 */
struct VAL_VCODEC_OAL_MEM_STAUTS_T {
	unsigned long	u4ReadAddr;
	unsigned int	u4ReadData;
};


struct VAL_VCODEC_OAL_HW_REGISTER_T {
	/*
	 *  /< [IN/OUT] HW is Completed or not, set by driver & clear by codec
	 *     (0: not completed or still in lock status;
	 *     1: HW is completed or in unlock status)
	 */
	unsigned int	u4HWIsCompleted;
	/*
	 *  /< [OUT]    HW is Timeout or not, set by driver & clear by codec
	 *     (0: not in timeout status;
	 *     1: HW is in timeout status)
	 */
	unsigned int	u4HWIsTimeout;
	/* /< [IN]     Number of HW register need to store; */
	unsigned int	u4NumOfRegister;
	struct VAL_VCODEC_OAL_MEM_STAUTS_T *pHWStatus;
};


struct VAL_VCODEC_OAL_HW_CONTEXT_T {
	struct VAL_VCODEC_OAL_HW_REGISTER_T	*Oal_HW_reg;
	unsigned int			*Oal_HW_mem_reg;
	unsigned int			*kva_Oal_HW_mem_reg;
	unsigned long			pa_Oal_HW_mem_reg;
	unsigned long			ObjId;
	struct VAL_EVENT_T			IsrEvent;
	unsigned int			slotindex;
	unsigned int			u4VCodecThreadNum;
	unsigned int			u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
	/* physical address of the owner handle */
	unsigned long			pvHandle;
	unsigned int			u4NumOfRegister;
	/* MAX 16 items could be read; //kernel space access register */
	struct VAL_VCODEC_OAL_MEM_STAUTS_T oalmem_status[OALMEM_STATUS_NUM];
	unsigned long			kva_u4HWIsCompleted;
	unsigned long			kva_u4HWIsTimeout;
	unsigned int			tid1;
	unsigned int			tid2;

	/* record VA, PA */
	unsigned int			*va1;
	unsigned int			*va2;
	unsigned int			*va3;
	unsigned int			pa1;
	unsigned int			pa2;
	unsigned int			pa3;
};


struct VAL_VCODEC_CORE_LOADING_T {
	int CPUid;                              /* [in] */
	int Loading;                            /* [out] */
};


struct VAL_INIT_HANDLE {
	int i4DriverType;
	int i4VENCLivePhoto;
};

#endif /* #ifndef _VAL_TYPES_PRIVATE_H_ */

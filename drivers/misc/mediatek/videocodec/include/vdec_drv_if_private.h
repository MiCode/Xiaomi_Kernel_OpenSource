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

#ifndef _VDEC_DRV_IF_PRIVATE_H_
#define _VDEC_DRV_IF_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_private.h"
#include "vdec_drv_if_public.h"


typedef enum {
	UNKNOWN_FBTYPE = 0,                         /* /< Unknown type */
	VDEC_DRV_FBTYPE_YUV420_BK_16x1 = (1 << 0),  /* /< YCbCr 420 block in three planar */
	VDEC_DRV_FBTYPE_YUV420_BK_8x2  = (1 << 1),  /* /< YCbCr 420 block in three planar */
	VDEC_DRV_FBTYPE_YUV420_BK_4x4  = (1 << 2),  /* /< YCbCr 420 block in three planar */
	VDEC_DRV_FBTYPE_YUV420_RS      = (1 << 3),  /* /< YCbCr 420 raster scan in three planar */
	VDEC_DRV_FBTYPE_RGB565_RS      = (1 << 4)   /* /< RGB565 in one planar */
}
VDEC_DRV_FBTYPE_T;


/**
 * @par Structure
 *  VDEC_DRV_BUFFER_CONTROL_T
 * @par Description
 *  Type of buffer control
 *  - Here are two types of buffer
 *    - 1.Reference buffer
 *    - 2.Display buffer
 *  - Buffer can be fixed size or derived from memory pool.
 *  - Buffer can be created from internal or external memory.
 */
typedef enum {
	/* /< Unknown Type */
	VDEC_DRV_BUFFER_CONTROL_UNKNOWN                    = 0,

	/* /< Reference frame and Display frame share the same external buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_IS_DISP_EXT            = (1 << 0),

	/* /< Reference frame and Display frame share the same internal buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_IS_DISP_INT            = (1 << 1),

	/* /< Reference frame and Display frame share the same external memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_IS_DISP_EXT_POOL       = (1 << 2),

	/* /< Reference frame and Display frame share the same internal memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_IS_DISP_INT_POOL       = (1 << 3),

	/* /< Reference frame uses external buffer and Display frame use another external buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_DISP_EXT           = (1 << 4),

	/* /< Reference frame uses external buffer and Display frame uses internal buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_DISP_INT           = (1 << 5),

	/* /< Reference frame uses external buffer and Display frame uses external memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_DISP_EXT_POOL      = (1 << 6),

	/* /< Reference frame uses external buffer and Display frame uses internal memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_DISP_INT_POOL      = (1 << 7),

	/* /< Reference frame uses external memory pool and Display frame use external buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_POOL_DISP_EXT      = (1 << 8),

	/* /< Reference frame uses external memory pool and Display frame uses internal buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_POOL_DISP_INT      = (1 << 9),

	/* /< Reference frame uses external memory pool and Display frame uses external memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_POOL_DISP_EXT_POOL = (1 << 10),

	/* /< Reference frame uses external memory pool and Display frame uses internal memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_EXT_POOL_DISP_INT_POOL = (1 << 11),

	/* /< Reference frame uses internal buffer and Display frame use external buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_DISP_EXT           = (1 << 12),

	/* /< Reference frame uses internal buffer and Display frame uses internal buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_DISP_INT           = (1 << 13),

	/* /< Reference frame uses internal buffer and Display frame uses external memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_DISP_EXT_POOL      = (1 << 14),

	/* /< Reference frame uses internal buffer and Display frame uses internal memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_DISP_INT_POOL      = (1 << 15),

	/* /< Reference frame uses internal memory pool and Display frame use external buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_POOL_DISP_EXT      = (1 << 16),

	/* /< Reference frame uses internal memory pool and Display frame uses internal buffer */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_POOL_DISP_INT      = (1 << 17),

	/* /< Reference frame uses internal memory pool and Display frame uses external memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_POOL_DISP_EXT_POOL = (1 << 18),

	/* /< Reference frame uses external memory pool and Display frame uses another internal memory pool */
	VDEC_DRV_BUFFER_CONTROL_REF_INT_POOL_DISP_INT_POOL = (1 << 19)
} VDEC_DRV_BUFFER_CONTROL_T;


/**
 * @par Structure
 *  VDEC_DRV_DOWNSIZE_RATIO_T
 * @par Description
 *  DownSize Ratio
 *  - The aspect ratio of frame is kept after downsizing.
 */
typedef enum {
	VDEC_DRV_DOWNSIZE_RATIO_UNKNOWN = 0,        /* /< Unknown ratio */
	VDEC_DRV_DOWNSIZE_RATIO_1_1     = (1 << 0), /* /< Original ratio */
	VDEC_DRV_DOWNSIZE_RATIO_1_2     = (1 << 1), /* /< ratio = 1/2 */
	VDEC_DRV_DOWNSIZE_RATIO_1_3     = (1 << 2), /* /< ratio = 1/3 */
	VDEC_DRV_DOWNSIZE_RATIO_1_4     = (1 << 3), /* /< ratio = 1/4 */
	VDEC_DRV_DOWNSIZE_RATIO_1_5     = (1 << 4), /* /< ratio = 1/5 */
	VDEC_DRV_DOWNSIZE_RATIO_1_6     = (1 << 5), /* /< ratio = 1/6 */
	VDEC_DRV_DOWNSIZE_RATIO_1_7     = (1 << 6), /* /< ratio = 1/7 */
	VDEC_DRV_DOWNSIZE_RATIO_1_8     = (1 << 7)  /* /< ratio = 1/8 */
} VDEC_DRV_DOWNSIZE_RATIO_T;


/**
 * @par Structure
 *  VDEC_DRV_PIC_STRUCT_T
 * @par Description
 *  [Unused]Picture Struct
 *  - Consecutive Frame or filed
 *  - Separated  top/bottom field
 */
typedef enum {
	VDEC_DRV_PIC_STRUCT_UNKNOWN = 0,            /* /< Unknown */
	VDEC_DRV_PIC_STRUCT_CONSECUTIVE_FRAME,      /* /< Consecutive Frame */
	VDEC_DRV_PIC_STRUCT_CONSECUTIVE_TOP_FIELD,  /* /< Consecutive top field */
	VDEC_DRV_PIC_STRUCT_CONSECUTIVE_BOT_FIELD,  /* /< Consecutive bottom field */
	VDEC_DRV_PIC_STRUCT_SEPARATED_TOP_FIELD,    /* /< Separated  top field */
	VDEC_DRV_PIC_STRUCT_SEPARATED_BOT_FIELD,     /* /< Separated  bottom field */
	VDEC_DRV_PIC_STRUCT_FIELD,
} VDEC_DRV_PIC_STRUCT_T;


/**
 * @par Structure
 *  VDEC_DRV_FRAME_RATE_T
 * @par Description
 *  Frame rate types
 */
typedef enum {
	VDEC_DRV_FRAME_RATE_UNKNOWN = 0,            /* /< Unknown fps */
	VDEC_DRV_FRAME_RATE_23_976,                 /* /< fps = 24000/1001 (23.976...) */
	VDEC_DRV_FRAME_RATE_24,                     /* /< fps = 24 */
	VDEC_DRV_FRAME_RATE_25,                     /* /< fps = 25 */
	VDEC_DRV_FRAME_RATE_29_97,                  /* /< fps = 30000/1001 (29.97...) */
	VDEC_DRV_FRAME_RATE_30,                     /* /< fps = 30 */
	VDEC_DRV_FRAME_RATE_50,                     /* /< fps = 50 */
	VDEC_DRV_FRAME_RATE_59_94,                  /* /< fps = 60000/1001 (59.94...) */
	VDEC_DRV_FRAME_RATE_60,                     /* /< fps = 60 */
	VDEC_DRV_FRAME_RATE_120,                    /* /< fps = 120 */
	VDEC_DRV_FRAME_RATE_1,                      /* /< fps = 1 */
	VDEC_DRV_FRAME_RATE_5,                      /* /< fps = 5 */
	VDEC_DRV_FRAME_RATE_8,                      /* /< fps = 8 */
	VDEC_DRV_FRAME_RATE_10,                     /* /< fps = 10 */
	VDEC_DRV_FRAME_RATE_12,                     /* /< fps = 12 */
	VDEC_DRV_FRAME_RATE_15,                     /* /< fps = 15 */
	VDEC_DRV_FRAME_RATE_16,                     /* /< fps = 16 */
	VDEC_DRV_FRAME_RATE_17,                     /* /< fps = 17 */
	VDEC_DRV_FRAME_RATE_18,                     /* /< fps = 18 */
	VDEC_DRV_FRAME_RATE_20,                     /* /< fps = 20 */
	VDEC_DRV_FRAME_RATE_2,                      /* /< fps = 2 */
	VDEC_DRV_FRAME_RATE_6,                      /* /< fps = 6 */
	VDEC_DRV_FRAME_RATE_48,                     /* /< fps = 48 */
	VDEC_DRV_FRAME_RATE_70,                     /* /< fps = 70 */
	VDEC_DRV_FRAME_RATE_VARIABLE                /* /< fps = VBR */
} VDEC_DRV_FRAME_RATE_T;


/**
 * @par Structure
 *  VDEC_DRV_POST_PROC_T
 * @par Description
 *  input of type SET_POST_PROC (output is NULL, use return value)
 */
typedef enum {
	VDEC_DRV_POST_PROC_UNKNOWN = 0,             /* /< Unknown */
	VDEC_DRV_POST_PROC_DISABLE,                 /* /< Do not do post-processing */
	VDEC_DRV_POST_PROC_DOWNSIZE,                /* /< Do downsize */
	VDEC_DRV_POST_PROC_RESIZE,                  /* /< Do resize */
	VDEC_DRV_POST_PROC_DEBLOCK,                 /* /< Do deblocking */
	VDEC_DRV_POST_PROC_DEINTERLACE              /* /< Do deinterlace */
} VDEC_DRV_POST_PROC_T;


/**
 * @par Structure
 *  VDEC_DRV_NALU_T
 * @par Description
 *  Buffer Structure
 *  - Store NALU buffer base address
 *  - Store length of NALU buffer
 */

typedef struct {
	VAL_UINT32_T    u4AddrOfNALu;               /* /< NALU buffer base address */
	VAL_UINT32_T    u4LengthOfNALu;             /* /< Length of NALU buffer */
	void            *pReseved;                  /* /< reserved */
} VDEC_DRV_NALU_T;


/**
 * @par Structure
 *  VDEC_DRV_STATISTIC_T
 * @par Description
 *  VDecDrv Statistic information
 */
typedef struct __VDEC_DRV_STATISTIC_T {
	VAL_UINT32_T   u4DecTimeMax;                /* /< [Out] Decode one frame period, Max. */
	VAL_UINT32_T   u4DecTimeMin;                /* /< [Out] Decode one frame period, Min. */
	VAL_UINT32_T   u4DecTimeAvg;                /* /< [Out] Decode one frame period, Average. */
} VDEC_DRV_STATISTIC_T;

/**
 * @par Structure
 *  P_VDEC_DRV_STATISTIC_T
 * @par Description
 *  Pointer of VDEC_DRV_STATISTIC_T
 */
typedef VDEC_DRV_STATISTIC_T * P_VDEC_DRV_STATISTIC_T;


/**
 * @par Structure
 *  VDEC_DRV_FBTYPE_T
 * @par Description
 *  Supported frame buffer type in driver layer
 */
typedef struct {
	/* for speedy mode */
	VAL_UINT32_T    nBufferStatus;
	VAL_INT64_T     llLastVideoTime;
	VAL_INT64_T     llCurrentPlayTime;
} DRIVER_HANDLER_T;


/**
 * @par Structure
 *  VDEC_DRV_VIDEO_FBTYPE_T
 * @par Description
 *  Both input and output of type QUERY_FBTYPE
 */
typedef struct __VDEC_DRV_VIDEO_FBTYPE_T {
	VAL_UINT32_T u4FBType;  /* /< VDEC_DRV_FBTYPE */
} VDEC_DRV_VIDEO_FBTYPE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_VIDEO_FBTYPE_T
 * @par Description
 *  Pointer of VDEC_DRV_VIDEO_FBTYPE_T
 */
typedef VDEC_DRV_VIDEO_FBTYPE_T * P_VDEC_DRV_VIDEO_FBTYPE_T;


/**
 * @par Structure
 *  VDEC_DRV_QUERY_BUFFER_MODE_T
 * @par Description
 *  Both input and output of type QUERY_BUFFER_CONTROL
 */
typedef struct __VDEC_DRV_QUERY_BUFFER_MODE_T {
	VAL_UINT32_T u4BufCtrl;        /* /< VDEC_DRV_BUFFER_CONTROL */
} VDEC_DRV_QUERY_BUFFER_MODE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_QUERY_BUFFER_MODE_T
 * @par Description
 *  Pointer of VDEC_DRV_QUERY_BUFFER_MODE_T
 */
typedef VDEC_DRV_QUERY_BUFFER_MODE_T * P_VDEC_DRV_QUERY_BUFFER_MODE_T;


/**
 * @par Structure
 *  VDEC_DRV_QUERY_POOL_SIZE_T
 * @par Description
 *   output of type QUERY_REF_POOL_SIZE and QUERY_DISP_POOL_SIZE (input is NULL)
 */
typedef struct __VDEC_DRV_QUERY_POOL_SIZE_T {
	VAL_UINT32_T    u4Size;     /* /< buffer size of the memory pool */
} VDEC_DRV_QUERY_POOL_SIZE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_QUERY_POOL_SIZE_T
 * @par Description
 *  Pointer of VDEC_DRV_QUERY_POOL_SIZE_T
 */
typedef VDEC_DRV_QUERY_POOL_SIZE_T * P_VDEC_DRV_QUERY_POOL_SIZE_T;

/* output of type DISP_FRAME_BUFFER and FREE_FRAME_BUFFER is P_VDEC_DRV_FRAMEBUF_T (input is NULL) */
/* output of type GET_PICTURE_INFO is P_VDEC_DRV_PICINFO_T (input is NULL) */
/* both input and output of type QUERY_REORDER_ABILITY are NULL (use return value) */


/**
 * @par Structure
 *  VDEC_DRV_QUERY_POOL_DOWNSIZE_T
 * @par Description
 *  output of type QUERY_DOWNSIZE_ABILITY (input is NULL)
 */
typedef struct __VDEC_DRV_QUERY_POOL_DOWNSIZE_T {
	VAL_UINT32_T    u4Ratio;    /* /< VDEC_DRV_DOWNSIZE_RATIO */
} VDEC_DRV_QUERY_POOL_DOWNSIZE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_QUERY_POOL_DOWNSIZE_T
 * @par Description
 *  Pointer of VDEC_DRV_QUERY_POOL_DOWNSIZE_T
 */
typedef VDEC_DRV_QUERY_POOL_DOWNSIZE_T * P_VDEC_DRV_QUERY_POOL_DOWNSIZE_T;


/**
 * @par Structure
 *  VDEC_DRV_QUERY_POOL_RESIZE_T
 * @par Description
 *  input of type QUERY_RESIZE_ABILITY (output is NULL, use return value)
 */
typedef struct __VDEC_DRV_QUERY_POOL_RESIZE_T {
	VAL_UINT32_T    u4OutWidth;     /* /<Width of buffer */
	VAL_UINT32_T    u4OutHeight;    /* /<Height of buffer */
} VDEC_DRV_QUERY_POOL_RESIZE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_QUERY_POOL_RESIZE_T
 * @par Description
 *  Pointer of VDEC_DRV_QUERY_POOL_RESIZE_T
 */
typedef VDEC_DRV_QUERY_POOL_RESIZE_T * P_VDEC_DRV_QUERY_POOL_RESIZE_T;

/* both input and output of type QUERY_DEBLOCK_ABILITY are NULL (use return value) */
/* both input and output of type QUERY_DERING_ABILITY are NULL (use return value) */
/* both input and output of type QUERY_DEINTERLACE_ABILITY are NULL (use return value) */
/* both input and output of type QUERY_DROPFRAME_ABILITY are NULL (use return value) */


/**
 * @par Structure
 *  VDEC_DRV_SET_BUFFER_MODE_T
 * @par Description
 *  input of type VDEC_DRV_SET_BUFFER_MODE_T (output is NULL, use return value)
 */
typedef struct __VDEC_DRV_SET_BUFFER_MODE_T {
	VAL_UINT32_T    u4BufferMode;       /* /< VDEC_DRV_BUFFER_CONTROL */
} VDEC_DRV_SET_BUFFER_MODE_T;

/* input of type SET_FRAME_BUFFER_TYPE is VDEC_DRV_VIDEO_FBTYPE_T (output is NULL, use return value) */

/**
 * @par Structure
 *  P_VDEC_DRV_SET_BUFFER_MODE_T
 * @par Description
 *  Pointer of VDEC_DRV_SET_BUFFER_MODE_T
 */
typedef VDEC_DRV_SET_BUFFER_MODE_T * P_VDEC_DRV_SET_BUFFER_MODE_T;

/* input of type SET_FRAME_BUFFER_TYPE is VDEC_DRV_VIDEO_FBTYPE_T (output is NULL, use return value) */


/**
 * @par Structure
 *  VDEC_DRV_SET_BUFFER_ADDR_T
 * @par Description
 *  input of type FREE_FRAME_BFFER (buffer_len=NULL, output is NULL, use return value)
 */
typedef struct __VDEC_DRV_SET_BUFFER_ADDR_T {
	VAL_MEM_ADDR_T rBufferAddr;         /* /< buffer memory base address */
} VDEC_DRV_SET_BUFFER_ADDR_T;

/**
 * @par Structure
 *  P_VDEC_DRV_SET_BUFFER_ADDR_T
 * @par Description
 *  Pointer of VDEC_DRV_SET_BUFFER_ADDR_T
 */
typedef VDEC_DRV_SET_BUFFER_ADDR_T * P_VDEC_DRV_SET_BUFFER_ADDR_T;

/* input of type SET_REF_EXT_POOL_ADDR and SET_DISP_EXT_POOL_ADDR is
    VDEC_DRV_SET_BUFFER_ADDR_T (output is NULL, use return value) */


/**
 * @par Structure
 *  VDEC_DRV_SET_POST_PROC_MODE_T
 * @par Description
 *  Parameters of set post process mode
 */
typedef struct __VDEC_DRV_SET_POST_PROC_MODE_T {
	VAL_UINT32_T    u4PostProcMode;     /* /< one of VDEC_DRV_POST_PROC */
	VAL_UINT32_T    u4DownsizeRatio;    /* /< if mode is POST_PROC_DOWNSIZE */
	VAL_UINT32_T    u4ResizeWidth;      /* /< if mode is POST_PROC_RESIZE */
	VAL_UINT32_T    u4ResizeHeight;     /* /< if mode is POST_PROC_RESIZE */
} VDEC_DRV_SET_POST_PROC_MODE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_SET_POST_PROC_MODE_T
 * @par Description
 *  Pointer of VDEC_DRV_SET_POST_PROC_MODE_T
 */
typedef VDEC_DRV_SET_POST_PROC_MODE_T * P_VDEC_DRV_SET_POST_PROC_MODE_T;



typedef struct _VDEC_DRV_HW_REG_T {
	VAL_UINT32_T    u4VdecHWBase;
	VAL_UINT32_T    u4VdecHWSYS;
	VAL_UINT32_T    u4VdecMISC;
	VAL_UINT32_T    u4VdecVLD;
	VAL_UINT32_T    u4VdecVLDTOP;
	VAL_UINT32_T    u4VdecMC;
	VAL_UINT32_T    u4VdecAVCVLD;
	VAL_UINT32_T    u4VdecAVCMV;
	VAL_UINT32_T    u4VdecPP;
	VAL_UINT32_T    u4VdecSQT;
	VAL_UINT32_T    u4VdecVP8VLD;
	VAL_UINT32_T    u4VdecVP6VLD;
	VAL_UINT32_T    u4VdecVP8VLD2;
} VDEC_DRV_HW_REG_T;

typedef VDEC_DRV_HW_REG_T * P_VDEC_DRV_HW_REG_T;


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VDEC_DRV_IF_PRIVATE_H_ */

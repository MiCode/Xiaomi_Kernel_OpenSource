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

#ifndef _VDEC_DRV_IF_PUBLIC_H_
#define _VDEC_DRV_IF_PUBLIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "val_types_public.h"


/**
 * @par Enumeration
 *   VDEC_DRV_FBSTSTUS
 * @par Description
 *   This is the item for frame buffer status
 */
typedef enum _VDEC_DRV_FBSTSTUS {
	VDEC_DRV_FBSTSTUS_NORMAL      = 0,          /* /< normal type */
	VDEC_DRV_FBSTSTUS_REPEAT_LAST = (1 << 0),   /* /< repeat last frame */
	VDEC_DRV_FBSTSTUS_NOT_DISPLAY = (1 << 1),   /* /< not displayed */
	VDEC_DRV_FBSTSTUS_NOT_USED    = (1 << 2),   /* /< not used */
}
VDEC_DRV_FBSTSTUS;

/**
 * @par Enumeration
 *   VDEC_DRV_FBSTSTUS
 * @par Description
 *   This is the item for frame buffer status
 */
typedef enum _VDEC_DRV_FBTYPE {
	VDEC_DRV_FBTYPE_NORMAL      = 0,   /* /< normal type */
	VDEC_DRV_FBTYPE_3D_SBS      = 1,   /* /< side by side 3D frame */
	VDEC_DRV_FBTYPE_3D_TAB      = 2,   /* /< top and bottim 3D frame */
}
VDEC_DRV_FBTYPE;


/**
 * @par Enumeration
 *  VDEC_DRV_VIDEO_FORMAT_T
 * @par Description
 *  video_format of VDecDrvCreate()
 */
typedef enum _VDEC_DRV_VIDEO_FORMAT_T {
	VDEC_DRV_VIDEO_FORMAT_UNKNOWN_VIDEO_FORMAT  = 0,            /* /< Unknown video format */
	VDEC_DRV_VIDEO_FORMAT_DIVX311               = (1 << 0),     /* /< Divix 3.11 */
	VDEC_DRV_VIDEO_FORMAT_DIVX4                 = (1 << 1),     /* /< Divix 4 */
	VDEC_DRV_VIDEO_FORMAT_DIVX5                 = (1 << 2),     /* /< Divix 5 */
	VDEC_DRV_VIDEO_FORMAT_XVID                  = (1 << 3),     /* /< Xvid */
	VDEC_DRV_VIDEO_FORMAT_MPEG1                 = (1 << 4),     /* /< MPEG-1 */
	VDEC_DRV_VIDEO_FORMAT_MPEG2                 = (1 << 5),     /* /< MPEG-2 */
	VDEC_DRV_VIDEO_FORMAT_MPEG4                 = (1 << 6),     /* /< MPEG-4 */
	VDEC_DRV_VIDEO_FORMAT_H263                  = (1 << 7),     /* /< H263 */
	VDEC_DRV_VIDEO_FORMAT_H264                  = (1 << 8),     /* /< H264 */
	VDEC_DRV_VIDEO_FORMAT_H265                  = (1 << 9),     /* /< H265 */
	VDEC_DRV_VIDEO_FORMAT_WMV7                  = (1 << 10),    /* /< WMV7 */
	VDEC_DRV_VIDEO_FORMAT_WMV8                  = (1 << 11),    /* /< WMV8 */
	VDEC_DRV_VIDEO_FORMAT_WMV9                  = (1 << 12),    /* /< WMV9 */
	VDEC_DRV_VIDEO_FORMAT_VC1                   = (1 << 13),    /* /< VC1 */
	VDEC_DRV_VIDEO_FORMAT_REALVIDEO8            = (1 << 14),    /* /< RV8 */
	VDEC_DRV_VIDEO_FORMAT_REALVIDEO9            = (1 << 15),    /* /< RV9 */
	VDEC_DRV_VIDEO_FORMAT_VP6                   = (1 << 16),    /* /< VP6 */
	VDEC_DRV_VIDEO_FORMAT_VP7                   = (1 << 17),    /* /< VP7 */
	VDEC_DRV_VIDEO_FORMAT_VP8                   = (1 << 18),    /* /< VP8 */
	VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_PICTURE_MODE = (1 << 19),    /* /< VP8 WEBP PICTURE MODE */
	VDEC_DRV_VIDEO_FORMAT_VP8_WEBP_MB_ROW_MODE  = (1 << 20),    /* /< VP8 WEBP ROW MODE */
	VDEC_DRV_VIDEO_FORMAT_VP9                   = (1 << 21),    /* /< VP9 */
	VDEC_DRV_VIDEO_FORMAT_VP9_WEBP_PICTURE_MODE = (1 << 22),    /* /< VP9 WEBP PICTURE MODE */
	VDEC_DRV_VIDEO_FORMAT_VP9_WEBP_MB_ROW_MODE  = (1 << 23),    /* /< VP9 WEBP ROW MODE */
	VDEC_DRV_VIDEO_FORMAT_AVS                   = (1 << 24),    /* /< AVS */
	VDEC_DRV_VIDEO_FORMAT_MJPEG                 = (1 << 25),    /* /< Motion JPEG */
	VDEC_DRV_VIDEO_FORMAT_S263                  = (1 << 26),    /* /< Sorenson Spark */
	VDEC_DRV_VIDEO_FORMAT_H264HP                = (1 << 27),
	VDEC_DRV_VIDEO_FORMAT_H264SEC               = (1 << 28),
	VDEC_DRV_VIDEO_FORMAT_H265SEC               = (1 << 29),
	VDEC_DRV_VIDEO_FORMAT_VP9SEC                = (1 << 30)
} VDEC_DRV_VIDEO_FORMAT_T;


/**
 * @par Enumeration
 *  VDEC_DRV_H265_VIDEO_PROFILE_T
 * @par Description
 *  video profile for H.265
 */
typedef enum _VDEC_DRV_H265_VIDEO_PROFILE_T {
	VDEC_DRV_H265_VIDEO_PROFILE_UNKNOWN         = 0,            /* /< Unknown video profile */
	VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN       = (1 << 0),      /* /< H265 main profile */
	VDEC_DRV_H265_VIDEO_PROFILE_H265_MAIN_10       = (1 << 1),      /* /< H265 main 10 profile */
	VDEC_DRV_H265_VIDEO_PROFILE_H265_STILL_IMAGE       = (1 << 2)      /* /< H265 still image profile */
} VDEC_DRV_H265_VIDEO_PROFILE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_H264_VIDEO_PROFILE_T
 * @par Description
 *  video profile for H.264
 */
typedef enum _VDEC_DRV_H264_VIDEO_PROFILE_T {
	VDEC_DRV_H264_VIDEO_PROFILE_UNKNOWN                   = 0,          /* /< Unknown video profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_BASELINE             = (1 << 0),   /* /< H264 baseline profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE = (1 << 1),   /* /< H264 constrained baseline profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_MAIN                 = (1 << 2),   /* /< H264 main profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_EXTENDED             = (1 << 3),   /* /< H264 extended profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH                 = (1 << 4),   /* /< H264 high profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH_10              = (1 << 5),   /* /< H264 high 10 profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH422              = (1 << 6),   /* /< H264 high 422 profile */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444              = (1 << 7),   /* /< H264 high 444 profile */

	/* /< H264 high 10 intra profile in Amendment 2 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH_10_INTRA        = (1 << 8),

	/* /< H264 high 422 intra profile in Amendment 2 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH422_INTRA        = (1 << 9),

	/* /< H264 high 444 intra profile in Amendment 2 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444_INTRA        = (1 << 10),

	/* /< H264 CAVLC 444 intra profile in Amendment 2 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_CAVLC444_INTRA       = (1 << 11),

	/* /< H264 high 444 predictive profile in Amendment 2 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_HIGH444_PREDICTIVE   = (1 << 12),

	/* /< H264 scalable baseline profile in Amendment 3 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_BASELINE    = (1 << 13),

	/* /< H264 scalable high profile in Amendment 3 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_HIGH        = (1 << 14),

	/* /< H264 scalable high intra profile in Amendment 3 */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_SCALABLE_HIGH_INTRA  = (1 << 15),

	/* /< Corrigendum 1 (2009) */
	VDEC_DRV_H264_VIDEO_PROFILE_H264_MULTIVIEW_HIGH       = (1 << 16)
} VDEC_DRV_H264_VIDEO_PROFILE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_MPEG_VIDEO_PROFILE_T
 * @par Description
 *  video profile for H263, MPEG2, MPEG4
 */
typedef enum _VDEC_DRV_MPEG_VIDEO_PROFILE_T {
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_0                = (1 << 0),   /* /< H263 Profile 0 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_1                = (1 << 1),   /* /< H263 Profile 1 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_2                = (1 << 2),   /* /< H263 Profile 2 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_3                = (1 << 3),   /* /< H263 Profile 3 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_4                = (1 << 4),   /* /< H263 Profile 4 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_5                = (1 << 5),   /* /< H263 Profile 5 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_6                = (1 << 6),   /* /< H263 Profile 6 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_7                = (1 << 7),   /* /< H263 Profile 7 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_H263_8                = (1 << 8),   /* /< H263 Profile 8 */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_SIMPLE          = (1 << 9),   /* /< MPEG2 Simple Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_MAIN            = (1 << 10),  /* /< MPEG2 Main Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_SNR             = (1 << 11),  /* /< MPEG2 SNR Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_SPATIAL         = (1 << 12),  /* /< MPEG2 Spatial Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG2_HIGH            = (1 << 13),  /* /< MPEG2 High Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_SIMPLE          = (1 << 14),  /* /< MPEG4 Simple Profile */
	VDEC_DRV_MPEG_VIDEO_PROFILE_MPEG4_ADVANCED_SIMPLE = (1 << 15)   /* /< MPEG4 Advanced Simple Profile */
} VDEC_DRV_MPEG_VIDEO_PROFILE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_MS_VIDEO_PROFILE_T
 * @par Description
 *  video profile for VC1, WMV9
 */
typedef enum _VDEC_DRV_MS_VIDEO_PROFILE_T {
	VDEC_DRV_MS_VIDEO_PROFILE_VC1_SIMPLE   = (1 << 0),  /* /< VC-1 Simple Profile */
	VDEC_DRV_MS_VIDEO_PROFILE_VC1_MAIN     = (1 << 1),  /* /< VC-1 Main Profile */
	VDEC_DRV_MS_VIDEO_PROFILE_VC1_ADVANCED = (1 << 2),  /* /< VC-1 Advanced Profile */
	VDEC_DRV_MS_VIDEO_PROFILE_WMV9_SIMPLE  = (1 << 3),  /* /< WMV9 Simple Profile */
	VDEC_DRV_MS_VIDEO_PROFILE_WMV9_MAIN    = (1 << 4),  /* /< WMV9 Main Profile */
	VDEC_DRV_MS_VIDEO_PROFILE_WMV9_COMPLEX = (1 << 5)   /* /< WMV9 Complex Profile */
} VDEC_DRV_MS_VIDEO_PROFILE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_VIDEO_LEVEL_T
 * @par Description
 *  video level
 */
typedef enum _VDEC_DRV_VIDEO_LEVEL_T {
	VDEC_DRV_VIDEO_LEVEL_UNKNOWN = 0,           /* /< Unknown level */
	VDEC_DRV_VIDEO_LEVEL_0,                     /* /< Specified by VC1 */
	VDEC_DRV_VIDEO_LEVEL_1,                     /* /< Specified by H264, VC1, MPEG4, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_1,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_1b,                    /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_1_1,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_1_2,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_1_3,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_2,                     /* /< Specified by H264, VC1, MPEG4, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_2_1,                   /* /< Specified by H264, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_2_1,  /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_2_2,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_3,                     /* /< Specified by H264, VC1, MPEG4, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3,    /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_3_1,                   /* /< Specified by H264, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_3_1,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_3_2,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_4,                     /* /< Specified by H264, VC1, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4,     /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_4_1,                   /* /< Specified by H264, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_4_1,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_4_2,                   /* /< Specified by H264 */
	VDEC_DRV_VIDEO_LEVEL_5,                     /* /< Specified by H264, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5,     /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_5_1,                   /* /< Specified by H264, HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_1,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_5_2,                    /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_5_2,    /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_6,                        /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6,     /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_6_1,                    /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_1,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_6_2,                    /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_HIGH_TIER_LEVEL_6_2,   /* /< Specified by HEVC */
	VDEC_DRV_VIDEO_LEVEL_LOW,                   /* /< Specified by MPEG2, VC1 */
	VDEC_DRV_VIDEO_LEVEL_MEDIUM,                /* /< Specified by MPEG2, VC1 */
	VDEC_DRV_VIDEO_LEVEL_HIGH1440,              /* /< Specified by MPEG2 */
	VDEC_DRV_VIDEO_LEVEL_HIGH                  /* /< Specified by MPEG2, VC1 */

} VDEC_DRV_VIDEO_LEVEL_T;


/**
 * @par Enumeration
 *  VDEC_DRV_RESOLUTION_T
 * @par Description
 *  video resolution support
 */
typedef enum _VDEC_DRV_RESOLUTION_T {
	VDEC_DRV_RESOLUTION_UNKNOWN = 0,    /* /< Unknown resolution */
	VDEC_DRV_RESOLUTION_SUPPORT_QCIF,   /* /< QCIF */
	VDEC_DRV_RESOLUTION_SUPPORT_QVGA,   /* /< QVGA */
	VDEC_DRV_RESOLUTION_SUPPORT_CIF,    /* /< CIF */
	VDEC_DRV_RESOLUTION_SUPPORT_480I,   /* /< 720x480 interlace */
	VDEC_DRV_RESOLUTION_SUPPORT_480P,   /* /< 720x480 progressive */
	VDEC_DRV_RESOLUTION_SUPPORT_576I,   /* /< 720x576 interlace */
	VDEC_DRV_RESOLUTION_SUPPORT_576P,   /* /< 720x576 progressive */
	VDEC_DRV_RESOLUTION_SUPPORT_720P,   /* /< 1280x720 progressive */
	VDEC_DRV_RESOLUTION_SUPPORT_1080I,  /* /< 1920x1080 interlace */
	VDEC_DRV_RESOLUTION_SUPPORT_1080P,   /* /< 1920x1080 progressive */
	VDEC_DRV_RESOLUTION_SUPPORT_2160P   /* /< 4096x2160 progressive */
} VDEC_DRV_RESOLUTION_T;


/**
 * @par Enumeration
 *  VDEC_DRV_QUERY_TYPE_T
 * @par Description
 *  video driver used to query different info
 */
typedef enum _VDEC_DRV_QUERY_TYPE_T {
	VDEC_DRV_QUERY_TYPE_FBTYPE,             /* /< Query VDEC_DRV_QUERY_TYPE_FBTYPE */
	VDEC_DRV_QUERY_TYPE_VIDEO_FORMAT,       /* /< Query VDEC_DRV_QUERY_TYPE_VIDEO_FORMAT */
	VDEC_DRV_QUERY_TYPE_PROPERTY,           /* /< Query VDEC_DRV_PROPERTY_T */
	VDEC_DRV_QUERY_TYPE_CHIP_NAME,          /* /< Query VDEC_DRV_QUERY_TYPE_CHIP_NAME */
	VDEC_DRV_QUERY_TYPE_BUFFER_CONTROL,     /* /< Query VDEC_DRV_QUERY_TYPE_BUFFER_CONTROL */
	VDEC_DRV_QUERY_TYPE_FEATURE_SUPPORTED,   /* /< Query VDEC_DRV_QUERY_TYPE_FEATURE_SUPPORTED */
	VDEC_DRV_QUERY_TYPE_CPUCORE_FREQUENCY,   /* /< Query VDEC_DRV_QUERY_TYPE_CPUCORE_FREQUENCY */
	VDEC_DRV_QUERY_TYPE_UFO_SUPPORT,         /* /< Query VDEC_DRV_QUERY_TYPE_UFO_SUPPORT */
} VDEC_DRV_QUERY_TYPE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_QUERY_TYPE_T
 * @par Description
 *  video driver used to queue multiple input buffers
 */
typedef enum _VDEC_DRV_FEATURE_TYPE_T {
	VDEC_DRV_FEATURE_TYPE_NONE                = 0,        /* /< Empty */
	VDEC_DRV_FEATURE_TYPE_QUEUE_INPUT_BUFFER  = (1 << 0), /* /< Driver will queue multiple input buffers */
} VDEC_DRV_FEATURE_TYPE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_GET_TYPE_T
 * @par Description
 *  video driver used to get/query different info
 */
typedef enum _VDEC_DRV_GET_TYPE_T {
	/* /< how many buffer size of the reference pool needs in driver */
	VDEC_DRV_GET_TYPE_QUERY_REF_POOL_SIZE,

	/* /< how many buffer size of the display pool needs in driver */
	VDEC_DRV_GET_TYPE_QUERY_DISP_POOL_SIZE,

	/* /< return a P_VDEC_DRV_FRAMEBUF_T address (especially in display order != decode order) */
	VDEC_DRV_GET_TYPE_DISP_FRAME_BUFFER,

	/*
	/< return a frame didn't be a reference more
	(when buffer_mode = REF_IS_DISP_EXT, REF_INT_DISP_EXT or REF_INT_POOL_DISP_EXT)
	*/
	VDEC_DRV_GET_TYPE_FREE_FRAME_BUFFER,
	VDEC_DRV_GET_TYPE_GET_PICTURE_INFO,             /* /< return a pointer address point to P_VDEC_DRV_PICINFO_T */
	VDEC_DRV_GET_TYPE_GET_STATISTIC_INFO,           /* /< return statistic information. */
	VDEC_DRV_GET_TYPE_GET_FRAME_MODE,               /* /< return frame mode parameter. */
	VDEC_DRV_GET_TYPE_GET_FRAME_CROP_INFO,          /* /< return frame crop information. */

	/* /< query if driver can re-order the decode order to display order */
	VDEC_DRV_GET_TYPE_QUERY_REORDER_ABILITY,
	VDEC_DRV_GET_TYPE_QUERY_DOWNSIZE_ABILITY,       /* /< query if driver can downsize decoded frame */
	VDEC_DRV_GET_TYPE_QUERY_RESIZE_ABILITY,         /* /< query if driver can resize decoded frame */
	VDEC_DRV_GET_TYPE_QUERY_DEBLOCK_ABILITY,        /* /< query if driver can do deblocking */
	VDEC_DRV_GET_TYPE_QUERY_DEINTERLACE_ABILITY,    /* /< query if driver can do deinterlace */
	VDEC_DRV_GET_TYPE_QUERY_DROPFRAME_ABILITY,      /* /< query if driver can drop frame */

	/* /< query if driver finish decode one frame but no output (main profile with B frame case.) */
	VDEC_DRV_GET_TYPE_GET_DECODE_STATUS_INFO,
	VDEC_DRV_GET_TYPE_GET_PIXEL_FORMAT,             /* /< query the driver output pixel format */
	VDEC_DRV_GET_TYPE_GET_CPU_LOADING_INFO,         /* /< query the cpu loading info from kernel driver */
	VDEC_DRV_GET_TYPE_GET_HW_CRC,                   /* /< query the hw CRC */
	VDEC_DRV_GET_TYPE_GET_CODEC_TIDS,               /* /< query the thread ids from the codec lib */
	VDEC_DRV_GET_TYPE_GET_FRAME_INTERVAL,           /* /< query frame interval from the codec lib */
	VDEC_DRV_GET_TYPE_FREE_INPUT_BUFFER,            /* /< free input buffer */
	VDEC_DRV_GET_TYPE_QUERY_VIDEO_INTERLACING,      /* /< query video interlace information */
	VDEC_DRV_GET_TYPE_QUERY_VIDEO_DPB_SIZE          /* /< query video DPB size */
} VDEC_DRV_GET_TYPE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_PIXEL_FORMAT_T
 * @par Description
 *  pixel format
 */
typedef enum _VDEC_DRV_PIXEL_FORMAT_T {
	VDEC_DRV_PIXEL_FORMAT_NONE =   0,               /* /< None */
	VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER,           /* /< YUV 420 planer */
	VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_MTK,       /* /< YUV 420 planer MTK mode */
	VDEC_DRV_PIXEL_FORMAT_YUV_420_PLANER_UFO,       /* /< YUV 420 planer MTK UFO mode */
	VDEC_DRV_PIXEL_FORMAT_YUV_YV12                  /* /< YUV YV12 */
} VDEC_DRV_PIXEL_FORMAT_T;


/**
 * @par Enumeration
 *  VDEC_DRV_DECODER_TYPE_T
 * @par Description
 *  decoder type
 */
typedef enum _VDEC_DRV_DECODER_TYPE_T {
	VDEC_DRV_DECODER_MTK_HARDWARE = 0,              /* /< MTK software */
	VDEC_DRV_DECODER_MTK_SOFTWARE,                  /* /< MTK hardware */
	VDEC_DRV_DECODER_GOOGLE_SOFTWARE                /* /< google software (default) */
}   VDEC_DRV_DECODER_TYPE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_SET_TYPE_T
 * @par Description
 *  video driver used to set different info
 */
typedef enum _VDEC_DRV_SET_TYPE_T {
	/* /< =1, use timestamp in sVDEC_DRV_FRAMEBUF_T for the picture */
	VDEC_DRV_SET_TYPE_USE_EXT_TIMESTAMP,
	VDEC_DRV_SET_TYPE_SET_BUFFER_MODE,              /* /< value is one of VDEC_DRV_BUFFER_MODE */

	/* /< one of VDEC_DRV_FBTYPE, if output type is the same as decode type, buffer mode can be REF_IS_DISP */
	VDEC_DRV_SET_TYPE_SET_FRAME_BUFFER_TYPE,
	VDEC_DRV_SET_TYPE_FREE_FRAME_BFFER,             /* /< release buffer if DISP BUFFER is allocated from driver */
	VDEC_DRV_SET_TYPE_SET_REF_EXT_POOL_ADDR,        /* /< if use REF_EXT_POOL in SET_BUFFER_MODE */
	VDEC_DRV_SET_TYPE_SET_DISP_EXT_POOL_ADDR,       /* /< if use DISP_EXT_POOL in SET_BUFFER_MODE */
	VDEC_DRV_SET_TYPE_SET_DECODE_MODE,              /* /< set if drop frame */

	/* /< buffer mode cannot set to REF_IS_DISP when using post-processing */
	VDEC_DRV_SET_TYPE_SET_POST_PROC,
	VDEC_DRV_SET_TYPE_SET_STATISTIC_ON,             /* /< enable statistic function. */
	VDEC_DRV_SET_TYPE_SET_STATISTIC_OFF,            /* /< disable statistic function. */
	VDEC_DRV_SET_TYPE_SET_FRAME_MODE,               /* /< set frame mode */
	VDEC_DRV_SET_TYPE_SET_BUF_STATUS_FOR_SPEEDY,    /* /< set buffer status for speedy mode */
	VDEC_DRV_SET_TYPE_SET_LAST_DISPLAY_TIME,        /* /< set the last display time */
	VDEC_DRV_SET_TYPE_SET_CURRENT_PLAY_TIME,        /* /< set the current play time */
	VDEC_DRV_SET_TYPE_SET_CONCEAL_LEVEL,            /* /< error conceal level for decoder */
	VDEC_DRV_SET_TYPE_SET_OMX_TIDS,                 /* /< set omx thread ids */
	VDEC_DRV_SET_TYPE_SET_SWITCH_TVOUT,             /* /< set ot switch to TV OUT */
	VDEC_DRV_SET_TYPE_SET_CODEC_COLOR_FORAMT,       /* /< set codec color format */
	VDEC_DRV_SET_TYPE_SET_CODEC_YUV_STRIDE,         /* /< set codec yuv stride */
	VDEC_DRV_SET_TYPE_SET_FRAMESIZE,                /* /< set frame size from caller for MPEG4 decoder */

	/* /< use the max suppoerted size as output buffer size. for smooth */
	VDEC_DRV_SET_TYPE_SET_FIXEDMAXOUTPUTBUFFER,
	VDEC_DRV_SET_TYPE_SET_UFO_DECODE,
} VDEC_DRV_SET_TYPE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_DECODE_MODE_T
 * @par Description
 *  video driver decode mode
 */
typedef enum _VDEC_DRV_DECODE_MODE_T {
	VDEC_DRV_DECODE_MODE_UNKNOWN = 0,               /* /< Unknown */
	VDEC_DRV_DECODE_MODE_NORMAL,                    /* /< decode all frames (no drop) */
	VDEC_DRV_DECODE_MODE_I_ONLY,                    /* /< skip P and B frame */
	VDEC_DRV_DECODE_MODE_B_SKIP,                    /* /< skip B frame */
	VDEC_DRV_DECODE_MODE_DROPFRAME,                 /* /< display param1 frames & drop param2 frames */
	VDEC_DRV_DECODE_MODE_NO_REORDER,                /* /< output display ASAP without reroder */
	VDEC_DRV_DECODE_MODE_THUMBNAIL,                 /* /< thumbnail mode */

	/* /< skip reference check mode - force decode and display from first frame */
	VDEC_DRV_DECODE_MODE_SKIP_REFERENCE_CHECK,

	/* /< decode immediately no check. (parser should make sure the completed frame) */
	VDEC_DRV_DECODE_MODE_LOW_LATENCY_DECODE,
} VDEC_DRV_DECODE_MODE_T;


/**
 * @par Enumeration
 *  VDEC_DRV_MRESULT_T
 * @par Description
 *  Driver return type
 */
typedef enum __VDEC_DRV_MRESULT_T {
	VDEC_DRV_MRESULT_OK = 0,                        /* /< OK */
	VDEC_DRV_MRESULT_FAIL,                          /* /< Fail */
	VDEC_DRV_MRESULT_FATAL,                         /* /< Fatal error to stop. */
	VDEC_DRV_MRESULT_RESOLUTION_CHANGED,            /* /< Represent resoluion changed */
	VDEC_DRV_MRESULT_NEED_MORE_OUTPUT_BUFFER,       /* /< Represent need more output buffer */
	VDEC_DRV_MRESULT_MAX = 0x0FFFFFFF               /* /< Max Value */
} VDEC_DRV_MRESULT_T;

typedef enum _VDEC_DRV_COLOR_PRIMARIES_E {
	COLOR_PRIMARIES_NO_INFO = 0,
	COLOR_PRIMARIES_BT601,
	COLOR_PRIMARIES_BT709,
	COLOR_PRIMARIES_BT2020
} VDEC_DRV_COLOR_PRIMARIES_E;

typedef struct __VDEC_DRV_COLOR_PRIMARIES_INFO_T {
	VAL_BOOL_T  bVideoRangeExist;           /* 0: not exist; 1: exist */
	VAL_UINT32_T u4VideoRange;                 /* 0: narrow; 1: full  */
	VAL_BOOL_T  bColourPrimariesExist;   /* 0: not exist; 1: exist */
	VDEC_DRV_COLOR_PRIMARIES_E eColourPrimaries;         /* VDEC_DRV_COLOR_PRIMARIES_E */
} VDEC_DRV_COLOR_PRIMARIES_INFO_T;

/**
 * @par Structure
 *  VDEC_DRV_RINGBUF_T
 * @par Description
 *  Ring Buffer Structure
 *  - Store buffer base address
 *  - Store read/write pointer address
 */
typedef struct __VDEC_DRV_RINGBUF_T { /* union extend 64bits for TEE*/
	VAL_MEM_ADDR_T  rBase;         /* /< [IN]     Base address of ring buffer */
	union {
		VAL_ULONG_T u4Read;        /* /< [IN/OUT] Virtual address of read pointer */
		VAL_UINT64_T u4Read_ext64;
	};
	union {
		VAL_ULONG_T u4Write;       /* /< [IN]     Virtual address of write pointer */
		VAL_UINT64_T u4Write_ext64;
	};
	VAL_UINT32_T    u4Timestamp;   /* /< [IN/OUT] store timestamp */
	VAL_UINT32_T    rSecMemHandle; /* /< [IN/OUT] security memory handle    // MTK_SEC_VIDEO_PATH_SUPPORT */
	VAL_UINT32_T    u4InputFlag;   /*/ < [IN]     the property of input buffer */
} VDEC_DRV_RINGBUF_T;

/**
 * @par Structure
 *  P_VDEC_DRV_RINGBUF_T
 * @par Description
 *  Pointer of VDEC_DRV_RINGBUF_T
 */
typedef VDEC_DRV_RINGBUF_T *P_VDEC_DRV_RINGBUF_T;


/**
 * @par Structure
 *  VDEC_DRV_FRAMEBUF_T
 * @par Description
 *  Frame buffer information
 */
typedef struct __VDEC_DRV_FRAMEBUF_T {
	VAL_MEM_ADDR_T  rBaseAddr;                  /* /< [IN/OUT] Base address */
	VAL_MEM_ADDR_T  rPostProcAddr;              /* /< [IN/OUT] Post process address */
	VAL_UINT32_T    u4BufWidth;                 /* /< [IN/OUT] Buffer width */
	VAL_UINT32_T    u4BufHeight;                /* /< [IN/OUT] Buffer height */
	VAL_UINT32_T    u4DispWidth;                /* /< [OUT]    Display width */
	VAL_UINT32_T    u4DispHeight;               /* /< [OUT]    Display width */
	VAL_UINT32_T    u4DispPitch;                /* /< [OUT]    Display pitch */
	VAL_UINT32_T    u4Timestamp;                /* /< [IN/OUT] Timestamp for last decode picture */
	VAL_UINT32_T    u4AspectRatioW;             /* /< [OUT]    The horizontal size of the sample aspect ratio. */
	VAL_UINT32_T    u4AspectRatioH;             /* /< [OUT]    The vertical size of the sample aspect ratio. */
	VAL_UINT32_T    u4FrameBufferType;          /* /< [OUT]    One of VDEC_DRV_FBTYPE */
	VAL_UINT32_T    u4PictureStructure;         /* /< [OUT]    One of VDEC_DRV_PIC_STRUCT */
	VAL_UINT32_T    u4FrameBufferStatus;        /* /< [OUT]    One of VDEC_DRV_FBSTSTUS */
	VAL_UINT32_T    u4IsUFOEncoded;             /* /< [OUT]    FB Is UFO Encoded */
	VAL_UINT32_T    u4Reserved1;                /* /< [IN/OUT] Reserved */
	VAL_UINT32_T    u4Reserved2;                /* /< [IN/OUT] Reserved */
	VAL_UINT32_T    u4Reserved3;                /* /< [IN/OUT] Reserved */

	/* /< [IN/OUT] security memory handle    // MTK_SEC_VIDEO_PATH_SUPPORT */
	VAL_UINT32_T    rSecMemHandle;
	VAL_UINT32_T    u4ReeVA;                    /* /< [IN/OUT] Ree Va    // MTK_SEC_VIDEO_PATH_SUPPORT */

	/* /< [IN/OUT] share handle of rBaseAddr.u4VA (for UT only)  // MTK_SEC_VIDEO_PATH_SUPPORT */
	VAL_UINT32_T    rFrameBufVaShareHandle;
	VDEC_DRV_COLOR_PRIMARIES_INFO_T rColorPriInfo;
} VDEC_DRV_FRAMEBUF_T;

/**
 * @par Structure
 *  P_VDEC_DRV_FRAMEBUF_T
 * @par Description
 *  Pointer of VDEC_DRV_FRAMEBUF_T
 */
typedef VDEC_DRV_FRAMEBUF_T *P_VDEC_DRV_FRAMEBUF_T;


/**
 * @par Structure
 *  VDEC_DRV_CROPINFO_T
 * @par Description
 *  Frame cropping information
 */
typedef struct __VDEC_DRV_CROPINFO_T {
	VAL_UINT32_T u4CropLeft;        /* /< Frame cropping left index */
	VAL_UINT32_T u4CropRight;      /* /< Frame cropping right index */
	VAL_UINT32_T u4CropTop;           /* /< Frame cropping top index */
	VAL_UINT32_T u4CropBottom;      /* /< Frame cropping bottom index */
} VDEC_DRV_CROPINFO_T;


/**
 * @par Structure
 *  P_VDEC_DRV_CROPINFO_T
 * @par Description
 *  Pointer of VDEC_DRV_CROPINFO_T
 */
typedef VDEC_DRV_CROPINFO_T * P_VDEC_DRV_CROPINFO_T;

/**
 * @par Structure
 *  VDEC_DRV_PICINFO_T
 * @par Description
 *  Picture information
 */
typedef struct __VDEC_DRV_PICINFO_T {
	VAL_UINT32_T    u4Width;                    /* /< [OUT] Frame width */
	VAL_UINT32_T    u4Height;                   /* /< [OUT] Frame height */
	VAL_UINT32_T    u4RealWidth;                /* /< [OUT] Frame real width (allocate buffer size) */
	VAL_UINT32_T    u4RealHeight;               /* /< [OUT] Frame real height (allocate buffer size) */
	VAL_UINT32_T    u4Timestamp;                /* /< [OUT] Timestamp for last decode picture */
	VAL_UINT32_T    u4AspectRatioW;             /* /< [OUT] The horizontal size of the sample aspect ratio */
	VAL_UINT32_T    u4AspectRatioH;             /* /< [OUT] The vertical size of the sample aspect ratio */
	VAL_UINT32_T    u4FrameRate;                /* /< [OUT] One of VDEC_DRV_FRAME_RATE */
	VAL_UINT32_T    u4PictureStructure;         /* /< [OUT] One of VDEC_DRV_PIC_STRUCT */
	VAL_UINT32_T    u4IsProgressiveOnly;        /* /< [OUT] 1: Progressive only. 0: Not progressive only. */
	VAL_INT32_T     u4BitDepthLuma;             /* /< [OUT] Sequence luma bitdepth */
	VAL_INT32_T     u4BitDepthChroma;           /* /< [OUT] Sequence chroma bitdepth */
	VAL_BOOL_T      bIsHorizontalScaninLSB;     /* /< [OUT] Scan direction in 10bit LSB 2 bit */
} VDEC_DRV_PICINFO_T;

/**
 * @par Structure
 *  P_VDEC_DRV_PICINFO_T
 * @par Description
 *  Pointer of VDEC_DRV_PICINFO_T
 */
typedef VDEC_DRV_PICINFO_T * P_VDEC_DRV_PICINFO_T;


/**
 * @par Structure
 *  VDEC_DRV_SEQINFO_T
 * @par Description
 *  Sequence information.
 *  - Including Width/Height
 */
typedef struct __VDEC_DRV_SEQINFO_T {
	VAL_UINT32_T    u4Width;                    /* /< [OUT] Sequence buffer width */
	VAL_UINT32_T    u4Height;                   /* /< [OUT] Sequence buffer height */
	VAL_UINT32_T    u4PicWidth;                 /* /< [OUT] Sequence display width */
	VAL_UINT32_T    u4PicHeight;                /* /< [OUT] Sequence display height */
	VAL_INT32_T     i4AspectRatioWidth;         /* /< [OUT] Sequence aspect ratio width */
	VAL_INT32_T     i4AspectRatioHeight;        /* /< [OUT] Sequence aspect ratio height */
	VAL_BOOL_T      bIsThumbnail;               /* /< [OUT] check thumbnail */
	VAL_INT32_T     u4BitDepthLuma;             /* /< [OUT] Sequence luma bitdepth */
	VAL_INT32_T     u4BitDepthChroma;           /* /< [OUT] Sequence chroma bitdepth */
	VAL_BOOL_T      bIsHorizontalScaninLSB;     /* /< [OUT] Scan direction in 10bit LSB 2 bit */
} VDEC_DRV_SEQINFO_T;

/**
 * @par Structure
 *  P_VDEC_DRV_SEQINFO_T
 * @par Description
 *  Pointer of VDEC_DRV_SEQINFO_T
 */
typedef VDEC_DRV_SEQINFO_T * P_VDEC_DRV_SEQINFO_T;


/**
 * @par Structure
 *  VDEC_DRV_YUV_STRIDE_T
 * @par Description
 *  Y/UV Stride information
 */
typedef struct __VDEC_DRV_YUV_STRIDE_T {
	unsigned int    u4YStride;                  /* /< [IN] Y Stride */
	unsigned int    u4UVStride;                 /* /< [IN] UV Stride */
} VDEC_DRV_YUV_STRIDE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_YUV_STRIDE_T
 * @par Description
 *  Pointer of VDEC_DRV_YUV_STRIDE_T
 */
typedef VDEC_DRV_YUV_STRIDE_T * P_VDEC_DRV_YUV_STRIDE_T;

#define VDEC_DRV_CONCURRENCE_LIMIT_WFD       0x00000001
#define VDEC_DRV_CONCURRENCE_LIMIT_MHL       0x00000002
#define VDEC_DRV_CONCURRENCE_LIMIT_BLUETOOTH 0x00000004
#define VDEC_DRV_CONCURRENCE_LIMIT_MASK      0x00000007
/**
 * @par Structure
 *  VDEC_DRV_QUERY_VIDEO_FORMAT_T
 * @par Description
 *  Both input and output of type QUERY_VIDEO_FORMAT
 */
typedef struct __VDEC_DRV_QUERY_VIDEO_FORMAT_T {
	VAL_UINT32_T            u4VideoFormat;          /* /< [OUT] VDEC_DRV_VIDEO_FORMAT */
	VAL_UINT32_T            u4Profile;              /* /< [OUT] VDEC_DRV_VIDEO_PROFILE */
	VAL_UINT32_T            u4Level;                /* /< [OUT] VDEC_DRV_VIDEO_LEVEL */
	VAL_UINT32_T            u4Resolution;           /* /< [OUT] VDEC_DRV_RESOLUTION */
	VAL_UINT32_T            u4Width;                /* /< [OUT] Frame Width */
	VAL_UINT32_T            u4Height;               /* /< [OUT] Frame Height */
	VAL_UINT32_T            u4StrideAlign;          /* /< [OUT] Frame Stride Alignment */
	VAL_UINT32_T            u4SliceHeightAlign;     /* /< [OUT] Frame Slice Height Alignment */
	VDEC_DRV_PIXEL_FORMAT_T ePixelFormat;           /* /< [OUT] Frame Format */
	VDEC_DRV_DECODER_TYPE_T eDecodeType;            /* /< [OUT] Decoder type */
	VAL_UINT32_T            u4CompatibleFlag;       /* /< [OUT] CompatibleFlag */
} VDEC_DRV_QUERY_VIDEO_FORMAT_T;

/**
 * @par Structure
 *  P_VDEC_DRV_QUERY_VIDEO_FORMAT_T
 * @par Description
 *  Pointer of VDEC_DRV_QUERY_VIDEO_FORMAT_T
 */
typedef VDEC_DRV_QUERY_VIDEO_FORMAT_T * P_VDEC_DRV_QUERY_VIDEO_FORMAT_T;


/**
 * @par Structure
 *  VDEC_DRV_SET_DECODE_MODE_T
 * @par Description
 *  [Unused]Set Decode Mode
 */
typedef struct __VDEC_DRV_SET_DECODE_MODE_T {
	VDEC_DRV_DECODE_MODE_T  eDecodeMode;            /* /< [IN/OUT] one of VDEC_DRV_DECODE_MODE */
	VAL_UINT32_T            u4DisplayFrameNum;      /* /< [IN/OUT] 0  8  7  6  5  4  3  2  1  1  1  1  1  1  1  1 */
	VAL_UINT32_T            u4DropFrameNum;         /* /< [IN/OUT] 0  1  1  1  1  1  1  1  1  2  3  4  5  6  7  8 */
} VDEC_DRV_SET_DECODE_MODE_T;

/**
 * @par Structure
 *  P_VDEC_DRV_SET_DECODE_MODE_T
 * @par Description
 *  Pointer of VDEC_DRV_SET_DECODE_MODE_T
 */
typedef VDEC_DRV_SET_DECODE_MODE_T *P_VDEC_DRV_SET_DECODE_MODE_T;


/**
 * @par Structure
 *  VDEC_DRV_PROPERTY_T
 * @par Description
 *  VDecDrv property information
 */
typedef struct __VDEC_DRV_PROPERTY_T {
	/* /< [OUT] buffer alignment requirement. */
	VAL_UINT32_T        u4BufAlign;

	/* /< [OUT] buffer unit size is N bytes . (e.g., 8, 16, or 64 bytes per unit.) */
	VAL_UINT32_T        u4BufUnitSize;

	/* /< [OUT] support post-process. */
	VAL_BOOL_T          fgPostprocessSupport;

	/* /< [IN/OUT] Post process property */
	struct {
		VAL_UINT32_T    fgOverlay:1;
		VAL_UINT32_T    fgRotate:1;
		VAL_UINT32_T    fgResize:1;
		VAL_UINT32_T    fgCrop:1;
	} PostProcCapability;
} VDEC_DRV_PROPERTY_T;

/**
 * @par Structure
 *  P_VDEC_DRV_PROPERTY_T
 * @par Description
 *  Pointer of VDEC_DRV_PROPERTY_T
 */
typedef VDEC_DRV_PROPERTY_T * P_VDEC_DRV_PROPERTY_T;


/**
 * @par Function:
 *   eVDecDrvQueryCapability
 * @par Description:
 *    - Query Decode Driver Capability
 *    - Input argument will be compare with driver's capability to check if the query is successful or not.
 * @param
 *   a_eType            [IN]  Driver query type, such as FBType, Video Format, etc.
 * @param
 *   a_pvInParam        [IN]  Input parameter for each type of query.
 * @param
 *   a_pvOutParam       [OUT] Store query result, such as FBType, Video Format, etc.
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Query Success
 *    - VDEC_DRV_MRESULT_FAIL: Query Fail
 */
VDEC_DRV_MRESULT_T  eVDecDrvQueryCapability(
	VDEC_DRV_QUERY_TYPE_T a_eType,
	VAL_VOID_T *a_pvInParam,
	VAL_VOID_T *a_pvOutParam
);


/**
 * @par Function:
 *   eVDecDrvCreate
 * @par Description:
 *    - Create handle
 *    - Allocate extra data for each driver
 *      - According to the input parameter, "a_eVideoFormat."
 * @param
 *   a_phHandle         [IN/OUT] Driver handle
 * @param
 *   a_eVideoFormat     [IN]     Video format, such as MPEG4, H264, etc.
 * @par Returns:
 *   Reason for return value. Show the default returned value at which condition.
 *    - VDEC_DRV_MRESULT_OK:   Create handle successfully
 *    - VDEC_DRV_MRESULT_FAIL: Failed to create handle
 */
VDEC_DRV_MRESULT_T  eVDecDrvCreate(VAL_HANDLE_T *a_phHandle, VDEC_DRV_VIDEO_FORMAT_T a_eVideoFormat);


/**
 * @par Function:
 *   eVDecDrvRelease
 * @par Description:
 *    - Release Decode Driver
 *      - Need to perform driver deinit before driver release.
 *    - Procedure of release
 *      - Release extra data
 *      - Release handle
 * @param
 *   a_hHandle          [IN] Handle needed to be released.
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Release handle successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to release handle.
 */
VDEC_DRV_MRESULT_T  eVDecDrvRelease(VAL_HANDLE_T a_hHandle);


/**
 * @par Function:
 *   eVDecDrvInit
 * @par Description:
 *    - Initialize Decode Driver
 *    - Get width and height of bitstream
 * @param
 *   a_hHandle          [IN]  Driver handle
 * @param
 *   a_prBitstream      [IN]  Input bitstream for driver initialization
 * @param
 *   a_prSeqinfo        [OUT] Return width and height of bitstream
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Init driver successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to init driver.
 */
VDEC_DRV_MRESULT_T  eVDecDrvInit(
	VAL_HANDLE_T a_hHandle,
	VDEC_DRV_RINGBUF_T *a_prBitstream,
	VDEC_DRV_SEQINFO_T * a_prSeqinfo
);


/**
 * @par Function:
 *   eVDecDrvDeInit
 * @par Description:
 *    - Deinitialize driver
 *      - Have to deinit driver before release driver
 * @param
 *   a_hHandle          [IN] Driver handle
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Deinit driver successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to deinit driver.
 */
VDEC_DRV_MRESULT_T  eVDecDrvDeInit(VAL_HANDLE_T a_hHandle);


/**
 * @par Function:
 *   eVDecDrvGetParam
 * @par Description:
 *    - Get driver's parameter
 *      - Type of parameter can be referred to VDEC_DRV_GET_TYPE_T.
 * @param
 *   a_hHandle          [IN]  Driver handle
 * @param
 *   a_eType            [IN]  Parameter type
 * @param
 *   a_pvInParam        [OUT] Input argument for query parameter.
 * @param
 *   a_pvOutParam       [OUT] Store output parameter
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Get parameter successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to get parameter.
 *      - Fail reason might be
 *        - wrong or unsupported parameter type
 *        - fail to get reference memory pool size.
 */
VDEC_DRV_MRESULT_T  eVDecDrvGetParam(
	VAL_HANDLE_T a_hHandle,
	VDEC_DRV_GET_TYPE_T a_eType,
	VAL_VOID_T *a_pvInParam,
	VAL_VOID_T *a_pvOutParam
);


/**
 * @par Function:
 *   eVDecDrvSetParam
 * @par Description:
 *    - Set driver's parameters
 * @param
 *   a_hHandle          [IN]  driver handle
 * @param
 *   a_eType            [IN]  parameter type
 * @param
 * a_pvInParam          [IN]  input parameter
 * @param
 * a_pvOutParam         [OUT] output parameter
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Get parameter successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to get parameter.
 *      - Fail reason might be
 *        - wrong or unsupported parameter type
 *        - fail to set parameter
 */
VDEC_DRV_MRESULT_T  eVDecDrvSetParam(
	VAL_HANDLE_T a_hHandle,
	VDEC_DRV_SET_TYPE_T a_eType,
	VAL_VOID_T *a_pvInParam,
	VAL_VOID_T *a_pvOutParam
);


/**
 * @par Function:
 *  eVDecDrvDecode
 * @par Description:
 *    - Trigger Decode
 *      - Need to Provide frame buffer to store unused buffer
 *    - The procedure of decode including:
 *      - Header parsing
 *      - trigger hw decode
 *    - While we want to decode the last frame,
 *       we need to set input bitstream as VAL_NULL and still give free frame buffer.
 * @param
 *   a_hHandle          [IN] driver handle
 * @param
 *   a_prBitstream      [IN] input bitstream
 * @param
 *   a_prFramebuf       [IN] free frame buffer
 * @par Returns:
 *    - VDEC_DRV_MRESULT_OK:   Decode successfully.
 *    - VDEC_DRV_MRESULT_FAIL: Failed to decode.
 */
VDEC_DRV_MRESULT_T  eVDecDrvDecode(
	VAL_HANDLE_T a_hHandle,
	VDEC_DRV_RINGBUF_T *a_prBitstream,
	VDEC_DRV_FRAMEBUF_T *a_prFramebuf
);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VDEC_DRV_IF_PUBLIC_H_ */

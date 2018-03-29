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

#ifndef _KD_CAMERA_FEATURE_ID_H_
#define _KD_CAMERA_FEATURE_ID_H_



/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* Definition of Feature ID. */
/* ps: FID = 0 is reserved. */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
typedef enum {
    FID_PRE_BEGIN = 0,
    /* //////////////////////////////////////////////////////////////////////////// */
    /* Scene-Independent (SI) feature id. */
    FID_PRE_BEGIN_SI = FID_PRE_BEGIN,
    /* ---------------------------------------------------------------------------- */
    /* Misc. feature id. */
    FID_PRE_BEGIN_MISC_SI = FID_PRE_BEGIN_SI,
        FID_ZOOM_FACTOR,  /* Legacy symbol (Remove if possible) */
        FID_AE_STROBE,
        FID_EIS,
        FID_ZSD,
        FID_AWB2PASS,
        FID_AF_LAMP,
        FID_FAST_CONTINUOUS_SHOT,
        FID_OVER_LAST_MISC_SI,
        FID_PRE_BEGIN_RAW_ONLY_SI = FID_OVER_LAST_MISC_SI - 1,
        FID_OVER_LAST_RAW_ONLY_SI,
        FID_PRE_BEGIN_RAW_YUV_SI = FID_OVER_LAST_RAW_ONLY_SI - 1,
        FID_SCENE_MODE,
        FID_COLOR_EFFECT,
        FID_CAPTURE_MODE,
        FID_CAP_SIZE,
        FID_PREVIEW_SIZE,
        FID_VIDEO_PREVIEW_SIZE,
        FID_FRAME_RATE,
        FID_FRAME_RATE_RANGE,
        FID_AE_FLICKER,
        FID_FOCUS_DIST_NORMAL,
        FID_FOCUS_DIST_MACRO,
        FID_STEREO_3D_CAP_SIZE,
        FID_STEREO_3D_PREVIEW_SIZE,
        FID_STEREO_3D_TYPE,
        FID_STEREO_3D_MODE,
        FID_STEREO_3D_IMAGE_FORMAT,
        FID_OVER_LAST_RAW_YUV_SI,
    /* YUV-only feature id. */
    FID_PRE_BEGIN_YUV_ONLY_SI = FID_OVER_LAST_RAW_YUV_SI - 1,
    FID_OVER_LAST_YUV_ONLY_SI,
    
    /* ---------------------------------------------------------------------------- */
    FID_OVER_LAST_SI = FID_OVER_LAST_YUV_ONLY_SI,
    /* //////////////////////////////////////////////////////////////////////////// */
    /* Scene-Dependent (SD) feature id. */
    FID_PRE_BEGIN_SD = FID_OVER_LAST_SI - 1,
    /* ---------------------------------------------------------------------------- */
    /* Misc. feature id. */
    FID_PRE_BEGIN_MISC_SD = FID_PRE_BEGIN_SD,
    FID_FD_ON_OFF,
    FID_OVER_LAST_MISC_SD,
    /* RAW-only feature id. */
    FID_PRE_BEGIN_RAW_ONLY_SD = FID_OVER_LAST_MISC_SD - 1,
    FID_OVER_LAST_RAW_ONLY_SD,
    /* RAW-YUV-shared feature id. */
    FID_PRE_BEGIN_RAW_YUV_SD = FID_OVER_LAST_RAW_ONLY_SD - 1,
        FID_AE_SCENE_MODE,
        FID_AE_METERING,
        FID_AE_ISO,
        FID_AE_EV,
        FID_AF_MODE,
        FID_AF_METERING,
        FID_AWB_MODE,
        FID_ISP_EDGE,
        FID_ISP_HUE,
        FID_ISP_SAT,
        FID_ISP_BRIGHT,
        FID_ISP_CONTRAST,
        FID_OVER_LAST_RAW_YUV_SD,
    /* YUV-only feature id. */
        FID_PRE_BEGIN_YUV_ONLY_SD = FID_OVER_LAST_RAW_YUV_SD - 1,
        FID_YUV_AUTOTEST,
        FID_OVER_LAST_YUV_ONLY_SD,
    
    /* ---------------------------------------------------------------------------- */
    FID_OVER_LAST_SD = FID_OVER_LAST_YUV_ONLY_SD,
    /* //////////////////////////////////////////////////////////////////////////// */
    FID_OVER_LAST = FID_OVER_LAST_SD
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

} FEATURE_ID;


#endif              /* _KD_CAMERA_FEATURE_ID_H_ */

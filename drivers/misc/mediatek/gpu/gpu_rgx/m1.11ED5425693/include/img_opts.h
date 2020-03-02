/*************************************************************************/ /*!
@File
@Title          Driver optimisations
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Branch-specific optimisation defines.
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(IMG_OPTS_H)
#define IMG_OPTS_H

#define IMG_1_11_OPT_AUTO_PLS 0x1
#define IMG_1_11_OPT_SCREEN_SPACE_TEX_COORD_REPLACEMENT 0x2
#define IMG_1_11_OPT_REDUCE_SHARED_REG_PRESSURE 0x4
#define IMG_1_11_OPT_VS_OUTPUT_ELIMINATION 0x8
#define IMG_1_11_OPT_TDM_SETUP 0x10
#define IMG_1_11_OPT_DISABLE_FW_LOGGING_BY_DEFAULT 0x1
#define IMG_1_12_OPT_CACHE_TDM_SHADERS 0x1
#define IMG_1_12_OPT_CACHE_TPC 0x2
#define IMG_1_12_OPT_TRILINEAR_TO_BILINEAR 0x4
#define IMG_1_12_OPT_DROP_RENDUNDANT_DRAW_CALL 0x8
#define IMG_1_12_OPT_FBC_D24S8 0x10

#define IMG_OPT_ENABLED(rel, flag) ((IMG_##rel##_OPTS) & IMG_##rel##_OPT_##flag)
#endif /* #if !defined(IMG_OPTS_H) */
/*****************************************************************************
 End of file (img_opts.h)
*****************************************************************************/

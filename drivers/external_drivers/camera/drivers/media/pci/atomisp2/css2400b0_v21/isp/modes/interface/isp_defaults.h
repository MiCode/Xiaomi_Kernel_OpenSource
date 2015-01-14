/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2013 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

#ifndef _ISP_DEFAULTS
#define _ISP_DEFAULTS

#if !defined(USE_DMA_PROXY)
#define USE_DMA_PROXY 1
#endif

#if !defined(ENABLE_DIS_CROP)
#define ENABLE_DIS_CROP 0
#endif

/* Duplicates from "isp/common/defs.h" */
#if !defined(ENABLE_FIXED_BAYER_DS)
#define ENABLE_FIXED_BAYER_DS 0
#endif

#if !defined(SUPPORTED_BDS_FACTORS)
#define SUPPORTED_BDS_FACTORS PACK_BDS_FACTOR(SH_CSS_BDS_FACTOR_1_00)
#endif

#if !defined(ENABLE_RAW_BINNING)
#define ENABLE_RAW_BINNING 0
#endif

#if !defined(ENABLE_AA_BEFORE_RAW_BINNING)
#define ENABLE_AA_BEFORE_RAW_BINNING 0
#endif

#if !defined(ENABLE_YUV_AA)
#define ENABLE_YUV_AA           0
#endif

#if !defined(ENABLE_VF_BINNING)
#define ENABLE_VF_BINNING       0
#endif

#if !defined(ENABLE_PARAMS)
#define ENABLE_PARAMS		1
#endif

#if !defined(ENABLE_RAW)
#define ENABLE_RAW		0
#endif

#if !defined(ENABLE_INPUT_YUV)
#define ENABLE_INPUT_YUV	0
#endif

#if !defined(ENABLE_DVS_6AXIS)
#define ENABLE_DVS_6AXIS	0
#endif

#if !defined(ENABLE_TNR)
#define ENABLE_TNR              0
#endif

#if !defined(ENABLE_UDS)
#define ENABLE_UDS              0
#endif

#if !defined(ENABLE_OUTPUT)
#define ENABLE_OUTPUT           0
#endif

#if !defined(NUM_OUTPUT_PINS)
#define NUM_OUTPUT_PINS         0
#endif

#if !defined(ENABLE_FLIP)
#define ENABLE_FLIP           0
#endif

#if !defined(ENABLE_BAYER_OUTPUT)
#define ENABLE_BAYER_OUTPUT     0
#endif

#if !defined(ENABLE_MACC)
#define ENABLE_MACC           0
#endif

#if !defined(ENABLE_SS)
#define ENABLE_SS             0
#endif

#if !defined(ENABLE_OB)
#define ENABLE_OB		1
#endif

#if !defined(ENABLE_LIN)
#define ENABLE_LIN            0
#endif

#if !defined(ENABLE_DPC)
#define ENABLE_DPC		1
#endif

#if !defined(ENABLE_DPCBNR)
#define ENABLE_DPCBNR         1
#endif

#if !defined(ENABLE_RGBA)
#define ENABLE_RGBA 0
#endif

#if !defined(ENABLE_YEE)
#define ENABLE_YEE            0
#endif

#if !defined(ENABLE_PIXELNOISE_IN_DE_MOIRE_CORING)
#define ENABLE_PIXELNOISE_IN_DE_MOIRE_CORING 0
#endif

#if !defined(ENABLE_DE_C1C2_CORING)
#define ENABLE_DE_C1C2_CORING 0
#endif

#if !defined(ENABLE_YNR)
#define ENABLE_YNR            0
#endif

#if !defined(ENABLE_CNR)
#define ENABLE_CNR            1
#endif

#if !defined(ENABLE_XNR)
#define ENABLE_XNR            0
#endif

#if !defined(ENABLE_GAMMA)
#define ENABLE_GAMMA          0
#endif

#if !defined(ENABLE_CTC)
#define ENABLE_CTC            0
#endif

#if !defined(ENABLE_CLIPPING_IN_YEE)
#define ENABLE_CLIPPING_IN_YEE 0
#endif

#if !defined(ENABLE_GAMMA_UPGRADE)
#define ENABLE_GAMMA_UPGRADE  0
#endif

#if !defined(ENABLE_BAYER_HIST)
#if ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_2_2
#define ENABLE_BAYER_HIST  0 /* should be 1 */
#elif ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_1
#define ENABLE_BAYER_HIST  0
#endif
#endif

#if !defined(ENABLE_QPLANE_STORE)
#define ENABLE_QPLANE_STORE     0
#endif

#if !defined(ENABLE_QPLANE_LOAD)
#define ENABLE_QPLANE_LOAD      0
#endif

#if !defined(ENABLE_HIGH_SPEED)
#define ENABLE_HIGH_SPEED 0
#endif

#ifndef ENABLE_REF_FRAME
#define ENABLE_REF_FRAME        (ENABLE_TNR || ENABLE_DIS_CROP || \
				ENABLE_UDS || ENABLE_DVS_6AXIS)
#endif

/* sdis2 supports 64/32/16BQ grid */
#if !defined(ENABLE_SDIS2_64BQ_32BQ_16BQ_GRID)
#define ENABLE_SDIS2_64BQ_32BQ_16BQ_GRID  0
#endif

/* sdis2 supports 64/32BQ grid */
#if !defined(ENABLE_SDIS2_64BQ_32BQ_GRID)
#define ENABLE_SDIS2_64BQ_32BQ_GRID       0
#endif

#ifndef ENABLE_CONTINUOUS
#define ENABLE_CONTINUOUS       0
#endif

#ifndef ENABLE_CA_GDC
#define ENABLE_CA_GDC           0
#endif

#ifndef ENABLE_OVERLAY
#define ENABLE_OVERLAY          0
#endif

#ifndef ENABLE_ISP_ADDRESSES
#define ENABLE_ISP_ADDRESSES    1
#endif

#ifndef ENABLE_IN_FRAME
#define ENABLE_IN_FRAME		0
#endif

#ifndef ENABLE_OUT_FRAME
#define ENABLE_OUT_FRAME	0
#endif

#ifndef ISP_DMEM_PARAMETERS_SIZE
#define ISP_DMEM_PARAMETERS_SIZE	0
#endif

#ifndef ISP_DMEM_RESULTS_SIZE
#define ISP_DMEM_RESULTS_SIZE		0
#endif

#ifndef ISP_VMEM_PARAMETERS_SIZE
#define ISP_VMEM_PARAMETERS_SIZE	0
#endif

#ifndef ISP_VMEM_RESULTS_SIZE
#define ISP_VMEM_RESULTS_SIZE		0
#endif

#ifndef ISP_VAMEM0_PARAMETERS_SIZE
#define ISP_VAMEM0_PARAMETERS_SIZE	0
#endif

#ifndef ISP_VAMEM0_RESULTS_SIZE
#define ISP_VAMEM0_RESULTS_SIZE		0
#endif

#ifndef ISP_VAMEM1_PARAMETERS_SIZE
#define ISP_VAMEM1_PARAMETERS_SIZE	0
#endif

#ifndef ISP_VAMEM1_RESULTS_SIZE
#define ISP_VAMEM1_RESULTS_SIZE		0
#endif

#ifndef ISP_VAMEM2_PARAMETERS_SIZE
#define ISP_VAMEM2_PARAMETERS_SIZE	0
#endif

#ifndef ISP_VAMEM2_RESULTS_SIZE
#define ISP_VAMEM2_RESULTS_SIZE		0
#endif

#ifndef ISP_VAMEM3_PARAMETERS_SIZE
#define ISP_VAMEM3_PARAMETERS_SIZE	0
#endif

#ifndef ISP_HMEM0_PARAMETERS_SIZE
#define ISP_HMEM0_PARAMETERS_SIZE	0
#endif

#if !defined(USE_BNR_LITE)
#if ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_2_2
#define USE_BNR_LITE  1 /* should be 0 */
#elif ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_1
#define USE_BNR_LITE  1
#endif
#endif

#if !defined(USE_YEEYNR_LITE)
#if ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_2_2
#define USE_YEEYNR_LITE  0
#elif ISP_PIPE_VERSION == SH_CSS_ISP_PIPE_VERSION_1
#define USE_YEEYNR_LITE  1
#endif
#endif

#if !defined(SPLIT_DP)
#define SPLIT_DP 0
#endif

#ifndef ISP_NUM_STRIPES
#define ISP_NUM_STRIPES         1
#endif

#ifndef ISP_ROW_STRIPES_HEIGHT
#define ISP_ROW_STRIPES_HEIGHT         0
#endif

#ifndef ISP_ROW_STRIPES_OVERLAP_LINES
#define ISP_ROW_STRIPES_OVERLAP_LINES         0
#endif

#ifndef VARIABLE_OUTPUT_FORMAT
#define VARIABLE_OUTPUT_FORMAT  1
#endif

#ifndef STREAM_OUTPUT
#define STREAM_OUTPUT  0
#endif

#ifndef VARIABLE_RESOLUTION
#define VARIABLE_RESOLUTION  1
#endif

#ifndef ISP_MAX_INTERNAL_WIDTH
#define ISP_MAX_INTERNAL_WIDTH  0
#endif

#ifndef ENABLE_INPUT_CHUNKING
#define ENABLE_INPUT_CHUNKING  0
#endif

#endif /* _ISP_DEFAULTS */


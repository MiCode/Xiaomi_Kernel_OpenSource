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

#ifndef _isp_globals_hive_h_
#define _isp_globals_hive_h_

#ifndef SYNC_WITH
#define SYNC_WITH(x)
#endif
#ifndef MEM
#define MEM(x)
#endif
#ifndef NO_SYNC
#define NO_SYNC
#endif
#ifndef NO_HOIST
#define NO_HOIST
#endif

#include "isp_types.h"
#include <sh_css_internal.h>

#if !defined(__HOST)
#include "dma_proxy.common.h"
#endif
#include "input_buf.isp.h"
#include "uds/uds_1.0/ia_css_uds_param.h" /* sh_css_sp_uds_params */

#if defined(HAS_RES_MGR)
#include <components/resolutions_mgr/src/resolutions_mgr_private.h>
#endif

/* Initialized by the SP: binary dependent */
/* Some of these globals are used inside dma transfer/configure commands.
   Therefore, their load will get a sync attribute. NO_SYNC prevents that.
*/
typedef struct s_isp_globals {
  /* DMA settings for output image */
  unsigned dma_output_block_width_a;
  unsigned dma_output_block_width_b;
#if defined(HAS_RES_MGR)
  /* Work around to get the skycam resolution manager to work.
   * This belongs in isp_dmem_configurations.iterator.internal_info.
   */
  struct s_res_mgr_isp_globals res_mgr;
#endif

} s_isp_globals;

#ifdef __SP
#undef ISP_DMEM
#define ISP_DMEM MEM(SP_XMEM)
#define PVECTOR short MEM(SP_XMEM) *
#else
#undef ISP_DMEM
#define ISP_DMEM
#define PVECTOR tmemvectoru MEM(VMEM) *
#endif

typedef void *pipeline_param_h;

typedef struct s_isp_addresses {
  struct {
    struct sh_css_sp_uds_params     ISP_DMEM *uds_params;
    unsigned                        ISP_DMEM *isp_deci_log_factor;
    struct s_isp_frames             ISP_DMEM *isp_frames;
    struct s_isp_globals            ISP_DMEM *isp_globals;
    unsigned                        ISP_DMEM *isp_online;
    struct s_output_dma_info        ISP_DMEM *output_dma_info;
    unsigned                        ISP_DMEM *vertical_upsampled;
    struct sh_css_ddr_address_map   ISP_DMEM *xmem_base;
    struct ia_css_isp_3a_statistics ISP_DMEM *s3a_data;
    struct ia_css_isp_dvs_statistics ISP_DMEM *sdis_data;
    sh_dma_cmd*                     ISP_DMEM *sh_dma_cmd_ptr;
    unsigned                        ISP_DMEM *g_isp_do_zoom;
    struct isp_uds_config           ISP_DMEM *uds_config;
    int                             ISP_DMEM *g_sdis_horiproj_tbl;
    int                             ISP_DMEM *g_sdis_vertproj_tbl;
    unsigned                        ISP_DMEM *isp_enable_xnr;
    struct s_isp_gdcac_config       ISP_DMEM *isp_gdcac_config;
    unsigned                        ISP_DMEM *stripe_id;
    unsigned                        ISP_DMEM *stripe_row_id;
    unsigned                        ISP_DMEM *isp_continuous;
    unsigned                        ISP_DMEM *required_bds_factor;
    unsigned                        ISP_DMEM *isp_raw_stride_b;
    unsigned                        ISP_DMEM *isp_raw_block_width_b;
    unsigned                        ISP_DMEM *isp_raw_line_width_b;
    unsigned                        ISP_DMEM *isp_raw_stripe_offset_b;
    uint32_t                        ISP_DMEM *isp_output_stride_b;
    uint8_t                         ISP_DMEM *enable_output_hflip;
    uint8_t                         ISP_DMEM *enable_output_vflip;
  } dmem;
  struct {
    PVECTOR  input_buf;
    PVECTOR  g_macc_coef;
    PVECTOR  uds_data_via_sp;
    PVECTOR  uds_ipxs_via_sp;
    PVECTOR  uds_ibuf_via_sp;
    PVECTOR  uds_obuf_via_sp;
    PVECTOR  raw_fir_buf;
    PVECTOR  raw_fir1_buf;
    PVECTOR  raw_fir2_buf;
  } vmem;
  struct {
    unsigned uds_params;
    unsigned g_sdis_horiproj_tbl;
    unsigned g_sdis_vertproj_tbl;
    unsigned raw_fir_buf;
    unsigned raw_fir1_buf;
    unsigned raw_fir2_buf;
  } sizes;
} s_isp_addresses;

extern s_isp_globals   NO_SYNC NO_HOIST isp_globals;
extern s_isp_addresses NO_SYNC NO_HOIST isp_addresses;

#ifdef __ISP
#define isp_envelope_width         isp_dmem_configurations.iterator.dvs_envelope.width
#define isp_envelope_height        isp_dmem_configurations.iterator.dvs_envelope.height
#define isp_bits_per_pixel         isp_dmem_configurations.iterator.input_info.raw_bit_depth
#define isp_deinterleaved          isp_dmem_configurations.raw.deinterleaved
#define isp_vectors_per_line       iterator_config.vectors_per_line
#define isp_vectors_per_input_line iterator_config.vectors_per_input_line
#define isp_uv_internal_width_vecs iterator_config.uv_internal_width_vecs

#if ENABLE_RAW_INPUT
#define isp_2ppc                   isp_dmem_configurations.raw.two_ppc
#else
#define isp_2ppc                   false
#endif

/* Still used by SP */
#define isp_vf_output_width_vecs   vf_config.output_width_vecs

#define g_dma_output_block_width_a isp_globals.dma_output_block_width_a
#define g_dma_output_block_width_b isp_globals.dma_output_block_width_b

#if ENABLE_VF_VECEVEN
#define isp_vf_downscale_bits      isp_dmem_configurations.vf.vf_downscale_bits
#else
#define isp_vf_downscale_bits      0
#endif

#endif /* __ISP */

extern unsigned isp_deci_log_factor;
extern unsigned isp_online;

extern struct sh_css_ddr_address_map xmem_base;
extern struct ia_css_isp_3a_statistics s3a_data;
extern struct ia_css_isp_dvs_statistics sdis_data;
extern struct s_isp_frames isp_frames;
extern struct isp_uds_config uds_config;

/* *****************************************************************
 * 		uds parameters
 * *****************************************************************/
extern NO_HOIST struct sh_css_sp_uds_params uds_params[SH_CSS_MAX_STAGES];

/* DMA settings for viewfinder image */

#define isp_do_zoom (ENABLE_DVS_ENVELOPE ? 1 : g_isp_do_zoom)
extern unsigned g_isp_do_zoom;

#if MODE != IA_CSS_BINARY_MODE_COPY
#if !defined(IS_ISP_2500_SYSTEM)
extern input_line_type SYNC_WITH (INPUT_BUF) MEM (VMEM) input_buf;
#endif
#endif /* MODE != IA_CSS_BINARY_MODE_COPY */

/* striped-ISP information */
extern unsigned stripe_id;
extern unsigned stripe_row_id;

extern unsigned isp_continuous;
extern unsigned required_bds_factor;
extern unsigned isp_raw_stride_b;
extern unsigned isp_raw_block_width_b;
extern unsigned isp_raw_line_width_b;
extern unsigned isp_raw_stripe_offset_b;

#endif /* _isp_globals_hive_h_ */

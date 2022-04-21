// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "../mtk-cmdq-ext.h"
#endif

#include "../mtk_drm_ddp.h"
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_disp_ovl.h"
#include "../mtk_disp_wdma.h"
#include "../mtk_mipi_tx.h"
#include "mtk_drm_6789.h"

static void mt6789_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			struct cmdq_pkt *handle, void *data);

// overlay
static const struct compress_info compr_info_mt6789  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

const struct mtk_disp_ovl_data mt6789_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x04,
	.el_hdr_addr = 0xfd0,
	.el_hdr_addr_offset = 0x08,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6789,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 15,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 15,
	.greq_num_dl = 0x5555,
	.is_support_34bits = false,
	.source_bpc = 10,
};

// wdma
const struct mtk_disp_wdma_data mt6789_wdma_driver_data = {
	.fifo_size_1plane = 578,
	.fifo_size_uv_1plane = 29,
	.fifo_size_2plane = 402,
	.fifo_size_uv_2plane = 201,
	.fifo_size_3plane = 402,
	.fifo_size_uv_3plane = 99,
	.sodi_config = mt6789_mtk_sodi_config,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.is_support_34bits = false,
};

// rdma
const struct mtk_disp_rdma_data mt6789_rdma_driver_data = {
	.fifo_size = SZ_32K + SZ_8K + SZ_4K + SZ_2K + SZ_1K +
			SZ_512 + SZ_256 + SZ_64 + SZ_32,
	.pre_ultra_low_us = 250,
	.pre_ultra_high_us = 260,
	.ultra_low_us = 230,
	.ultra_high_us = 250,
	.urgent_low_us = 110,
	.urgent_high_us = 120,
	.sodi_config = mt6789_mtk_sodi_config,
	.shadow_update_reg = 0x00bc,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.has_greq_urg_num = false,
	.is_support_34bits = false,
	.dsi_buffer = false,
};

// dsc
const struct mtk_disp_dsc_data mt6789_dsc_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = false,
	.need_obuf_sw = true,
	.dsi_buffer = false,
};

// rsz
const struct mtk_disp_rsz_data mt6789_rsz_driver_data = {
	.tile_length = 1200, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

// postmask
const struct mtk_disp_postmask_data mt6789_postmask_driver_data = {
	.is_support_34bits = false,
};

// aal
const struct mtk_disp_aal_data mt6789_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

// ccorr
const struct mtk_disp_ccorr_data mt6789_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 1,
};

// color
#define DISP_COLOR_START_MT6789 0x0c00
const struct mtk_disp_color_data mt6789_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6789,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400B000, 0x1400C000,
			0x1400D000, 0x1400F000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

// dither
const struct mtk_disp_dither_data mt6789_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

// dsi
const struct mtk_dsi_driver_data mt6789_dsi_driver_data = {
	.reg_cmdq0_ofs = 0xd00,
	.reg_cmdq1_ofs = 0xd04,
	.reg_vm_cmd_con_ofs = 0x200,
	.reg_vm_cmd_data0_ofs = 0x208,
	.reg_vm_cmd_data10_ofs = 0x218,
	.reg_vm_cmd_data20_ofs = 0x228,
	.reg_vm_cmd_data30_ofs = 0x238,
	.poll_for_idle = mtk_dsi_poll_for_idle,
	.irq_handler = mtk_dsi_irq_status,
	.esd_eint_compat = "mediatek, DSI_TE-eint",
	.support_shadow = false,
	.need_bypass_shadow = true,
	.need_wait_fifo = true,
	.dsi_buffer = false,
	.dsi_new_trail = false,
	.max_vfp = 0x7ffe,
	.mmclk_by_datarate = mtk_dsi_set_mmclk_by_datarate_V2,
};

// ddp
#define MT6789_MMSYS_OVL_CON 0xF04
	#define MT6789_DISP_OVL0_GO_BG          BIT(1)
	#define MT6789_DISP_OVL0_GO_BLEND       BIT(0)
	#define MT6789_DISP_OVL0_2L_GO_BG       BIT(3)
	#define MT6789_DISP_OVL0_2L_GO_BLEND    BIT(2)

#define MT6789_DISP_REG_CONFIG_DL_VALID_0 0xe9c
#define MT6789_DISP_REG_CONFIG_DL_VALID_1 0xea0

#define MT6789_DISP_REG_CONFIG_DL_READY_0 0xea8
#define MT6789_DISP_REG_CONFIG_DL_READY_1 0xeac
#define MT6789_DISP_REG_CONFIG_SMI_LARB0_GREQ 0x8dc
#define MT6789_DISP_REG_CONFIG_SMI_LARB1_GREQ 0x8e0

#define DISP_REG_CONFIG_MMSYS_CG_CON0_MT6789 0x100
#define DISP_REG_CONFIG_MMSYS_CG_CON1_MT6789 0x110

#define MT6789_RDMA0_SOUT_COLOR0 0x1

static const unsigned int mt6789_mutex_mod[DDP_COMPONENT_ID_MAX] = {
		[DDP_COMPONENT_OVL0] = MT6789_MUTEX_MOD_DISP_OVL0,
		[DDP_COMPONENT_OVL0_2L] = MT6789_MUTEX_MOD_DISP_OVL0_2L,
		[DDP_COMPONENT_RDMA0] = MT6789_MUTEX_MOD_DISP_RDMA0,
		[DDP_COMPONENT_RSZ0] = MT6789_MUTEX_MOD_DISP_RSZ0,
		[DDP_COMPONENT_COLOR0] = MT6789_MUTEX_MOD_DISP_COLOR0,
		[DDP_COMPONENT_CCORR0] = MT6789_MUTEX_MOD_DISP_CCORR0,
		[DDP_COMPONENT_AAL0] = MT6789_MUTEX_MOD_DISP_AAL0,
		[DDP_COMPONENT_GAMMA0] = MT6789_MUTEX_MOD_DISP_GAMMA0,
		[DDP_COMPONENT_POSTMASK0] = MT6789_MUTEX_MOD_DISP_POSTMASK0,
		[DDP_COMPONENT_DITHER0] = MT6789_MUTEX_MOD_DISP_DITHER0,
		[DDP_COMPONENT_DSC0]
			= MT6789_MUTEX_MOD_DISP_DSC_WRAP0_CORE0,
		[DDP_COMPONENT_DSI0] = MT6789_MUTEX_MOD_DISP_DSI0,
		[DDP_COMPONENT_WDMA0] = MT6789_MUTEX_MOD_DISP_WDMA0,
		[DDP_COMPONENT_PWM0] = MT6789_MUTEX_MOD_DISP_PWM0,
};

static const unsigned int mt6789_mutex_sof[DDP_MUTEX_SOF_MAX] = {
		[DDP_MUTEX_SOF_SINGLE_MODE] = MT6789_MUTEX_SOF_SINGLE_MODE,
		[DDP_MUTEX_SOF_DSI0] =
			MT6789_MUTEX_SOF_DSI0 | MT6789_MUTEX_EOF_DSI0,
};

const struct mtk_disp_ddp_data mt6789_ddp_driver_data = {
	.mutex_mod = mt6789_mutex_mod,
	.mutex_sof = mt6789_mutex_sof,
	.mutex_mod_reg = MT6789_DISP_MUTEX0_MOD0,
	.mutex_sof_reg = MT6789_DISP_MUTEX0_SOF,
};

const struct mtk_mmsys_reg_data mt6789_mmsys_reg_data = {
	.ovl0_mout_en = MT6789_DISP_OVL0_MOUT_EN,
	.rdma0_sout_sel_in = MT6789_DISP_REG_CONFIG_DISP_RDMA0_RSZ0_SOUT_SEL,
	.rdma0_sout_color0 = MT6789_RDMA0_SOUT_COLOR0,
};

static char *ddp_signal_0_mt6789(int bit)
{
	switch (bit) {
	case 0:
		return
			"DISP_AAL0_TO_DISP_GAMMA0";
	case 1:
		return
			"DISP_CCORR0_TO_DISP_AAL0";
	case 2:
		return
			"DISP_COLOR0_TO_DISP_CCORR0";
	case 3:
		return
			"DISP_DITHER0_TO_DISP_DITHER0_MOUT";
	case 4:
		return
			"DISP_DITHER0_MOUT_OUT0_TO_DSI0_SEL_IN1";
	case 5:
		return
			"DISP_DITHER0_MOUT_OUT1_TO_DISP_WDMA0_SEL_IN0";
	case 6:
		return
			"DISP_DITHER0_MOUT_OUT2_TO_DISP_DSC_WRAP0";
	case 7:
		return
			"DISP_GAMMA0_TO_DISP_POSTMASK0";
	case 8:
		return
			"DISP_OVL0_2L_OUT0_TO_DISP_TOVL0_OUT0_MOUT";
	case 9:
		return
			"DISP_OVL0_2L_OUT1_TO_DISP_OVL0_IN0";
	case 10:
		return
			"DISP_OVL0_OUT0_TO_DISP_TOVL0_OUT1_MOUT";
	case 11:
		return
			"DISP_OVL0_OUT1_TO_DISP_OVL0_2L_IN0";
	case 12:
		return
			"DISP_POSTMASK0_TO_DISP_DITHER0";
	case 13:
		return
			"DISP_RDMA0_TO_DISP_RDMA0_RSZ0_SOUT";
	case 14:
		return
			"DISP_RDMA0_RSZ0_SOUT_OUT0_TO_DSI0_SEL_IN0";
	case 15:
		return
			"DISP_RDMA0_RSZ0_SOUT_OUT1_TO_DISP_COLOR0";
	case 16:
		return
			"DISP_RDMA0_SEL_TO_DISP_RDMA0";
	case 17:
		return
		"DISP_RDMA2_RSZ0_RSZ1_SOUT_OUT0_TO_DISP_OVL0_2L_IN2";
	case 18:
		return
		"DISP_RDMA2_RSZ0_RSZ1_SOUT_OUT1_TO_DISP_OVL0_IN2";
	case 19:
		return
		"DISP_RSZ0_TO_DISP_RSZ0_MOUT";
	case 20:
		return
			"DISP_RSZ0_MOUT_OUT0_TO_DISP_RDMA0_SEL_IN1";
	case 21:
		return
			"DISP_RSZ0_MOUT_OUT1_TO_DISP_WDMA0_SEL_IN1";
	case 22:
		return
			"DISP_RSZ0_MOUT_OUT2_TO_DISP_RDMA2_RSZ0_RSZ1_SOUT";
	case 23:
		return
			"DISP_RSZ0_SEL_TO_DISP_RSZ0";
	case 24:
		return
			"DISP_TOVL0_OUT0_MOUT_OUT0_TO_DISP_RDMA0_SEL_IN2";
	case 25:
		return
			"DISP_TOVL0_OUT0_MOUT_OUT1_TO_DISP_RSZ0_SEL_IN0";
	case 26:
		return
			"DISP_TOVL0_OUT0_MOUT_OUT2_TO_DISP_WDMA0_SEL_IN2";
	case 27:
		return
			"DISP_TOVL0_OUT1_MOUT_OUT0_TO_DISP_RDMA0_SEL_IN0";
	case 28:
		return
			"DISP_TOVL0_OUT1_MOUT_OUT1_TO_DISP_RSZ0_SEL_IN1";
	case 29:
		return
			"DISP_TOVL0_OUT1_MOUT_OUT2_TO_DISP_WDMA0_SEL_IN3";
	case 30:
		return
			"DISP_WDMA0_SEL_TO_DISP_WDMA0";
	case 31:
		return
			"DSI0_SEL_TO_THP_LMT_DSI0";
	default:
		return NULL;
	}
}

static char *ddp_signal_1_mt6789(int bit)
{
	switch (bit) {
	case 0:
		return
			"THP_LMT_DSI0_TO_DSI0";
	case 1:
		return
			"DISP_DSC_WRAP0_MOUT_OUT0_TO_DISP_WDMA0_SEL_IN4";
	case 2:
		return
			"DISP_DSC_WRAP0_MOUT_OUT1_TO_DSI0_SEL_IN2";
	default:
		return NULL;
	}
}

static char *ddp_greq_name_larb0_mt6789(int bit)
{
	switch (bit) {
	case 0:
		return "DISP_POSTMASK0 ";
	case 1:
		return "null module ";
	case 2:
		return "DISP_OVL0 ";
	case 3:
		return "DISP_FAKE_ENG0 ";
	default:
		return NULL;
	}
}

static char *ddp_greq_name_larb1_mt6789(int bit)
{
	switch (bit) {
	case 0:
		return "DISP_RDMA1 ";
	case 1:
		return "DISP_OVL0_2L ";
	case 2:
		return "DISP_RDMA0";
	case 3:
		return "DISP_WDMA0";
	case 4:
		return "DISP_FAKE_ENG1 ";
	default:
		return NULL;
	}
}

static char *ddp_get_mutex_module0_name_mt6789(unsigned int bit)
{
	switch (bit) {
	case 0:
		return "disp_ovl0";
	case 1:
		return "disp_ovl0_2l";
	case 2:
		return "disp_rdma0";
	case 3:
		return "disp_rsz0";
	case 4:
		return "disp_color0";
	case 5:
		return "disp_ccorr0";
	case 7:
		return "disp_aal0";
	case 8:
		return "disp_gamma0";
	case 9:
		return "disp_postmask0";
	case 10:
		return "disp_dither0";
	case 13:
		return "disp_dsc_wrap0";
	case 14:
		return "dsi0";
	case 15:
		return "disp_wdma0";
	case 16:
		return "disp_pwm0";
	default:
		return "mutex-unknown";
	}
}

//char *mtk_ddp_get_mutex_sof_name_mt6789(unsigned int regval)
//{
//	switch (regval) {
//	case MUTEX_SOF_SINGLE_MODE:
//		return "single";
//	case MUTEX_SOF_DSI0:
//		return "dsi0";
//	default:
//		DDPDUMP("%s, unknown reg=%d\n", __func__, regval);
//		return "unknown";
//	}
//}

static char *ddp_clock_0_mt6789(int bit)
{
	switch (bit) {
	case 0:
		return "disp_mutex0, ";
	case 1:
		return "apb_bus, ";
	case 2:
		return "disp_ovl0, ";
	case 3:
		return "disp_rdma0, ";
	case 4:
		return "disp_ovl0_2l, ";
	case 5:
		return "disp_wdma0, ";
	case 6:
		return "reserve, ";
	case 7:
		return "disp_rsz0, ";
	case 8:
		return "disp_aal0, ";
	case 9:
		return "disp_ccorr0, ";
	case 10:
		return "disp_color0, ";
	case 11:
		return "smi_infra, ";
	case 12:
		return "disp_dsc_wrap0 ";
	case 13:
		return "disp_gama0, ";
	case 14:
		return "disp_postmask0, ";
	case 15:
		return "reserve, ";
	case 16:
		return "disp_dither0, ";
	case 17:
		return "smi_common, ";
	case 18:
		return "reserve, ";
	case 19:
		return "dsi0, ";
	case 20:
		return "disp_fake_eng0, ";
	case 21:
		return "disp_fake_eng1, ";
	case 22:
		return "smi_gals, ";
	case 23:
		return "reserve, ";
	case 24:
		return "smi_iommu, ";
	default:
		return NULL;
	}
}

int mtk_ddp_mout_en_MT6789(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr)
{
	int value;

	/*DISP_TOVL0_OUT0_MOUT*/
	if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT0_MOUT_EN;
		value = MT6789_OVL0_2L_MOUT_TO_DISP_RDMA0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT0_MOUT_EN;
		value = MT6789_OVL0_2L_MOUT_TO_DISP_RSZ0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT0_MOUT_EN;
		value = MT6789_OVL0_2L_MOUT_TO_DISP_WDMA0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT1_MOUT_EN;
		value = MT6789_OVL0_MOUT_TO_DISP_RDMA0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT1_MOUT_EN;
		value = MT6789_OVL0_MOUT_TO_DISP_RSZ0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT1_MOUT_EN;
		value = MT6789_OVL0_MOUT_TO_DISP_WDMA0_SEL;
	} else if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RSZ0_MOUT_EN;
		value = MT6789_RSZ0_MOUT_TO_DISP_RDMA2_RSZ0_RSZ1_SOUT;
	/*DISP_DITHER0_MOUT*/
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_DSC0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
		value = MT6789_DITHER0_MOUT_TO_DISP_DISP_DSC_WRAP0;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
		value = MT6789_DITHER0_MOUT_TO_DISP_DISP_WDMA0;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
		value = MT6789_DITHER0_MOUT_TO_DISP_DSI0_SEL;
	} else if (cur == DDP_COMPONENT_DSC0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_DSC_WRAP0_MOUT_EN;
		value = MT6789_DISP_DSC_WRAP0_TO_DISP_DSI0_SEL;
	} else if (cur == DDP_COMPONENT_DSC0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_DSC_WRAP0_MOUT_EN;
		value = MT6789_DISP_DSC_WRAP0_TO_DISP_WDMA0_SEL;
	/*No cur or next component*/
	} else {
		value = -1;
	}
	return value;
}

int mtk_ddp_sel_in_MT6789(const struct mtk_mmsys_reg_data *data,
			  enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			  unsigned int *addr)
{
	int value;

	if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RDMA0_SEL_IN;
		value = MT6789_SEL_IN_RDMA0_FROM_DISP_OVL0;
	/*DISP_DSI0_SEL*/
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DSI0_SEL_IN;
		value = MT6789_SEL_IN_FROM_DISP_RDMA0_RSZ0_SOUT;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DSI0_SEL_IN;
		value = MT6789_SEL_IN_FROM_DISP_DITHERR0;
	} else if (cur == DDP_COMPONENT_DSC0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DSI0_SEL_IN;
		value = MT6789_SEL_IN_FROM_DISP_DSC_WRAP0;
	/*DISP_WDMA0_SEL*/
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
		value = MT6789_WDMA0_SEL_IN_FROM_DISP_DITHER0_MOUT;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
		value = MT6789_WDMA0_SEL_IN_FROM_DISP_OVL0;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
		value = MT6789_WDMA0_SEL_IN_FROM_DISP_OVL0_2L;
	/*DISP_RSZ0_SEL*/
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RSZ0_SEL_IN;
		value = MT6789_RSZ0_FROM_DISP_OVL0_2L;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RSZ0_SEL_IN;
		value = MT6789_RSZ0_FROM_DISP_OVL0;
	/*No cur or next component*/
	} else {
		value = -1;
	}

	return value;
}

int mtk_ddp_sout_sel_MT6789(const struct mtk_mmsys_reg_data *data,
			    enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			    unsigned int *addr)
{
	int value;

	/*DISP_RDMA2_RSZ0_RSZ1_SOUT*/
	if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0_2L) {
		*addr = MT6789_DISP_RDMA2_RSZ0_RSZ1_SOUT_SEL;
		value = MT6789_RSZ0_SOUT_TO_DISP_OVL0_2L;
	} else if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6789_DISP_RDMA2_RSZ0_RSZ1_SOUT_SEL;
		value = MT6789_RSZ0_SOUT_TO_DISP_OVL0;
	/*DISP_RDMA0_RSZ0_SOUT*/
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RDMA0_RSZ0_SOUT_SEL;
		value = MT6789_SOUT_TO_DISP_DSI0_SEL;
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_COLOR0) {
		*addr = MT6789_DISP_REG_CONFIG_DISP_RDMA0_RSZ0_SOUT_SEL;
		value = MT6789_RDMA0_SOUT_TO_DISP_COLOR0;
	/*No cur or next component*/
	} else {
		value = -1;
	}

	return value;
}

int mtk_ddp_ovl_bg_blend_en_MT6789(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr)
{
	int value;

	/*OVL0_2L*/
	if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_2L_GO_BG;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_2L_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_2L_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0_2L &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_2L_GO_BLEND;
	/*OVL0*/
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_OVL0_2L) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_GO_BG;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6789_MMSYS_OVL_CON;
		value = MT6789_DISP_OVL0_GO_BLEND;
	/*No cur or next component*/
	} else {
		value = -1;
	}

	return value;
}

void mtk_ddp_insert_dsc_prim_MT6789(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle)
{
	unsigned int addr, value;

	/* DISP_DITHER0_MOUT -> DISP_DSC_WRAP0 */
	addr = MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
	value = MT6789_DITHER0_MOUT_TO_DISP_DISP_DSC_WRAP0;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);

	/* DISP_DSC_WRAP0 -> DISP_DSI */
	addr = MT6789_DISP_REG_CONFIG_DISP_DSC_WRAP0_MOUT_EN;
	value = MT6789_DISP_DSC_WRAP0_TO_DISP_DSI0_SEL;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);

	addr = MT6789_DISP_REG_CONFIG_DSI0_SEL_IN;
	value = MT6789_SEL_IN_FROM_DISP_DSC_WRAP0;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);
}

void mtk_ddp_remove_dsc_prim_MT6789(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle)
{
	unsigned int addr, value;

	/* DISP_DITHER0_MOUT -> DISP_DSC_WRAP0 */
	addr = MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN;
	value = 0;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);

	/* DISP_DSC_WRAP0 -> DISP_DSI */
	addr = MT6789_DISP_REG_CONFIG_DISP_DSC_WRAP0_MOUT_EN;
	value = 0;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);

	addr = MT6789_DISP_REG_CONFIG_DSI0_SEL_IN;
	value = 0;
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
		       mtk_crtc->config_regs_pa + addr, value, ~0);
}

void mutex_dump_analysis_mt6789(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp =
		container_of(mutex, struct mtk_ddp, mutex[mutex->id]);
	int i = 0;
	int j = 0;
	char mutex_module[512] = {'\0'};
	char *p = NULL;
	int len = 0;
	unsigned int val;
	int string_buf_avail_len = 0;

	DDPDUMP("== DISP Mutex Analysis ==\n");
	for (i = 0; i < 5; i++) {
		p = mutex_module;
		len = 0;
		string_buf_avail_len = sizeof(mutex_module) - 1;
		if (readl_relaxed(ddp->regs +
				  DISP_REG_MUTEX_MOD(ddp->data, i)) == 0)
			continue;

		val = readl_relaxed(ddp->regs +
				    DISP_REG_MUTEX_SOF(ddp->data, i));

		len = snprintf(p, string_buf_avail_len, "MUTEX%d:SOF=%s,EOF=%s,WAIT=%d,module=(", i,
			      mtk_ddp_get_mutex_sof_name(
				      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF, val)),
			      mtk_ddp_get_mutex_sof_name(
				      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_EOF, val)),
			      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF_WAIT, val));

		if (len >= 0 && len <= string_buf_avail_len) {
			p += len;
			string_buf_avail_len -= len;
		} else {
			DDPPR_ERR("%s: out of mutex_module array range\n", __func__);
			return;
		}
		for (j = 0; j < 32; j++) {
			unsigned int regval = readl_relaxed(
				ddp->regs + DISP_REG_MUTEX_MOD(ddp->data, i));

			if ((regval & (1 << j))) {
				len = snprintf(p, string_buf_avail_len, "%s,",
					ddp_get_mutex_module0_name_mt6789(j));
				if (len >= 0 && len <= string_buf_avail_len) {
					p += len;
					string_buf_avail_len -= len;
				} else {
					DDPPR_ERR("%s: out of mutex_module array range\n",
						__func__);
					return;
				}
			}
		}
		DDPDUMP("%s)\n", mutex_module);
	}
}

void mmsys_config_dump_analysis_mt6789(void __iomem *config_regs)
{
	unsigned int i = 0;
	unsigned int reg = 0;
	char clock_on[512] = {'\0'};
	char *pos = NULL;
	char *name;
	int len = 0;
	int string_buf_avail_len = 0;

	//same address for 6789
	unsigned int valid0 =
		readl_relaxed(config_regs + MT6789_DISP_REG_CONFIG_DL_VALID_0);
	unsigned int valid1 =
		readl_relaxed(config_regs + MT6789_DISP_REG_CONFIG_DL_VALID_1);


	unsigned int ready0 =
		readl_relaxed(config_regs + MT6789_DISP_REG_CONFIG_DL_READY_0);
	unsigned int ready1 =
		readl_relaxed(config_regs + MT6789_DISP_REG_CONFIG_DL_READY_1);


	unsigned int greq0 =
		readl_relaxed(config_regs +
				MT6789_DISP_REG_CONFIG_SMI_LARB0_GREQ);
	unsigned int greq1 =
		readl_relaxed(config_regs +
				MT6789_DISP_REG_CONFIG_SMI_LARB1_GREQ);

	DDPDUMP("== DISP MMSYS_CONFIG ANALYSIS ==\n");
	reg = readl_relaxed(config_regs + DISP_REG_CONFIG_MMSYS_CG_CON0_MT6789);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_0_mt6789(i);
			if (name)
				strncat(clock_on, name, (sizeof(clock_on) -
							 strlen(clock_on) - 1));
		}
	}

	DDPDUMP("clock on modules:%s\n", clock_on);

	DDPDUMP("va0=0x%x,va1=0x%x\n",
		valid0, valid1);
	DDPDUMP("rd0=0x%x,rd1=0x%x\n",
		ready0, ready1);
	DDPDUMP("greq0=0x%x greq1=0x%x\n", greq0, greq1);
	for (i = 0; i < 32; i++) {
		name = ddp_signal_0_mt6789(i);
		if (!name)
			continue;

		pos = clock_on;
		string_buf_avail_len = sizeof(clock_on) - 1;

		if ((valid0 & (1 << i))) {
			len = snprintf(pos, string_buf_avail_len, "%s,", "v");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		} else {
			len = snprintf(pos, string_buf_avail_len, "%s,", "n");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		}
		if ((ready0 & (1 << i))) {
			len = snprintf(pos, string_buf_avail_len, "%s,", "r");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		} else {
			len = snprintf(pos, string_buf_avail_len, "%s,", "n");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		}

		len = snprintf(pos, string_buf_avail_len, ": %s,", name);
		if (len >= 0 && len <= string_buf_avail_len) {
			pos += len;
			string_buf_avail_len -= len;
		} else {
			DDPPR_ERR("%s: out of clock_on array range\n", __func__);
			return;
		}

		DDPDUMP("%s\n", clock_on);
	}

	for (i = 0; i < 32; i++) {
		name = ddp_signal_1_mt6789(i);
		if (!name)
			continue;

		pos = clock_on;
		string_buf_avail_len = sizeof(clock_on) - 1;

		if ((valid1 & (1 << i))) {
			len = snprintf(pos, string_buf_avail_len, "%s,", "v");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		} else {
			len = snprintf(pos, string_buf_avail_len, "%s,", "n");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		}
		if ((ready1 & (1 << i))) {
			len = snprintf(pos, string_buf_avail_len, "%s,", "r");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		} else {
			len = snprintf(pos, string_buf_avail_len, "%s,", "n");
			if (len >= 0 && len <= string_buf_avail_len) {
				pos += len;
				string_buf_avail_len -= len;
			} else {
				DDPPR_ERR("%s: out of clock_on array range\n", __func__);
				return;
			}
		}

		len = snprintf(pos, string_buf_avail_len, ": %s,", name);
		if (len >= 0 && len <= string_buf_avail_len) {
			pos += len;
			string_buf_avail_len -= len;
		} else {
			DDPPR_ERR("%s: out of clock_on array range\n", __func__);
			return;
		}

		DDPDUMP("%s\n", clock_on);
	}


	/* greq: 1 means SMI dose not grant, maybe SMI hang */
	if (greq0) {
		DDPDUMP("smi larb0 greq not grant module:\n");
		DDPDUMP(
		"(greq0: 1 means SMI dose not grant, maybe SMI larb0 hang)\n");
	}
	if (greq1) {
		DDPDUMP("smi larb1 greq not grant module:\n");
		DDPDUMP(
		"(greq1: 1 means SMI dose not grant, maybe SMI larb1 hang)\n");
	}

	clock_on[0] = '\0';
	for (i = 0; i < 32; i++) {
		if (greq0 & (1 << i)) {
			name = ddp_greq_name_larb0_mt6789(i);
			if (!name)
				continue;
			strncat(clock_on, name,
				(sizeof(clock_on) -
				strlen(clock_on) - 1));
		}
	}

	for (i = 0; i < 32; i++) {
		if (greq1 & (1 << i)) {
			name = ddp_greq_name_larb1_mt6789(i);
			if (!name)
				continue;
			strncat(clock_on, name,
				(sizeof(clock_on) -
				strlen(clock_on) - 1));
		}
	}

	DDPDUMP("%s\n", clock_on);

#ifdef CONFIG_MTK_SMI_EXT
	if (greq0 || greq1) {
		if (!in_interrupt())
			smi_debug_bus_hang_detect(false, "DISP");
		else
			DDPDUMP("%s, Can't smi dump in IRQ\n", __func__);
	}
#endif
}

// ddp_comp
#define MT6789_INFRA_DISP_DDR_CTL 0xB8
#define MT6789_INFRA_DISP_DDR_MASK 0xC02
#define MT6789_SODI_REQ_SEL_ALL                   REG_FLD_MSB_LSB(9, 8)
#define MT6789_SODI_REQ_VAL_ALL                   REG_FLD_MSB_LSB(13, 12)
#define MT6789_FLD_OVL0_RDMA_ULTRA_SEL            REG_FLD_MSB_LSB(3, 2)
#define MT6789_FLD_OVL0_2L_RDMA_ULTRA_SEL         REG_FLD_MSB_LSB(7, 6)

static void mt6789_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int sodi_req_val = 0, sodi_req_mask = 0;
	unsigned int emi_req_val = 0, emi_req_mask = 0;
	unsigned int ultra_ovl_val = 0, ultra_ovl_mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6789_SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6789_SODI_REQ_VAL_ALL);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_WDMA0);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_WDMA0);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_WDMA0);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!en),
					SODI_REQ_SEL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
				HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
				DVFS_HALT_MASK_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
					HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA0);
	} else
		return;

	if (handle == NULL) {
		unsigned int v;

		v = (readl(priv->config_regs + MMSYS_SODI_REQ_MASK)
			& (~sodi_req_mask));
		v += (sodi_req_val & sodi_req_mask);
		writel_relaxed(v, priv->config_regs + MMSYS_SODI_REQ_MASK);

		v = (readl(priv->config_regs +  MMSYS_EMI_REQ_CTL)
			& (~emi_req_mask));
		v += (emi_req_val & emi_req_mask);
		writel_relaxed(v, priv->config_regs +  MMSYS_EMI_REQ_CTL);
		if (priv->data->bypass_infra_ddr_control) {
			if (!IS_ERR(priv->infra_regs)) {
				v = (readl(priv->infra_regs + MT6789_INFRA_DISP_DDR_CTL)
					| MT6789_INFRA_DISP_DDR_MASK);
				writel_relaxed(v, priv->infra_regs + MT6789_INFRA_DISP_DDR_CTL);
			} else
				DDPINFO("%s: failed to disable infra ddr control\n", __func__);
		}

		/* enable ultra signal from rdma to ovl0 and ovl1_2l */
		v = readl(priv->config_regs +  DISP_REG_CONFIG_MMSYS_MISC);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6789_FLD_OVL0_RDMA_ULTRA_SEL);
		v = (v & ~ultra_ovl_mask) | (ultra_ovl_val & ultra_ovl_mask);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6789_FLD_OVL0_2L_RDMA_ULTRA_SEL);
		v = (v & ~ultra_ovl_mask) | (ultra_ovl_val & ultra_ovl_mask);
		writel_relaxed(v, priv->config_regs +  DISP_REG_CONFIG_MMSYS_MISC);
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);
		if (priv->data->bypass_infra_ddr_control) {
			if (priv->infra_regs_pa) {
				cmdq_pkt_write(handle, NULL,  priv->infra_regs_pa +
						MT6789_INFRA_DISP_DDR_CTL,
						MT6789_INFRA_DISP_DDR_MASK,
						MT6789_INFRA_DISP_DDR_MASK);
			} else
				DDPINFO("%s: failed to disable infra ddr control\n", __func__);
		}

		/* enable ultra signal from rdma to ovl0 and ovl1_2l*/
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6789_FLD_OVL0_RDMA_ULTRA_SEL);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa + DISP_REG_CONFIG_MMSYS_MISC,
			       ultra_ovl_val, ultra_ovl_mask);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6789_FLD_OVL0_2L_RDMA_ULTRA_SEL);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa + DISP_REG_CONFIG_MMSYS_MISC,
			       ultra_ovl_val, ultra_ovl_mask);
	}
}

// drv
static const enum mtk_ddp_comp_id mt6789_mtk_ddp_main[] = {
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_OVL0_2L,
#endif
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6789_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6789_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6789_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6789_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6789_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6789_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_crtc_path_data mt6789_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6789_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6789_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6789_addon_main,
};

static const struct mtk_crtc_path_data mt6789_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6789_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6789_mtk_ddp_third),
	.addon_data = mt6789_addon_ext,
};

static const struct mtk_session_mode_tb mt6789_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6789_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6789_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6789_fake_eng_reg),
	.fake_eng_reg = mt6789_fake_eng_reg,
};

const struct mtk_mmsys_driver_data mt6789_mmsys_driver_data = {
	.main_path_data = &mt6789_mtk_main_path_data,
	.ext_path_data = &mt6789_mtk_third_path_data,
	.third_path_data = &mt6789_mtk_third_path_data,
	.fake_eng_data = &mt6789_fake_eng_data,
	.mmsys_id = MMSYS_MT6789,
	.mode_tb = mt6789_mode_tb,
	.sodi_config = mt6789_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
};

// mipi_tx
static int mtk_mipi_tx_pll_prepare_mt6789(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	DDPINFO(
		"prepare: %u MHz, mipi_tx->data_rate_adpt: %d MHz, mipi_tx->data_rate : %d MHz\n",
		rate, mipi_tx->data_rate_adpt,
		(mipi_tx->data_rate / 1000000));

	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en,
			FLD_DSI_SW_CTL_EN, 1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en,
			FLD_DSI_SW_CTL_EN, 1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en,
			FLD_DSI_SW_CTL_EN, 1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en,
			FLD_DSI_SW_CTL_EN, 1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en,
			FLD_DSI_SW_CTL_EN, 1);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = _dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       RG_DSI_PLL_EN);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);

	return 0;
}

static void mtk_mipi_tx_pll_unprepare_mt6789(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

//	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en,
//			DSI_D0_SW_CTL_EN);
//	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en,
//			DSI_D1_SW_CTL_EN);
//	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en,
//			DSI_D2_SW_CTL_EN);
//	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en,
//			DSI_D3_SW_CTL_EN);
//	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en,
//			DSI_CK_SW_CTL_EN);

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPDBG("%s-\n", __func__);
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6789(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}
	/*set volate*/
	writel(0x4444236A, mipi_tx->regs + MIPITX_VOLTAGE_SEL);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt<<6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	DDPDBG("%s-\n", __func__);

	return 0;
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6789(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);

}

const struct mtk_mipitx_data mt6789_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6789,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6789,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
	.mipi_tx_ssc_en = mtk_mipi_tx_ssc_en,
};

const struct mtk_mipitx_data mt6789_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6789,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6789,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};


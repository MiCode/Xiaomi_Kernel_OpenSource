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
#include "mtk_drm_6835.h"

/* AID offset in mmsys config */
#define MT6835_OVL0_AID_SEL		(0xB00UL)
#define MT6835_OVL0_2L_AID_SEL	(0xB04UL)
#define MT6835_OVL1_2L_AID_SEL	(0xB08UL)
#define MT6835_OVL0_2L_NWCG_AID_SEL	(0xB0CUL)
#define MT6835_OVL1_2L_NWCG_AID_SEL	(0xB10UL)

//#define MTK_DRM_BRINGUP_STAGE
//#define DRM_BYPASS_PQ

static void mt6835_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			struct cmdq_pkt *handle, void *data);

// overlay
static const struct compress_info compr_info_mt6835  = {
	.name = "AFBC_V1_2_MTK_1",
	.l_config = &compr_l_config_AFBC_V1_2,
};

unsigned int mtk_ovl_aid_sel_MT6835(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
		return MT6835_OVL0_AID_SEL;
	case DDP_COMPONENT_OVL0_2L:
		return MT6835_OVL0_2L_AID_SEL;
	case DDP_COMPONENT_OVL1_2L:
		return MT6835_OVL1_2L_AID_SEL;
	case DDP_COMPONENT_OVL0_2L_NWCG:
		return MT6835_OVL0_2L_NWCG_AID_SEL;
	case DDP_COMPONENT_OVL1_2L_NWCG:
		return MT6835_OVL1_2L_NWCG_AID_SEL;
	default:
		DDPINFO("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

resource_size_t mtk_ovl_mmsys_mapping_MT6835(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL1_2L_NWCG:
		return priv->config_regs_pa;
	default:
		DDPINFO("%s invalid ovl module=%d\n", __func__, comp->id);
		return 0;
	}
}

const struct mtk_disp_ovl_data mt6835_ovl_driver_data = {
	.addr = DISP_REG_OVL_ADDR_BASE,
	.el_addr_offset = 0x10,
	.el_hdr_addr = 0xfb4,
	.el_hdr_addr_offset = 0x10,
	.fmt_rgb565_is_0 = true,
	.fmt_uyvy = 4U << 12,
	.fmt_yuyv = 5U << 12,
	.compr_info = &compr_info_mt6835,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.preultra_th_dc = 0xe0,
	.fifo_size = 288,
	.issue_req_th_dl = 191,
	.issue_req_th_dc = 31,
	.issue_req_th_urg_dl = 95,
	.issue_req_th_urg_dc = 31,
	.greq_num_dl = 0xbbbb,
	.is_support_34bits = true,
	.aid_sel_mapping = &mtk_ovl_aid_sel_MT6835,
	.mmsys_mapping = &mtk_ovl_mmsys_mapping_MT6835,
	.source_bpc = 8,
};

// wdma
const struct mtk_disp_wdma_data mt6835_wdma_driver_data = {
	.fifo_size_1plane = 766,
	.fifo_size_uv_1plane = 29,
	.fifo_size_2plane = 506,
	.fifo_size_uv_2plane = 253,
	.fifo_size_3plane = 503,
	.fifo_size_uv_3plane = 125,
	.sodi_config = mt6835_mtk_sodi_config,
	.support_shadow = false,
	.need_bypass_shadow = true,
	.is_support_34bits = true,
};

// rdma
const struct mtk_disp_rdma_data mt6835_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.pre_ultra_low_us = 250,
	.pre_ultra_high_us = 260,
	.ultra_low_us = 230,
	.ultra_high_us = 250,
	.urgent_low_us = 110,
	.urgent_high_us = 120,
	.sodi_config = mt6835_mtk_sodi_config,
	.shadow_update_reg = 0x00bc,
	.support_shadow = false,
	.need_bypass_shadow = false,
	.has_greq_urg_num = true,
	.dsi_buffer = false,
	.disable_underflow = true,
};

// rsz
const struct mtk_disp_rsz_data mt6835_rsz_driver_data = {
	.tile_length = 1608, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

// postmask
const struct mtk_disp_postmask_data mt6835_postmask_driver_data = {
	.is_support_34bits = true,
};

// aal
const struct mtk_disp_aal_data mt6835_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

// ccorr
const struct mtk_disp_ccorr_data mt6835_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 1,
};

// color
#define DISP_COLOR_START_MT6835 0x0c00
const struct mtk_disp_color_data mt6835_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6835,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000, 0x14010000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};


// dither
const struct mtk_disp_dither_data mt6835_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

// dsi
const struct mtk_dsi_driver_data mt6835_dsi_driver_data = {
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
	.buffer_unit = 18,
	.sram_unit = 18,
	.max_vfp = 0xffe,
	.mmclk_by_datarate = mtk_dsi_set_mmclk_by_datarate_V1,
};

// ddp
#define MT6835_MMSYS_OVL_CON 0xF08
	#define MT6835_DISP_OVL0_GO_BLEND            BIT(0)
	#define MT6835_DISP_OVL0_GO_BG               BIT(1)
	#define MT6835_DISP_OVL0_2L_GO_BLEND         BIT(2)
	#define MT6835_DISP_OVL0_2L_GO_BG            BIT(3)
	#define MT6835_DISP_OVL1_2L_GO_BLEND         BIT(4)
	#define MT6835_DISP_OVL1_2L_GO_BG            BIT(5)

#define DISP_REG_CONFIG_MMSYS_CG_CON0_MT6835 0x100
#define DISP_REG_CONFIG_MMSYS_CG_CON1_MT6835 0x110

#define MT6835_DISP_REG_CONFIG_DL_VALID_0 0xe9c
#define MT6835_DISP_REG_CONFIG_DL_VALID_1 0xea0
#define MT6835_DISP_REG_CONFIG_DL_VALID_2 0xea4
#define MT6835_DISP_REG_CONFIG_DL_VALID_3 0xe80
#define MT6835_DISP_REG_CONFIG_DL_VALID_4 0xe84
#define MT6835_DISP_REG_CONFIG_DL_VALID_5 0xe88

#define MT6835_DISP_REG_CONFIG_DL_READY_0 0xea8
#define MT6835_DISP_REG_CONFIG_DL_READY_1 0xeac
#define MT6835_DISP_REG_CONFIG_DL_READY_2 0xeb0
#define MT6835_DISP_REG_CONFIG_DL_READY_3 0xe70
#define MT6835_DISP_REG_CONFIG_DL_READY_4 0xe74
#define MT6835_DISP_REG_CONFIG_DL_READY_5 0xe78
#define MT6835_DISP_REG_CONFIG_SMI_LARB0_GREQ 0x8dc
#define MT6835_DISP_REG_CONFIG_SMI_LARB1_GREQ 0x8e0

static const unsigned int mt6835_mutex_mod[DDP_COMPONENT_ID_MAX] = {
		[DDP_COMPONENT_OVL0] = MT6835_MUTEX_MOD_DISP_OVL0,
		[DDP_COMPONENT_OVL1_2L] = MT6835_MUTEX_MOD_DISP_OVL1_2L,
		[DDP_COMPONENT_RDMA0] = MT6835_MUTEX_MOD_DISP_RDMA0,
		[DDP_COMPONENT_RSZ0] = MT6835_MUTEX_MOD_DISP_RSZ0,
		[DDP_COMPONENT_COLOR0] = MT6835_MUTEX_MOD_DISP_COLOR0,
		[DDP_COMPONENT_CCORR0] = MT6835_MUTEX_MOD_DISP_CCORR0,
		[DDP_COMPONENT_AAL0] = MT6835_MUTEX_MOD_DISP_AAL0,
		[DDP_COMPONENT_GAMMA0] = MT6835_MUTEX_MOD_DISP_GAMMA0,
		[DDP_COMPONENT_POSTMASK0] = MT6835_MUTEX_MOD_DISP_POSTMASK0,
		[DDP_COMPONENT_DITHER0] = MT6835_MUTEX_MOD_DISP_DITHER0,
		[DDP_COMPONENT_DSI0] = MT6835_MUTEX_MOD_DISP_DSI0,
		[DDP_COMPONENT_WDMA0] = MT6835_MUTEX_MOD_DISP_WDMA0,
		[DDP_COMPONENT_PWM0] = MT6835_MUTEX_MOD_DISP_PWM0,
};

static const unsigned int mt6835_mutex_sof[DDP_MUTEX_SOF_MAX] = {
		[DDP_MUTEX_SOF_SINGLE_MODE] = MT6835_MUTEX_SOF_SINGLE_MODE,
		[DDP_MUTEX_SOF_DSI0] =
			MT6835_MUTEX_SOF_DSI0 | MT6835_MUTEX_EOF_DSI0,
};

const struct mtk_disp_ddp_data mt6835_ddp_driver_data = {
	.mutex_mod = mt6835_mutex_mod,
	.mutex_sof = mt6835_mutex_sof,
	.mutex_mod_reg = MT6835_DISP_MUTEX0_MOD0,
	.mutex_sof_reg = MT6835_DISP_MUTEX0_SOF,
};

const struct mtk_mmsys_reg_data mt6835_mmsys_reg_data = {
	.ovl0_mout_en = MT6835_MMSYS_OVL_CON,
	.rdma0_sout_sel_in = MT6835_DISP_RDMA0_SOUT_SEL,
	.rdma0_sout_color0 = MT6835_DISP_RDMA0_SOUT_TO_DISP_TDSHP0,
};

static char *ddp_signal_0_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return
			"DISP_AAL0_TO_DISP_GAMMA0_VALID";
	case 1:
		return
			"DISP_C3D0_TO_DISP_C3D0_SOUT_VALID";
	case 2:
		return
			"DISP_C3D0_SEL_TO_DISP_C3D0_VALID";
	case 3:
		return
			"DISP_C3D0_SOUT_OUT0_TO_DISP_COLOR0_SEL_IN0_VALID";
	case 4:
		return
			"DISP_C3D0_SOUT_OUT1_TO_DISP_MDP_AAL0_SEL_IN1_VALID";
	case 5:
		return
			"DISP_CCORR0_TO_DISP_CCORR1_VALID";
	case 6:
		return
			"DISP_CCORR1_TO_DISP_CCORR1_SOUT_VALID";
	case 7:
		return
			"DISP_CCORR1_SOUT_OUT0_TO_DISP_MDP_AAL0_SEL_IN0_VALID";
	case 8:
		return
			"DISP_CCORR1_SOUT_OUT1_TO_DISP_C3D0_SEL_IN1_VALID";
	case 9:
		return
			"DISP_CHIST0_SEL_TO_DISP_CHIST0_VALID";
	case 10:
		return
			"DISP_CHIST1_SEL_TO_DISP_CHIST1_VALID";
	case 11:
		return
			"DISP_CM0_TO_DISP_SPR0_VALID";
	case 12:
		return
			"DISP_COLOR0_TO_DISP_CCORR0_VALID";
	case 13:
		return
			"DISP_COLOR0_SEL_TO_DISP_COLOR0_VALID";
	case 14:
		return
			"DISP_DITHER0_TO_DISP_DITHER0_MOUT_VALID";
	case 15:
		return
			"DISP_DITHER0_MOUT_OUT0_TO_DISP_CM0_VALID";
	case 16:
		return
			"DISP_DITHER0_MOUT_OUT1_TO_DISP_CHIST0_SEL_IN2_VALID";
	case 17:
		return
			"DISP_DITHER0_MOUT_OUT2_TO_DISP_CHIST1_SEL_IN2_VALID";
	case 18:
		return
			"DISP_DLI0_SOUT_OUT0_TO_DISP_MERGE0_R_SEL_IN0_VALID";
	case 19:
		return
			"DISP_DLI0_SOUT_OUT1_TO_DISP_DSC_WRAP0_R_SEL_IN0_VALID";
	case 20:
		return
			"DISP_DLI2_SOUT_OUT0_TO_DISP_MERGE0_R_SEL_IN1_VALID";
	case 21:
		return
			"DISP_DLI2_SOUT_OUT1_TO_DISP_DSC_WRAP0_R_SEL_IN1_VALID";
	case 22:
		return
			"DISP_DLI_ASYNC0_TO_DISP_DLI_RELAY0_VALID";
	case 23:
		return
			"DISP_DLI_ASYNC1_TO_DISP_DLI_RELAY1_VALID";
	case 24:
		return
			"DISP_DLI_ASYNC2_TO_DISP_DLI_RELAY2_VALID";
	case 25:
		return
			"DISP_DLI_ASYNC3_TO_DISP_DLI_RELAY3_VALID";
	case 26:
		return
			"DISP_DLI_RELAY0_TO_DISP_DLI0_SOUT_VALID";
	case 27:
		return
			"DISP_DLI_RELAY1_TO_DISP_MAIN0_SEL_IN0_VALID";
	case 28:
		return
			"DISP_DLI_RELAY2_TO_DISP_DLI2_SOUT_VALID";
	case 29:
		return
			"DISP_DLI_RELAY3_TO_DISP_Y2R0_VALID";
	case 30:
		return
			"DISP_DLO_RELAY0_TO_DISP_DLO0_ASYNC0_VALID";
	case 31:
		return
			"DISP_DLO_RELAY1_TO_DISP_DLO0_ASYNC1_VALID";
	default:
		return NULL;
	}
}

static char *ddp_signal_1_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return
			"DISP_DLO_RELAY2_TO_DISP_DLO0_ASYNC2_VALID";
	case 1:
		return
			"DISP_DLO_RELAY3_TO_DISP_DLO0_ASYNC3_VALID";
	case 2:
		return
			"DISP_DP_INTF0_SEL_TO_DISP_SUB0_TX_SOUT_VALID";
	case 3:
		return
			"DISP_DSC_WRAP0_L_SEL_TO_DISP_DSC_WRAP0_IN0_VALID";
	case 4:
		return
			"DISP_DSC_WRAP0_MOUT_OUT0_TO_DISP_MAIN0_SEL_IN3_VALID";
	case 5:
		return
			"DISP_DSC_WRAP0_MOUT_OUT1_TO_DISP_SUB0_SEL_IN3_VALID";
	case 6:
		return
			"DISP_DSC_WRAP0_MOUT_OUT2_TO_DISP_WDMA1_SEL_IN0_VALID";
	case 7:
		return
			"DISP_DSC_WRAP0_OUT0_TO_DISP_DSC_WRAP0_MOUT_VALID";
	case 8:
		return
			"DISP_DSC_WRAP0_OUT1_TO_DISP_DLO_RELAY1_VALID";
	case 9:
		return
			"DISP_DSC_WRAP0_R_SEL_TO_DISP_DSC_WRAP0_IN1_VALID";
	case 10:
		return
			"DISP_DSI0_SEL_TO_DISP_DSI0_VALID";
	case 11:
		return
			"DISP_GAMMA0_TO_DISP_POSTMASK0_VALID";
	case 12:
		return
			"DISP_MAIN0_SEL_TO_DISP_MAIN0_SOUT_VALID";
	case 13:
		return
			"DISP_MAIN0_SOUT_OUT0_TO_DISP_RDMA0_IN_RELAY_VALID";
	case 14:
		return
			"DISP_MAIN0_SOUT_OUT1_TO_DISP_DSI0_SEL_IN1_VALID";
	case 15:
		return
			"DISP_MAIN_OVL_DISP_PQ0_SEL_TO_DISP_PQ0_SEL_IN1_VALID";
	case 16:
		return
			"DISP_MAIN_OVL_DISP_UFBC_WDMA0_SEL_TO_DISP_UFBC_WDMA0_SEL_IN1_VALID";
	case 17:
		return
			"DISP_MAIN_OVL_DISP_WDMA0_SEL_TO_DISP_WDMA0_SEL_IN1_VALID";
	case 18:
		return
			"DISP_MAIN_OVL_DMDP_SEL_TO_DISP_DLO_RELAY3_VALID";
	case 19:
		return
			"DISP_MDP_AAL0_TO_DISP_AAL0_VALID";
	case 20:
		return
			"DISP_MDP_AAL0_SEL_TO_DISP_MDP_AAL0_VALID";
	case 21:
		return
			"DISP_MERGE0_TO_DISP_MERGE0_MOUT_VALID";
	case 22:
		return
			"DISP_MERGE0_L_SEL_TO_DISP_MERGE0_IN0_VALID";
	case 23:
		return
			"DISP_MERGE0_MOUT_OUT0_TO_DISP_WDMA0_SEL_IN3_VALID";
	case 24:
		return
			"DISP_MERGE0_MOUT_OUT1_TO_DISP_MAIN0_SEL_IN2_VALID";
	case 25:
		return
			"DISP_MERGE0_MOUT_OUT2_TO_DISP_SUB0_SEL_IN2_VALID";
	case 26:
		return
			"DISP_MERGE0_MOUT_OUT3_TO_DISP_UFBC_WDMA0_SEL_IN3_VALID";
	case 27:
		return
			"DISP_MERGE0_R_SEL_TO_DISP_MERGE0_IN1_VALID";
	case 28:
		return
			"DISP_OVL0_2L_BLEND_MOUT_OUT0_TO_DISP_RSZ0_SEL_IN0_VALID";
	case 29:
		return
			"DISP_OVL0_2L_BLEND_MOUT_OUT1_TO_DISP_MAIN_OVL_DMDP_SEL_IN1_VALID";
	case 30:
		return
			"DISP_OVL0_2L_BLEND_MOUT_OUT2_TO_DISP_MAIN_OVL_DISP_WDMA_SEL_IN1_VALID";
	case 31:
		return
			"DISP_OVL0_2L_BLEND_MOUT_OUT3_TO_DISP_MAIN_OVL_UFBC_WDMA_SEL_IN1_VALID";
	default:
		return NULL;
	}
}

static char *ddp_greq_name_larb0_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return "DISP_POSTMASK0 ";
	case 1:
		return "DISP_OVL0 ";
	case 2:
		return "DISP_OVL0_HDR ";
	case 3:
		return "DISP_WDMA0 ";
	case 4:
		return "DISP_FAKE_ENG0 ";
	default:
		return NULL;
	}
}

static char *ddp_greq_name_larb1_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return "DISP_RDMA0 ";
	case 1:
		return "DISP_OVL1_2L ";
	case 2:
		return "DISP_OVL1_2L_HDR ";
	case 3:
		return "DISP_WDMA0_UFBC ";
	case 4:
		return "DISP_FAKE_ENG1 ";
	default:
		return NULL;
	}
}

static char *ddp_get_mutex_module0_name_mt6835(unsigned int bit)
{
	switch (bit) {
	case 0:
		return "disp_ovl0";
	case 2:
		return "disp_ovl1_2l";
	case 3:
		return "disp_rsz0";
	case 4:
		return "disp_rdma0";
	case 7:
		return "disp_color0";
	case 8:
		return "disp_ccorr0";
	case 11:
		return "disp_aal0";
	case 12:
		return "disp_gamma0";
	case 13:
		return "disp_postmask0";
	case 14:
		return "disp_dither0";
	case 22:
		return "dsi0";
	case 23:
		return "disp_wdma0";
	default:
		return "mutex-unknown";
	}
}

static char *ddp_get_mutex_module1_name_mt6835(unsigned int bit)
{
	switch (bit) {
	case 0:
		return "inlinerot0";
	case 1:
		return "disp_dli_async0";
	case 2:
		return "disp_dli_async1";
	case 3:
		return "disp_dli_async2";
	case 4:
		return "disp_dli_async3";
	case 5:
		return "disp_dlo_async0";
	case 6:
		return "disp_dlo_async1";
	case 7:
		return "disp_dlo_async2";
	case 8:
		return "disp_dlo_async3";
	case 9:
		return "disp_pwm0";
	default:
		return "mutex-unknown";
	}
}

static char *ddp_clock_0_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return "disp_mutex0, ";
	case 1:
		return "disp_ovl0, ";
	case 2:
		return "disp_merge0, ";
	case 3:
		return "disp_fake_eng0, ";
	case 4:
		return "disp_inlinerot0, ";
	case 5:
		return "disp_wdma0, ";
	case 6:
		return "disp_fake_eng1, ";
	case 7:
		return "disp_dpi0, ";
	case 8:
		return "disp_ovl0_2l_nwcg, ";
	case 9:
		return "disp_rdma0, ";
	case 10:
		return "disp_rdma1, ";
	case 11:
		return "reserve, ";
	case 12:
		return "reserve, ";
	case 13:
		return "reserve, ";
	case 14:
		return "reserve, ";
	case 15:
		return "reserve, ";
	case 16:
		return "reserve, ";
	case 17:
		return "disp_rsz0, ";
	case 18:
		return "disp_color0, ";
	case 19:
		return "disp_ccorr0, ";
	case 20:
		return "disp_ccorr1, ";
	case 21:
		return "disp_aal0, ";
	case 22:
		return "disp_gamma0, ";
	case 23:
		return "disp_postmask0, ";
	case 24:
		return "disp_dither0, ";
	case 25:
		return "disp_cm0, ";
	case 26:
		return "disp_spr0, ";
	case 27:
		return "disp_dsc_wrap0, ";
	case 28:
		return "dummy, ";
	case 29:
		return "dsi0, ";
	case 30:
		return "disp_ufbc_wdma0, ";
	case 31:
		return "disp_wdma1, ";
	default:
		return NULL;
	}
}

static char *ddp_clock_1_mt6835(int bit)
{
	switch (bit) {
	case 0:
		return "reserve, ";
	case 1:
		return "apb_bus, ";
	case 2:
		return "disp_tdshp0, ";
	case 3:
		return "disp_c3d0, ";
	case 4:
		return "disp_y2r0, ";
	case 5:
		return "reserve, ";
	case 6:
		return "disp_chist0, ";
	case 7:
		return "reserve, ";
	case 8:
		return "disp_ovl0_2l, ";
	case 9:
		return "disp_dli_async3, ";
	case 10:
		return "disp_dlo_async3, ";
	case 11:
		return "dummy, ";
	case 12:
		return "disp_ovl1_2l, ";
	case 13:
		return "dummy, ";
	case 14:
		return "dummy, ";
	case 15:
		return "dummy, ";
	case 16:
		return "reserve, ";
	case 17:
		return "dummy, ";
	case 18:
		return "dummy, ";
	case 19:
		return "dummy, ";
	case 20:
		return "smi_sub_comm0, ";
	case 21:
		return "reserve, ";
	case 22:
		return "reserve, ";
	case 23:
		return "reserve, ";
	case 24:
		return "reserve, ";
	case 25:
		return "reserve, ";
	case 26:
		return "reserve, ";
	case 27:
		return "reserve, ";
	case 28:
		return "reserve, ";
	case 29:
		return "reserve, ";
	case 30:
		return "reserve, ";
	case 31:
		return "reserve, ";
	default:
		return NULL;
	}
}

int mtk_ddp_mout_en_MT6835(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr)
{
	int value;

	if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6835_DISP_OVL1_2L_BLEND_MOUT_EN;
		value = MT6835_DISP_OVL1_2L_MOUT_TO_DISP_RSZ0_SEL;
	} else if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6835_DISP_RSZ0_MOUT_EN;
		value = MT6835_DISP_RSZ0_MOUT_TO_DISP_OVL_SOUT;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL) {
		*addr = MT6835_DISP_OVL0_BLEND_MOUT_EN;
		value = MT6835_DISP_OVL0_MOUT_TO_DISP_MAIN_OVL_DISP_PQ0_SEL;
	} else if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_DISP_OVL1_2L_BLEND_MOUT_EN;
		value = MT6835_DISP_OVL1_2L_MOUT_TO_DISP_WDMA0_SEL;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_DISP_OVL0_BLEND_MOUT_EN;
		value = MT6835_DISP_OVL0_MOUT_TO_DISP_MAIN_OVL_DISP_WDMA_SEL;
	} else if (cur == DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6835_DISP_RDMA0_POS_MOUT_EN;
		value = MT6835_DISP_RDMA0_POS_MOUT_TO_DISP_RDMA0_SEL;
	} else if (cur == DDP_COMPONENT_POSTMASK0 &&
		next == DDP_COMPONENT_DITHER0) {
		*addr = MT6835_DISP_POSTMASK0_MOUT_EN;
		value = MT6835_DISP_POSTMASK0_MOUT_TO_DISP_DITHER0_SEL;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_SPR0_VIRTUAL) {
		*addr = MT6835_DISP_DITHER0_MOUT_EN;
		value = MT6835_DISP_DITHER0_MOUT_TO_DISP_SPR0_VIRTUAL;
	} else if (cur == DDP_COMPONENT_SPR0_VIRTUAL &&
		next == DDP_COMPONENT_PQ0_VIRTUAL) {
		*addr = MT6835_DISP_SPR0_MOUT_EN;
		value = MT6835_DISP_SPR0_MOUT_TO_DISP_PQ0_SOUT;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_WDMA0){
		*addr = MT6835_DISP_SPR0_MOUT_EN;
		value = MT6835_DISP_SPR0_MOUT_TO_DISP_WDMA0_SEL;
	} else {
		value = -1;
	}
	return value;

}

int mtk_ddp_sel_in_MT6835(const struct mtk_mmsys_reg_data *data,
			  enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			  unsigned int *addr)
{
	int value;

	if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6835_DISP_RSZ0_SEL;
		value = MT6835_DISP_RSZ0_FROM_DISP_OVL1_2L_BLEND_MOUT;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6835_DISP_RSZ0_SEL;
		value = MT6835_DISP_RSZ0_FROM_DISP_OVL0_BLEND_MOUT;
	} else if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6835_DISP_OVL0_UFOD_SEL;
		value = MT6835_DISP_OVL0_UFOD_FROM_DISP_RSZ0_MAIN_OVL_SOUT;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL) {
		*addr = MT6835_DISP_MAIN_OVL_DISP_PQ0_SEL;
		value = MT6835_DISP_MAIN_OVL_DISP_PQ0_FROM_DISP_OVL0_BLEND_MOUT;
	}  else if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_DISP_MAIN_OVL_DISP_WDMA_SEL;
		value = MT6835_DISP_MAIN_OVL_DISP_WDMA_FROM_DISP_OVL1_2L_BLEND_MOUT;
	}  else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_DISP_MAIN_OVL_DISP_WDMA_SEL;
		value = MT6835_DISP_MAIN_OVL_DISP_WDMA_FROM_DISP_OVL0_BLEND_MOUT;
	} else if (cur == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6835_DISP_WDMA0_SEL;
		value = MT6835_DISP_WDMA0_FROM_DISP_MAIN_OVL_DISP_WDMA_SEL;
	} else if (cur == DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL &&
		next == DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL) {
		*addr = MT6835_DISP_PQ0_SEL;
		value = MT6835_DISP_PQ0_FROM_DISP_MAIN_OVL_DISP_PQ0_SEL;
	} else if (cur == DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6835_DISP_RDMA0_SEL;
		value = MT6835_DISP_RDMA0_FROM_DISP_RDMA0_POS_MOUT;
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_MAIN0_TX_VIRTUAL0) {
		*addr = MT6835_DISP_DSI0_SEL;
		value = MT6835_DISP_DSI0_FROM_DISP_RDMA0_OUT_RELAY;
	}  else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_TDSHP_VIRTUAL0) {
		*addr = MT6835_DISP_RDMA0_POS_SEL;
		value = MT6835_DISP_RDMA0_POS_FROM_DISP_RDMA0_SOUT;
	} else if (cur == DDP_COMPONENT_TDSHP_VIRTUAL0 &&
		next == DDP_COMPONENT_COLOR0) {
		*addr = MT6835_DISP_COLOR0_SEL;
		value = MT6835_DISP_COLOR0_FROM_DISP_TDSHP0_SOUT;
	} else if (cur == DDP_COMPONENT_CCORR0 &&
		next == DDP_COMPONENT_AAL0) {
		*addr = MT6835_DISP_MDP_AAL0_SEL;
		value = MT6835_DISP_MDP_AAL0_FROM_DISP_CCORR0_SOUT;
	} else if (cur == DDP_COMPONENT_PQ0_VIRTUAL &&
		next == DDP_COMPONENT_MAIN0_VIRTUAL) {
		*addr = MT6835_DISP_MAIN0_SEL;
		value = MT6835_DISP_MAIN0_FROM_DISP_PQ0_SOUT;
	} else if (cur == DDP_COMPONENT_MAIN0_VIRTUAL &&
		next == DDP_COMPONENT_MAIN0_TX_VIRTUAL0) {
		*addr = MT6835_DISP_DSI0_SEL;
		value = MT6835_DISP_DSI0_FROM_DISP_MAIN0_SOUT;
	} else if (cur == DDP_COMPONENT_DITHER0 &&
		next == DDP_COMPONENT_WDMA0) {
		*addr = MT6835_DISP_WDMA0_SEL;
		value = MT6835_DISP_WDMA0_FROM_DISP_SPR0_VIRTUAL;
	} else {
		value = -1;
	}

	return value;
}

int mtk_ddp_sout_sel_MT6835(const struct mtk_mmsys_reg_data *data,
			    enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			    unsigned int *addr)
{
	int value;

	if (cur == DDP_COMPONENT_RSZ0 &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6835_DISP_RSZ0_MAIN_OVL_SOUT_SEL;
		value = MT6835_DISP_RSZ0_MAIN_OVL_SOUT_TO_DISP_OVL0_UFOD_SEL;
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_MAIN0_TX_VIRTUAL0) {
		*addr = MT6835_DISP_RDMA0_SOUT_SEL;
		value = MT6835_DISP_RDMA0_SOUT_TO_DISP_DSI0_SEL;
	} else if (cur == DDP_COMPONENT_RDMA0 &&
		next == DDP_COMPONENT_TDSHP_VIRTUAL0) {
		*addr = MT6835_DISP_RDMA0_SOUT_SEL;
		value = MT6835_DISP_RDMA0_SOUT_TO_DISP_TDSHP0;
	} else if (cur == DDP_COMPONENT_TDSHP_VIRTUAL0 &&
		next == DDP_COMPONENT_COLOR0) {
		*addr = MT6835_DISP_TDSHP0_SOUT_SEL;
		value = MT6835_DISP_TDSHP0_SOUT_TO_DISP_COLOR0_SEL;
	} else if (cur == DDP_COMPONENT_CCORR0 &&
		next == DDP_COMPONENT_AAL0) {
		*addr = MT6835_DISP_CCORR0_SOUT_SEL;
		value = MT6835_DISP_CCORR0_SOUT_TO_DISP_MDP_AAL0_SEL;
	} else if (cur == DDP_COMPONENT_PQ0_VIRTUAL &&
		next == DDP_COMPONENT_MAIN0_VIRTUAL) {
		*addr = MT6835_DISP_PQ0_SOUT_SEL;
		value = MT6835_DISP_PQ0_SOUT_TO_DISP_MAIN0_SEL;
	} else if (cur == DDP_COMPONENT_MAIN0_VIRTUAL &&
		next == DDP_COMPONENT_MAIN0_TX_VIRTUAL0) {
		*addr = MT6835_DISP_MAIN0_SOUT_SEL;
		value = MT6835_DISP_MAIN0_SOUT_TO_DISP_DSI0_SEL;
	} else if (cur == DDP_COMPONENT_MAIN0_TX_VIRTUAL0 &&
		next == DDP_COMPONENT_DSI0) {
		*addr = MT6835_DISP_MAIN0_TX_SOUT_SEL;
		value = MT6835_DISP_MAIN0_TX_SOUT_TO_DISP_DSI0;
	} else {
		value = -1;
	}

	return value;
}

int mtk_ddp_ovl_bg_blend_en_MT6835(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr)
{
	int value;

	/*OVL1_2L*/
	if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_OVL0) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL1_2L_GO_BG;
	} else if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL1_2L_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_RDMA0) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL1_2L_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL1_2L &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL1_2L_GO_BLEND;
	/*OVL0*/
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_OVL1_2L) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL0_GO_BG;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_RSZ0) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL0_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL0_GO_BLEND;
	} else if (cur == DDP_COMPONENT_OVL0 &&
		next == DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL) {
		*addr = MT6835_MMSYS_OVL_CON;
		value = MT6835_DISP_OVL0_GO_BLEND;
	} else {
		value = -1;
	}


	return value;
}

void mutex_dump_analysis_mt6835(struct mtk_disp_mutex *mutex)
{
#define LEN 512
	struct mtk_ddp *ddp =
		container_of(mutex, struct mtk_ddp, mutex[mutex->id]);
	int i = 0;
	int j = 0;
	char mutex_module[LEN] = {'\0'};
	int n = 0;
	unsigned int val;

	DDPDUMP("== DISP Mutex Analysis ==\n");
	for (i = 0; i < 5; i++) {
		unsigned int mod0, mod1;

		n = 0;
		if (readl_relaxed(ddp->regs +
				  DISP_REG_MUTEX_MOD(ddp->data, i)) == 0)
			continue;

		val = readl_relaxed(ddp->regs +
				    DISP_REG_MUTEX_SOF(ddp->data, i));

		n += snprintf(mutex_module + n, LEN - n,
				"MUTEX%d:SOF=%s,EOF=%s,WAIT=%d,module=(", i,
			      mtk_ddp_get_mutex_sof_name(
				      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF, val)),
			      mtk_ddp_get_mutex_sof_name(
				      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_EOF, val)),
			      REG_FLD_VAL_GET(SOF_FLD_MUTEX0_SOF_WAIT, val));

		mod0 = readl_relaxed(ddp->regs +
			DISP_REG_MUTEX_MOD(ddp->data, i));
		for (j = 0; j < 32; j++) {
			if ((mod0 & (1 << j))) {
				n += snprintf(mutex_module + n, LEN - n, "%s,",
					ddp_get_mutex_module0_name_mt6835(j));
			}
		}

		mod1 = readl_relaxed(ddp->regs +
			DISP_REG_MUTEX_MOD(ddp->data, i));
		for (j = 0; j < 32; j++) {
			if ((mod1 & (1 << j))) {
				n += snprintf(mutex_module + n, LEN - n, "%s,",
					ddp_get_mutex_module1_name_mt6835(j));
			}
		}
		DDPDUMP("%s)\n", mutex_module);
	}
}

void mmsys_config_dump_analysis_mt6835(void __iomem *config_regs)
{
#define LEN 512
	unsigned int i = 0;
	unsigned int reg = 0;
	char clock_on[LEN] = {'\0'};
	char *name;
	int n = 0;

	unsigned int valid0 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_0);
	unsigned int valid1 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_1);
	unsigned int valid2 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_2);
	unsigned int valid3 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_3);
	unsigned int valid4 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_4);
	unsigned int valid5 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_VALID_5);

	unsigned int ready0 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_0);
	unsigned int ready1 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_1);
	unsigned int ready2 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_2);
	unsigned int ready3 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_3);
	unsigned int ready4 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_4);
	unsigned int ready5 =
		readl_relaxed(config_regs + MT6835_DISP_REG_CONFIG_DL_READY_5);

	unsigned int greq0 =
		readl_relaxed(config_regs +
				MT6835_DISP_REG_CONFIG_SMI_LARB0_GREQ);
	unsigned int greq1 =
		readl_relaxed(config_regs +
				MT6835_DISP_REG_CONFIG_SMI_LARB1_GREQ);

	DDPDUMP("== DISP MMSYS_CONFIG ANALYSIS ==\n");
	reg = readl_relaxed(config_regs + DISP_REG_CONFIG_MMSYS_CG_CON0_MT6835);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_0_mt6835(i);
			if (name)
				strncat(clock_on, name, (sizeof(clock_on) -
							 strlen(clock_on) - 1));
		}
	}

	reg = readl_relaxed(config_regs + DISP_REG_CONFIG_MMSYS_CG_CON1_MT6835);
	for (i = 0; i < 32; i++) {
		if ((reg & (1 << i)) == 0) {
			name = ddp_clock_1_mt6835(i);
			if (name)
				strncat(clock_on, name, (sizeof(clock_on) -
							 strlen(clock_on) - 1));
		}
	}

	DDPDUMP("clock on modules:%s\n", clock_on);

	DDPDUMP("va0=0x%x,va1=0x%x,va2=0x%x,va3=0x%x,va4=0x%x,va5=0x%x\n",
		valid0, valid1,	valid2, valid3, valid4, valid5);
	DDPDUMP("rd0=0x%x,rd1=0x%x,rd2=0x%x,rd3=0x%x,rd4=0x%x,rd5=0x%x\n",
		ready0, ready1, ready2, ready3, ready4, ready5);
	DDPDUMP("greq0=0x%x greq1=0x%x\n", greq0, greq1);
	for (i = 0; i < 32; i++) {
		name = ddp_signal_0_mt6835(i);
		if (!name)
			continue;

		n = 0;
		if ((valid0 & (1 << i)))
			n += snprintf(clock_on + n, LEN - n, "%s,", "v");
		else
			n += snprintf(clock_on + n, LEN - n, "%s,", "n");

		if ((ready0 & (1 << i)))
			n += snprintf(clock_on + n, LEN - n, "%s,", "r");
		else
			n += snprintf(clock_on + n, LEN - n, "%s,", "n");

		n += snprintf(clock_on + n, LEN - n, "%s,", name);

		DDPDUMP("%s\n", clock_on);
	}

	for (i = 0; i < 32; i++) {
		name = ddp_signal_1_mt6835(i);
		if (!name)
			continue;

		n = 0;
		if ((valid1 & (1 << i)))
			n += snprintf(clock_on + n, LEN - n, "%s,", "v");
		else
			n += snprintf(clock_on + n, LEN - n, "%s,", "n");

		if ((ready1 & (1 << i)))
			n += snprintf(clock_on + n, LEN - n, "%s,", "r");
		else
			n += snprintf(clock_on + n, LEN - n, "%s,", "n");

		n += snprintf(clock_on + n, LEN - n, "%s,", name);

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
			name = ddp_greq_name_larb0_mt6835(i);
			if (!name)
				continue;
			strncat(clock_on, name,
				(sizeof(clock_on) -
				strlen(clock_on) - 1));
		}
	}

	for (i = 0; i < 32; i++) {
		if (greq1 & (1 << i)) {
			name = ddp_greq_name_larb1_mt6835(i);
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
#define MT6835_HRT_URGENT_CTL_SEL_ALL             REG_FLD_MSB_LSB(7, 0)
	#define MT6835_HRT_URGENT_CTL_SEL_RDMA0       REG_FLD_MSB_LSB(0, 0)
	#define MT6835_HRT_URGENT_CTL_SEL_WDMA0       REG_FLD_MSB_LSB(2, 2)
	#define MT6835_HRT_URGENT_CTL_SEL_DSI0        REG_FLD_MSB_LSB(5, 5)

#define MT6835_HRT_URGENT_CTL_VAL_ALL             REG_FLD_MSB_LSB(15, 8)
	#define MT6835_HRT_URGENT_CTL_VAL_RDMA0       REG_FLD_MSB_LSB(8, 8)
	#define MT6835_HRT_URGENT_CTL_VAL_WDMA0       REG_FLD_MSB_LSB(10, 10)
	#define MT6835_HRT_URGENT_CTL_VAL_DSI0        REG_FLD_MSB_LSB(13, 13)

#define MT6835_FLD_OVL0_RDMA_ULTRA_SEL            REG_FLD_MSB_LSB(5, 2)
#define MT6835_FLD_OVL1_2L_RDMA_ULTRA_SEL         REG_FLD_MSB_LSB(13, 10)

static void mt6835_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
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
					0, SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, SODI_REQ_VAL_ALL);

		/* apply sodi hrt with rdma fifo*/
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_HRT_FIFO_SEL_DISP0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_HRT_FIFO_SEL_DISP0_CG_MODE);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0xFF, MT6835_HRT_URGENT_CTL_SEL_ALL);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, MT6835_HRT_URGENT_CTL_VAL_ALL);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!en),
					SODI_REQ_SEL_RDMA0_CG_MODE);
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
					MT6835_HRT_URGENT_CTL_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
					MT6835_HRT_URGENT_CTL_SEL_WDMA0);
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

		/* enable ultra signal from rdma to ovl0 and ovl1_2l */
		v = readl(priv->config_regs +  DISP_REG_CONFIG_MMSYS_MISC);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6835_FLD_OVL0_RDMA_ULTRA_SEL);
		v = (v & ~ultra_ovl_mask) | (ultra_ovl_val & ultra_ovl_mask);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6835_FLD_OVL1_2L_RDMA_ULTRA_SEL);
		v = (v & ~ultra_ovl_mask) | (ultra_ovl_val & ultra_ovl_mask);
		writel_relaxed(v, priv->config_regs +  DISP_REG_CONFIG_MMSYS_MISC);
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);

		/* enable ultra signal from rdma to ovl0 and ovl1_2l*/
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6835_FLD_OVL0_RDMA_ULTRA_SEL);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa + DISP_REG_CONFIG_MMSYS_MISC,
			       ultra_ovl_val, ultra_ovl_mask);
		SET_VAL_MASK(ultra_ovl_val, ultra_ovl_mask, 0,
			MT6835_FLD_OVL1_2L_RDMA_ULTRA_SEL);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa + DISP_REG_CONFIG_MMSYS_MISC,
			       ultra_ovl_val, ultra_ovl_mask);
	}
}

// drv
static const enum mtk_ddp_comp_id mt6835_mtk_ddp_main[] = {
#ifndef MTK_DRM_BRINGUP_STAGE
	DDP_COMPONENT_OVL1_2L,
#endif
	DDP_COMPONENT_OVL0, DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL,
	DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP_VIRTUAL0, DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_SPR0_VIRTUAL,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_MAIN0_TX_VIRTUAL0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6835_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

static const struct mtk_addon_module_data mt6835_addon_rsz_data[] = {
	{DISP_RSZ, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data mt6835_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_DITHER0},
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6835_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6835_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6835_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6835_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
		},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(mt6835_addon_rsz_data),
				.module_data = mt6835_addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
		},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(mt6835_addon_rsz_data),
				.module_data = mt6835_addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
		},
#ifdef IF_ZERO
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6835_addon_wdma0_data),
				.module_data = mt6835_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
		},
#endif
};

static const struct mtk_addon_scenario_data mt6835_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_crtc_path_data mt6835_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6835_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6835_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = NULL,
	.path_len[DDP_MINOR][1] = 0,
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6835_addon_main,
};

static const struct mtk_crtc_path_data mt6835_mtk_ext_path_data = {
	.is_fake_path = true,
	.path[DDP_MAJOR][0] = mt6835_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6835_mtk_ddp_third),
	.addon_data = mt6835_addon_ext,
};

static const struct mtk_crtc_path_data mt6835_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6835_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6835_mtk_ddp_third),
	.addon_data = mt6835_addon_ext,
};

static const struct mtk_session_mode_tb mt6835_mode_tb[MTK_DRM_SESSION_NUM] = {
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

static const struct mtk_fake_eng_reg mt6835_fake_eng_reg[] = {
	{.CG_idx = 0, .CG_bit = 3, .share_port = false},
	{.CG_idx = 0, .CG_bit = 6, .share_port = false},
};

static const struct mtk_fake_eng_data mt6835_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6835_fake_eng_reg),
	.fake_eng_reg = mt6835_fake_eng_reg,
};

const struct mtk_mmsys_driver_data mt6835_mmsys_driver_data = {
	.main_path_data = &mt6835_mtk_main_path_data,
	.ext_path_data = &mt6835_mtk_ext_path_data,
	.third_path_data = &mt6835_mtk_third_path_data,
	.fake_eng_data = &mt6835_fake_eng_data,
	.mmsys_id = MMSYS_MT6835,
	.mode_tb = mt6835_mode_tb,
	.sodi_config = mt6835_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = true,
	.bypass_infra_ddr_control = true,
};

// mipi_tx
#define RG_DSI_PLL_SDM_SSC_EN_MT6835 BIT(1)

static int mtk_mipi_tx_pll_prepare_mt6835(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
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

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

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

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);
#endif
	return 0;
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6835(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
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
	/*set volate: cphy need 500mV*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
		FLD_RG_DSI_HSTX_LDO_REF_SEL, 0xD << 6);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
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

	tmp = _dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       RG_DSI_PLL_EN);

	usleep_range(50, 100);

#endif
	DDPDBG("%s-\n", __func__);

	return 0;
}

static void mtk_mipi_tx_pll_unprepare_mt6835(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D0_SW_CTL_EN, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D1_SW_CTL_EN, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D2_SW_CTL_EN, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D3_SW_CTL_EN, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_CK_SW_CTL_EN, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6835(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

const struct mtk_mipitx_data mt6835_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6835,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D3_SW_CTL_EN,
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
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6835,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6835,
	.dsi_get_pcw = _dsi_get_pcw,
	.dsi_get_data_rate = _dsi_get_data_rate,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
	.mipi_tx_ssc_en = mtk_mipi_tx_ssc_en_N6,
};

const struct mtk_mipitx_data mt6835_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
		.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6835,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D3_SW_CTL_EN,
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
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6835,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6835,
	.dsi_get_pcw = _dsi_get_pcw,
	.dsi_get_data_rate = _dsi_get_data_rate,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};


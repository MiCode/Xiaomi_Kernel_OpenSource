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

#include "cmdq_subsys_common.h"
#include "cmdq_helper_ext.h"

static struct cmdq_subsys_dts_name subsys[] = {
	[CMDQ_SUBSYS_G3D_CONFIG_BASE] = {
		.group = "MFG", .name = "g3d_config_base"},
	[CMDQ_SUBSYS_MMSYS_CONFIG] = {
		.group = "MMSYS", .name = "mmsys_config_base"},
	[CMDQ_SUBSYS_DISP_DITHER] = {
		.group = "MMSYS", .name = "disp_dither_base"},
	[CMDQ_SUBSYS_NA] = {.group = "MMSYS", .name = "mm_na_base"},
	[CMDQ_SUBSYS_IMGSYS] = {.group = "CAM", .name = "imgsys_base"},
	[CMDQ_SUBSYS_VDEC_GCON] = {.group = "VDEC", .name = "vdec_gcon_base"},
	[CMDQ_SUBSYS_VENC_GCON] = {.group = "VENC", .name = "venc_gcon_base"},
	[CMDQ_SUBSYS_CONN_PERIPHERALS] = {
		.group = "PERISYS", .name = "conn_peri_base"},

	[CMDQ_SUBSYS_TOPCKGEN] = {
		.group = "TOP_AO_3", .name = "topckgen_base"},
	[CMDQ_SUBSYS_KP] = {.group = "INFRA_AO", .name = "kp_base"},
	[CMDQ_SUBSYS_SCP_SRAM] = {
		.group = "INFRA_AO", .name = "scp_sram_base"},
	[CMDQ_SUBSYS_INFRA_NA3] = {.group = "NA", .name = "infra_na3_base"},
	[CMDQ_SUBSYS_INFRA_NA4] = {.group = "NA", .name = "infra_na4_base"},
	[CMDQ_SUBSYS_SCP] = {.group = "SCP", .name = "scp_base"},

	[CMDQ_SUBSYS_MCUCFG] = {.group = "INFRASYS", .name = "mcucfg_base"},
	[CMDQ_SUBSYS_GCPU] = {.group = "INFRASYS", .name = "gcpu_base"},
	[CMDQ_SUBSYS_USB0] = {.group = "PERISYS", .name = "usb0_base"},
	[CMDQ_SUBSYS_USB_SIF] = {.group = "PERISYS", .name = "usb_sif_base"},
	[CMDQ_SUBSYS_AUDIO] = {.group = "PERISYS", .name = "audio_base"},
	[CMDQ_SUBSYS_MSDC0] = {.group = "PERISYS", .name = "msdc0_base"},
	[CMDQ_SUBSYS_MSDC1] = {.group = "PERISYS", .name = "msdc1_base"},
	[CMDQ_SUBSYS_MSDC2] = {.group = "PERISYS", .name = "msdc2_base"},
	[CMDQ_SUBSYS_MSDC3] = {.group = "PERISYS", .name = "msdc3_base"},
	[CMDQ_SUBSYS_AP_DMA] = {.group = "INFRASYS", .name = "ap_dma_base"},
	[CMDQ_SUBSYS_GCE] = {.group = "GCE", .name = "gce_base"},

	[CMDQ_SUBSYS_VDEC] = {.group = "VDEC", .name = "vdec_base"},
	[CMDQ_SUBSYS_VDEC1] = {.group = "VDEC", .name = "vdec1_base"},
	[CMDQ_SUBSYS_VDEC2] = {.group = "VDEC", .name = "vdec2_base"},
	[CMDQ_SUBSYS_VDEC3] = {.group = "VDEC", .name = "vdec3_base"},
	[CMDQ_SUBSYS_CAMSYS] = {.group = "CAMSYS", .name = "camsys_base"},
	[CMDQ_SUBSYS_CAMSYS1] = {.group = "CAMSYS", .name = "camsys1_base"},
	[CMDQ_SUBSYS_CAMSYS2] = {.group = "CAMSYS", .name = "camsys2_base"},
	[CMDQ_SUBSYS_CAMSYS3] = {.group = "CAMSYS", .name = "camsys3_base"},
	[CMDQ_SUBSYS_IMGSYS1] = {.group = "IMGSYS", .name = "imgsys1_base"},

	[CMDQ_SUBSYS_SMI_LAB1] = {
		.group = "SMI_LAB1", .name = "smi_larb1_base"},

	/* Special subsys */
	[CMDQ_SUBSYS_PWM_SW] = {.group = "SPECIAL", .name = "pwm_sw_base"},
	[CMDQ_SUBSYS_PWM1_SW] = {.group = "SPECIAL", .name = "pwm1_sw_base"},
	[CMDQ_SUBSYS_DIP_A0_SW] = {
		.group = "SPECIAL", .name = "dip_a0_sw_base"},
	[CMDQ_SUBSYS_MIPITX0] = {.group = "SPECIAL", .name = "mipitx0_base"},
	[CMDQ_SUBSYS_MIPITX1] = {.group = "SPECIAL", .name = "mipitx1_base"},
	[CMDQ_SUBSYS_VENC] = {.group = "SPECIAL", .name = "venc_base"},
};

struct cmdq_subsys_dts_name *cmdq_subsys_get_dts(void)
{
	return subsys;
}

u32 cmdq_subsys_get_size(void)
{
	return ARRAY_SIZE(subsys);
}


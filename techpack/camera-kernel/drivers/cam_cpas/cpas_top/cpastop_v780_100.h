/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CPASTOP_V780_100_H_
#define _CPASTOP_V780_100_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v780_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x240, /* CAM_NOC_SBM_FAULTINEN0_LOW */
		.value = 0x2 |    /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x04 |     /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x08 |     /* SBM_FAULTINEN0_LOW_PORT3_MASK */
			0x10 |    /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			0x20 |    /* SBM_FAULTINEN0_LOW_PORT5_MASK */
			(TEST_IRQ_ENABLE ?
			0x80 :    /* SBM_FAULTINEN0_LOW_PORT7_MASK */
			0x0),
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0x248, /* CAM_NOC_SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0x280, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x5 : 0x1,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v780_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = false,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x8, /* CAM_NOC_ERL_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x10, /* CAM_NOC_ERL_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x18, /* CAM_NOC_ERL_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x20, /* SBM_FAULTINSTATUS0_LOW_PORT5_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x77A0, /* IFE_UBWC : RT_1_NIU_ENCERREN_LOW */
			.value = 0xF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x7790, /* IFE_UBWC : RT_1_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x7798, /* IFE_UBWC : RT_1_NIU_ENCERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_BPS_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT1_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x6BA0, /* BPS_WR : NRT_2_NIU_ENCERREN_LOW */
			.value = 0XF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x6B90, /* BPS_WR : NRT_2_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x6B98, /* BPS_WR : NRT_2_NIU_ENCERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x4, /* SBM_FAULTINSTATUS0_LOW_PORT2_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x6F20, /* IPE_0_RD : NRT_4_NIU_DECERREN_LOW */
			.value = 0xFF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x6F10, /* IPE_0_RD : NRT_4_NIU_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x6F18, /* IPE_0_RD : NRT_4_NIU_DECERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE1_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x8, /* SBM_FAULTINSTATUS0_LOW_PORT3_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x7120, /* IPE_1_RD : NRT_5_NIU_DECERREN_LOW */
			.value = 0XFF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x7110, /* IPE_1_RD : NRT_5_NIU_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x7118, /* IPE_1_RD : NRT_5_NIU_DECERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x10, /* SBM_FAULTINSTATUS0_LOW_PORT4_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x73A0, /* IPE_WR : NRT_6_NIU_ENCERREN_LOW */
			.value = 0XF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x7390, /* IPE_WR : NRT_6_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x7398, /* IPE_WR : NRT_6_NIU_ENCERRCLR_LOW */
			.value = 0x1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT,
		.enable = false,
		.sbm_port = 0x40, /* SBM_FAULTINSTATUS0_LOW_PORT6_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x288, /* CAM_NOC_SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x290, /* CAM_NOC_SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED1,
		.enable = false,
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED2,
		.enable = false,
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_CAMNOC_TEST,
		.enable = TEST_IRQ_ENABLE ? true : false,
		.sbm_port = 0x80, /* SBM_FAULTINSTATUS0_LOW_PORT7_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x288, /* CAM_NOC_SBM_FLAGOUTSET0_LOW */
			.value = 0x5,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x290, /* CAM_NOC_SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		},
	},
};

static struct cam_camnoc_specific
	cam_cpas_v780_100_camnoc_specific[] = {
	/* RT ports */
	{
		.port_name = "RT0-SFE_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7430, /* SFE_RD : NOC_RT_0_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7434, /* SFE_RD : NOC_RT_0_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7438, /* SFE_RD : NOC_RT_0_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7440, /* SFE_RD : NOC_RT_0_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7448, /* SFE_RD : NOC_RT_0_NIU_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8188, /* SFE_RD : NOC_RT_0_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5388, /* SFE_RD : NOC_RT_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x53A0, /* SFE_RD : NOC_RT_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x53A4, /* SFE_RD : NOC_RT_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_name = "RT1-IFE_UBWC_LINEAR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7630, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7634, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7638, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_URGENCY_LOW */
			.value = 0x1B30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7640, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_DANGERLUT_LOW */
			.value = 0xffffff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7648, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_SAFELUT_LOW */
			.value = 0x000f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8208, /* IFE_UBWC_LINEAR : NOC_RT_1_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5408, /* IFE_UBWC_LINEAR : NOC_RT_1_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5420, /* IFE_UBWC_LINEAR : NOC_RT_1_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5424, /* IFE_UBWC_LINEAR : NOC_RT_1_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7620, /* IFE_UBWC_LINEAR : NOC_RT_1_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "RT2-IFE_STATS",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7830, /* IFE_STATS : NOC_RT_2_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7834, /* IFE_STATS : NOC_RT_2_NIU_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7838, /* IFE_STATS : NOC_RT_2_NIU_URGENCY_LOW */
			.value = 0x1B30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7840, /* IFE_STATS : NOC_RT_2_NIU_DANGERLUT_LOW */
			.value = 0xffffff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7848, /* IFE_STATS : NOC_RT_2_NIU_SAFELUT_LOW */
			.value = 0x000f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8288, /* IFE_STATS : NOC_RT_2_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5488, /* IFE_STATS : NOC_RT_2_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x54A0, /* IFE_STATS : NOC_RT_2_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x54A4, /* IFE_STATS : NOC_RT_2_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7820, /* IFE_STATS : NOC_RT_2_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "RT3-IFE_PDAF_IFELITE",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7A30, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7A34, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7A38, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_URGENCY_LOW */
			.value = 0x1B30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7A40, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_DANGERLUT_LOW */
			.value = 0xffffff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7A48, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_SAFELUT_LOW */
			.value = 0x000f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8308, /* IFE_PDAF_IFELITE : NOC_RT_3_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5508, /* IFE_PDAF_IFELITE : NOC_RT_3_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5520, /* IFE_PDAF_IFELITE : NOC_RT_3_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5524, /* IFE_PDAF_IFELITE : NOC_RT_3_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7A20, /* IFE_PDAF_IFELITE : NOC_RT_3_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "RT4-IFE_RDI_SFE",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7C30, /* IFE_RDI_SFE : NOC_RT_4_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7C34, /* IFE_RDI_SFE : NOC_RT_4_NIU_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7C38, /* IFE_RDI_SFE : NOC_RT_4_NIU_URGENCY_LOW */
			.value = 0x1B30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7C40, /* IFE_RDI_SFE : NOC_RT_4_NIU_DANGERLUT_LOW */
			.value = 0xffffff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7C48, /* IFE_RDI_SFE : NOC_RT_4_NIU_SAFELUT_LOW */
			.value = 0x000f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8388, /* IFE_RDI_SFE : NOC_RT_4_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5588, /* IFE_RDI_SFE : NOC_RT_4_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x55A0, /* IFE_RDI_SFE : NOC_RT_4_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x55A4, /* IFE_RDI_SFE : NOC_RT_4_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7C20, /* IFE_RDI_SFE : NOC_RT_4_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	/* NRT ports */
	{
		.port_name = "NRT0-CDM",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6630, /* CDM : NOC_NRT_0_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6634, /* CDM : NOC_NRT_0_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6638, /* CDM : NOC_NRT_0_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6640, /* CDM : NOC_NRT_0_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6648, /* CDM : NOC_NRT_0_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7E08, /* CDM : NOC_NRT_0_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5008, /* CDM : NOC_NRT_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5020, /* CDM : NOC_NRT_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5024, /* CDM : NOC_NRT_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_name = "NRT1-JPEG_RD_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6830, /* JPEG : NOC_NRT_1_NIU_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6834, /* JPEG : NOC_NRT_1_NIU_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6838, /* JPEG : NOC_NRT_1_NIU_URGENCY_LOW */
			.value = 0x22,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6840, /* JPEG : NOC_NRT_1_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6848, /* JPEG : NOC_NRT_1_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7E88, /* JPEG : NOC_NRT_1_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5088, /* JPEG : NOC_NRT_1_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x50A0, /* JPEG : NOC_NRT_1_QOSGEN_SHAPING_LOW */
			.value = 0x10101010,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x50A4, /* JPEG : NOC_NRT_1_QOSGEN_SHAPING_HIGH */
			.value = 0x10101010,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6820, /* JPEG : NOC_NRT_1_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "NRT2-BPS_CRE_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A30, /* BPS_CRE_WR : NOC_NRT_2_NIU_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A34, /* BPS_CRE_WR : NOC_NRT_2_NIU_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A38, /* BPS_CRE_WR : NOC_NRT_2_NIU_URGENCY_LOW */
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A40, /* BPS_CRE_WR : NOC_NRT_2_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A48, /* BPS_CRE_WR : NOC_NRT_2_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7F08, /* BPS_CRE_WR : NOC_NRT_2_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5108, /* BPS_CRE_WR : NOC_NRT_2_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5120, /* BPS_CRE_WR : NOC_NRT_2_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5124, /* BPS_CRE_WR : NOC_NRT_2_BPS_WR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6A20, /* BPS_CRE_WR : NOC_NRT_2_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "NRT3-BPS_CRE_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C30, /* BPS_CRE_RD : NOC_NRT_3_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C34, /* BPS_CRE_RD : NOC_NRT_3_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C38, /* BPS_CRE_RD : NOC_NRT_3_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C40, /* BPS_CRE_RD : NOC_NRT_3_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C48, /* BPS_CRE_RD : NOC_NRT_3_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7F88, /* BPS_CRE_RD : NOC_NRT_3_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5188, /* BPS_CRE_RD : NOC_NRT_3_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A0, /* BPS_CRE_RD : NOC_NRT_3_QOSGEN_SHAPING_LOW */
			.value = 0x14141414,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A4, /* BPS_CRE_RD : NOC_NRT_3_QOSGEN_SHAPING_HIGH */
			.value = 0x14141414,
		},
	},
	{
		.port_name = "NRT4-IPE_0_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E30, /* IPE_0_RD : NOC_NRT_4_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E34, /* IPE_0_RD : NOC_NRT_4_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E38, /* IPE_0_RD : NOC_NRT_4_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E40, /* IPE_0_RD : NOC_NRT_4_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E48, /* IPE_0_RD : NOC_NRT_4_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6F08, /* IPE_0_RD : NOC_NRT_4_NIU_DECCTL_LOW */
			.value = 1,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8008, /* IPE_0_RD : NOC_NRT_4_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5208, /* IPE_0_RD : NOC_NRT_4_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5220, /* IPE_0_RD : NOC_NRT_4_QOSGEN_SHAPING_LOW */
			.value = 0x29292929,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5224, /* IPE_0_RD : NOC_NRT_4_QOSGEN_SHAPING_HIGH */
			.value = 0x29292929,
		},
	},
	{
		.port_name = "NRT5-IPE_1_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7030, /* IPE_1_RD : NOC_NRT_5_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7034, /* IPE_1_RD : NOC_NRT_5_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7038, /* IPE_1_RD : NOC_NRT_5_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7040, /* IPE_1_RD : NOC_NRT_5_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7048, /* IPE_1_RD : NOC_NRT_5_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8088, /* IPE_1_RD : NOC_NRT_5_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5288, /* IPE_1_RD : NOC_NRT_5_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x52A0, /* IPE_1_RD : NOC_NRT_5_QOSGEN_SHAPING_LOW */
			.value = 0x29292929,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x52A4, /* IPE_1_RD : NOC_NRT_5_QOSGEN_SHAPING_HIGH */
			.value = 0x29292929,
		},
	},
	{
		.port_name = "NRT6-IPE_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7230, /* IPE_WR : NOC_NRT_6_NIU_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7234, /* IPE_WR : NOC_NRT_6_NIU_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7238, /* IPE_WR : NOC_NRT_6_NIU_URGENCY_LOW */
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7240, /* IPE_WR : NOC_NRT_6_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7248, /* IPE_WR : NOC_NRT_6_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.dynattr_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8108, /* IPE_WR : NOC_NRT_6_DYNATTR_MAINCTL */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5308, /* IPE_WR : NOC_NRT_6_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5320, /* IPE_WR : NOC_NRT_6_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5324, /* IPE_WR : NOC_NRT_6_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7220, /* IPE_WR : NOC_NRT_6_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_name = "ICP_RD_WR",
		.enable = true,
		.dynattr_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x8408, /* ICP_RD_WR : NOC_XM_ICP_DYNATTR_MAINCTL */
			.value = 0x10,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5608, /* ICP_RD_WR : NOC_XM_ICP_QOSGEN_MAINCTL */
			.value = 0x40,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5620, /* ICP_RD_WR : NOC_XM_ICP_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5624, /* ICP_RD_WR : NOC_XM_ICP_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
};

static struct cam_camnoc_err_logger_info cam780_cpas100_err_logger_offsets = {
	.mainctrl     =  0x08, /* NOC_ERL_MAINCTL_LOW */
	.errvld       =  0x10, /* NOC_ERL_ERRVLD_LOW */
	.errlog0_low  =  0x20, /* NOC_ERL_ERRLOG0_LOW */
	.errlog0_high =  0x24, /* NOC_ERL_ERRLOG0_HIGH */
	.errlog1_low  =  0x28, /* NOC_ERL_ERRLOG1_LOW */
	.errlog1_high =  0x2C, /* NOC_ERL_ERRLOG1_HIGH */
	.errlog2_low  =  0x30, /* NOC_ERL_ERRLOG2_LOW */
	.errlog2_high =  0x34, /* NOC_ERL_ERRLOG2_HIGH */
	.errlog3_low  =  0x38, /* NOC_ERL_ERRLOG3_LOW */
	.errlog3_high =  0x3C, /* NOC_ERL_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam780_cpas100_errata_wa_list = {
	.camnoc_flush_slave_pending_trans = {
		.enable = false,
		.data.reg_info = {
			.access_type = CAM_REG_TYPE_READ,
			.offset = 0x300, /* sbm_SenseIn0_Low */
			.mask = 0xE0000, /* Bits 17, 18, 19 */
			.value = 0, /* expected to be 0 */
		},
	},
};

static struct cam_camnoc_info cam780_cpas100_camnoc_info = {
	.specific = &cam_cpas_v780_100_camnoc_specific[0],
	.specific_size = ARRAY_SIZE(cam_cpas_v780_100_camnoc_specific),
	.irq_sbm = &cam_cpas_v780_100_irq_sbm,
	.irq_err = &cam_cpas_v780_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v780_100_irq_err),
	.err_logger = &cam780_cpas100_err_logger_offsets,
	.errata_wa_list = &cam780_cpas100_errata_wa_list,
};

static struct cam_cpas_camnoc_qchannel cam780_cpas100_qchannel_info = {
	.qchannel_ctrl   = 0x5C,
	.qchannel_status = 0x60,
};
#endif /* _CPASTOP_V780_100_H_ */


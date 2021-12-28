/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CPASTOP_V570_100_H_
#define _CPASTOP_V570_100_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v570_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x3840, /* SBM_FAULTINEN0_LOW */
		.value = 0x2 | /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x4 | /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x8 | /* SBM_FAULTINEN0_LOW_PORT3_MASK */
			0x10 | /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			0x1000 | /* SBM_FAULTINEN0_LOW_PORT12_MASK */
			(TEST_IRQ_ENABLE ?
			0x40 : /* SBM_FAULTINEN0_LOW_PORT6_MASK */
			0x0),
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0x3848, /* SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0x3880, /* SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x5 : 0x1,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v570_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = false,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x7008, /* ERL_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x7010, /* ERL_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x7018, /* ERL_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT1_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x1BA0, /* IFE_UBWC_STATS_0_ENCERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x1B90,
			/* IFE_UBWC_STATS_0_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x1B98, /* IFE_UBWC_STATS_0_ENCERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x4, /* SBM_FAULTINSTATUS0_LOW_PORT2_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x2520, /* IPE1_BPS_RD_DECERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2510, /* IPE1_BPS_RD_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x2518, /* IPE1_BPS_RD_DECERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x8, /* SBM_FAULTINSTATUS0_LOW_PORT3_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x1F20, /* IPE0_RD_DECERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x1F10, /* IPE0_RD_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x1F18, /* IPE0_RD_DECERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x10, /* SBM_FAULTINSTATUS0_LOW_PORT4_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x29A0, /* IPE_BPS_WR_ENCERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2990,
			/* IPE_BPS_WR_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x2998, /* IPE_BPS_WR_ENCERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT,
		.enable = false,
		.sbm_port = 0x20, /* SBM_FAULTINSTATUS0_LOW_PORT5_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x3888, /* SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x3890, /* SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false,
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
		.sbm_port = 0x40, /* SBM_FAULTINSTATUS0_LOW_PORT6_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x3888, /* SBM_FLAGOUTSET0_LOW */
			.value = 0x5,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x3890, /* SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false,
		},
	},
};

static struct cam_camnoc_specific
	cam_cpas_v570_100_camnoc_specific[] = {
	{
		.port_type = CAM_CAMNOC_CDM,
		.port_name = "CDM",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6030, /* CDM_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6034, /* CDM_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6038, /* CDM_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6040, /* CDM_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6048, /* CDM_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5008, /* CDM_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5020, /* CDM_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5024, /* CDM_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_LINEAR,
		.port_name = "IFE_LINEAR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6430, /* IFE_LINEAR_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6434, /* IFE_LINEAR_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6438, /* IFE_LINEAR_URGENCY_LOW */
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6440, /* IFE_LINEAR_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6448, /* IFE_LINEAR_SAFELUT_LOW */
			.value = 0x000F,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5108, /* IFE_LINEAR_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5120, /* IFE_LINEAR_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5124, /* IFE_LINEAR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6420, /* IFE_LINEAR_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_RDI_RD,
		.port_name = "IFE_RDI_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6630, /* IFE_RDI_RD_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6634, /* IFE_RDI_RD_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6638, /* IFE_RDI_RD_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6640, /* IFE_RDI_RD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6648, /* IFE_RDI_RD_SAFELUT_LOW */
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
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5188, /* IFE_RDI_RD_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A0, /* IFE_RDI_RD_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A4, /* IFE_RDI_RD_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_RDI_WR,
		.port_name = "IFE_RDI_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6830, /* IFE_RDI_WR_0_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6834, /* IFE_RDI_WR_0_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6838, /* IFE_RDI_WR_0_URGENCY_LOW */
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6840, /* IFE_RDI_WR_0_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6848, /* IFE_RDI_WR_0_SAFELUT_LOW */
			.value = 0x000F,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5208, /* IFE_RDI_WR_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5220, /* IFE_RDI_WR_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5224, /* IFE_RDI_WR_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6820, /* IFE_RDI_WR_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_UBWC_STATS,
		.port_name = "IFE_UBWC_STATS",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6230, /* IFE_UBWC_STATS_0_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6234,
			/* IFE_UBWC_STATS_0_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6238, /* IFE_UBWC_STATS_0_URGENCY_LOW */
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6240, /* IFE_UBWC_STATS_0_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x6248, /* IFE_UBWC_STATS_0_SAFELUT_LOW */
			.value = 0x000F,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x1B88, /* IFE_UBWC_STATS_0_ENCCTL_LOW */
			.value = 1,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5088, /* IFE_UBWC_STATS_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x50A0,
			/* IFE_UBWC_STATS_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x50A4,
			/* IFE_UBWC_STATS_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6220, /* IFE_UBWC_STATS_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE0_RD,
		.port_name = "IPE0_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E30, /* IPE0_RD_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E34, /* IPE0_RD_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E38, /* IPE0_RD_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E40, /* IPE0_RD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6E48, /* IPE0_RD_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x1F08, /* IPE0_RD_DECCTL_LOW */
			.value = 1,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5388, /* IPE0_RD_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x53A0, /* IPE0_RD_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x53A4, /* IPE0_RD_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE1_BPS_RD,
		.port_name = "IPE1_BPS_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A30, /* IPE1_BPS_RD_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A34, /* IPE1_BPS_RD_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A38, /* IPE1_BPS_RD_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A40, /* IPE1_BPS_RD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6A48, /* IPE1_BPS_RD_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2508, /* IPE1_BPS_RD_DECCTL_LOW */
			.value = 1,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5288, /* IPE1_BPS_RD_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		//  TITAN_A_CAMNOC_cam_noc_amm_nrt_niu_0_qosgen_Shaping_Low
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x52A0, /* IPE1_BPS_RD_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x52A4, /* IPE1_BPS_RD_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_BPS_WR,
		.port_name = "IPE_BPS_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C30, /* IPE_BPS_WR_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C34, /* IPE_BPS_WR_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C38, /* IPE_BPS_WR_URGENCY_LOW */
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C40, /* IPE_BPS_WR_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x6C48, /* IPE_BPS_WR_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2988, /* IPE_BPS_WR_ENCCTL_LOW */
			.value = 1,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5308, /* IPE_BPS_WR_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5320, /* IPE_BPS_WR_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5324, /* IPE_BPS_WR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x6C20, /* IFE_IPE_BPS_WR_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_JPEG,
		.port_name = "JPEG",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7030, /* JPEG_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7034, /* JPEG_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7038, /* JPEG_URGENCY_LOW */
			.value = 0x22,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7040, /* JPEG_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7048, /* JPEG_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5408, /* JPEG_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5420, /* JPEG_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5424, /* JPEG_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7020, /* IFE_JPEG_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_ICP,
		.port_name = "ICP",
		.enable = true,
		.flag_out_set0_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x7A88,
			.value = 0x100000,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5488, /* ICP_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x54A0, /* ICP_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x54A4, /* ICP_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
	},
};

static struct cam_camnoc_err_logger_info cam570_cpas100_err_logger_offsets = {
	.mainctrl     =  0x7008, /* ERRLOGGER_MAINCTL_LOW */
	.errvld       =  0x7010, /* ERRLOGGER_ERRVLD_LOW */
	.errlog0_low  =  0x7020, /* ERRLOGGER_ERRLOG0_LOW */
	.errlog0_high =  0x7024, /* ERRLOGGER_ERRLOG0_HIGH */
	.errlog1_low  =  0x7028, /* ERRLOGGER_ERRLOG1_LOW */
	.errlog1_high =  0x702c, /* ERRLOGGER_ERRLOG1_HIGH */
	.errlog2_low  =  0x7030, /* ERRLOGGER_ERRLOG2_LOW */
	.errlog2_high =  0x7034, /* ERRLOGGER_ERRLOG2_HIGH */
	.errlog3_low  =  0x7038, /* ERRLOGGER_ERRLOG3_LOW */
	.errlog3_high =  0x703c, /* ERRLOGGER_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam570_cpas100_errata_wa_list = {
	.camnoc_flush_slave_pending_trans = {
		.enable = false,
	},
};

static struct cam_camnoc_info cam570_cpas100_camnoc_info = {
	.specific = &cam_cpas_v570_100_camnoc_specific[0],
	.specific_size = ARRAY_SIZE(cam_cpas_v570_100_camnoc_specific),
	.irq_sbm = &cam_cpas_v570_100_irq_sbm,
	.irq_err = &cam_cpas_v570_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v570_100_irq_err),
	.err_logger = &cam570_cpas100_err_logger_offsets,
	.errata_wa_list = &cam570_cpas100_errata_wa_list,
};

static struct cam_cpas_camnoc_qchannel cam570_cpas100_qchannel_info = {
	.qchannel_ctrl   = 0x5C,
	.qchannel_status = 0x60,
};
#endif /* _CPASTOP_V570_100_H_ */

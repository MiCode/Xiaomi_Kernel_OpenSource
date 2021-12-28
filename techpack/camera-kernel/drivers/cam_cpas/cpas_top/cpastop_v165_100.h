/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

 #ifndef _CPASTOP_V165_100_H_
 #define _CPASTOP_V165_100_H_

 #define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v165_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x2240, /* SBM_FAULTINEN0_LOW */
		.value =  0x1 | /* SBM_FAULTINEN0_LOW_PORT0_MASK*/
			0x2 | /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x4 | /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x8 | /* SBM_FAULTINEN0_LOW_PORT3_MASK */
			0x10 | /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			(TEST_IRQ_ENABLE ?
			0x100 : /* SBM_FAULTINEN0_LOW_PORT8_MASK */
			0x0),
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0x2248, /* SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0x2280, /* SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x6 : 0x2,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v165_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = true,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x4F08, /* ERL_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x4F10, /* ERL_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x4F18, /* ERL_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT1_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x3BA0, /* SPECIFIC_IFE0_MAIN_ENCERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x3B90,
			/* SPECIFIC_IFE0_MAIN_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x3B98, /* SPECIFIC_IFE0_MAIN_ENCERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x4, /* SBM_FAULTINSTATUS0_LOW_PORT2_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x55A0, /* SPECIFIC_IFE1_WR_ENCERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x5590, /* SPECIFIC_IFE1_WR_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x5598, /* SPECIFIC_IFE1_WR_ENCERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x8, /* SBM_FAULTINSTATUS0_LOW_PORT3_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x2F20, /* SPECIFIC_IBL_RD_DECERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2F10, /* SPECIFIC_IBL_RD_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x2F18, /* SPECIFIC_IBL_RD_DECERRCLR_LOW */
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
			.offset = 0x2BA0, /* SPECIFIC_IBL_WR_ENCERREN_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2B90,
			/* SPECIFIC_IBL_WR_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x2B98, /* SPECIFIC_IBL_WR_ENCERRCLR_LOW */
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
			.offset = 0x2288, /* SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2290, /* SBM_FLAGOUTSTATUS0_LOW */
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
		.sbm_port = 0x40, /* SBM_FAULTINSTATUS0_LOW_PORT8_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x2288, /* SBM_FLAGOUTSET0_LOW */
			.value = 0x5,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x2290, /* SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false,
		},
	},
};

static struct cam_camnoc_specific
	cam_cpas_v165_100_camnoc_specific[] = {
	{
		.port_type = CAM_CAMNOC_CDM,
		.port_name = "CDM",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4230, /* CDM_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4234, /* CDM_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0, /* CDM_Urgency_Low */
			.offset = 0x4238,
			.mask = 0x7, /* SPECIFIC_CDM_URGENCY_LOW_READ_MASK */
			.shift = 0x0, /* SPECIFIC_CDM_URGENCY_LOW_READ_SHIFT */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4240, /* CDM_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4248, /* CDM_SAFELUT_LOW */
			.value = 0xFFFF,
		},
		.ubwc_ctl = {
			.enable = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE01234_RDI_WRITE,
		.port_name = "IFE01234_RDI_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3630, /* IFE01234_RDI_PRIORITYLUT_LOW */
			.value = 0x55554433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3634, /* IFE01234_RDI_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3638, /* IFE01234_RDI_LINEAR_URGENCY_LOW */
			.mask = 0x1FF0,
			.shift = 0x4,
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3640, /* IFE01234_RDI_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3648, /* IFE01234_RDI_SAFELUT_LOW */
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
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x3620, /* IFE01234_RDI_MAXWR_LOW */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4808, /* IFE01234_RDI_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4820, /* IFE01234_RDI_QOSGEN_SHAPING_LOW */
			.value = 0x07070707,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4824, /* IFE01234_RDI_QOSGEN_SHAPING_HIGH */
			.value = 0x07070707,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE01_NRDI_WRITE,
		.port_name = "IFE01_NRDI_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3A30, /* IFE01_NRDI_PRIORITYLUT_LOW */
			.value = 0x55554433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3A34, /* IFE01_NRDI_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x3A38, /* IFE01_NRDI_URGENCY_LOW */
			/* IFE01_NRDI_URGENCY_LOW_WRITE_MASK */
			.mask = 0x1FF0,
			/* IFE01_NRDI_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3A40, /* IFE01_NRDI_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3A48, /* IFE01_NRDI_SAFELUT_LOW */
			.value = 0xF,
		},
		/* no reg for this */
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3B88, /* IFE01_NRDI_ENCCTL_LOW */
			.value = 1,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x3A20, /* IFE01_MAXWR_LOW */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4708, /* IFE01_NRDI_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4720, /* IFE01_NRDI_QOSGEN_SHAPING_LOW */
			.value = 0x07070707,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4724, /* IFE01_NRDI_QOSGEN_SHAPING_HIGH */
			.value = 0x07070707,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE2_NRDI_WRITE,
		.port_name = "IFE2_NRDI_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5430, /* IFE2_NDRI_PRIORITYLUT_LOW */
			.value = 0x55554433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* IFE2_NRDI_PRIORITYLUT_HIGH */
			.offset = 0x5434,
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x5438, /* IFE2_NRDI_URGENCY_LOW */
			/* IFE2_NRDI_URGENCY_LOW_WRITE_MASK */
			.mask = 0x1FF0,
			/* IFE2_NRDI_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 0x1030,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x5440, /* IFE2_NRDI_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x5448, /* IFE2_NRDI_SAFELUT_LOW */
			.value = 0xF,
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
			.offset = 0x5588, /* IFE2_NRDI_ENCCTL_LOW */
			.value = 0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x5420, /* IFE2_MAXWR_LOW */
			.value = 0x0,
		},
		.qosgen_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5188, /* IFE2_NRDI_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A0, /* IFE2_NRDI_QOSGEN_SHAPING_LOW */
			.value = 0x07070707,
		},
		.qosgen_shaping_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x51A4, /* IFE2_NRDI_QOSGEN_SHAPING_HIGH */
			.value = 0x07070707,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_BPS_LRME_READ,
		.port_name = "IPE_BPS_LRME_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E30, /* IPE_BPS_LRME_RD_PRIORITYLUT_LOW */
			.value = 0x33333333, /*Value is 0 in excel sheet */
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E34, /* IPE_BPS_LRME_RD_PRIORITYLUT_HIGH */
			.value = 0x33333333, /*Value is 0 in excel sheet */
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x2E38, /* IPE_BPS_LRME_RD_URGENCY_LOW */
			/* IPE_BPS_LRME_RD_URGENCY_LOW_READ_MASK */
			.mask = 0x7,
			/* IPE_BPS_LRME_RD_URGENCY_LOW_READ_SHIFT */
			.shift = 0x0,
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E40, /* IPE_BPS_LRME_RD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E48, /* IPE_BPS_LRME_RD_SAFELUT_LOW */
			.value = 0xFFFF,
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
			.offset = 0x2F08, /* IPE_BPS_LRME_RD_DECCTL_LOW */
			.value = 1,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_BPS_LRME_WRITE,
		.port_name = "IPE_BPS_LRME_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A30, /* IPE_BPS_LRME_WR_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A34, /* IPE_BPS_LRME_WR_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x2A38, /* IPE_BPS_LRME_WR_URGENCY_LOW */
			/* IPE_BPS_LRME_WR_URGENCY_LOW_WRITE_MASK */
			.mask = 0x70,
			/* IPE_BPS_LRME_WR_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A40, /* IPE_BPS_LRME_WR_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A48, /* IPE_BPS_LRME_WR_SAFELUT_LOW */
			.value = 0xFFFF,
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
			.offset = 0x2B88, /* IPE_BPS_LRME_WR_ENCCTL_LOW */
			.value = 0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x2A20, /* IBL_WR_MAXWR_LOW */
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
			.offset = 0x2630, /* JPEG_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2634, /* JPEG_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x2638, /* JPEG_URGENCY_LOW */
			.mask = 0x3F,
			.shift = 0x0,
			.value = 0x22,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2640, /* JPEG_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2648, /* JPEG_SAFELUT_LOW */
			.value = 0xFFFF,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x2620, /* JPEG_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		/*SidebandManager_main_SidebandManager_FlagOutSet0_Low*/
		.port_type = CAM_CAMNOC_ICP,
		.port_name = "ICP",
		.enable = true,
		.flag_out_set0_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x5088,
			.value = 0x50,
		},
	},
};

static struct cam_camnoc_err_logger_info cam165_cpas100_err_logger_offsets = {
	.mainctrl     =  0x4F08, /* ERRLOGGER_MAINCTL_LOW */
	.errvld       =  0x4F10, /* ERRLOGGER_ERRVLD_LOW */
	.errlog0_low  =  0x4F20, /* ERRLOGGER_ERRLOG0_LOW */
	.errlog0_high =  0x4F24, /* ERRLOGGER_ERRLOG0_HIGH */
	.errlog1_low  =  0x4F28, /* ERRLOGGER_ERRLOG1_LOW */
	.errlog1_high =  0x4F2C, /* ERRLOGGER_ERRLOG1_HIGH */
	.errlog2_low  =  0x4F30, /* ERRLOGGER_ERRLOG2_LOW */
	.errlog2_high =  0x4F34, /* ERRLOGGER_ERRLOG2_HIGH */
	.errlog3_low  =  0x4F38, /* ERRLOGGER_ERRLOG3_LOW */
	.errlog3_high =  0x4F3C, /* ERRLOGGER_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam165_cpas100_errata_wa_list = {
	.camnoc_flush_slave_pending_trans = {
		.enable = false,
		.data.reg_info = {
			.access_type = CAM_REG_TYPE_READ,
			.offset = 0x2300, /* sbm_SenseIn0_Low */
			.mask = 0xE0000, /* Bits 17, 18, 19 */
			.value = 0, /* expected to be 0 */
		},
	},
};

static struct cam_camnoc_info cam165_cpas100_camnoc_info = {
	.specific = &cam_cpas_v165_100_camnoc_specific[0],
	.specific_size = ARRAY_SIZE(cam_cpas_v165_100_camnoc_specific),
	.irq_sbm = &cam_cpas_v165_100_irq_sbm,
	.irq_err = &cam_cpas_v165_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v165_100_irq_err),
	.err_logger = &cam165_cpas100_err_logger_offsets,
	.errata_wa_list = &cam165_cpas100_errata_wa_list,
};

#endif /* _CPASTOP_V165_100_H_ */


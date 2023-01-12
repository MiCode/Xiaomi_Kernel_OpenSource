/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CPASTOP_V175_130_H_
#define _CPASTOP_V175_130_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v175_130_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x2240, /* SBM_FAULTINEN0_LOW */
		.value = 0x1 | /* SBM_FAULTINEN0_LOW_PORT0_MASK*/
			0x2 | /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x4 | /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x8 | /* SBM_FAULTINEN0_LOW_PORT3_MASK */
			0x10 | /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			0x20 | /* SBM_FAULTINEN0_LOW_PORT5_MASK */
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
	cam_cpas_v175_130_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = true,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x4F08, /* ERRORLOGGER_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x4F10, /* ERRORLOGGER_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x4F18, /* ERRORLOGGER_ERRCLR_LOW */
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
			/* SPECIFIC_IFE0_MAIN_ENCERRSTATUS_LOW */
			.offset = 0x3B90,
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
			/* SPECIFIC_IFE1_WR_ENCERRSTATUS_LOW */
			.offset = 0x5590,
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
		.enable = true,
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
		.sbm_port = 0x100, /* SBM_FAULTINSTATUS0_LOW_PORT8_MASK */
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
	cam_cpas_v175_130_camnoc_specific[] = {
	{
		.port_type = CAM_CAMNOC_CDM,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4230, /* SPECIFIC_CDM_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4234, /* SPECIFIC_CDM_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			/* cdm_main_SpecificToNttpTr_Urgency_Low */
			.offset = 0x4238,
			.mask = 0x7, /* SPECIFIC_CDM_URGENCY_LOW_READ_MASK */
			.shift = 0x0, /* SPECIFIC_CDM_URGENCY_LOW_READ_SHIFT */
			.value = 0x2,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4240, /* SPECIFIC_CDM_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x4248, /* SPECIFIC_CDM_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE0123_RDI_WRITE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IFE0123_PRIORITYLUT_LOW */
			.offset = 0x3630,
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IFE0123_PRIORITYLUT_HIGH */
			.offset = 0x3634,
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x3638, /* SPECIFIC_IFE0123_URGENCY_LOW */
			/* SPECIFIC_IFE0123_URGENCY_LOW_WRITE_MASK */
			.mask = 0x70,
			/* SPECIFIC_IFE0123_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 3,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3640, /* SPECIFIC_IFE0123_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3648, /* SPECIFIC_IFE0123_SAFELUT_LOW */
			.value = 0xF,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE0_NRDI_WRITE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3A30, /* SPECIFIC_IFE0_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3A34, /* SPECIFIC_IFE0_PRIORITYLUT_HIGH */
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x3A38, /* SPECIFIC_IFE0_URGENCY_LOW */
			/* SPECIFIC_IFE0_URGENCY_LOW_WRITE_MASK */
			.mask = 0x70,
			/* SPECIFIC_IFE0_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 3,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3A40, /* SPECIFIC_IFE0_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3A48, /* SPECIFIC_IFE0_SAFELUT_LOW */
			.value = 0xF,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3B88, /* SPECIFIC_IFE0_ENCCTL_LOW */
			.value = 1,
		},
	},
	{
		/* IFE0/1 RDI READ PATH */
		.port_type = CAM_CAMNOC_IFE01_RDI_READ,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3230, /* SPECIFIC_IFE1_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3234, /* SPECIFIC_IFE1_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x3238, /* SPECIFIC_IFE1_URGENCY_LOW */
			/* SPECIFIC_IFE1_URGENCY_LOW_WRITE_MASK */
			.mask = 0x7,
			/* SPECIFIC_IFE1_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x0,
			.value = 3,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3240, /* SPECIFIC_IFE1_DANGERLUT_LOW */
			.value = 0x00000000,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x3248, /* SPECIFIC_IFE1_SAFELUT_LOW */
			.value = 0xFFFFFFFF,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE1_NRDI_WRITE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5430, /* SPECIFIC_IFE1_WR_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IFE1_WR_PRIORITYLUT_HIGH */
			.offset = 0x5434,
			.value = 0x66666666,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x5438, /* SPECIFIC_IFE1_WR_URGENCY_LOW */
			/* SPECIFIC_IFE1_WR_URGENCY_LOW_WRITE_MASK */
			.mask = 0x70,
			/* SPECIFIC_IFE1_WR_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 3,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x5440, /* SPECIFIC_IFE1_WR_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x5448, /* SPECIFIC_IFE1_WR_SAFELUT_LOW */
			.value = 0xF,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x5588, /* SPECIFIC_IFE1_WR_ENCCTL_LOW */
			.value = 1,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_BPS_LRME_READ,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E30, /* SPECIFIC_IBL_RD_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E34, /* SPECIFIC_IBL_RD_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x2E38, /* SPECIFIC_IBL_RD_URGENCY_LOW */
			/* SPECIFIC_IBL_RD_URGENCY_LOW_READ_MASK */
			.mask = 0x7,
			/* SPECIFIC_IBL_RD_URGENCY_LOW_READ_SHIFT */
			.shift = 0x0,
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E40, /* SPECIFIC_IBL_RD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2E48, /* SPECIFIC_IBL_RD_SAFELUT_LOW */
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
			.offset = 0x2F08, /* SPECIFIC_IBL_RD_DECCTL_LOW */
			.value = 1,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_BPS_LRME_WRITE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A30, /* SPECIFIC_IBL_WR_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A34, /* SPECIFIC_IBL_WR_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			.offset = 0x2A38, /* SPECIFIC_IBL_WR_URGENCY_LOW */
			/* SPECIFIC_IBL_WR_URGENCY_LOW_WRITE_MASK */
			.mask = 0x70,
			/* SPECIFIC_IBL_WR_URGENCY_LOW_WRITE_SHIFT */
			.shift = 0x4,
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A40, /* SPECIFIC_IBL_WR_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2A48, /* SPECIFIC_IBL_WR_SAFELUT_LOW */
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
			.offset = 0x2B88, /* SPECIFIC_IBL_WR_ENCCTL_LOW */
			.value = 0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_VID_DISP_WRITE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IPE_VID_DISP_PRIORITYLUT_LOW */
			.offset = 0x5E30,
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IPE_VID_DISP_PRIORITYLUT_HIGH */
			.offset = 0x5E34,
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 1,
			/* SPECIFIC_IPE_VID_DISP_URGENCY_LOW */
			.offset = 0x5E38,
			/* SPECIFIC_IPE_VID_DISP_URGENCY_LOW_READ_MASK */
			.mask = 0x70,
			/* SPECIFIC_IPE_VID_DISP_URGENCY_LOW_READ_SHIFT */
			.shift = 0x4,
			.value = 3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC__IPE_VID_DISP_DANGERLUT_LOW */
			.offset = 0x5E40,
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* SPECIFIC_IPE_VID_DISP_SAFELUT_LOW */
			.offset = 0x5E48,
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
			.offset = 0x5F88, /* SPECIFIC_IBL_WR_ENCCTL_LOW */
			.value = 1,
		},
	},

	{
		.port_type = CAM_CAMNOC_JPEG,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2630, /* SPECIFIC_JPEG_PRIORITYLUT_LOW */
			.value = 0x22222222,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2634, /* SPECIFIC_JPEG_PRIORITYLUT_HIGH */
			.value = 0x22222222,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2638, /* SPECIFIC_JPEG_URGENCY_LOW */
			.value = 0x22,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2640, /* SPECIFIC_JPEG_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x2648, /* SPECIFIC_JPEG_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_FD,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3E30, /* SPECIFIC_FD_PRIORITYLUT_LOW */
			.value = 0x44444444,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3E34, /* SPECIFIC_FD_PRIORITYLUT_HIGH */
			.value = 0x44444444,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3E38, /* SPECIFIC_FD_URGENCY_LOW */
			.value = 0x44,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3E40, /* SPECIFIC_FD_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x3E48, /* SPECIFIC_FD_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
		},

	},
	{
		/*SidebandManager_main_SidebandManager_FlagOutSet0_Low*/
		.port_type = CAM_CAMNOC_ICP,
		.enable = true,
		.flag_out_set0_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_WRITE,
			.masked_value = 0,
			.offset = 0x2288,
			.value = 0x100000,
		},
	},
};

static struct cam_camnoc_err_logger_info cam175_cpas130_err_logger_offsets = {
	.mainctrl     =  0x4F08, /* ERRLOGGER_MAINCTL_LOW */
	.errvld       =  0x4F10, /* ERRLOGGER_ERRVLD_LOW */
	.errlog0_low  =  0x4F20, /* ERRLOGGER_ERRLOG0_LOW */
	.errlog0_high =  0x4F24, /* ERRLOGGER_ERRLOG0_HIGH */
	.errlog1_low  =  0x4F28, /* ERRLOGGER_ERRLOG1_LOW */
	.errlog1_high =  0x4F2c, /* ERRLOGGER_ERRLOG1_HIGH */
	.errlog2_low  =  0x4F30, /* ERRLOGGER_ERRLOG2_LOW */
	.errlog2_high =  0x4F34, /* ERRLOGGER_ERRLOG2_HIGH */
	.errlog3_low  =  0x4F38, /* ERRLOGGER_ERRLOG3_LOW */
	.errlog3_high =  0x4F3c, /* ERRLOGGER_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam175_cpas130_errata_wa_list = {
	.camnoc_flush_slave_pending_trans = {
		.enable = false,
		.data.reg_info = {
			.access_type = CAM_REG_TYPE_READ,
			.offset = 0x2300, /* SidebandManager_SenseIn0_Low */
			.mask = 0xE0000, /* Bits 17, 18, 19 */
			.value = 0, /* expected to be 0 */
		},
	},
	/* TZ owned register */
	.tcsr_camera_hf_sf_ares_glitch = {
		.enable = true,
		.data.reg_info = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			/* TCSR_CAMERA_HF_SF_ARES_GLITCH_MASK */
			.offset = 0x01FCA08C,
			.value = 0x4, /* set bit[2] to 1 */
		},
	},
};

static struct cam_camnoc_fifo_lvl_info cam175_cpas130_camnoc_fifo_info = {
	.IFE0_nRDI_maxwr_offset = 0x3A20,
	.IFE1_nRDI_maxwr_offset = 0x5420,
	.IFE0123_RDI_maxwr_offset = 0x3620,
};

static struct cam_camnoc_info cam175_cpas130_camnoc_info = {
	.specific = &cam_cpas_v175_130_camnoc_specific[0],
	.specific_size =  ARRAY_SIZE(cam_cpas_v175_130_camnoc_specific),
	.irq_sbm = &cam_cpas_v175_130_irq_sbm,
	.irq_err = &cam_cpas_v175_130_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v175_130_irq_err),
	.err_logger = &cam175_cpas130_err_logger_offsets,
	.errata_wa_list = &cam175_cpas130_errata_wa_list,
	.fill_lvl_register = &cam175_cpas130_camnoc_fifo_info,
};

#endif /* _CPASTOP_V175_130_H_ */

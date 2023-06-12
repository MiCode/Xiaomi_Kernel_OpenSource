/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CPASTOP_V520_100_H_
#define _CPASTOP_V520_100_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v520_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0xA40, /* SBM_FAULTINEN0_LOW */
		.value = 0x1 | /* SBM_FAULTINEN0_LOW_PORT0_MASK*/
			(TEST_IRQ_ENABLE ?
			0x2 : /* SBM_FAULTINEN0_LOW_PORT6_MASK */
			0x0) /* SBM_FAULTINEN0_LOW_PORT1_MASK */,
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0xA48, /* SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0xA80, /* SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x3 : 0x1,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v520_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = true,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0xD08, /* ERRORLOGGER_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0xD10, /* ERRORLOGGER_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0xD18, /* ERRORLOGGER_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_CAMNOC_TEST,
		.enable = TEST_IRQ_ENABLE ? true : false,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT6_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0xA88, /* SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0xA90, /* SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false,
		},
	},
};


static struct cam_camnoc_specific
	cam_cpas_v520_100_camnoc_specific[] = {
	{
		.port_type = CAM_CAMNOC_CDM,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xE30, /* CDM_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xE34, /* CDM_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xE38, /* CDM_URGENCY_LOW */
			.value = 0x00000003,
		},
		.danger_lut = {
			.enable = false,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xE40, /* CDM_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xE48, /* CDM_SAFELUT_LOW */
			.value = 0x0,
		},
		.ubwc_ctl = {
			.enable = false,
			.is_fuse_based = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_TFE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* TFE_PRIORITYLUT_LOW */
			.offset = 0x30,
			.value = 0x44443333,
		},
		.priority_lut_high = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			/* TFE_PRIORITYLUT_HIGH */
			.offset = 0x34,
			.value = 0x66665555,
		},
		.urgency = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x38, /* TFE_URGENCY_LOW */
			.value = 0x00001030,
		},
		.danger_lut = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x40, /* TFE_DANGERLUT_LOW */
			.value = 0xffff0000,
		},
		.safe_lut = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x48, /* TFE_SAFELUT_LOW */
			.value = 0x00000003,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.is_fuse_based = false,
		},
	},
	{
		.port_type = CAM_CAMNOC_OPE,
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x430, /* OPE_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x434, /* OPE_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x438, /* OPE_URGENCY_LOW */
			.value = 0x00000033,
		},
		.danger_lut = {
			.enable = false,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x440, /* OPE_DANGERLUT_LOW */
			.value = 0xFFFFFF00,
		},
		.safe_lut = {
			.enable = false,
			.is_fuse_based = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.offset = 0x448, /* OPE_SAFELUT_LOW */
			.value = 0xF,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
			.is_fuse_based = false,
		},
	},
};

static struct cam_camnoc_err_logger_info cam520_cpas100_err_logger_offsets = {
	.mainctrl     =  0xD08, /* ERRLOGGER_MAINCTL_LOW */
	.errvld       =  0xD10, /* ERRLOGGER_ERRVLD_LOW */
	.errlog0_low  =  0xD20, /* ERRLOGGER_ERRLOG0_LOW */
	.errlog0_high =  0xD24, /* ERRLOGGER_ERRLOG0_HIGH */
	.errlog1_low  =  0xD28, /* ERRLOGGER_ERRLOG1_LOW */
	.errlog1_high =  0xD2C, /* ERRLOGGER_ERRLOG1_HIGH */
	.errlog2_low  =  0xD30, /* ERRLOGGER_ERRLOG2_LOW */
	.errlog2_high =  0xD34, /* ERRLOGGER_ERRLOG2_HIGH */
	.errlog3_low  =  0xD38, /* ERRLOGGER_ERRLOG3_LOW */
	.errlog3_high =  0xD3C, /* ERRLOGGER_ERRLOG3_HIGH */
};

static struct cam_camnoc_info cam520_cpas100_camnoc_info = {
	.specific = &cam_cpas_v520_100_camnoc_specific[0],
	.specific_size =  ARRAY_SIZE(cam_cpas_v520_100_camnoc_specific),
	.irq_sbm = &cam_cpas_v520_100_irq_sbm,
	.irq_err = &cam_cpas_v520_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v520_100_irq_err),
	.err_logger = &cam520_cpas100_err_logger_offsets,
	.errata_wa_list = NULL,
};

static struct cam_cpas_camnoc_qchannel cam520_cpas100_qchannel_info = {
	.qchannel_ctrl   = 0x14,
	.qchannel_status = 0x18,
};
#endif /* _CPASTOP_V520_100_H_ */

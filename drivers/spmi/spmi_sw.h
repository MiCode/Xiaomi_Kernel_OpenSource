// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */
#ifndef __SPMI_SW_H__
#define __SPMI_SW_H__

#include <linux/pmif.h>
#include <linux/spmi.h>

#define DEFAULT_VALUE_READ_TEST		(0x5a)
#define DEFAULT_VALUE_WRITE_TEST	(0xa5)

enum spmi_regs {
	SPMI_OP_ST_CTRL,
	SPMI_GRP_ID_EN,
	SPMI_OP_ST_STA,
	SPMI_MST_SAMPL,
	SPMI_MST_REQ_EN,
	SPMI_REC_CTRL,
	SPMI_REC0,
	SPMI_REC1,
	SPMI_REC2,
	SPMI_REC3,
	SPMI_REC4,
	SPMI_MST_DBG,
	/* RCS support */
	SPMI_MST_RCS_CTRL,
	SPMI_MST_IRQ,
	SPMI_SLV_3_0_EINT,
	SPMI_SLV_7_4_EINT,
	SPMI_SLV_B_8_EINT,
	SPMI_SLV_F_C_EINT,
	SPMI_TIA,
	SPMI_DEC_DBG,
	SPMI_REC_CMD_DEC,
};

/* pmif debug API declaration */
extern void spmi_dump_wdt_reg(void);
extern void spmi_dump_pmif_acc_vio_reg(void);
extern void spmi_dump_pmic_acc_vio_reg(void);
extern void spmi_dump_pmif_busy_reg(void);
extern void spmi_dump_pmif_swinf_reg(void);
extern void spmi_dump_pmif_all_reg(void);
extern void spmi_dump_pmif_record_reg(void);
/* spmi debug API declaration */
extern void spmi_dump_spmimst_all_reg(void);
/* pmic debug API declaration */
extern int spmi_pmif_create_attr(struct device_driver *driver);
extern int spmi_pmif_dbg_init(struct spmi_controller *ctrl);
#endif /*__SPMI_SW_H__*/

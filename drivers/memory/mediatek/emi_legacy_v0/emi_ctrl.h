/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __EMI_BWL_H__
#define __EMI_BWL_H__


/* BWL Begin */

/*
 * Define EMI hardware registers.
 */
#define EMI_CONM		(CEN_EMI_BASE + 0x0060)
#define EMI_MDCT		(CEN_EMI_BASE + 0x0078)
#define EMI_ARBA		(CEN_EMI_BASE + 0x0100)
#define EMI_ARBB		(CEN_EMI_BASE + 0x0108)
#define EMI_ARBC		(CEN_EMI_BASE + 0x0110)
#define EMI_ARBD		(CEN_EMI_BASE + 0x0118)
#define EMI_ARBE		(CEN_EMI_BASE + 0x0120)
#define EMI_ARBF		(CEN_EMI_BASE + 0x0128)
#define EMI_ARBG		(CEN_EMI_BASE + 0x0130)
#define EMI_ARBH		(CEN_EMI_BASE + 0x0138)

/* define concurrency scenario ID */
enum {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg, arbh, \
conm, mdct) con_sce,
#include "con_sce_lpddr3.h"
#undef X_CON_SCE
	NR_CON_SCE
};

/* define control operation */
enum {
	ENABLE_CON_SCE = 0,
	DISABLE_CON_SCE = 1
};

#define EN_CON_SCE_STR "ON"
#define DIS_CON_SCE_STR "OFF"

/* define control table entry */
struct emi_bwl_ctrl {
	unsigned int ref_cnt;
};

/* BWL End */

unsigned int get_dram_type(void);
void __iomem *mt_cen_emi_base_get(void);
void __iomem *mt_chn_emi_base_get(void);
void __iomem *mt_emi_mpu_base_get(void);
unsigned int mt_emi_mpu_irq_get(void);
extern int mpu_init(void);

#endif  /* !__EMI_BWL_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6761_DCM_INTERNAL_H__
#define __MT6761_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mt6761_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */
/* #define DCM_BRINGUP */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* #define CTRL_BIGCORE_DCM_IN_KERNEL */

/* #define reg_read(addr)	__raw_readl(IOMEM(addr)) */
#define reg_read(addr) readl((void *)addr)
/*#define reg_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))*/
#define reg_write(addr, val) \
	do { writel(val, (void *)addr); wmb(); } while (0) /* sync write */

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write_phy(addr##_PHYS, val)
#ifndef mcsi_reg_read
#define mcsi_reg_read(offset) \
	mt_secure_call(MTK_SIP_KERENL_MCSI_NS_ACCESS, 0, offset, 0)
#endif
#ifndef mcsi_reg_write
#define mcsi_reg_write(val, offset) \
	mt_secure_call(MTK_SIP_KERENL_MCSI_NS_ACCESS, 1, offset, val)
#endif
#define MCSI_SMC_WRITE(addr, val)  mcsi_reg_write(val, (addr##_PHYS & 0xFFFF))
#define MCSI_SMC_READ(addr)  mcsi_reg_read(addr##_PHYS & 0xFFFF)
#else
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write(addr, val)
#define MCSI_SMC_WRITE(addr, val)  reg_write(addr, val)
#define MCSI_SMC_READ(addr)  reg_read(addr)
#endif

#define REG_DUMP(addr) \
	dcm_pr_info("%-30s(0x%08lx): 0x%08x\n", #addr, addr, reg_read(addr))
#define SECURE_REG_DUMP(addr) \
	dcm_pr_info("%-30s(0x%08lx): 0x%08x\n", \
	#addr, addr, mcsi_reg_read(addr##_PHYS & 0xFFFF))

/* Sync DCM related RG bit definitions. */
/* TODO: Why not autogen? */
#define SYNC_DCM_CLK_MIN_FREQ		52
#define SYNC_DCM_MAX_DIV_VAL		127

#define MCUCFG_SYNC_DCM_CCI_REG		SYNC_DCM_CONFIG
#define MCUCFG_SYNC_DCM_CCI		(1)
#define MCUCFG_SYNC_DCM_CCI_TOGMASK	(0x1 << MCUCFG_SYNC_DCM_CCI)
#define MCUCFG_SYNC_DCM_TOGMASK		(MCUCFG_SYNC_DCM_CCI_TOGMASK)
#define MCUCFG_SYNC_DCM_TOG1		MCUCFG_SYNC_DCM_TOGMASK
#define MCUCFG_SYNC_DCM_CCI_TOG1	MCUCFG_SYNC_DCM_CCI_TOGMASK
#define MCUCFG_SYNC_DCM_CCI_TOG0	(0x0 << MCUCFG_SYNC_DCM_CCI)
#define MCUCFG_SYNC_DCM_TOG0		(MCUCFG_SYNC_DCM_CCI_TOG0)
#define MCUCFG_SYNC_DCM_SEL_CCI		(2)
#define MCUCFG_SYNC_DCM_SEL_CCI_MASK	(0x7F << MCUCFG_SYNC_DCM_SEL_CCI)
#define MCUCFG_SYNC_DCM_SEL_MASK	(MCUCFG_SYNC_DCM_SEL_CCI_MASK)

int dcm_armcore(int mode);
int dcm_infra(int on);
int dcm_peri(int on);
int dcm_mcusys(int on);
int dcm_dramc_ao(int on);
int dcm_emi(int on);
int dcm_ddrphy(int on);
int dcm_stall(int on);
int dcm_big_core(int on);
int dcm_gic_sync(int on);
int dcm_last_core(int on);
int dcm_rgu(int on);
int dcm_topckg(int on);
int dcm_lpdma(int on);
int dcm_mcsi(int on);

int mt_dcm_dts_map(void);
void dcm_set_hotplug_nb(void);
short dcm_get_cpu_cluster_stat(void);
/* unit of frequency is MHz */
int sync_dcm_set_cpu_freq(unsigned int cci,
			  unsigned int mp0,
			  unsigned int mp1,
			  unsigned int mp2);
int sync_dcm_set_cpu_div(unsigned int cci,
			 unsigned int mp0,
			 unsigned int mp1,
			 unsigned int mp2);

/* remove for new arch extern struct DCM dcm_array[NR_DCM_TYPE]; */
void dcm_array_register(void);

extern void *mt_dramc_chn_base_get(int channel);
extern void *mt_ddrphy_chn_base_get(int channel);
extern void __iomem *mt_cen_emi_base_get(void);
extern void __iomem *mt_chn_emi_base_get(int chn);

#endif /* #ifndef __MT6761_DCM_INTERNAL_H__ */


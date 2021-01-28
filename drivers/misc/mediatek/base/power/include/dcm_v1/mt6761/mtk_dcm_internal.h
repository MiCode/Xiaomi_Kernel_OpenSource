/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_DCM_INTERNAL_H__
#define __MTK_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mtk_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */
/* #define DCM_BRINGUP */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* #define CTRL_BIGCORE_DCM_IN_KERNEL */

#define reg_read(addr)	__raw_readl(IOMEM(addr))
#define reg_write(addr, val)	mt_reg_sync_writel((val), ((void *)addr))

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

enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON+1,
};

enum {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
};

enum {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
};

enum {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
};

enum {
	DRAMC_AO_DCM_OFF = DCM_OFF,
	DRAMC_AO_DCM_ON = DCM_ON,
};

enum {
	DDRPHY_DCM_OFF = DCM_OFF,
	DDRPHY_DCM_ON = DCM_ON,
};

enum {
	EMI_DCM_OFF = DCM_OFF,
	EMI_DCM_ON = DCM_ON,
};

enum {
	STALL_DCM_OFF = DCM_OFF,
	STALL_DCM_ON = DCM_ON,
};

enum {
	BIG_CORE_DCM_OFF = DCM_OFF,
	BIG_CORE_DCM_ON = DCM_ON,
};

enum {
	GIC_SYNC_DCM_OFF = DCM_OFF,
	GIC_SYNC_DCM_ON = DCM_ON,
};

enum {
	LAST_CORE_DCM_OFF = DCM_OFF,
	LAST_CORE_DCM_ON = DCM_ON,
};

enum {
	RGU_DCM_OFF = DCM_OFF,
	RGU_DCM_ON = DCM_ON,
};

enum {
	TOPCKG_DCM_OFF = DCM_OFF,
	TOPCKG_DCM_ON = DCM_ON,
};

enum {
	LPDMA_DCM_OFF = DCM_OFF,
	LPDMA_DCM_ON = DCM_ON,
};

enum {
	MCSI_DCM_OFF = DCM_OFF,
	MCSI_DCM_ON = DCM_ON,
};

enum {
	ARMCORE_DCM = 0,
	MCUSYS_DCM,
	INFRA_DCM,
	PERI_DCM,
	EMI_DCM,
	DRAMC_DCM,
	DDRPHY_DCM,
	STALL_DCM,
	BIG_CORE_DCM,
	GIC_SYNC_DCM,
	LAST_CORE_DCM,
	RGU_DCM,
	TOPCKG_DCM,
	LPDMA_DCM,
	MCSI_DCM,
	NR_DCM,
};

enum {
	ARMCORE_DCM_TYPE	= (1U << ARMCORE_DCM),
	MCUSYS_DCM_TYPE		= (1U << MCUSYS_DCM),
	INFRA_DCM_TYPE		= (1U << INFRA_DCM),
	PERI_DCM_TYPE		= (1U << PERI_DCM),
	EMI_DCM_TYPE		= (1U << EMI_DCM),
	DRAMC_DCM_TYPE		= (1U << DRAMC_DCM),
	DDRPHY_DCM_TYPE		= (1U << DDRPHY_DCM),
	STALL_DCM_TYPE		= (1U << STALL_DCM),
	BIG_CORE_DCM_TYPE	= (1U << BIG_CORE_DCM),
	GIC_SYNC_DCM_TYPE	= (1U << GIC_SYNC_DCM),
	LAST_CORE_DCM_TYPE	= (1U << LAST_CORE_DCM),
	RGU_DCM_TYPE		= (1U << RGU_DCM),
	TOPCKG_DCM_TYPE		= (1U << TOPCKG_DCM),
	LPDMA_DCM_TYPE		= (1U << LPDMA_DCM),
	MCSI_DCM_TYPE		= (1U << MCSI_DCM),
	NR_DCM_TYPE = NR_DCM,
};

enum {
	DCM_CPU_CLUSTER_LL	= (1U << 0),
	DCM_CPU_CLUSTER_L	= (1U << 1),
	DCM_CPU_CLUSTER_B	= (1U << 2),
};

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

extern struct DCM dcm_array[NR_DCM_TYPE];

extern void *mt_dramc_chn_base_get(int channel);
extern void *mt_ddrphy_chn_base_get(int channel);
extern void __iomem *mt_cen_emi_base_get(void);
extern void __iomem *mt_chn_emi_base_get(int chn);

#endif /* #ifndef __MTK_DCM_INTERNAL_H__ */


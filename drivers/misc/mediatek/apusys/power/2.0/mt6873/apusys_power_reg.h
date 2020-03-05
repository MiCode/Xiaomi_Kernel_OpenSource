/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/io.h>
#include <sync_write.h>

/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) (((_val_) & \
(BITMASK(_bits_))) >> ((0) ? _bits_))

static inline void DRV_WriteReg32(void *addr, uint32_t value)
{
	mt_reg_sync_writel(value, addr);
}

static inline u32 DRV_Reg32(void *addr)
{
	return ioread32(addr);
}

static inline void DRV_SetBitReg32(void *addr, uint32_t bit_mask)
{
	u32 tmp = ioread32(addr);

	tmp |= bit_mask;
	mt_reg_sync_writel(tmp, addr);
}

static inline void DRV_ClearBitReg32(void *addr, uint32_t bit_mask)
{
	u32 tmp = ioread32(addr);

	tmp &= ~bit_mask;
	mt_reg_sync_writel(tmp, addr);
}


extern void *g_APU_RPCTOP_BASE;
extern void *g_APU_PCUTOP_BASE;
extern void *g_APU_VCORE_BASE;
extern void *g_APU_INFRACFG_AO_BASE;
extern void *g_APU_INFRA_BCRM_BASE;
extern void *g_APU_CONN_BASE;
extern void *g_APU_VPU0_BASE;
extern void *g_APU_VPU1_BASE;
extern void *g_APU_MDLA0_BASE;
extern void *g_APU_SPM_BASE;
extern void *g_APU_APMIXED_BASE;

#define REG_POLLING_TIMEOUT_ROUNDS	(50) // 50 * 10us = 500us

/**************************************************
 * APU_RPC related register
 *************************************************/
#define	APU_RPCTOP_BASE		(g_APU_RPCTOP_BASE)
#define	APU_RPC_TOP_CON		(void *)(APU_RPCTOP_BASE + 0x000)
#define	APU_RPC_TOP_SEL		(void *)(APU_RPCTOP_BASE + 0x004)
#define	APU_RPC_SW_FIFO_WE	(void *)(APU_RPCTOP_BASE + 0x008)
#define	APU_RPC_INTF_PWR_RDY	(void *)(APU_RPCTOP_BASE + 0x044)

#define	APU_RPC_PWR_CON2	(void *)(APU_RPCTOP_BASE + 0x108)
#define	APU_RPC_PWR_CON3	(void *)(APU_RPCTOP_BASE + 0x10C)
#define	APU_RPC_PWR_CON4	(void *)(APU_RPCTOP_BASE + 0x110)
#define	APU_RPC_PWR_CON6	(void *)(APU_RPCTOP_BASE + 0x118)
#define	APU_RPC_PWR_CON7	(void *)(APU_RPCTOP_BASE + 0x11C)

#define	APU_RPC_SW_TYPE0	(void *)(APU_RPCTOP_BASE + 0x200)
#define	APU_RPC_SW_TYPE1	(void *)(APU_RPCTOP_BASE + 0x210)
#define	APU_RPC_SW_TYPE2	(void *)(APU_RPCTOP_BASE + 0x220)
#define	APU_RPC_SW_TYPE3	(void *)(APU_RPCTOP_BASE + 0x230)
#define	APU_RPC_SW_TYPE4	(void *)(APU_RPCTOP_BASE + 0x240)
#define	APU_RPC_SW_TYPE6	(void *)(APU_RPCTOP_BASE + 0x260)
#define	APU_RPC_SW_TYPE7	(void *)(APU_RPCTOP_BASE + 0x270)

#define REG_WAKEUP_SET		(BIT(8) | BIT(9) | BIT(10) | BIT(11))
#define REG_WAKEUP_CLR		(BIT(12) | BIT(13) | BIT(14) | BIT(15))

/**************************************************
 * APU_PCU related register
 *************************************************/
#define	APU_PCUTOP_BASE		(g_APU_PCUTOP_BASE)
#define	APU_PCU_PMIC_TAR_BUF	(void *)(APU_PCUTOP_BASE + 0x120)
#define	APU_PCU_PMIC_CUR_BUF	(void *)(APU_PCUTOP_BASE + 0x124)
#define APU_PCU_PMIC_STATUS	(void *)(APU_PCUTOP_BASE + 0x128)

/**************************************************
 * APU_VCORE related register
 *************************************************/
#define	APU_VCORE_BASE		(g_APU_VCORE_BASE)
#define APU_VCORE_CG_CON	(void *)(APU_VCORE_BASE + 0x000)
#define APU_VCORE_CG_SET	(void *)(APU_VCORE_BASE + 0x004)
#define	APU_VCORE_CG_CLR	(void *)(APU_VCORE_BASE + 0x008)

/**************************************************
 * INFRA_AO and related register
 *************************************************/
#define APU_INFRACFG_AO_BASE	(g_APU_INFRACFG_AO_BASE)
#define INFRACFG_AO_MEM_PROT	(void *)(APU_INFRACFG_AO_BASE + 0xE98)

/**************************************************
 * INFRA_BCRM and related register
 *************************************************/
#define APU_INFRA_BCRM_BASE	(g_APU_INFRA_BCRM_BASE)
#define INFRA_BCRM_MEM_PROT_A	(void *)(APU_INFRA_BCRM_BASE + 0x170)
#define INFRA_BCRM_MEM_PROT_B	(void *)(APU_INFRA_BCRM_BASE + 0x174)
#define INFRA_BCRM_MEM_PROT_C	(void *)(APU_INFRA_BCRM_BASE + 0x178)

/**************************************************
 * SPM and related register
 *************************************************/
#define APU_SPM_BASE		(g_APU_SPM_BASE)
#define OTHER_PWR_STATUS	(void *)(APU_SPM_BASE + 0x178)
#define BUCK_ISOLATION		(void *)(APU_SPM_BASE + 0x39C)
#define SPM_CROSS_WAKE_M01_REQ	(void *)(APU_SPM_BASE + 0x670)

/**************************************************
 * debug related register
 *************************************************/
#define	APU_CONN_BASE			(g_APU_CONN_BASE)
#define APU_CONN_CG_CON			(void *)(APU_CONN_BASE+0x000)

#define	APU0_BASE			(g_APU_VPU0_BASE)
#define APU0_APU_CG_CON			(void *)(APU0_BASE+0x100)

#define	APU1_BASE			(g_APU_VPU1_BASE)
#define APU1_APU_CG_CON			(void *)(APU1_BASE+0x100)

#define	APU_MDLA0_BASE			(g_APU_MDLA0_BASE)
#define APU_MDLA0_APU_MDLA_CG_CON	(void *)(APU_MDLA0_BASE+0x000)

/**************************************************
 * Clock Setting
 **************************************************/
#define APU_APMIXED_BASE		(g_APU_APMIXED_BASE)
#define POSDIV_4_MAX_FREQ               (950000)        /* KHz */
#define POSDIV_4_MIN_FREQ               (375000)        /* KHz */
#define POSDIV_8_MAX_FREQ               (475000)        /* KHz */
#define POSDIV_8_MIN_FREQ               (187500)        /* KHz */
#define POSDIV_SHIFT                    (24)            /* bit */
#define DDS_SHIFT                       (14)            /* bit */
#define TO_MHZ_HEAD                     (100)
#define TO_MHZ_TAIL                     (10)
#define ROUNDING_VALUE                  (5)
#define PLL_FIN                      (26)            /* MHz */
#define APUPLL_FH_PLL                   FH_PLL13
#define NPUPLL_FH_PLL                   FH_PLL4
#define APUPLL_CON1                     (void *)(APU_APMIXED_BASE + 0x03A4)
#define NPUPLL_CON1                     (void *)(APU_APMIXED_BASE + 0x03B8)

/**************************************************
 * Vol Binning
 **************************************************/
#define EFUSE_INDEX 209  //(PTPOD26)
#define EFUSE_POD19 69   //(PTPOD19)


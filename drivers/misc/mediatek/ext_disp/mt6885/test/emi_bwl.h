/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MT_EMI_BW_LIMITER__
#define __MT_EMI_BW_LIMITER__

/*
 * Define EMI hardware registers.
 */

#define EMI_CONA		(EMI_BASE_ADDR + 0x0000)
#define EMI_CONB		(EMI_BASE_ADDR + 0x0008)
#define EMI_CONC		(EMI_BASE_ADDR + 0x0010)
#define EMI_COND		(EMI_BASE_ADDR + 0x0018)
#define EMI_CONE		(EMI_BASE_ADDR + 0x0020)
#define EMI_CONF		(EMI_BASE_ADDR + 0x0028)
#define EMI_CONG		(EMI_BASE_ADDR + 0x0030)
#define EMI_CONH		(EMI_BASE_ADDR + 0x0038)
#define EMI_TESTB		(EMI_BASE_ADDR + 0x00E8)
#define EMI_TESTD		(EMI_BASE_ADDR + 0x00F8)
#define EMI_ARBA		(EMI_BASE_ADDR + 0x0100)
#define EMI_ARBB		(EMI_BASE_ADDR + 0x0108)
#define EMI_ARBC		(EMI_BASE_ADDR + 0x0110)
#define EMI_ARBD		(EMI_BASE_ADDR + 0x0118)
#define EMI_ARBE		(EMI_BASE_ADDR + 0x0120)
#define EMI_ARBF		(EMI_BASE_ADDR + 0x0128)
#define EMI_ARBG		(EMI_BASE_ADDR + 0x0130)
#define EMI_ARBG_2ND	(EMI_BASE_ADDR + 0x0134)
#define EMI_ARBH		(EMI_BASE_ADDR + 0x0138)
#define EMI_ARBI		(EMI_BASE_ADDR + 0x0140)
#define EMI_ARBI_2ND	(EMI_BASE_ADDR + 0x0144)
#define EMI_ARBJ		(EMI_BASE_ADDR + 0x0148)
#define EMI_ARBJ_2ND	(EMI_BASE_ADDR + 0x014C)
#define EMI_ARBK		(EMI_BASE_ADDR + 0x0150)
#define EMI_ARBK_2ND	(EMI_BASE_ADDR + 0x0154)
#define EMI_SLCT		(EMI_BASE_ADDR + 0x0158)

#define LOW_POWER_CORRELATION
#ifdef LOW_POWER_CORRELATION
enum TRANS_TYPE {
	R = 0,
	W,
	RW
};

extern phys_addr_t dram_rank0_addr;

extern unsigned int bw_mon_in_ms(enum TRANS_TYPE trans_type, unsigned int ms);
extern unsigned int get_dram_data_rate(void);

#define CLK_CFG_0		(CKGEN_BASE_ADDR + 0x100)
#define CLK_CFG_5		(CKGEN_BASE_ADDR + 0x150)
#define CLK_CFG_0_CLR		(CKGEN_BASE_ADDR + 0x108)
#define CLK_CFG_UPDATE		(CKGEN_BASE_ADDR + 0x900)

#define EMI_BMEN		(EMI_BASE_ADDR + 0x0400)
#define EMI_BCNT		(EMI_BASE_ADDR + 0x0408)
#define EMI_WSCT		(EMI_BASE_ADDR + 0x0428)
#define EMI_MSEL		(EMI_BASE_ADDR + 0x0440)
#define EMI_MSEL2		(EMI_BASE_ADDR + 0x0468)
#define EMI_BMEN2		(EMI_BASE_ADDR + 0x04E8)
#define EMI_BMRW0		(EMI_BASE_ADDR + 0x04F8)
#define EMI_IOCL		(EMI_BASE_ADDR + 0x00D0)
#define EMI_IOCL_2ND		(EMI_BASE_ADDR + 0x00D4)
#define EMI_IOCM		(EMI_BASE_ADDR + 0x00D8)
#define EMI_IOCM_2ND		(EMI_BASE_ADDR + 0x00DC)

#define DISP_FAKE_ENG_RD_ADDR	(MMSYS_BASE_ADDR + 0x210)
#define DISP_FAKE_ENG_WR_ADDR	(MMSYS_BASE_ADDR + 0x214)
#define DISP_FAKE_ENG_CON0	(MMSYS_BASE_ADDR + 0x208)
#define DISP_FAKE_ENG_CON1	(MMSYS_BASE_ADDR + 0x20C)
#define DISP_FAKE_ENG_EN	(MMSYS_BASE_ADDR + 0x200)
#define DISP_FAKE_ENG_STATE	(MMSYS_BASE_ADDR + 0x218)
#define DISP_FAKE_ENG2_RD_ADDR	(MMSYS_BASE_ADDR + 0x230)
#define DISP_FAKE_ENG2_WR_ADDR	(MMSYS_BASE_ADDR + 0x234)
#define DISP_FAKE_ENG2_CON0	(MMSYS_BASE_ADDR + 0x228)
#define DISP_FAKE_ENG2_CON1	(MMSYS_BASE_ADDR + 0x22C)
#define DISP_FAKE_ENG2_EN	(MMSYS_BASE_ADDR + 0x220)
#define DISP_FAKE_ENG2_STATE	(MMSYS_BASE_ADDR + 0x238)
#define MMSYS_CG_CLR0		(MMSYS_BASE_ADDR + 0x108)
#define MMSYS_CG_CLR1		(MMSYS_BASE_ADDR + 0x118)
#define MMSYS_CG_CLR2		(MMSYS_BASE_ADDR + 0x148)
#define MMSYS_CG_CON0		(MMSYS_BASE_ADDR + 0x100)
#define MMSYS_CG_CON1		(MMSYS_BASE_ADDR + 0x110)

#define SMI0_MON_ENA		(SMI_COMMON_BASE + 0x1A0)
#define SMI0_MON_CLR		(SMI_COMMON_BASE + 0x1A4)
#define SMI0_BUS_SEL            (SMI_COMMON_BASE + 0x220)
#define SMI_LARB0_MON_EN	(SMI_LARB0_BASE + 0x400)
#define SMI_LARB0_MON_CLR	(SMI_LARB0_BASE + 0x404)
#define SMI_LARB0_OSTDL_PORT	(SMI_LARB0_BASE + 0x200)
#define SMI_LARB1_MON_EN	(SMI_LARB1_BASE + 0x400)
#define SMI_LARB1_MON_CLR	(SMI_LARB1_BASE + 0x404)
#define SMI_LARB1_OSTDL_PORT	(SMI_LARB1_BASE + 0x200)
#endif /* LOW_POWER_CORRELATION */

#define DRAMC_CONF1     (0x004)
#define DRAMC_LPDDR2    (0x1e0)
#define DRAMC_PADCTL4   (0x0e4)
#define DRAMC_ACTIM1    (0x1e8)
#define DRAMC_DQSCAL0   (0x1c0)

#define DRAMC_READ(offset) ( \
				readl(IOMEM(DRAMC0_BASE + (offset)))| \
				readl(IOMEM(DDRPHY_BASE + (offset)))| \
				readl(IOMEM(DRAMC_NAO_BASE + (offset))))

#define DRAMC_WRITE(offset, data) do { \
				writel((unsigned int) (data), \
				(DRAMC0_BASE + (offset))); \
				writel((unsigned int) (data), \
				(DDRPHY_BASE + (offset))); \
				mt65xx_reg_sync_writel((unsigned int) (data), \
				(DRAMC_NAO_BASE + (offset))); \
} while (0)



/*
 * Define constants.
 */

/* define supported DRAM types */
enum {
	LPDDR2_1066 = 0,
	LPDDR4_3200,
	mDDR,
};

/* define concurrency scenario ID */
enum {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, \
arbf, arbg, arbh) con_sce,
#include "con_sce_lpddr4_3200.h"
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

/*
 * Define data structures.
 */

/* define control table entry */
struct emi_bwl_ctrl {
	unsigned int ref_cnt;
};

/*
 * Define function prototype.
 */

extern int mtk_mem_bw_ctrl(int sce, int op);
extern int get_ddr_type(void);
extern unsigned int ucDram_Register_Read(unsigned long u4reg_addr);

extern void __iomem *EMI_BASE_ADDR;
#endif  /* !__MT_EMI_BWL_H__ */


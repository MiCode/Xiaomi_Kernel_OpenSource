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

#ifndef __CCIF_PLATFORM_H__
#define __CCIF_PLATFORM_H__
#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccif_hif_platform.h"

/*MD peripheral register: MD bank8; AP bank2*/
/*Modem WDT */
#define WDT_MD_MODE     (0x00)
#define WDT_MD_LENGTH   (0x04)
#define WDT_MD_RESTART  (0x08)
#define WDT_MD_STA      (0x0C)
#define WDT_MD_SWRST    (0x1C)
#define WDT_MD_MODE_KEY (0x0000220E)

/*C2K */
#define C2K_CONFIG (0x360)
#define C2K_STATUS (0x364)
#define C2K_SPM_CTRL (0x368)

#define INFRA_C2K_BOOT_STATUS (0x308)
#define INFRA_C2K_BOOT_STATUS2 (0x30C)

#define SLEEP_CLK_CON (0x328)

#define TOP_RGU_WDT_MODE (0x0)
#define TOP_RGU_WDT_SWRST (0x14)
#define TOP_RGU_WDT_SWSYSRST (0x18)
#define TOP_RGU_WDT_NONRST_REG (0x20)

/* [3:0]:mdsrc_req_0_en */
/* [7:4]:mdsrc_req_1_en */
#define INFRA_MISC2				(0xF0C)
/* mdsrc_req_0/1_en[2]: for C2K */
#define INFRA_MISC2_C2K_BIT		(2)
#define INFRA_MISC2_C2K_EN		(0x11 << INFRA_MISC2_C2K_BIT)

/****mt6755 special****/

#define AP_PLL_CON0		0x0	/*((UINT32P)(APMIXED_BASE+0x0))*/
#define MDPLL_CON3              0x3AC	/*((UINT32P)(APMIXED_BASE+0x03AC))*/

#define INFRA_TOPAXI_PROTECTEN_1_SET 0x2A8	/*((UINT32P)(INFRACFG_AO_BASE+0x2A8))*/
#define INFRA_TOPAXI_PROTECTEN_1_CLR 0x2AC	/*((UINT32P)(INFRACFG_AO_BASE+0x2AC))*/
#define INFRA_TOPAXI_PROTECTSTA1 0x228		/*((UINT32P)(INFRACFG_AO_BASE+0x228))*/
#define AP_POWERON_CONFIG_EN        0x000	/*((UINT32P)(SLEEP_BASE+0x000))*/
#define AP_PWR_STATUS               0x180	/*((UINT32P)(SLEEP_BASE+0x180))*/
#define AP_PWR_STATUS_2ND           0x184	/*((UINT32P)(SLEEP_BASE+0x184))*/
#define PWR_RST_B     0
#define PWR_ISO       1
#define PWR_ON        2
#define PWR_ON_2ND    3
#define PWR_CLK_DIS   4
#define C2K_PWR_STA_MASK  11

/*#define C2K          28*/

#define C2K_MAGIC_NUM	0xC275

#define C2K_SBC_KEY0   0x8B0	/* ((UINT32P)(INFRACFG_AO_BASE+0x8B0)) */
#define C2K_SBC_KEY1   0x8B4
#define C2K_SBC_KEY2   0x8B8
#define C2K_SBC_KEY3   0x8BC
#define C2K_SBC_KEY4   0x8C0
#define C2K_SBC_KEY5   0x8C4
#define C2K_SBC_KEY6   0x8C8
#define C2K_SBC_KEY7   0x8CC
#define C2K_SBC_KEY_LOCK        0x8D0	/*((UINT32P)(INFRACFG_AO_BASE+0x8D0))*/

#define C2KSYS_BASE (0x38000000)
#define C2K_CGBR1               0x0200B004	/*(C2KSYS_BASE+0x0200B004)*/
#define C2K_C2K_PLL_CON3        0x02013008
#define C2K_C2K_PLL_CON2        0x02013004
#define C2K_C2K_PLLTD_CON0      0x02013074
#define C2K_CLK_CTRL9	0x0200029C
#define C2K_CLK_CTRL4	0x02000010
#define C2K_CG_ARM_AMBA_CLKSEL  0x02000234
#define C2K_C2K_C2KPLL1_CON0    0x02013018
#define C2K_C2K_CPPLL_CON0      0x02013040
#define C2K_C2K_DSPPLL_CON0     0x02013050


#define MD1_C2K_CCIRQ_BASE		0x10705000
#define C2K_MD1_CCIRQ_BASE		0x10706000

#define ETS_SEL_BIT					(0x1 << 13)

struct md_hw_info {
	/*HW info - Register Address */
	unsigned long md_rgu_base;
	unsigned long md_boot_slave_Vector;
	unsigned long md_boot_slave_Key;
	unsigned long md_boot_slave_En;
	unsigned long ap_ccif_base;
	unsigned long md_ccif_base;
	unsigned int sram_size;
	/* #ifdef CONFIG_MTK_ECCCI_C2K */
	void __iomem *sleep_base;
	void __iomem *infra_ao_base;
	unsigned long  c2k_misc;
	void __iomem *toprgu_base;
	unsigned long  c2k_chip_id_base;
	unsigned long  md1_pccif_base;
	unsigned long  md3_pccif_base;
	/* #endif */

	/*HW info - Interrutpt ID */
	unsigned int ap_ccif_irq_id;
	unsigned int md_wdt_irq_id;

	/*HW info - Interrupt flags */
	unsigned long ap_ccif_irq_flags;
	unsigned long md_wdt_irq_flags;
};

struct c2k_pll_t {
	void __iomem *c2k_pll_con3;
	void __iomem *c2k_pll_con2;
	void __iomem *c2k_plltd_con0;
	void __iomem *c2k_cppll_con0;
	void __iomem *c2k_dsppll_con0;
	void __iomem *c2k_c2kpll1_con0;
	void __iomem *c2k_cg_amba_clksel;
	void __iomem *c2k_clk_ctrl4;
	void __iomem *c2k_clk_ctrl9;

};

extern unsigned long ccci_modem_boot_count[];

extern int md_ccif_power_off(struct ccci_modem *md, unsigned int timeout);
extern int md_ccif_power_on(struct ccci_modem *md);
extern int md_ccif_let_md_go(struct ccci_modem *md);
int md_ccif_get_modem_hw_info(struct platform_device *dev_ptr,
			      struct ccci_dev_cfg *dev_cfg,
			      struct md_hw_info *hw_info);
int md_ccif_io_remap_md_side_register(struct ccci_modem *md);
void reset_md1_md3_pccif(struct ccci_modem *md);
void dump_c2k_register(struct ccci_modem *md, unsigned int dump_boot_reg);

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
extern void mtk_wdt_set_c2k_sysrst(unsigned int flag);
extern void ccci_mem_dump(int md_id, void *start_addr, int len);
extern int mtk_wdt_swsysret_config(int bit, int set_value);

#endif /*__CLDMA_PLATFORM_H__*/

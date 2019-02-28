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

#ifndef __MODEM_REG_BASE_H__
#define __MODEM_REG_BASE_H__

/* ============================================================ */
/* Modem 1 part */
/* ============================================================ */
/* MD peripheral register: MD bank8; AP bank2 */
#define CLDMA_AP_BASE 0x200F0000
#define CLDMA_AP_LENGTH 0x3000
#define CLDMA_MD_BASE 0x200E0000
#define CLDMA_MD_LENGTH 0x3000

#define MD_BOOT_VECTOR_EN 0x20000024

#define MD_PCORE_PCCIF_BASE 0x20510000

#define MD_GLOBAL_CON0 0x20000450
#define MD_GLOBAL_CON0_CLDMA_BIT 12
#define CCIF_SRAM_SIZE 512

#define BASE_ADDR_MDRSTCTL   0x200f0000  /* From md, no use by AP directly */
#define L1_BASE_ADDR_L1RGU   0x26010000  /* From md, no use by AP directly  */
#define MD_RGU_BASE          (BASE_ADDR_MDRSTCTL + 0x100)  /* AP use */
#define L1_RGU_BASE          L1_BASE_ADDR_L1RGU    /* AP use */

/* MD1 PLL */
#define MDTOP_PLLMIXED_BASE		(0x20140000)
#define MDTOP_PLLMIXED_LENGTH	(0x1000)
#define MDTOP_CLKSW_BASE			(0x20150000)
#define MDTOP_CLKSW_LENGTH	(0x1000)

#define MD_PERI_MISC_BASE			(0x20060000)
#define MD_PERI_MISC_LEN   0xD0
#define MDL1A0_BASE				(0x260F0000)
#define MDL1A0_LEN  0x200

#define MDSYS_CLKCTL_BASE			(0x20120000)
#define MDSYS_CLKCTL_LEN   0xD0
/*#define L1_BASE_MADDR_MDL1_CONF	(0x260F0000)*/

/* MD Exception dump register list start[ */
#define MD1_OPEN_DEBUG_APB_CLK		(0x10006000)
/* PC Monitor */
#define MD_PC_MONITOR_BASE		(0x0D0D9000)
#define MD_PC_MONITOR_LEN		(0x1000)

 /* PLL reg (clock control) */
 /** MD CLKSW **/
 #define MD_CLKSW_BASE			(0x0D0D6000)
#define MD_CLKSW_LEN  0xF08

 /** MD PLLMIXED **/
#define MD_PLL_MIXED_BASE	(0x0D0D4000)
#define MD_PLL_MIXED_LEN	(0xF14)

 /** MD CLKCTL **/
#define MD_CLKCTL_BASE			(0x0D0C3800)
#define MD_CLKCTL_LEN			0x130

 /** MD GLOBALCON **/
#define MD_GLOBALCON_BASE		(0x0D0D5000)
#define MD_GLOBALCON_LEN		0x1000

 /* BUS reg */
#define MD_BUS_REG_BASE0		(0x0D0C2000)/* mdmcu_misc_reg */
#define MD_BUS_REG_LEN0			0x100
#define MD_BUS_REG_BASE1		(0x0D0C7000)/* mdinfra_misc_reg */
#define MD_BUS_REG_LEN1			0xAC
#define MD_BUS_REG_BASE2		(0x0D0C9000)/* cm2_misc */
#define MD_BUS_REG_LEN2			0xAC
#define MD_BUS_REG_BASE3		(0x0D0E0000)/* modeml1_ao_config */
#define MD_BUS_REG_LEN3			0x6C
 /* BUSREC */
#define MD_MCU_MO_BUSREC_BASE		(0x0D0C6000)
#define MD_MCU_MO_BUSREC_LEN		0x1000

#define MD_INFRA_BUSREC_BASE		(0x0D0C8000)
#define MD_INFRA_BUSREC_LEN		0x1000

#define MD_BUSREC_LAY_BASE		(0x0D0C2500)
#define MD_BUSREC_LAY_LEN		0x8

/* ECT */
/* MD ECT triggerIn/Out status */
#define MD_ECT_REG_BASE0		(0x0D0CC130)
#define MD_ECT_REG_LEN0			0x8
/* ModemSys ECT triggerIn/Out status */
#define MD_ECT_REG_BASE1		(0x0D0CD130)
#define MD_ECT_REG_LEN1			0x8
/* MD32 ECT status */
#define MD_ECT_REG_BASE2		(0x0D0CE000)
#define MD_ECT_REG_LEN2			0x20
 /* TOPSM reg */
#define MD_TOPSM_REG_BASE		(0x0200D0000)
#define MD_TOPSM_REG_LEN		0x8E4
 /* MD RGU reg */
#define MD_RGU_REG_BASE		(0x0200F0100)
#define MD_RGU_REG_LEN			0x400
 /* OST status */
 #define MD_OST_STATUS_BASE		0x200E0000
#define MD_OST_STATUS_LEN		0x300
 /* CSC reg */
 #define MD_CSC_REG_BASE			0x20100000
#define MD_CSC_REG_LEN		0x214
/* ELM reg */
#define MD_ELM_REG_BASE			0x20350000
#define MD_ELM_REG_LEN		0x480

/*MD bootup register*/
#define MD1_CFG_BASE (0x1020E300)
#define MD1_CFG_BOOT_STATS0 (MD1_CFG_BASE+0x00)
#define MD1_CFG_BOOT_STATS1 (MD1_CFG_BASE+0x04)

/* MD Exception dump register list end] */

#define MD_SRAM_PD_PSMCUSYS_SRAM_BASE	(0x200D0000)
#define MD_SRAM_PD_PSMCUSYS_SRAM_LEN	(0xB00)

/*
 * ============================================================
 *  Modem 3 part
 * ============================================================
 * need modify, haow
 */
#define MD3_BOOT_VECTOR 0x30190000
#define MD3_BOOT_VECTOR_KEY 0x3019379C
#define MD3_BOOT_VECTOR_EN 0x30195488

#define MD3_BOOT_VECTOR_VALUE		0x00000000
#define MD3_BOOT_VECTOR_KEY_VALUE	0x3567C766
#define MD3_BOOT_VECTOR_EN_VALUE	0xA3B66175

#define MD3_RGU_BASE			0x3A001080
#define APCCIF1_SRAM_SIZE		512

#endif				/* __MODEM_REG_BASE_H__ */

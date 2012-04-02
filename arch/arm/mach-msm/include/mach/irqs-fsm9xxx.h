/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ASM_ARCH_MSM_IRQS_FSM9XXX_H
#define __ASM_ARCH_MSM_IRQS_FSM9XXX_H

/* MSM ACPU Interrupt Numbers */

#define INT_DEBUG_TIMER_EXP	0
#define INT_GPT0_TIMER_EXP	1
#define INT_GPT1_TIMER_EXP	2
#define INT_WDT0_ACCSCSSBARK	3
#define INT_WDT1_ACCSCSSBARK	4
#define INT_AVS_SVIC		5
#define INT_AVS_SVIC_SW_DONE	6
#define INT_SC_DBG_RX_FULL	7
#define INT_SC_DBG_TX_EMPTY	8
#define INT_ARMQC_PERFMON	9
#define INT_AVS_REQ_DOWN	10
#define INT_AVS_REQ_UP		11
#define INT_SC_ACG		12
/* SCSS_VICFIQSTS0[13:15] are RESERVED */
#define INT_BPU_CPU		16
#define INT_L2_SVICDMANSIRPTREQ 17
#define INT_L2_SVICDMASIRPTREQ  18
#define INT_L2_SVICSLVIRPTREQ	19
#define INT_SEAWOLF_IRQ0	20
#define INT_SEAWOLF_IRQ1	21
#define INT_SEAWOLF_IRQ2	22
#define INT_SEAWOLF_IRQ3	23
#define INT_CARIBE_SUPSS_IRQ	24
#define INT_ADM_SEC0_IRQ	25
/* SCSS_VICFIQSTS0[26] is RESERVED */
#define INT_GMII_PHY		27
#define INT_SBD_IRQ		28
#define INT_HH_SUPSS_IRQ	29
#define INT_EMAC_SBD_IRQ	30
#define INT_PERPH_SUPSS_IRQ	31

#define INT_Q6_SW_IRQ_0		(32 + 0)
#define INT_Q6_SW_IRQ_1		(32 + 1)
#define INT_Q6_SW_IRQ_2		(32 + 2)
#define INT_Q6_SW_IRQ_3		(32 + 3)
#define INT_Q6_SW_IRQ_4		(32 + 4)
#define INT_Q6_SW_IRQ_5		(32 + 5)
#define INT_Q6_SW_IRQ_6		(32 + 6)
#define INT_Q6_SW_IRQ_7		(32 + 7)
#define INT_IMEM_IRQ		(32 + 8)
#define INT_IMEM_ECC_IRQ	(32 + 9)
#define INT_HSDDRX_IRQ		(32 + 10)
#define INT_BUFMEM_XPU_IRQ	(32 + 11)
#define INT_A9_M2A_0		(32 + 12)
#define INT_A9_M2A_1		(32 + 13)
#define INT_A9_M2A_2		(32 + 14)
#define INT_A9_M2A_3		(32 + 15)
#define INT_A9_M2A_4		(32 + 16)
#define INT_A9_M2A_5		(32 + 17)
#define INT_A9_M2A_6		(32 + 18)
#define INT_A9_M2A_7		(32 + 19)
#define INT_SC_PRI_IRQ		(32 + 20)
#define INT_SC_SEC_IRQ		(32 + 21)
#define INT_Q6_WDOG_IRQ		(32 + 22)
#define INT_ADM_SEC3_IRQ	(32 + 23)
#define INT_ARM_WAKE_IRQ	(32 + 24)
#define INT_ARM_WDOG_IRQ	(32 + 25)
#define INT_SUPSS_CFG_XPU_IRQ	(32 + 26)
#define INT_SPB_XPU_IRQ		(32 + 27)
#define INT_FPB_XPU_IRQ		(32 + 28)
#define INT_Q6_XPU_IRQ		(32 + 29)
/* SCSS_VICFIQSTS1[30:31] are RESERVED */
/* SCSS_VICFIQSTS2[0:31] are RESERVED */
/* SCSS_VICFIQSTS3[0:31] are RESERVED */

/* Retrofit universal macro names */
#define INT_ADM_AARM		INT_ADM_SEC3_IRQ
#define INT_GP_TIMER_EXP	INT_GPT0_TIMER_EXP
#define INT_ADSP_A11		INT_Q6_SW_IRQ_0
#define INT_ADSP_A11_SMSM	INT_ADSP_A11
#define INT_SIRC_0		INT_PERPH_SUPSS_IRQ
#define WDT0_ACCSCSSNBARK_INT	INT_WDT0_ACCSCSSBARK

#define NR_MSM_IRQS		128
#define NR_GPIO_IRQS		0
#define PMIC8058_IRQ_BASE	(NR_MSM_IRQS + NR_GPIO_IRQS + NR_SIRC_IRQS)
#define NR_PMIC8058_IRQS	256
#define NR_BOARD_IRQS		(NR_SIRC_IRQS + NR_PMIC8058_IRQS)

#define NR_MSM_GPIOS		168

#endif /* __ASM_ARCH_MSM_IRQS_FSM9XXX_H */

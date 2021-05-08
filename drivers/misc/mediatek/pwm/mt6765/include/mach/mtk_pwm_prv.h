/******************************************************************************
 * mtk_pwm_prv.h PWM Drvier
 *
 * Copyright (c) 2016, Media Teck.inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public Licence,
 * version 2, as publish by the Free Software Foundation.
 *
 * This program is distributed and in hope it will be useful, but WITHOUT
 * ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *
 ******************************************************************************
 */

#ifndef __MT_PWM_PRV_H__
#define __MT_PWM_PRV_H__

#ifdef CONFIG_OF
extern void __iomem *pwm_base;
extern void __iomem *pwm_infracfg_base;

#undef PWM_BASE
#define PWM_BASE pwm_base
#define PWM_INFRACFG_BASE pwm_infracfg_base
#endif

/* This variable is for pwm hw new change.
 * 1. change 8G DRAM enable from PERICFG domain to PWM internal register
 * 2. change 26M clock source to use INFRA domain control
 */
#define PWM_HW_V_1_0
/***********************************
 * PWM register address
 ************************************/
#define PWM_ENABLE (PWM_BASE+0x0000)

#define PWM_3DLCM	(PWM_BASE+0x1D0)
#define PWM_3DLCM_ENABLE_OFFSET  0

#define PWM_INT_ENABLE (PWM_BASE+0x0200)
#define PWM_INT_STATUS (PWM_BASE+0x0204)
#define PWM_INT_ACK (PWM_BASE+0x0208)
#define PWM_EN_STATUS (PWM_BASE+0x020c)

#define PWM_CK_26M_SEL (PWM_BASE+0x0210)
#define PWM_CK_26M_SEL_OFFSET 0

#define PWM_CON_CLKDIV_MASK 0x00000007
#define PWM_CON_CLKDIV_OFFSET 0
#define PWM_CON_CLKSEL_MASK 0x00000008
#define PWM_CON_CLKSEL_OFFSET 3
#define PWM_CON_CLKSEL_OLD_MASK 0x00000010
#define PWM_CON_CLKSEL_OLD_OFFSET 4

#define PWM_CON_SRCSEL_MASK 0x00000020
#define PWM_CON_SRCSEL_OFFSET 5

#define PWM_CON_MODE_MASK 0x00000040
#define PWM_CON_MODE_OFFSET 6

#define PWM_CON_IDLE_VALUE_MASK 0x00000080
#define PWM_CON_IDLE_VALUE_OFFSET 7

#define PWM_CON_GUARD_VALUE_MASK 0x00000100
#define PWM_CON_GUARD_VALUE_OFFSET 8

#define PWM_CON_STOP_BITS_MASK 0x00007E00
#define PWM_CON_STOP_BITS_OFFSET 9
#define PWM_CON_OLD_MODE_MASK 0x00008000
#define PWM_CON_OLD_MODE_OFFSET 15

#define BLOCK_CLK     (66UL*1000*1000)
#define PWM_26M_CLK   (26UL*1000*1000)

/* PWM infracfg control register */
#define PWM_CLK_SRC_CTRL  (pwm_infracfg_base + 0x410)
#define PWM_BCLK_SW_CTRL_OFFSET     12

void mt_pwm_platform_init(void);
#endif

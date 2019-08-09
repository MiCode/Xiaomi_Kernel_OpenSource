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

/* Register address define */
#define ACCDET_BASE              0x00000002
#define TOP_RST_ACCDET           0x0114
#define TOP_RST_ACCDET_SET		 0x0116
#define TOP_RST_ACCDET_CLR		 0x0118

#define INT_CON_ACCDET           0x017E
#define INT_CON_ACCDET_SET		 0x0180	/* 6320 Design */
#define INT_CON_ACCDET_CLR       0x0182

#define INT_STATUS_ACCDET        0x0186

/* 6320 clock register */
#define TOP_CKPDN		         0x0102
#define TOP_CKPDN_SET            0x0104
#define TOP_CKPDN_CLR            0x0106

#define ACCDET_RSV               (ACCDET_BASE + 0x0582)

#define ACCDET_CTRL              (ACCDET_BASE + 0x0584)
#define ACCDET_STATE_SWCTRL      (ACCDET_BASE + 0x0586)
#define ACCDET_PWM_WIDTH		 (ACCDET_BASE + 0x0588)
#define ACCDET_PWM_THRESH		 (ACCDET_BASE + 0x058A)
#define ACCDET_EN_DELAY_NUM      (ACCDET_BASE + 0x058c)
#define ACCDET_DEBOUNCE0         (ACCDET_BASE + 0x058E)
#define ACCDET_DEBOUNCE1         (ACCDET_BASE + 0x0590)
#define ACCDET_DEBOUNCE2         (ACCDET_BASE + 0x0592)
#define ACCDET_DEBOUNCE3         (ACCDET_BASE + 0x0594)

#define ACCDET_DEFAULT_STATE_RG  (ACCDET_BASE + 0x0596)


#define ACCDET_IRQ_STS           (ACCDET_BASE + 0x0598)

#define ACCDET_CONTROL_RG        (ACCDET_BASE + 0x059A)
#define ACCDET_STATE_RG          (ACCDET_BASE + 0x059C)

#define ACCDET_CUR_DEB			 (ACCDET_BASE + 0x059E)
#define ACCDET_RSV_CON0			 (ACCDET_BASE + 0x05A0)
#define ACCDET_RSV_CON1			 (ACCDET_BASE + 0x05A2)

/* Register value define */
#define ACCDET_CTRL_EN           (1<<0)
#define ACCDET_MIC_PWM_IDLE      (1<<6)
#define ACCDET_VTH_PWM_IDLE      (1<<5)
#define ACCDET_CMP_PWM_IDLE      (1<<4)
#define ACCDET_CMP_EN            (1<<0)
#define ACCDET_VTH_EN            (1<<1)
#define ACCDET_MICBIA_EN         (1<<2)

#define ACCDET_ENABLE			 (1<<0)
#define ACCDET_DISABLE			 (0<<0)

#define ACCDET_RESET_SET         (1<<4)
#define ACCDET_RESET_CLR         (1<<4)

#define IRQ_CLR_BIT              0x100
#define IRQ_STATUS_BIT           (1<<0)

#define RG_ACCDET_IRQ_SET        (1<<2)
#define RG_ACCDET_IRQ_CLR        (1<<2)
#define RG_ACCDET_IRQ_STATUS_CLR (1<<2)

/* CLOCK */
#define RG_ACCDET_CLK_SET        (1<<14)
#define RG_ACCDET_CLK_CLR        (1<<14)


#define ACCDET_PWM_EN_SW         (1<<15)
#define ACCDET_MIC_EN_SW         (1<<15)
#define ACCDET_VTH_EN_SW         (1<<15)
#define ACCDET_CMP_EN_SW         (1<<15)

#define ACCDET_SWCTRL_EN         0x07

#define ACCDET_IN_SW             0x10

#define ACCDET_PWM_SEL_CMP       0x00
#define ACCDET_PWM_SEL_VTH       0x01
#define ACCDET_PWM_SEL_MIC       0x10
#define ACCDET_PWM_SEL_SW        0x11
#define ACCDET_SWCTRL_IDLE_EN    (0x07<<4)

#define ACCDET_TEST_MODE5_ACCDET_IN_GPI       (1<<5)
#define ACCDET_TEST_MODE4_ACCDET_IN_SW        (1<<4)
#define ACCDET_TEST_MODE3_MIC_SW              (1<<3)
#define ACCDET_TEST_MODE2_VTH_SW              (1<<2)
#define ACCDET_TEST_MODE1_CMP_SW              (1<<1)
#define ACCDET_TEST_MODE0_GPI                 (1<<0)

#define ACCDET_1V9_MODE_ON        0x10B0
#define ACCDET_1V9_MODE_OFF       0x1090
#define ACCDET_2V8_MODE_ON        0x01
#define ACCDET_2V8_MODE_OFF       0x5090

/* #define ACCDET_DEFVAL_SEL        (1<<15) */

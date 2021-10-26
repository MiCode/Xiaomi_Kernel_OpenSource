/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _REG_ACCDET_H_
#define _REG_ACCDET_H_

/* Register Address Define */
#define ACCDET_BASE		(0x00000000)
#define PMIC_REG_BASE_START	(0x0000)
#define PMIC_REG_BASE_END	(0x2540)


/*-------Register_TOP_REG----------------------*/
/* 32k clock  bit15 */
#define TOP_CKPDN_CON0		((unsigned int)(ACCDET_BASE + 0x010C))
#define TOP_CKPDN_CON0_SET	((unsigned int)(ACCDET_BASE + 0x010E))
#define TOP_CKPDN_CON0_CLR	((unsigned int)(ACCDET_BASE + 0x0110))

/*-------Register_AUXADC_REG----------------------*/
#define AUXADC_RQST0		((unsigned int)(ACCDET_BASE + 0x1108))
#define AUXADC_RQST0_SET	((unsigned int)(ACCDET_BASE + 0x1108))
#define AUXADC_RQST0_CLR	((unsigned int)(ACCDET_BASE + 0x1108))
#define AUXADC_ACCDET		((unsigned int)(ACCDET_BASE + 0x11B8))

/*--------------Register_AUD_REG--------------------------*/
/* Accdet clk&reset&int all in AUD_REG domain */

#define AUD_TOP_CKPDN_CON0	((unsigned int)(ACCDET_BASE + 0x220C))
#define AUD_TOP_RST_CON0	((unsigned int)(ACCDET_BASE + 0x2220))
#define AUD_TOP_INT_CON0	((unsigned int)(ACCDET_BASE + 0x2228))
#define AUD_TOP_INT_CON0_SET	((unsigned int)(ACCDET_BASE + 0x222A))
#define AUD_TOP_INT_CON0_CLR	((unsigned int)(ACCDET_BASE + 0x222C))

#define AUD_TOP_INT_MASK_CON0	((unsigned int)(ACCDET_BASE + 0x222E))
#define AUD_TOP_INT_MASK_CON0_SET	((unsigned int)(ACCDET_BASE + 0x2230))
#define AUD_TOP_INT_MASK_CON0_CLR	((unsigned int)(ACCDET_BASE + 0x2232))
#define AUD_TOP_INT_STATUS0	((unsigned int)(ACCDET_BASE + 0x2234))

/* analog RG */
/* ACCDET_FAST_DISCHARGE_REG */
#define AUDENC_ANA_CON6		((unsigned int)(ACCDET_BASE + 0x2394))
/* ACCDET_MOISTURE_REG */
#define AUDENC_ANA_CON9		((unsigned int)(ACCDET_BASE + 0x239A))
/* ACCDET_MICBIAS_REG */
#define AUDENC_ANA_CON10	((unsigned int)(ACCDET_BASE + 0x239C))
/* ACCDET_ADC_REG */
#define AUDENC_ANA_CON11	((unsigned int)(ACCDET_BASE + 0x239E))

/*--------------ACCDET_REG-------------------------------*/
#define ACCDET_RSV		((unsigned int)(ACCDET_BASE + 0x2508))
#define ACCDET_CTRL		((unsigned int)(ACCDET_BASE + 0x250A))
#define ACCDET_STATE_SWCTRL	((unsigned int)(ACCDET_BASE + 0x250C))
#define ACCDET_PWM_WIDTH	((unsigned int)(ACCDET_BASE + 0x250E))
#define ACCDET_PWM_THRESH	((unsigned int)(ACCDET_BASE + 0x2510))
#define ACCDET_EN_DELAY_NUM	((unsigned int)(ACCDET_BASE + 0x2512))
#define ACCDET_DEBOUNCE0	((unsigned int)(ACCDET_BASE + 0x2514))
#define ACCDET_DEBOUNCE1	((unsigned int)(ACCDET_BASE + 0x2516))
#define ACCDET_DEBOUNCE2	((unsigned int)(ACCDET_BASE + 0x2518))
#define ACCDET_DEBOUNCE3	((unsigned int)(ACCDET_BASE + 0x251A))
#define ACCDET_DEBOUNCE4	((unsigned int)(ACCDET_BASE + 0x251C))
#define ACCDET_DEFAULT_STATE_RG	((unsigned int)(ACCDET_BASE + 0x251E))
#define ACCDET_IRQ_STS		((unsigned int)(ACCDET_BASE + 0x2520))
#define ACCDET_CONTROL_RG	((unsigned int)(ACCDET_BASE + 0x2522))
#define ACCDET_STATE_RG		((unsigned int)(ACCDET_BASE + 0x2524))
#define ACCDET_EINT0_CTL	((unsigned int)(ACCDET_BASE + 0x2526))
#define ACCDET_EINT0_PWM_DELAY	((unsigned int)(ACCDET_BASE + 0x2528))
#define ACCDET_TEST_DEBUG	((unsigned int)(ACCDET_BASE + 0x252A))
#define ACCDET_EINT0_STATE	((unsigned int)(ACCDET_BASE + 0x252C))
#define ACCDET_CUR_DEB		((unsigned int)(ACCDET_BASE + 0x252E))
#define ACCDET_EINT0_CUR_DEB	((unsigned int)(ACCDET_BASE + 0x2530))
#define ACCDET_RSV_CON0		((unsigned int)(ACCDET_BASE + 0x2532))
#define ACCDET_RSV_CON1		((unsigned int)(ACCDET_BASE + 0x2534))
#define ACCDET_AUXADC_CON_TIME	((unsigned int)(ACCDET_BASE + 0x2536))
#define ACCDET_HW_MODE_DFF	((unsigned int)(ACCDET_BASE + 0x2538))
#define ACCDET_EINT1_CTL	((unsigned int)(ACCDET_BASE + 0x253A))
#define ACCDET_EINT1_PWM_DELAY	((unsigned int)(ACCDET_BASE + 0x253C))
#define ACCDET_EINT1_STATE	((unsigned int)(ACCDET_BASE + 0x253E))
#define ACCDET_EINT1_CUR_DEB	((unsigned int)(ACCDET_BASE + 0x2540))


/*************Register Bit Define*************/
/* AUD_TOP_CKPDN_CON0:  bit0, 1,power-on;0,power-off  */
#define RG_ACCDET_CK_PDN_B0	(0x01<<0)

/* AUD_TOP_RST_CON0:   bit1, 1,reset;0,normal */
#define RG_ACCDET_RST_B1	(0x01<<1)

/* AUD_TOP_INT_CON0: accdet_int: bit5 int;bit6,eint0;bit7,eint1 */
#define RG_INT_EN_ACCDET_B5		(0x01<<5)
#define RG_INT_EN_ACCDET_EINT0_B6	(0x01<<6)
#define RG_INT_EN_ACCDET_EINT1_B7	(0x01<<7)
#define RG_INT_EN_ACCDET_EINT_B6_7	(0x03<<6)
#define ACCDET_EINT1_IVAL_SEL_B13		(0x2000)
#define ACCDET_EINT0_IVAL_SEL_B14		(0x4000)
#define ACCDET_EINT1_IVAL_SEL_B15		(0x8000)

/* AUD_TOP_INT_MASK_CON0:bit5,int;bit6,eint0;bit7,eint1 */
#define RG_INT_MASK_ACCDET_B5		(0x01<<5)
#define RG_INT_MASK_ACCDET_EINT0_B6	(0x01<<6)
#define RG_INT_MASK_ACCDET_EINT1_B7	(0x01<<7)
#define RG_INT_EMASK_ACCDET_EINT_B6_7	(0x03<<6)

/* AUD_TOP_INT_STATUS0: accdet_int issue: bit5,int;bit6,eint0;bit7,eint1 */
#define RG_INT_STATUS_ACCDET_B5		(0x01<<5)
#define RG_INT_STATUS_ACCDET_EINT0_B6	(0x01<<6)
#define RG_INT_STATUS_ACCDET_EINT1_B7	(0x01<<7)
#define RG_INT_STATUS_ACCDET_EINT_B6_7	(0x03<<6)

/* AUDENC_ANA_CON6:  analog fast discharge*/
#define RG_AUDSPARE			(0x00A0)
#define RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN	(1<<5)
#define RG_AUDSPARE_FSTDSCHRG_IMPR_EN		(1<<6)

/* AUDENC_ANA_CON10: */
#define RG_AUDMICBIAS1_DCSW1PEN		(0x01<<8)
#define RG_AUD_MICBIAS1_LOWP_EN		(1<<2)

/* AUDENC_ANA_CON11: */
#define RG_ACCDET_MODE_ANA11_MODE1	(0x0807)
#define RG_ACCDET_MODE_ANA11_MODE2	(0x0887)
#define RG_ACCDET_MODE_ANA11_MODE6	(0x0887)

/* ------Register_AUXADC_REG  Bit Define------ */
/* AUXADC_ADC5:  Auxadc CH5 read data */
#define AUXADC_DATA_RDY_CH5		(1<<15)
#define AUXADC_DATA_PROCEED_CH5		(0<<15)
#define AUXADC_DATA_MASK		(0x0FFF)

/* AUXADC_RQST0_SET:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_SET		(1<<5)
/* AUXADC_RQST0_CLR:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_CLR		(1<<5)

/* AUXADC_ACCDET: :ACCDET auto request enable/disable*/
#define AUXADC_ACCDET_AUTO_SPL_EN	(0x01<<0)
#define AUXADC_ACCDET_AUTO_SPL_DISEN	(0x00<<0)
#define AUXADC_ACCDET_AUTO_RQST_CLR	(0x01<<1)
#define AUXADC_ACCDET_AUTO_RQST_NONE	(0x00<<1)

/* -----Register_EFUSE_REG  Bit Define-------- */
#define ACCDET_CALI_MASK0		(0xFF)
#define ACCDET_CALI_MASK1		(0xFF<<8)
#define ACCDET_CALI_MASK2		(0xFF)
#define ACCDET_CALI_MASK3		(0xFF<<8)
#define ACCDET_CALI_MASK4		(0xFF)

/* -----Register_ACCDET_REG  Bit Define------- */
/* ACCDET_CON0,
 * bit10: control connection between Analog and auxadc
 * bit11: 0,HW mode;1,SW mode
 * bit[14:13]: reserve
 */
#define AUD_ACCDET_AUXADC_SW_B10	(0x01<<10)
#define AUD_ACCDET_AUXADC_SW_SEL_B11	(0x01<<11)
#define RG_AUD_ACCDET_RSV_B13_14	(0x03<<13)

/* ACCDET_CON1,
 * bit0: ACCDET_EN, 1,enable;0,disable
 * bit1: ACCDET_SEQ_INIT, 1,initialized mode;0,normal mode
 * bit2: ACCDET_EINT0_EN, 1,enable;0,disable
 * bit3: ACCDET_EINT0_SEQ_EN, 1,enable;0,disable
 * bit4: ACCDET_EINT1_EN, 1,enable;0,disable
 * bit5: ACCDET_EINT1_SEQ_EN, 1,enable;0,disable
 * bit6: ACCDET_ANASWCTRL_SEL,
 * Control connection of adc and accdet, when 00, 1,accdet c;0,auxadc
 */
#define ACCDET_ENABLE_B0		(0x01<<0)
#define ACCDET_SEQ_INIT_EN_B1		(0x01<<1)
#define ACCDET_EINT0_EN_B2		(0x01<<2)
#define ACCDET_EINT0_SEQ_INIT_EN_B3	(0x01<<3)
#define ACCDET_EINT1_EN_B4		(0x01<<4)
#define ACCDET_EINT1_SEQ_INIT_EN_B5	(0x01<<5)
#define ACCDET_ANASWCTRL_SEL_B6		(0x01<<6)
#define ACCDET_EINT_EN_B2_4		(0x05<<2)
#define ACCDET_EINT_SEQ_INIT_EN_B3_5	(0x05<<3)

/* ACCDET_CON2,
 * bit0: ACCDET_CMP_PWM_EN, 1,enable;0,disable
 * bit1: ACCDET_VTH_PWM_EN, threshold: 1,enable;0,disable
 * bit2: ACCDET_MICBIAS_PWM_EN, 1,enable;0,disable
 * bit3: ACCDET_EINT0_PWM_EN, 1,enable;0,disable
 * bit4: ACCDET_EINT1_PWM_EN, 1,enable;0,disable
 * bit8: ACCDET_CMP_PWM_IDLE, 1,high;0,low
 * bit9: ACCDET_VTH_PWM_IDLE, threshold:  1,high;0,low
 * bit10: ACCDET_MICBIAS_PWM_IDLE, 1,high;0,low
 * bit11: ACCDET_EINT0_PWM_IDLE,  1,high;0,low
 * bit12: ACCDET_EINT1_PWM_IDLE,  1,high;0,low
 */
#define ACCDET_EINT1_PWM_IDLE_B12	(0x1<<12)
#define ACCDET_EINT0_PWM_IDLE_B11	(0x1<<11)
#define ACCDET_EINT_PWM_IDLE_B11_12	(0x3<<11)
#define ACCDET_MBIAS_PWM_IDLE_B10	(0x01<<10)
#define ACCDET_VTH_PWM_IDLE_B9		(0x01<<9)
#define ACCDET_CMP_PWM_IDLE_B8		(0x01<<8)
#define ACCDET_PWM_IDLE			(0x07<<8)
#define ACCDET_EINT1_PWM_EN_B4		(0x01<<4)
#define ACCDET_EINT0_PWM_EN_B3		(0x01<<3)
#define ACCDET_MICBIAS_PWM_EN_B2	(0x01<<2)
#define ACCDET_VTH_PWM_EN_B1		(0x01<<1)
#define ACCDET_CMP_PWM_EN_B0		(0x01<<0)
#define ACCDET_PWM_EN			(0x07)
#define ACCDET_EINT_PWM_EN_B3_4		(0x03<<3)

/* ACCDET_CON3-CON5, set ACCDET PWM width, thresh, rise/falling */
/* ACCDET_CON6-CON10, set debounce[0-4].  deb/freq=(deb/32768) s */

/* ACCDET_CON11
 * bit[0:11]: set default value of MEM,CUR,SAM, etc.
 * set default value of accdet eint if use high_level trigger
 * bit13: ACCDET_IVAL_SEL, 1,from RG;0,status=2'b11
 * bit14: ACCDET_EINT0_IVAL_SEL, 1,from RG;0,status=1'b1
 * bit15: ACCDET_EINT1_IVAL_SEL, 1,from RG;0,status=1'b1
 */
#define ACCDET_IVAL_B0_1_4_5_8_9	(0x0333)
#define ACCDET_EINT0_IVAL_B2_6_10	(0x0444)
#define ACCDET_EINT1_IVAL_B3_7_11	(0x0888)
#define ACCDET_IVAL_SEL_B13		(0x2000)
#define ACCDET_EINT0_IVAL_SEL		(0x4000)
#define ACCDET_EINT1_IVAL_SEL		(0x8000)
#define ACCDET_EINT_IVAL (ACCDET_EINT0_IVAL_B2_6_10|ACCDET_EINT1_IVAL_B3_7_11)
#define ACCDET_EINT_IVAL_SEL	(ACCDET_EINT0_IVAL_SEL | ACCDET_EINT1_IVAL_SEL)

/* ACCDET_CON12, ACCDET(AB) interrupt status
 * bit15: ACCDET_EINT1_IRQ_POLARITY, 1,Rising edge;0,Falling edge
 * bit14: ACCDET_EINT0_IRQ_POLARITY, 1,Rising edge;0,Falling edge
 */
#define ACCDET_EINT1_IRQ_POL_B15	(0x01<<15)
#define ACCDET_EINT0_IRQ_POL_B14	(0x01<<14)
#define ACCDET_EINT_IRQ_POL_B14_15	(0x03<<14)

#define ACCDET_EINT1_IRQ_CLR_B11	(0x01<<11)
#define ACCDET_EINT0_IRQ_CLR_B10	(0x01<<10)
#define ACCDET_EINT_IRQ_CLR_B10_11	(0x03<<10)
#define ACCDET_IRQ_CLR_B8		(0x01<<8)

#define ACCDET_EINT1_IRQ_B3		(0x01<<3)
#define ACCDET_EINT0_IRQ_B2		(0x01<<2)
#define ACCDET_EINT_IRQ_B2_B3		(0x03<<2)
#define ACCDET_IRQ_B0			(0x01<<0)
#define ACCDET_IRQ_STS_BIT_ALL		(0x0D)

/* ACCDET_CON13,:  accdet pwm,cmp,mbias,SW selection, etc. */
#define ACCDET_CMP0_SWSEL		(1<<1)
#define ACCDET_VTH_SWSEL		(1<<2)
#define ACCDET_MBIAS_SWSEL		(1<<3)
#define ACCDET_CMP0_EN_SW		(1<<12)
#define ACCDET_VTH_EN_SW		(1<<13)
#define ACCDET_MBIAS_EN_SW		(1<<14)
#define ACCDET_PWM_EN_SW		(1<<15)

/* ACCDET_CON14: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET	(0x06)
#define ACCDET_STATE_AB_MASK		(0x03)
#define ACCDET_STATE_AB_00		(0x00)
#define ACCDET_STATE_AB_01		(0x01)
#define ACCDET_STATE_AB_10		(0x02)
#define ACCDET_STATE_AB_11		(0x03)

/* ACCDET_CON15: accdet eint0 debounce, PWM width&thresh, etc.
 * bit0: ACCDET_EINT0_DEB_SEL, 1,debounce_multi_sync_path;0,from register
 */
#define ACCDET_EINT0_DEB_SEL		(0x01<<0)
/* 0ms */
#define ACCDET_EINT0_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT0_DEB_OUT_012	(0x01<<3)
/* 32ms */
#define ACCDET_EINT0_DEB_IN_32		(0x0A<<3)
/* 64ms */
#define ACCDET_EINT0_DEB_IN_64		(0x0C<<3)
/* 256ms */
#define ACCDET_EINT0_DEB_IN_256		(0x0E<<3)
/* 512ms */
#define ACCDET_EINT0_DEB_512		(0x0F<<3)
#define ACCDET_EINT0_DEB_CLR		(0x0F<<3)
#define ACCDET_EINT0_PWM_THRSH_MASK	(0x07<<8)
#define ACCDET_EINT0_PWM_WIDTH_MASK	(0x03<<12)

/* 16ms */
#define ACCDET_EINT0_PWM_THRSH		(0x06<<8)
/* 16ms */
#define ACCDET_EINT0_PWM_WIDTH		(0x02<<12)

/* ACCDET_CON16, accdet eint0 PWM rise/falling set */

/* ACCDET_CON17,
 * accdet eint CMP,AUXADC,etc. switch
 * accdet test mode
 */
#define ACCDET_EINT0_CMPOUR_SW_B7	(0x01<<7)
#define ACCDET_EINT1_CMPOUT_SW_B8	(0x01<<8)
#define ACCDET_AUXADC_CTRL_SW_B11	(0x01<<11)
#define ACCDET_EINT0_CMP_EN_SW_B14	(0x01<<14)
#define ACCDET_EINT1_CMP_EN_SW_B15	(0x01<<15)

/* ACCDET_CON18, RO. accdet_eint0 FSM state, etc. */
/* ACCDET_CON19, RO. accdet current used debounce time */
/* ACCDET_CON20, RO. accdet_eint0 current used debounce time */

/* ACCDET_CON21, accdet monitor flag select */
#define ACCDET_MON_FLAG_SEL_B4_B11	(0xFF<<4)
#define ACCDET_MON_FLAG_EN_B0		(0x01<<0)


/* ACCDET_CON22, reserve */
/* ACCDET_CON23, auxadc to accdet connect time (time/32K) */

/* ACCDET_CON24, SW/HW path & EINT reverse.
 * 0x00--> HW mode is triggered by eint0
 * 0x01--> HW mode is triggered by eint1
 * 0x02--> HW mode is triggered by eint0 or eint1
 * 0x03--> HW mode is triggered by eint0 and eint1
 */
#define ACCDET_HWEN_SEL_0		(0x00)
#define ACCDET_HWEN_SEL_1		(0x01)
#define ACCDET_HWEN_SEL_0_OR_1		(0x02)
#define ACCDET_HWEN_SEL_0_AND_1		(0x03)

#define ACCDET_HWMODE_SEL		(0x01<<2)
#define ACCDET_EINT_DEB_OUT_DFF		(0x01<<3)
#define ACCDET_FAST_DISCAHRGE		(0x01<<4)
#define ACCDET_FAST_DISCAHRGE_EN	(0xC01C)
#define ACCDET_FAST_DISCAHRGE_DIS	(0xC00C)
#define ACCDET_FAST_DISCAHRGE_REVISE	(0x000C)
#define ACCDET_EINIT0_REVERSE		(0x01<<14)
#define ACCDET_EINIT1_REVERSE		(0x01<<15)
#define ACCDET_EINIT_REVERSE		(0x03<<14)


/* ACCDET_CON25,: accdet eint0 debounce, PWM width&thresh, etc. set
 * bit0: ACCDET_EINT1_DEB_SEL, 1,debounce_multi_sync_path;0,from register
 */
#define ACCDET_EINT1_DEB_SEL		(0x01<<0)
/* 0ms */
#define ACCDET_EINT1_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT1_DEB_OUT_012	(0x01<<3)
/* 32ms */
#define ACCDET_EINT1_DEB_IN_32		(0x0A<<3)
/* 64ms */
#define ACCDET_EINT1_DEB_IN_64		(0x0C<<3)
/* 256ms */
#define ACCDET_EINT1_DEB_IN_256		(0x0E<<3)
/* 512ms */
#define ACCDET_EINT1_DEB_512		(0x0F<<3)
#define ACCDET_EINT1_DEB_CLR		(0x0F<<3)
#define ACCDET_EINT1_PWM_THRSH_MASK	(0x07<<8)
#define ACCDET_EINT1_PWM_WIDTH_MASK	(0x03<<12)

/* 16ms */
#define ACCDET_EINT1_PWM_THRSH		(0x06<<8)
/* 16ms */
#define ACCDET_EINT1_PWM_WIDTH		(0x02<<12)

/* ACCDET_CON26, accdet eint1 PWM rise/falling set */
/* ACCDET_CON27, RO. accdet_eint FSM state, etc. */
/* ACCDET_CON28, RO. accdet_eint1 current used debounce time */

#endif/* end _REG_ACCDET_H_ */

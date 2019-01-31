/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* Register address define */
#define ACCDET_BASE		(0x00000000)
#define PMIC_REG_BASE_START	(0x0000)
#define PMIC_REG_BASE_END	(0x380A)

/*-------Register_TOP_REG----------------------*/
 /* clock register */
#define TOP_CKPDN		((unsigned int)(ACCDET_BASE+0x0412))
#define TOP_CKPDN_SET		((unsigned int)(ACCDET_BASE+0x0414))
#define TOP_CKPDN_CLR		((unsigned int)(ACCDET_BASE+0x0416))

#define TOP_RST_ACCDET		((unsigned int)(ACCDET_BASE+0x0600))
#define TOP_RST_ACCDET_SET	((unsigned int)(ACCDET_BASE+0x0602))
#define TOP_RST_ACCDET_CLR	((unsigned int)(ACCDET_BASE+0x0604))

#define INT_CON_ACCDET		((unsigned int)(ACCDET_BASE+0x0824))
#define INT_CON_ACCDET_SET	((unsigned int)(ACCDET_BASE+0x0826))
#define INT_CON_ACCDET_CLR	((unsigned int)(ACCDET_BASE+0x0828))

#define INT_STATUS_ACCDET	((unsigned int)(ACCDET_BASE+0x0860))

 /*--------------ACCDET_REG-------------------------------*/
 /*  accdet register */
#define ACCDET_RSV		((unsigned int)(ACCDET_BASE+0x1C00))
#define ACCDET_CTRL		((unsigned int)(ACCDET_BASE+0x1C02))
#define ACCDET_STATE_SWCTRL	((unsigned int)(ACCDET_BASE+0x1C04))
#define ACCDET_PWM_WIDTH	((unsigned int)(ACCDET_BASE+0x1C06))
#define ACCDET_PWM_THRESH	((unsigned int)(ACCDET_BASE+0x1C08))
#define ACCDET_EN_DELAY_NUM	((unsigned int)(ACCDET_BASE+0x1C0A))
#define ACCDET_DEBOUNCE0	((unsigned int)(ACCDET_BASE+0x1C0C))
#define ACCDET_DEBOUNCE1	((unsigned int)(ACCDET_BASE+0x1C0E))
#define ACCDET_DEBOUNCE2	((unsigned int)(ACCDET_BASE+0x1C10))
#define ACCDET_DEBOUNCE3	((unsigned int)(ACCDET_BASE+0x1C12))
#define ACCDET_DEBOUNCE4	((unsigned int)(ACCDET_BASE+0x1C14))
#define ACCDET_DEFAULT_STATE_RG	((unsigned int)(ACCDET_BASE+0x1C16))
#define ACCDET_IRQ_STS		((unsigned int)(ACCDET_BASE+0x1C18))
#define ACCDET_CONTROL_RG	((unsigned int)(ACCDET_BASE+0x1C1A))
#define ACCDET_STATE_RG		((unsigned int)(ACCDET_BASE+0x1C1C))
#define ACCDET_EINT_CTL		((unsigned int)(ACCDET_BASE+0x1C1E))
#define ACCDET_EINT_PWM_DELAY	((unsigned int)(ACCDET_BASE+0x1C20))
#define ACCDET_TEST_DEBUG	((unsigned int)(ACCDET_BASE+0x1C22))
#define ACCDET_EINT_STATE	((unsigned int)(ACCDET_BASE+0x1C24))
#define ACCDET_CUR_DEB		((unsigned int)(ACCDET_BASE+0x1C26))
#define ACCDET_EINT_CUR_DEB	((unsigned int)(ACCDET_BASE+0x1C28))
/* Reserve */
#define ACCDET_RSV_CON0		((unsigned int)(ACCDET_BASE+0x1C2A))
#define ACCDET_RSV_CON1		((unsigned int)(ACCDET_BASE+0x1C2C))
#define ACCDET_AUXADC_CON_TIME	((unsigned int)(ACCDET_BASE+0x1C2E))
/* accdet eint HW mode */
#define ACCDET_HW_MODE_DFF	((unsigned int)(ACCDET_BASE+0x1C30))

/*--------------AUXADC_REG-------------------------------*/
#define ACCDET_AUXADC_CTL	((unsigned int)(ACCDET_BASE+0x32A0))
#define ACCDET_AUXADC_CTL_SET	((unsigned int)(ACCDET_BASE+0x32A2))
#define ACCDET_AUXADC_REG	((unsigned int)(ACCDET_BASE+0x320A))
#define ACCDET_AUXADC_AUTO_SPL	((unsigned int)(ACCDET_BASE+0x32F0))

/*--------------Register_AUD_REG--------------------------*/
#define ACCDET_ADC_REG		((unsigned int)(ACCDET_BASE+0x3636))
#define ACCDET_MICBIAS_REG	((unsigned int)(ACCDET_BASE+0x3634))

/*--------------Register_EFUSE_REG--------------------------*/
#define REG_ACCDET_AD_CALI_0	((unsigned int)(ACCDET_BASE+0x1E84))
#define REG_ACCDET_AD_CALI_1	((unsigned int)(ACCDET_BASE+0x1E86))
#define REG_ACCDET_AD_CALI_2	((unsigned int)(ACCDET_BASE+0x1E88))
#define REG_ACCDET_AD_CALI_3	((unsigned int)(ACCDET_BASE+0x1E8A))

/*-------------Bit Define-----------------------------------*/
/* clock, TOP_CKPDN_CON3 */
#define RG_ACCDET_CLK_SET        (1<<2)
#define RG_ACCDET_CLK_CLR        (1<<2)

/* RST, TOP_RST_ACCDET_SET */
#define ACCDET_RESET_SET          (1<<4)
#define ACCDET_RESET_CLR          (1<<4)

/* INT, INT_STATUS6 INT_CON_ACCDET_SET */
#define RG_ACCDET_IRQ_SET		(1<<7)
#define RG_ACCDET_IRQ_CLR		(1<<7)
#define RG_ACCDET_EINT_IRQ_SET		(1<<8)
#define RG_ACCDET_EINT_IRQ_CLR		(1<<8)
#define RG_INT_STATUS_ACCDET		(0x01<<7)
#define RG_INT_STATUS_ACCDET_EINT	(0x01<<8)

/* AUDENC_ANA_CON11 */
/* Mic bias1: 0,Normal mode; 1, Lowpower mode */
#define RG_AUDMICBIAS1LOWPEN		(0x01<<7)
/* Micbias1 voltage output: 000, 1.7V;010,1.9V;101,2.5V;111,2.7V */
#define RG_AUDMICBIAS1VREF		(0x07<<4)
/* 1,Mic bias1 DC couple switch 1P, for mode6 */
#define RG_AUDMICBIAS1DCSW1PEN		(0x01<<1)
/* 1,Mic bias1 DC couple switch 1N */
#define RG_AUDMICBIAS1DCSW1NEN		(0x01<<2)
#define RG_MICBIAS1DCSWPEN		(RG_AUDMICBIAS1DCSW1PEN)

 /* AUDENC_ANA_CON12 */
#define RG_AUDACCDETMICBIAS0PULLLOW	(0x01<<0)
/* Mic bias1: 0,Normal mode; 1, Lowpower mode */
#define RG_AUDACCDETMICBIAS1PULLLOW	(0x01<<1)
#define RG_AUDACCDETMICBIAS2PULLLOW	(0x01<<2)
#define RG_AUDACCDETVIN1PULLLOW		(0x01<<3)
/* Pull low Mic bias pads when MICBIAS is off */
#define RG_AUDACCDETPULLLOW		(0x0F)
#define RG_ACCDETSEL			(0x01<<8)
#define RG_EINTTHIRENB			(0x00<<4)
/* 1,enable Internal connection between ACCDET and EINT */
#define RG_EINTCONFIGACCDET		(0x01<<12)
#define RG_EINT_ANA_CONFIG		(RG_ACCDETSEL|RG_EINTCONFIGACCDET)


/* ACCDET_CON0 ACCDET_RSV */
#define ACCDET_INPUT_MICP	(1<<3)

/* ACCDET_CON1  ACCDET_CTRL */
#define ACCDET_ENABLE		(1<<0)
#define ACCDET_DISABLE		(0<<0)
#define ACCDET_EINT_EN		(1<<2)
#define ACCDET_EINT_INIT	(1<<3)

/* ACCDET_CON2: ACCDET_STATE_SWCTRL */
#define ACCDET_EINT_PWM_IDLE     (1<<7)
#define ACCDET_MIC_PWM_IDLE      (1<<6)
#define ACCDET_VTH_PWM_IDLE      (1<<5)
#define ACCDET_CMP_PWM_IDLE      (1<<4)
#define ACCDET_PWM_IDLE		 (0x07<<4)
#define ACCDET_EINT_PWM_EN       (1<<3)
#define ACCDET_CMP_EN            (1<<0)
#define ACCDET_VTH_EN            (1<<1)
#define ACCDET_MICBIA_EN         (1<<2)
#define ACCDET_PWM_EN		 (0x07)

/* ACCDET_CON10: ACCDET_DEBOUNCE4 */
/* 2ms */
#define ACCDET_DE4		0x42

/* ACCDET_CON11: ACCDET default value */
#define ACCDET_EINT_IVAL_SEL	(1<<14)
#define ACCDET_VAL_DEF		(0x0333)


/* ACCDET_CON12: ACCDET_IRQ_STS */
#define IRQ_CLR_BIT		0x100
#define IRQ_EINT_CLR_BIT	0x400
#define IRQ_STATUS_BIT		(1<<0)
#define EINT_IRQ_STATUS_BIT	(1<<2)
#define EINT_IRQ_POL		(1<<15)
#define EINT_IRQ_POL_HIGH	(1<<15)
#define EINT_IRQ_POL_LOW	(1<<15)

/* ACCDET_CON14 ACCDET_STATE_RG */
#define ACCDET_STATE_MEM_IN_OFFSET	(0x06)
#define ACCDET_STATE_AB_MASK	(0x03)
#define ACCDET_STATE_AB_00		(0x00)
#define ACCDET_STATE_AB_01		(0x01)
#define ACCDET_STATE_AB_10		(0x02)
#define ACCDET_STATE_AB_11		(0x03)

/* ACCDET_CON15 ACCDET_EINT_CTL */
/* 0.12 msec */
#define EINT_IRQ_DE_OUT		(0x00<<3)
/* 256 msec */
#define EINT_IRQ_DE_IN		(0x0E<<3)
#define EINT_PLUG_DEB_CLR	(~(0x0F<<3))
#define EINT_PWM_THRESH		0x400

 /* ACCDET_CON24 ACCDET_HW_MODE_DFF */
/* Enable HW mode */
#define ACCDET_EINT_REVERSE	(0x01<<15)
#define ACCDET_HWMODE_SEL	(0x01<<0)
/* 1, modified for path mt6355;0,path to mt6337 */
#define ACCDET_EINT_DEB_OUT_DFF	(0x01<<1)


/* AUXADC */
 /* AUXAD_ACCDET: ACCDET_AUXADC_AUTO_SPL */
#define ACCDET_AUXADC_AUTO_SET   (1<<0)
 /* ACCDET_AUXADC_REG */
#define ACCDET_DATA_READY        (1<<15)
#define ACCDET_DATA_MASK         (0x0FFF)


/* EFUSE  offset */
#define RG_ACCDET_BIT_SHIFT	(0x09)
#define RG_ACCDET_HIGH_BIT_SHIFT	(0x07)
/* offset mask */
#define ACCDET_CALI_MASK0	(0x7F)
/* reserve */
#define ACCDET_CALI_MASK1	(0xFF)
/* AD efuse mask */
#define ACCDET_CALI_MASK2	(0x7F)
/* DB efuse mask */
#define ACCDET_CALI_MASK3	(0xFF)
/* BC efuse mask */
#define ACCDET_CALI_MASK4	(0x7F)

#endif/* end _REG_ACCDET_H_ */




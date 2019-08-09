/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*************Register Bit Define*************/
/* AUDENC_ANA_CON16: */
#define RG_AUD_MICBIAS1_LOWP_EN		(1<<PMIC_RG_AUDMICBIAS1LOWPEN_SHIFT)

/* AUDENC_ANA_CON18: */
#define RG_ACCDET_MODE_ANA11_MODE1	(0x000F)
#define RG_ACCDET_MODE_ANA11_MODE2	(0x008F)
#define RG_ACCDET_MODE_ANA11_MODE6	(0x008F)

/* ------Register_AUXADC_REG  Bit Define------ */
/* AUXADC_ADC5:  Auxadc CH5 read data */
#define AUXADC_DATA_RDY_CH5		(1<<15)
#define AUXADC_DATA_PROCEED_CH5	(0<<15)
#define AUXADC_DATA_MASK		(0x0FFF)

/* AUXADC_RQST0_SET:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_SET		(1<<5)
/* AUXADC_RQST0_CLR:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_CLR		(1<<5)

/* -----Register_EFUSE_REG  Bit Define-------- */
#define ACCDET_CALI_MASK0		(0xFF)
#define ACCDET_CALI_MASK1		(0xFF<<8)
#define ACCDET_CALI_MASK2		(0xFF)
#define ACCDET_CALI_MASK3		(0xFF<<8)
#define ACCDET_CALI_MASK4		(0xFF)

/* -----Register_ACCDET_REG  Bit Define------- */

#define ACCDET_EINT1_IRQ_CLR_B11	(0x01<<PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT)
#define ACCDET_EINT0_IRQ_CLR_B10	(0x01<<PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_EINT_IRQ_CLR_B10_11	(0x03<<PMIC_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_IRQ_CLR_B8		(0x01<<PMIC_ACCDET_IRQ_CLR_SHIFT)

#define ACCDET_EINT1_IRQ_B3		(0x01<<PMIC_ACCDET_EINT1_IRQ_SHIFT)
#define ACCDET_EINT0_IRQ_B2		(0x01<<PMIC_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_EINT_IRQ_B2_B3		(0x03<<PMIC_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_IRQ_B0			(0x01<<PMIC_ACCDET_IRQ_SHIFT)

/* ACCDET_CON25: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET	(PMIC_ACCDET_MEM_IN_SHIFT)
#define ACCDET_STATE_AB_MASK		(0x03)
#define ACCDET_STATE_AB_00			(0x00)
#define ACCDET_STATE_AB_01			(0x01)
#define ACCDET_STATE_AB_10			(0x02)
#define ACCDET_STATE_AB_11			(0x03)

/* ACCDET_CON19 */
#define ACCDET_EINT0_STABLE_VAL ((1<<PMIC_ACCDET_DA_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT0_EN_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT0_CMPEN_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT0_CEN_STABLE_SHIFT))

#define ACCDET_EINT1_STABLE_VAL ((1<<PMIC_ACCDET_DA_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT1_EN_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT1_CMPEN_STABLE_SHIFT) | \
				(1<<PMIC_ACCDET_EINT1_CEN_STABLE_SHIFT))

#endif/* end _REG_ACCDET_H_ */

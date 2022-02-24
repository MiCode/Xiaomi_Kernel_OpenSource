// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __TSCPU_TSENSE_SETTINGS_H__
#define __TSCPU_TSENSE_SETTINGS_H__

/*=============================================================
 * Chip related
 *=============================================================
 */
/* chip dependent */
/* TODO: change to new reg addr. */
#define ADDRESS_INDEX_0  101 /* 184 */
#define ADDRESS_INDEX_1	 100 /* 180 */
#define ADDRESS_INDEX_2	 102 /* 188 */
#define ADDRESS_INDEX_3	 111 /* 1AC */
#define ADDRESS_INDEX_4	 112 /* 1B0 */

/* TSCON1 bit table */
#define TSCON0_bit_6_7_00 0x00  /* TSCON0[7:6]=2'b00*/
#define TSCON0_bit_6_7_01 0x40  /* TSCON0[7:6]=2'b01*/
#define TSCON0_bit_6_7_10 0x80  /* TSCON0[7:6]=2'b10*/
#define TSCON0_bit_6_7_11 0xc0  /* TSCON0[7:6]=2'b11*/
#define TSCON0_bit_6_7_MASK 0xc0

#define TSCON1_bit_4_5_00 0x00  /* TSCON1[5:4]=2'b00*/
#define TSCON1_bit_4_5_01 0x10  /* TSCON1[5:4]=2'b01*/
#define TSCON1_bit_4_5_10 0x20  /* TSCON1[5:4]=2'b10*/
#define TSCON1_bit_4_5_11 0x30  /* TSCON1[5:4]=2'b11*/
#define TSCON1_bit_4_5_MASK 0x30

#define TSCON1_bit_0_2_000 0x00  /*TSCON1[2:0]=3'b000*/
#define TSCON1_bit_0_2_001 0x01  /*TSCON1[2:0]=3'b001*/
#define TSCON1_bit_0_2_010 0x02  /*TSCON1[2:0]=3'b010*/
#define TSCON1_bit_0_2_011 0x03  /*TSCON1[2:0]=3'b011*/
#define TSCON1_bit_0_2_100 0x04  /*TSCON1[2:0]=3'b100*/
#define TSCON1_bit_0_2_101 0x05  /*TSCON1[2:0]=3'b101*/
#define TSCON1_bit_0_2_110 0x06  /*TSCON1[2:0]=3'b110*/
#define TSCON1_bit_0_2_111 0x07  /*TSCON1[2:0]=3'b111*/
#define TSCON1_bit_0_2_MASK 0x07

#define TSCON1_bit_0_3_0000 0x00  /*TSCON1[3:0]=4'b0000*/
#define TSCON1_bit_0_3_0001 0x01  /*TSCON1[3:0]=4'b0001*/
#define TSCON1_bit_0_3_0010 0x02  /*TSCON1[3:0]=4'b0010*/
#define TSCON1_bit_0_3_0011 0x03  /*TSCON1[3:0]=4'b0011*/
#define TSCON1_bit_0_3_0100 0x04  /*TSCON1[3:0]=4'b0100*/
#define TSCON1_bit_0_3_0101 0x05  /*TSCON1[3:0]=4'b0101*/
#define TSCON1_bit_0_3_0110 0x06  /*TSCON1[3:0]=4'b0110*/
#define TSCON1_bit_0_3_0111 0x07  /*TSCON1[3:0]=4'b0111*/
#define TSCON1_bit_0_3_1000 0x08  /*TSCON1[3:0]=4'b1000*/
#define TSCON1_bit_0_3_1001 0x09  /*TSCON1[3:0]=4'b1001*/
#define TSCON1_bit_0_3_1010 0x0A  /*TSCON1[3:0]=4'b1010*/
#define TSCON1_bit_0_3_1011 0x0B  /*TSCON1[3:0]=4'b1011*/
#define TSCON1_bit_0_3_1100 0x0C  /*TSCON1[3:0]=4'b1100*/
#define TSCON1_bit_0_3_1101 0x0D  /*TSCON1[3:0]=4'b1101*/
#define TSCON1_bit_0_3_1110 0x0E  /*TSCON1[3:0]=4'b1110*/
#define TSCON1_bit_0_3_1111 0x0F  /*TSCON1[3:0]=4'b1111*/
#define TSCON1_bit_0_3_MASK 0x0F

/* ADC value to mcu */
/*chip dependent*/
#define TEMPADC_MCU0    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0000))
#define TEMPADC_MCU1    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0001))
#define TEMPADC_MCU2    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0010))
#define TEMPADC_MCU4    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0100))
#define TEMPADC_MCU5    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0101))
#define TEMPADC_MCU6    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0110))
#define TEMPADC_MCU7    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_0111))
#define TEMPADC_MCU8    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_1000))
#define TEMPADC_MCU9    ((0x30&TSCON1_bit_4_5_00)|(0x0F&TSCON1_bit_0_3_1001))
#define TEMPADC_ABB     ((0x30&TSCON1_bit_4_5_01)|(0x0F&TSCON1_bit_0_3_0000))

/*******************************************************************************
 * Thermal Controller Register Definition
 *****************************************************************************
 */
#define TEMPMONCTL0		(THERM_CTRL_BASE_2 + 0x800)
#define TEMPMONCTL1		(THERM_CTRL_BASE_2 + 0x804)
#define TEMPMONCTL2		(THERM_CTRL_BASE_2 + 0x808)
#define TEMPMONINT		(THERM_CTRL_BASE_2 + 0x80C)
#define TEMPMONINTSTS		(THERM_CTRL_BASE_2 + 0x810)
#define TEMPMONIDET0		(THERM_CTRL_BASE_2 + 0x814)
#define TEMPMONIDET1		(THERM_CTRL_BASE_2 + 0x818)
#define TEMPMONIDET2		(THERM_CTRL_BASE_2 + 0x81C)
#define TEMPH2NTHRE		(THERM_CTRL_BASE_2 + 0x824)
#define TEMPHTHRE		(THERM_CTRL_BASE_2 + 0x828)
#define TEMPCTHRE		(THERM_CTRL_BASE_2 + 0x82C)
#define TEMPOFFSETH		(THERM_CTRL_BASE_2 + 0x830)
#define TEMPOFFSETL		(THERM_CTRL_BASE_2 + 0x834)
#define TEMPMSRCTL0		(THERM_CTRL_BASE_2 + 0x838)
#define TEMPMSRCTL1		(THERM_CTRL_BASE_2 + 0x83C)
#define TEMPAHBPOLL		(THERM_CTRL_BASE_2 + 0x840)
#define TEMPAHBTO		(THERM_CTRL_BASE_2 + 0x844)
#define TEMPADCPNP0		(THERM_CTRL_BASE_2 + 0x848)
#define TEMPADCPNP1		(THERM_CTRL_BASE_2 + 0x84C)
#define TEMPADCPNP2		(THERM_CTRL_BASE_2 + 0x850)
#define TEMPADCMUX		(THERM_CTRL_BASE_2 + 0x854)
#define TEMPADCEXT		(THERM_CTRL_BASE_2 + 0x858)
#define TEMPADCEXT1		(THERM_CTRL_BASE_2 + 0x85C)
#define TEMPADCEN		(THERM_CTRL_BASE_2 + 0x860)
#define TEMPPNPMUXADDR		(THERM_CTRL_BASE_2 + 0x864)
#define TEMPADCMUXADDR		(THERM_CTRL_BASE_2 + 0x868)
#define TEMPADCEXTADDR		(THERM_CTRL_BASE_2 + 0x86C)
#define TEMPADCEXT1ADDR		(THERM_CTRL_BASE_2 + 0x870)
#define TEMPADCENADDR		(THERM_CTRL_BASE_2 + 0x874)
#define TEMPADCVALIDADDR	(THERM_CTRL_BASE_2 + 0x878)
#define TEMPADCVOLTADDR		(THERM_CTRL_BASE_2 + 0x87C)
#define TEMPRDCTRL		(THERM_CTRL_BASE_2 + 0x880)
#define TEMPADCVALIDMASK	(THERM_CTRL_BASE_2 + 0x884)
#define TEMPADCVOLTAGESHIFT	(THERM_CTRL_BASE_2 + 0x888)
#define TEMPADCWRITECTRL	(THERM_CTRL_BASE_2 + 0x88C)
#define TEMPMSR0		(THERM_CTRL_BASE_2 + 0x890)
#define TEMPMSR1		(THERM_CTRL_BASE_2 + 0x894)
#define TEMPMSR2		(THERM_CTRL_BASE_2 + 0x898)
#define TEMPADCHADDR		(THERM_CTRL_BASE_2 + 0x89C)
#define TEMPIMMD0		(THERM_CTRL_BASE_2 + 0x8A0)
#define TEMPIMMD1		(THERM_CTRL_BASE_2 + 0x8A4)
#define TEMPIMMD2		(THERM_CTRL_BASE_2 + 0x8A8)
#define TEMPMONIDET3		(THERM_CTRL_BASE_2 + 0x8B0)
#define TEMPADCPNP3		(THERM_CTRL_BASE_2 + 0x8B4)
#define TEMPMSR3		(THERM_CTRL_BASE_2 + 0x8B8)
#define TEMPIMMD3		(THERM_CTRL_BASE_2 + 0x8BC)
#define TEMPPROTCTL		(THERM_CTRL_BASE_2 + 0x8C0)
#define TEMPPROTTA		(THERM_CTRL_BASE_2 + 0x8C4)
#define TEMPPROTTB		(THERM_CTRL_BASE_2 + 0x8C8)
#define TEMPPROTTC		(THERM_CTRL_BASE_2 + 0x8CC)
#define TEMPSPARE0		(THERM_CTRL_BASE_2 + 0x8F0)
#define TEMPSPARE1		(THERM_CTRL_BASE_2 + 0x8F4)
#define TEMPSPARE2		(THERM_CTRL_BASE_2 + 0x8F8)
#define TEMPSPARE3		(THERM_CTRL_BASE_2 + 0x8FC)

#define PTPCORESEL          (THERM_CTRL_BASE_2 + 0xF00)
#define THERMINTST          (THERM_CTRL_BASE_2 + 0xF04)
#define PTPODINTST          (THERM_CTRL_BASE_2 + 0xF08)
#define THSTAGE0ST          (THERM_CTRL_BASE_2 + 0xF0C)
#define THSTAGE1ST          (THERM_CTRL_BASE_2 + 0xF10)
#define THSTAGE2ST          (THERM_CTRL_BASE_2 + 0xF14)
#define THAHBST0            (THERM_CTRL_BASE_2 + 0xF18)
#define THAHBST1            (THERM_CTRL_BASE_2 + 0xF1C)
#define PTPSPARE0           (THERM_CTRL_BASE_2 + 0xF20)
#define PTPSPARE1           (THERM_CTRL_BASE_2 + 0xF24)
#define PTPSPARE2           (THERM_CTRL_BASE_2 + 0xF28)
#define PTPSPARE3           (THERM_CTRL_BASE_2 + 0xF2C)
#define THSLPEVEB           (THERM_CTRL_BASE_2 + 0xF30)

#define PTPSPARE0_P           (thermal_phy_base + 0xF20)
#define PTPSPARE1_P           (thermal_phy_base + 0xF24)
#define PTPSPARE2_P           (thermal_phy_base + 0xF28)
#define PTPSPARE3_P           (thermal_phy_base + 0xF2C)
#endif	/* __TSCPU_TSENSE_SETTINGS_H__ */

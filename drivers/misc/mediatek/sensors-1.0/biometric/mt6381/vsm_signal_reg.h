/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef DEMOS_VSM_SIGNAL_REG_H_
#define DEMOS_VSM_SIGNAL_REG_H_

struct signal_data_t {
	uint16_t addr;
	uint32_t value;
};

/* voltage signal */
struct signal_data_t VSM_SIGNAL_IDLE_array[] = {
	/* Enable digital part  Mode[0:6]=[000:018:124:1A5:164:13C:1FF] */
	{0x3360, 0x00000000},
	{0x2308, 0xC0CCCC00},	/* IO DRV0 */
	{0x230C, 0x00000000},	/* IO DRV1 */
	{0x2324, 0x00000155},	/* GPIO5~9 OFF */
	{0x3344, 0x7CC0007C},	/* TOP CK CON Mode */
	{0x3348, 0x800000FF},	/* TOP CK CON1 Mode */
	{0x3300, 0xA9C71555},	/* Enable Bandgap Rotation */
	{0x3300, 0xA8C71555},	/* Enable Bandgap Rotation */
	{0x3308, 0x0101D043},	/* IA & EKGFE PWD */
	{0x3310, 0x002F5555},	/* EKGADC PWD */
	{0x3314, 0x0000A800},	/* EKGADC CLK PWD */
	{0x3318, 0x70244212},	/* PPGFE PWD */
	{0x331C, 0x0048C429},	/* PPGFE PWD */
	{0x3320, 0x000D5555},	/* PPGADC PWD */
	{0x3324, 0x0000A050},	/* PPGADC PWD */
	{0x3328, 0x00002900},	/* LED DRV PWD */
};

/* voltage signal */
struct signal_data_t VSM_SIGNAL_INIT_array[] = {
/* SW Flow: */
/* Initial=>EKG=>IDLE(SW)=>RESET(HW)=>Initial(EKG+PPG1+PPG2)=>
 *EKG(0x3360=17C)=>PPG1(LED1 ON)=>PPG2 (LED2 ON)
 */

/* initial setting */

	{0x2308, 0xC0CCCC00},	/* IO DRV0 */
	{0x230C, 0x00000000},	/* IO DRV1 */
	{0x2324, 0x00000155},	/* GPIO5~9 OFF */
	{0x3344, 0x7CC00048},	/* TOP CK CON Mode */
	{0x3348, 0x80000000},	/* TOP CK CON1 Mode */

	{0x3300, 0xA9C71555},	/* Enable Bandgap Rotation */
	/* delay 50msec */
	{0x3300, 0xA8C71555},	/* Enable Bandgap Rotation */

	/* Enable EKG+PPG1+PPG2 SRAM threshold Interrupt */
	{0x334C, 0x00000092},

/* EKG */
	/* EKG Mode selection (1"011") Fs = 128Hz */
	/* {0x3364,0x00000009},   */
	/* EKG Mode selection (1"010") Fs = 256Hz */
	/* {0x3364,0x0000000A},   */
	{0x3364, 0x0000000B},	/* EKG Mode selection (1"011") Fs = 512Hz */

	/* {0x3308,0x0001D442},   */ /* IA gain = 6 , 2E mode */
	/* IA gain = 6 , 4E=RLD mode, enable leadoff function */
	{0x3308, 0x0000D042},

	/* EKG Mode selection (002F5554) Fs = 128Hz */
	/*  {0x3310,0x00275554},  */
	/* EKG Mode selection (002B5554) Fs = 256Hz */
	/*  {0x3310,0x002B5554},  */
	{0x3310, 0x002F5554},	/* EKG Mode selection (002F5554) Fs = 512Hz */

	{0x3314, 0x0000A802},	/* EKGADC CON1 */


/* PPG */
	{0x232C, 0x000007FF},	/* PRF=512Hz */

	{0x2338, 0x008000C0},	/* SAMPLED1 */
	{0x233C, 0x02020242},	/* SAMPLED2 */
	{0x2340, 0x01410181},	/* SAMPAMB1 */
	{0x2344, 0x02C30303},	/* SAMPAMB2 */
	{0x2348, 0x00C20182},	/* ConvLED1 */
	{0x234C, 0x02440304},	/* ConvLED2 */
	{0x2350, 0x01830243},	/* ConvAMB1 */
	{0x2354, 0x030503C5},	/* ConvAMB2 */
	{0x2358, 0x00C100C2},	/* ADC RST1 */
	{0x235C, 0x01820183},	/* ADC RST2 */
	{0x2360, 0x02430244},	/* ADC RST3 */
	{0x2364, 0x03040305},	/* ADC RST4 */
	{0x2368, 0x03F70744},	/* ADC Enable Dynamic PWD */
/*  {0x2368, 0x00000000},   */ /* ADC Disable Dynamic PWD */
	{0x236C, 0x03F70776},	/* FE Enable Dynamic PWD */
/*  {0x236C, 0x00000000},   */ /* FE Disable Dynamic PWD */
	{0x2370, 0x00C00343},	/* PPG out EN Enable Dynamic PWD */
/*  {0x2370, 0x20C320C3},   */ /* PPG out EN Disable Dynamic PWD */

	/* PPG1 SRAM = AMB1, LED1, AMB1, LED1= reg2, reg3, reg2, reg3 */
	/* PPG2 SRAM =
	 *LED1-AMB1, LED2-AMB2, LED1-AMB1, LED2-AMB2, =
	 *reg5, reg6, reg5, reg6
	 */
/*    {0x3368, 0x04C9A772},   */ /* NUMAVG=1 */
	/* sram1 = reg2, reg4,...; sram2 = reg5, reg6,...; */
	{0x3368, 0x04C9A972},
/*    {0x3368, 0x04B6270A},   */ /* NUMAVG=1 */
/*    {0x3368, 0x04C9A772},   */ /* Let reg5 = reg3-reg2, reg6 = reg1-reg4 */
/*  {0x3368, 0x08B6270A},   */ /* NUMAVG=2 */

	{0x3318, 0x31FE4212},	/* small PD (TIA=100KOhm,PGA=6,Cp=+50pF) */
/*  {0x3318,0x303E5292},    */ /* Large PD (TIA=100KOhm,PGA=6,Cp=70pF) */

	{0x331C, 0x0048cc29},	/* PPG AMBDAC Power ON */
/*  {0x3320,0x000F5554},    */ /* PPG ADC Chopper ON */
	{0x3320, 0x000D5554},	/* PPG ADC Chopper OFF */
	{0x3324, 0x0000A010},	/* PPGADC_CON1 */

	/* LED Full range Current [7:5]=[010] =>7.5mA*7 =103mA */
	{0x3328, 0x00002CFE},
	/* LED Current =22.5mA/256x(DAC code)= 0.087890625mA x 32 = 2.8125mA */
	{0x332C, 0x0000252A},


	{0x33CC, 0x00000100},	/* Write irq_th EKG */
	/* Write EKG SRAM address & 0xC0[29] & 0xC0[30] */
	{0x33C0, 0x60000000},

	{0x33DC, 0x000000FA},	/* Write irq_th PPG1 */
	/* Write PPG1 SRAM address & 0xC0[29] & 0xC0[30] */
	{0x33D0, 0x60000000},

	{0x33EC, 0x000000FA},	/* Write irq_th PPG2 */
	/* Write PPG2 SRAM address & 0xC0[29] & 0xC0[30] */
	{0x33E0, 0x60000000},

/* 0x3360 Enable Signal */
	{0x3360, 0x0000017C}	/* ALL ON Mode (EKG+PPG1+PPG2) */
};

#endif				/* DEMOS_VSM_SIGNAL_REG_H_ */

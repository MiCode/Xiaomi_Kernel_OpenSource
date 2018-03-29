/*
 * Copyright (C) 2011 MediaTek Inc.
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

#include "sec_hal.h"
#include <linux/types.h>
#include "hacc_mach.h"

/******************************************************************************
 * DEBUG
 ******************************************************************************/
#define SEC_DEBUG                   (false)
#define SMSG                        DBG_MSG
#if SEC_DEBUG
#define DMSG                        DBG_MSG
#else
#define DMSG
#endif

static const unsigned int g_CFG_RANDOM_PATTERN[3][4] = {
	{0x2D44BB70, 0xA744D227, 0xD0A9864B, 0x83FFC244},
	{0x7EC8266B, 0x43E80FB2, 0x01A6348A, 0x2067F9A0},
	{0x54536405, 0xD546A6B1, 0x1CC3EC3A, 0xDE377A83}
};


/******************************************************************************
 * HACC HW internal function
 ******************************************************************************/
void HACC_V3_Init(bool encode, const unsigned int g_AC_CFG[])
{

	unsigned int i, j;
	const unsigned int *p1;
	/* const unsigned int *p2; */
	unsigned int acon_setting;

	/* -------------------------- */
	/* Power On HACC               */
	/* -------------------------- */
	masp_hal_secure_algo_init();

	/* -------------------------- */
	/* Configuration              */
	/* -------------------------- */

	/* set little endian */
	acon_setting = HACC_AES_CHG_BO_OFF;

	/* set mode */
	acon_setting |= HACC_AES_CBC;

	/* type */
	acon_setting |= HACC_AES_128;

	/* operation */
	if (encode)
		acon_setting |= HACC_AES_ENC;
	else
		acon_setting |= HACC_AES_DEC;

	/* -------------------------- */
	/* Set Key                    */
	/* -------------------------- */

	/* clear key */
	for (i = 0; i < 8; i++)
		*(((volatile unsigned int *)HACC_AKEY0) + i) = 0;

	/* -------------------------- */
	/* Generate META Key          */
	/* -------------------------- */
	*((volatile unsigned int *)HACC_ACON) =
	    (HACC_AES_CHG_BO_OFF | HACC_AES_128 | HACC_AES_CBC | HACC_AES_DEC);

	/* init ACONK
	   B2C: bind HUID/HUK to HACC */
	*((volatile unsigned int *)HACC_ACONK) = HACC_AES_BK2C;
	/* enable R2K, so that output data is feedback to key by HACC internal algorithm */
	*((volatile unsigned int *)HACC_ACONK) |= HACC_AES_R2K;

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	*((volatile unsigned int *)HACC_ACON2) = HACC_AES_CLR;

	/* set cfg */
	p1 = &g_AC_CFG[0];
	for (i = 0; i < 4; i++)
		*(((volatile unsigned int *)HACC_ACFG0) + i) = *(p1 + i);

	/* encrypt fix pattern 3 rounds to generate a pattern from HUID/HUK */
	for (i = 0; i < 3; i++) {
		/* set fixed pattern into source */
		p1 = g_CFG_RANDOM_PATTERN[i];
		for (j = 0; j < 4; j++)
			*(((volatile unsigned int *)HACC_ASRC0) + j) = *(p1 + j);
		/* start decryption */
		*((volatile unsigned int *)HACC_ACON2) = HACC_AES_START;
		/* polling ready */
		while (0 == ((*((volatile unsigned int *)HACC_ACON2)) & HACC_AES_RDY))
			;
	}

	/* -------------------------- */
	/* Set CFG                     */
	/* -------------------------- */

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	*((volatile unsigned int *)HACC_ACON2) = HACC_AES_CLR;

	/* set cfg */
	p1 = &g_AC_CFG[0];
	for (i = 0; i < 4; i++)
		*(((volatile unsigned int *)HACC_ACFG0) + i) = *(p1 + i);

	/* set config without R2K */
	*((volatile unsigned int *)HACC_ACON) = acon_setting;
	*((volatile unsigned int *)HACC_ACONK) = 0;
}

void HACC_V3_Run(volatile unsigned int *p_src, unsigned int src_len, volatile unsigned int *p_dst)
{
	unsigned int i, j;

	/* config src/dst addr and len */
	unsigned int len = src_len;

	/* fo operation */
	for (i = 0; i < len; i += 16, p_src += 4, p_dst += 4) {
		/* set fixed pattern into source */
		for (j = 0; j < 4; j++)
			*(((volatile unsigned int *)HACC_ASRC0) + j) = *(p_src + j);
		/* start encryption */
		*((volatile unsigned int *)HACC_ACON2) = HACC_AES_START;
		/* polling ready */
		while (0 == ((*((volatile unsigned int *)HACC_ACON2)) & HACC_AES_RDY))
			;
		/* read out data */
		for (j = 0; j < 4; j++)
			*(p_dst + j) = *(((volatile unsigned int *)HACC_AOUT0) + j);
	}
}

void HACC_V3_Terminate(void)
{
	unsigned int i;

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	*((volatile unsigned int *)HACC_ACON2) = HACC_AES_CLR;

	/* -------------------------- */
	/* Clear Key                  */
	/* -------------------------- */
	/* clear key */
	for (i = 0; i < 8; i++)
		*(((volatile unsigned int *)HACC_AKEY0) + i) = 0;

	masp_hal_secure_algo_deinit();
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include <linux/types.h>

#include "sec_hal.h"
#include "hacc_mach.h"

#define MOD                         "MASP"

#define SEC_DEBUG                   (0)
#define SMSG                        pr_info
#define DMSG                        pr_debug

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
	unsigned int acon_setting;

	/* Power On HACC              */
	masp_hal_secure_algo_init();

	/* Configuration              */

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

	/* clear key */
	for (i = 0; i < 8; i++)
		writel(0, (void *)(HACC_AKEY0 + 4 * i));

	/* Generate META Key          */
	writel(HACC_AES_CHG_BO_OFF | HACC_AES_128 | HACC_AES_CBC | HACC_AES_DEC,
	       (void *)HACC_ACON);

	/* init ACONK
	 * B2C: bind HUID/HUK to HACC
	 */
	writel(HACC_AES_BK2C, (void *)HACC_ACONK);
	/* enable R2K, so that output data is feedback to key
	 * by HACC internal algorithm
	 */
	writel(readl((const void *)HACC_ACONK) | HACC_AES_R2K,
	       (void *)HACC_ACONK);

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	writel(HACC_AES_CLR, (void *)HACC_ACON2);

	/* set cfg */
	p1 = &g_AC_CFG[0];
	for (i = 0; i < 4; i++)
		writel(*(p1 + i), (void *)(HACC_ACFG0 + 4 * i));

	/* encrypt fix pattern 3 rounds to generate a pattern from HUID/HUK */
	for (i = 0; i < 3; i++) {
		/* set fixed pattern into source */
		p1 = g_CFG_RANDOM_PATTERN[i];
		for (j = 0; j < 4; j++)
			writel(*(p1 + j), (void *)(HACC_ASRC0 + 4 * j));
		/* start decryption */
		writel(HACC_AES_START, (void *)HACC_ACON2);
		/* polling ready */
		while (0 == (readl((const void *)HACC_ACON2) & HACC_AES_RDY))
			;
	}

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	writel(HACC_AES_CLR, (void *)HACC_ACON2);

	/* set cfg */
	p1 = &g_AC_CFG[0];
	for (i = 0; i < 4; i++)
		writel(*(p1 + i), (void *)(HACC_ACFG0 + 4 * i));

	/* set config without R2K */
	writel(acon_setting, (void *)HACC_ACON);
	writel(0, (void *)HACC_ACONK);
}

void HACC_V3_Run(unsigned int *p_src, unsigned int src_len, unsigned int *p_dst)
{
	unsigned int i, j;

	/* config src/dst addr and len */
	unsigned int len = src_len;

	/* fo operation */
	for (i = 0; i < len; i += 16, p_src += 4, p_dst += 4) {
		/* set fixed pattern into source */
		for (j = 0; j < 4; j++)
			writel(*(p_src + j), (void *)(HACC_ASRC0 + 4 * j));
		/* start encryption */
		writel(HACC_AES_START, (void *)HACC_ACON2);
		/* polling ready */
		while (0 == (readl((const void *)HACC_ACON2) & HACC_AES_RDY))
			;
		/* read out data */
		for (j = 0; j < 4; j++) {
			writel(readl((const void *)(HACC_AOUT0 + 4 * j)),
			       (void *)(p_dst + j));
		}
	}
}

void HACC_V3_Terminate(void)
{
	unsigned int i;

	/* clear HACC_ASRC/HACC_ACFG/HACC_AOUT */
	writel(HACC_AES_CLR, (void *)HACC_ACON2);

	/* clear key */
	for (i = 0; i < 8; i++)
		writel(0, (void *)(HACC_AKEY0 + 4 * i));

	masp_hal_secure_algo_deinit();
}

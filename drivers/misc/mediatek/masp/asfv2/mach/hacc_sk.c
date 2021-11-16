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

/*#include <mach/mt_typedefs.h>
 * #include <mach/mt_reg_base.h>
 */
#include "sec_error.h"
#include "hacc_mach.h"
#include <linux/io.h>

/******************************************************************************
 * this file contains the hardware secure engine low-level operations
 * note that : all the functions in this file are ONLY for HACC internal usages.
 ******************************************************************************/

/******************************************************************************
 * CONSTANT DEFINITIONS
 ******************************************************************************/
#define MOD                         "HACC"
#define HACC_TEST                    (0)

/******************************************************************************
 * DEBUG
 ******************************************************************************/
#define SEC_DEBUG                   (0)
#define SMSG                        printk
#if SEC_DEBUG
#define DMSG                        printk
#else
#define DMSG
#endif

/******************************************************************************
 * LOCAL VERIABLE
 ******************************************************************************/
static struct hacc_context hacc_ctx;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

#if HACC_TEST
static void hacc_test(void)
{
	unsigned int i, test_sz = HACC_AES_MAX_KEY_SZ * 24;
	unsigned int test_keysz = AES_KEY_256;
	unsigned char *test_src = (unsigned char *)HACC_AES_TEST_SRC;
	unsigned char *test_dst = (unsigned char *)HACC_AES_TEST_DST;
	unsigned char *test_tmp = (unsigned char *)HACC_AES_TEST_TMP;

	/* prepare data */
	for (i = 0; i < test_sz; i++)
		test_src[i] = i + 1;

	hacc_set_key(AES_HW_WRAP_KEY, test_keysz);
	hacc_do_aes(AES_ENC, test_src, test_tmp, test_sz);
	hacc_set_key(AES_HW_WRAP_KEY, test_keysz);
	hacc_do_aes(AES_DEC, test_tmp, test_dst, test_sz);

	for (i = 0; i < test_sz; i++) {
		if (test_src[i] != test_dst[i]) {
			DMSG("[%s] mismatch at %d, ", MOD, i);
			DMSG("src = 0x%x, ", test_src[i]);
			DMSG("dst = 0x%x\n", test_dst[i]);
		}
	}
	DMSG("[%s] encrypt & descrypt unit test pass. (Key = %dbits)\n",
	     MOD,
	     test_keysz << 3);
}
#else
#define hacc_test()      do {} while (0)
#endif

/******************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************/
static unsigned int hacc_set_cfg(struct aes_cfg *cfg)
{
	memcpy(&hacc_ctx.cfg, cfg, sizeof(struct aes_cfg));
	return SEC_OK;
}

static unsigned int hacc_set_mode(enum aes_mode mode)
{
	struct aes_cfg cfg;

	writel(readl((const void *)HACC_ACON) & ~(HACC_AES_MODE_MASK),
	       (void *)HACC_ACON);

	switch (mode) {
	case AES_ECB_MODE:
		/* no need cfg */
		memset(&cfg.config[0], 0, sizeof(cfg.config));
		writel(readl((const void *)HACC_ACON) | HACC_AES_ECB,
		       (void *)HACC_ACON);
		break;
	case AES_CBC_MODE:
		writel(readl((const void *)HACC_ACON) | HACC_AES_CBC,
		       (void *)HACC_ACON);
		break;
	default:
		return ERR_HACC_MODE_INVALID;
	}

	return SEC_OK;
}

unsigned int hacc_set_key(enum aes_key_id id, enum aes_key key)
{
	unsigned int i, acon = 0;
	unsigned int akey;
	unsigned char *tkey;

	switch (key) {
	case AES_KEY_128:
		acon |= HACC_AES_128;
		break;
	case AES_KEY_192:
		acon |= HACC_AES_192;
		break;
	case AES_KEY_256:
		acon |= HACC_AES_256;
		break;
	default:
		return ERR_HACC_KEY_INVALID;
	}
	/* set aes block size */
	hacc_ctx.blk_sz = key;

	/* set aes key length */
	writel(readl((const void *)HACC_ACON) & ~(HACC_AES_TYPE_MASK),
	       (void *)HACC_ACON);
	writel(readl((const void *)HACC_ACON) | acon, (void *)HACC_ACON);

	/* clear key */
	for (i = 0; i < HACC_AES_MAX_KEY_SZ; i += 4)
		writel(0, (void *)(HACC_AKEY0 + i));

	/* set aes key */
	switch (id) {
	case AES_HW_KEY:
		writel(readl((const void *)HACC_ACONK) | HACC_AES_BK2C,
		       (void *)HACC_ACONK);
		return 0;
	case AES_HW_WRAP_KEY:
		tkey = &hacc_ctx.hw_key[0];
		break;
	case AES_SW_KEY:
	default:
		tkey = &hacc_ctx.sw_key[0];
		break;
	}

	/* non hardware binding key */
	writel(readl((const void *)HACC_ACONK) & ~(HACC_AES_BK2C),
	       (void *)HACC_ACONK);

	/* update key. note that don't use key directly */
	for (i = 0; i < HACC_AES_MAX_KEY_SZ; i += 4) {
		akey = (tkey[i] << 24) |
		       (tkey[i + 1] << 16) |
		       (tkey[i + 2] << 8) |
		       (tkey[i + 3]);
		writel(akey, (void *)(HACC_AKEY0 + i));
	}

	return SEC_OK;
}

unsigned int hacc_do_aes(enum aes_ops ops,
			 unsigned char *src,
			 unsigned char *dst,
			 unsigned int size)
{
	unsigned int i;
	unsigned int *ds, *dt, *vt;

	/* make sure size is aligned to aes block size */
	if ((size % AES_BLK_SZ) != 0) {
		SMSG("[%s] size = %d is not %d bytes alignment\n",
		     MOD,
		     size,
		     AES_BLK_SZ);
		return ERR_HACC_DATA_UNALIGNED;
	}

	vt = (unsigned int *)&hacc_ctx.cfg.config[0];

	/* erase src, cfg, out register */
	writel(readl((const void *)HACC_ACON2) | HACC_AES_CLR,
	       (void *)HACC_ACON2);

	/* set init config */
	for (i = 0; i < AES_CFG_SZ; i += 4)
		writel(*vt++, (void *)(HACC_ACFG0 + i));

	if (ops == AES_ENC)
		writel(readl((const void *)HACC_ACON) | HACC_AES_ENC,
		       (void *)HACC_ACON);
	else
		writel(readl((const void *)HACC_ACON) & ~HACC_AES_ENC,
		       (void *)HACC_ACON);

	ds = (unsigned int *)src;
	dt = (unsigned int *)dst;

	do {
		/* fill in the data */
		for (i = 0; i < AES_BLK_SZ; i += 4)
			writel(*ds++, (void *)(HACC_ASRC0 + i));

		/* start aes engine */
		writel(readl((const void *)HACC_ACON2) | HACC_AES_START,
		       (char *)HACC_ACON2);

		/* wait for aes engine ready */
		while ((readl((const void *)HACC_ACON2) & HACC_AES_RDY) == 0)
			;

		/* read out the data */
		for (i = 0; i < AES_BLK_SZ; i += 4)
			* dt++ = readl((const void *)(HACC_AOUT0 + i));

		if (size == 0)
			goto end;

		size -= AES_BLK_SZ;

	} while (size != 0);

end:

	return SEC_OK;
}

unsigned int hacc_deinit(void)
{
	unsigned int ret = 0;

	/* clear aes module */
	writel(readl((const void *)HACC_ACON2) | HACC_AES_CLR,
	       (void *)HACC_ACON2);

	return ret;
}

unsigned int hacc_init(struct aes_key_seed *keyseed)
{
	unsigned int i = 0;
	unsigned int *config;
	unsigned int ret = 0;

	hacc_deinit();

	/* clear aes module */
	writel(readl((const void *)HACC_ACON2) | HACC_AES_CLR,
	       (void *)HACC_ACON2);

	/* set aes module in cbc mode with no byte order change */
	writel(readl((const void *)HACC_ACON2) &
	       ~(HACC_AES_CHG_BO_MASK |
		 HACC_AES_MODE_MASK),
	       (void *)HACC_ACON2);
	writel(readl((const void *)HACC_ACON2) |
	       (HACC_AES_CHG_BO_OFF |
		HACC_AES_CBC),
	       (void *)HACC_ACON2);

	/* aes secure initialiation */
	memset(&hacc_ctx, 0, sizeof(struct hacc_context));

	for (i = 0; i < keyseed->size; i++)
		hacc_ctx.sw_key[i] = keyseed->seed[i];

	config = (unsigned int *)&hacc_ctx.cfg.config[0];

	*config++ = HACC_CFG_0;
	*config++ = HACC_CFG_1;
	*config++ = HACC_CFG_2;
	*config = HACC_CFG_3;

	ret = hacc_set_cfg(&hacc_ctx.cfg);
	if (ret != SEC_OK)
		goto end;

	ret = hacc_set_mode(AES_CBC_MODE);
	if (ret != SEC_OK)
		goto end;

	/* derive the hardware wrapper key */
	ret = hacc_set_key(AES_HW_KEY, HACC_HW_KEY_SZ);
	if (ret != SEC_OK)
		goto end;

	ret = hacc_do_aes(AES_ENC,
			  &hacc_ctx.sw_key[0],
			  &hacc_ctx.hw_key[0],
			  AES_KEY_256);
	if (ret != SEC_OK)
		goto end;

	ret = hacc_set_key(AES_HW_WRAP_KEY, AES_KEY_256);
	if (ret != SEC_OK)
		goto end;

	hacc_test();

	/* from now on, HACC HW wrap key can be used */
	bHACC_HWWrapKeyInit = 1;

	/* from now on, HACC SW key can be used */
	bHACC_SWKeyInit = 1;

end:

	return ret;
}

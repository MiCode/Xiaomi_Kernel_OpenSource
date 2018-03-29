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
#include <mach/mt_reg_base.h>
*/
#include "sec_error.h"
#include "hacc_mach.h"

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
			DMSG("[%s] test_src[%d] = 0x%x != test_dst[%d] = 0x%x\n", MOD, i,
			     test_src[i], i, test_dst[i]);
			DMSG(0);
		}
	}
	DMSG("[%s] encrypt & descrypt unit test pass. (Key = %dbits)\n", MOD, test_keysz << 3);
}
#else
#define hacc_test()      do {} while (0)
#endif

/******************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************/
static unsigned int hacc_set_cfg(AES_CFG *cfg)
{
	memcpy(&hacc_ctx.cfg, cfg, sizeof(AES_CFG));
	return SEC_OK;
}

static unsigned int hacc_set_mode(AES_MODE mode)
{
	AES_CFG cfg;

	DRV_ClrReg32(HACC_ACON, HACC_AES_MODE_MASK);

	switch (mode) {
	case AES_ECB_MODE:
		/* no need cfg */
		memset(&cfg.config[0], 0, sizeof(cfg.config));
		DRV_SetReg32(HACC_ACON, HACC_AES_ECB);
		break;
	case AES_CBC_MODE:
		DRV_SetReg32(HACC_ACON, HACC_AES_CBC);
		break;
	default:
		return ERR_HACC_MODE_INVALID;
	}

	return SEC_OK;
}

unsigned int hacc_set_key(AES_KEY_ID id, AES_KEY key)
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
	DRV_ClrReg32(HACC_ACON, HACC_AES_TYPE_MASK);
	DRV_SetReg32(HACC_ACON, acon);

	/* clear key */
	for (i = 0; i < HACC_AES_MAX_KEY_SZ; i += 4)
		DRV_WriteReg32(HACC_AKEY0 + i, 0);

	/* set aes key */
	switch (id) {
	case AES_HW_KEY:
		DRV_SetReg32(HACC_ACONK, HACC_AES_BK2C);
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
	DRV_ClrReg32(HACC_ACONK, HACC_AES_BK2C);

	/* update key. note that don't use key directly */
	for (i = 0; i < HACC_AES_MAX_KEY_SZ; i += 4) {
		akey = (tkey[i] << 24) | (tkey[i + 1] << 16) | (tkey[i + 2] << 8) | (tkey[i + 3]);
		DRV_WriteReg32(HACC_AKEY0 + i, akey);
	}

	return SEC_OK;
}

unsigned int hacc_do_aes(AES_OPS ops, unsigned char *src, unsigned char *dst, unsigned int size)
{
	unsigned int i;
	unsigned int *ds, *dt, *vt;

	/* make sure size is aligned to aes block size */
	if ((size % AES_BLK_SZ) != 0) {
		SMSG("[%s] size = %d is not %d bytes alignment\n", MOD, size, AES_BLK_SZ);
		return ERR_HACC_DATA_UNALIGNED;
	}

	vt = (unsigned int *)&hacc_ctx.cfg.config[0];

	/* erase src, cfg, out register */
	DRV_SetReg32(HACC_ACON2, HACC_AES_CLR);

	/* set init config */
	for (i = 0; i < AES_CFG_SZ; i += 4)
		DRV_WriteReg32(HACC_ACFG0 + i, *vt++);

	if (ops == AES_ENC)
		DRV_SetReg32(HACC_ACON, HACC_AES_ENC);
	else
		DRV_ClrReg32(HACC_ACON, HACC_AES_ENC);

	ds = (unsigned int *)src;
	dt = (unsigned int *)dst;

	do {
		/* fill in the data */
		for (i = 0; i < AES_BLK_SZ; i += 4)
			DRV_WriteReg32(HACC_ASRC0 + i, *ds++);

		/* start aes engine */
		DRV_SetReg32(HACC_ACON2, HACC_AES_START);

		/* wait for aes engine ready */
		while ((DRV_Reg32(HACC_ACON2) & HACC_AES_RDY) == 0)
			;

		/* read out the data */
		for (i = 0; i < AES_BLK_SZ; i += 4)
			*dt++ = DRV_Reg32(HACC_AOUT0 + i);

		if (size == 0)
			goto _end;

		size -= AES_BLK_SZ;

	} while (size != 0);

_end:

	return SEC_OK;
}

unsigned int hacc_deinit(void)
{
	unsigned int ret = 0;

	/* clear aes module */
	DRV_SetReg32(HACC_ACON2, HACC_AES_CLR);

	return ret;
}

unsigned int hacc_init(AES_KEY_SEED *keyseed)
{
	unsigned int i = 0;
	unsigned int *config;
	unsigned int ret = 0;

	hacc_deinit();
	/* DRV_WriteReg32(HACC_SECINIT0, HACC_SECINIT0_MAGIC); */
	/* DRV_WriteReg32(HACC_SECINIT1, HACC_SECINIT1_MAGIC); */
	/* DRV_WriteReg32(HACC_SECINIT2, HACC_SECINIT2_MAGIC); */

	/* clear aes module */
	DRV_SetReg32(HACC_ACON2, HACC_AES_CLR);

	/* set aes module in cbc mode with no byte order change */
	DRV_ClrReg32(HACC_ACON2, HACC_AES_CHG_BO_MASK | HACC_AES_MODE_MASK);
	DRV_SetReg32(HACC_ACON2, HACC_AES_CHG_BO_OFF | HACC_AES_CBC);

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
	if (SEC_OK != ret)
		goto _end;

	ret = hacc_set_mode(AES_CBC_MODE);
	if (SEC_OK != ret)
		goto _end;

	/* derive the hardware wrapper key */
	ret = hacc_set_key(AES_HW_KEY, HACC_HW_KEY_SZ);
	if (SEC_OK != ret)
		goto _end;

	ret = hacc_do_aes(AES_ENC, &hacc_ctx.sw_key[0], &hacc_ctx.hw_key[0], AES_KEY_256);
	if (SEC_OK != ret)
		goto _end;

	ret = hacc_set_key(AES_HW_WRAP_KEY, AES_KEY_256);
	if (SEC_OK != ret)
		goto _end;

	hacc_test();

	/* from now on, HACC HW wrap key can be used */
	bHACC_HWWrapKeyInit = 1;

	/* from now on, HACC SW key can be used */
	bHACC_SWKeyInit = 1;

_end:

	return ret;
}

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>
#include <linux/crypto-qti-common.h>
#include "crypto-qti-platform.h"
#include "crypto-qti-tz.h"

unsigned int storage_type = SDCC_CE;

#define ICE_BUFFER_SIZE 128

static uint8_t ice_buffer[ICE_BUFFER_SIZE];
static uint8_t secret_buffer[32];

int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key,
			   unsigned int slot, unsigned int data_unit_mask,
			   int capid)
{
	int err = 0;
	uint32_t smc_id = 0;
	char *tzbuf = NULL;
	struct scm_desc desc = {0};
	int i;
	union {
		u8 bytes[BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE];
		u32 words[BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE / sizeof(u32)];
	} key_new;

	tzbuf = ice_buffer;

	memcpy(key_new.bytes, key->raw, key->size);
	if (!key->is_hw_wrapped) {
		for (i = 0; i < ARRAY_SIZE(key_new.words); i++)
			__cpu_to_be32s(&key_new.words[i]);
	}

	memcpy(tzbuf, key_new.bytes, key->size);
	dmac_flush_range(tzbuf, tzbuf + key->size);

	smc_id = TZ_ES_CONFIG_SET_ICE_KEY_ID;
	desc.arginfo = TZ_ES_CONFIG_SET_ICE_KEY_PARAM_ID;
	desc.args[0] = slot;
	desc.args[1] = virt_to_phys(tzbuf);
	desc.args[2] = key->size;
	desc.args[3] = ICE_CIPHER_MODE_XTS_256;
	desc.args[4] = data_unit_mask;


	err = scm_call2_noretry(smc_id, &desc);
	if (err)
		pr_err("%s:SCM call Error: 0x%x slot %d\n",
				__func__, err, slot);

	return err;
}

int crypto_qti_invalidate_key(
		struct crypto_vops_qti_entry *ice_entry, unsigned int slot)
{
	int err = 0;
	uint32_t smc_id = 0;
	struct scm_desc desc = {0};

	smc_id = TZ_ES_INVALIDATE_ICE_KEY_ID;

	desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID;
	desc.args[0] = slot;

	err = scm_call2_noretry(smc_id, &desc);
	if (err)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, err);
	return err;
}

int crypto_qti_tz_raw_secret(const u8 *wrapped_key,
			     unsigned int wrapped_key_size, u8 *secret,
			     unsigned int secret_size)
{
	int err = 0;
	uint32_t smc_id = 0;

	struct scm_desc desc = {0};
	char *tzbuf_key, *secret_key;

	tzbuf_key = ice_buffer;
	secret_key = secret_buffer;
	memcpy(tzbuf_key, wrapped_key, wrapped_key_size);
	dmac_flush_range(tzbuf_key, tzbuf_key + wrapped_key_size);

	smc_id = TZ_ES_RETRIEVE_RAW_SECRET_CE_TYPE_ID;
	desc.arginfo = TZ_ES_RETRIEVE_RAW_SECRET_CE_TYPE_PARAM_ID;
	desc.args[0] = virt_to_phys(tzbuf_key);
	desc.args[1] = wrapped_key_size;
	desc.args[2] = virt_to_phys(secret_key);
	desc.args[3] = secret_size;

	memset(secret_key, 0, secret_size);
	dmac_flush_range(secret_key, secret_key + secret_size);

	err = scm_call2_noretry(smc_id, &desc);
	if (err) {
		pr_err("%s failed to retrieve raw secret\n", __func__, err);
		return err;
	}

	dmac_inv_range(secret_key, secret_key + secret_size);
	memcpy(secret, secret_key, secret_size);

	return err;
}

static int crypto_qti_storage_type(unsigned int *s_type)
{
	char boot[20] = {'\0'};
	char *match = (char *)strnstr(saved_command_line,
				"androidboot.bootdevice=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.bootdevice=")),
			sizeof(boot) - 1);
		if (strnstr(boot, "ufs", strlen(boot)))
			*s_type = UFS_CE;

		return 0;
	}
	return -EINVAL;
}

static int __init crypto_qti_init(void)
{
	return crypto_qti_storage_type(&storage_type);
}

module_init(crypto_qti_init);

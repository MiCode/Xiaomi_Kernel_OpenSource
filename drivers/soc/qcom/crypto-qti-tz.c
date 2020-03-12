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
#include <soc/qcom/qtee_shmbridge.h>
#include <linux/crypto-qti-common.h>
#include "crypto-qti-platform.h"
#include "crypto-qti-tz.h"

unsigned int storage_type = SDCC_CE;

int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key,
			   unsigned int slot, unsigned int data_unit_mask,
			   int capid)
{
	int err = 0;
	uint32_t smc_id = 0;
	char *tzbuf = NULL;
	struct qtee_shm shm;
	struct scm_desc desc = {0};

	err = qtee_shmbridge_allocate_shm(key->size, &shm);
	if (err)
		return -ENOMEM;

	tzbuf = shm.vaddr;

	memcpy(tzbuf, key->raw, key->size);
	dmac_flush_range(tzbuf, tzbuf + key->size);

	smc_id = TZ_ES_CONFIG_SET_ICE_KEY_CE_TYPE_ID;
	desc.arginfo = TZ_ES_CONFIG_SET_ICE_KEY_CE_TYPE_PARAM_ID;
	desc.args[0] = slot;
	desc.args[1] = shm.paddr;
	desc.args[2] = shm.size;
	desc.args[3] = ICE_CIPHER_MODE_XTS_256;
	desc.args[4] = data_unit_mask;
	desc.args[5] = storage_type;


	err = scm_call2_noretry(smc_id, &desc);
	if (err)
		pr_err("%s:SCM call Error: 0x%x slot %d\n",
				__func__, err, slot);

	qtee_shmbridge_free_shm(&shm);

	return err;
}

int crypto_qti_invalidate_key(
		struct crypto_vops_qti_entry *ice_entry, unsigned int slot)
{
	int err = 0;
	uint32_t smc_id = 0;
	struct scm_desc desc = {0};

	smc_id = TZ_ES_INVALIDATE_ICE_KEY_CE_TYPE_ID;

	desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_CE_TYPE_PARAM_ID;
	desc.args[0] = slot;
	desc.args[1] = storage_type;

	err = scm_call2_noretry(smc_id, &desc);
	if (err)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, err);
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

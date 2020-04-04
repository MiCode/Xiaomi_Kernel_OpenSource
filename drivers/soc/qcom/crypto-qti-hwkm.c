// SPDX-License-Identifier: GPL-2.0-only
/*
 * Crypto HWKM library for storage encryption.
 *
 * Copyright (c) 2020, Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/crypto-qti-common.h>
#include <linux/hwkm.h>
#include <linux/module.h>

#include "crypto-qti-ice-regs.h"
#include "crypto-qti-platform.h"

#define TPKEY_SLOT_ICEMEM_SLAVE		0x92
#define KEYMANAGER_ICE_MAP_SLOT(slot)	((slot * 2) + 10)
#define GP_KEYSLOT			134

#define QTI_HWKM_INIT_DONE		0x1

union crypto_cfg {
	__le32 regval[2];
	struct {
		u8 dusize;
		u8 capidx;
		u8 nop;
		u8 cfge;
		u8 dumb[4];
	};
};

static int crypto_qti_hwkm_evict_slot(unsigned int slot)
{
	int err = 0;
	struct hwkm_cmd cmd_clear;
	struct hwkm_rsp rsp_clear;

	memset(&cmd_clear, 0, sizeof(cmd_clear));
	cmd_clear.op = KEY_SLOT_CLEAR;
	cmd_clear.clear.dks = slot;
	err = qti_hwkm_handle_cmd(&cmd_clear, &rsp_clear);
	if (err)
		pr_err("%s: Error with key clear %d\n", __func__, err);

	return err;
}

int crypto_qti_program_key(struct crypto_vops_qti_entry *ice_entry,
			   const struct blk_crypto_key *key, unsigned int slot,
			   unsigned int data_unit_mask, int capid)
{
	int err = 0;
	struct hwkm_cmd cmd_unwrap;
	struct hwkm_cmd cmd_kdf;
	struct hwkm_rsp rsp_unwrap;
	struct hwkm_rsp rsp_kdf;

	struct hwkm_key_policy policy_kdf = {
		.security_lvl = MANAGED_KEY,
		.hw_destination = ICEMEM_SLAVE,
		.key_type = GENERIC_KEY,
		.enc_allowed = true,
		.dec_allowed = true,
		.alg_allowed = AES256_XTS,
		.km_by_nsec_allowed = true,
	};
	struct hwkm_bsve bsve_kdf = {
		.enabled = false,
	};
	u8 ctx[] = {
		0xee, 0xb6, 0xa4, 0xc8, 0x6f, 0x22, 0x5f, 0xda,
		0x18, 0xff, 0x61, 0x07, 0xfb, 0x88, 0x17, 0x7f,
		0xe4, 0x89, 0x8f, 0xed, 0xdb, 0x0c, 0x68, 0xb2,
		0x18, 0xe7, 0x58, 0xd0, 0xf7, 0x79, 0x61, 0xad,
		0x77, 0xc6, 0x4d, 0x2b, 0x53, 0x93, 0x4f, 0x34,
		0xaf, 0x51, 0xab, 0xda, 0x24, 0xa0, 0xa4, 0x76,
		0xf4, 0x09, 0xed, 0xa3, 0x2c, 0xa1, 0x8b, 0xcd,
		0x01, 0xe7, 0x0a, 0x3e, 0x9d, 0x73, 0xac, 0x96,
	};

	union crypto_cfg cfg;

	err = qti_hwkm_clocks(true);
	if (err) {
		pr_err("%s: Error enabling clocks %d\n", __func__, err);
		return err;
	}

	if ((ice_entry->flags & QTI_HWKM_INIT_DONE) != QTI_HWKM_INIT_DONE) {
		err = qti_hwkm_init();
		if (err) {
			pr_err("%s: Error with HWKM init %d\n", __func__, err);
			return err;
		}
		ice_entry->flags |= QTI_HWKM_INIT_DONE;
	}

	/* Unwrap keyblob into a non ICE slot using TP key */
	cmd_unwrap.op = KEY_UNWRAP_IMPORT;
	cmd_unwrap.unwrap.dks = GP_KEYSLOT;
	cmd_unwrap.unwrap.kwk = TPKEY_SLOT_ICEMEM_SLAVE;
	cmd_unwrap.unwrap.sz = (key->size) - RAW_SECRET_SIZE;
	memcpy(cmd_unwrap.unwrap.wkb, (key->raw) + RAW_SECRET_SIZE,
			cmd_unwrap.unwrap.sz);
	err = qti_hwkm_handle_cmd(&cmd_unwrap, &rsp_unwrap);
	if (err) {
		pr_err("%s: Error with key unwrap %d\n", __func__, err);
		return err;
	}

	/* Derive a 512-bit key which will be the key to encrypt/decrypt data */
	cmd_kdf.op = SYSTEM_KDF;
	cmd_kdf.kdf.dks = KEYMANAGER_ICE_MAP_SLOT(slot);
	cmd_kdf.kdf.kdk = GP_KEYSLOT;
	cmd_kdf.kdf.policy = policy_kdf;
	cmd_kdf.kdf.bsve = bsve_kdf;
	cmd_kdf.kdf.sz = 64;
	memcpy(cmd_kdf.kdf.ctx, ctx, HWKM_MAX_CTX_SIZE);

	memset(&cfg, 0, sizeof(cfg));
	cfg.dusize = data_unit_mask;
	cfg.capidx = capid;
	cfg.cfge = 0x80;

	ice_writel(ice_entry, 0x0, (ICE_LUT_KEYS_CRYPTOCFG_R_16 +
					ICE_LUT_KEYS_CRYPTOCFG_OFFSET*slot));
	/* Make sure CFGE is cleared */
	wmb();

	err = qti_hwkm_handle_cmd(&cmd_kdf, &rsp_kdf);
	if (err) {
		pr_err("%s: Error programming key %d\n", __func__, err);
		/* Clear slot if setting key fails */
		err = crypto_qti_hwkm_evict_slot(
				KEYMANAGER_ICE_MAP_SLOT(slot));
		err = crypto_qti_hwkm_evict_slot(GP_KEYSLOT);
		if (err) {
			pr_err("%s: Error clearing slots %d\n",
						__func__, err);
		}
		qti_hwkm_clocks(false);
		return err;
	}

	ice_writel(ice_entry, cfg.regval[0], (ICE_LUT_KEYS_CRYPTOCFG_R_16 +
					ICE_LUT_KEYS_CRYPTOCFG_OFFSET*slot));
	/* Make sure CFGE is enabled before moving forward */
	wmb();

	err = crypto_qti_hwkm_evict_slot(GP_KEYSLOT);
	if (err)
		pr_err("%s: Error unwrapped slot clear %d\n", __func__, err);

	qti_hwkm_clocks(false);

	return err;
}
EXPORT_SYMBOL(crypto_qti_program_key);

int crypto_qti_invalidate_key(struct crypto_vops_qti_entry *ice_entry,
			      unsigned int slot)
{
	int err = 0;

	err = qti_hwkm_clocks(true);
	if (err) {
		pr_err("%s: Error enabling clocks %d\n", __func__, err);
		return err;
	}

	/* Clear key from ICE keyslot */
	err = crypto_qti_hwkm_evict_slot(KEYMANAGER_ICE_MAP_SLOT(slot));
	if (err)
		pr_err("%s: Error with key clear %d\n", __func__, err);

	qti_hwkm_clocks(false);

	return err;
}
EXPORT_SYMBOL(crypto_qti_invalidate_key);

void crypto_qti_disable_platform(struct crypto_vops_qti_entry *ice_entry)
{
	ice_entry->flags &= ~QTI_HWKM_INIT_DONE;
}
EXPORT_SYMBOL(crypto_qti_disable_platform);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto HWKM library for storage encryption");

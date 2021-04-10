// SPDX-License-Identifier: GPL-2.0-only
/*
 * virtio block crypto ops QTI implementation.
 *
 * Copyright (c) 2021, Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/crypto_qti_virt.h>
#include <linux/keyslot-manager.h>

/*keyslot manager for vrtual IO*/
static struct keyslot_manager *virtio_ksm;
/*To get max ice slots for guest vm */
static uint32_t num_ice_slots;

void virtblk_crypto_qti_setup_rq_keyslot_manager(struct request_queue *q)
{
		q->ksm = virtio_ksm;
}
EXPORT_SYMBOL(virtblk_crypto_qti_setup_rq_keyslot_manager);

void virtblk_crypto_qti_destroy_rq_keyslot_manager(struct request_queue *q)
{
		keyslot_manager_destroy(virtio_ksm);
}
EXPORT_SYMBOL(virtblk_crypto_qti_destroy_rq_keyslot_manager);

static inline bool virtblk_keyslot_valid(unsigned int slot)
{
	/*
	 * slot numbers range from 0 to max available
	 * slots for vm.
	 */
	return slot < num_ice_slots;
}

static int virtblk_crypto_qti_keyslot_program(struct keyslot_manager *ksm,
						const struct blk_crypto_key *key,
						unsigned int slot)
{
	int err = 0;

	if (!virtblk_keyslot_valid(slot)) {
		pr_err("%s: key slot is not valid\n",
			__func__);
		return -EINVAL;
	}
	err = crypto_qti_virt_program_key(key, slot);
	if (err) {
		pr_err("%s: program key failed with error %d\n",
			__func__, err);
		err = crypto_qti_virt_invalidate_key(slot);
		if (err) {
			pr_err("%s: invalidate key failed with error %d\n",
				__func__, err);
			return err;
		}
	}
	return err;
}

static int virtblk_crypto_qti_keyslot_evict(struct keyslot_manager *ksm,
					const struct blk_crypto_key *key,
					unsigned int slot)
{
	int err = 0;

	if (!virtblk_keyslot_valid(slot)) {
		pr_err("%s: key slot is not valid\n",
			__func__);
		return -EINVAL;
	}
	err = crypto_qti_virt_invalidate_key(slot);
	if (err) {
		pr_err("%s: evict key failed with error %d\n",
			__func__, err);
		return err;
	}
	return err;
}

static int virtblk_crypto_qti_derive_raw_secret(struct keyslot_manager *ksm,
						const u8 *wrapped_key,
						unsigned int wrapped_key_size,
						u8 *secret,
						unsigned int secret_size)
{
	int err = 0;

	if (wrapped_key_size <= RAW_SECRET_SIZE) {
		pr_err("%s: Invalid wrapped_key_size: %u\n",
			__func__, wrapped_key_size);
		err = -EINVAL;
		return err;
	}
	if (secret_size != RAW_SECRET_SIZE) {
		pr_err("%s: Invalid secret size: %u\n",
			__func__, secret_size);
		err = -EINVAL;
		return err;
	}
	err = crypto_qti_virt_derive_raw_secret_platform(wrapped_key,
							 wrapped_key_size,
							 secret,
							 secret_size);
	return err;
}

static const struct keyslot_mgmt_ll_ops virtio_blk_crypto_qti_ksm_ops = {
	.keyslot_program        = virtblk_crypto_qti_keyslot_program,
	.keyslot_evict          = virtblk_crypto_qti_keyslot_evict,
	.derive_raw_secret      = virtblk_crypto_qti_derive_raw_secret,
};

int virtblk_init_crypto_qti_spec(void)
{
	int err = 0;
	int cap_idx = 0;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];

	/* Actual determination of capabilities for UFS/EMMC for different
	 * encryption modes are done in the back end in case of virtualization
	 * driver, so initializing this to 0xFFFFFFFF meaning it supports
	 * all crypto capabilities to please the keyslot manager. feeding
	 * as input parameter to the keyslot manager
	 */
	for (cap_idx = 0; cap_idx < BLK_ENCRYPTION_MODE_MAX; cap_idx++)
		crypto_modes_supported[cap_idx] = 0xFFFFFFFF;
	crypto_modes_supported[BLK_ENCRYPTION_MODE_INVALID] = 0;

	/* Get max number of ice  slots for guest vm */
	err = crypto_qti_virt_ice_get_info(&num_ice_slots);
	if (err) {
		pr_err("crypto_qti_virt_ice_get_info failed error = %d\n", err);
		return err;
	}
	/* Return from here inacse keyslot manger is already created */
	if (virtio_ksm)
		return 0;

	/* create keyslot manager and which will manage the keyslots for all
	 * virtual disks
	 */
	virtio_ksm = keyslot_manager_create(NULL,
					num_ice_slots,
					&virtio_blk_crypto_qti_ksm_ops,
					BLK_CRYPTO_FEATURE_STANDARD_KEYS |
					BLK_CRYPTO_FEATURE_WRAPPED_KEYS,
					crypto_modes_supported,
					NULL);

	if (!virtio_ksm)
		return  -ENOMEM;

	pr_info("%s: keyslot manager created\n", __func__);

	return err;
}
EXPORT_SYMBOL(virtblk_init_crypto_qti_spec);

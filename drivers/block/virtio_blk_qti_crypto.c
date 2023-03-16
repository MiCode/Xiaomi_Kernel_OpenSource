// SPDX-License-Identifier: GPL-2.0-only
/*
 * virtio block crypto ops QTI implementation.
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/crypto_qti_virt.h>
#include <linux/keyslot-manager.h>

/*keyslot manager for vrtual IO*/
static struct blk_keyslot_manager virtio_ksm;
/* initialize ksm only once */
static bool is_ksm_initalized;
/*To get max ice slots for guest vm */
static uint32_t num_ice_slots;

void virtblk_crypto_qti_setup_rq_keyslot_manager(struct request_queue *q)
{
	blk_ksm_register(&virtio_ksm, q);
}
EXPORT_SYMBOL(virtblk_crypto_qti_setup_rq_keyslot_manager);

static inline bool virtblk_keyslot_valid(unsigned int slot)
{
	/*
	 * slot numbers range from 0 to max available
	 * slots for vm.
	 */
	return slot < num_ice_slots;
}

static int virtblk_crypto_qti_keyslot_program(struct blk_keyslot_manager *ksm,
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

static int virtblk_crypto_qti_keyslot_evict(struct blk_keyslot_manager *ksm,
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

static int virtblk_crypto_qti_derive_raw_secret(struct blk_keyslot_manager *ksm,
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
	if (wrapped_key_size > 64) {
		err = crypto_qti_virt_derive_raw_secret_platform(wrapped_key,
								 wrapped_key_size,
								 secret,
								 secret_size);
	} else {
		memcpy(secret, wrapped_key, secret_size);
	}
	return err;
}

static const struct blk_ksm_ll_ops virtio_blk_crypto_qti_ksm_ops = {
	.keyslot_program        = virtblk_crypto_qti_keyslot_program,
	.keyslot_evict          = virtblk_crypto_qti_keyslot_evict,
	.derive_raw_secret      = virtblk_crypto_qti_derive_raw_secret,
};

int virtblk_init_crypto_qti_spec(struct device *dev)
{
	int err = 0;
	unsigned int crypto_modes_supported[BLK_ENCRYPTION_MODE_MAX];

	memset(crypto_modes_supported, 0, sizeof(crypto_modes_supported));

	/* Actual determination of capabilities for UFS/EMMC for different
	 * encryption modes are done in the back end (host operating system)
	 * in case of virtualization driver, so will get crypto capabilities
	 * from the back end. The received capabilities is feeded as input
	 * parameter to keyslot manager
	 */
	err = crypto_qti_virt_get_crypto_capabilities(crypto_modes_supported,
						      sizeof(crypto_modes_supported));
	if (err) {
		pr_err("crypto_qti_virt_get_crypto_capabilities failed error = %d\n", err);
		return err;
	}
	/* Get max number of ice  slots for guest vm */
	err = crypto_qti_virt_ice_get_info(&num_ice_slots);
	if (err) {
		pr_err("crypto_qti_virt_ice_get_info failed error = %d\n", err);
		return err;
	}
	/* Return from here incase keyslot manager is already initialized */
	if (is_ksm_initalized)
		return 0;

	/* create keyslot manager and which will manage the keyslots for all
	 * virtual disks
	 */
	err = devm_blk_ksm_init(dev, &virtio_ksm, num_ice_slots);
	if (err) {
		pr_err("%s: devm_blk_ksm_init failed\n", __func__);
		return err;
	}
	is_ksm_initalized = true;
	virtio_ksm.ksm_ll_ops = virtio_blk_crypto_qti_ksm_ops;
	/* This value suppose to get from host based on storage type
	 * will remove hard code value later
	 */
	virtio_ksm.max_dun_bytes_supported = 8;
	virtio_ksm.features = BLK_CRYPTO_FEATURE_WRAPPED_KEYS;
	memcpy(virtio_ksm.crypto_modes_supported, crypto_modes_supported,
	       sizeof(crypto_modes_supported));

	pr_info("%s: ksm initialized.\n", __func__);

	return err;
}
EXPORT_SYMBOL(virtblk_init_crypto_qti_spec);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Virtual library for storage encryption");

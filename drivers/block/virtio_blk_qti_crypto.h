/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _VIRTIO_BLK_QTI_CRYPTO_H
#define _VIRTIO_BLK_QTI_CRYPTO_H

#include <linux/device.h>
#include <linux/blkdev.h>

/**
 * This function intializes the supported crypto capabilities
 * and create keyslot manager to manage keyslots for virtual
 * disks.
 *
 * Return: zero on success, else a -errno value
 */
int virtblk_init_crypto_qti_spec(void);

/**
 * set up a keyslot manager in the virtual disks request_queue
 *
 * @request_queue: virtual disk request queue
 */
void virtblk_crypto_qti_setup_rq_keyslot_manager(struct request_queue *q);
/**
 * destroy keyslot manager
 *
 * @request_queue: virtual disk request queue
 */
void virtblk_crypto_qti_destroy_rq_keyslot_manager(struct request_queue *q);

#endif /* _VIRTIO_BLK_QTI_CRYPTO_H */


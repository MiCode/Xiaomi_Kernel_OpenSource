// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 */

#include <linux/blk-crypto.h>
#include <linux/blkdev.h>
#include <linux/keyslot-manager.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "queue.h"

void mmc_crypto_setup_queue(struct mmc_host *host, struct request_queue *q)
{
	if (host->caps2 & MMC_CAP2_CRYPTO)
		q->ksm = host->ksm;
}
EXPORT_SYMBOL_GPL(mmc_crypto_setup_queue);

void mmc_crypto_free_host(struct mmc_host *host)
{
	keyslot_manager_destroy(host->ksm);
}

void mmc_crypto_prepare_req(struct mmc_queue_req *mqrq)
{
	struct request *req = mqrq->req;
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct mmc_request *mrq = &(mqrq->cmdq_req.mrq);
#else /* let BUG() if SW-CQHCI run to here */
	struct mmc_request *mrq = NULL;
#endif
	const struct bio_crypt_ctx *bc;

	if (!bio_crypt_should_process(req))
		return;

	bc = req->bio->bi_crypt_context;
	mrq->crypto_key_slot = bc->bc_keyslot;
	/*
	 * OTA with ext4 (dun is 512 bytes) used LBA,
	 * with F2FS (dun is 512 bytes), the dun[0] had
	 * multiplied by 8.
	 */
	if (bc->bc_dun[0] == 0xFFFFFFFFFFFFFFFFULL &&
		bc->bc_dun[1] == 0xFFFFFFFFFFFFFFFFULL)
		mrq->data_unit_num = blk_rq_pos(req);
	else
		mrq->data_unit_num = lower_32_bits(bc->bc_dun[0]);
	mrq->crypto_key = bc->bc_key;
}
EXPORT_SYMBOL_GPL(mmc_crypto_prepare_req);

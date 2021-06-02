/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _MMC_CRYPTO_H
#define _MMC_CRYPTO_H

#ifdef CONFIG_MMC_CRYPTO
#include <linux/keyslot-manager.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/blkdev.h>

#define NUM_KEYSLOTS(host) \
	(((u32)(host->crypto_capabilities.config_count) & 0xFF))

/* vendor's host structure will be hook by mmc_host->private */
static inline void *get_ll_mmc_host(struct mmc_host *host)
{
	return (void *)host->private;
}

static inline bool mmc_keyslot_valid(struct mmc_host *host, unsigned int slot)
{
	/*
	 * The actual number of configurations supported is (CFGC+1), so slot
	 * numbers range from 0 to config_count inclusive.
	 */
	return slot < NUM_KEYSLOTS(host);
}

static inline bool mmc_is_crypto_supported(struct mmc_host *host)
{
	return host->crypto_capabilities.reg_val != 0;
}

static inline bool mmc_is_crypto_enabled(struct mmc_host *host)
{
	return host->caps2 & MMC_CAP2_CRYPTO;
}

struct keyslot_mgmt_ll_ops;

/* Crypto Variant Ops Support */

int mmc_init_crypto(struct mmc_host *host);

int mmc_prepare_mqr_crypto(struct mmc_host *host,
					struct mmc_queue_req *mqr);
int mmc_swcq_prepare_mqr_crypto(struct mmc_host *host,
					struct mmc_request *mrq);

int mmc_complete_mqr_crypto(struct mmc_host *host);

void mmc_crypto_debug(struct mmc_host *host);

int mmc_crypto_suspend(struct mmc_host *host);

int mmc_crypto_resume(struct mmc_host *host);

void mmc_crypto_set_vops(struct mmc_host *host,
			    struct mmc_crypto_variant_ops *crypto_vops);

#else /* CONFIG_MMC_CRYPTO */
#include "queue.h"
static inline bool mmc_keyslot_valid(struct mmc_host *host,
					unsigned int slot)
{
	return false;
}

static inline bool mmc_is_crypto_supported(struct mmc_host *host)
{
	return false;
}

static inline bool mmc_is_crypto_enabled(struct mmc_host *host)
{
	return false;
}

static inline int mmc_init_crypto(struct mmc_host *host)
{
	return 0;
}

static inline int mmc_swcq_prepare_mqr_crypto(struct mmc_host *host,
					struct mmc_request *mrq)
{
	return 0;
}

static inline int mmc_complete_mqr_crypto(struct mmc_host *host)
{
	return 0;
}

static inline void mmc_crypto_debug(struct mmc_host *host) { }

static inline int mmc_crypto_suspend(struct mmc_host *host)
{
	return 0;
}

static inline int mmc_crypto_resume(struct mmc_host *host)
{
	return 0;
}

static inline void mmc_crypto_set_vops(struct mmc_host *host,
			struct mmc_crypto_variant_ops *crypto_vops) { }

#endif /* CONFIG_MMC_CRYPTO */

#endif /* _MMC_CRYPTO_H */

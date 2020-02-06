/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2019 Google LLC
 *
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 */

#ifndef _CQHCI_CRYPTO_H
#define _CQHCI_CRYPTO_H

#ifdef CONFIG_MMC_CQHCI_CRYPTO
#include <linux/mmc/host.h>
#include "cqhci.h"

static inline int cqhci_num_keyslots(struct cqhci_host *host)
{
	return host->crypto_capabilities.config_count + 1;
}

static inline bool cqhci_keyslot_valid(struct cqhci_host *host,
				       unsigned int slot)
{
	/*
	 * The actual number of configurations supported is (CFGC+1), so slot
	 * numbers range from 0 to config_count inclusive.
	 */
	return slot < cqhci_num_keyslots(host);
}

static inline bool cqhci_host_is_crypto_supported(struct cqhci_host *host)
{
	return host->crypto_capabilities.reg_val != 0;
}

static inline bool cqhci_is_crypto_enabled(struct cqhci_host *host)
{
	return host->caps & CQHCI_CAP_CRYPTO_SUPPORT;
}

/* Functions implementing eMMC v5.2 specification behaviour */
int cqhci_prepare_crypto_desc_spec(struct cqhci_host *host,
				    struct mmc_request *mrq,
				    u64 *ice_ctx);

void cqhci_crypto_enable_spec(struct cqhci_host *host);

void cqhci_crypto_disable_spec(struct cqhci_host *host);

int cqhci_host_init_crypto_spec(struct cqhci_host *host,
				const struct keyslot_mgmt_ll_ops *ksm_ops);

void cqhci_crypto_setup_rq_keyslot_manager_spec(struct cqhci_host *host,
						 struct request_queue *q);

void cqhci_crypto_destroy_rq_keyslot_manager_spec(struct cqhci_host *host,
						   struct request_queue *q);

void cqhci_crypto_set_vops(struct cqhci_host *host,
			    struct cqhci_host_crypto_variant_ops *crypto_vops);

/* Crypto Variant Ops Support */

void cqhci_crypto_enable(struct cqhci_host *host);

void cqhci_crypto_disable(struct cqhci_host *host);

int cqhci_host_init_crypto(struct cqhci_host *host);

void cqhci_crypto_setup_rq_keyslot_manager(struct cqhci_host *host,
					    struct request_queue *q);

void cqhci_crypto_destroy_rq_keyslot_manager(struct cqhci_host *host,
					      struct request_queue *q);

int cqhci_crypto_get_ctx(struct cqhci_host *host,
			       struct mmc_request *mrq,
			       u64 *ice_ctx);

int cqhci_complete_crypto_desc(struct cqhci_host *host,
				struct mmc_request *mrq,
				u64 *ice_ctx);

void cqhci_crypto_debug(struct cqhci_host *host);

int cqhci_crypto_suspend(struct cqhci_host *host);

int cqhci_crypto_resume(struct cqhci_host *host);

int cqhci_crypto_reset(struct cqhci_host *host);

int cqhci_crypto_recovery_finish(struct cqhci_host *host);

int cqhci_crypto_cap_find(void *host_p,  enum blk_crypto_mode_num crypto_mode,
			  unsigned int data_unit_size);

#else /* CONFIG_MMC_CQHCI_CRYPTO */

static inline bool cqhci_keyslot_valid(struct cqhci_host *host,
					unsigned int slot)
{
	return false;
}

static inline bool cqhci_host_is_crypto_supported(struct cqhci_host *host)
{
	return false;
}

static inline bool cqhci_is_crypto_enabled(struct cqhci_host *host)
{
	return false;
}

static inline void cqhci_crypto_enable(struct cqhci_host *host) { }

static inline int cqhci_crypto_cap_find(void *host_p,
					enum blk_crypto_mode_num crypto_mode,
					unsigned int data_unit_size)
{
	return 0;
}

static inline void cqhci_crypto_disable(struct cqhci_host *host) { }

static inline int cqhci_host_init_crypto(struct cqhci_host *host)
{
	return 0;
}

static inline void cqhci_crypto_setup_rq_keyslot_manager(
					struct cqhci_host *host,
					struct request_queue *q) { }

static inline void
cqhci_crypto_destroy_rq_keyslot_manager(struct cqhci_host *host,
					 struct request_queue *q) { }

static inline int cqhci_crypto_get_ctx(struct cqhci_host *host,
				       struct mmc_request *mrq,
				       u64 *ice_ctx)
{
	*ice_ctx = 0;
	return 0;
}

static inline int cqhci_complete_crypto_desc(struct cqhci_host *host,
					     struct mmc_request *mrq,
					     u64 *ice_ctx)
{
	return 0;
}

static inline void cqhci_crypto_debug(struct cqhci_host *host) { }

static inline void cqhci_crypto_set_vops(struct cqhci_host *host,
			struct cqhci_host_crypto_variant_ops *crypto_vops) { }

static inline int cqhci_crypto_suspend(struct cqhci_host *host)
{
	return 0;
}

static inline int cqhci_crypto_resume(struct cqhci_host *host)
{
	return 0;
}

static inline int cqhci_crypto_reset(struct cqhci_host *host)
{
	return 0;
}

static inline int cqhci_crypto_recovery_finish(struct cqhci_host *host)
{
	return 0;
}

#endif /* CONFIG_MMC_CQHCI_CRYPTO */
#endif /* _CQHCI_CRYPTO_H */



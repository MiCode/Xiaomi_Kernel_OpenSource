/* Copyright 2019 Google LLC
 *
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CMDQ_CRYPTO_H
#define _CMDQ_CRYPTO_H

#ifdef CONFIG_MMC_CQ_HCI_CRYPTO
#include <linux/mmc/host.h>
#include "cmdq_hci.h"

static inline int cmdq_num_keyslots(struct cmdq_host *host)
{
	return host->crypto_capabilities.config_count + 1;
}

static inline bool cmdq_keyslot_valid(struct cmdq_host *host,
				       unsigned int slot)
{
	/*
	 * The actual number of configurations supported is (CFGC+1), so slot
	 * numbers range from 0 to config_count inclusive.
	 */
	return slot < cmdq_num_keyslots(host);
}

static inline bool cmdq_host_is_crypto_supported(struct cmdq_host *host)
{
	return host->crypto_capabilities.reg_val != 0;
}

static inline bool cmdq_is_crypto_enabled(struct cmdq_host *host)
{
	return host->caps & CMDQ_CAP_CRYPTO_SUPPORT;
}

/* Functions implementing eMMC v5.2 specification behaviour */
int cmdq_prepare_crypto_desc_spec(struct cmdq_host *host,
				    struct mmc_request *mrq,
				    u64 *ice_ctx);

void cmdq_crypto_enable_spec(struct cmdq_host *host);

void cmdq_crypto_disable_spec(struct cmdq_host *host);

int cmdq_host_init_crypto_spec(struct cmdq_host *host,
				const struct keyslot_mgmt_ll_ops *ksm_ops);

void cmdq_crypto_setup_rq_keyslot_manager_spec(struct cmdq_host *host,
						 struct request_queue *q);

void cmdq_crypto_destroy_rq_keyslot_manager_spec(struct cmdq_host *host,
						   struct request_queue *q);

void cmdq_crypto_set_vops(struct cmdq_host *host,
			    struct cmdq_host_crypto_variant_ops *crypto_vops);

/* Crypto Variant Ops Support */

void cmdq_crypto_enable(struct cmdq_host *host);

void cmdq_crypto_disable(struct cmdq_host *host);

int cmdq_host_init_crypto(struct cmdq_host *host);

void cmdq_crypto_setup_rq_keyslot_manager(struct cmdq_host *host,
					    struct request_queue *q);

void cmdq_crypto_destroy_rq_keyslot_manager(struct cmdq_host *host,
					      struct request_queue *q);

int cmdq_crypto_get_ctx(struct cmdq_host *host,
			       struct mmc_request *mrq,
			       u64 *ice_ctx);

int cmdq_complete_crypto_desc(struct cmdq_host *host,
				struct mmc_request *mrq,
				u64 *ice_ctx);

void cmdq_crypto_debug(struct cmdq_host *host);

int cmdq_crypto_suspend(struct cmdq_host *host);

int cmdq_crypto_resume(struct cmdq_host *host);

int cmdq_crypto_reset(struct cmdq_host *host);

int cmdq_crypto_recovery_finish(struct cmdq_host *host);

int cmdq_crypto_cap_find(void *host_p,  enum blk_crypto_mode_num crypto_mode,
			  unsigned int data_unit_size);

#else /* CONFIG_MMC_CQ_HCI_CRYPTO */

static inline bool cmdq_keyslot_valid(struct cmdq_host *host,
					unsigned int slot)
{
	return false;
}

static inline bool cmdq_host_is_crypto_supported(struct cmdq_host *host)
{
	return false;
}

static inline bool cmdq_is_crypto_enabled(struct cmdq_host *host)
{
	return false;
}

static inline void cmdq_crypto_enable(struct cmdq_host *host) { }

static inline int cmdq_crypto_cap_find(void *host_p,
					enum blk_crypto_mode_num crypto_mode,
					unsigned int data_unit_size)
{
	return 0;
}

static inline void cmdq_crypto_disable(struct cmdq_host *host) { }

static inline int cmdq_host_init_crypto(struct cmdq_host *host)
{
	return 0;
}

static inline void cmdq_crypto_setup_rq_keyslot_manager(
					struct cmdq_host *host,
					struct request_queue *q) { }

static inline void
cmdq_crypto_destroy_rq_keyslot_manager(struct cmdq_host *host,
					 struct request_queue *q) { }

static inline int cmdq_crypto_get_ctx(struct cmdq_host *host,
				       struct mmc_request *mrq,
				       u64 *ice_ctx)
{
	*ice_ctx = 0;
	return 0;
}

static inline int cmdq_complete_crypto_desc(struct cmdq_host *host,
					     struct mmc_request *mrq,
					     u64 *ice_ctx)
{
	return 0;
}

static inline void cmdq_crypto_debug(struct cmdq_host *host) { }

static inline void cmdq_crypto_set_vops(struct cmdq_host *host,
			struct cmdq_host_crypto_variant_ops *crypto_vops) { }

static inline int cmdq_crypto_suspend(struct cmdq_host *host)
{
	return 0;
}

static inline int cmdq_crypto_resume(struct cmdq_host *host)
{
	return 0;
}

static inline int cmdq_crypto_reset(struct cmdq_host *host)
{
	return 0;
}

static inline int cmdq_crypto_recovery_finish(struct cmdq_host *host)
{
	return 0;
}

#endif /* CONFIG_MMC_CMDQ_CRYPTO */
#endif /* _CMDQ_CRYPTO_H */



/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_MMC_SWCQHCI_CRYPTO_H__
#define __MTK_MMC_SWCQHCI_CRYPTO_H__

#include <linux/keyslot-manager.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/blkdev.h>
#include "mtk-mmc-swcqhci.h"

#ifdef CONFIG_MMC_CRYPTO

/* Crypto Variant Ops Support */
int swcq_mmc_init_crypto(struct mmc_host *host);

int swcq_mmc_start_crypto(struct mmc_host *mmc,
		struct mmc_request *mrq, u32 opcode);

void swcq_mmc_complete_mqr_crypto(struct mmc_host *host);

void swcq_mmc_crypto_debug(struct mmc_host *host);

int swcq_mmc_crypto_suspend(struct mmc_host *host);

int swcq_mmc_crypto_resume(struct mmc_host *host);

#else /* CONFIG_MMC_CRYPTO */

static inline int swcq_mmc_init_crypto(struct mmc_host *host)
{
	return 0;
}

int swcq_mmc_start_crypto(struct mmc_host *mmc,
	struct mmc_request *mrq, u32 opcode)

{
	return 0;
}

static inline void swcq_mmc_complete_mqr_crypto(struct mmc_host *host)
{
}

static inline void swcq_mmc_crypto_debug(struct mmc_host *host) { }

static inline int swcq_mmc_crypto_suspend(struct mmc_host *host)
{
	return 0;
}

static inline int swcq_mmc_crypto_resume(struct mmc_host *host)
{
	return 0;
}

#endif /* CONFIG_MMC_CRYPTO */

#endif /* __MTK_MMC_SWCQHCI_CRYPTO_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020 Google LLC
 */

#ifndef LINUX_MMC_CQHCI_CRYPTO_H
#define LINUX_MMC_CQHCI_CRYPTO_H

#include "cqhci.h"

#ifdef CONFIG_MMC_CRYPTO

bool cqhci_crypto_enable(struct cqhci_host *host);

int cqhci_host_init_crypto(struct cqhci_host *host);

int cqhci_prep_crypto_desc(struct mmc_request *mrq, __le64 *task_desc);

void cqhci_crypto_recovery_finish(struct cqhci_host *host);

#else /* CONFIG_MMC_CRYPTO */

static inline bool cqhci_crypto_enable(struct cqhci_host *host)
{
	return false;
}

static inline int cqhci_host_init_crypto(struct cqhci_host *host)
{
	return 0;
}

static inline int cqhci_prep_crypto_desc(struct mmc_request *mrq,
					 __le64 *task_desc)
{
	return 0;
}

static inline void cqhci_crypto_recovery_finish(struct cqhci_host *host) { }

#endif /* CONFIG_MMC_CRYPTO */

#endif /* LINUX_MMC_CQHCI_CRYPTO_H */

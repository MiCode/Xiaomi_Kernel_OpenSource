// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2021 UNISOC, Inc.
// Author: Zhongwu Zhu <zhongwu.zhu@unisoc.com>
#ifndef __SDHCI_SPRD_SWCQ_H
#define __SDHCI_SPRD_SWCQ_H

#if IS_ENABLED(CONFIG_MMC_SWCQ)
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>

#ifdef CONFIG_SPRD_DEBUG
#include "sdhci.h"
#endif
extern int _sdhci_request_atomic(struct mmc_host *mmc, struct mmc_request *mrq);
extern int sprd_sdhci_request_sync(struct mmc_host *mmc, struct mmc_request *mrq);
extern int mmc_hsq_swcq_init(struct sdhci_host *host,
					struct platform_device *pdev);
extern int sprd_sdhci_irq_request_swcq(struct sdhci_host *host);
#endif
#endif


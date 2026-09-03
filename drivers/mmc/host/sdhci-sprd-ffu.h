/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_FFU_H
#define _SDHCI_SPRD_FFU_H

int mmc_ffu_init(struct sdhci_host *host);
void mmc_ffu_remove(struct sdhci_host *host);

#endif


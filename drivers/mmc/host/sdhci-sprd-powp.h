/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_POWP_H_
#define _SDHCI_SPRD_POWP_H_

int mmc_set_powp(struct mmc_card *card);
int mmc_check_wp_fn(struct mmc_host *host);
int mmc_wp_init(struct mmc_host *mmc);
void mmc_wp_remove(struct mmc_host *mmc);

#endif


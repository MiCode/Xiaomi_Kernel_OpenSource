/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_DEBUG_H
#define _SDHCI_SPRD_DEBUG_H

extern void mmc_debug_update(struct sdhci_host *host, struct mmc_command *cmd, u32 intmask);
extern void sdhci_sprd_mod_debug_timer(struct sdhci_host *host, unsigned long time);
extern void sdhci_sprd_del_debug_timer(struct sdhci_host *host);
extern void sdhci_sprd_del_debug_timer_sync(struct sdhci_host *host);
extern void sdhci_sprd_debug_timer_setup(struct sdhci_host *host);
extern void sdhci_sprd_debug_init(struct sdhci_host *host);
extern u32 sd_hotplug_cnt;
extern bool anomaly_sd_remove_flag;


#endif


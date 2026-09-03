/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SDHCI_SPRD_DEBUGFS_H
#define _SDHCI_SPRD_DEBUGFS_H

extern bool sdhci_sprd_mmc_debug_judge(void);
extern void sdhci_sprd_mmc_update_throughput(struct sdhci_host *host,
	u64 read, u64 write, u64 read_blk, u64 write_blk);
extern void sdhci_sprd_add_host_debugfs(struct sdhci_host *host);
extern void sdhci_sprd_force_error(bool enable);
extern int sdhci_sprd_should_force_error(void);
extern u32 sdhci_sprd_mmc_update_sd_anomaly(void);
#endif


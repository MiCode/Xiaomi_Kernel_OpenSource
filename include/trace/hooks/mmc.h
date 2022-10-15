/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmc
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_MMC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MMC_H
#include <trace/hooks/vendor_hooks.h>
/* struct blk_mq_queue_data */
#include <linux/blk-mq.h>
/* struct mmc_host */
#include <linux/mmc/host.h>
/* struct mmc_card */
#include <linux/mmc/card.h>
/* struct sdhci_host */
#include "../../drivers/mmc/host/sdhci.h"

/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
DECLARE_HOOK(android_vh_mmc_check_status,
	TP_PROTO(const struct blk_mq_queue_data *bd, int *ret),
	TP_ARGS(bd, ret));

DECLARE_HOOK(android_vh_mmc_sdio_pm_flag_set,
	TP_PROTO(struct mmc_host *host),
	TP_ARGS(host));
DECLARE_HOOK(android_vh_mmc_blk_reset,
	TP_PROTO(struct mmc_host *host, int err),
	TP_ARGS(host, err));
DECLARE_HOOK(android_vh_mmc_blk_mq_rw_recovery,
	TP_PROTO(struct mmc_card *card),
	TP_ARGS(card));
DECLARE_HOOK(android_vh_sd_update_bus_speed_mode,
	TP_PROTO(struct mmc_card *card),
	TP_ARGS(card));
DECLARE_HOOK(android_vh_mmc_attach_sd,
	TP_PROTO(struct mmc_host *host, u32 ocr, int err),
	TP_ARGS(host, ocr, err));
DECLARE_HOOK(android_vh_sdhci_get_cd,
	TP_PROTO(struct sdhci_host *host, bool *allow),
	TP_ARGS(host, allow));
DECLARE_HOOK(android_vh_mmc_gpio_cd_irqt,
	TP_PROTO(struct mmc_host *host, bool *allow),
	TP_ARGS(host, allow));
DECLARE_HOOK(android_vh_mmc_ffu_update_cid,
	TP_PROTO(struct mmc_host *host, struct mmc_card *card, u32 *cid),
	TP_ARGS(host, card, cid));

DECLARE_RESTRICTED_HOOK(android_rvh_mmc_cache_card_properties,
	TP_PROTO(struct mmc_host *host),
	TP_ARGS(host), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_partial_init,
	TP_PROTO(struct mmc_host *host, bool *partial_init),
	TP_ARGS(host, partial_init), 1);

DECLARE_HOOK(android_vh_mmc_update_partition_status,
	TP_PROTO(struct mmc_card *card),
	TP_ARGS(card));

DECLARE_HOOK(android_vh_mmc_sd_update_cmdline_timing,
	TP_PROTO(struct mmc_card *card, int *err),
	TP_ARGS(card, err));

DECLARE_HOOK(android_vh_mmc_sd_update_dataline_timing,
	TP_PROTO(struct mmc_card *card, int *err),
	TP_ARGS(card, err));

#endif /* _TRACE_HOOK_MMC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

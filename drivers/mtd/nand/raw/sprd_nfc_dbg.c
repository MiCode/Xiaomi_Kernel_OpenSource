// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc NAND driver
 *
 * Copyright (C) 2023 Unisoc, Inc.
 * Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
 */
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/flashchip.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/panic_notifier.h>
#include <../drivers/unisoc_platform/sysdump/unisoc_sysdump.h>
#include "sprd_nfc.h"
#include "sprd_nfc_dbg.h"

static struct sprd_nfc_dbg *g_sprd_nfc_dbg;

static void *sprd_nfc_dbg_memcpy(u32 *dest, u32 *source, size_t bytes)
{
	u32 *pdest = dest, *psrc = source;
	size_t x;

	for (x = 0; x < bytes; x += 4) {
		*pdest = *psrc;
		pdest++;
		psrc++;
	}

	return dest;
}

void sprd_nfc_dbg_sav_startup(void *priv)
{
	struct sprd_nfc *host = (struct sprd_nfc *)priv;

	if (!g_sprd_nfc_dbg || g_sprd_nfc_dbg->startup.valid)
		return;

	g_sprd_nfc_dbg->startup.time = ktime_get();
	g_sprd_nfc_dbg->startup.cpu = current->cpu;
	g_sprd_nfc_dbg->startup.pid = current->pid;
	sprd_nfc_dbg_memcpy(g_sprd_nfc_dbg->startup.reg, host->ioaddr,
		       sizeof(g_sprd_nfc_dbg->startup.reg));
	g_sprd_nfc_dbg->startup.valid = true;
}
EXPORT_SYMBOL(sprd_nfc_dbg_sav_startup);

void sprd_nfc_dbg_sav_last_err(void *priv)
{
	struct sprd_nfc *host = (struct sprd_nfc *)priv;

	if (!g_sprd_nfc_dbg)
		return;

	g_sprd_nfc_dbg->last_err.time = ktime_get();
	g_sprd_nfc_dbg->last_err.cpu = current->cpu;
	g_sprd_nfc_dbg->last_err.pid = current->pid;
	sprd_nfc_dbg_memcpy(g_sprd_nfc_dbg->last_err.reg, host->ioaddr,
		       sizeof(g_sprd_nfc_dbg->last_err.reg));
	g_sprd_nfc_dbg->last_err.valid = true;
}
EXPORT_SYMBOL(sprd_nfc_dbg_sav_last_err);

void sprd_nfc_dbg_sav_exec(void *priv, bool post)
{
	struct sprd_nfc *host = (struct sprd_nfc *)priv;
	struct sprd_nfc_dbg_inst *d_nfc;
	struct sprd_snfc_dbg_inst *d_snfc;
	u32 idx;

	if (!g_sprd_nfc_dbg)
		return;

	idx = g_sprd_nfc_dbg->dbg_idx;
	if (!post) {
		g_sprd_nfc_dbg->exec[idx].time_e = 0;
		g_sprd_nfc_dbg->exec[idx].time_s = ktime_get();
	} else
		g_sprd_nfc_dbg->exec[idx].time_e = ktime_get();

	g_sprd_nfc_dbg->exec[idx].cpu = current->cpu;
	g_sprd_nfc_dbg->exec[idx].pid = current->pid;

	if (host->intf_type != INTF_TYPE_SPI) {
		g_sprd_nfc_dbg->exec[idx].magic = SPRD_DBG_MAGIC_NFC;
		d_nfc = &g_sprd_nfc_dbg->exec[idx].d.nfc;
		if (!post) {
			d_nfc->start = sprd_nfc_readl(host, NFC_START_REG);
			d_nfc->cfg0 = sprd_nfc_readl(host, NFC_CFG0_REG);
			d_nfc->cfg4 = sprd_nfc_readl(host, NFC_CFG4_REG);
			d_nfc->tim0 = sprd_nfc_readl(host, NFC_TIMING0_REG);
			d_nfc->sts_match = sprd_nfc_readl(host, NFC_STAT_STSMCH_REG);

			memcpy(d_nfc->inst0, host->ioaddr + NFC_INST00_REG,
			       sizeof(d_nfc->inst0));
		} else {
			d_nfc->intr = sprd_nfc_readl(host, NFC_INT_REG);
			memcpy(d_nfc->status, host->ioaddr + NFC_STATUS0_REG,
			       sizeof(d_nfc->status));
		}
	} else {
		g_sprd_nfc_dbg->exec[idx].magic = SPRD_DBG_MAGIC_SNFC;
		d_snfc = &g_sprd_nfc_dbg->exec[idx].d.snfc;
		if (!post) {
			d_snfc->start = sprd_nfc_readl(host, NFC_START_REG);
			d_snfc->cfg0 = sprd_nfc_readl(host, NFC_CFG0_REG);
			d_snfc->cfg4 = sprd_nfc_readl(host, NFC_CFG4_REG);
			d_snfc->tim0 = sprd_nfc_readl(host, NFC_TIMING0_REG);
			d_snfc->sts_match = sprd_nfc_readl(host, NFC_STAT_STSMCH_REG);

			memcpy(d_snfc->inst0, host->ioaddr + NFC_INST00_REG,
			       sizeof(d_snfc->inst0));
		} else {
			d_snfc->intr = sprd_nfc_readl(host, NFC_INT_REG);
			d_snfc->status = sprd_nfc_readl(host, SNFC_STATUS);
			d_snfc->get_features = sprd_nfc_readl(host, SNFC_FEATURE);
			d_snfc->sub_inst0 = sprd_nfc_readl(host, SNFC_INST0);
		}
	}

	g_sprd_nfc_dbg->exec[idx].valid = true;

	if (post) {
		if (++idx >= ARRAY_SIZE(g_sprd_nfc_dbg->exec))
			idx = 0;
	}
	g_sprd_nfc_dbg->dbg_idx = idx;
}
EXPORT_SYMBOL(sprd_nfc_dbg_sav_exec);

int sprd_nfc_dbg_init(void *priv)
{
	struct sprd_nfc *host = (struct sprd_nfc *)priv;
	struct sprd_nfc_dbg *dbg;

	dbg = devm_kzalloc(host->dev, sizeof(*dbg), GFP_KERNEL);
	if (!dbg)
		return -ENOMEM;

	if (minidump_save_extend_information("sprd_nfc_dbg",
			__pa(dbg),  __pa((u8 *)dbg + sizeof(*dbg))))
		dev_err(host->dev, "failed register to minidump\n");

	dbg->priv = host;
	g_sprd_nfc_dbg = dbg;

	return 0;
}
EXPORT_SYMBOL(sprd_nfc_dbg_init);

/*
 * arch/arm/mach-tegra/tegra_emc.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "tegra_emc.h"

static u8 emc_iso_share = 100;
static unsigned long last_iso_bw;

static struct emc_iso_usage emc_usage_table[TEGRA_EMC_ISO_USE_CASES_MAX_NUM];


void __init tegra_emc_iso_usage_table_init(struct emc_iso_usage *table,
					   int size)
{
	size = min(size, TEGRA_EMC_ISO_USE_CASES_MAX_NUM);

	if (size && table)
		memcpy(emc_usage_table, table,
		       size * sizeof(struct emc_iso_usage));
}

static u8 tegra_emc_get_iso_share(u32 usage_flags, unsigned long iso_bw)
{
	int i;
	u8 iso_share = 100;

	if (usage_flags) {
		for (i = 0; i < TEGRA_EMC_ISO_USE_CASES_MAX_NUM; i++) {
			struct emc_iso_usage *iso_usage = &emc_usage_table[i];
			u32 flags = iso_usage->emc_usage_flags;
			u8 share = iso_usage->iso_usage_share;

			if (!flags)
				continue;

			if (iso_usage->iso_share_calculator)
				share = iso_usage->iso_share_calculator(iso_bw);
			if (!share) {
				WARN(1, "%s: entry %d: iso_share 0\n",
				     __func__, i);
				continue;
			}

			if ((flags & usage_flags) == flags)
				iso_share = min(iso_share, share);
		}
	}
	last_iso_bw = iso_bw;
	emc_iso_share = iso_share;
	return iso_share;
}

unsigned long tegra_emc_apply_efficiency(unsigned long total_bw,
	unsigned long iso_bw, unsigned long max_rate, u32 usage_flags)
{
	u8 efficiency = tegra_emc_get_iso_share(usage_flags, iso_bw);
	if (iso_bw && efficiency && (efficiency < 100)) {
		iso_bw /= efficiency;
		iso_bw = (iso_bw < max_rate / 100) ?
				(iso_bw * 100) : max_rate;
	}

	efficiency = tegra_emc_bw_efficiency;
	if (total_bw && efficiency && (efficiency < 100)) {
		total_bw = total_bw / efficiency;
		total_bw = (total_bw < max_rate / 100) ?
				(total_bw * 100) : max_rate;
	}
	return max(total_bw, iso_bw);
}

#ifdef CONFIG_DEBUG_FS

#define USER_NAME(module) \
[EMC_USER_##module] = #module

static const char *emc_user_names[EMC_USER_NUM] = {
	USER_NAME(DC),
	USER_NAME(VI),
	USER_NAME(MSENC),
	USER_NAME(2D),
	USER_NAME(3D),
	USER_NAME(VDE),
};

static int emc_usage_table_show(struct seq_file *s, void *data)
{
	int i, j;

	seq_printf(s, "EMC USAGE\t\tISO SHARE %% @ last bw %lu\n", last_iso_bw);

	for (i = 0; i < TEGRA_EMC_ISO_USE_CASES_MAX_NUM; i++) {
		u32 flags = emc_usage_table[i].emc_usage_flags;
		u8 share = emc_usage_table[i].iso_usage_share;
		bool fixed_share = true;
		bool first = false;

		if (emc_usage_table[i].iso_share_calculator) {
			share = emc_usage_table[i].iso_share_calculator(
				last_iso_bw);
			fixed_share = false;
		}

		seq_printf(s, "[%d]: ", i);
		if (!flags) {
			seq_printf(s, "reserved\n");
			continue;
		}

		for (j = 0; j < EMC_USER_NUM; j++) {
			u32 mask = 0x1 << j;
			if (!(flags & mask))
				continue;
			seq_printf(s, "%s%s", first ? "+" : "",
				   emc_user_names[j]);
			first = true;
		}
		seq_printf(s, "\r\t\t\t= %d(%s across bw)\n",
			   share, fixed_share ? "fixed" : "vary");
	}
	return 0;
}

static int emc_usage_table_open(struct inode *inode, struct file *file)
{
	return single_open(file, emc_usage_table_show, inode->i_private);
}

static const struct file_operations emc_usage_table_fops = {
	.open		= emc_usage_table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init tegra_emc_iso_usage_debugfs_init(struct dentry *emc_debugfs_root)
{
	struct dentry *d;

	d = debugfs_create_file("emc_usage_table", S_IRUGO, emc_debugfs_root,
		NULL, &emc_usage_table_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_u8("emc_iso_share", S_IRUGO, emc_debugfs_root,
			      &emc_iso_share);
	if (!d)
		return -ENOMEM;

	return 0;
}
#endif

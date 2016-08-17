/*
 * arch/arm/mach-tegra/nct.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>
#include <linux/sysfs.h>

#include <mach/nct.h>
#include "board.h"


#define USE_CRC32_IN_NCT	1

static bool tegra_nct_initialized;
static struct nct_part_head_type nct_head;
static void __iomem *nct_ptr;


int tegra_nct_read_item(u32 index, union nct_item_type *buf)
{
	struct nct_entry_type *entry = NULL;
	u8 *nct;
#if USE_CRC32_IN_NCT
	u32 crc = 0;
#endif

	if (!tegra_nct_initialized)
		return -EPERM;

	entry = kmalloc(sizeof(struct nct_entry_type), GFP_KERNEL);
	if (!entry) {
		pr_err("%s: failed to allocate buffer\n", __func__);
		return -ENOMEM;
	}

	nct = (u8 *)(nct_ptr + NCT_ENTRY_OFFSET +
			(index * sizeof(*entry)));
	memcpy((u8 *)entry, nct, sizeof(*entry));

	/* check CRC integrity */
#if USE_CRC32_IN_NCT
	/* last checksum field of entry is not included in CRC calculation */
	crc = crc32_le(~0, (u8 *)entry, sizeof(*entry) -
			sizeof(entry->checksum)) ^ ~0;
	if (crc != entry->checksum) {
		pr_err("%s: checksum err(0x%x/0x%x)\n", __func__,
			crc, entry->checksum);
		kfree(entry);
		return -EINVAL;
	}
#endif
	/* check index integrity */
	if (index != entry->index) {
		pr_err("%s: index err(0x%x/0x%x)\n", __func__,
			index, entry->index);
		kfree(entry);
		return -EINVAL;
	}

	memcpy(buf, &entry->data, sizeof(*buf));
	kfree(entry);

	return 0;
}

int tegra_nct_is_init(void)
{
	return tegra_nct_initialized;
}

static int __init tegra_nct_init(void)
{
	if (tegra_nct_initialized)
		return 0;

	if ((tegra_nck_start == 0) || (tegra_nck_size == 0)) {
		pr_err("tegra_nct: not configured\n");
		return -ENOTSUPP;
	}

	nct_ptr = ioremap_nocache(tegra_nck_start,
				tegra_nck_size);
	if (!nct_ptr) {
		pr_err("tegra_nct: failed to ioremap memory at 0x%08lx\n",
			tegra_nck_start);
		return -EIO;
	}

	memcpy(&nct_head, nct_ptr, sizeof(nct_head));

	pr_info("%s: magic(0x%x),vid(0x%x),pid(0x%x),ver(V%x.%x),rev(%d)\n",
		__func__,
		nct_head.magicId,
		nct_head.vendorId,
		nct_head.productId,
		(nct_head.version >> 16) & 0xFFFF,
		(nct_head.version & 0xFFFF),
		nct_head.revision);

	if (nct_head.magicId != NCT_MAGIC_ID) {
		pr_err("%s: magic ID error (0x%x/0x%x)\n", __func__,
			nct_head.magicId, NCT_MAGIC_ID);
		BUG();
	}

	tegra_nct_initialized = true;

	return 0;
}

early_initcall(tegra_nct_init);

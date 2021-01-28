// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat_dbg_info.h>

static void __iomem **plat_dbg_info_base;
static unsigned int *plat_dbg_info_size;
static unsigned int *plat_dbg_info_key;
static unsigned int plat_dbg_info_max;

static int __init plat_dbg_info_init(void)
{
	unsigned int *temp_base;
	unsigned int i;
	int ret;

	if (of_chosen) {
		ret = of_property_read_u32(
			of_chosen, "plat_dbg_info,max", &plat_dbg_info_max);

		temp_base = kmalloc_array(
			plat_dbg_info_max, sizeof(unsigned int), GFP_KERNEL);
		plat_dbg_info_key = kmalloc_array(
			plat_dbg_info_max, sizeof(unsigned int), GFP_KERNEL);
		plat_dbg_info_size = kmalloc_array(
			plat_dbg_info_max, sizeof(unsigned int), GFP_KERNEL);
		plat_dbg_info_base = kmalloc_array(
			plat_dbg_info_max, sizeof(void __iomem *), GFP_KERNEL);

		if ((temp_base == NULL) ||
		    (plat_dbg_info_key == NULL) ||
		    (plat_dbg_info_size == NULL) ||
		    (plat_dbg_info_base == NULL)) {
			pr_debug("[PLAT DBG INFO] cannot allocate memory\n");
			ret = -ENOMEM;
			goto alloc_fail;
		}

		ret |= of_property_read_u32_array(
			of_chosen, "plat_dbg_info,key",
			plat_dbg_info_key, plat_dbg_info_max);
		ret |= of_property_read_u32_array(
			of_chosen, "plat_dbg_info,base",
			temp_base, plat_dbg_info_max);
		ret |= of_property_read_u32_array(
			of_chosen, "plat_dbg_info,size",
			plat_dbg_info_size, plat_dbg_info_max);
		if (ret != 0) {
			pr_debug("[PLAT DBG INFO] cannot find property\n");
			ret = -ENODEV;
			goto alloc_fail;
		}

		for (i = 0; i < plat_dbg_info_max; i++) {
			if (temp_base[i] != 0)
				plat_dbg_info_base[i] =
				ioremap(temp_base[i], plat_dbg_info_size[i]);
			else
				plat_dbg_info_base[i] = NULL;

			pr_debug("[PLAT DBG INFO] 0x%x: 0x%x(%p), %d\n",
				plat_dbg_info_key[i], temp_base[i],
				plat_dbg_info_base[i], plat_dbg_info_size[i]);
		}

		kfree(temp_base);
	} else {
		pr_debug("[PLAT DBG INFO] cannot find node \"of_chosen\"\n");
		return -ENODEV;
	}

	return 0;

alloc_fail:
	kfree(temp_base);
	kfree(plat_dbg_info_key);
	kfree(plat_dbg_info_size);
	kfree(plat_dbg_info_base);

	return ret;
}

void __iomem *get_dbg_info_base(unsigned int key)
{
	unsigned int i;

	for (i = 0; i < plat_dbg_info_max; i++) {
		if (plat_dbg_info_key[i] == key)
			return plat_dbg_info_base[i];
	}
	return NULL;
}
EXPORT_SYMBOL(get_dbg_info_base);

unsigned int get_dbg_info_size(unsigned int key)
{
	unsigned int i;

	for (i = 0; i < plat_dbg_info_max; i++) {
		if (plat_dbg_info_key[i] == key)
			return plat_dbg_info_size[i];
	}

	return 0;
}
EXPORT_SYMBOL(get_dbg_info_size);

core_initcall(plat_dbg_info_init);

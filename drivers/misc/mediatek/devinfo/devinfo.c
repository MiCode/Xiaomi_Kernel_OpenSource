// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 */

#include <linux/module.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_devinfo.h>

/**************************************************************************
 *EXTERN FUNCTION
 **************************************************************************/
u32 devinfo_get_size(void)
{
	aee_kernel_warning("mtk devinfo", "module deprecated");
	return 0;
}
EXPORT_SYMBOL(devinfo_get_size);

u32 devinfo_ready(void)
{
	aee_kernel_warning("mtk devinfo", "module deprecated");
	return 0;
}
EXPORT_SYMBOL(devinfo_ready);

u32 get_devinfo_with_index(u32 index)
{
	aee_kernel_warning("mtk devinfo", "module deprecated");
	return 0;
}
EXPORT_SYMBOL(get_devinfo_with_index);

u32 get_hrid_size(void)
{
	aee_kernel_warning("mtk devinfo", "module deprecated");
	return 0;
}
EXPORT_SYMBOL(get_hrid_size);

u32 get_hrid(unsigned char *rid, unsigned char *rid_sz)
{
	aee_kernel_warning("mtk devinfo", "module deprecated");
	return 0;
}
EXPORT_SYMBOL(get_hrid);

MODULE_LICENSE("GPL");



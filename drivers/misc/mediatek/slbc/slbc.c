// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include <slbc.h>

int slbc_enable;
EXPORT_SYMBOL_GPL(slbc_enable);

struct slbc_common_ops *common_ops;

/* need to modify enum slbc_uid */
char *slbc_uid_str[UID_MAX] = {
	"UID_ZERO",
	"UID_MM_VENC",
	"UID_MM_DISP",
	"UID_MM_MDP",
	"UID_MM_VDEC",
	"UID_AI_MDLA",
	"UID_AI_ISP",
	"UID_GPU",
	"UID_HIFI3",
	"UID_CPU",
	"UID_AOV",
	"UID_SH_P2",
	"UID_SH_APU",
	"UID_MML",
	"UID_DSC_IDLE",
	"UID_AINR",
	"UID_TEST_BUFFER",
	"UID_TEST_CACHE",
	"UID_TEST_ACP",
	"UID_DISP",
};
EXPORT_SYMBOL_GPL(slbc_uid_str);

/* bit count */
int popcount(unsigned int x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);

	/* pr_info("popcount %d\n", x & 0x0000003F); */

	return x & 0x0000003F;
}
EXPORT_SYMBOL_GPL(popcount);

u32 slbc_sram_read(u32 offset)
{
	if (common_ops && common_ops->slbc_sram_read)
		return common_ops->slbc_sram_read(offset);
	else
		return 0;
}
EXPORT_SYMBOL_GPL(slbc_sram_read);

void slbc_sram_write(u32 offset, u32 val)
{
	if (common_ops && common_ops->slbc_sram_write)
		return common_ops->slbc_sram_write(offset, val);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_sram_write);

int slbc_request(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_request)
		return common_ops->slbc_request(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_request);

int slbc_release(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_release)
		return common_ops->slbc_release(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_release);

int slbc_power_on(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_power_on)
		return common_ops->slbc_power_on(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_power_on);

int slbc_power_off(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_power_off)
		return common_ops->slbc_power_off(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_power_off);

int slbc_secure_on(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_secure_on)
		return common_ops->slbc_secure_on(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_secure_on);

int slbc_secure_off(struct slbc_data *d)
{
	if (common_ops && common_ops->slbc_secure_off)
		return common_ops->slbc_secure_off(d);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(slbc_secure_off);

void slbc_update_mm_bw(unsigned int bw)
{
	if (common_ops && common_ops->slbc_update_mm_bw)
		return common_ops->slbc_update_mm_bw(bw);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_update_mm_bw);

void slbc_update_mic_num(unsigned int num)
{
	if (common_ops && common_ops->slbc_update_mic_num)
		return common_ops->slbc_update_mic_num(num);
	else
		return;
}
EXPORT_SYMBOL_GPL(slbc_update_mic_num);

void slbc_register_common_ops(struct slbc_common_ops *ops)
{
	common_ops = ops;
}
EXPORT_SYMBOL_GPL(slbc_register_common_ops);

void slbc_unregister_common_ops(struct slbc_common_ops *ops)
{
	common_ops = NULL;
}
EXPORT_SYMBOL_GPL(slbc_unregister_common_ops);

int __init slbc_common_module_init(void)
{
	return 0;
}

late_initcall(slbc_common_module_init);

MODULE_DESCRIPTION("SLBC Driver common v0.1");
MODULE_LICENSE("GPL");

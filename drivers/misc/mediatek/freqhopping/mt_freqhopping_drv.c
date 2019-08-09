/*
 * Copyright (C) 2011 MediaTek, Inc.
 *
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/seq_file.h>
#include "mt_freqhopping_drv.h"

#define SUPPORT_SLT_TEST 0
#define FREQ_HOPPING_DEVICE "mt-freqhopping"
#define FH_PLL_COUNT (g_p_fh_hal_drv->pll_cnt)

static struct mt_fh_hal_driver *g_p_fh_hal_drv;
static struct fh_pll_t *g_fh_drv_pll;
static unsigned int g_drv_pll_count;
static int mt_freqhopping_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);

#ifdef CONFIG_OF
static const struct of_device_id mt_fhctl_of_match[] = {
	{ .compatible = "mediatek,fhctl", },
	{},
};
#endif

static struct miscdevice mt_fh_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtfreqhopping",
};

static int mt_fh_drv_probe(struct platform_device *dev)
{
	int err = 0;

	FH_MSG("EN: mt_fh_probe()");

	err = misc_register(&mt_fh_device);
	if (err)
		FH_MSG("register fh driver error!");

	return 0;
}

static int mt_fh_drv_remove(struct platform_device *dev)
{
	misc_deregister(&mt_fh_device);
	return 0;
}

static void mt_fh_drv_shutdown(struct platform_device *dev)
{
	int id = 0;

	FH_MSG("mt_fh_shutdown");

	FH_MSG("Disable SSC before system reset.\n");

	for (id = 0; id < FH_PLL_COUNT; id++)
		freqhopping_config(id, 0, false);

}


static struct platform_driver freqhopping_driver = {
	.probe = mt_fh_drv_probe,
	.remove = mt_fh_drv_remove,
	.shutdown = mt_fh_drv_shutdown,
	.driver = {
		.name = FREQ_HOPPING_DEVICE,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_fhctl_of_match,
#endif
	},
};


#if SUPPORT_SLT_TEST
static int fhctl_slt_test_result = -2;

static int freqhopping_slt_test_proc_read(struct seq_file *m, void *v)
{
	/* This FH_MSG_NOTICE doesn't need to be replaced by seq_file *m
	 * because userspace parser only needs to know fhctl_slt_test_result
	 */
	pr_info("[FH] %s()\n", __func__);
	pr_info("[FH] fhctl slt result info\n");
	pr_info("[FH] return 0 => [SLT]FHCTL Test Pass!\n");
	pr_info("[FH] return -1 => [SLT]FHCTL Test Fail!\n");
	pr_info("[FH] return (result != 0 && result != -1) ");
	pr_info("[FH] => [SLT]FHCTL Test Untested!\n");

	seq_printf(m, "%d\n", fhctl_slt_test_result);

	fhctl_slt_test_result = -2; /* reset fhctl slt test */

	return 0;
}

static ssize_t freqhopping_slt_test_proc_write(struct file *file,
	const char *buffer, size_t count, loff_t *data)
{
	char *cmd = (char *)__get_free_page(GFP_USER);

	pr_info("[FH] %s()\n", __func__);

	if (cmd == NULL)
		return -ENOMEM;

	if (copy_from_user(cmd, buffer, count))
		goto out;

	cmd[count-1] = '\0';

	if (!strcmp(cmd, "slt_start"))
		fhctl_slt_test_result = g_p_fh_hal_drv->mt_fh_hal_slt_start();
	else
		pr_info("[FH]unknown cmd = %s\n", cmd);
out:
	free_page((unsigned long)cmd);

	return count;
}

static int freqhopping_slt_test_proc_open(struct inode *inode
	, struct file *file)
{
	return single_open(file, freqhopping_slt_test_proc_read, NULL);
}

static const struct file_operations slt_test_fops = {
	.owner = THIS_MODULE,
	.open = freqhopping_slt_test_proc_open,
	.read = seq_read,
	.write = freqhopping_slt_test_proc_write,
	.release = single_release,
};
#endif /* SUPPORT_SLT_TEST */

static int freqhopping_dumpregs_proc_open(struct inode *inode
	, struct file *file)
{
	return single_open(file, g_p_fh_hal_drv->mt_fh_hal_dumpregs_read, NULL);
}

static const struct file_operations dumpregs_fops = {
	.owner = THIS_MODULE,
	.open = freqhopping_dumpregs_proc_open,
	.read = seq_read,
	.release = single_release,
};

static int freqhopping_debug_proc_read(struct seq_file *m, void *v)
{
	struct FH_IO_PROC_READ_T arg;

	arg.m = m;
	arg.v = v;
	arg.pll = g_fh_drv_pll;
	g_p_fh_hal_drv->ioctl(FH_IO_PROC_READ, &arg);
	return 0;
}

static ssize_t freqhopping_debug_proc_write(struct file *file,
				const char *buffer, size_t count, loff_t *data)
{
	int ret, n;
	char kbuf[256];
	size_t len = 0;
	unsigned int cmd, p1, p2, p3, p4, p5, p6, p7;
	struct freqhopping_ioctl fh_ctl;

	p1 = p2 = p3 = p4 = p5 = p6 = p7 = 0;

	FH_MSG("EN: %s", __func__);

	len = min(count, (sizeof(kbuf) - 1));

	if (count == 0)
		return -1;
	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	n = sscanf(kbuf, "%x %x %x %x %x %x %x %x",
				&cmd, &p1, &p2, &p3, &p4, &p5, &p6, &p7);
	fh_ctl.pll_id = p2;
	fh_ctl.ssc_setting.dds = p3;
	fh_ctl.ssc_setting.df = p4;
	fh_ctl.ssc_setting.dt = p5;
	fh_ctl.ssc_setting.upbnd = p6;
	fh_ctl.ssc_setting.lowbnd = p7;
	/* fh_ctl.ssc_setting.freq = 0; */

	/* Check validity of PLL ID */
	if (fh_ctl.pll_id >= FH_PLL_COUNT)
		return -1;


	if (cmd < FH_CMD_INTERNAL_MAX_CMD)
		mt_freqhopping_ioctl(NULL, cmd, (unsigned long)(&fh_ctl));
	else if ((cmd > FH_DCTL_CMD_ID) && (cmd < FH_DCTL_CMD_MAX))
		mt_freqhopping_devctl(cmd, &fh_ctl);
	else
		FH_MSG("CMD error!");


	return count;
}

static int freqhopping_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, freqhopping_debug_proc_read, NULL);
}

static const struct file_operations freqhopping_debug_fops = {
	.owner = THIS_MODULE,
	.open = freqhopping_debug_proc_open,
	.read = seq_read,
	.write = freqhopping_debug_proc_write,
	.release = single_release,
};

#define FH_STATUS_PROC_BANNER \
	"id == fh_status == pll_status == setting_id" \
	" == curr_freq == user_defined ==\r\n"

static int freqhopping_status_proc_read(struct seq_file *m, void *v)
{
	int i = 0;

	FH_MSG("EN: %s", __func__);

	seq_puts(m, "FH status:\r\n");

	seq_puts(m, "===============================================\r\n");
	seq_printf(m, FH_STATUS_PROC_BANNER);
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	for (i = 0; i < g_drv_pll_count; ++i) {
		seq_printf(m, "%2d    %8d      %8d",
				i, g_p_fh_hal_drv->fh_pll_get(i, FH_STATUS),
				g_p_fh_hal_drv->fh_pll_get(i, PLL_STATUS));
		seq_printf(m, "      %8d     %8d ",
				g_p_fh_hal_drv->fh_pll_get(i, SETTING_ID),
				g_p_fh_hal_drv->fh_pll_get(i, CURR_FREQ));

		seq_printf(m, "        %d\r\n",
				g_p_fh_hal_drv->fh_pll_get(i, USER_DEFINED));
	}
#else
	for (i = 0; i < g_drv_pll_count; ++i) {
		seq_printf(m, "%2d    %8d      %8d",
				i, g_fh_drv_pll[i].fh_status,
				g_fh_drv_pll[i].pll_status);
		seq_printf(m, "      %8d     %8d ",
				g_fh_drv_pll[i].setting_id,
				g_fh_drv_pll[i].curr_freq);

		seq_printf(m, "        %d\r\n",
				g_fh_drv_pll[i].user_defined);
	}
#endif
	seq_puts(m, "\r\n");

	return 0;
}
#undef FH_STATUS_PROC_BANNER

static ssize_t freqhopping_status_proc_write(struct file *file,
			const char *buffer, size_t count, loff_t *data)
{
	int ret, n;
	char kbuf[256];
	size_t len = 0;
	unsigned int p1, p2, p3, p4, p5, p6;
	struct freqhopping_ioctl fh_ctl;

	p1 = p2 = p3 = p4 = p5 = p6 = 0;

	FH_MSG("EN: %s", __func__);

	len = min(count, (sizeof(kbuf) - 1));

	if (count == 0)
		return -1;
	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	n = sscanf(kbuf, "%x %x", &p1, &p2);

	fh_ctl.pll_id = p2;
	fh_ctl.ssc_setting.df = 0;
	fh_ctl.ssc_setting.dt = 0;
	fh_ctl.ssc_setting.upbnd = 0;
	fh_ctl.ssc_setting.lowbnd = 0;

	/* Check validity of PLL ID */
	if (fh_ctl.pll_id >= FH_PLL_COUNT)
		return -1;


	if (p1 == 0)
		mt_freqhopping_ioctl(NULL, FH_CMD_DISABLE,
					(unsigned long)(&fh_ctl));
	else
		mt_freqhopping_ioctl(NULL, FH_CMD_ENABLE,
					(unsigned long)(&fh_ctl));


	return count;
}


static int freqhopping_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, freqhopping_status_proc_read, NULL);
}

static const struct file_operations status_fops = {
	.owner = THIS_MODULE,
	.open = freqhopping_status_proc_open,
	.read = seq_read,
	.write = freqhopping_status_proc_write,
	.release = single_release,
};

static int mt_fh_hal_ctrl_lock(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	int retVal = 1;
	unsigned long flags = 0;

	FH_MSG("EN: _fctr_lck %d:%d", fh_ctl->pll_id, enable);

	g_p_fh_hal_drv->mt_fh_lock(&flags);
	retVal = g_p_fh_hal_drv->mt_fh_hal_ctrl(fh_ctl, enable);
	g_p_fh_hal_drv->mt_fh_unlock(&flags);

	return retVal;
}

static int mt_freqhopping_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	/* Get the structure of ioctl */
	int ret = 0;

	struct freqhopping_ioctl *freqhopping_ctl =
			(struct freqhopping_ioctl *)arg;

	FH_MSG("EN:CMD:%d pll id:%d", cmd, freqhopping_ctl->pll_id);

	if (cmd == FH_CMD_ENABLE) {
		ret = mt_fh_hal_ctrl_lock(freqhopping_ctl, true);
	} else if (cmd == FH_CMD_DISABLE) {
		ret = mt_fh_hal_ctrl_lock(freqhopping_ctl, false);
	} else {
		/* Invalid command is not acceptable!! */
		WARN_ON(1);
	}

	/* FH_MSG("Exit"); */
	return ret;
}

int mt_freqhopping_devctl(unsigned int cmd, void *args)
{
	if (!g_p_fh_hal_drv)
		return 1;
	g_p_fh_hal_drv->ioctl(cmd, args);
	return 0;
}
EXPORT_SYMBOL(mt_freqhopping_devctl);

int mt_dfs_general_pll(unsigned int pll_id, unsigned int target_dds)
{
	if ((!g_p_fh_hal_drv) || (!g_p_fh_hal_drv->mt_dfs_general_pll)) {
		FH_MSG("[%s]: g_p_fh_hal_drv->mt_dfs_general_pll is uninit.",
			__func__);
		return 1;
	}

	return g_p_fh_hal_drv->mt_dfs_general_pll(pll_id, target_dds);
}
EXPORT_SYMBOL(mt_dfs_general_pll);

int mt_dfs_armpll(unsigned int pll, unsigned int dds)
{
	if (!g_p_fh_hal_drv) {
		pr_info("[FH] [%s]: g_p_fh_hal_drv is uninitialized."
			, __func__);
		return 1;
	}

	return g_p_fh_hal_drv->mt_dfs_general_pll(pll, dds);
}
EXPORT_SYMBOL(mt_dfs_armpll);

void mt_fh_popod_save(void)
{
	if (!g_p_fh_hal_drv) {
		FH_MSG("[%s]: g_p_fh_hal_drv is uninitialized.", __func__);
		return;
	}
	FH_MSG("EN: %s", __func__);

	g_p_fh_hal_drv->mt_fh_popod_save();
}
EXPORT_SYMBOL(mt_fh_popod_save);

void mt_fh_popod_restore(void)
{
	if (!g_p_fh_hal_drv) {
		FH_MSG("[%s]: g_p_fh_hal_drv is uninitialized.", __func__);
		return;
	}

	FH_MSG("EN: %s", __func__);

	g_p_fh_hal_drv->mt_fh_popod_restore();
}
EXPORT_SYMBOL(mt_fh_popod_restore);

#define PROC_FH_(FOLDER) \
	"/proc/freqhopping/"#FOLDER

static int freqhopping_debug_proc_init(void)
{
	struct proc_dir_entry *prdumpregentry;
#ifdef FH_FULL_PROC_INTERFACE_SUPPORT
	struct proc_dir_entry *prStatusentry;
	struct proc_dir_entry *prDebugentry;
#endif
#if SUPPORT_SLT_TEST
	struct proc_dir_entry *prslttestentry;
#endif /* SUPPORT_SLT_TEST */
	struct proc_dir_entry *fh_proc_dir = NULL;

	pr_info("[FH] %s", __func__);

	fh_proc_dir = proc_mkdir("freqhopping", NULL);
	if (fh_proc_dir == NULL) {
		pr_info("[FH]proc_mkdir fail!");
		return -EINVAL;
	}

	/* /proc/freqhopping/dumpregs */
	prdumpregentry
		= proc_create("dumpregs", 0664, fh_proc_dir, &dumpregs_fops);
	if (prdumpregentry == NULL) {
		pr_info("[FH][%s]: failed to create "PROC_FH_(dumpregs)
			, __func__);
		return -EINVAL;
	}
#ifdef FH_FULL_PROC_INTERFACE_SUPPORT
	/* /proc/freqhopping/status */
	prStatusentry
		= proc_create("status", 0664, fh_proc_dir, &status_fops);
	if (prStatusentry == NULL) {
		pr_info("[FH][%s]: failed to create "PROC_FH_(status)
			, __func__);
		return -EINVAL;
	}

	/* /proc/freqhopping/ */
	prDebugentry
		= proc_create("freqhopping_debug", 0664, fh_proc_dir,
				&freqhopping_debug_fops);
	if (prDebugentry == NULL) {
		pr_info("[FH][%s]: failed to create "PROC_FH_(freqhopping_debug)
			, __func__);
		return -EINVAL;
	}
#endif

#if SUPPORT_SLT_TEST
	/* /proc/freqhopping/slt_test */
	prslttestentry
		= proc_create("slt_test", 0664, fh_proc_dir, &slt_test_fops);
	if (prslttestentry == NULL) {
		pr_info("[FH][%s]: failed to create /proc/freqhopping/slt_test"
			, __func__);
		return -EINVAL;
	}
#endif /* SUPPORT_SLT_TEST */
	return 0;
}

int freqhopping_config(unsigned int pll_id
	, unsigned long vco_freq, unsigned int enable)
{
	struct freqhopping_ioctl fh_ctl;
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	unsigned int fh_status;
	unsigned long flags = 0;
	unsigned int skip_flag = 0;

	if ((g_p_fh_hal_drv->mt_fh_get_init()) == 0) {
		pr_info("[FH]Not init yet, init first.");
		return 1;
	}

	g_p_fh_hal_drv->mt_fh_lock(&flags);

	/* backup */
	fh_status = g_fh_drv_pll[pll_id].fh_status;

	g_fh_drv_pll[pll_id].curr_freq = vco_freq;

	g_fh_drv_pll[pll_id].pll_status
		= (enable > 0) ? FH_PLL_ENABLE : FH_PLL_DISABLE;

	/* prepare freqhopping_ioctl */
	fh_ctl.pll_id = pll_id;

	if (g_fh_drv_pll[pll_id].fh_status != FH_FH_DISABLE)
		g_p_fh_hal_drv->mt_fh_hal_ctrl(&fh_ctl, enable);
	else
		skip_flag = 1;

	/* restore */
	g_fh_drv_pll[pll_id].fh_status = fh_status;

	g_p_fh_hal_drv->mt_fh_unlock(&flags);
#else
	if ((g_p_fh_hal_drv->mt_fh_get_init()) == 0) {
		FH_MSG("Not init yet, init first.");
		return 1;
	}
	fh_ctl.pll_id = pll_id;
	g_p_fh_hal_drv->mt_fh_hal_ctrl(&fh_ctl, enable);
#endif
	return 0;
}
EXPORT_SYMBOL(freqhopping_config);

static int mt_freqhopping_init(void)
{
	int ret;

	pr_info("[FH]: init\n");

	g_p_fh_hal_drv = mt_get_fh_hal_drv();
	if (g_p_fh_hal_drv == NULL) {
		pr_info("[FH]No fh driver is found\n");
		return -EINVAL;
	}

	ret = g_p_fh_hal_drv->mt_fh_hal_init();
	if (ret != 0)
		return ret;

	g_fh_drv_pll = g_p_fh_hal_drv->fh_pll;
	g_drv_pll_count = g_p_fh_hal_drv->pll_cnt;
	g_p_fh_hal_drv->mt_fh_hal_default_conf();

	platform_driver_register(&freqhopping_driver);

	ret = freqhopping_debug_proc_init();
	if (ret != 0)
		return ret;

	pr_info("[FH]: init success\n");

	return 0;
}

static int __init mt_freqhopping_drv_init(void)
{
	int ret;

	ret = mt_freqhopping_init();

	return ret;
}

subsys_initcall(mt_freqhopping_drv_init);


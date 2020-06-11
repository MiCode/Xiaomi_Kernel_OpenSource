// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/skbuff.h>

bool __weak spm_is_md1_sleep(void)
{
	pr_notice("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

void __weak spm_ap_mdsrc_req(u8 lock)
{
	pr_notice("[ccci/dummy] %s is not supported!\n", __func__);
}

int __weak exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
	unsigned int len)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

int __weak switch_sim_mode(int id, char *buf, unsigned int len)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

unsigned int __weak get_sim_switch_type(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

unsigned int __weak get_modem_is_enabled(int md_id)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

int __weak register_ccci_sys_call_back(int md_id, unsigned int id,
	int (*func)(int, int))
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

unsigned int __weak mt_irq_get_pending(unsigned int irq)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

unsigned long __weak ccci_get_md_boot_count(int md_id)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
char * __weak ccci_get_ap_platform(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return "MTxxxxE1";
}

bool __weak is_clk_buf_from_pmic(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return false;
}

void __weak clk_buf_get_swctrl_status(void *swctrl_status)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
}

void __weak clk_buf_get_rf_drv_curr(void *rf_drv_curr)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
}

void __weak clk_buf_save_afc_val(unsigned int afcdac)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
}
int __weak rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
		unsigned int length)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
int __weak mbim_start_xmit(struct sk_buff *skb, int ifid)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}


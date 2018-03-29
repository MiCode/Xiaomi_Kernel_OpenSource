/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include "mach/mt_emi_bm.h"
#include "mach/mt_mem_bw.h"
#include <asm/div64.h>

unsigned long long last_time_ns;
long long LastWordAllCount = 0;

/***********************************************
 * register / unregister g_pGetMemBW CB
 ***********************************************/
static getmembw_func g_pGetMemBW; /* not initialise statics to 0 or NULL */

void mt_getmembw_registerCB(getmembw_func pCB)
{
	if (pCB == NULL) {
		/* reset last time & word all count */
		last_time_ns = sched_clock();
		LastWordAllCount = 0;
		pr_err("[get_mem_bw] register CB is a null function\n");
	}	else {
		pr_err("[get_mem_bw] register CB successful\n");
	}

	g_pGetMemBW = pCB;
}
EXPORT_SYMBOL(mt_getmembw_registerCB);

unsigned long long get_mem_bw(void)
{
	unsigned long long throughput;
	long long WordAllCount;
	unsigned long long current_time_ns, time_period_ns;
	int count;
	long long value;
	int emi_dcm_disable;

#if DISABLE_FLIPPER_FUNC
	return 0;
#endif

	if (g_pGetMemBW)
		return g_pGetMemBW();

	emi_dcm_disable = BM_GetEmiDcm();
	/* pr_err("[get_mem_bw]emi_dcm_disable = %d\n", emi_dcm_disable); */
	current_time_ns = sched_clock();
	time_period_ns = current_time_ns - last_time_ns;
	/* pr_err("[get_mem_bw]last_time=%llu, current_time=%llu,
	period=%llu\n", last_time_ns, current_time_ns, time_period_ns); */

	/* disable_infra_dcm(); */
	BM_SetEmiDcm(0xff);	/* disable EMI dcm */

	BM_Pause();
	WordAllCount = BM_GetWordAllCount();
	if (WordAllCount == 0) {
		LastWordAllCount = 0;
	} else if (WordAllCount == BM_ERR_OVERRUN) {
		pr_debug("[get_mem_bw] BM_ERR_OVERRUN\n");
		WordAllCount = 0;
		LastWordAllCount = 0;
		BM_Enable(0);	/* stop EMI monitors will reset all counters */
		BM_Enable(1);	/* start EMI monitor counting */
	}

	WordAllCount -= LastWordAllCount;
	throughput = (WordAllCount * 8 * 1000);

	if (time_period_ns >= 0xFFFFFFFF) { /* uint32_t overflow */
		do_div(time_period_ns, 10000000);
		do_div(throughput, 10000000);
		pr_debug("[get_mem_bw] time_period_ns overflow lst\n");
		/* pr_err("[get_mem_bw] time_period_ns overflow 1st\n"); */

		if (time_period_ns >= 0xFFFFFFFF) { /* uint32_t overflow */
			do_div(time_period_ns, 1000);
			do_div(throughput, 1000);
			pr_debug("[get_mem_bw] time_period_ns overflow 2nd\n");
			/* pr_err("[get_mem_bw] time_period overflow 2nd\n"); */
		}
	}

	do_div(throughput, time_period_ns);
	/* pr_err("[get_mem_bw]Total MEMORY THROUGHPUT =%llu(MB/s),
	WordAllCount_delta = 0x%llx, LastWordAllCount = 0x%llx\n",
	throughput, WordAllCount, LastWordAllCount); */

	/* stopping EMI monitors will reset all counters */
	BM_Enable(0);

	value = BM_GetWordAllCount();
	count = 100;
	if ((value != 0) && (value > 0xB0000000)) {
		do {
			value = BM_GetWordAllCount();
			if (value != 0) {
				count--;
				BM_Enable(1);
				BM_Enable(0);
			} else
				break;

		} while (count > 0);
	}
	LastWordAllCount = value;

	/*pr_err("[get_mem_bw]loop count = %d,
		last_word_all_count = 0x%llx\n", count, LastWordAllCount); */

	/* start EMI monitor counting */
	BM_Enable(1);
	last_time_ns = sched_clock();

	/* restore_infra_dcm();*/
	BM_SetEmiDcm(emi_dcm_disable);

	/*pr_err("[get_mem_bw]throughput = %llx\n", throughput);*/

	return throughput;
}

static int mem_bw_suspend_callback(struct device *dev)
{
	/*pr_err("[get_mem_bw]mem_bw_suspend_callback\n");*/
	LastWordAllCount = 0;
	BM_Pause();
	return 0;
}

static int mem_bw_resume_callback(struct device *dev)
{
	/* pr_err("[get_mem_bw]mem_bw_resume_callback\n"); */
	BM_Continue();
	return 0;
}

const struct dev_pm_ops mt_mem_bw_pm_ops = {
	.suspend = mem_bw_suspend_callback,
	.resume = mem_bw_resume_callback,
	.restore_early = NULL,
};

struct platform_device mt_mem_bw_pdev = {
	.name = "mt-mem_bw",
	.id = -1,
};

static struct platform_driver mt_mem_bw_pdrv = {
	.probe = NULL,
	.remove = NULL,
	.driver = {
		   .name = "mt-mem_bw",
		   .pm = &mt_mem_bw_pm_ops,
		   .owner = THIS_MODULE,
		   },
};

static int __init mon_kernel_init(void)
{
	int ret = 0;
	int emi_dcm_disable;

	BM_Init();

	/* disable_infra_dcm(); */
	emi_dcm_disable = BM_GetEmiDcm();
	/* pr_err("[MT_MEM_BW]emi_dcm_disable = %d\n", emi_dcm_disable); */
	BM_SetEmiDcm(0xff);	/* disable EMI dcm */

	BM_SetReadWriteType(BM_BOTH_READ_WRITE);
	BM_SetMonitorCounter(1, BM_MASTER_MM1 | BM_MASTER_MM1,
	BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

	BM_SetMonitorCounter(2, BM_MASTER_AP_MCU1 | BM_MASTER_AP_MCU2,
	BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

	BM_SetMonitorCounter(3, BM_MASTER_MD_MCU | BM_MASTER_2G_3G_MDDMA,
	BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

	BM_SetMonitorCounter(4, BM_MASTER_GPU1 | BM_MASTER_GPU1,
	BM_TRANS_TYPE_4BEAT | BM_TRANS_TYPE_8Byte | BM_TRANS_TYPE_BURST_WRAP);

	BM_SetLatencyCounter();

	/* stopping EMI monitors will reset all counters */
	BM_Enable(0);
	/* start EMI monitor counting */
	BM_Enable(1);
	last_time_ns = sched_clock();

	/* restore_infra_dcm(); */
	BM_SetEmiDcm(emi_dcm_disable);	/* enable EMI dcm */

	/* register platform device/driver */
	ret = platform_device_register(&mt_mem_bw_pdev);
	if (ret) {
		pr_err("fail to register mem_bw device @ %s()\n", __func__);
		goto out;
	}

	ret = platform_driver_register(&mt_mem_bw_pdrv);
	if (ret) {
		pr_err("fail to register mem_bw driver @ %s()\n", __func__);
		platform_device_unregister(&mt_mem_bw_pdev);
	}
out:
	return ret;
}

static void __exit mon_kernel_exit(void)
{

}

module_init(mon_kernel_init);
module_exit(mon_kernel_exit);

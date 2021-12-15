/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <mt-plat/sync_write.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <asm/div64.h>
#include <mtk_dramc.h>

#include "emi_mbw.h"
#include "emi_bwl.h"
#include "emi_elm.h"

static bool dump_latency_status;

unsigned long long last_time_ns;
long long LastWordAllCount;

void dump_last_bm(char *buf, unsigned int leng)
{
#if ENABLE_ELM
	elm_dump(buf, leng);
#else
	snprintf(buf, leng, "[EMI] no ELM and runtime BM support\n");
#endif
}

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
		pr_info("[get_mem_bw] register CB is a null function\n");
	}	else {
		pr_info("[get_mem_bw] register CB successful\n");
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
	int emi_dcm_status;

#if DISABLE_FLIPPER_FUNC
	return 0;
#endif

	if (g_pGetMemBW)
		return g_pGetMemBW();

	emi_dcm_status = BM_GetEmiDcm();
	/* pr_info("[get_mem_bw]emi_dcm_status = %d\n", emi_dcm_status); */
	current_time_ns = sched_clock();
	time_period_ns = current_time_ns - last_time_ns;
	/* pr_info("[get_mem_bw]last_time=%llu, current_time=%llu,
	 * period=%llu\n", last_time_ns, current_time_ns, time_period_ns);
	 */

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
		/* pr_info("[get_mem_bw] time_period_ns overflow 1st\n"); */

		if (time_period_ns >= 0xFFFFFFFF) { /* uint32_t overflow */
			do_div(time_period_ns, 1000);
			do_div(throughput, 1000);
			pr_debug("[get_mem_bw] time_period_ns overflow 2nd\n");
			/* pr_info("[get_mem_bw] time_period overflow 2nd\n"); */
		}
	}

	do_div(throughput, time_period_ns);
	/* pr_info("[get_mem_bw]Total MEMORY THROUGHPUT =%llu(MB/s),
	 * WordAllCount_delta = 0x%llx, LastWordAllCount = 0x%llx\n",
	 * throughput, WordAllCount, LastWordAllCount);
	 */

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

	/* pr_info("[get_mem_bw]loop count = %d, last_word_all_count = 0x%llx\n", count, LastWordAllCount); */

	/* start EMI monitor counting */
	BM_Enable(1);
	last_time_ns = sched_clock();

	/* restore_infra_dcm();*/
	BM_SetEmiDcm(emi_dcm_status);

	/*pr_info("[get_mem_bw]throughput = %llx\n", throughput);*/

	return throughput;
}

static int mem_bw_suspend_callback(struct device *dev)
{
	/*pr_info("[get_mem_bw]mem_bw_suspend_callback\n");*/
	LastWordAllCount = 0;
	BM_Pause();
	return 0;
}

static int mem_bw_resume_callback(struct device *dev)
{
	/* pr_info("[get_mem_bw]mem_bw_resume_callback\n"); */
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

int mbw_init(void)
{
	int ret = 0;

	BM_Init();

	/* register platform device/driver */
	ret = platform_device_register(&mt_mem_bw_pdev);
	if (ret) {
		pr_info("fail to register mem_bw device @ %s()\n", __func__);
		goto out;
	}

	ret = platform_driver_register(&mt_mem_bw_pdrv);
	if (ret) {
		pr_info("fail to register mem_bw driver @ %s()\n", __func__);
		platform_device_unregister(&mt_mem_bw_pdev);
	}

	enable_dump_latency();

out:
	return ret;
}

void enable_dump_latency(void)
{
	dump_latency_status = true;
}

void disable_dump_latency(void)
{
	dump_latency_status = false;
}

static int __init dvfs_bwct_init(void)
{
	return 0;
}

late_initcall(dvfs_bwct_init);

#ifdef ENABLE_RUNTIME_BM
static int __init runtime_bm_init(void)
{
	setup_deferrable_timer_on_stack(&bm_timer, bm_timer_callback, 0);
	if (mod_timer(&bm_timer, jiffies + msecs_to_jiffies(RUNTIME_PERIOD)))
		pr_debug("[EMI MBW] Error in BM mod_timer\n");

	return 0;
}
late_initcall(runtime_bm_init);
#endif

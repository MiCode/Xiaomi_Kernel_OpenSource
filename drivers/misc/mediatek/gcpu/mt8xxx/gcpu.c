/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/clk.h>
#include <linux/of_address.h>

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) || \
	defined(CONFIG_TRUSTY))
#include "trustzone/kree/system.h"
#include "trustzone/tz_cross/ta_gcpu.h"
#define GCPU_TEE_ENABLE 1
#else
#define GCPU_TEE_ENABLE 0
#endif

#define GCPU_DEV_NAME "MTK_GCPU"

#define GCPU_INFO(log, args...) \
	pr_info("[%s] [%d] INFO: "log, __func__, __LINE__, ##args)
#define GCPU_DEBUG(log, args...) \
	pr_debug("[%s] [%d] "log, __func__, __LINE__, ##args)

#if GCPU_TEE_ENABLE
static int gcpu_tee_call(uint32_t cmd)
{
	int l_ret = TZ_RESULT_SUCCESS;
	int ret = 0;
	KREE_SESSION_HANDLE test_session;
	/* MTEEC_PARAM param[4]; */
	struct timespec start, end;
	long long ns;

	l_ret = KREE_CreateSession(TZ_TA_GCPU_UUID, &test_session);
	if (l_ret != TZ_RESULT_SUCCESS) {
		GCPU_INFO("KREE_CreateSession error, ret = %x\n", l_ret);
		return 1;
	}

	getnstimeofday(&start);
	l_ret = KREE_TeeServiceCall(test_session, cmd, 0, NULL);
	if (l_ret != TZ_RESULT_SUCCESS) {
		GCPU_INFO("KREE_TeeServiceCall error, ret = %x\n", l_ret);
		ret = 1;
	}
	getnstimeofday(&end);
	ns = ((long long)end.tv_sec - start.tv_sec) * 1000000000 +
		(end.tv_nsec - start.tv_nsec);
	GCPU_DEBUG("gcpu_tee_call, cmd: %d, time: %lld ns\n", cmd, ns);

	l_ret = KREE_CloseSession(test_session);
	if (l_ret != TZ_RESULT_SUCCESS) {
		GCPU_INFO("KREE_CloseSession error, ret = %x\n", l_ret);
		ret = 1;
	}

	return ret;
}

static int gcpu_probe(struct platform_device *pdev)
{
	GCPU_DEBUG("gcpu_probe\n");

	return 0;
}

static int gcpu_remove(struct platform_device *pdev)
{
	GCPU_DEBUG("gcpu_remove\n");

	return 0;
}

static int gcpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int ret = 0;

	GCPU_DEBUG("gcpu_suspend\n");
	if (gcpu_tee_call(TZCMD_GCPU_SUSPEND)) {
		GCPU_INFO("Suspend fail\n");
		ret = 1;
	} else {
		GCPU_DEBUG("Suspend ok\n");
		ret = 0;
	}
	return ret;
}

static int gcpu_resume(struct platform_device *pdev)
{
	int ret = 0;

	GCPU_DEBUG("gcpu_resume\n");
	if (gcpu_tee_call(TZCMD_GCPU_RESUME)) {
		GCPU_INFO("gcpu_resume fail\n");
		ret = 1;
	} else {
		GCPU_DEBUG("gcpu_resume ok\n");
		ret = 0;
	}
	return ret;
}

struct platform_device gcpu_device = {
	.name = GCPU_DEV_NAME,
	.id = -1,
};

static struct platform_driver gcpu_driver = {
	.probe = gcpu_probe,
	.remove = gcpu_remove,
	.suspend = gcpu_suspend,
	.resume = gcpu_resume,
	.driver = {
		.name = GCPU_DEV_NAME,
		.owner = THIS_MODULE,
		}
};
#endif

static int __init gcpu_init(void)
{
#if GCPU_TEE_ENABLE
	int ret = 0;

	GCPU_DEBUG("module init\n");

	ret = platform_driver_register(&gcpu_driver);
	if (ret) {
		GCPU_INFO("Unable to register driver, ret = %d\n", ret);
		return ret;
	}
	gcpu_tee_call(TZCMD_GCPU_KERNEL_INIT_DONE);
#endif
	return 0;
}

static void __exit gcpu_exit(void)
{
#if GCPU_TEE_ENABLE
	GCPU_DEBUG("module exit\n");
#endif
}
module_init(gcpu_init);
module_exit(gcpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("Mediatek GCPU Module");

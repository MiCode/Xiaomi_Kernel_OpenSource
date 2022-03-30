// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sizes.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>

#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "private/tmem_priv.h"
#include "private/tmem_entry.h"

#include "private/ut_cmd.h"
#include "tee_impl/tee_invoke.h"
#include "public/mtee_regions.h"
#include "mtee_impl/tmem_carveout_heap.h"

#include "memory_ssmr.h"
#include "memory_ssheap.h"

static int tmem_open(struct inode *inode, struct file *file)
{
	UNUSED(inode);
	UNUSED(file);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static int tmem_release(struct inode *ino, struct file *file)
{
	UNUSED(ino);
	UNUSED(file);

	pr_info("%s:%d\n", __func__, __LINE__);
	return TMEM_OK;
}

static u32 g_common_mem_handle[TRUSTED_MEM_MAX];
static void trusted_mem_device_chunk_alloc(enum TRUSTED_MEM_TYPE mem_type)
{
	int ret = TMEM_OK;
	u32 alignment = 0, ref_count;
	u32 min_chunk_sz = tmem_core_get_min_chunk_size(mem_type);

	if (IS_ZERO(g_common_mem_handle[mem_type]))
		ret = tmem_core_alloc_chunk(
			mem_type, alignment, min_chunk_sz, &ref_count,
			&g_common_mem_handle[mem_type], NULL, 0, 0);
	else
		pr_info("%d chunk is already allocated, handle:0x%x\n",
			mem_type, g_common_mem_handle[mem_type]);

	if (ret)
		pr_err("%d alloc chunk failed:%d\n", mem_type, ret);
}

static void trusted_mem_device_chunk_free(enum TRUSTED_MEM_TYPE mem_type)
{
	int ret = TMEM_OK;

	if (!IS_ZERO(g_common_mem_handle[mem_type]))
		ret = tmem_core_unref_chunk(
			mem_type, g_common_mem_handle[mem_type], NULL, 0);

	if (ret)
		pr_err("%d free chunk failed:%d\n", mem_type, ret);
	else
		g_common_mem_handle[mem_type] = 0;
}

static void trusted_mem_device_common_operations(u64 cmd, u64 param1,
						 u64 param2, u64 param3)
{
	int device_mem_type = ((u32)cmd) / 10;
	int device_cmd = ((u32)cmd) % 10;

	if (device_mem_type >= TRUSTED_MEM_MAX) {
		pr_err("unsupported mem type: %d (user cmd:%lld)\n",
		       device_mem_type, cmd);
		return;
	}

	switch (device_cmd) {
	case TMEM_DEVICE_COMMON_OPERATION_SSMR_ALLOC:
		tmem_core_ssmr_allocate(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_SSMR_RELEASE:
		tmem_core_ssmr_release(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_SESSION_OPEN:
		tmem_core_session_open(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_SESSION_CLOSE:
		tmem_core_session_close(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_REGION_ON:
		tmem_core_regmgr_online(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_REGION_OFF:
		tmem_core_regmgr_offline(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_CHUNK_ALLOC:
		trusted_mem_device_chunk_alloc(device_mem_type);
		break;
	case TMEM_DEVICE_COMMON_OPERATION_CHUNK_FREE:
		trusted_mem_device_chunk_free(device_mem_type);
		break;
	default:
		pr_err("unsupported device cmd: %d, mem type: %d (user cmd:%lld)\n",
		       device_cmd, device_mem_type, cmd);
		break;
	}
}

static void trusted_mem_region_status_dump(void)
{
	int mem_idx;
	bool is_region_on;
	bool is_dev_registered;

	for (mem_idx = 0; mem_idx < TRUSTED_MEM_MAX; mem_idx++) {
		is_region_on = tmem_core_is_regmgr_region_on(mem_idx);
		is_dev_registered = tmem_core_is_device_registered(mem_idx);
		pr_info("mem%d reg_state:%s registered:%s\n", mem_idx,
			is_region_on ? "BUSY" : "IDLE",
			is_dev_registered ? "YES" : "NO");
	}
}

static void trusted_mem_manual_cmd_invoke(u64 cmd, u64 param1, u64 param2,
					  u64 param3)
{
	if (cmd >= TMEM_DEVICE_COMMON_OPERATION_START
	    && cmd <= TMEM_DEVICE_COMMON_OPERATION_END) {
		trusted_mem_device_common_operations(cmd, param1, param2,
						     param3);
		return;
	}

	switch (cmd) {
	case TMEM_REGION_STATUS_DUMP:
		trusted_mem_region_status_dump();
		break;
	case TMEM_SECMEM_SVP_DUMP_INFO:
#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)
		secmem_svp_dump_info();
#endif
		break;
	case TMEM_SECMEM_FR_DUMP_INFO:
#if IS_ENABLED(CONFIG_TRUSTONIC_TEE_SUPPORT) || \
	IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)
		secmem_fr_dump_info();
#endif
		break;
	case TMEM_SECMEM_WFD_DUMP_INFO:
#if IS_ENABLED(CONFIG_MTK_WFD_SMEM_SUPPORT)
		wfd_smem_dump_info();
#endif
		break;
	case TMEM_SECMEM_DYNAMIC_DEBUG_ENABLE:
#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)
		secmem_dynamic_debug_control(true);
#endif
		break;
	case TMEM_SECMEM_DYNAMIC_DEBUG_DISABLE:
#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)
		secmem_dynamic_debug_control(false);
#endif
		break;
	case TMEM_SECMEM_FORCE_HW_PROTECTION:
#if IS_ENABLED(CONFIG_MTK_SECURE_MEM_SUPPORT)
		secmem_force_hw_protection();
#endif
		break;
	default:
		break;
	}
}

static ssize_t tmem_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	long cmd;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtol(desc, 10, &cmd) != 0)
		return count;

	pr_debug("receives user space cmd '%ld'\n", cmd);

	if ((cmd >= TMEM_MANUAL_CMD_RESERVE_START)
	    && (cmd <= TMEM_MANUAL_CMD_RESERVE_END)) {
		trusted_mem_manual_cmd_invoke((u64)cmd, (u64)0, (u64)0, (u64)0);
	} else {
		trusted_mem_ut_cmd_invoke((u64)cmd, (u64)0, (u64)0, (u64)0);
	}

	return count;
}

static const struct proc_ops tmem_fops = {
	.proc_open = tmem_open,
	.proc_release = tmem_release,
	.proc_ioctl = NULL,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = NULL,
#endif
	.proc_write = tmem_write,
};

static void trusted_mem_create_proc_entry(void)
{
	proc_create("tmem0", 0664, NULL, &tmem_fops);
}

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
#define UT_MULTITHREAD_TEST_DEFAULT_WAIT_COMPLETION_TIMEOUT_MS (900000)
#define UT_SATURATION_STRESS_PMEM_MIN_CHUNK_SIZE (SZ_8M)

static unsigned int ut_multithread_wait_completion_timeout_ms =
	UT_MULTITHREAD_TEST_DEFAULT_WAIT_COMPLETION_TIMEOUT_MS;
int get_multithread_test_wait_completion_time(void)
{
	return ut_multithread_wait_completion_timeout_ms;
}

module_param_named(wait_comp_ms, ut_multithread_wait_completion_timeout_ms,
		   uint, 0644);
MODULE_PARM_DESC(ut_multithread_wait_completion_timeout_ms,
		 "set wait completion timeout in ms for multithread UT tests");

static unsigned int ut_saturation_stress_pmem_min_chunk_size =
	UT_SATURATION_STRESS_PMEM_MIN_CHUNK_SIZE;
int get_saturation_stress_pmem_min_chunk_size(void)
{
	return ut_saturation_stress_pmem_min_chunk_size;
}

module_param_named(pmem_min_chunk_size,
		   ut_saturation_stress_pmem_min_chunk_size, uint, 0644);
MODULE_PARM_DESC(ut_saturation_stress_pmem_min_chunk_size,
		 "set pmem minimal chunk size for saturation stress tests");
#endif

static int trusted_mem_init(struct platform_device *pdev)
{
	pr_info("%s:%d\n", __func__, __LINE__);

#if WITH_SSHEAP_PROC
	if (strncmp(dev_name(&pdev->dev), "ssheap", 6) == 0)
		return ssheap_init(pdev);
#endif

	ssmr_init(pdev);

	trusted_mem_subsys_init();

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	tmem_ut_server_init();
	tmem_ut_cases_init();
#endif

#ifdef TEE_DEVICES_SUPPORT
	tee_smem_devs_init();
#endif

#ifdef MTEE_DEVICES_SUPPORT
	mtee_mchunks_init();
#endif

	if (IS_ENABLED(CONFIG_TMEM_MEMORY_POOL_ALLOCATOR))
		tmem_carveout_init();

	trusted_mem_create_proc_entry();

	pr_info("%s:%d (end)\n", __func__, __LINE__);
	return TMEM_OK;
}

static int trusted_mem_exit(struct platform_device *pdev)
{
#if WITH_SSHEAP_PROC
	if (strncmp(dev_name(&pdev->dev), "ssheap", 6) == 0)
		return ssheap_exit(pdev);
#endif

#ifdef MTEE_DEVICES_SUPPORT
	mtee_mchunks_exit();
#endif

#ifdef TEE_DEVICES_SUPPORT
	tee_smem_devs_exit();
#endif

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
	tmem_ut_cases_exit();
	tmem_ut_server_exit();
#endif

	trusted_mem_subsys_exit();

	return 0;
}

static const struct of_device_id tm_of_match_table[] = {
	{ .compatible = "mediatek,trusted_mem"},
	{ .compatible = "mediatek,trusted_mem_ssheap"},
	{},
};

static struct platform_driver trusted_mem_driver = {
	.probe = trusted_mem_init,
	.remove = trusted_mem_exit,
	.driver = {
		.name = "trusted_mem",
		.of_match_table = tm_of_match_table,
	},
};
module_platform_driver(trusted_mem_driver);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek Trusted Memory Driver");
MODULE_LICENSE("GPL v2");

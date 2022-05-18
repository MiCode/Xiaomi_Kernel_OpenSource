// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem_define.h>
#endif

/* TODO: GPU fix */
#if 0
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
#include <mtk_gpu_power_sspm_ipi.h>
#endif
#endif

#include <swpm_module.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
DEFINE_MUTEX(swpm_mutex);
EXPORT_SYMBOL(swpm_mutex);

struct timer_list swpm_timer;
EXPORT_SYMBOL(swpm_timer);

struct workqueue_struct *swpm_common_wq;
EXPORT_SYMBOL(swpm_common_wq);

unsigned int swpm_log_interval_ms = DEFAULT_LOG_INTERVAL_MS;
EXPORT_SYMBOL(swpm_log_interval_ms);
/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static struct swpm_manager swpm_m = {
	.initialize = 0,
	.plat_ready = 0,
	.mem_ref_tbl = NULL,
	.ref_tbl_size = 0,
};

/***************************************************************************
 *  API
 ***************************************************************************/
int swpm_core_ops_register(struct swpm_core_internal_ops *ops)
{
	if (!swpm_m.plat_ops && ops) {
		swpm_m.plat_ops = ops;
		swpm_m.plat_ready = (swpm_m.plat_ops->cmd) ? true : false;
	} else
		return -1;

	return 0;
}
EXPORT_SYMBOL(swpm_core_ops_register);

void swpm_get_rec_addr(phys_addr_t *phys,
		       phys_addr_t *virt,
		       unsigned long long *size)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	/* get sspm reserved mem */
	*phys = sspm_reserve_mem_get_phys(SWPM_MEM_ID);
	*virt = sspm_reserve_mem_get_virt(SWPM_MEM_ID);
	*size = sspm_reserve_mem_get_size(SWPM_MEM_ID);

	pr_notice("phy_addr = 0x%llx, virt_addr=0x%llx, size = %llu\n",
		  (unsigned long long) *phys,
		  (unsigned long long) *virt,
		  *size);
#endif
}
EXPORT_SYMBOL(swpm_get_rec_addr);

int swpm_interface_manager_init(struct swpm_mem_ref_tbl *ref_tbl,
				unsigned int tbl_size)
{
	if (!ref_tbl)
		return -1;

	swpm_lock(&swpm_mutex);
	swpm_m.initialize = true;
	swpm_m.mem_ref_tbl = ref_tbl;
	swpm_m.ref_tbl_size = tbl_size;
	swpm_unlock(&swpm_mutex);

	return 0;
}
EXPORT_SYMBOL(swpm_interface_manager_init);

int swpm_mem_addr_request(enum swpm_type id, phys_addr_t **ptr)
{
	int ret = 0;

	if (!swpm_m.initialize || !swpm_m.mem_ref_tbl) {
		pr_notice("swpm not initialize\n");
		ret = -1;
		goto end;
	} else if (id >= swpm_m.ref_tbl_size) {
		pr_notice("swpm_type invalid\n");
		ret = -2;
		goto end;
	} else if (!(swpm_m.mem_ref_tbl[id].valid)
		   || !(swpm_m.mem_ref_tbl[id].virt)) {
		ret = -3;
		pr_notice("swpm_mem_ref id not initialize\n");
		goto end;
	}

	swpm_lock(&swpm_mutex);
	*ptr = (swpm_m.mem_ref_tbl[id].virt);
	swpm_unlock(&swpm_mutex);

end:
	return ret;
}
EXPORT_SYMBOL(swpm_mem_addr_request);

int swpm_pmu_enable(enum swpm_pmu_user id,
		    unsigned int enable)
{
	unsigned int cmd_code;

	if (!swpm_m.plat_ready) {
		pr_notice("swpm platform init not ready\n");
		return SWPM_INIT_ERR;
	} else if (id >= NR_SWPM_PMU_USER)
		return SWPM_ARGS_ERR;

	cmd_code = (!!enable) | (id << SWPM_CODE_USER_BIT);
	SWPM_OPS->cmd(SET_PMU, cmd_code);

	return SWPM_SUCCESS;
}
EXPORT_SYMBOL(swpm_pmu_enable);

int swpm_reserve_mem_init(phys_addr_t *virt,
			   unsigned long long *size)
{
	int i;
	unsigned char *ptr;

	if (!virt)
		return -1;

	/* clear reserve mem */
	ptr = (unsigned char *)(uintptr_t)*virt;
	for (i = 0; i < *size; i++)
		ptr[i] = 0x0;

	return 0;
}
EXPORT_SYMBOL(swpm_reserve_mem_init);

static int __init swpm_init(void)
{
	int ret = 0;

	swpm_common_wq = create_workqueue("swpm_common_wq");

	return ret;
}

#ifdef MTK_SWPM_KERNEL_MODULE
static void __exit swpm_deinit(void)
{
	flush_workqueue(swpm_common_wq);
	destroy_workqueue(swpm_common_wq);
}

module_init(swpm_init);
module_exit(swpm_deinit);
#else
device_initcall_sync(swpm_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk swpm module");
MODULE_AUTHOR("MediaTek Inc.");

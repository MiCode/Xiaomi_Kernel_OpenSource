// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#include <sspm_reservedmem_define.h>
#endif

/* TODO: GPU fix */
/* #if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT) */
/* #include <mtk_gpu_power_sspm_ipi.h> */
/* #endif */

#include <swpm_module.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
/* DEFINE_MUTEX(swpm_mutex); */
struct mutex swpm_mutex = __MUTEX_INITIALIZER(swpm_mutex);
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

static struct atomic_notifier_head swpm_notifier_list;
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

int swpm_mem_addr_request(unsigned int id, phys_addr_t **ptr)
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

	if (SWPM_OPS->cmd)
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

/* TODO:deprecated, compatible with old version */
void swpm_set_cmd(unsigned int type, unsigned int cnt)
{
	swpm_lock(&swpm_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	{
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_SET_UPDATE_CNT;
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		qos_d.u.swpm_set_update_cnt.type = type;
		qos_d.u.swpm_set_update_cnt.cnt = cnt;
#endif
		qos_ipi_to_sspm_scmi_command(qos_d.cmd, type, cnt, 0,
					     QOS_IPI_SCMI_SET);
	}
#endif
	swpm_unlock(&swpm_mutex);
}
EXPORT_SYMBOL(swpm_set_cmd);

void swpm_set_only_cmd(unsigned int args_0,
		       unsigned int args_1,
		       unsigned int action,
		       unsigned int type)
{
	unsigned int code;

	swpm_lock(&swpm_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	{
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_INIT;

		code = (SWPM_CMD_MAGIC << SWPM_CMD_MAGIC_BIT)
		 | ((action & SWPM_CMD_ACTION_MASK) << SWPM_CMD_ACTION_BIT)
		 | ((type & SWPM_CMD_TYPE_MASK) << SWPM_CMD_TYPE_BIT);
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		qos_d.u.swpm_init.dram_addr = args_0;
		qos_d.u.swpm_init.dram_size = args_1;
		qos_d.u.swpm_init.dram_ch_num = code;
#endif
		qos_ipi_to_sspm_scmi_command(qos_d.cmd, args_0, args_1, code,
					     QOS_IPI_SCMI_SET);
	}
#endif
	swpm_unlock(&swpm_mutex);
}
EXPORT_SYMBOL(swpm_set_only_cmd);

unsigned int swpm_set_and_get_cmd(unsigned int args_0,
				  unsigned int args_1,
				  unsigned int action,
				  unsigned int type)
{
	unsigned int ret = 0;
	unsigned int code;

	swpm_lock(&swpm_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&              \
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	if (type < NR_SWPM_CMD_TYPE) {
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_INIT;

		code = (SWPM_CMD_MAGIC << SWPM_CMD_MAGIC_BIT)
		 | ((action & SWPM_CMD_ACTION_MASK) << SWPM_CMD_ACTION_BIT)
		 | ((type & SWPM_CMD_TYPE_MASK) << SWPM_CMD_TYPE_BIT);
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCMI)
		qos_d.u.swpm_init.dram_addr = args_0;
		qos_d.u.swpm_init.dram_size = args_1;
		qos_d.u.swpm_init.dram_ch_num = code;
#endif
		qos_ipi_to_sspm_scmi_command(qos_d.cmd,
					     args_0, args_1, code,
					     QOS_IPI_SCMI_SET);
		ret = qos_ipi_to_sspm_scmi_command(qos_d.cmd,
						   args_0, args_1, code,
						   QOS_IPI_SCMI_GET);
	}
#endif
	swpm_unlock(&swpm_mutex);

	return ret;
}
EXPORT_SYMBOL(swpm_set_and_get_cmd);

int swpm_register_event_notifier(struct notifier_block *nb)
{
	int err;

	err = atomic_notifier_chain_register(&swpm_notifier_list, nb);
	return err;
}
EXPORT_SYMBOL_GPL(swpm_register_event_notifier);

int swpm_unregister_event_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&swpm_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(swpm_unregister_event_notifier);

int swpm_call_event_notifier(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&swpm_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(swpm_call_event_notifier);

static int __init swpm_init(void)
{
	int ret = 0;

	swpm_common_wq = create_workqueue("swpm_common_wq");

	ATOMIC_INIT_NOTIFIER_HEAD(&swpm_notifier_list);

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

/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>
#include <linux/compat.h>
#include <linux/freezer.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/of_platform.h>

#include "teei_client.h"
#include "teei_common.h"
#include "teei_id.h"
#include "smc_id.h"
/* #include "TEEI.h" */
#include "tz_service.h"
#include "nt_smc_call.h"
#include "teei_client_main.h"
#include "utos_version.h"

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/completion.h>

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>
#include "sched_status.h"
#include "teei_smc_struct.h"
#include "utdriver_macro.h"
#include "teei_log.h"
#include "teei_cancel_cmd.h"
#include "teei_id.h"
#include "teei_client_main.h"
#include "switch_queue.h"
#include "teei_capi.h"
#include "teei_fp.h"
#include "teei_keymaster.h"
#include "irq_register.h"
#include "tz_log.h"
#include "notify_queue.h"
#include "teei_smc_call.h"
#ifdef TUI_SUPPORT
#include <utr_tui_cmd.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#endif
#include "../teei_fp/fp_func.h"

#include <teei_secure_api.h>

#include <fdrv.h>
#include <linux/topology.h>

#if CONFIG_MICROTRUST_TZ_DRIVER_MTK_BOOTPROF
#define TEEI_BOOT_FOOTPRINT(str) log_boot(str)
#else
#define TEEI_BOOT_FOOTPRINT(str) IMSG_PRINTK("%s\n", str)
#endif

#define DECLARE_SEMA(name, init_value) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, init_value)

DECLARE_SEMA(boot_sema, 0);
DECLARE_SEMA(fdrv_sema, 0);
DECLARE_SEMA(ut_pm_count_sema, 1);
DECLARE_SEMA(fdrv_lock, 1);
DECLARE_SEMA(api_lock, 1);
#ifdef TUI_SUPPORT
DECLARE_SEMA(tui_notify_sema, 0);
#endif
DECLARE_COMPLETION(boot_decryto_lock);

enum {
	TEEI_BOOT_OK = 0,
	TEEI_BOOT_ERROR_CREATE_TLOG_BUF = 1,
	TEEI_BOOT_ERROR_CREATE_TLOG_THREAD = 2,
	TEEI_BOOT_ERROR_CREATE_VFS_ADDR = 3,
	TEEI_BOOT_ERROR_LOAD_SOTER_FAILED = 4,
	TEEI_BOOT_ERROR_INIT_CMD_BUFF_FAILED = 5,
	TEEI_BOOT_ERROR_INIT_UTGATE_FAILED = 6,
	TEEI_BOOT_ERROR_INIT_SERVICE1_FAILED = 7,
	TEEI_BOOT_ERROR_INIT_CAPI_FAILED = 8,
	TEEI_BOOT_ERROR_INIT_SERVICE2_FAILED = 9,
	TEEI_BOOT_ERROR_LOAD_TA_FAILED = 10,
};

struct teei_boot_error_item {
	unsigned int id;
	char *str;
};
struct teei_boot_error_item teei_boot_error_items[] = {
	{ TEEI_BOOT_OK, "TEEI_BOOT_OK"},
	{ TEEI_BOOT_ERROR_CREATE_TLOG_BUF,
			"TEEI_BOOT_ERROR_CREATE_TLOG_BUF" },
	{ TEEI_BOOT_ERROR_CREATE_TLOG_THREAD,
			"TEEI_BOOT_ERROR_CREATE_TLOG_THREAD" },
	{ TEEI_BOOT_ERROR_CREATE_VFS_ADDR,
			"TEEI_BOOT_ERROR_CREATE_VFS_ADDR" },
	{ TEEI_BOOT_ERROR_LOAD_SOTER_FAILED,
			"TEEI_BOOT_ERROR_LOAD_SOTER_FAILED" },
	{ TEEI_BOOT_ERROR_INIT_CMD_BUFF_FAILED,
			"TEEI_BOOT_ERROR_INIT_CMD_BUFF_FAILED" },
	{ TEEI_BOOT_ERROR_INIT_UTGATE_FAILED,
			"TEEI_BOOT_ERROR_INIT_UTGATE_FAILED" },
	{ TEEI_BOOT_ERROR_INIT_SERVICE1_FAILED,
			"TEEI_BOOT_ERROR_INIT_SERVICE1_FAILED" },
	{ TEEI_BOOT_ERROR_INIT_CAPI_FAILED,
			"TEEI_BOOT_ERROR_INIT_CAPI_FAILED" },
	{ TEEI_BOOT_ERROR_INIT_SERVICE2_FAILED,
			"TEEI_BOOT_ERROR_INIT_SERVICE2_FAILED" },
	{ TEEI_BOOT_ERROR_LOAD_TA_FAILED,
			"TEEI_BOOT_ERROR_LOAD_TA_FAILED" }
};

char *teei_boot_error_to_string(uint32_t id)
{
	int i = 0;

	for (i = 0; i < (sizeof(teei_boot_error_items)
			/ sizeof(struct teei_boot_error_item)); i++) {
		if (id == teei_boot_error_items[i].id)
			return teei_boot_error_items[i].str;
	}

	return "TEEI_BOOT_ERROR_NDEFINED";
}

struct workqueue_struct *secure_wq;

unsigned long message_buff;
unsigned long bdrv_message_buff;
unsigned long fdrv_message_buff;
static int current_cpu_id;

#if !defined(CONFIG_ARCH_MT6580) && !defined(CONFIG_ARCH_MT6570)
static int tz_driver_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu);
static struct notifier_block tz_driver_cpu_notifer = {
	.notifier_call = tz_driver_cpu_callback,
};
#endif

#ifdef TUI_SUPPORT
static struct notifier_block tui_notifier = {
	.notifier_call = tui_notify_reboot,
	.next = NULL,
	.priority = INT_MAX,
};
#endif

struct teei_shared_mem_head {
	int shared_mem_cnt;
	struct list_head shared_mem_list;
};

struct boot_stage1_struct {
	unsigned long vfs_phy_addr;
	unsigned long tlog_phy_addr;
};

struct boot_switch_core_struct {
	unsigned long from;
	unsigned long to;
};

asmlinkage long sys_setpriority(int which, int who, int niceval);
asmlinkage long sys_getpriority(int which, int who);

int forward_call_flag;
int irq_call_flag;
int fp_call_flag;
int keymaster_call_flag;
unsigned long teei_config_flag;
unsigned int soter_error_flag;
unsigned long boot_vfs_addr;
unsigned long boot_soter_flag;
unsigned long ut_pm_count;
unsigned long device_file_cnt;

/* ARMv8.2 for CA55, CA75 etc */
static int teei_cpu_id_arm82[] = {
	0x81000000, 0x81000100, 0x81000200, 0x81000300,
	0x81000400, 0x81000500, 0x81000600, 0x81000700,
	0x81000800, 0x81000900, 0x81000a00, 0x81000b00};

/* ARMv8 */
static int teei_cpu_id_arm80[] = {
		0x0000, 0x0001, 0x0002, 0x0003,
		0x0100, 0x0101, 0x0102, 0x0103,
		0x0200, 0x0201, 0x0202, 0x0203};

static int *teei_cpu_id;

unsigned int teei_flags;
static dev_t teei_config_device_no;
static struct cdev teei_config_cdev;
static struct class *config_driver_class;

struct teei_contexts_head_t  teei_contexts_head;

struct timeval stime;
struct timeval etime;
struct smc_call_struct smc_call_entry;
struct task_struct *teei_switch_task;
static struct cpumask mask = { CPU_BITS_NONE };
static struct class *driver_class;
static dev_t teei_client_device_no;
static struct cdev teei_client_cdev;
struct mutex device_cnt_mutex;
struct boot_stage1_struct boot_stage1_entry;
struct init_cmdbuf_struct init_cmdbuf_entry;
struct boot_switch_core_struct boot_switch_core_entry;

DECLARE_COMPLETION(global_down_lock);
EXPORT_SYMBOL_GPL(global_down_lock);
DEFINE_KTHREAD_WORKER(ut_fastcall_worker);

struct semaphore smc_lock;

static struct tz_driver_state *tz_drv_state;

struct tz_driver_state *get_tz_drv_state(void)
{
	return tz_drv_state;
}

int tz_call_notifier_register(struct notifier_block *n)
{
	struct tz_driver_state *s = get_tz_drv_state();

	if (!s) {
		IMSG_ERROR("tz_driver_state is NULL\n");
		return -EFAULT;
	}

	return atomic_notifier_chain_register(&s->notifier, n);
}

int tz_call_notifier_unregister(struct notifier_block *n)
{
	struct tz_driver_state *s = get_tz_drv_state();

	if (!s) {
		IMSG_ERROR("tz_driver_state is NULL\n");
		return -EFAULT;
	}

	return atomic_notifier_chain_unregister(&s->notifier, n);
}

void *tz_malloc(size_t size, int flags)
{
	void *ptr = kmalloc(size, flags | GFP_ATOMIC);
	return ptr;
}

void *tz_malloc_shared_mem(size_t size, int flags)
{
#ifdef UT_DMA_ZONE
	return (void *) __get_free_pages(flags | GFP_DMA,
					get_order(ROUND_UP(size, SZ_4K)));
#else
	return (void *) __get_free_pages(flags,
					get_order(ROUND_UP(size, SZ_4K)));
#endif
}

void tz_free_shared_mem(void *addr, size_t size)
{
	free_pages((unsigned long)addr, get_order(ROUND_UP(size, SZ_4K)));
}

void ut_pm_mutex_lock(struct mutex *lock)
{
	/* add_work_entry(LOCK_PM_MUTEX, (unsigned long)lock); */
	mutex_lock(lock);
}


void ut_pm_mutex_unlock(struct mutex *lock)
{
	/* add_work_entry(UNLOCK_PM_MUTEX, (unsigned long)lock); */
	mutex_unlock(lock);
}

int get_current_cpuid(void)
{
	return current_cpu_id;
}


void secondary_boot_stage2(void *info)
{
	unsigned long smc_type = 2;

	smc_type = teei_secure_call(N_SWITCH_TO_T_OS_STAGE2, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
}

static void boot_stage2(void)
{
	int retVal = 0;

	retVal = add_work_entry(BOOT_STAGE2, 0);
}

int switch_to_t_os_stages2(void)
{
	down(&(smc_lock));

	forward_call_flag = GLSCH_LOW;
	boot_stage2();

	down(&(boot_sema));

	return 0;
}

void secondary_load_tee(void *info)
{
	unsigned long smc_type = 2;

	smc_type = teei_secure_call(N_INVOKE_T_LOAD_TEE, 0, 0, 0);
	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);
}


static void load_tee(void)
{
	add_work_entry(LOAD_TEE, 0);
}


void set_sch_load_img_cmd(void)
{
	struct message_head msg_head;

	memset(&msg_head, 0, sizeof(struct message_head));

	msg_head.invalid_flag = VALID_TYPE;
	msg_head.message_type = STANDARD_CALL_TYPE;
	msg_head.child_type = N_INVOKE_T_LOAD_TEE_CMD;

	memcpy((void *)message_buff, &msg_head, sizeof(struct message_head));

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + MESSAGE_SIZE);
}


int t_os_load_image(void)
{
	ut_pm_mutex_lock(&pm_mutex);
	down(&smc_lock);
	forward_call_flag = GLSCH_LOW;
	set_sch_load_img_cmd();
	load_tee();

	down(&(boot_sema));
	ut_pm_mutex_unlock(&pm_mutex);

	return 0;
}

void secondary_boot_stage1(void *info)
{
	struct boot_stage1_struct *cd = (struct boot_stage1_struct *)info;
	unsigned long smc_type = 2;
	/* with a rmb() */
	rmb();

	smc_type = teei_secure_call(N_INIT_T_BOOT_STAGE1,
				cd->vfs_phy_addr, cd->tlog_phy_addr, 0);

	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	/* with a wmb() */
	wmb();

}


static void boot_stage1(unsigned long vfs_addr, unsigned long tlog_addr)
{
	int retVal = 0;

	boot_stage1_entry.vfs_phy_addr = vfs_addr;
	boot_stage1_entry.tlog_phy_addr = tlog_addr;

	/* with a wmb() */
	wmb();

	retVal = add_work_entry(BOOT_STAGE1,
				(unsigned long)(&boot_stage1_entry));

	/* with a rmb() */
	rmb();
}

#define TZ_PREFER_BIND_CORE (4)
static bool is_prefer_core(int cpu)
{
	/* bind to a specific core */
	if (cpu == TZ_PREFER_BIND_CORE)
		return true;

	return false;
}

static int find_prefer_core(int excluded_cpu)
{
	int i = 0;
	int prefer_core = -1;

	/* search for prefer cpu firstly */
	for_each_online_cpu(i) {
		if (i == excluded_cpu)
			continue;

		if (is_prefer_core(i)) {
			prefer_core = i;
			break;
		}
	}

	/* if prefer is found, return directly */
	if (prefer_core != -1)
		return prefer_core;

	/* if not found, then search for other online cpu */
	for_each_online_cpu(i) {
		if (i == excluded_cpu)
			continue;

		prefer_core = i;
		/* break when next active cpu has been selected */
		break;
	}

	return prefer_core;
}

static bool is_prefer_core_binded(void)
{
	unsigned int curr = get_current_cpuid();

	if (is_prefer_core(curr))
		return true;

	return false;
}

static bool is_prefer_core_onlined(void)
{
	int i = 0;

	for_each_online_cpu(i) {
		if (is_prefer_core(i))
			return true;
	}

	return false;
}

int handle_switch_core(int cpu)
{
	int switch_to_cpu_id = 0;

	switch_to_cpu_id = find_prefer_core(cpu);

	IMSG_DEBUG("[%s][%d]before cpumask set cpu, find %d\n",
				__func__, __LINE__, switch_to_cpu_id);

	set_cpus_allowed_ptr(teei_switch_task, cpumask_of(switch_to_cpu_id));

	teei_secure_call(N_SWITCH_CORE,
			teei_cpu_id[switch_to_cpu_id], teei_cpu_id[cpu], 0);

	current_cpu_id = switch_to_cpu_id;

	IMSG_DEBUG("change cpu id from %d(0x%lx) to %d(0x%lx)\n",
		cpu, teei_cpu_id[cpu],
		switch_to_cpu_id, teei_cpu_id[switch_to_cpu_id]);

	return 0;
}

int handle_move_core(int cpu)
{
	int original_cpu_id = 0;
	int target_cpu_id = cpu;

	original_cpu_id = get_current_cpuid();

	IMSG_DEBUG("[%s][%d]before cpumask set cpu, find %d\n",
					__func__, __LINE__, target_cpu_id);

	set_cpus_allowed_ptr(teei_switch_task, cpumask_of(target_cpu_id));

	teei_secure_call(N_SWITCH_CORE,
		teei_cpu_id[target_cpu_id], teei_cpu_id[original_cpu_id], 0);

	current_cpu_id = target_cpu_id;
	IMSG_DEBUG("change cpu id from [%d] to [%d]\n",
		target_cpu_id, original_cpu_id);

	return 0;
}

int tz_move_core(uint32_t cpu_id)
{
	ut_pm_mutex_lock(&pm_mutex);
	if (!cpu_online(cpu_id)) {
		IMSG_ERROR("The CPU %d is offline !\n", cpu_id);
		ut_pm_mutex_unlock(&pm_mutex);
		return -EINVAL;
	}
	add_work_entry(MOVE_CORE, (unsigned long)cpu_id);
	ut_pm_mutex_unlock(&pm_mutex);

	return 0;
}

static int tz_driver_cpu_callback(struct notifier_block *self,
		unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	unsigned int sched_cpu = get_current_cpuid();

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		if (cpu == sched_cpu) {
			IMSG_DEBUG("cpu down prepare for %d.\n", cpu);
			add_work_entry(SWITCH_CORE,
					(unsigned long)(unsigned long)cpu);
		} else if (is_prefer_core(cpu))
			IMSG_DEBUG("cpu down prepare for prefer %d.\n", cpu);
		else if (!is_prefer_core_binded()
				&& is_prefer_core_onlined()) {
			IMSG_DEBUG("cpu down prepare for changing %d %d.\n",
								sched_cpu, cpu);
			add_work_entry(SWITCH_CORE,
				(unsigned long)(unsigned long)sched_cpu);
		}
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

struct init_cmdbuf_struct {
	unsigned long phy_addr;
	unsigned long fdrv_phy_addr;
	unsigned long bdrv_phy_addr;
	unsigned long tlog_phy_addr;
};

struct init_cmdbuf_struct init_cmdbuf_entry;


void secondary_init_cmdbuf(void *info)
{
	struct init_cmdbuf_struct *cd = (struct init_cmdbuf_struct *)info;
	unsigned long smc_type = 2;

	/* with a rmb() */
	rmb();

	IMSG_DEBUG("[%s][%d] message = %lx, fdrv msg = %lx, bdrv_msg = %lx\n",
		__func__, __LINE__,
		(unsigned long)cd->phy_addr, (unsigned long)cd->fdrv_phy_addr,
		(unsigned long)cd->bdrv_phy_addr);

	smc_type = teei_secure_call(N_INIT_T_FC_BUF,
				cd->phy_addr, cd->fdrv_phy_addr, 0);

	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	smc_type = teei_secure_call(N_INIT_T_FC_BUF,
				cd->bdrv_phy_addr, cd->tlog_phy_addr, 0);

	while (smc_type == SMC_CALL_INTERRUPTED_IRQ)
		smc_type = teei_secure_call(NT_SCHED_T, 0, 0, 0);

	/* with a wmb() */
	wmb();
}

static void init_cmdbuf(unsigned long phy_address,
			unsigned long fdrv_phy_address,
			unsigned long bdrv_phy_address,
			unsigned long tlog_phy_address)
{
	int retVal = 0;

	init_cmdbuf_entry.phy_addr = phy_address;
	init_cmdbuf_entry.fdrv_phy_addr = fdrv_phy_address;
	init_cmdbuf_entry.bdrv_phy_addr = bdrv_phy_address;
	init_cmdbuf_entry.tlog_phy_addr = tlog_phy_address;

	/* with a wmb() */
	wmb();
	Flush_Dcache_By_Area((unsigned long)&init_cmdbuf_entry,
				(unsigned long)&init_cmdbuf_entry
				+ sizeof(struct init_cmdbuf_struct));

	retVal = add_work_entry(INIT_CMD_CALL,
				(unsigned long)(&init_cmdbuf_entry));

	/* with a rmb() */
	rmb();
}

int set_soter_version(void)
{
	unsigned int versionlen = 0;
	char *version = NULL;

	memcpy(&versionlen, message_buff, sizeof(unsigned int));
	if (versionlen > 0 && versionlen < 100) {
		version = kmalloc(versionlen + 1, GFP_KERNEL);
		if (version == NULL)
			return -1;
		memset(version, 0, versionlen + 1);
		memcpy(version, message_buff + 4, versionlen);
	} else {
		return -2;
	}
	TEEI_BOOT_FOOTPRINT(version);
	kfree(version);

	return 0;
}

long create_cmd_buff(void)
{
#ifdef UT_DMA_ZONE
	message_buff = (unsigned long)__get_free_pages(GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	message_buff = (unsigned long)__get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if ((unsigned char *)message_buff == NULL) {
		IMSG_ERROR("[%s][%d] Create message buffer failed!\n",
							__FILE__, __LINE__);
		return -ENOMEM;
	}
#ifdef UT_DMA_ZONE
	fdrv_message_buff = (unsigned long)__get_free_pages(
				GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	fdrv_message_buff = (unsigned long)__get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if ((unsigned char *)fdrv_message_buff == NULL) {

		IMSG_ERROR("[%s][%d] Create fdrv message buffer failed!\n",
							__FILE__, __LINE__);

		free_pages(message_buff,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

		return -ENOMEM;
	}

#ifdef UT_DMA_ZONE
	bdrv_message_buff = (unsigned long)__get_free_pages(
				GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#else
	bdrv_message_buff = (unsigned long)__get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
#endif
	if ((unsigned char *)bdrv_message_buff == NULL) {
		IMSG_ERROR("[%s][%d] Create bdrv message buffer failed!\n",
							__FILE__, __LINE__);
		free_pages(message_buff,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

		free_pages(fdrv_message_buff,
				get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

		return -ENOMEM;
	}

	IMSG_DEBUG("[%s][%d] message = %lx,  fdrv msg = %lx, bdrv_msg = %lx\n",
			__func__, __LINE__,
			(unsigned long)virt_to_phys((void *)message_buff),
			(unsigned long)virt_to_phys((void *)fdrv_message_buff),
			(unsigned long)virt_to_phys((void *)bdrv_message_buff));

	init_cmdbuf((unsigned long)virt_to_phys((void *)message_buff),
			(unsigned long)virt_to_phys((void *)fdrv_message_buff),
			(unsigned long)virt_to_phys((void *)bdrv_message_buff),
			(unsigned long)NULL);

	return 0;
}

long teei_service_init_first(void)
{
	long retVal = 0;

	IMSG_DEBUG("[%s][%d] begin to create nq buffer!\n", __func__, __LINE__);

	retVal = create_nq_buffer();
	if (retVal < 0) {
		IMSG_ERROR("[%s][%d] create nq buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	IMSG_DEBUG("[%s][%d] begin to create cancel command buffer!\n",
						__func__, __LINE__);

	cancel_message_buff = create_cancel_fdrv(CANCEL_MESSAGE_SIZE);
	if ((unsigned char *)cancel_message_buff == NULL) {
		IMSG_ERROR("[%s][%d] create cancel buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;


	IMSG_DEBUG("[%s][%d] begin to create keymaster buffer!\n",
						__func__, __LINE__);

	keymaster_buff_addr = create_keymaster_fdrv(KEYMASTER_BUFF_SIZE);
	if ((unsigned char *)keymaster_buff_addr == NULL) {
		IMSG_ERROR("[%s][%d] create keymaster buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}

	if (soter_error_flag == 1)
		return -1;

	retVal = create_all_fdrv();
	if (retVal < 0) {
		IMSG_ERROR("[%s][%d] create_all_fdrv failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	/**
	 * init service handler
	 */
	retVal = init_all_service_handlers();
	if (retVal < 0) {
		IMSG_ERROR("[%s][%d] init_all_service_handlers failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

	return 0;
}

long teei_service_init_second(void)
{
	IMSG_DEBUG("[%s][%d] begin to create fp buffer!\n",
						__func__, __LINE__);

	fp_buff_addr = create_fp_fdrv(FP_BUFF_SIZE);
	if ((unsigned char *)fp_buff_addr == NULL) {
		IMSG_ERROR("[%s][%d] create fp buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;

#ifdef TUI_SUPPORT
	IMSG_DEBUG("[%s][%d] begin to tui display command buffer!\n",
						__func__, __LINE__);

	tui_display_message_buff = create_tui_buff(
				TUI_DISPLAY_BUFFER, TUI_DISPLAY_SYS_NO);

	if ((unsigned char *)tui_display_message_buff == NULL) {
		IMSG_ERROR("[%s][%d] create tui display buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}

	if (soter_error_flag == 1)
		return -1;

	IMSG_DEBUG("[%s][%d] begin to tui notice command buffer!\n",
						__func__, __LINE__);

	tui_notice_message_buff = create_tui_buff(
				TUI_NOTICE_BUFFER, TUI_NOTICE_SYS_NO);

	if ((unsigned char *)tui_notice_message_buff == NULL) {
		IMSG_ERROR("[%s][%d] create tui notice buffer failed!\n",
						__func__, __LINE__);
		return -1;
	}
	if (soter_error_flag == 1)
		return -1;
#endif

	return 0;
}

/**
 * @brief  init TEEI Framework
 * init Soter OS
 * init Global Schedule
 * init Forward Call Service
 * init CallBack Service
 * @return
 */

static int init_teei_framework(void)
{
	long retVal = 0;
	struct tz_log_state *s = dev_get_platdata(
				&tz_drv_state->tz_log_pdev->dev);

	phys_addr_t tz_log_buf_pa = page_to_phys(s->log_pages);

	boot_soter_flag = START_STATUS;

	mutex_init(&device_cnt_mutex);

	secure_wq = create_workqueue("Secure Call");

	TEEI_BOOT_FOOTPRINT("TEEI WorkQueue Created");

#ifdef UT_DMA_ZONE
	boot_vfs_addr = (unsigned long)__get_free_pages(GFP_KERNEL | GFP_DMA,
					get_order(ROUND_UP(VFS_SIZE, SZ_4K)));
#else
	boot_vfs_addr = (unsigned long) __get_free_pages(GFP_KERNEL,
					get_order(ROUND_UP(VFS_SIZE, SZ_4K)));
#endif
	if ((unsigned char *)boot_vfs_addr == NULL)
		return TEEI_BOOT_ERROR_CREATE_VFS_ADDR;

	TEEI_BOOT_FOOTPRINT("TEEI VFS Buffer Created");

	down(&(smc_lock));
	boot_stage1((unsigned long)virt_to_phys((void *)boot_vfs_addr),
						(unsigned long)tz_log_buf_pa);

	down(&(boot_sema));

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Stage1 Completed");

	free_pages(boot_vfs_addr, get_order(ROUND_UP(VFS_SIZE, SZ_4K)));

	boot_soter_flag = END_STATUS;
	if (soter_error_flag == 1)
		return TEEI_BOOT_ERROR_LOAD_SOTER_FAILED;

	down(&smc_lock);
	retVal = create_cmd_buff();
	up(&smc_lock);
	if (retVal < 0)
		return TEEI_BOOT_ERROR_INIT_CMD_BUFF_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI BOOT CMD Buffer Created");

	set_soter_version();

	switch_to_t_os_stages2();

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Stage2 Completed");

	if (soter_error_flag == 1)
		return TEEI_BOOT_ERROR_INIT_UTGATE_FAILED;

	retVal = teei_service_init_first();
	if (retVal == -1)
		return TEEI_BOOT_ERROR_INIT_SERVICE1_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Service1 Inited");

	retVal = teei_new_capi_init();
	if (retVal < 0)
		return TEEI_BOOT_ERROR_INIT_CAPI_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI NEW CAPI Inited");

	/* waiting for keymaster shm ready and anable the keymaster IOCTL */
	up(&keymaster_api_lock);
	TEEI_BOOT_FOOTPRINT("TEEI BOOT Keymaster Unlocked");

	/* android notify the uTdriver that the TAs is ready !*/
	wait_for_completion(&boot_decryto_lock);
	TEEI_BOOT_FOOTPRINT("TEEI BOOT Decrypt Unlocked");

	retVal = teei_service_init_second();
	TEEI_BOOT_FOOTPRINT("TEEI BOOT Service2 Inited");
	if (retVal == -1)
		return TEEI_BOOT_ERROR_INIT_SERVICE2_FAILED;

	t_os_load_image();
	TEEI_BOOT_FOOTPRINT("TEEI BOOT Load TEES Completed");
	if (soter_error_flag == 1)
		return TEEI_BOOT_ERROR_LOAD_TA_FAILED;

	teei_config_flag = 1;
	complete(&global_down_lock);
	wake_up(&__fp_open_wq);
	TEEI_BOOT_FOOTPRINT("TEEI BOOT All Completed");

	return TEEI_BOOT_OK;
}

/**
 * @brief
 *
 * @param	file
 * @param	cmd
 * @param	arg
 *
 * @return
 */

int is_teei_ready(void)
{
	return teei_flags;
}
EXPORT_SYMBOL(is_teei_ready);

static long teei_config_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int retVal = 0;
	struct init_param param;

	switch (cmd) {

	case TEEI_CONFIG_IOCTL_INIT_TEEI:
		if (teei_flags != 1) {
			long res;
			int i;

			res = copy_from_user(&param, (void *)arg,
					sizeof(struct init_param));
			if (res) {
				IMSG_ERROR("failed to copy from user\n");
				retVal = -EINVAL;
				goto err;
			}

			retVal = init_teei_framework();

			TEEI_BOOT_FOOTPRINT(
				teei_boot_error_to_string(retVal));

			teei_flags = 1;

			for (i = 0; i < param.uuid_count; i++)
				tz_load_drv_by_str(param.uuids[i]);

			param.flag = teei_flags;

			res = copy_to_user((void *)arg, &param,
					sizeof(struct init_param));
			if (res)
				IMSG_ERROR("failed to copy to user\n");
		}

		break;
	case TEEI_CONFIG_IOCTL_UNLOCK:
		complete(&boot_decryto_lock);
		break;

	default:
			retVal = -EINVAL;
	}

err:
	return retVal;
}

/**
 * @brief		The open operation of /dev/teei_config device node.
 *
 * @param		inode
 * @param		file
 *
 * @return		ENOMEM: no enough memory in the linux kernel
 *			0: on success
 */

static int teei_config_open(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * @brief	Map the vma with the free pages
 *
 * @param	filp
 * @param	vma
 *
 * @return	0: success
 *		EINVAL: Invalid parament
 *		ENOMEM: No enough memory
 */

static int teei_config_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

/**
 * @brief		The release operation of /dev/teei_config device node.
 *
 * @param		inode: device inode structure
 * @param		file:  struct file
 *
 * @return		0: on success
 */
static int teei_config_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * @brief
 */
static const struct file_operations teei_config_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = teei_config_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = teei_config_ioctl,
#endif
	.open = teei_config_open,
	.mmap = teei_config_mmap,
	.release = teei_config_release
};

/**
 * @brief TEEI Agent Driver initialization
 * initialize Microtrust Tee environment
 * @return
 **/


static int teei_config_init(void)
{
	int retVal = 0;
	struct device *class_dev = NULL;

	retVal = alloc_chrdev_region(&teei_config_device_no,
						0, 1, TEEI_CONFIG_DEV);

	if (retVal < 0) {
		IMSG_ERROR("alloc_chrdev_region failed %x.\n", retVal);
		return retVal;
	}

	config_driver_class = class_create(THIS_MODULE, TEEI_CONFIG_DEV);
	if (IS_ERR(config_driver_class)) {
		retVal = -ENOMEM;
		IMSG_ERROR("class_create failed %x\n", retVal);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(config_driver_class, NULL,
			teei_config_device_no, NULL, TEEI_CONFIG_DEV);

	if (class_dev == NULL) {
		IMSG_ERROR("class_device_create failed %x\n", retVal);
		retVal = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&teei_config_cdev, &teei_config_fops);
	teei_config_cdev.owner = THIS_MODULE;

	retVal = cdev_add(&teei_config_cdev,
			MKDEV(MAJOR(teei_config_device_no), 0), 1);

	if (retVal < 0) {
		IMSG_ERROR("cdev_add failed %x\n", retVal);
		goto class_device_destroy;
	}

	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, teei_config_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(teei_config_device_no, 1);
return_fn:
	return retVal;
}

/**
 * @brief
 *
 * @param file
 * @param cmd
 * @param arg
 *
 * @return
 */
static long teei_client_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int retVal = 0;
	void *argp = (void __user *) arg;

	if (teei_config_flag == 0) {
		IMSG_ERROR("Error: soter is NOT ready!\n");
		return -ECANCELED;
	}

	if (cmd == TEEI_CANCEL_COMMAND) {
		IMSG_DEBUG("[%s][%d] TEEI_CANCEL_COMMAND beginning.\n",
						__func__, __LINE__);

		/*ut_pm_mutex_lock(&pm_mutex);*/

		if (copy_from_user((void *)cancel_message_buff,
					(void *)argp, MAX_BUFF_SIZE)) {
			/*ut_pm_mutex_unlock(&pm_mutex);*/
			return -EINVAL;
		}
		if ((void *)cancel_message_buff != NULL)
			return -EINVAL;

		send_cancel_command(0);

		/*ut_pm_mutex_unlock(&pm_mutex);*/

		IMSG_DEBUG("[%s][%d] TEEI_CANCEL_COMMAND end.\n",
						__func__, __LINE__);
		return 0;
	}

	down(&api_lock);
	ut_pm_mutex_lock(&pm_mutex);
	switch (cmd) {

	case TEEI_CLIENT_IOCTL_INITCONTEXT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_context_init(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed init context %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_CLOSECONTEXT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_context_close(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed close context: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_INIT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_init(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session init: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_OPEN_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_open(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session open: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN end.\n",
						__func__, __LINE__);
#endif
		break;


	case TEEI_CLIENT_IOCTL_SES_CLOSE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE beginning.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_close(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session close: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_OPERATION_RELEASE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_OPERATION_RELEASE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_operation_release(
						file->private_data, argp);

		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed operation release: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_OPERATION_RELEASE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SEND_CMD_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_send_cmd(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed send cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_GET_DECODE_TYPE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_get_decode_type(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed decode cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_UINT32:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_encode_uint32(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_UINT32:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_decode_uint32(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to decode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_encode_array(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_DEC_ARRAY_SPACE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_decode_array_space(
						file->private_data, argp);

		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to decode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_DEC_ARRAY_SPACE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_MEM_REF:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_ENC_ARRAY_SPACE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_ENC_ARRAY_SPACE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_SHR_MEM_ALLOCATE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_alloc(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to shared_mem_alloc: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_SHR_MEM_ALLOCATE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_FREE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_free(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to shared_mem_free: %x.\n",
					__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE end.\n",
					__func__, __LINE__);
#endif
		break;

	case TEEI_GET_TEEI_CONFIG_STAT:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT begin.\n",
					__func__, __LINE__);
#endif
		retVal = teei_config_flag;
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT end.\n",
					__func__, __LINE__);
#endif
		break;

	default:
		IMSG_ERROR("[%s][%d] command not found! 0x%x\n",
						__func__, __LINE__, cmd);
		retVal = -EINVAL;
	}
	ut_pm_mutex_unlock(&pm_mutex);
	up(&api_lock);
	return retVal;
}

static long teei_client_unioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	int retVal = 0;
	void *argp = (void __user *) arg;

	if (teei_config_flag == 0) {
		IMSG_ERROR("soter is NOT ready, Can not support IOCTL!\n");
		return -ECANCELED;
	}

	if (cmd == TEEI_CANCEL_COMMAND) {
		IMSG_DEBUG("[%s][%d] TEEI_CANCEL_COMMAND begin.\n",
						__func__, __LINE__);

		/*ut_pm_mutex_lock(&pm_mutex);*/

		if (copy_from_user((void *)cancel_message_buff,
						(void *)argp, MAX_BUFF_SIZE)) {
			/*ut_pm_mutex_unlock(&pm_mutex);*/
			return -EINVAL;
		}
		if ((void *)cancel_message_buff != NULL)
			return -EINVAL;

		send_cancel_command(0);

		/*ut_pm_mutex_unlock(&pm_mutex);*/

		IMSG_DEBUG("[%s][%d] TEEI_CANCEL_COMMAND end.\n",
						__func__, __LINE__);
		return 0;
	}

	down(&api_lock);
	ut_pm_mutex_lock(&pm_mutex);
	switch (cmd) {

	case TEEI_CLIENT_IOCTL_INITCONTEXT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_context_init(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed init context %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_INITCONTEXT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_CLOSECONTEXT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_context_close(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed close context: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_CLOSECONTEXT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_INIT_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_init(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session init: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_INIT end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SES_OPEN_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_open(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session open: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_OPEN end.\n",
						__func__, __LINE__);
#endif
		break;


	case TEEI_CLIENT_IOCTL_SES_CLOSE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_session_close(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed session close: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SES_CLOSE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_OPERATION_RELEASE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_OPERATION_RELEASE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_operation_release(
						file->private_data, argp);

		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed operation release: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_OPERATION_RELEASE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SEND_CMD_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SEND_CMD begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_send_cmd(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed send cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d]TEEI_CLIENT_IOCTL_SEND_CMD end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_GET_DECODE_TYPE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_get_decode_type(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed decode cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_GET_DECODE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_UINT32:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_encode_uint32_64bit(
						file->private_data, argp);

		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_UINT32 end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_UINT32:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_decode_uint32(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to decode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_UINT32 end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_encode_array_64bit(
						file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_ENC_ARRAY end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_DEC_ARRAY_SPACE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_decode_array_space(
						file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to decode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_DEC_ARRAY_SPACE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_MEM_REF:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref_64bit(
						file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_DEC_MEM_REF end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_ENC_ARRAY_SPACE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_encode_mem_ref_64bit(
						file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed to encode_cmd: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_ENC_ARRAY_SPACE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_SHR_MEM_ALLOCATE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_alloc(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed shared_mem_alloc: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] IOCTL_SHR_MEM_ALLOCATE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_CLIENT_IOCTL_SHR_MEM_FREE_REQ:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_client_shared_mem_free(file->private_data, argp);
		if (retVal != 0)
			IMSG_ERROR("[%s][%d] failed shared_mem_free: %x.\n",
						__func__, __LINE__, retVal);
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_CLIENT_IOCTL_SHR_MEM_FREE end.\n",
						__func__, __LINE__);
#endif
		break;

	case TEEI_GET_TEEI_CONFIG_STAT:

#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT begin.\n",
						__func__, __LINE__);
#endif
		retVal = teei_config_flag;
#ifdef UT_DEBUG
		IMSG_DEBUG("[%s][%d] TEEI_GET_TEEI_CONFIG_STAT end.\n",
						__func__, __LINE__);
#endif
		break;

	default:
		IMSG_ERROR("[%s][%d] command not found! 0x%x\n",
						__func__, __LINE__, cmd);
		retVal = -EINVAL;
	}
	ut_pm_mutex_unlock(&pm_mutex);
	up(&api_lock);
	return retVal;
}


/**
 * @brief		The open operation of /dev/teei_client device node.
 *
 * @param inode
 * @param file
 *
 * @return		ENOMEM: no enough memory in the linux kernel
 *			0: on success
 */

static int teei_client_open(struct inode *inode, struct file *file)
{
	long dev_cnt = 0;

	dev_cnt = __teei_client_open_dev();
	if (dev_cnt < 0)
		return -ENOMEM;

	file->private_data = (void *)dev_cnt;

	return 0;
}

void show_utdriver_lock_status(void)
{
	int retVal = 0;

	IMSG_PRINTK("[%s][%d] how_utdriver_lock_status begin.\n",
							__func__, __LINE__);

	retVal = down_trylock(&api_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] api_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] api_lock is up\n",
							__func__, __LINE__);
		up(&api_lock);
	}


	retVal = down_trylock(&fp_api_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] fp_api_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] fp_api_lock is up\n",
							__func__, __LINE__);
		up(&fp_api_lock);
	}

	retVal = down_trylock(&keymaster_api_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] keymaster_api_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] keymaster_api_lock is up\n",
							__func__, __LINE__);
		up(&keymaster_api_lock);
	}

	retVal = down_trylock(&fdrv_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] fdrv_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] fdrv_lock is up\n",
							__func__, __LINE__);
		up(&fdrv_lock);
	}

	retVal = down_trylock(&smc_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] smc_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] smc_lock is up\n",
							__func__, __LINE__);
		up(&smc_lock);
	}

	retVal = mutex_trylock(&pm_mutex);
	if (retVal == 0)
		IMSG_PRINTK("[%s][%d] pm_mutex is locked\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] pm_mutex is unlocked\n",
							__func__, __LINE__);
		mutex_unlock(&pm_mutex);
	}

	IMSG_PRINTK("[%s][%d] how_utdriver_lock_status end.\n",
							__func__, __LINE__);
	return;

}


static ssize_t teei_client_dump(struct file *filp,
				char __user *buf, size_t size, loff_t *ppos)
{
	IMSG_PRINTK("[%s][%d] teei_client_dump begin.....\n",
							__func__, __LINE__);

	show_utdriver_lock_status();

	add_work_entry(NT_DUMP_T, 0);

	IMSG_PRINTK("[%s][%d] teei_client_dump finished.....\n",
							__func__, __LINE__);

	return 0;
}




/**
 * @brief	Map the vma with the free pages
 *
 * @param filp
 * @param vma
 *
 * @return	0: success
 *		EINVAL: Invalid parament
 *		ENOMEM: No enough memory
 */
static int teei_client_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int retVal = 0;
	void *alloc_addr = NULL;
	long length = vma->vm_end - vma->vm_start;
	int dev_file_id = (unsigned long)filp->private_data;

	alloc_addr =  __teei_client_map_mem(dev_file_id, length, vma->vm_start);
	if (alloc_addr == NULL) {
		IMSG_ERROR("[%s][%d] get free pages failed!\n",
							__func__, __LINE__);
		return -ENOMEM;
	}

	vma->vm_flags = vma->vm_flags | VM_IO;

	/* Remap the free pages to the VMA */
	retVal = remap_pfn_range(vma, vma->vm_start,
			((virt_to_phys((void *)alloc_addr)) >> PAGE_SHIFT),
			length, vma->vm_page_prot);

	if (retVal) {
		IMSG_ERROR("[%s][%d] remap_pfn_range failed!\n",
							__func__, __LINE__);
		return retVal;
	}

	return 0;
}
/**
 * @brief		The release operation of /dev/teei_client device node.
 *
 * @param		inode: device inode structure
 * @param		file:  struct file
 *
 * @return		0: on success
 */
static int teei_client_release(struct inode *inode, struct file *file)
{
	int retVal = 0;

	retVal = teei_client_service_exit(file->private_data);

	return retVal;
}

/**
 * @brief
 */
static const struct file_operations teei_client_fops = {
	.owner = THIS_MODULE,
#ifdef CONFIG_ARM64
	.unlocked_ioctl = teei_client_unioctl,
#else
	.unlocked_ioctl = teei_client_ioctl,
#endif
	.compat_ioctl = teei_client_ioctl,
	.open = teei_client_open,
	.mmap = teei_client_mmap,
	.read = teei_client_dump,
	.release = teei_client_release
};

static int teei_probe(struct platform_device *pdev)
{
	int ut_irq = 0;
	int soter_irq = 0;

#ifdef CONFIG_OF
	ut_irq = platform_get_irq(pdev, 0);
	IMSG_INFO("teei device ut_irq is %d\n", ut_irq);
	soter_irq = platform_get_irq(pdev, 1);
	IMSG_INFO("teei device soter_irq is %d\n", soter_irq);

	if (ut_irq <= 0 || soter_irq <= 0) {
		IMSG_ERROR("teei_device can't get correct irqs\n");
		return -1;
	}
#else
	ut_irq = UT_DRV_IRQ;
	soter_irq = SOTER_IRQ;
#endif

	if (init_sysfs(pdev) < 0) {
		IMSG_ERROR("failed to init tz_driver sysfs\n");
		return -1;
	}

	if (register_ut_irq_handler(ut_irq) < 0) {
		IMSG_ERROR("teei_device can't register irq %d\n", ut_irq);
		return -1;
	}
	if (register_soter_irq_handler(soter_irq) < 0) {
		IMSG_ERROR("teei_device can't register irq %d\n", soter_irq);
		return -1;
	}

	IMSG_INFO("teei device irqs are registered successfully\n");

	return 0;
}

static int teei_remove(struct platform_device *pdev)
{
	remove_sysfs(pdev);
	return 0;
}

static const struct of_device_id teei_of_ids[] = {
	{ .compatible = "microtrust,utos", },
	{}
};

static struct platform_driver teei_driver = {
	.probe = teei_probe,
	.remove = teei_remove,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "utos",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = teei_of_ids,
#endif
	},
};

/**
 * @brief TEEI Agent Driver initialization
 * initialize service framework
 * @return
 */
static int teei_client_init(void)
{
	int ret_code = 0;
	struct device *class_dev = NULL;
	int i;

#ifdef TUI_SUPPORT
	int pwr_pid = 0;
#endif

	/* IMSG_DEBUG("TEEI Agent Driver Module Init ...\n"); */

	IMSG_DEBUG("=====================================================\n\n");
	IMSG_DEBUG("~~~~~~~uTos version [%s]~~~~~~~\n", UTOS_VERSION);
	IMSG_DEBUG("=====================================================\n\n");

	ret_code = alloc_chrdev_region(&teei_client_device_no,
						0, 1, TEEI_CLIENT_DEV);

	if (ret_code < 0) {
		IMSG_ERROR("alloc_chrdev_region failed %x\n", ret_code);
		return ret_code;
	}

	ret_code = platform_driver_register(&teei_driver);
	if (ret_code) {
		IMSG_ERROR("unable to register teei driver(%d)\n", ret_code);
		return ret_code;
	}

	tz_drv_state = kzalloc(sizeof(struct tz_driver_state), GFP_KERNEL);
	if (!tz_drv_state)
		return -ENOMEM;

	mutex_init(&tz_drv_state->smc_lock);
	ATOMIC_INIT_NOTIFIER_HEAD(&tz_drv_state->notifier);

	tz_drv_state->tz_log_pdev = platform_device_alloc("tz_log", 0);
	if (!tz_drv_state->tz_log_pdev)
		goto failed_alloc_dev;

	platform_device_add(tz_drv_state->tz_log_pdev);

	ret_code = tz_log_probe(tz_drv_state->tz_log_pdev);
	if (ret_code) {
		IMSG_ERROR("failed to initial tz_log driver (%d)\n", ret_code);
		goto del_pdev;
	}

	driver_class = class_create(THIS_MODULE, TEEI_CLIENT_DEV);
	if (IS_ERR(driver_class)) {
		ret_code = -ENOMEM;
		IMSG_ERROR("class_create failed %x\n", ret_code);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL,
				teei_client_device_no, NULL, TEEI_CLIENT_DEV);

	if (class_dev == NULL) {
		IMSG_ERROR("class_device_create failed %x\n", ret_code);
		ret_code = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&teei_client_cdev, &teei_client_fops);
	teei_client_cdev.owner = THIS_MODULE;

	ret_code = cdev_add(&teei_client_cdev,
				MKDEV(MAJOR(teei_client_device_no), 0), 1);

	if (ret_code < 0) {
		IMSG_ERROR("cdev_add failed %x\n", ret_code);
		goto class_device_destroy;
	}

	memset(&teei_contexts_head, 0, sizeof(teei_contexts_head));

	teei_contexts_head.dev_file_cnt = 0;
	init_rwsem(&teei_contexts_head.teei_contexts_sem);

	INIT_LIST_HEAD(&teei_contexts_head.context_list);

	TZ_SEMA_INIT_1(&(smc_lock));

	for_each_online_cpu(i) {
		current_cpu_id = i;
		IMSG_DEBUG("init stage : current_cpu_id = %d\n",
							current_cpu_id);
		/* break when first active cpu has been selected */
		break;
	}

	if (read_cpuid_mpidr() & MPIDR_MT_BITMASK)
		teei_cpu_id = teei_cpu_id_arm82;
	else
		teei_cpu_id = teei_cpu_id_arm80;

	IMSG_DEBUG("begin to create sub_thread.\n");

	/* create the switch thread */
	teei_switch_task = kthread_create(kthread_worker_fn,
				&ut_fastcall_worker, "teei_switch_thread");

	if (IS_ERR(teei_switch_task)) {
		IMSG_ERROR("create switch thread failed: %ld\n",
						PTR_ERR(teei_switch_task));
		teei_switch_task = NULL;
		goto class_device_destroy;
	}

	wake_up_process(teei_switch_task);
	cpumask_set_cpu(get_current_cpuid(), &mask);
	set_cpus_allowed_ptr(teei_switch_task, &mask);

	IMSG_DEBUG("create the sub_thread successfully!\n");

#if defined(CONFIG_ARCH_MT6580) || defined(CONFIG_ARCH_MT6570)
	/* Core migration not supported */
#else
	register_cpu_notifier(&tz_driver_cpu_notifer);

	IMSG_DEBUG("after  register cpu notify\n");
#endif

#ifdef TUI_SUPPORT
	pwr_pid = kthread_run(wait_for_power_down, 0, POWER_DOWN);
	if (IS_ERR(pwr_pid)) {
		pwr_pid = PTR_ERR(pwr_pid);
		IMSG_ERROR("failed to create kernel thread: %d\n", pwr_pid);
	}
	register_reboot_notifier(&tui_notifier);
#endif

	teei_config_init();

	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, teei_client_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(teei_client_device_no, 1);
	tz_log_remove(tz_drv_state->tz_log_pdev);
del_pdev:
	platform_device_del(tz_drv_state->tz_log_pdev);
failed_alloc_dev:
	platform_device_put(tz_drv_state->tz_log_pdev);
	mutex_destroy(&tz_drv_state->smc_lock);
	kfree(tz_drv_state);
return_fn:
	return ret_code;
}

/**
 * @brief
 */
static void teei_client_exit(void)
{
	IMSG_INFO("teei_client exit");
#ifdef TUI_SUPPORT
	unregister_reboot_notifier(&tui_notifier);
#endif
	device_destroy(driver_class, teei_client_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(teei_client_device_no, 1);
	platform_driver_unregister(&teei_driver);
	if (tz_drv_state) {
		tz_log_remove(tz_drv_state->tz_log_pdev);
		platform_device_del(tz_drv_state->tz_log_pdev);
		platform_device_put(tz_drv_state->tz_log_pdev);
		mutex_destroy(&tz_drv_state->smc_lock);
		kfree(tz_drv_state);
	}
}


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("TEEI <www.microtrust.com>");
MODULE_DESCRIPTION("TEEI Agent");
MODULE_VERSION("1.00");

module_init(teei_client_init);

module_exit(teei_client_exit);


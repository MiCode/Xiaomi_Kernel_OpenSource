/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <asm/cputype.h>
#include <linux/cpu.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#define TEEI_SWITCH_BIG_CORE

#ifdef TEEI_FIND_PREFER_CORE_AUTO
#include <kernel/sched/sched.h>
#endif

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
#include <uapi/linux/sched/types.h>
#endif

#ifdef CONFIG_MTPROF
#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
#include <linux/bootprof.h>
#else
#include "bootprof.h"
#endif /* KERNEL_VERSION */
#endif /* CONFIG_MTPROF */

#include <teei_client_main.h>
#include <teei_id.h>
#include <switch_queue.h>
#include <teei_task_link.h>
#include <teei_secure_api.h>
#include <utdriver_macro.h>
#include <notify_queue.h>
#include <nt_smc_call.h>
#include <fdrv.h>
#include <backward_driver.h>
#include <teei_fp.h>
#include <tz_log.h>
#include <utos_version.h>
#include <sysfs.h>
#include <teei_keymaster.h>
#include <irq_register.h>
#include <../teei_fp/fp_func.h>

#if (CONFIG_MICROTRUST_TZ_DRIVER_MTK_BOOTPROF && CONFIG_MTPROF)

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
#define TEEI_BOOT_FOOTPRINT(str) bootprof_log_boot(str)
#else
#define TEEI_BOOT_FOOTPRINT(str) log_boot(str)
#endif

#else
#define TEEI_BOOT_FOOTPRINT(str) IMSG_PRINTK("%s\n", str)
#endif

#define DECLARE_SEMA(name, init_value) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, init_value)

#define DECLARE_RW_SEMA(name) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

DECLARE_RW_SEMA(teei_cpus_lock);
DECLARE_SEMA(boot_sema, 0);
DECLARE_SEMA(pm_sema, 0);

DECLARE_COMPLETION(boot_decryto_lock);

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
#define TZ_PREFER_BIND_CORE (7)
#endif

#define TEEI_RT_POLICY (0x01)
#define TEEI_NORMAL_POLICY (0x02)

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

static int current_cpu_id;

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
#if KERNEL_VERSION(4, 14, 0) >= LINUX_VERSION_CODE
static int tz_driver_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu);
static struct notifier_block tz_driver_cpu_notifer = {
	.notifier_call = tz_driver_cpu_callback,
};
#endif
#endif

unsigned long teei_config_flag;
unsigned int soter_error_flag;
unsigned long boot_vfs_addr;
unsigned long boot_soter_flag;
unsigned long device_file_cnt;

struct list_head g_block_link;

/* For keymaster */
unsigned long teei_capi_ready;



static unsigned int teei_flags;
static dev_t teei_config_device_no;
static struct cdev teei_config_cdev;
static struct class *config_driver_class;

struct timeval stime;
struct timeval etime;
struct task_struct *teei_switch_task;
struct task_struct *teei_bdrv_task;
struct task_struct *teei_log_task;
static struct cpumask mask = { CPU_BITS_NONE };
static struct class *driver_class;
static dev_t teei_client_device_no;
static struct cdev teei_client_cdev;

DEFINE_KTHREAD_WORKER(ut_fastcall_worker);


static struct tz_driver_state *tz_drv_state;
static void *teei_cpu_write_owner;

int teei_set_switch_pri(unsigned long policy)
{
	struct sched_param param = {.sched_priority = 50 };
	int retVal = 0;

	if (policy == TEEI_RT_POLICY) {
		if (teei_switch_task != NULL) {
			sched_setscheduler_nocheck(teei_switch_task,
						SCHED_FIFO, &param);
			return 0;
		} else
			return -EINVAL;
	} else if (policy == TEEI_NORMAL_POLICY) {
		if (teei_switch_task != NULL) {
			param.sched_priority = 0;
			sched_setscheduler_nocheck(teei_switch_task,
						SCHED_NORMAL, &param);
			return 0;
		} else
			return -EINVAL;
	} else {
		IMSG_PRINTK("TEEI: %s invalid Param (%lx)\n",
						__func__, policy);
		retVal = -EINVAL;
	}

	return retVal;
}

void teei_cpus_read_lock(void)
{
	if (current != teei_cpu_write_owner)
		cpus_read_lock();
}

void teei_cpus_read_unlock(void)
{
	if (current != teei_cpu_write_owner)
		cpus_read_unlock();
}

void teei_cpus_write_lock(void)
{
	cpus_write_lock();
	teei_cpu_write_owner = current;
}

void teei_cpus_write_unlock(void)
{
	teei_cpu_write_owner = NULL;
	cpus_write_unlock();
}

struct tz_driver_state *get_tz_drv_state(void)
{
	return tz_drv_state;
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

int teei_move_cpu_context(int target_cpu_id, int original_cpu_id)
{
	teei_secure_call(N_SWITCH_CORE, teei_cpu_id[target_cpu_id],
			teei_cpu_id[original_cpu_id], 0);

	return 0;
}

void set_current_cpuid(int cpu)
{
	current_cpu_id = cpu;
}

int get_current_cpuid(void)
{
	return current_cpu_id;
}

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE

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

	IMSG_PRINTK("[%s][%d]before cpumask set cpu, find %d\n",
				__func__, __LINE__, switch_to_cpu_id);

	set_cpus_allowed_ptr(teei_switch_task, cpumask_of(switch_to_cpu_id));

	teei_secure_call(N_SWITCH_CORE,
			teei_cpu_id[switch_to_cpu_id], teei_cpu_id[cpu], 0);

	current_cpu_id = switch_to_cpu_id;

	IMSG_PRINTK("change cpu id from %d(0x%lx) to %d(0x%lx)\n",
			cpu, teei_cpu_id[cpu],
			switch_to_cpu_id, teei_cpu_id[switch_to_cpu_id]);

	up(&pm_sema);

	return 0;
}

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
static int nq_cpu_up_prep(unsigned int cpu)
{
#ifdef TEEI_SWITCH_BIG_CORE
	int retVal = 0;
	unsigned int sched_cpu = get_current_cpuid();

	IMSG_DEBUG("current_cpu_id = %d power on %d\n",
				sched_cpu, cpu);

	if (cpu == TZ_PREFER_BIND_CORE) {
		IMSG_DEBUG("cpu up: prepare for changing %d to %d\n",
			sched_cpu, cpu);

		retVal = add_work_entry(SWITCH_CORE_TYPE,
				(unsigned long)sched_cpu, 0, 0, 0);

		teei_notify_switch_fn();

		down(&pm_sema);
	}
	return retVal;
#else
	return 0;
#endif
}


static int nq_cpu_down_prep(unsigned int cpu)
{
	int retVal = 0;
	unsigned int sched_cpu = get_current_cpuid();

	if (cpu == sched_cpu) {
		IMSG_PRINTK("cpu down prepare for %d.\n", cpu);
		retVal = add_work_entry(SWITCH_CORE_TYPE, (unsigned long)cpu,
						0, 0, 0);
		teei_notify_switch_fn();
		down(&pm_sema);
	} else if (is_prefer_core(cpu))
		IMSG_DEBUG("cpu down prepare for prefer %d.\n", cpu);
	else if (!is_prefer_core_binded()
			&& is_prefer_core_onlined()) {
		IMSG_PRINTK("cpu down prepare for changing %d %d.\n",
							sched_cpu, cpu);
		retVal = add_work_entry(SWITCH_CORE_TYPE,
				(unsigned long)sched_cpu, 0, 0, 0);
		teei_notify_switch_fn();
		down(&pm_sema);
	}
	return retVal;
}

#elif KERNEL_VERSION(3, 18, 0) <= LINUX_VERSION_CODE

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
			add_work_entry(SWITCH_CORE_TYPE, (unsigned long)cpu,
						0, 0, 0);
			teei_notify_switch_fn();
			down(&pm_sema);
		} else if (is_prefer_core(cpu))
			IMSG_DEBUG("cpu down prepare for prefer %d.\n", cpu);
		else if (!is_prefer_core_binded()
				&& is_prefer_core_onlined()) {
			IMSG_DEBUG("cpu down prepare for changing %d %d.\n",
								sched_cpu, cpu);
			add_work_entry(SWITCH_CORE_TYPE,
					(unsigned long)sched_cpu, 0, 0, 0);
			teei_notify_switch_fn();
			down(&pm_sema);
		}
		break;

#ifdef TEEI_SWITCH_BIG_CORE
	case CPU_ONLINE:
		if (cpu == TZ_PREFER_BIND_CORE) {
			IMSG_DEBUG("cpu up: prepare for changing %d to %d.\n",
					sched_cpu, cpu);
			add_work_entry(SWITCH_CORE_TYPE,
					(unsigned long)sched_cpu, 0, 0, 0);
			teei_notify_switch_fn();
			down(&pm_sema);
		}

		break;
#endif

	default:
		break;
	}
	return NOTIFY_OK;
}

#endif
#endif /* CONFIG_MICROTRUST_DYNAMIC_CORE */


int t_os_load_image(void)
{
	int retVal = 0;

	retVal = add_work_entry(SMC_CALL_TYPE, N_INVOKE_T_NQ, 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the add_work_entry!\n",
				__func__, __LINE__);
	}

	retVal = add_nq_entry(TEEI_LOAD_TEE, 0,
				(unsigned long long)(&boot_sema),
				 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] Failed to call the add_nq_entry!\n",
				__func__, __LINE__);
	}

	teei_notify_switch_fn();

	down(&boot_sema);

	return retVal;
}

static void boot_stage1(unsigned long vfs_addr, unsigned long tlog_addr)
{
	int retVal = 0;

	switch_input_index = ((unsigned long)switch_input_index  + 1) % 10000;

	retVal = add_work_entry(SMC_CALL_TYPE, N_INIT_T_BOOT_STAGE1,
					vfs_addr, tlog_addr, 0);
	if (retVal != 0) {
		IMSG_ERROR("[%s][%d] TEEI: Failed to call add_work_entry!\n",
				__func__, __LINE__);
		return;
	}

	teei_notify_switch_fn();

	down(&(boot_sema));
}

long teei_create_drv_shm(void)
{
	long retVal = 0;

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

	if (soter_error_flag == 1)
		return -1;

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

struct notifier_block ut_smc_nb;

static int init_teei_framework(void)
{
	long retVal = 0;
	struct tz_log_state *s = dev_get_platdata(
				&tz_drv_state->tz_log_pdev->dev);

	phys_addr_t tz_log_buf_pa = page_to_phys(s->log_pages);

	boot_soter_flag = START_STATUS;

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

	teei_cpus_read_lock();

	boot_stage1((unsigned long)virt_to_phys((void *)boot_vfs_addr),
						(unsigned long)tz_log_buf_pa);

	teei_cpus_read_unlock();

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Stage1 Completed");

	free_pages(boot_vfs_addr, get_order(ROUND_UP(VFS_SIZE, SZ_4K)));

	boot_soter_flag = END_STATUS;
	if (soter_error_flag == 1)
		return TEEI_BOOT_ERROR_LOAD_SOTER_FAILED;

	teei_cpus_read_lock();

	retVal = create_nq_buffer();

	teei_cpus_read_unlock();

	if (retVal < 0)
		return TEEI_BOOT_ERROR_INIT_CMD_BUFF_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI BOOT CREATE NQ DONE");

	teei_cpus_read_lock();

	retVal = teei_create_drv_shm();

	teei_cpus_read_unlock();

	if (retVal == -1)
		return TEEI_BOOT_ERROR_INIT_SERVICE1_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI BOOT CREATE DRV SHM DONE");

	retVal = teei_new_capi_init();

	if (retVal < 0)
		return TEEI_BOOT_ERROR_INIT_CAPI_FAILED;

	TEEI_BOOT_FOOTPRINT("TEEI NEW CAPI Inited");

	/* waiting for keymaster shm ready and anable the keymaster IOCTL */
	teei_capi_ready = 1;
	up(&keymaster_api_lock);

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Keymaster Unlocked");

	/* android notify the uTdriver that the TAs is ready !*/
	wait_for_completion(&boot_decryto_lock);
	TEEI_BOOT_FOOTPRINT("TEEI BOOT Decrypt Unlocked");

	teei_cpus_read_lock();

	retVal = teei_service_init_second();

	teei_cpus_read_unlock();

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Service2 Inited");
	if (retVal == -1)
		return TEEI_BOOT_ERROR_INIT_SERVICE2_FAILED;

	teei_cpus_read_lock();

	t_os_load_image();

	teei_cpus_read_unlock();

	TEEI_BOOT_FOOTPRINT("TEEI BOOT Load TEES Completed");
	if (soter_error_flag == 1)
		return TEEI_BOOT_ERROR_LOAD_TA_FAILED;

	teei_config_flag = 1;

#ifdef CONFIG_MICROTRUST_FP_DRIVER
	wake_up(&__fp_open_wq);
#endif
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

#ifdef TEEI_FIND_PREFER_CORE_AUTO
int teei_get_max_freq(int cpu_index)
{
#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	return arch_max_cpu_freq(NULL, cpu_index);
#elif KERNEL_VERSION(4, 4, 0) <= LINUX_VERSION_CODE
	return arch_scale_get_max_freq(cpu_index);
#endif
	return 0;
}
#endif

static long teei_config_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int retVal = 0;
	struct init_param param;
	unsigned int teei_ta_flags;

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

			TEEI_BOOT_FOOTPRINT("TEEI start to load driver TAs");

			teei_ta_flags = param.flag;
			for (i = 0; i < param.uuid_count; i++) {
				if ((teei_ta_flags >> i) & (0x01))
					tz_load_ta_by_str(param.uuids[i]);
				else
					tz_load_drv_by_str(param.uuids[i]);
			}

			param.flag = teei_flags;

			TEEI_BOOT_FOOTPRINT("TEEI end of load driver TAs");

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

static ssize_t teei_config_read(struct file *filp,
			char __user *buf, size_t size, loff_t *ppos)
{
	char *file_context = NULL;
	int retVal = 0;

	file_context = kmalloc(size, GFP_KERNEL);
	if (file_context == NULL)
		return -ENOMEM;

	retVal = tz_driver_read_logs(file_context, (unsigned long)size);
	if (retVal < 0) {
		IMSG_ERROR("Failed to call the %s! retVal = %d\n",
							__func__, retVal);
		kfree(file_context);
		return (ssize_t)retVal;
	}

	if (copy_to_user(buf, file_context, retVal)) {
		IMSG_ERROR("copy to user failed.\n");
		kfree(file_context);
		return -EFAULT;
	}

	kfree(file_context);
	return (ssize_t)retVal;
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
	.read = teei_config_read,
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
	return 0;
}

void show_utdriver_lock_status(void)
{
	int retVal = 0;

	IMSG_PRINTK("[%s][%d] how_utdriver_lock_status begin.\n",
							__func__, __LINE__);
#ifdef CONFIG_MICROTRUST_FP_DRIVER
	retVal = down_trylock(&fp_api_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] fp_api_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] fp_api_lock is up\n",
							__func__, __LINE__);
		up(&fp_api_lock);
	}
#endif

	retVal = down_trylock(&keymaster_api_lock);
	if (retVal == 1)
		IMSG_PRINTK("[%s][%d] keymaster_api_lock is down\n",
							__func__, __LINE__);
	else {
		IMSG_PRINTK("[%s][%d] keymaster_api_lock is up\n",
							__func__, __LINE__);
		up(&keymaster_api_lock);
	}


	IMSG_PRINTK("[%s][%d] how_utdriver_lock_status end.\n",
							__func__, __LINE__);
	return;

}


static ssize_t teei_client_dump(struct file *filp,
				char __user *buf, size_t size, loff_t *ppos)
{
	IMSG_PRINTK("[%s][%d] begin.....\n", __func__, __LINE__);

	show_utdriver_lock_status();

	IMSG_PRINTK("[%s][%d] finished.....\n", __func__, __LINE__);

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
	return 0;
}

/**
 * @brief
 */
static const struct file_operations teei_client_fops = {
	.owner = THIS_MODULE,
	.open = teei_client_open,
	.read = teei_client_dump,
	.release = teei_client_release
};

static int teei_probe(struct platform_device *pdev)
{
	int ut_irq = 0;

	ut_irq = platform_get_irq(pdev, 0);
	IMSG_INFO("teei device ut_irq is %d\n", ut_irq);

	if (init_sysfs(pdev) < 0) {
		IMSG_ERROR("failed to init tz_driver sysfs\n");
		return -1;
	}

	if (register_ut_irq_handler(ut_irq) < 0) {
		IMSG_ERROR("teei_device can't register irq %d\n", ut_irq);
		return -1;
	}

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

	struct sched_param param = {.sched_priority = 50 };

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

	init_teei_switch_comp();
	teei_init_task_link();

	if (read_cpuid_mpidr() & MPIDR_MT_BITMASK)
		teei_cpu_id = teei_cpu_id_arm82;
	else
		teei_cpu_id = teei_cpu_id_arm80;

	IMSG_DEBUG("begin to create sub_thread.\n");

	/* create the switch thread */
	teei_switch_task = kthread_create(teei_switch_fn,
					NULL, "teei_switch_thread");

	if (IS_ERR(teei_switch_task)) {
		IMSG_ERROR("create switch thread failed: %ld\n",
						PTR_ERR(teei_switch_task));
		teei_switch_task = NULL;
		goto class_device_destroy;
	}

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE
	teei_cpus_write_lock();
#ifdef TEEI_SWITCH_BIG_CORE
	if (cpu_online(TZ_PREFER_BIND_CORE)) {
		current_cpu_id = TZ_PREFER_BIND_CORE;
		teei_move_cpu_context(current_cpu_id, 0);
	}
#endif
	cpumask_set_cpu(get_current_cpuid(), &mask);
	set_cpus_allowed_ptr(teei_switch_task, &mask);
	teei_cpus_write_unlock();
#endif

	/* sched_setscheduler_nocheck(teei_switch_task, SCHED_FIFO, &param); */
	wake_up_process(teei_switch_task);

#ifndef CONFIG_MICROTRUST_DYNAMIC_CORE

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				"tee/teei:online",
				nq_cpu_up_prep, nq_cpu_down_prep);
#elif KERNEL_VERSION(3, 18, 0) <= LINUX_VERSION_CODE
	register_cpu_notifier(&tz_driver_cpu_notifer);
	IMSG_DEBUG("after  register cpu notify\n");
#endif
#endif /* CONFIG_MICROTRUST_DYNAMIC_CORE */

	init_bdrv_comp_fn();

	/* create the backward handler thread */
	teei_bdrv_task = kthread_create(teei_bdrv_fn,
					NULL, "teei_bdrv_thread");

	if (IS_ERR(teei_bdrv_task)) {
		IMSG_ERROR("create bdrv thread failed: %ld\n",
						PTR_ERR(teei_bdrv_task));
		teei_bdrv_task = NULL;
		goto class_device_destroy;
	}

	param.sched_priority = 51;
	sched_setscheduler_nocheck(teei_bdrv_task, SCHED_FIFO, &param);
	wake_up_process(teei_bdrv_task);

	init_tlog_comp_fn();

	/* create the teei log thread */
	teei_log_task = kthread_create(teei_log_fn, NULL, "teei_log_thread");
	if (IS_ERR(teei_log_task)) {
		IMSG_ERROR("create teei log thread failed: %ld\n",
						PTR_ERR(teei_log_task));
		teei_log_task = NULL;
		goto class_device_destroy;
	}

	wake_up_process(teei_log_task);

	IMSG_DEBUG("create the sub_thread successfully!\n");


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

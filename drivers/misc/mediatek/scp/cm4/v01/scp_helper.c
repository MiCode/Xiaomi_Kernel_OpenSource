// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/init.h>         /* needed by module macros */
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/module.h>       /* needed by all modules */
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>         /* needed by poll */
#include <linux/sched.h>
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <mt-plat/aee.h>
#include "scp_dvfs.h"
#include "scp_err_info.h"
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_scpctl.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include "scp_reservedmem_define.h"
#endif

#if ENABLE_SCP_EMI_PROTECTION
#include <mt_emi_api.h>
#endif

/* scp semaphore timeout count definition */
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* scp ready timeout definition */
#define SCP_READY_TIMEOUT (30 * HZ) /* 30 seconds*/
#define SCP_A_TIMER 0
#define CLK_BANK_LEN		(0x00A8)

/* scp ready status for notify*/
unsigned int scp_ready[SCP_CORE_TOTAL];

/* scp enable status*/
unsigned int scp_enable[SCP_CORE_TOTAL];

/* scp dvfs variable*/
unsigned int scp_expected_freq;
unsigned int scp_current_freq;

/*scp awake variable*/
int scp_awake_counts[SCP_CORE_TOTAL];

#if SCP_RECOVERY_SUPPORT
unsigned int scp_recovery_flag[SCP_CORE_TOTAL];
#define SCP_A_RECOVERY_OK	0x44
/*  scp_reset_status
 *  0: scp not in reset status
 *  1: scp in reset status
 */
atomic_t scp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);

/* shadow it due to sram may not access during sleep */
struct scp_region_info_st scp_region_info_copy;

unsigned int scp_reset_by_cmd;
struct scp_region_info_st *scp_region_info;
struct completion scp_sys_reset_cp;
struct scp_work_struct scp_sys_reset_work;
struct wakeup_source *scp_reset_lock;
phys_addr_t scp_loader_base_virt;
DEFINE_SPINLOCK(scp_reset_spinlock);

/* l1c enable */

void __iomem *scp_l1c_start_virt;
#endif

phys_addr_t scp_mem_base_phys;
void __iomem *scp_mem_base_virt;
phys_addr_t scp_mem_size;
struct scp_regs scpreg;

unsigned char *scp_send_buff[SCP_CORE_TOTAL];
unsigned char *scp_recv_buff[SCP_CORE_TOTAL];

static struct workqueue_struct *scp_workqueue;
#if SCP_RECOVERY_SUPPORT
static struct workqueue_struct *scp_reset_workqueue;
#endif
#if SCP_LOGGER_ENABLE
static struct workqueue_struct *scp_logger_workqueue;
#endif
#if SCP_BOOT_TIME_OUT_MONITOR
struct scp_timer {
	struct timer_list tl;
	int tid;
};
static struct scp_timer scp_ready_timer[SCP_CORE_TOTAL];
#endif
static struct scp_work_struct scp_A_notify_work;
static struct scp_work_struct scp_timeout_work;
static unsigned int scp_timeout_times;

static DEFINE_MUTEX(scp_A_notify_mutex);
static DEFINE_MUTEX(scp_feature_mutex);
static DEFINE_MUTEX(scp_register_sensor_mutex);

char *core_ids[SCP_CORE_TOTAL] = {"SCP A"};

DEFINE_SPINLOCK(scp_awake_spinlock);

/* set flag after driver initial done */
static bool driver_init_done;

/*
 * memory copy to scp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_to_scp(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}
EXPORT_SYMBOL_GPL(memcpy_to_scp);


/*
 * memory copy from scp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_from_scp(void *trg, const void __iomem *src, int size)
{
	int i;
	u32 *t = trg;
	const u32 __iomem *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return  1 :get sema success
 *        -1 :get sema timeout
 */
int get_scp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
		return ret;
	}

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), SCP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), SCP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[SCP] get scp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_err("[SCP] already hold scp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	if (scp_awake_unlock(SCP_A_ID) == -1)
		pr_debug("[SCP] %s: scp_awake_unlock fail\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(get_scp_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_scp_semaphore(int flag)
{
	int read_back;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
		return ret;
	}
	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), SCP_SEMAPHORE);
		read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[SCP] release scp sema. %d failed\n", flag);
	} else {
		pr_err("[SCP] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	if (scp_awake_unlock(SCP_A_ID) == -1)
		pr_debug("[SCP] %s: scp_awake_unlock fail\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(release_scp_semaphore);


static BLOCKING_NOTIFIER_HEAD(scp_A_notifier_list);
/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void scp_A_register_notify(struct notifier_block *nb)
{
	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_chain_register(&scp_A_notifier_list, nb);

	pr_debug("[SCP] register scp A notify callback..\n");

	if (is_scp_ready(SCP_A_ID))
		nb->notifier_call(nb, SCP_EVENT_READY, NULL);
	mutex_unlock(&scp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(scp_A_register_notify);


/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void scp_A_unregister_notify(struct notifier_block *nb)
{
	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_chain_unregister(&scp_A_notifier_list, nb);
	mutex_unlock(&scp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(scp_A_unregister_notify);


void scp_schedule_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_workqueue, &scp_ws->work);
}

#if SCP_RECOVERY_SUPPORT
void scp_schedule_reset_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_reset_workqueue, &scp_ws->work);
}
#endif

#if SCP_LOGGER_ENABLE
void scp_schedule_logger_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_logger_workqueue, &scp_ws->work);
}
#endif

/*
 * callback function for work struct
 * notify apps to start their tasks
 * or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void scp_A_notify_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws =
		container_of(ws, struct scp_work_struct, work);
	unsigned int scp_notify_flag = sws->flags;

	scp_ready[SCP_A_ID] = scp_notify_flag;

	if (scp_notify_flag) {
#if SCP_DVFS_INIT_ENABLE
		/* release pll clock after scp ulposc calibration */
		scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif
#if SCP_RECOVERY_SUPPORT
		scp_recovery_flag[SCP_A_ID] = SCP_A_RECOVERY_OK;
#endif
		writel(0xff, SCP_TO_SPM_REG); /* patch: clear SPM interrupt */
		mutex_lock(&scp_A_notify_mutex);
#if SCP_RECOVERY_SUPPORT
		atomic_set(&scp_reset_status, RESET_STATUS_STOP);
#endif
		pr_debug("[SCP] notify blocking call\n");
		blocking_notifier_call_chain(&scp_A_notifier_list
			, SCP_EVENT_READY, NULL);
		mutex_unlock(&scp_A_notify_mutex);
	}

	if (!scp_ready[SCP_A_ID])
		scp_aed(EXCEP_RESET, SCP_A_ID);

#if SCP_RECOVERY_SUPPORT
	/*clear reset status and unlock wake lock*/
	pr_debug("[SCP] clear scp reset flag and unlock\n");
#ifndef CONFIG_FPGA_EARLY_PORTING
	scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REL, 0);

#endif	// CONFIG_FPGA_EARLY_PORTING
	/* register scp dvfs*/
	msleep(2000);
	__pm_relax(scp_reset_lock);
	scp_register_feature(RTOS_FEATURE_ID);
#endif  // SCP_RECOVERY_SUPPORT
}


/*
 * callback function for work struct
 * notify apps to start their tasks
 * or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void scp_timeout_ws(struct work_struct *ws)
{
#if SCP_RECOVERY_SUPPORT
	if (scp_timeout_times < 10)
		scp_send_reset_wq(RESET_TYPE_AWAKE);
#endif

	scp_timeout_times++;
	pr_notice("[SCP] scp_timeout_times=%x\n", scp_timeout_times);
}


#ifdef SCP_PARAMS_TO_SCP_SUPPORT
/*
 * Function/Space for kernel to pass static/initial parameters to scp's driver
 * @return: 0 for success, positive for info and negtive for error
 *
 * Note: The function should be called before disabling 26M & resetting scp.
 *
 * An example of function instance of sensor_params_to_scp:
 * int sensor_params_to_scp(phys_addr_t addr_vir, size_t size)
 * {
 *     int *params;
 *
 *     params = (int *)addr_vir;
 *     params[0] = 0xaaaa;
 *
 *     return 0;
 * }
 */
static int params_to_scp(void)
{
#ifdef CFG_SENSOR_PARAMS_TO_SCP_SUPPORT
	int ret = 0;
	struct scp_region_info_st *region_info =
		(struct scp_region_info_st *)(SCP_TCM + SCP_REGION_INFO_OFFSET);

	mt_reg_sync_writel(scp_get_reserve_mem_phys(SCP_DRV_PARAMS_MEM_ID),
			&(region_info->ap_params_start));

	ret = sensor_params_to_scp(
		(void *)scp_get_reserve_mem_virt(SCP_DRV_PARAMS_MEM_ID),
		scp_get_reserve_mem_size(SCP_DRV_PARAMS_MEM_ID));

	return ret;
#else
	/* return success, if sensor_params_to_scp is not defined */
	return 0;
#endif
}
#endif

/*
 * mark notify flag to 1 to notify apps to start their tasks
 */
static void scp_A_set_ready(void)
{
	pr_debug("[SCP] %s()\n", __func__);
	scp_timeout_times = 0;
#if SCP_BOOT_TIME_OUT_MONITOR
	del_timer(&(scp_ready_timer[SCP_A_ID].tl));
#endif
#if SCP_RECOVERY_SUPPORT
	atomic_set(&scp_reset_status, RESET_STATUS_STOP);
#endif
	scp_A_notify_work.flags = 1;
	scp_schedule_work(&scp_A_notify_work);
}


/*
 * callback for reset timer
 * mark notify flag to 0 to generate an exception
 * @param data: unuse
 */
#if SCP_BOOT_TIME_OUT_MONITOR
static void scp_wait_ready_timeout(struct timer_list *t)
{
	struct scp_timer *timer = from_timer(timer, t, tl);

	/*id = 0: SCP A, id=1: SCP B*/
	pr_notice("%s(),timer %d timeout\n", __func__, timer->tid);
	scp_timeout_work.flags = 0;
	scp_timeout_work.id = SCP_A_ID;
	scp_schedule_work(&scp_timeout_work);
}
#endif

/*
 * handle notification from scp
 * mark scp is ready for running tasks
 * It is important to call scp_ram_dump_init() in this IPI handler. This
 * timing is necessary to ensure that the region_info has been initialized.
 * @param id:   ipi id
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static void scp_A_ready_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int scp_image_size = *(unsigned int *)data;

	if (!scp_ready[SCP_A_ID])
		scp_A_set_ready();

	/*verify scp image size*/
	if (scp_image_size != SCP_A_TCM_SIZE) {
		pr_err("[SCP]image size ERROR! AP=0x%x,SCP=0x%x\n",
					SCP_A_TCM_SIZE, scp_image_size);
		WARN_ON(1);
	}

	pr_debug("[SCP] ramdump init\n");
	scp_ram_dump_init();
}

__attribute__((weak)) void report_hub_dmd(uint32_t case_id,
				uint32_t sensor_id, char *context)
{
	pr_notice("[SCP] weak function to do nothing for cid(%d), sid(%d)\n",
			case_id, sensor_id);
}


/*
 * Handle notification from scp.
 * Report error from SCP to other kernel driver.
 * @param id:   ipi id
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static void scp_err_info_handler(int id, void *data, unsigned int len)
{
	struct error_info *info = (struct error_info *)data;

	if (sizeof(*info) != len) {
		pr_notice("[SCP] error: incorrect size %d of error_info\n",
				len);
		WARN_ON(1);
		return;
	}

	/* Ensure the context[] is terminated by the NULL character. */
	info->context[ERR_MAX_CONTEXT_LEN - 1] = '\0';
	pr_notice("[SCP] Error_info: case id: %u\n", info->case_id);
	pr_notice("[SCP] Error_info: sensor id: %u\n", info->sensor_id);
	pr_notice("[SCP] Error_info: context: %s\n", info->context);

	report_hub_dmd(info->case_id, info->sensor_id, info->context);
}


/*
 * @return: 1 if scp is ready for running tasks
 */
unsigned int is_scp_ready(enum scp_core_id id)
{
	if (scp_ready[id])
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(is_scp_ready);

/*
 * reset scp and create a timer waiting for scp notify
 * notify apps to stop their tasks if needed
 * generate error if reset fail
 * NOTE: this function may be blocked
 *       and should not be called in interrupt context
 * @param reset:    bit[0-3]=0 for scp enable, =1 for reboot
 *                  bit[4-7]=0 for All, =1 for scp_A, =2 for scp_B
 * @return:         0 if success
 */
int reset_scp(int reset)
{
	void __iomem *scp_reset_reg = scpreg.cfg;

	if (((reset & 0xf0) != 0x10) && ((reset & 0xf0) != 0x00)) {
		pr_debug("[SCP] %s: skipped!\n", __func__);
		return 0;
	}

	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_call_chain(&scp_A_notifier_list, SCP_EVENT_STOP,
		NULL);
	mutex_unlock(&scp_A_notify_mutex);

#if SCP_DVFS_INIT_ENABLE
	/* request pll clock before turn on scp */
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

	if (reset & 0x0f) { /* do reset */
		/* make sure scp is in idle state */
		int timeout = 50; /* max wait 1s */

		while (timeout--) {
#if SCP_RECOVERY_SUPPORT
			if (readl(SCP_GPR_CM4_A_REBOOT) == 0x34) {
				if (readl(SCP_SLEEP_STATUS_REG) &
					SCP_A_IS_SLEEP) {
					writel(0, scp_reset_reg);  /* reset */
					scp_ready[SCP_A_ID] = 0;
					writel(1, SCP_GPR_CM4_A_REBOOT);
					/* lock pll for ulposc calibration */
					/* do it only in reset */
					dsb(SY);
					break;
				}
			}
#else
			if (readl(SCP_SLEEP_STATUS_REG) & SCP_A_IS_SLEEP) {
				writel(0, scp_reset_reg);  /* reset */
				scp_ready[SCP_A_ID] = 0;
				dsb(SY);
				break;
			}
#endif  // SCP_RECOVERY_SUPPORT
			mdelay(20);
		}
		pr_debug("[SCP] %s: timeout = %d\n", __func__, timeout);
	}

	if (scp_enable[SCP_A_ID]) {
		writel(1, scp_reset_reg);  /* release reset */
		dsb(SY);

#if SCP_BOOT_TIME_OUT_MONITOR
		scp_ready_timer[SCP_A_ID].tl.expires =
			jiffies + SCP_READY_TIMEOUT;
		add_timer(&(scp_ready_timer[SCP_A_ID].tl));
#endif
	}

	pr_debug("[SCP] %s: done, %p, 0x%x\n",
		__func__,
		scp_reset_reg,
		readl(scp_reset_reg));

	return 0;
}


/*
 * TODO: what should we do when hibernation ?
 */
static int scp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	int retval;

		switch (pm_event) {
		case PM_POST_HIBERNATION:
			pr_debug("[SCP] %s: reboot\n", __func__);
			retval = reset_scp(1);
			if (retval < 0) {
				retval = -EINVAL;
				pr_debug("[SCP] %s: reboot fail\n", __func__);
			}
			return NOTIFY_DONE;
		}
	return NOTIFY_OK;
}

static struct notifier_block scp_pm_notifier_block = {
	.notifier_call = scp_pm_event,
	.priority = 0,
};


static inline ssize_t scp_A_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	if (scp_ready[SCP_A_ID])
		return scnprintf(buf, PAGE_SIZE, "SCP A is ready\n");
	else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_status);

static inline ssize_t scp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	scp_A_dump_regs();

	if (scp_ready[SCP_A_ID]) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_DEBUG_PC_REG:0x%x\n"
			, readl(SCP_A_DEBUG_PC_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_DEBUG_LR_REG:0x%x\n"
			, readl(SCP_A_DEBUG_LR_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_DEBUG_PSP_REG:0x%x\n"
			, readl(SCP_A_DEBUG_PSP_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_DEBUG_SP_REG:0x%x\n"
			, readl(SCP_A_DEBUG_SP_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG0:0x%x\n"
			, readl(SCP_A_GENERAL_REG0));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG1:0x%x\n"
			, readl(SCP_A_GENERAL_REG1));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG2:0x%x\n"
			, readl(SCP_A_GENERAL_REG2));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG3:0x%x\n"
			, readl(SCP_A_GENERAL_REG3));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG4:0x%x\n"
			, readl(SCP_A_GENERAL_REG4));
		len += scnprintf(buf + len, PAGE_SIZE - len,
			"[SCP] SCP_A_GENERAL_REG5:0x%x\n"
			, readl(SCP_A_GENERAL_REG5));
		return len;
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_reg_status);

static inline ssize_t scp_A_db_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	scp_aed_reset(EXCEP_RUNTIME, SCP_A_ID);
	if (scp_ready[SCP_A_ID])
		return scnprintf(buf, PAGE_SIZE, "dumping SCP A db\n");
	else
		return scnprintf(buf, PAGE_SIZE,
				"SCP A is not ready, try to dump EE\n");
}

DEVICE_ATTR_RO(scp_A_db_test);

#ifdef CONFIG_MTK_ENG_BUILD
static ssize_t scp_ee_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_ee_enable);
}

static ssize_t scp_ee_enable_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		scp_ee_enable = value;
		pr_debug("[SCP] scp_ee_enable = %d(1:enable, 0:disable)\n"
				, scp_ee_enable);
	}
	return n;
}

DEVICE_ATTR_RW(scp_ee_enable);

static inline ssize_t scp_A_awake_lock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_lock(SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_awake_lock);

static inline ssize_t scp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_unlock(SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_awake_unlock);

static inline ssize_t scp_ipi_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	unsigned int value = 0x5A5A;
	enum scp_ipi_status ret;

	if (scp_ready[SCP_A_ID]) {
		scp_ipi_status_dump();
		ret = scp_ipi_send(IPI_TEST1
			, &value, sizeof(value), 0, SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE
			, "SCP A ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_ipi_test);

#endif

#if SCP_RECOVERY_SUPPORT
void scp_wdt_reset(enum scp_core_id cpu_id)
{
	switch (cpu_id) {
	case SCP_A_ID:
		writel(0x8000000f, SCP_A_WDT_REG);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(scp_wdt_reset);

/*
 * trigger wdt manually
 * debug use
 */
static ssize_t wdt_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || !count)
		return count;
	pr_debug("[SCP] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			scp_wdt_reset(SCP_A_ID);
	}
	return count;
}

DEVICE_ATTR_WO(wdt_reset);

/*
 * trigger scp reset manually
 * debug use
 */
static ssize_t scp_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_debug("[SCP] %s: %8s\n", __func__, buf);
	/* scp reset by cmdm set flag =1 */
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666) {
			scp_reset_by_cmd = 1;
			scp_wdt_reset(SCP_A_ID);
		}
	}

	return count;
}

DEVICE_ATTR_WO(scp_reset);

/*
 * trigger wdt manually
 * debug use
 */

static ssize_t scp_recovery_flag_show(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_recovery_flag[SCP_A_ID]);
}

static ssize_t scp_recovery_flag_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (kstrtoint(buf, 10, &tmp) < 0) {
		pr_debug("scp_recovery_flag error\n");
		return count;
	}
	scp_recovery_flag[SCP_A_ID] = tmp;
	return count;
}

static DEVICE_ATTR_RW(scp_recovery_flag);

#endif


/******************************************************************************
 *****************************************************************************/
static ssize_t log_filter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	enum scp_ipi_status ret;
	uint32_t filter;
	const unsigned int len = sizeof(filter);

	if (sscanf(buf, "0x%08x", &filter) != 1)
		return -EINVAL;

	ret = scp_ipi_send(IPI_SCP_LOG_FILTER, &filter, len, 0, SCP_A_ID);
	switch (ret) {
	case SCP_IPI_DONE:
		pr_notice("[SCP] Set log filter to 0x%08x\n", filter);
		return count;

	case SCP_IPI_BUSY:
		pr_notice("[SCP] IPI busy. Set log filter failed!\n");
		return -EBUSY;

	case SCP_IPI_ERROR:
	default:
		pr_notice("[SCP] IPI error. Set log filter failed!\n");
		return -EIO;
	}
}

DEVICE_ATTR_WO(log_filter);

/******************************************************************************
 *****************************************************************************/
static struct miscdevice scp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "scp",
	.fops = &scp_A_log_file_ops
};


/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
static int create_files(void)
{
	int ret;

	ret = misc_register(&scp_device);
	if (unlikely(ret != 0)) {
		pr_err("[SCP] misc register failed\n");
		return ret;
	}

#if SCP_LOGGER_ENABLE
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_logger_wakeup_AP);
	if (unlikely(ret != 0))
		return ret;

#ifdef CONFIG_MTK_ENG_BUILD
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;
#endif  // CONFIG_MTK_ENG_BUILD

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif  // SCP_LOGGER_ENABLE

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_status);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(scp_device.this_device
					, &bin_attr_scp_dump);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_reg_status);
	if (unlikely(ret != 0))
		return ret;

	/*only support debug db test in engineer build*/
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_db_test);
	if (unlikely(ret != 0))
		return ret;

#ifdef CONFIG_MTK_ENG_BUILD
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_ee_enable);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_awake_lock);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_awake_unlock);
	if (unlikely(ret != 0))
		return ret;

	/* SCP IPI Debug sysfs*/
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_ipi_test);
	if (unlikely(ret != 0))
		return ret;
#endif  // CONFIG_MTK_ENG_BUILD

#if SCP_RECOVERY_SUPPORT
	ret = device_create_file(scp_device.this_device
					, &dev_attr_wdt_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_recovery_flag);
	if (unlikely(ret != 0))
		return ret;
#endif  // SCP_RECOVERY_SUPPORT

	ret = device_create_file(scp_device.this_device, &dev_attr_log_filter);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(scp_device.this_device
					, &dev_attr_scpctl);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID || id < 0) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_phys);

phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID || id < 0) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_virt);

phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID || id < 0) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_size);

#if SCP_RESERVED_MEM && defined(CONFIG_OF)

static int scp_reserve_memory_ioremap(struct platform_device *pdev)
{
#define MEMORY_TBL_ELEM_NUM (2)
	unsigned int num = (unsigned int)(sizeof(scp_reserve_mblock)
			/ sizeof(scp_reserve_mblock[0]));
	enum scp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	const char *mem_key;
	unsigned int scp_mem_num = 0;
	unsigned int i, m_idx, m_size;
	int ret;

	if (num != NUMS_MEM_ID) {
		pr_err("[SCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		BUG_ON(1);
		return -1;
	}

	/* Get reserved memory */
	of_property_read_string(pdev->dev.of_node, "scp_mem_key", &mem_key);
	rmem_node = of_find_compatible_node(NULL, NULL, mem_key);

	if (!rmem_node) {
		pr_info("[SCP] no node for reserved memory\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("[SCP] cannot lookup reserved memory\n");
		return -EINVAL;
	}

	scp_mem_base_phys = (phys_addr_t) rmem->base;
	scp_mem_size = (phys_addr_t) rmem->size;

	pr_notice("[SCP] %s is called, 0x%x, 0x%x",
		__func__,
		(unsigned int)scp_mem_base_phys,
		(unsigned int)scp_mem_size);

	if ((scp_mem_base_phys >= (0x90000000ULL)) ||
			 (scp_mem_base_phys <= 0x0)) {
		/* The scp remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		pr_err("[SCP] Error: Wrong Address (0x%llx)\n",
			(uint64_t)scp_mem_base_phys);
		BUG_ON(1);
		return -1;
	}

	/* Set reserved memory table */
	scp_mem_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"scp_mem_tbl")
				/ MEMORY_TBL_ELEM_NUM;
	if (scp_mem_num <= 0) {
		pr_err("[SCP] scp_mem_tbl not found\n");
		scp_mem_num = 0;
	}

	for (i = 0; i < scp_mem_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"scp_mem_tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			pr_err("Cannot get memory index(%d)\n", i);
			return -1;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"scp_mem_tbl",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		if (ret) {
			pr_err("Cannot get memory size(%d)\n", i);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			pr_notice("[SCP] skip unexpected index, %d\n", m_idx);
			continue;
		}

		scp_reserve_mblock[m_idx].size = m_size;
		pr_err("@@@@ reserved: <%d  %d>\n", m_idx, m_size);
	}

	/* set virtual and physical address for the reserved memory */
	scp_mem_base_virt = ioremap_wc(scp_mem_base_phys,
		scp_mem_size);
	pr_debug("[SCP] rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_mem_base_virt, (uint64_t)scp_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		scp_reserve_mblock[id].start_phys = (u64)(scp_mem_base_phys +
			accumlate_memory_size);
		scp_reserve_mblock[id].start_virt = (u64)(scp_mem_base_virt +
			accumlate_memory_size);
		accumlate_memory_size += scp_reserve_mblock[id].size;
#ifdef DEBUG
		pr_debug("[SCP] [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)scp_reserve_mblock[id].start_phys,
			(uint64_t)scp_reserve_mblock[id].start_virt,
			(uint64_t)scp_reserve_mblock[id].size);
#endif  // DEBUG
	}
	BUG_ON(accumlate_memory_size > scp_mem_size);

#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		uint64_t start_phys = (uint64_t)scp_get_reserve_mem_phys(id);
		uint64_t start_virt = (uint64_t)scp_get_reserve_mem_virt(id);
		uint64_t len = (uint64_t)scp_get_reserve_mem_size(id);

		pr_notice("[SCP][rsrv_mem-%d] phy:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_phys, start_phys + len - 1, len);
		pr_notice("[SCP][rsrv_mem-%d] vir:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_virt, start_virt + len - 1, len);
	}
#endif  // DEBUG
	return 0;
}
#endif

#if ENABLE_SCP_EMI_PROTECTION
void set_scp_mpu(void)
{
	struct emi_region_info_t region_info;

	region_info.region = MPU_REGION_ID_SCP_SMEM;
	region_info.start = scp_mem_base_phys;
	region_info.end =  scp_mem_base_phys + scp_mem_size - 0x1;

	SET_ACCESS_PERMISSION(region_info.apc, UNLOCK,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			NO_PROTECTION, FORBIDDEN, FORBIDDEN, NO_PROTECTION);

	pr_debug("[SCP] MPU protect SCP Share region<%d:%08llx:%08llx> %x, %x\n",
			MPU_REGION_ID_SCP_SMEM,
			(uint64_t)region_info.start,
			(uint64_t)region_info.end,
			region_info.apc[1], region_info.apc[1]);

	emi_mpu_set_protection(&region_info);
}
#endif

void scp_register_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/*prevent from access when scp is down*/
	if (!scp_ready[SCP_A_ID]) {
		pr_debug("[SCP] %s: not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&scp_feature_mutex);

	/*SCP keep awake */
	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
		mutex_unlock(&scp_feature_mutex);
		return;
	}

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 1;
	}
#if SCP_DVFS_INIT_ENABLE
	scp_expected_freq = scp_get_freq();
#endif

	scp_current_freq = readl(CURRENT_FREQ_REG);
	writel(scp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			/* set scp freq. */
#if SCP_DVFS_INIT_ENABLE
			ret = scp_request_freq();
#endif
			if (ret == -1) {
				pr_err("[SCP]%s request_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_err("[SCP]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

	/*SCP release awake */
	if (scp_awake_unlock(SCP_A_ID) == -1)
		pr_debug("[SCP] %s: awake unlock fail\n", __func__);

	mutex_unlock(&scp_feature_mutex);
}
EXPORT_SYMBOL_GPL(scp_register_feature);

void scp_deregister_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID]) {
		pr_debug("[SCP] %s:not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	mutex_lock(&scp_feature_mutex);

	/*SCP keep awake */
	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
		mutex_unlock(&scp_feature_mutex);
		return;
	}

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 0;
	}
#if SCP_DVFS_INIT_ENABLE
	scp_expected_freq = scp_get_freq();
#endif

	scp_current_freq = readl(CURRENT_FREQ_REG);
	writel(scp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			/* set scp freq. */
#if SCP_DVFS_INIT_ENABLE
			ret = scp_request_freq();
#endif
			if (ret == -1) {
				pr_err("[SCP] %s: req_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_err("[SCP]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

	/*SCP release awake */
	if (scp_awake_unlock(SCP_A_ID) == -1)
		pr_debug("[SCP] %s: awake unlock fail\n", __func__);

	mutex_unlock(&scp_feature_mutex);
}
EXPORT_SYMBOL_GPL(scp_deregister_feature);

/*scp sensor type register*/
void scp_register_sensor(enum feature_id id, enum scp_sensor_id sensor_id)
{
	uint32_t i;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID])
		return;

	if (id != SENS_FEATURE_ID) {
		pr_debug("[SCP]register sensor id err");
		return;
	}
	/* because feature_table is a global variable
	 * use mutex lock to protect it from
	 * accessing in the same time
	 */
	mutex_lock(&scp_register_sensor_mutex);
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].feature == sensor_id)
			sensor_type_table[i].enable = 1;
	}

	/* register sensor*/
	scp_register_feature(id);
	mutex_unlock(&scp_register_sensor_mutex);

}
/*scp sensor type deregister*/
void scp_deregister_sensor(enum feature_id id, enum scp_sensor_id sensor_id)
{
	uint32_t i;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID])
		return;

	if (id != SENS_FEATURE_ID) {
		pr_debug("[SCP]deregister sensor id err");
		return;
	}
	/* because feature_table is a global variable
	 * use mutex lock to protect it from
	 * accessing in the same time
	 */
	mutex_lock(&scp_register_sensor_mutex);
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].feature == sensor_id)
			sensor_type_table[i].enable = 0;
	}
	/* deregister sensor*/
	scp_deregister_feature(id);
	mutex_unlock(&scp_register_sensor_mutex);
}

/*
 * apps notification
 */
void scp_extern_notify(enum SCP_NOTIFY_EVENT notify_status)
{
	blocking_notifier_call_chain(&scp_A_notifier_list, notify_status, NULL);
}

/*
 * reset awake counter
 */
void scp_reset_awake_counts(void)
{
	int i;

	/* scp ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++)
		scp_awake_counts[i] = 0;
}

void scp_awake_init(void)
{
	scp_reset_awake_counts();
}

#if SCP_RECOVERY_SUPPORT
/*
 * scp_set_reset_status, set and return scp reset status function
 * return value:
 *   0: scp not in reset status
 *   1: scp in reset status
 */
unsigned int scp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&scp_reset_spinlock, spin_flags);
	if (atomic_read(&scp_reset_status) == RESET_STATUS_START) {
		spin_unlock_irqrestore(&scp_reset_spinlock, spin_flags);
		return 1;
	}
	/* scp not in reset status, set it and return*/
	atomic_set(&scp_reset_status, RESET_STATUS_START);
	spin_unlock_irqrestore(&scp_reset_spinlock, spin_flags);
	return 0;
}
EXPORT_SYMBOL_GPL(scp_set_reset_status);

/******************************************************************************
 *****************************************************************************/
void print_clk_registers(void)
{
	void __iomem *loader_base = (void __iomem *)scp_loader_base_virt;
	void __iomem *cfg = scpreg.cfg;          // 0x105C_0000
	void __iomem *clkctrl = scpreg.clkctrl;  // 0x105C_4000
	unsigned int offset;
	unsigned int value;

	// Print the first few bytes of the loader binary.
	if (loader_base) {
		for (offset = 0; offset < 16; offset += 4) {
			value = (unsigned int)readl(loader_base + offset);
			pr_notice("[SCP] loader[%u]: 0x%08x\n", offset, value);
		}
	}

	// 0x0000 ~ 0x01CC (inclusive)
	for (offset = 0x0000; offset <= 0x01CC; offset += 4) {
		value = (unsigned int)readl(cfg + offset);
		pr_notice("[SCP] cfg[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x4000 ~ 0x40A4 (inclusive)
	for (offset = 0x0000; offset < CLK_BANK_LEN; offset += 4) {
		value = (unsigned int)readl(clkctrl + offset);
		pr_notice("[SCP] clk[0x%04x]: 0x%08x\n", offset, value);
	}
}

/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
void scp_sys_reset_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws = container_of(ws
					, struct scp_work_struct, work);
	unsigned int scp_reset_type = sws->flags;
	void __iomem *scp_reset_reg = scpreg.cfg;
	unsigned long spin_flags;
	/* make sure scp is in idle state */
	int timeout = 50; /* max wait 1s */

	/*notify scp functions stop*/
	pr_debug("[SCP] %s(): scp_extern_notify\n", __func__);
	scp_extern_notify(SCP_EVENT_STOP);
	/*set scp not ready*/
	pr_debug("[SCP] %s(): scp_status_set\n", __func__);

	/*
	 *   scp_ready:
	 *   SCP_PLATFORM_STOP  = 0,
	 *   SCP_PLATFORM_READY = 1,
	 */
	scp_ready[SCP_A_ID] = 0;

	/* wake lock AP*/
	__pm_stay_awake(scp_reset_lock);

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* keep 26Mhz */
	scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REQ,
			SCP_REQ_RESOURCE_26M);
#endif  // CONFIG_FPGA_EARLY_PORTING
	/*request pll clock before turn off scp */
	pr_debug("[SCP] %s(): scp_pll_ctrl_set\n", __func__);
#if SCP_DVFS_INIT_ENABLE
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

	/*workqueue for scp ee, scp reset by cmd will not trigger scp ee*/
	if (scp_reset_by_cmd == 0) {
		pr_debug("[SCP] %s(): scp_aed_reset\n", __func__);
		scp_aed_reset(EXCEP_RUNTIME, SCP_A_ID);

		/*wait scp ee finished*/
		pr_debug("[SCP] %s(): wait ee finished...\n", __func__);
		if (wait_for_completion_interruptible_timeout(&scp_sys_reset_cp
			, jiffies_to_msecs(1000)) == 0)
			pr_debug("[SCP] %s: scp ee time out\n", __func__);
	}

	/*disable scp logger
	 * 0: scp logger disable
	 * 1: scp logger enable
	 */
	pr_debug("[SCP] %s(): disable logger\n", __func__);
	scp_logger_init_set(0);

	print_clk_registers();

	/* scp reset by CMD, WDT or awake fail */
	if (scp_reset_type == RESET_TYPE_WDT) {
		/* reset type scp WDT */
		pr_notice("[SCP] %s(): scp wdt reset\n", __func__);
		/* make sure scp is in idle state */
		while (timeout--) {
			if (readl(SCP_GPR_CM4_A_REBOOT) == 0x34) {
				if (readl(SCP_SLEEP_STATUS_REG)
					& SCP_A_IS_SLEEP) {
					/* SCP stops any activities
					 * and parks at wfi
					 */
					break;
				}
			}
			mdelay(20);
		}

		if (timeout == 0)
			pr_notice("[SCP]wdt reset timeout, still reset scp\n");

		writel(0, scp_reset_reg);
		writel(1, SCP_GPR_CM4_A_REBOOT);
		dsb(SY);
	} else if (scp_reset_type == RESET_TYPE_AWAKE) {
		/* reset type awake fail */
		pr_debug("[SCP] %s(): scp awake fail reset\n", __func__);
		/* stop scp */
		writel(0, scp_reset_reg);
	} else {
		/* reset type cmd */
		pr_debug("[SCP] %s(): scp awake fail reset\n", __func__);
		/* stop scp */
		writel(0, scp_reset_reg);
	}

	/* scp reset */
	scp_sys_full_reset();

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_reset_awake_counts();
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	/* start scp */
	timeout = 5;
	writel(1, scp_reset_reg);
	dsb(SY);

	while ((readl(scp_reset_reg) == 0) && (timeout > 0)) {
		pr_notice("[SCP]reset countdown, %d\n", timeout);
		writel(1, scp_reset_reg);
		mdelay(20);
		timeout--;
	};

	if (readl(scp_reset_reg))
		pr_notice("[SCP]start scp\n");
	else
		pr_notice("[SCP]start scp failed\n");

#if SCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&(scp_ready_timer[SCP_A_ID].tl), jiffies + SCP_READY_TIMEOUT);
#endif
	/* clear scp reset by cmd flag*/
	scp_reset_by_cmd = 0;
}


/*
 * schedule a work to reset scp
 * @param type: exception type
 */
void scp_send_reset_wq(enum SCP_RESET_TYPE type)
{
	scp_sys_reset_work.flags = (unsigned int) type;
	scp_sys_reset_work.id = SCP_A_ID;
	if (scp_ee_enable != 3646633)
		scp_schedule_reset_work(&scp_sys_reset_work);
}
#endif

int scp_check_resource(void)
{
	/* called by lowpower related function
	 * main purpose is to ensure main_pll is not disabled
	 * because scp needs main_pll to run at vcore 1.0 and 354Mhz
	 * return value:
	 * 1: main_pll shall be enabled
	 *    26M shall be enabled, infra shall be enabled
	 * 0: main_pll may disable, 26M may disable, infra may disable
	 */
	int scp_resource_status = 0;
#ifdef CONFIG_MACH_MT6799
	unsigned long spin_flags;

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_current_freq = readl(CURRENT_FREQ_REG);
	scp_expected_freq = readl(EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	if (scp_expected_freq == FREQ_416MHZ || scp_current_freq == FREQ_416MHZ)
		scp_resource_status = 1;
	else
		scp_resource_status = 0;
#endif

	return scp_resource_status;
}

#if SCP_RECOVERY_SUPPORT
void scp_region_info_init(void)
{
	int region_size = SCP_RTOS_START - SCP_REGION_INFO_OFFSET -
		(SHARE_BUF_SIZE * 2);
	int struct_size = sizeof(scp_region_info_copy);

	if (struct_size > region_size) {
		pr_debug("[SCP] Error: Structure exceeds region info!\n");
		WARN_ON(1);
		return;
	}

	/*get scp loader/firmware info from scp sram*/
	scp_region_info = (SCP_TCM + SCP_REGION_INFO_OFFSET);
	pr_debug("[SCP] scp_region_info = %p\n", scp_region_info);
	memcpy_from_scp(&scp_region_info_copy, scp_region_info, struct_size);
}
#else
void scp_region_info_init(void) {}
#endif

void scp_recovery_init(void)
{
#if SCP_RECOVERY_SUPPORT
	/*create wq for scp reset*/
	scp_reset_workqueue = create_singlethread_workqueue("SCP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&scp_sys_reset_work.work, scp_sys_reset_ws);
	/*init completion for identify scp aed finished*/
	init_completion(&scp_sys_reset_cp);

	scp_loader_base_virt = (phys_addr_t)(size_t)ioremap_wc(
		scp_region_info_copy.ap_loader_start,
		scp_region_info_copy.ap_loader_size);
	pr_debug("[SCP] loader image mem: virt:0x%llx - 0x%llx\n",
		(uint64_t)(phys_addr_t)scp_loader_base_virt,
		(uint64_t)(phys_addr_t)scp_loader_base_virt +
		(phys_addr_t)scp_region_info_copy.ap_loader_size);
	/*init wake,
	 *this is for prevent scp pll cpu clock disabled during reset flow
	 */
	scp_reset_lock = wakeup_source_register(NULL, "scp reset wakelock");
	/* init reset by cmd flag */
	scp_reset_by_cmd = 0;

	if ((int)(scp_region_info_copy.ap_dram_size) > 0) {
		/*if l1c enable, map it */
		scp_l1c_start_virt = ioremap_wc(
			scp_region_info_copy.ap_dram_start,
			scp_region_info_copy.ap_dram_size);
	}
#endif
}

static int scp_device_probe(struct platform_device *pdev)
{
#define FEATURE_TBL_ELEM_NUM (2)
	int ret = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;
	unsigned int scp_feature_num = 0;
	int i, f_idx, f_mcps;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.sram)) {
		pr_err("[SCP] scpreg.sram error\n");
		return -1;
	}
	scpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[SCP] sram base = %p %x\n"
		, scpreg.sram, scpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	scpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg)) {
		pr_err("[SCP] scpreg.cfg error\n");
		return -1;
	}
	pr_debug("[SCP] cfg base = %p\n", scpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	scpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.clkctrl)) {
		pr_err("[SCP] scpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[SCP] clkctrl base = %p\n", scpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	scpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.l1cctrl)) {
		pr_debug("[SCP] scpreg.clkctrl error\n");
		return -1;
	}

	pr_debug("[SCP] l1cctrl base = %p\n", scpreg.l1cctrl);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("[SCP] IRQ resource not found!\n");
		return -1;
	}
	scpreg.irq = res->start;
	pr_debug("[SCP] scpreg.irq = %d\n", scpreg.irq);

	of_property_read_u32(pdev->dev.of_node,
			"scp_sramSize",
			&scpreg.scp_tcmsize);
	if (!scpreg.scp_tcmsize) {
		pr_err("[SCP] total_tcmsize not found\n");
		return -1;
	}
	pr_debug("[SCP] scpreg.scp_tcmsize = %d\n", scpreg.scp_tcmsize);

	/* get number of feature settings in dts */
	scp_feature_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"scp_feature_tbl")
				/ FEATURE_TBL_ELEM_NUM;
	if (scp_feature_num <= 0) {
		pr_err("[SCP] scp_feature_tbl not found\n");
		return -1;
	}

	/* get specified features's mcps */
	for (i = 0; i < scp_feature_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"scp_feature_tbl",
				i * FEATURE_TBL_ELEM_NUM,
				&f_idx);
		if (ret) {
			pr_err("Cannot get feature index(%d)\n", i);
			return -1;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"scp_feature_tbl",
				(i * FEATURE_TBL_ELEM_NUM) + 1,
				&f_mcps);
		if (ret) {
			pr_err("Cannot get feature mcps(%d)\n", i);
			return -1;
		}

		if (f_idx >= NUM_FEATURE_ID) {
			pr_notice("[SCP] skip unexpected index, %d\n", f_idx);
			continue;
		}

		feature_table[f_idx].feature = f_mcps;
		pr_err("@@@@: <%d  %d>\n", f_idx, f_mcps);
	}

	/*scp core 1*/
	of_property_read_string(pdev->dev.of_node, "core_1", &core_status);
	if (strcmp(core_status, "enable") != 0)
		pr_err("[SCP] core_1 not enable\n");
	else {
		pr_debug("[SCP] core_1 enable\n");
		scp_enable[SCP_A_ID] = 1;
	}

#if SCP_RESERVED_MEM && defined(CONFIG_OF)
	/*scp resvered memory*/
	ret = scp_reserve_memory_ioremap(pdev);
	if (ret)
		pr_err("[SCP]scp_reserve_memory_ioremap failed\n");

#endif  // SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)

	return ret;
}

static int scp_device_remove(struct platform_device *dev)
{
	return 0;
}

static int scpsys_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.scpsys = devm_ioremap_resource(dev, res);
	pr_debug("[SCP] scpreg.scpsys = %p\n", scpreg.scpsys);
	if (IS_ERR((void const *) scpreg.scpsys)) {
		pr_err("[SCP] scpreg.scpsys error\n");
		return -1;
	}
	return ret;
}

static int scpsys_device_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id scp_of_ids[] = {
	{ .compatible = "mediatek,scp", },
	{}
};

static struct platform_driver mtk_scp_device = {
	.probe = scp_device_probe,
	.remove = scp_device_remove,
	.driver = {
		.name = "scp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = scp_of_ids,
#endif
	},
};

static const struct of_device_id scpsys_of_ids[] = {
	{ .compatible = "mediatek,scpinfra", },
	{}
};

static struct platform_driver mtk_scpsys_device = {
	.probe = scpsys_device_probe,
	.remove = scpsys_device_remove,
	.driver = {
		.name = "scpsys",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = scpsys_of_ids,
#endif
	},
};

/*
 * driver initialization entry point
 */
static int __init scp_init(void)
{
	int ret = 0;
	int i = 0;
#if SCP_BOOT_TIME_OUT_MONITOR
	scp_ready_timer[SCP_A_ID].tid = SCP_A_TIMER;
	timer_setup(&(scp_ready_timer[SCP_A_ID].tl), scp_wait_ready_timeout, 0);
	scp_timeout_times = 0;
#endif

    /* scp platform initialise */
	pr_debug("[SCP] %s begins\n", __func__);

	/* scp ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++) {
		scp_enable[i] = 0;
		scp_ready[i] = 0;
	}

#if SCP_DVFS_INIT_ENABLE
	scp_dvfs_init();
	wait_scp_dvfs_init_done();

	/* pll maybe gate, request pll before access any scp reg/sram */
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* keep 26Mhz */
	scp_to_spm_resource_req(SCP_DVFS_SMC_RESOURCE_REQ,
			SCP_REQ_RESOURCE_26M);
#endif  // CONFIG_FPGA_EARLY_PORTING

	if (platform_driver_register(&mtk_scp_device)) {
		pr_err("[SCP] scp probe fail\n");
		goto err;
	}

	if (platform_driver_register(&mtk_scpsys_device)) {
		pr_err("[SCP] scpsys probe fail\n");
		goto err_1;
	}

	/* skip initial if dts status = "disable" */
	if (!scp_enable[SCP_A_ID]) {
		pr_err("[SCP] scp disabled!!\n");
		goto err_2;
	}
	/* scp platform initialise */
	scp_region_info_init();
	pr_debug("[SCP] platform init\n");
	scp_awake_init();
	scp_workqueue = create_singlethread_workqueue("SCP_WQ");
	ret = scp_excep_init();
	if (ret) {
		pr_debug("[SCP]Excep Init Fail\n");
		goto err_2;
	}

	/* scp ipi initialise */
	scp_send_buff[SCP_A_ID] = kmalloc((size_t) SHARE_BUF_SIZE, GFP_KERNEL);
	if (!scp_send_buff[SCP_A_ID])
		goto err_3;

	scp_recv_buff[SCP_A_ID] = kmalloc((size_t) SHARE_BUF_SIZE, GFP_KERNEL);
	if (!scp_recv_buff[SCP_A_ID])
		goto err_3;

	INIT_WORK(&scp_A_notify_work.work, scp_A_notify_ws);
	INIT_WORK(&scp_timeout_work.work, scp_timeout_ws);

	scp_A_irq_init();
	scp_A_ipi_init();

	scp_ipi_registration(IPI_SCP_A_READY,
			scp_A_ready_ipi_handler, "scp_A_ready");

	scp_ipi_registration(IPI_SCP_ERROR_INFO,
			scp_err_info_handler, "scp_err_info_handler");

	ret = register_pm_notifier(&scp_pm_notifier_block);
	if (ret)
		pr_err("[SCP] failed to register PM notifier %d\n", ret);

	/* scp sysfs initialise */
	pr_debug("[SCP] sysfs init\n");
	ret = create_files();
	if (unlikely(ret != 0)) {
		pr_err("[SCP] create files failed\n");
		goto err_3;
	}

	/* scp request irq */
	pr_debug("[SCP] request_irq\n");
	ret = request_irq(scpreg.irq, scp_A_irq_handler,
			IRQF_TRIGGER_NONE, "SCP A IPC2HOST", NULL);
	if (ret) {
		pr_err("[SCP] CM4 A require irq failed\n");
		goto err_3;
	}

#if SCP_LOGGER_ENABLE
	/* scp logger initialise */
	pr_debug("[SCP] logger init\n");
	/*create wq for scp logger*/
	scp_logger_workqueue = create_singlethread_workqueue("SCP_LOG_WQ");
	if (scp_logger_init(scp_get_reserve_mem_virt(SCP_A_LOGGER_MEM_ID),
		scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID)) == -1) {
		pr_err("[SCP] scp_logger_init_fail\n");
		goto err_3;
	}
#endif

#if ENABLE_SCP_EMI_PROTECTION
	set_scp_mpu();
#endif

	scp_recovery_init();

#ifdef SCP_PARAMS_TO_SCP_SUPPORT
	/* The function, sending parameters to scp must be anchored before
	 * 1. disabling 26M, 2. resetting SCP
	 */
	if (params_to_scp() != 0)
		goto err_3;
#endif

#if SCP_DVFS_INIT_ENABLE
	/* remember to release pll */
	scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

	driver_init_done = true;
	reset_scp(SCP_ALL_ENABLE);

	return ret;

err_3:
	scp_excep_cleanup();
err_2:
	platform_driver_register(&mtk_scpsys_device);
err_1:
	platform_driver_unregister(&mtk_scp_device);
err:
#if SCP_DVFS_INIT_ENABLE
	/* remember to release pll */
	scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif
	return -1;
}

/*
 * driver exit point
 */
static void __exit scp_exit(void)
{
	int i = 0;

#if SCP_DVFS_INIT_ENABLE
	scp_dvfs_exit();
#endif

#if SCP_LOGGER_ENABLE
	scp_logger_uninit();
#endif

	free_irq(scpreg.irq, NULL);
	misc_deregister(&scp_device);

	flush_workqueue(scp_workqueue);
	destroy_workqueue(scp_workqueue);

#if SCP_RECOVERY_SUPPORT
	flush_workqueue(scp_reset_workqueue);
	destroy_workqueue(scp_reset_workqueue);
#endif

#if SCP_LOGGER_ENABLE
	flush_workqueue(scp_logger_workqueue);
	destroy_workqueue(scp_logger_workqueue);
#endif

#if SCP_BOOT_TIME_OUT_MONITOR
	for (i = 0; i < SCP_CORE_TOTAL ; i++)
		del_timer(&(scp_ready_timer[i].tl));
#endif

	scp_excep_cleanup();

	for (i = 0; i < SCP_CORE_TOTAL; i++) {
		kfree(scp_send_buff[i]);
		kfree(scp_recv_buff[i]);
	}
}

device_initcall_sync(scp_init);
module_exit(scp_exit);

MODULE_DESCRIPTION("MEDIATEK Module SCP driver");
MODULE_AUTHOR("McInnis Yu<mcinnis.yu@mediatek.com>");
MODULE_LICENSE("GPL");



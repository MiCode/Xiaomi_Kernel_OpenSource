/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <mt-plat/sync_write.h>
//#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "scp_feature_define.h"
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "mtk_spm_resource_req.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include "scp_reservedmem_define.h"
#endif


#if ENABLE_SCP_EMI_PROTECTION
#include <mt_emi_api.h>
#endif


/* scp semaphore timout count definition*/
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* scp ready timout definition*/
#define SCP_READY_TIMEOUT (30 * HZ) /* 30 seconds*/
#define SCP_A_TIMER 0


/* scp ready status for notify*/
unsigned int scp_ready[SCP_CORE_TOTAL];

/* scp enable status*/
unsigned int scp_enable[SCP_CORE_TOTAL];

/* scp dvfs variable*/
unsigned int scp_expected_freq;
unsigned int scp_current_freq;

#if SCP_RECOVERY_SUPPORT
unsigned int scp_recovery_flag[SCP_CORE_TOTAL];
#define SCP_A_RECOVERY_OK	0x44
/*  scp_reset_status
 *  0: scp not in reset status
 *  1: scp in reset status
 */
atomic_t scp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);
unsigned int scp_reset_by_cmd;
struct scp_region_info_st *scp_region_info;
struct completion scp_sys_reset_cp;
struct scp_work_struct scp_sys_reset_work;
struct wakeup_source scp_reset_lock;
phys_addr_t scp_loader_base_phys;
phys_addr_t scp_loader_base_virt;
phys_addr_t scp_fw_base_phys;
uint32_t scp_loader_size;
uint32_t scp_fw_size;
DEFINE_SPINLOCK(scp_reset_spinlock);
#endif

phys_addr_t scp_mem_base_phys;
phys_addr_t scp_mem_base_virt;
phys_addr_t scp_mem_size;
struct scp_regs scpreg;

unsigned char *scp_send_buff[SCP_CORE_TOTAL];
unsigned char *scp_recv_buff[SCP_CORE_TOTAL];

static struct workqueue_struct *scp_workqueue;
static struct workqueue_struct *scp_reset_workqueue;
#if SCP_LOGGER_ENABLE
static struct workqueue_struct *scp_logger_workqueue;
#endif
#if SCP_BOOT_TIME_OUT_MONITOR
static struct timer_list scp_ready_timer[SCP_CORE_TOTAL];
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
unsigned char **scp_swap_buf;

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
		pr_debug("get_scp_semaphore: awake scp fail\n");
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
		pr_debug("get_scp_semaphore: scp_awake_unlock fail\n");


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
		pr_debug("release_scp_semaphore: awake scp fail\n");
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
		pr_debug("release_scp_semaphore: scp_awake_unlock fail\n");


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
	spm_resource_req(SPM_RESOURCE_USER_SCP, SPM_RESOURCE_RELEASE);
	/* register scp dvfs*/
	msleep(2000);
	__pm_relax(&scp_reset_lock);
	scp_register_feature(RTOS_FEATURE_ID);
#endif

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
	if (scp_timeout_times < 10)
		scp_send_reset_wq(RESET_TYPE_AWAKE);

	scp_timeout_times++;
	pr_notice("[SCP] scp_timeout_times=%x\n", scp_timeout_times);
}

/*
 * mark notify flag to 1 to notify apps to start their tasks
 */
static void scp_A_set_ready(void)
{
	pr_debug("%s()\n", __func__);
#if SCP_BOOT_TIME_OUT_MONITOR
	del_timer(&scp_ready_timer[SCP_A_ID]);
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
static void scp_wait_ready_timeout(unsigned long data)
{
	pr_notice("%s(),timer data=%lu\n", __func__, data);
	/*data=0: SCP A  ,  data=1: SCP B*/
	scp_timeout_work.flags = 0;
	scp_timeout_work.id = SCP_A_ID;
	scp_schedule_work(&scp_timeout_work);
}

#endif
/*
 * handle notification from scp
 * mark scp is ready for running tasks
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
	unsigned int *reg;
#if SCP_BOOT_TIME_OUT_MONITOR
	int i;

	for (i = 0; i < SCP_CORE_TOTAL ; i++)
		del_timer(&scp_ready_timer[i]);
#endif
	/*scp_logger_stop();*/
	if (((reset & 0xf0) == 0x10) ||
				((reset & 0xf0) == 0x00)) { /* reset A or All*/
		/*reset scp A*/
		mutex_lock(&scp_A_notify_mutex);
		blocking_notifier_call_chain(&scp_A_notifier_list
					, SCP_EVENT_STOP, NULL);
		mutex_unlock(&scp_A_notify_mutex);

#if SCP_DVFS_INIT_ENABLE
		/* request pll clock before turn on scp */
		scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

		reg = (unsigned int *)scpreg.cfg;
		if (reset & 0x0f) { /* do reset */
			/* make sure scp is in idle state */
			int timeout = 50; /* max wait 1s */

			while (--timeout) {
#if SCP_RECOVERY_SUPPORT
			if (*(unsigned int *)SCP_GPR_CM4_A_REBOOT == 0x34) {
				if (readl(SCP_SLEEP_STATUS_REG)
						& SCP_A_IS_SLEEP) {
				/* reset */
				*(unsigned int *)reg = 0x0;
				scp_ready[SCP_A_ID] = 0;
				*(unsigned int *)SCP_GPR_CM4_A_REBOOT = 1;
				/* lock pll for ulposc calibration */
				 /* do it only in reset */
				dsb(SY);
				break;
				}
			}
#else
			if (readl(SCP_SLEEP_STATUS_REG) & SCP_A_IS_SLEEP) {
				/* reset */
				*(unsigned int *)reg = 0x0;
				scp_ready[SCP_A_ID] = 0;
				dsb(SY);
				break;
			}
#endif
			mdelay(20);
			if (timeout == 0)
				pr_debug("[SCP]scp A reset timeout,skip\n");
		}
		pr_debug("[SCP] wait scp A reset timeout %d\n", timeout);
	}
	if (scp_enable[SCP_A_ID]) {
		pr_debug("[SCP] reset scp A\n");
		*(unsigned int *)reg = 0x1;
		dsb(SY);
#if SCP_BOOT_TIME_OUT_MONITOR
		init_timer(&scp_ready_timer[SCP_A_ID]);
		scp_ready_timer[SCP_A_ID].expires = jiffies + SCP_READY_TIMEOUT;
		scp_ready_timer[SCP_A_ID].function = &scp_wait_ready_timeout;
		scp_ready_timer[SCP_A_ID].data = (unsigned long) SCP_A_TIMER;
		/*data=0: SCP A    1: SCP B*/
		add_timer(&scp_ready_timer[SCP_A_ID]);
#endif
		}
	}

	pr_debug("[SCP] reset scp done\n");

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
			pr_debug("[SCP] scp_pm_event SCP reboot\n");
			retval = reset_scp(1);
			if (retval < 0) {
				retval = -EINVAL;
				pr_debug("[SCP]scp_pm_event SCP reboot Fail\n");
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

DEVICE_ATTR(scp_A_status, 0444, scp_A_status_show, NULL);

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

DEVICE_ATTR(scp_A_reg_status, 0444, scp_A_reg_status_show, NULL);

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

DEVICE_ATTR(scp_A_db_test, 0444, scp_A_db_test_show, NULL);


#ifdef CONFIG_MTK_ENG_BUILD
static ssize_t scp_ee_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_ee_enable);
}

static ssize_t scp_ee_ctrl(struct device *kobj
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
DEVICE_ATTR(scp_ee_enable, 0644, scp_ee_show, scp_ee_ctrl);

static inline ssize_t scp_A_awake_lock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_lock(SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR(scp_A_awake_lock, 0444, scp_A_awake_lock_show, NULL);

static inline ssize_t scp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_unlock(SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR(scp_A_awake_unlock, 0444, scp_A_awake_unlock_show, NULL);


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

DEVICE_ATTR(scp_ipi_test, 0444, scp_ipi_test_show, NULL);

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
static ssize_t scp_wdt_trigger(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	pr_debug("scp_wdt_trigger: %s\n", buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			scp_wdt_reset(SCP_A_ID);
	}
	return count;
}

DEVICE_ATTR(wdt_reset, 0200, NULL, scp_wdt_trigger);

/*
 * trigger scp reset manually
 * debug use
 */
static ssize_t scp_reset_trigger(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	pr_debug("scp_reset_trigger: %s\n", buf);

	/* scp reset by cmdm set flag =1 */
	scp_reset_by_cmd = 1;
	scp_wdt_reset(SCP_A_ID);

	return count;
}

DEVICE_ATTR(scp_reset, 0200, NULL, scp_reset_trigger);
/*
 * trigger wdt manually
 * debug use
 */

static ssize_t scp_recovery_flag_r(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_recovery_flag[SCP_A_ID]);
}
static ssize_t scp_recovery_flag_w(struct device *dev
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

DEVICE_ATTR(recovery_flag, 0600, scp_recovery_flag_r, scp_recovery_flag_w);

#endif

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
#endif
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif
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

#endif

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
					, &dev_attr_recovery_flag);
	if (unlikely(ret != 0))
		return ret;

#endif

	return 0;
}
#if SCP_RESERVED_MEM
#ifdef CONFIG_OF_RESERVED_MEM
#define SCP_MEM_RESERVED_KEY "mediatek,reserve-memory-scp_share"
int scp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	enum scp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;
	unsigned int num = 0;

	scp_mem_base_phys = (phys_addr_t) 0;
	scp_mem_size = (phys_addr_t) 0;

	num = (unsigned int)(sizeof(scp_reserve_mblock)
			/ sizeof(scp_reserve_mblock[0]));
	if (num != NUMS_MEM_ID) {
		pr_err("[SCP] number of entries of reserved memory %u / %u\n",
			num,
			NUMS_MEM_ID);

		return -1;
	}

	for (id = 0; id < NUMS_MEM_ID; id++)
		accumlate_memory_size += scp_reserve_mblock[id].size;

	if (accumlate_memory_size > rmem->size) {
		pr_err("[SCP] accumlated memory size = %llu / %llu\n"
			, (unsigned long long)accumlate_memory_size
			, (unsigned long long)rmem->size);

		return -1;
	}

	scp_mem_base_phys = (phys_addr_t) rmem->base;
	scp_mem_size = (phys_addr_t) rmem->size;
	if ((scp_mem_base_phys >= (0x90000000ULL)) ||
				 (scp_mem_base_phys <= 0x0)) {
		/*The scp remap region is fixed, only
		 * 0x4000_0000ULL~0x8FFF_FFFFULL
		 * can be accessible
		 */
		pr_err("[SCP]allocated memory(0x%llx)is larger than expected\n",
			    (unsigned long long)scp_mem_base_phys);
		/*should not call WARN_ON() here or there is no log, return -1
		 * instead.
		 */
		return -1;
	}

	pr_debug("[SCP] phys:0x%llx - 0x%llx (0x%llx)\n",
		(unsigned long long)(phys_addr_t)rmem->base,
		(unsigned long long)((phys_addr_t)rmem->base +
			(phys_addr_t)rmem->size),
		(unsigned long long)(phys_addr_t)rmem->size);

	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MEM_ID; id++) {
		scp_reserve_mblock[id].start_phys = scp_mem_base_phys +
							accumlate_memory_size;
		accumlate_memory_size += scp_reserve_mblock[id].size;

		pr_debug("[SCP][reserve_mem:%d]:phys:0x%llx - 0x%llx (0x%llx)\n",
			id,
			(unsigned long long)scp_reserve_mblock[id].start_phys,
			(unsigned long long)(scp_reserve_mblock[id].start_phys +
				scp_reserve_mblock[id].size),
			(unsigned long long)scp_reserve_mblock[id].size);
	}

	return 0;
}

RESERVEDMEM_OF_DECLARE(scp_reserve_mem_init
			, SCP_MEM_RESERVED_KEY, scp_reserve_mem_of_init);
#endif
#endif
phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_phys);

phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_virt);

phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_err("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_size);

#if SCP_RESERVED_MEM
static int scp_reserve_memory_ioremap(void)
{
	enum scp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size;


	if ((scp_mem_base_phys >= (0x90000000ULL)) ||
				(scp_mem_base_phys <= 0x0)) {
		/*The scp remap region is fixed, only
		 * 0x4000_0000ULL~0x8FFF_FFFFULL
		 * can be accessible
		 */
		pr_err("[SCP]allocated memory(0x%llx)is larger than expected\n",
			(unsigned long long)scp_mem_base_phys);
		/*call WARN_ON() to assert the unexpected memory allocation
		 */
		WARN_ON(1);
		return -1;
	}
	accumlate_memory_size = 0;
	scp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(scp_mem_base_phys
							, scp_mem_size);
	pr_debug("[SCP]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		(unsigned long long)scp_mem_base_virt,
		(unsigned long long)scp_mem_base_virt + scp_mem_size,
		(unsigned long long)scp_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		scp_reserve_mblock[id].start_virt = scp_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += scp_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or scp_reserve_mblock does not match dts
	 */
	WARN_ON(accumlate_memory_size > scp_mem_size);
#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		pr_debug("[SCP][mem_reserve-%d] phys:0x%llx\n",
			id,
			(unsigned long long)scp_get_reserve_mem_phys(id));
		pr_debug("[SCP][mem_reserve-%d] virt:0x%llx\n",
			id,
			(unsigned long long)scp_get_reserve_mem_virt(id));
		pr_debug("[SCP][mem_reserve-%d] size:0x%llx\n",
			id,
			(unsigned long long)scp_get_reserve_mem_size(id));
	}
#endif
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

	pr_debug("[SCP]MPU protect SCP Share region<%d:%08llx:%08llx> %x, %x\n",
			MPU_REGION_ID_SCP_SMEM,
			region_info.start,
			region_info.end,
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
		pr_debug("scp_register_feature:not ready, scp=%u\n"
						, scp_ready[SCP_A_ID]);
		return;
	}

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&scp_feature_mutex);

	/*SCP keep awake */
	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("scp_register_feature: awake scp fail\n");
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
		pr_debug("scp_register_feature: awake unlock fail\n");

	mutex_unlock(&scp_feature_mutex);
}

void scp_deregister_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID]) {
		pr_debug("scp_deregister_feature:not ready, scp=%u\n"
						, scp_ready[SCP_A_ID]);
		return;
	}

	mutex_lock(&scp_feature_mutex);

	/*SCP keep awake */
	if (scp_awake_lock(SCP_A_ID) == -1) {
		pr_debug("scp_deregister_feature: awake scp fail\n");
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
		pr_debug("scp_deregister_feature: awake unlock fail\n");

	mutex_unlock(&scp_feature_mutex);
}

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
	/* scp cfg reg,*/
	unsigned int *scp_reset_reg;
	unsigned long spin_flags;
	/* make sure scp is in idle state */
	int timeout = 50; /* max wait 1s */


	scp_reset_reg = (unsigned int *)scpreg.cfg;

	/*notify scp functions stop*/
	pr_debug("%s(): scp_extern_notify\n", __func__);
	scp_extern_notify(SCP_EVENT_STOP);
	/*set scp not ready*/
	pr_debug("%s(): scp_status_set\n", __func__);

	/*
	 *   scp_ready:
	 *   SCP_PLATFORM_STOP  = 0,
	 *   SCP_PLATFORM_READY = 1,
	 */
	scp_ready[SCP_A_ID] = 0;

	/* wake lock AP*/
	__pm_stay_awake(&scp_reset_lock);
	/* keep Univpll */
	spm_resource_req(SPM_RESOURCE_USER_SCP, SPM_RESOURCE_CK_26M);

	/*request pll clock before turn off scp */
	pr_debug("%s(): scp_pll_ctrl_set\n", __func__);
#if SCP_DVFS_INIT_ENABLE
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

	/*workqueue for scp ee, scp reset by cmd will not trigger scp ee*/
	if (scp_reset_by_cmd == 0) {
		pr_debug("%s(): scp_aed_reset\n", __func__);
		scp_aed_reset(EXCEP_RUNTIME, SCP_A_ID);

		/*wait scp ee finished*/
		pr_debug("%s(): wait ee finished...\n", __func__);
		if (wait_for_completion_interruptible_timeout(&scp_sys_reset_cp
			, jiffies_to_msecs(1000)) == 0)
			pr_debug("scp_sys_reset_ws: scp ee time out\n");
	}

	/*disable scp logger
	 * 0: scp logger disable
	 * 1: scp logger enable
	 */
	pr_debug("%s(): disable logger\n", __func__);
	scp_logger_init_set(0);

	/* scp reset by CMD, WDT or awake fail */
	if (scp_reset_type == RESET_TYPE_WDT) {
		/* reset type scp WDT */
		pr_notice("%s(): scp wdt reset\n", __func__);
		/* make sure scp is in idle state */
		while (timeout--) {
			if (*(unsigned int *)SCP_GPR_CM4_A_REBOOT == 0x34) {
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

		*(unsigned int *)scp_reset_reg = 0x0;
		*(unsigned int *)SCP_GPR_CM4_A_REBOOT = 1;
		dsb(SY);
	} else if (scp_reset_type == RESET_TYPE_AWAKE) {
		/* reset type awake fail */
		pr_debug("%s(): scp awake fail reset\n", __func__);
		/* stop scp */
		*(unsigned int *)scp_reset_reg = 0x0;
	} else {
		/* reset type cmd */
		pr_debug("%s(): scp awake fail reset\n", __func__);
		/* stop scp */
		*(unsigned int *)scp_reset_reg = 0x0;
	}

	/*scp reset*/
	scp_sys_full_reset();

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_reset_awake_counts();
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	/*start scp*/
	pr_debug("[SCP]start scp\n");
	*(unsigned int *)scp_reset_reg = 0x1;
	dsb(SY);
#if SCP_BOOT_TIME_OUT_MONITOR
	init_timer(&scp_ready_timer[SCP_A_ID]);
	scp_ready_timer[SCP_A_ID].expires = jiffies + SCP_READY_TIMEOUT;
	scp_ready_timer[SCP_A_ID].function = &scp_wait_ready_timeout;
	/* 0: SCP A, 1: SCP B */
	scp_ready_timer[SCP_A_ID].data = (unsigned long) SCP_A_TIMER;
	add_timer(&scp_ready_timer[SCP_A_ID]);
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

static int scp_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.sram)) {
		pr_err("[SCP] scpreg.sram error\n");
		return -1;
	}
	scpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[SCP] sram base=0x%p %x\n"
		, scpreg.sram, scpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	scpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg)) {
		pr_err("[SCP] scpreg.cfg error\n");
		return -1;
	}
	pr_debug("[SCP] cfg base=0x%p\n", scpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	scpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.clkctrl)) {
		pr_err("[SCP] scpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[SCP] clkctrl base=0x%p\n", scpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	scpreg.irq = res->start;
	pr_debug("[SCP] scpreg.irq=%d\n", scpreg.irq);

	of_property_read_u32(pdev->dev.of_node, "scp_sramSize"
						, &scpreg.scp_tcmsize);
	if (!scpreg.scp_tcmsize) {
		pr_err("[SCP] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[SCP] scpreg.scp_tcmsize =%d\n", scpreg.scp_tcmsize);

	/*scp core 1*/
	of_property_read_string(pdev->dev.of_node, "core_1", &core_status);
	if (strcmp(core_status, "enable") != 0)
		pr_err("[SCP] core_1 not enable\n");
	else {
		pr_debug("[SCP] core_1 enable\n");
		scp_enable[SCP_A_ID] = 1;
	}

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
	pr_debug("[SCP] scpreg.scpsys %p\n", scpreg.scpsys);
	if (IS_ERR((void const *) scpreg.scpsys)) {
		pr_err("[SCP] scpreg.sram error\n");
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
	{ .compatible = "mediatek,scpsys", },
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

    /* scp platform initialise */
	pr_debug("[SCP] platform init, scp_init\n");

	/* scp ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++) {
		scp_enable[i] = 0;
		scp_ready[i] = 0;
	}

#if SCP_DVFS_INIT_ENABLE
	scp_dvfs_init();
	/* pll maybe gate, request pll before access any scp reg/sram */
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif
	/* keep Univpll */
	spm_resource_req(SPM_RESOURCE_USER_SCP, SPM_RESOURCE_CK_26M);

	/* make sure the reserved memory for scp is ready */
	if (scp_mem_size == 0) {
		pr_err("[SCP] Reserving memory by of_device for SCP failed.\n");
		return -1;
	}

	/* scp platform initialise */
	pr_debug("[SCP] platform init\n");
	scp_awake_init();
	scp_workqueue = create_workqueue("SCP_WQ");
	ret = scp_excep_init();
	if (ret) {
		pr_err("[SCP]Excep Init Fail\n");
		goto err;
	}
	if (platform_driver_register(&mtk_scp_device))
		pr_err("[SCP] scp probe fail\n");

	if (platform_driver_register(&mtk_scpsys_device))
		pr_err("[SCP] scpsys probe fail\n");

	/* skip initial if dts status = "disable" */
	if (!scp_enable[SCP_A_ID]) {
		pr_err("[SCP] scp disabled!!\n");
		goto err;
	}
	/* scp ipi initialise */
	scp_send_buff[SCP_A_ID] = kmalloc((size_t) SHARE_BUF_SIZE, GFP_KERNEL);
	if (!scp_send_buff[SCP_A_ID])
		goto err;

	scp_recv_buff[SCP_A_ID] = kmalloc((size_t) SHARE_BUF_SIZE, GFP_KERNEL);
	if (!scp_recv_buff[SCP_A_ID])
		goto err;

	INIT_WORK(&scp_A_notify_work.work, scp_A_notify_ws);
	INIT_WORK(&scp_timeout_work.work, scp_timeout_ws);

	scp_A_irq_init();
	scp_A_ipi_init();

	scp_ipi_registration(IPI_SCP_A_READY,
			 scp_A_ready_ipi_handler, "scp_A_ready");

	/* scp ramdump initialise */
	pr_debug("[SCP] ramdump init\n");
	scp_ram_dump_init();
	ret = register_pm_notifier(&scp_pm_notifier_block);

	if (ret)
		pr_err("[SCP] failed to register PM notifier %d\n", ret);

	/* scp sysfs initialise */
	pr_debug("[SCP] sysfs init\n");
	ret = create_files();

	if (unlikely(ret != 0)) {
		pr_err("[SCP] create files failed\n");
		goto err;
	}

	/* scp request irq */
	pr_debug("[SCP] request_irq\n");
	ret = request_irq(scpreg.irq, scp_A_irq_handler,
			IRQF_TRIGGER_NONE, "SCP A IPC2HOST", NULL);
	if (ret) {
		pr_err("[SCP] CM4 A require irq failed\n");
		goto err;
	}

#if SCP_RESERVED_MEM
	/*scp resvered memory*/
	pr_notice("[SCP] scp_reserve_memory_ioremap\n");
	ret = scp_reserve_memory_ioremap();
	if (ret) {
		pr_err("[SCP]scp_reserve_memory_ioremap failed\n");
		goto err;
	}
#endif
#if SCP_LOGGER_ENABLE
	/* scp logger initialise */
	pr_debug("[SCP] logger init\n");
	/*create wq for scp logger*/
	scp_logger_workqueue = create_workqueue("SCP_LOG_WQ");
	if (scp_logger_init(scp_get_reserve_mem_virt(SCP_A_LOGGER_MEM_ID),
			scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID)) == -1) {
		pr_err("[SCP] scp_logger_init_fail\n");
		goto err;
	}
#endif

#if ENABLE_SCP_EMI_PROTECTION
	set_scp_mpu();
#endif

#if SCP_RECOVERY_SUPPORT
	/*create wq for scp reset*/
	scp_reset_workqueue = create_workqueue("SCP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&scp_sys_reset_work.work, scp_sys_reset_ws);
	/*init completion for identify scp aed finished*/
	init_completion(&scp_sys_reset_cp);
	/*get scp loader/firmware info from scp sram*/
	scp_region_info = (SCP_TCM + 0x400);
	pr_debug("[SCP]scp_region_info=%p\n", scp_region_info);

	scp_loader_base_phys = scp_region_info->ap_loader_start;
	scp_loader_size = scp_region_info->ap_loader_size;
	scp_fw_base_phys = scp_region_info->ap_firmware_start;
	scp_fw_size = scp_region_info->ap_firmware_size;
	pr_debug("[SCP]loader_addr=0x%llx,_sz=0x%x,fw_addr=0x%llx,sz=0x%x\n",
		scp_loader_base_phys,
		scp_loader_size,
		scp_fw_base_phys,
		scp_fw_size);


	scp_loader_base_virt =
			(phys_addr_t)(size_t)ioremap_wc(scp_loader_base_phys
			, scp_loader_size);
	pr_debug("[SCP]loader image mem:virt:0x%llx - 0x%llx (0x%x)\n",
		(phys_addr_t)scp_loader_base_virt,
		(phys_addr_t)scp_loader_base_virt +
		(phys_addr_t)scp_loader_size,
		scp_loader_size);
	/*init wake,
	 *this is for prevent scp pll cpu clock disabled during reset flow
	 */
	wakeup_source_init(&scp_reset_lock, "scp reset wakelock");
	/* init reset by cmd flag*/
	scp_reset_by_cmd = 0;
#endif

#if SCP_DVFS_INIT_ENABLE
	wait_scp_dvfs_init_done();
	/* remember to release pll */
	scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

	driver_init_done = true;
	reset_scp(SCP_ALL_ENABLE);

	return ret;

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
#if SCP_BOOT_TIME_OUT_MONITOR
	int i = 0;
#endif

#if SCP_DVFS_INIT_ENABLE
	scp_dvfs_exit();
#endif
#if SCP_LOGGER_ENABLE
	scp_logger_uninit();
#endif
	free_irq(scpreg.irq, NULL);
	misc_deregister(&scp_device);

	flush_workqueue(scp_workqueue);
	/*scp_logger_cleanup();*/
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
		del_timer(&scp_ready_timer[i]);
#endif
	kfree(scp_swap_buf);
}

module_init(scp_init);
module_exit(scp_exit);

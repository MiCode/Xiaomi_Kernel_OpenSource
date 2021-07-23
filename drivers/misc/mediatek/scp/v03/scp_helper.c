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
#include "scp_err_info.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp_scpctl.h"
#include <linux/syscore_ops.h>

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include "scp_reservedmem_define.h"
#endif

#if ENABLE_SCP_EMI_PROTECTION
#include "memory/mediatek/emi.h"
#endif

/* scp mbox/ipi related */
#include <mt-plat/mtk-mbox.h>
#include "scp_ipi.h"

/* scp semaphore timeout count definition */
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* scp ready timeout definition */
#define SCP_READY_TIMEOUT (3 * HZ) /* 30 seconds*/
#define SCP_A_TIMER 0

/* scp ipi message buffer */
uint32_t msg_scp_ready0, msg_scp_ready1;
char msg_scp_err_info0[40], msg_scp_err_info1[40];

/* scp ready status for notify*/
unsigned int scp_ready[SCP_CORE_TOTAL];

/* scp enable status*/
unsigned int scp_enable[SCP_CORE_TOTAL];

/* scp dvfs variable*/
unsigned int scp_expected_freq;
unsigned int scp_current_freq;

/*scp awake variable*/
int scp_awake_counts[SCP_CORE_TOTAL];


unsigned int scp_recovery_flag[SCP_CORE_TOTAL];
#define SCP_A_RECOVERY_OK	0x44
/*  scp_reset_status
 *  0: scp not in reset status
 *  1: scp in reset status
 */
atomic_t scp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);
unsigned int scp_reset_by_cmd;
struct scp_region_info_st *scp_region_info;
/* shadow it due to sram may not access during sleep */
struct scp_region_info_st scp_region_info_copy;

struct scp_work_struct scp_sys_reset_work;
struct wakeup_source scp_reset_lock;

DEFINE_SPINLOCK(scp_reset_spinlock);

/* l1c enable */
void __iomem *scp_ap_dram_virt;
void __iomem *scp_loader_virt;
void __iomem *scp_regdump_virt;


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

static unsigned int scp_timeout_times;

static DEFINE_MUTEX(scp_A_notify_mutex);
static DEFINE_MUTEX(scp_feature_mutex);
static DEFINE_MUTEX(scp_register_sensor_mutex);

char *core_ids[SCP_CORE_TOTAL] = {"SCP A"};
DEFINE_SPINLOCK(scp_awake_spinlock);
/* set flag after driver initial done */
static bool driver_init_done;
struct scp_ipi_irq {
	const char *name;
	int order;
	unsigned int irq_no;
};

struct scp_ipi_irq scp_ipi_irqs[] = {
	/* SCP IPC0 */
	{ "mediatek,scp", 0, 0},
	/* SCP IPC1 */
	{ "mediatek,scp", 1, 0},
	/* MBOX_0 */
	{ "mediatek,scp", 2, 0},
	/* MBOX_1 */
	{ "mediatek,scp", 3, 0},
	/* MBOX_2 */
	{ "mediatek,scp", 4, 0},
	/* MBOX_3 */
	{ "mediatek,scp", 5, 0},
	/* MBOX_4 */
	{ "mediatek,scp", 6, 0},
};
#define IRQ_NUMBER  (sizeof(scp_ipi_irqs)/sizeof(struct scp_ipi_irq))

static int scp_ipi_syscore_dbg_suspend(void) { return 0; }
static void scp_ipi_syscore_dbg_resume(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < IRQ_NUMBER; i++) {
#ifdef CONFIG_MTK_GIC_V3_EXT
		ret = mt_irq_get_pending(scp_ipi_irqs[i].irq_no);
#endif
		if (ret) {
			if (i < 2)
				pr_info("[SCP] ipc%d wakeup\n", i);
			else
				mt_print_scp_ipi_id(i - 2);
			break;
		}
	}
}

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

void scp_schedule_reset_work(struct scp_work_struct *scp_ws)
{
	queue_work(scp_reset_workqueue, &scp_ws->work);
}


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


	if (scp_notify_flag) {
		scp_recovery_flag[SCP_A_ID] = SCP_A_RECOVERY_OK;

		writel(0xff, SCP_TO_SPM_REG); /* patch: clear SPM interrupt */
		mutex_lock(&scp_A_notify_mutex);

#if SCP_RECOVERY_SUPPORT
		atomic_set(&scp_reset_status, RESET_STATUS_STOP);
#endif
		scp_ready[SCP_A_ID] = 1;

#if SCP_DVFS_INIT_ENABLE
#ifdef ULPOSC_CALI_BY_AP
		sync_ulposc_cali_data_to_scp();
#endif
		/* release pll clock after scp ulposc calibration */
		scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

		pr_debug("[SCP] notify blocking call\n");
		blocking_notifier_call_chain(&scp_A_notifier_list
			, SCP_EVENT_READY, NULL);
		mutex_unlock(&scp_A_notify_mutex);
	}


	/*clear reset status and unlock wake lock*/
	pr_debug("[SCP] clear scp reset flag and unlock\n");
#ifndef CONFIG_FPGA_EARLY_PORTING
	scp_resource_req(SCP_REQ_RELEASE);
#endif  // CONFIG_FPGA_EARLY_PORTING
	/* register scp dvfs*/
	msleep(2000);
#if SCP_RECOVERY_SUPPORT
	__pm_relax(&scp_reset_lock);
#endif
	scp_register_feature(RTOS_FEATURE_ID);

}




#ifdef SCP_PARAMS_TO_SCP_SUPPORT
/*
 * Function/Space for kernel to pass static/initial parameters to scp's driver
 * @return: 0 for success, positive for info and negtive for error
 *
 * Note: The function should be called before disabling 26M & resetting scp.
 *
 * An example of function instance of sensor_params_to_scp:

	int sensor_params_to_scp(phys_addr_t addr_vir, size_t size)
	{
		int *params;

		params = (int *)addr_vir;
		params[0] = 0xaaaa;

		return 0;
	}
 */

static int params_to_scp(void)
{
#ifdef CFG_SENSOR_PARAMS_TO_SCP_SUPPORT
	int ret = 0;

	scp_region_info = (SCP_TCM + SCP_REGION_INFO_OFFSET);

	mt_reg_sync_writel(scp_get_reserve_mem_phys(SCP_DRV_PARAMS_MEM_ID),
			&(scp_region_info->ap_params_start));

	ret = sensor_params_to_scp(
		scp_get_reserve_mem_virt(SCP_DRV_PARAMS_MEM_ID),
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
#if SCP_BOOT_TIME_OUT_MONITOR
	del_timer(&scp_ready_timer[SCP_A_ID]);
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
#if SCP_RECOVERY_SUPPORT
	if (scp_timeout_times < 10)
		scp_send_reset_wq(RESET_TYPE_TIMEOUT);
#endif
	scp_timeout_times++;
	pr_notice("[SCP] scp_timeout_times=%x\n", scp_timeout_times);
}
#endif

/*
 * handle notification from scp
 * mark scp is ready for running tasks
 * It is important to call scp_ram_dump_init() in this IPI handler. This
 * timing is necessary to ensure that the region_info has been initialized.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static int scp_A_ready_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
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

	return 0;
}

/*
 * Handle notification from scp.
 * Report error from SCP to other kernel driver.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static void scp_err_info_handler(int id, void *prdata, void *data,
				 unsigned int len)
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

	if (report_hub_dmd)
		report_hub_dmd(info->case_id, info->sensor_id, info->context);
	else
		pr_debug("[SCP] warning: report_hub_dmd() not defined.\n");
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
	mutex_lock(&scp_A_notify_mutex);
	blocking_notifier_call_chain(&scp_A_notifier_list, SCP_EVENT_STOP,
		NULL);
	mutex_unlock(&scp_A_notify_mutex);
#if SCP_DVFS_INIT_ENABLE
	/* request pll clock before turn on scp */
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif

#if SCP_RECOVERY_SUPPORT
	if (reset & 0x0f) { /* do reset */
		/* make sure scp is in idle state */
		scp_reset_wait_timeout();
	}
#endif

	if (scp_enable[SCP_A_ID]) {
		/* write scp reserved memory address/size to GRP1/GRP2
		 * to let scp setup MPU
		 */
		writel((unsigned int)scp_mem_base_phys, DRAM_RESV_ADDR_REG);
		writel((unsigned int)scp_mem_size, DRAM_RESV_SIZE_REG);
		writel(1, R_CORE0_SW_RSTN_CLR);  /* release reset */
		dsb(SY); /* may take lot of time */
#if SCP_BOOT_TIME_OUT_MONITOR
		scp_ready_timer[SCP_A_ID].expires = jiffies + SCP_READY_TIMEOUT;
		add_timer(&scp_ready_timer[SCP_A_ID]);
#endif
	}
	pr_debug("[SCP] %s: done\n", __func__);
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
		retval = reset_scp(SCP_ALL_REBOOT);
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

DEVICE_ATTR(scp_A_status, 0444, scp_A_status_show, NULL);

static inline ssize_t scp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	scp_dump_last_regs();
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_status = %08x\n", c0_m.status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc = %08x\n", c0_m.pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr = %08x\n", c0_m.lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp = %08x\n", c0_m.sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc_latch = %08x\n", c0_m.pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr_latch = %08x\n", c0_m.lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp_latch = %08x\n", c0_m.sp_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc = %08x\n", c1_m.pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr = %08x\n", c1_m.lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp = %08x\n", c1_m.sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc_latch = %08x\n", c1_m.pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr_latch = %08x\n", c1_m.lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp_latch = %08x\n", c1_m.sp_latch);
	return len;
}

DEVICE_ATTR(scp_A_reg_status, 0444, scp_A_reg_status_show, NULL);

static inline ssize_t scp_A_db_test_trigger(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666) {
			scp_aed(RESET_TYPE_CMD, SCP_A_ID);
			if (scp_ready[SCP_A_ID])
				pr_debug("dumping SCP db\n");
			else
				pr_debug("SCP is not ready, try to dump EE\n");
		}
	}

	return count;
}

DEVICE_ATTR(scp_A_db_test, 0200, NULL, scp_A_db_test_trigger);

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
		scp_awake_lock((void *)SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR(scp_A_awake_lock, 0444, scp_A_awake_lock_show, NULL);

static inline ssize_t scp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_unlock((void *)SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR(scp_A_awake_unlock, 0444, scp_A_awake_unlock_show, NULL);

enum ipi_debug_opt {
	IPI_TRACKING_OFF,
	IPI_TRACKING_ON,
	IPIMON_SHOW,
};

static inline ssize_t scp_ipi_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	unsigned int value = 0x5A5A;
	int ret;

	if (scp_ready[SCP_A_ID]) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_TEST_0, 0, &value,
				   PIN_OUT_SIZE_TEST_0, 0);
		return scnprintf(buf, PAGE_SIZE
			, "SCP A ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

static inline ssize_t scp_ipi_debug(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int opt;

	if (kstrtouint(buf, 10, &opt) != 0)
		return -EINVAL;

	switch (opt) {
	case IPI_TRACKING_ON:
	case IPI_TRACKING_OFF:
		mtk_ipi_tracking(&scp_ipidev, opt);
		break;
	case IPIMON_SHOW:
		ipi_monitor_dump(&scp_ipidev);
		break;
	default:
		pr_info("cmd '%d' is not supported.\n", opt);
		break;
	}

	return n;
}

DEVICE_ATTR(scp_ipi_test, 0644, scp_ipi_test_show, scp_ipi_debug);

#endif

#if SCP_RECOVERY_SUPPORT
void scp_wdt_reset(int cpu_id)
{
	switch (cpu_id) {
	case 0:
		writel(V_INSTANT_WDT, R_CORE0_WDT_CFG);
		break;
	}
}
EXPORT_SYMBOL(scp_wdt_reset);

/*
 * trigger wdt manually (debug use)
 * Warning! watch dog may be refresh just after you set
 */
static ssize_t scp_wdt_trigger(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_debug("[SCP] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			scp_wdt_reset(0);
		else if (value == 667)
			scp_wdt_reset(1);
	}
	return count;
}

DEVICE_ATTR(wdt_reset, 0200, NULL, scp_wdt_trigger);

/*
 * trigger scp reset manually (debug use)
 */
static ssize_t scp_reset_trigger(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int magic, trigger, counts;

	if (sscanf(buf, "%d %d %d", &magic, &trigger, &counts) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", __func__, magic, trigger, counts);

	if (magic != 666)
		return -EINVAL;

	scp_reset_counts = counts;
	if (trigger == 1) {
		scp_reset_by_cmd = 1;
		scp_send_reset_wq(RESET_TYPE_CMD);
	}
	return n;
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
					, &dev_attr_recovery_flag);
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

#if SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)
#define SCP_MEM_RESERVED_KEY "mediatek,reserve-memory-scp_share"
int scp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	pr_notice("[SCP]%s %pa %pa\n", __func__, &rmem->base, &rmem->size);
	scp_mem_base_phys = (phys_addr_t) rmem->base;
	scp_mem_size = (phys_addr_t) rmem->size;

	return 0;
}

RESERVEDMEM_OF_DECLARE(scp_reserve_mem_init
			, SCP_MEM_RESERVED_KEY, scp_reserve_mem_of_init);
#endif  // SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)

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
	unsigned int num = (unsigned int)(sizeof(scp_reserve_mblock)
			/ sizeof(scp_reserve_mblock[0]));
	enum scp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;

	if ((scp_mem_base_phys >= (0x90000000ULL)) ||
			 (scp_mem_base_phys <= 0x0)) {
		/* The scp remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		pr_err("[SCP] Error: Wrong Address (0x%llx)\n",
			    (uint64_t)scp_mem_base_phys);
		//BUG_ON(1);
		return -1;
	}

	if (num != NUMS_MEM_ID) {
		pr_err("[SCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		//BUG_ON(1);
		return -1;
	}

	scp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(scp_mem_base_phys,
		scp_mem_size);
	pr_debug("[SCP] rsrv_phy_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_mem_base_phys, (uint64_t)scp_mem_size);
	pr_debug("[SCP] rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)scp_mem_base_virt, (uint64_t)scp_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		scp_reserve_mblock[id].start_phys = scp_mem_base_phys +
			accumlate_memory_size;
		scp_reserve_mblock[id].start_virt = scp_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += scp_reserve_mblock[id].size;
#ifdef DEBUG
		pr_debug("[SCP] [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)scp_reserve_mblock[id].start_phys,
			(uint64_t)scp_reserve_mblock[id].start_virt,
			(uint64_t)scp_reserve_mblock[id].size);
#endif  // DEBUG
	}
#ifdef CONFIG_MTK_ENG_BUILD
	WARN_ON(accumlate_memory_size > scp_mem_size);
#endif
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
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_REGION_ID_SCP_SMEM);
	mtk_emimpu_set_addr(&md_region, scp_mem_base_phys,
		scp_mem_base_phys + scp_mem_size - 1);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D0,
		MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D3,
		MTK_EMIMPU_NO_PROTECTION);
	if (mtk_emimpu_set_protection(&md_region))
		pr_notice("[SCP]mtk_emimpu_set_protection fail\n");
	mtk_emimpu_free_region(&md_region);
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

	mutex_unlock(&scp_feature_mutex);
}

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

/******************************************************************************
 *****************************************************************************/
void print_clk_registers(void)
{
	void __iomem *cfg = scpreg.cfg;
	void __iomem *clkctrl = scpreg.clkctrl;
	void __iomem *cfg_core0 = scpreg.cfg_core0;

	unsigned int offset;
	unsigned int value;

	// 0x24000 ~ 0x24160 (inclusive)
	for (offset = 0x0000; offset <= 0x0160; offset += 4) {
		value = (unsigned int)readl(cfg + offset);
		pr_notice("[SCP] cfg[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x21000 ~ 0x210120 (inclusive)
	for (offset = 0x0000; offset < 0x0120; offset += 4) {
		value = (unsigned int)readl(clkctrl + offset);
		pr_notice("[SCP] clk[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x30000 ~ 0x30114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core0 + offset);
		pr_notice("[SCP] cfg_core0[0x%04x]: 0x%08x\n", offset, value);
	}

}

void scp_reset_wait_timeout(void)
{
	uint32_t core0_halt = 0;
	/* make sure scp is in idle state */
	int timeout = 50; /* max wait 1s */

	while (timeout--) {
		core0_halt = readl(R_CORE0_STATUS) & B_CORE_HALT;
		if (core0_halt) {
			/* SCP stops any activities
			 * and parks at wfi
			 */
			break;
		}
		mdelay(20);
	}

	if (timeout == 0)
		pr_notice("[SCP] reset timeout, still reset scp\n");

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
	unsigned long spin_flags;

	pr_debug("[SCP] %s(): remain %d times\n", __func__, scp_reset_counts);
	/*notify scp functions stop*/
	pr_debug("[SCP] %s(): scp_extern_notify\n", __func__);
	scp_extern_notify(SCP_EVENT_STOP);
	/*
	 *   scp_ready:
	 *   SCP_PLATFORM_STOP  = 0,
	 *   SCP_PLATFORM_READY = 1,
	 */
	scp_ready[SCP_A_ID] = 0;

	/* wake lock AP*/
	__pm_stay_awake(&scp_reset_lock);
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* keep Univpll */
	scp_resource_req(SCP_REQ_26M);
#endif  // CONFIG_FPGA_EARLY_PORTING

	/* print_clk and scp_aed before pll enable to keep ori CLK_SEL */
	print_clk_registers();
	/*workqueue for scp ee, scp reset by cmd will not trigger scp ee*/
	if (scp_reset_by_cmd == 0) {
		pr_debug("[SCP] %s(): scp_aed_reset\n", __func__);
		scp_aed(scp_reset_type, SCP_A_ID);
	}
	pr_debug("[SCP] %s(): disable logger\n", __func__);
	/* logger disable must after scp_aed() */
	scp_logger_init_set(0);

	pr_debug("[SCP] %s(): scp_pll_ctrl_set\n", __func__);
	/*request pll clock before turn off scp */
#if SCP_DVFS_INIT_ENABLE
	scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif
	pr_notice("[SCP] %s(): scp_reset_type %d\n", __func__, scp_reset_type);
	/* scp reset by CMD, WDT or awake fail */
	if ((scp_reset_type == RESET_TYPE_TIMEOUT) ||
		(scp_reset_type == RESET_TYPE_AWAKE)) {
		/* stop scp */
		writel(1, R_CORE0_SW_RSTN_SET);
		dsb(SY); /* may take lot of time */
		pr_notice("[SCP] rstn core0 %x\n",
		readl(R_CORE0_SW_RSTN_SET));
	} else {
		/* reset type scp WDT or CMD*/
		/* make sure scp is in idle state */
		scp_reset_wait_timeout();
		writel(1, R_CORE0_SW_RSTN_SET);
		writel(CORE_REBOOT_OK, SCP_GPR_CORE0_REBOOT);
		dsb(SY); /* may take lot of time */
		pr_notice("[SCP] rstn core0 %x\n",
		readl(R_CORE0_SW_RSTN_SET));
	}

	/* scp reset */
	scp_sys_full_reset();

#ifdef SCP_PARAMS_TO_SCP_SUPPORT
	/* The function, sending parameters to scp must be anchored before
	 * 1. disabling 26M, 2. resetting SCP
	 */
	if (params_to_scp() != 0)
		return;
#endif

	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	scp_reset_awake_counts();
	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	/* Setup dram reserved address and size for scp*/
	writel((unsigned int)scp_mem_base_phys, DRAM_RESV_ADDR_REG);
	writel((unsigned int)scp_mem_size, DRAM_RESV_SIZE_REG);
	/* start scp */
	pr_notice("[SCP] start scp\n");
	writel(1, R_CORE0_SW_RSTN_CLR);
	pr_notice("[SCP] rstn core0 %x\n", readl(R_CORE0_SW_RSTN_CLR));
	dsb(SY); /* may take lot of time */
#if SCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&scp_ready_timer[SCP_A_ID], jiffies + SCP_READY_TIMEOUT);
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
	if (scp_reset_counts > 0) {
		scp_reset_counts--;
		scp_schedule_reset_work(&scp_sys_reset_work);
	}
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

void scp_region_info_init(void)
{
	/*get scp loader/firmware info from scp sram*/
	scp_region_info = (SCP_TCM + SCP_REGION_INFO_OFFSET);
	pr_debug("[SCP] scp_region_info = %px\n", scp_region_info);
	memcpy_from_scp(&scp_region_info_copy,
		scp_region_info, sizeof(scp_region_info_copy));
}

void scp_recovery_init(void)
{
#if SCP_RECOVERY_SUPPORT
	/*create wq for scp reset*/
	scp_reset_workqueue = create_singlethread_workqueue("SCP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&scp_sys_reset_work.work, scp_sys_reset_ws);

	scp_loader_virt = ioremap_wc(
		scp_region_info_copy.ap_loader_start,
		scp_region_info_copy.ap_loader_size);
	pr_debug("[SCP] loader image mem: virt:0x%llx - 0x%llx\n",
		(uint64_t)(phys_addr_t)scp_loader_virt,
		(uint64_t)(phys_addr_t)scp_loader_virt +
		(phys_addr_t)scp_region_info_copy.ap_loader_size);
	/*init wake,
	 *this is for prevent scp pll cpu clock disabled during reset flow
	 */
	wakeup_source_init(&scp_reset_lock, "scp reset wakelock");
	/* init reset by cmd flag */
	scp_reset_by_cmd = 0;

	scp_regdump_virt = ioremap_wc(
			scp_region_info_copy.regdump_start,
			scp_region_info_copy.regdump_size);
	pr_debug("[SCP] scp_regdump_virt map: 0x%x + 0x%x\n",
		scp_region_info_copy.regdump_start,
		scp_region_info_copy.regdump_size);

	if ((int)(scp_region_info_copy.ap_dram_size) > 0) {
		/*if l1c enable, map it (include backup) */
		scp_ap_dram_virt = ioremap_wc(
		scp_region_info_copy.ap_dram_start,
		ROUNDUP(scp_region_info_copy.ap_dram_size, 1024)*4);

	pr_debug("[SCP] scp_ap_dram_virt map: 0x%x + 0x%x\n",
		scp_region_info_copy.ap_dram_start,
		scp_region_info_copy.ap_dram_size);
	}
#endif
}

static int scp_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.sram)) {
		pr_err("[SCP] scpreg.sram error\n");
		return -1;
	}
	scpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[SCP] sram base = 0x%px %x\n"
		, scpreg.sram, scpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	scpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg)) {
		pr_err("[SCP] scpreg.cfg error\n");
		return -1;
	}
	pr_debug("[SCP] cfg base = 0x%px\n", scpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	scpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.clkctrl)) {
		pr_err("[SCP] scpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[SCP] clkctrl base = 0x%px\n", scpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	scpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core0)) {
		pr_debug("[SCP] scpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_core0 base = 0x%p\n", scpreg.cfg_core0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	scpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core1)) {
		pr_debug("[SCP] scpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_core1 base = 0x%p\n", scpreg.cfg_core1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	scpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.bus_tracker)) {
		pr_debug("[SCP] scpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[SCP] bus_tracker base = 0x%p\n", scpreg.bus_tracker);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 6);
	scpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.l1cctrl)) {
		pr_debug("[SCP] scpreg.l1cctrl error\n");
		return -1;
	}
	pr_debug("[SCP] l1cctrl base = 0x%p\n", scpreg.l1cctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 7);
	scpreg.cfg_sec = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_sec)) {
		pr_debug("[SCP] scpreg.cfg_sec error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_sec base = 0x%p\n", scpreg.cfg_sec);


	of_property_read_u32(pdev->dev.of_node, "scp_sramSize"
						, &scpreg.scp_tcmsize);
	if (!scpreg.scp_tcmsize) {
		pr_err("[SCP] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[SCP] scpreg.scp_tcmsize = %d\n", scpreg.scp_tcmsize);

	/* scp core 0 */
	of_property_read_string(pdev->dev.of_node, "core_0", &core_status);
	if (strcmp(core_status, "enable") != 0)
		pr_err("[SCP] core_0 not enable\n");
	else {
		pr_debug("[SCP] core_0 enable\n");
		scp_enable[SCP_A_ID] = 1;
	}
	scpreg.irq = platform_get_irq_byname(pdev, "ipc0");
	ret = request_irq(scpreg.irq, scp_A_irq_handler,
		IRQF_TRIGGER_NONE, "SCP IPC0", NULL);
	if (ret) {
		pr_err("[SCP]ipc0 require irq fail %d %d\n", scpreg.irq, ret);
		//goto err;
	}
	pr_debug("ipc0 %d\n", scpreg.irq);
	scpreg.irq = platform_get_irq_byname(pdev, "ipc1");
	ret = request_irq(scpreg.irq, scp_A_irq_handler,
		IRQF_TRIGGER_NONE, "SCP IPC1", NULL);
	if (ret) {
		pr_err("[SCP]ipc1 require irq fail %d %d\n", scpreg.irq, ret);
		//goto err;
	}
	pr_debug("ipc1 %d\n", scpreg.irq);
	/* create mbox dev */
	pr_debug("[SCP] mbox mbox probe\n");
	for (i = 0; i < SCP_MBOX_TOTAL; i++) {
		scp_mbox_info[i].mbdev = &scp_mboxdev;
		mtk_mbox_probe(pdev, scp_mbox_info[i].mbdev, i);
		mbox_setup_pin_table(i);
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (scp_ipi_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
					      scp_ipi_irqs[i].name);
		if (!node) {
			pr_info("[SCP] find '%s' node failed\n",
				scp_ipi_irqs[i].name);
			continue;
		}
		scp_ipi_irqs[i].irq_no =
			irq_of_parse_and_map(node, scp_ipi_irqs[i].order);
		if (!scp_ipi_irqs[i].irq_no)
			pr_info("[SCP] get '%s' fail\n", scp_ipi_irqs[i].name);
	}

	ret = mtk_ipi_device_register(&scp_ipidev, pdev, &scp_mboxdev,
				      SCP_IPI_COUNT);
	if (ret)
		pr_err("[SCP] ipi_dev_register fail, ret %d\n", ret);

#if SCP_DVFS_INIT_ENABLE && defined(ULPOSC_CALI_BY_AP)
	ulposc_cali_init();
#endif

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
	pr_debug("[SCP] scpreg.scpsys = %px\n", scpreg.scpsys);
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
	{ .compatible = "mediatek,scp_infra", },
	{}
};

static struct platform_driver mtk_scpsys_device = {
	.probe = scpsys_device_probe,
	.remove = scpsys_device_remove,
	.driver = {
		.name = "scp_infra",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = scpsys_of_ids,
#endif
	},
};

static struct syscore_ops scp_ipi_dbg_syscore_ops = {
	.suspend = scp_ipi_syscore_dbg_suspend,
	.resume = scp_ipi_syscore_dbg_resume,
};

/*
 * driver initialization entry point
 */
static int __init scp_init(void)
{
	int ret = 0;
	int i = 0;
#if SCP_BOOT_TIME_OUT_MONITOR
	init_timer(&scp_ready_timer[SCP_A_ID]);
	scp_ready_timer[SCP_A_ID].function = &scp_wait_ready_timeout;
	scp_ready_timer[SCP_A_ID].data = (unsigned long) SCP_A_TIMER;
#endif
    /* scp platform initialise */
	pr_debug("[SCP2] %s begins\n", __func__);

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
	/* keep Univpll */
	scp_resource_req(SCP_REQ_26M);
#endif  // CONFIG_FPGA_EARLY_PORTING

#if SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)
	/* make sure the reserved memory for scp is ready */
	if (scp_mem_size == 0) {
		pr_err("[SCP]Reserving memory by of_device for SCP failed.\n");
		return -1;
	}
#endif  // SCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)

	if (platform_driver_register(&mtk_scp_device))
		pr_err("[SCP] scp probe fail\n");

	if (platform_driver_register(&mtk_scpsys_device))
		pr_err("[SCP] scpsys probe fail\n");

	/* skip initial if dts status = "disable" */
	if (!scp_enable[SCP_A_ID]) {
		pr_err("[SCP] scp disabled!!\n");
		goto err;
	}
	/* scp platform initialise */
	scp_region_info_init();
	pr_debug("[SCP] platform init\n");
	scp_awake_init();
	scp_workqueue = create_singlethread_workqueue("SCP_WQ");
	ret = scp_excep_init();
	if (ret) {
		pr_err("[SCP]Excep Init Fail\n");
		goto err;
	}

	INIT_WORK(&scp_A_notify_work.work, scp_A_notify_ws);

	scp_legacy_ipi_init();

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_0,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready0);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_1,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready1);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_0,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info0);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_1,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info1);

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
	scp_logger_workqueue = create_singlethread_workqueue("SCP_LOG_WQ");
	if (scp_logger_init(scp_get_reserve_mem_virt(SCP_A_LOGGER_MEM_ID),
			scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID)) == -1) {
		pr_err("[SCP] scp_logger_init_fail\n");
		goto err;
	}
#endif

	scp_recovery_init();

#ifdef SCP_PARAMS_TO_SCP_SUPPORT
	/* The function, sending parameters to scp must be anchored before
	 * 1. disabling 26M, 2. resetting SCP
	 */
	if (params_to_scp() != 0)
		goto err;
#endif

#if SCP_DVFS_INIT_ENABLE
	/* remember to release pll */
	scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

	register_syscore_ops(&scp_ipi_dbg_syscore_ops);

	driver_init_done = true;
	reset_scp(SCP_ALL_ENABLE);

#if SCP_DVFS_INIT_ENABLE
	/* set default VCORE request if SCP DVFS feature is OFF */
	if (scp_dvfs_flag != 1)
		scp_vcore_request(CLK_OPP0);
#endif

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
}

static int __init scp_late_init(void)
{
	pr_notice("[SCP] %s\n", __func__);
#if ENABLE_SCP_EMI_PROTECTION
	set_scp_mpu();
#endif
	return 0;
}

module_init(scp_init);
module_exit(scp_exit);
late_initcall(scp_late_init);

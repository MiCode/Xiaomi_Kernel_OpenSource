// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "scp_feature_define.h"
#include "scp_err_info.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_dvfs.h"
#include "scp_scpctl.h"
#include "scp.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#include "scp_reservedmem_define.h"
#endif

#if ENABLE_SCP_EMI_PROTECTION
#include "memory/mediatek/emi.h"
#endif

/* scp mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include "scp_ipi.h"

#include "scp_hwvoter_dbg.h"

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
unsigned int last_scp_expected_freq;
unsigned int scp_expected_freq;
unsigned int scp_current_freq;
unsigned int scp_dvfs_cali_ready;

/*scp awake variable*/
int scp_awake_counts[SCP_CORE_TOTAL];
/* debug flag for Dump timeout*/
unsigned int debug_dumptimeout_flag;

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
struct wakeup_source *scp_reset_lock;

DEFINE_SPINLOCK(scp_reset_spinlock);

/* l1c enable */
void __iomem *scp_ap_dram_virt;
void __iomem *scp_loader_virt;
void __iomem *scp_regdump_virt;


phys_addr_t scp_mem_base_phys;
phys_addr_t scp_mem_base_virt;
phys_addr_t scp_mem_size;
struct scp_regs scpreg;
bool scp_hwvoter_support = true;

unsigned char *scp_send_buff[SCP_CORE_TOTAL];
unsigned char *scp_recv_buff[SCP_CORE_TOTAL];

static struct workqueue_struct *scp_workqueue;

static struct workqueue_struct *scp_reset_workqueue;

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

static int scp_ipi_dbg_resume_noirq(struct device *dev)
{
	int i = 0;
	int ret = 0;
	bool state = false;

	for (i = 0; i < IRQ_NUMBER; i++) {
		ret = irq_get_irqchip_state(scp_ipi_irqs[i].irq_no,
			IRQCHIP_STATE_PENDING, &state);
		if (!ret && state) {
			if (i < 2)
				pr_info("[SCP] ipc%d wakeup\n", i);
			else
				mt_print_scp_ipi_id(i - 2);
			break;
		}
	}

	return 0;
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
 *         0 :get sema timeout
 *        -1 :get sema fail, driver not ready
 */
int get_scp_semaphore(int flag)
{
	int read_back;
	unsigned int cnt;
	int ret = SEMAPHORE_FAIL;
	unsigned long spin_flags;

	/* return -1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return SEMAPHORE_NOT_INIT;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		cnt = SEMAPHORE_TIMEOUT;
		writel((1 << flag), SCP_SEMAPHORE);

		while (cnt-- > 0) {
			/* repeat test if we get semaphore */
			read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = SEMAPHORE_SUCCESS;
				break;
			}
			writel((1 << flag), SCP_SEMAPHORE);
		}

		if (ret == SEMAPHORE_FAIL)
			pr_notice("[SCP] get scp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_notice("[SCP] already hold scp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(get_scp_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *         0 :release sema fail
 *        -1 :release sema fail, driver not ready
 */
int release_scp_semaphore(int flag)
{
	int read_back;
	int ret = SEMAPHORE_FAIL;
	unsigned long spin_flags;

	/* return -1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return SEMAPHORE_NOT_INIT;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), SCP_SEMAPHORE);
		read_back = (readl(SCP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = SEMAPHORE_SUCCESS;
		else
			pr_notice("[SCP] release scp sema. %d failed\n", flag);
	} else {
		pr_notice("[SCP] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(release_scp_semaphore);

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return 1: get sema success
 *        0: get sema timeout
 *       -1: get sema fail, driver not ready
 */
int scp_get_semaphore_3way(int flag)
{
	int ret = SEMAPHORE_FAIL;
	unsigned int cnt;
	unsigned long spin_flags;
	unsigned int read_back;

	/* return -1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return SEMAPHORE_NOT_INIT;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);

	flag = flag * 4 + 2;

	read_back = (readl(SCP_3WAY_SEMAPHORE) >> flag) & 0x1;
	if (read_back == 0) {
		cnt = SEMAPHORE_3WAY_TIMEOUT;

		while (cnt-- > 0) {
			writel((1 << flag), SCP_3WAY_SEMAPHORE);

			read_back = (readl(SCP_3WAY_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = SEMAPHORE_SUCCESS;
				break;
			}

		}
		if (ret == SEMAPHORE_FAIL)
			pr_notice("[SCP] get scp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_notice("[SCP] already hold scp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(scp_get_semaphore_3way);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return 1: release sema success
 *        0: release sema fail, sem busy
 *       -1: release sema fail, driver not ready
 */
int scp_release_semaphore_3way(int flag)
{
	int ret = SEMAPHORE_FAIL;
	unsigned long spin_flags;
	unsigned int read_back;

	/* return -1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return SEMAPHORE_NOT_INIT;

	/* spinlock context safe*/
	spin_lock_irqsave(&scp_awake_spinlock, spin_flags);

	flag = flag * 4 + 2;

	read_back = (readl(SCP_3WAY_SEMAPHORE) >> flag) & 0x1;
	if (read_back == 1) {
		writel((1 << flag), SCP_3WAY_SEMAPHORE);
		read_back = (readl(SCP_3WAY_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = SEMAPHORE_SUCCESS;
		else
			pr_notice("[SCP] release scp sema. %d failed\n", flag);
	} else {
		pr_notice("[SCP] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&scp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(scp_release_semaphore_3way);


static BLOCKING_NOTIFIER_HEAD(scp_A_notifier_list);
static struct notifier_block register_notify_pending;
static struct notifier_block *register_curr = &register_notify_pending;
static struct notifier_block unregister_notify_pending;
static struct notifier_block *unregister_curr = &unregister_notify_pending;
static DEFINE_SPINLOCK(notify_register_spinlock);
static DEFINE_SPINLOCK(notify_unregister_spinlock);
static atomic_t scp_A_notifier_status = ATOMIC_INIT(SCP_EVENT_STOP);

/*
 * scp_A_register_notify_pending && scp_A_unregister_notify_pending
 * is a mechanism to avoid user register and block when notifying chain
 * is called, and those function only call after the notifiy done,
 * should not call when notifying, or it will cause deadlock.
 */
static void scp_A_register_notify_pending(void)
{
	struct notifier_block *node = &register_notify_pending;
	struct notifier_block *nb = NULL;

	spin_lock(&notify_register_spinlock);
	while (node->next) {
		nb = node->next;
		node->next = node->next->next;
		spin_unlock(&notify_register_spinlock);
		/* should not call blocking API in atomic context */
		scp_A_register_notify(nb);
		spin_lock(&notify_register_spinlock);
	}
	register_curr = &register_notify_pending;
	spin_unlock(&notify_register_spinlock);
}

static void scp_A_unregister_notify_pending(void)
{
	struct notifier_block *node = &unregister_notify_pending;
	struct notifier_block *nb = NULL;

	spin_lock(&notify_unregister_spinlock);
	while (node->next) {
		nb = node->next;
		node->next = node->next->next;
		spin_unlock(&notify_unregister_spinlock);
		/* should not call blocking API in atomic context */
		scp_A_unregister_notify(nb);
		spin_lock(&notify_unregister_spinlock);
	}
	unregister_curr = &unregister_notify_pending;
	spin_unlock(&notify_unregister_spinlock);
}
/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void scp_A_register_notify(struct notifier_block *nb)
{
	pr_debug("%s start\n", __func__);
	switch (atomic_read(&scp_A_notifier_status)) {
	/*
	 * if user register nb after SCP ready event, should notify
	 * user the ready event, too.
	 */
	case SCP_EVENT_READY:
		nb->notifier_call(nb, SCP_EVENT_READY, NULL);
		blocking_notifier_chain_register(&scp_A_notifier_list, nb);
		pr_debug("%s callback finished\n", __func__);
		break;
	case SCP_EVENT_STOP:
		blocking_notifier_chain_register(&scp_A_notifier_list, nb);
		pr_debug("%s register finished\n", __func__);
		break;
	case SCP_EVENT_NOTIFYING:
		spin_lock(&notify_register_spinlock);
		register_curr->next = nb;
		register_curr = register_curr->next;
		spin_unlock(&notify_register_spinlock);
		pr_debug("%s pending finished\n", __func__);
		break;
	}
	pr_debug("%s end\n", __func__);
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
	switch (atomic_read(&scp_A_notifier_status)) {
	case SCP_EVENT_STOP:
	case SCP_EVENT_READY:
		blocking_notifier_chain_unregister(&scp_A_notifier_list, nb);
		break;
	case SCP_EVENT_NOTIFYING:
		spin_lock(&notify_unregister_spinlock);
		unregister_curr->next = nb;
		unregister_curr = unregister_curr->next;
		spin_unlock(&notify_unregister_spinlock);
		break;
	}
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

		scp_ready[SCP_A_ID] = 1;

		if (scp_dvfs_feature_enable()) {
			sync_ulposc_cali_data_to_scp();

			/* release pll clock after scp ulposc calibration */
			scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
		}

		scp_dvfs_cali_ready = 1;
		pr_debug("[SCP] notify blocking call\n");
		scp_extern_notify(SCP_EVENT_READY);
	}


	/*clear reset status and unlock wake lock*/
	pr_debug("[SCP] clear scp reset flag and unlock\n");

	/* request pll clock before turn on scp */
	if (scp_dvfs_feature_enable())
		scp_resource_req(SCP_REQ_RELEASE);

	/* register scp dvfs*/
	msleep(2000);
	__pm_relax(scp_reset_lock);
	scp_register_feature(RTOS_FEATURE_ID);
#if SCP_RECOVERY_SUPPORT
	if (atomic_read(&scp_reset_status) == RESET_STATUS_START_WDT) {
		pr_notice("[SCP] recovery fail, recovery again\n");
		atomic_set(&scp_reset_status, RESET_STATUS_START);
		scp_send_reset_wq(RESET_TYPE_WDT);
	} else {
		pr_notice("[SCP] recovery success\n");
		atomic_set(&scp_reset_status, RESET_STATUS_STOP);
	}
#endif
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
	del_timer(&scp_ready_timer[SCP_A_ID].tl);
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
#if SCP_RECOVERY_SUPPORT
	if (scp_timeout_times < 10)
		scp_send_reset_wq(RESET_TYPE_TIMEOUT);
	else
		__pm_relax(scp_reset_lock);

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
		pr_notice("[SCP]image size ERROR! AP=0x%x,SCP=0x%x\n",
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
 * apps to stop their tasks if needed
 * generate error if reset fail
 * NOTE: this function may be blocked
 *       and should not be called in interrupt context
 * @param reset:    bit[0-3]=0 for scp enable, =1 for reboot
 *                  bit[4-7]=0 for All, =1 for scp_A, =2 for scp_B
 * @return:         0 if success
 */
int reset_scp(int reset)
{
	scp_extern_notify(SCP_EVENT_STOP);

	if (scp_dvfs_feature_enable())
		/* request pll clock before turn on scp */
		scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);

	if (reset & 0x0f) { /* do reset */
		/* make sure scp is in idle state */
		scp_reset_wait_timeout();
	}
	if (scp_enable[SCP_A_ID]) {
		/* write scp reserved memory address/size to GRP1/GRP2
		 * to let scp setup MPU
		 */
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
		if (scpreg.secure_dump) {
			scp_do_rstn_clr();
		} else {
#else
		{
#endif
		writel((unsigned int)scp_mem_base_phys, DRAM_RESV_ADDR_REG);
		writel((unsigned int)scp_mem_size, DRAM_RESV_SIZE_REG);
		writel(1, R_CORE0_SW_RSTN_CLR);  /* release reset */
		dsb(SY); /* may take lot of time */
		}
#if SCP_BOOT_TIME_OUT_MONITOR
		scp_ready_timer[SCP_A_ID].tl.expires = jiffies + SCP_READY_TIMEOUT;
		add_timer(&scp_ready_timer[SCP_A_ID].tl);
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

DEVICE_ATTR_RO(scp_A_status);

static inline ssize_t scp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	scp_dump_last_regs();
	scp_show_last_regs();
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_status = %08x\n", c0_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_pc = %08x\n", c0_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_lr = %08x\n", c0_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_sp = %08x\n", c0_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_pc_latch = %08x\n", c0_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_lr_latch = %08x\n", c0_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h0_sp_latch = %08x\n", c0_m->sp_latch);
	if (!scpreg.twohart)
		goto core1;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_pc = %08x\n", c0_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_lr = %08x\n", c0_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_sp = %08x\n", c0_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_pc_latch = %08x\n", c0_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_lr_latch = %08x\n", c0_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0h1_sp_latch = %08x\n", c0_t1_m->sp_latch);
core1:
	if (scpreg.core_nums == 1)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_status = %08x\n", c1_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_pc = %08x\n", c1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_lr = %08x\n", c1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_sp = %08x\n", c1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_pc_latch = %08x\n", c1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_lr_latch = %08x\n", c1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h0_sp_latch = %08x\n", c1_m->sp_latch);
	if (!scpreg.twohart)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_pc = %08x\n", c1_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_lr = %08x\n", c1_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_sp = %08x\n", c1_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_pc_latch = %08x\n", c1_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_lr_latch = %08x\n", c1_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1h1_sp_latch = %08x\n", c1_t1_m->sp_latch);

end:
	return len;
}

DEVICE_ATTR_RO(scp_A_reg_status);

static inline ssize_t scp_A_db_test_store(struct device *kobj
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

DEVICE_ATTR_WO(scp_A_db_test);

#ifdef SCP_DEBUG_NODE_ENABLE
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
		scp_awake_lock((void *)SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_awake_lock);

static inline ssize_t scp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (scp_ready[SCP_A_ID]) {
		scp_awake_unlock((void *)SCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "SCP A awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "SCP A is not ready\n");
}

DEVICE_ATTR_RO(scp_A_awake_unlock);

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

static inline ssize_t scp_ipi_test_store(struct device *kobj
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

DEVICE_ATTR_RW(scp_ipi_test);

#endif

#if SCP_RECOVERY_SUPPORT
void scp_wdt_reset(int cpu_id)
{
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		switch (cpu_id) {
		case 0:
			scp_do_wdt_set(0);
			break;
		case 1:
			scp_do_wdt_set(1);
			break;
		}
	} else {
#else
	{
#endif
	switch (cpu_id) {
	case 0:
		writel(V_INSTANT_WDT, R_CORE0_WDT_CFG);
		break;
	case 1:
		writel(V_INSTANT_WDT, R_CORE1_WDT_CFG);
		break;
	}
	}
}
EXPORT_SYMBOL(scp_wdt_reset);

/*
 * trigger wdt manually (debug use)
 * Warning! watch dog may be refresh just after you set
 */
static ssize_t wdt_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;
	pr_notice("[SCP] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			scp_wdt_reset(0);
		else if (value == 667)
			scp_wdt_reset(1);
	}
	return count;
}

DEVICE_ATTR_WO(wdt_reset);

/*
 * trigger scp reset manually (debug use)
 */
static ssize_t scp_reset_store(struct device *dev
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

DEVICE_ATTR_WO(scp_reset);
/*
 * trigger wdt manually
 * debug use
 */

static ssize_t recovery_flag_show(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", scp_recovery_flag[SCP_A_ID]);
}
static ssize_t recovery_flag_store(struct device *dev
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

DEVICE_ATTR_RW(recovery_flag);

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
		pr_notice("[SCP] misc register failed\n");
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

#ifdef SCP_DEBUG_NODE_ENABLE
	ret = device_create_file(scp_device.this_device
					, &dev_attr_scp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;
#endif  // SCP_DEBUG_NODE_ENABLE

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

#ifdef SCP_DEBUG_NODE_ENABLE
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
#endif  // SCP_DEBUG_NODE_ENABLE

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

	ret = device_create_file(scp_device.this_device,
					&dev_attr_log_filter);
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
		pr_notice("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_phys);

phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_notice("[SCP] no reserve memory for %d", id);
		return 0;
	} else
		return scp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(scp_get_reserve_mem_virt);

phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_notice("[SCP] no reserve memory for %d", id);
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
		pr_notice("[SCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		BUG_ON(1);
		return -1;
	}
	/* Get reserved memory */
	ret = of_property_read_string(pdev->dev.of_node, "scp_mem_key",
			&mem_key);
	if (ret) {
		pr_info("[SCP] cannot find property\n");
		return -EINVAL;
	}

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
		pr_notice("[SCP] Error: Wrong Address (0x%llx)\n",
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
		pr_notice("[SCP] scp_mem_tbl not found\n");
		scp_mem_num = 0;
	}

	for (i = 0; i < scp_mem_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"scp_mem_tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			pr_notice("Cannot get memory index(%d)\n", i);
			return -1;
		}

		if (i == 0 && scpreg.secure_dump) {
			/* secure_dump */
			ret = of_property_read_u32(pdev->dev.of_node,
					"secure_dump_size",
					&m_size);
		} else {
			ret = of_property_read_u32_index(pdev->dev.of_node,
					"scp_mem_tbl",
					(i * MEMORY_TBL_ELEM_NUM) + 1,
					&m_size);
		}
		if (ret) {
			pr_notice("Cannot get memory size(%d)\n", i);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			pr_notice("[SCP] skip unexpected index, %d\n", m_idx);
			continue;
		}

		scp_reserve_mblock[m_idx].size = m_size;
		pr_notice("@@@@ reserved: <%d  %d>\n", m_idx, m_size);
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
#ifdef SCP_DEBUG_NODE_ENABLE
	BUG_ON(accumlate_memory_size > scp_mem_size);
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
#if IS_ENABLED(CONFIG_MTK_EMI) || IS_ENABLED(CONFIG_MEDIATEK_EMI)
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
#endif
}
#endif

void scp_register_feature(enum feature_id id)
{
	int ret = 0;

	/*prevent from access when scp is down*/
	if (!scp_ready[SCP_A_ID]) {
		pr_debug("[SCP] %s: not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	/* prevent from access when scp dvfs cali isn't done */
	if (!scp_dvfs_cali_ready) {
		pr_debug("[SCP] %s: dvfs cali not ready, scp_dvfs_cali=%u\n",
		__func__, scp_dvfs_cali_ready);
		return;
	}

	if (id < 0 || id >= NUM_FEATURE_ID) {
		pr_notice("[SCP] %s, invalid feature id:%u, max id:%u\n",
			__func__, id, NUM_FEATURE_ID - 1);
		return;
	}
	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&scp_feature_mutex);

	feature_table[id].enable = 1;
	if (scp_dvfs_feature_enable())
		scp_expected_freq = scp_get_freq();

	scp_current_freq = readl(CURRENT_FREQ_REG);
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	/* if secure_dump is enabled, expected_freq is sent in scp_request_freq() */
	if (!scpreg.secure_dump) {
#else
	{
#endif
	writel(scp_expected_freq, EXPECTED_FREQ_REG);
	}

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			if (scp_dvfs_feature_enable()) {
				/* set scp freq. */
				ret = scp_request_freq();
				if (ret == -1) {
					pr_notice("[SCP]%s request_freq fail\n", __func__);
					WARN_ON(1);
				}
			}
		}
	} else {
		pr_notice("[SCP]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

	mutex_unlock(&scp_feature_mutex);
}
EXPORT_SYMBOL_GPL(scp_register_feature);

void scp_deregister_feature(enum feature_id id)
{
	int ret = 0;

	/* prevent from access when scp is down */
	if (!scp_ready[SCP_A_ID]) {
		pr_notice("[SCP] %s:not ready, scp=%u\n", __func__,
			scp_ready[SCP_A_ID]);
		return;
	}

	/* prevent from access when scp dvfs cali isn't done */
	if (!scp_dvfs_cali_ready) {
		pr_notice("[SCP] %s: dvfs cali not ready, scp_dvfs_cali=%u\n",
			__func__, scp_dvfs_cali_ready);
		return;
	}

	if (id < 0 || id >= NUM_FEATURE_ID) {
		pr_notice("[SCP] %s, invalid feature id:%u, max id:%u\n",
			__func__, id, NUM_FEATURE_ID - 1);
		return;
	}
	mutex_lock(&scp_feature_mutex);

	feature_table[id].enable = 0;

	if (scp_dvfs_feature_enable())
		scp_expected_freq = scp_get_freq();

	scp_current_freq = readl(CURRENT_FREQ_REG);
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	/* if secure_dump is enabled, expected_freq is sent in scp_request_freq() */
	if (!scpreg.secure_dump) {
#else
	{
#endif
	writel(scp_expected_freq, EXPECTED_FREQ_REG);
	}

	/* send request only when scp is not down */
	if (scp_ready[SCP_A_ID]) {
		if (scp_current_freq != scp_expected_freq) {
			/* set scp freq. */
			if (scp_dvfs_feature_enable()) {
				ret = scp_request_freq();
				if (ret == -1) {
					pr_notice("[SCP] %s: req_freq fail\n", __func__);
					WARN_ON(1);
				}
			}
		}
	} else {
		pr_notice("[SCP]Not send SCP DVFS request because SCP is down\n");
		WARN_ON(1);
	}

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
	/* avoid re-entry */
	mutex_lock(&scp_A_notify_mutex);
	atomic_set(&scp_A_notifier_status, SCP_EVENT_NOTIFYING);
	blocking_notifier_call_chain(&scp_A_notifier_list, notify_status, NULL);
	atomic_set(&scp_A_notifier_status, notify_status);
	/*
	 * if there is some user (un)register when notifier is notifying,
	 * should (un)register them after the notify action is finish.
	 */
	if (unlikely(register_curr != &register_notify_pending))
		scp_A_register_notify_pending();
	if (unlikely(unregister_curr != &unregister_notify_pending))
		scp_A_unregister_notify_pending();
	mutex_unlock(&scp_A_notify_mutex);
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
 * return value: scp reset original status
 */
unsigned int scp_set_reset_status(void)
{
	unsigned long spin_flags;
	unsigned int res;

	spin_lock_irqsave(&scp_reset_spinlock, spin_flags);
	switch (res = atomic_read(&scp_reset_status)) {
	case RESET_STATUS_STOP:
		atomic_set(&scp_reset_status, RESET_STATUS_START);
		break;
	/*
	 * do nothing because scp bus hang may trigger reset
	 * by awake reset and then scp trigger wdt.
	 */
	case RESET_STATUS_START:
		break;
	/*
	 * after kick scp and wdt happen mean reboot fail, we
	 * need to reset again after recovery finish.
	 */
	case RESET_STATUS_START_KICK:
		atomic_set(&scp_reset_status, RESET_STATUS_START_WDT);
		break;
	/*
	 * do nothing because when status is wdt, the status should
	 * be changed by workqueue when recovery finish.
	 */
	case RESET_STATUS_START_WDT:
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&scp_reset_spinlock, spin_flags);
	return res;
}

/******************************************************************************
 *****************************************************************************/
void print_clk_registers(void)
{
	void __iomem *cfg = scpreg.cfg;
	void __iomem *clkctrl = scpreg.clkctrl;
	void __iomem *cfg_core0 = scpreg.cfg_core0;
	void __iomem *cfg_core1 = scpreg.cfg_core1;

	unsigned int offset;
	unsigned int value;

	// 0x24000 ~ 0x24160 (inclusive)
	for (offset = 0x0000; offset <= 0x0160; offset += 4) {
		value = (unsigned int)readl(cfg + offset);
		pr_notice("[SCP] cfg[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x21000 ~ 0x210144 (inclusive)
	for (offset = 0x0000; offset <= 0x0144; offset += 4) {
		value = (unsigned int)readl(clkctrl + offset);
		pr_notice("[SCP] clk[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x30000 ~ 0x30114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core0 + offset);
		pr_notice("[SCP] cfg_core0[0x%04x]: 0x%08x\n", offset, value);
	}
	if (debug_dumptimeout_flag == 1) {
	// 0x31000 ~ 0x31428 DMA register
		for (offset = 0x0000; offset <= 0x0428; offset += 4) {
			value = (unsigned int)readl(cfg_core0 + 0x1000 + offset);
			pr_notice("[SCP] cfg_core0_dma[0x%04x]: 0x%08x\n", offset, value);
		}
	}
	if (scpreg.core_nums == 1)
		return;
	// 0x40000 ~ 0x40114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core1 + offset);
		pr_notice("[SCP] cfg_core1[0x%04x]: 0x%08x\n", offset, value);
	}

}

void scp_reset_wait_timeout(void)
{
	uint32_t core0_halt = 0;
	uint32_t core1_halt = 0;
	/* make sure scp is in idle state */
	int timeout = 50; /* max wait 1s */

	while (timeout--) {
		core0_halt = readl(R_CORE0_STATUS) & B_CORE_HALT;
		core1_halt = scpreg.core_nums == 2? readl(R_CORE1_STATUS) & B_CORE_HALT: 1;
		if (core0_halt && core1_halt) {
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

static void wait_scp_ready_to_reboot(void)
{
	int retry = 0;
	unsigned long c0, c1;

	/* clr after SCP side INT trigger,
	 * or SCP may lost INT max wait = 200ms
	 */
	for (retry = 200; retry > 0; retry--) {
		c0 = readl(SCP_GPR_CORE0_REBOOT);
		c1 = scpreg.core_nums == 2 ? readl(SCP_GPR_CORE1_REBOOT) :
			CORE_RDY_TO_REBOOT;

		if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT))
			break;
		usleep_range(1000, 1100);
	}

	if (retry == 0)
		pr_notice("[SCP] SCP don't stay in wfi c0:%lx c1:%lx\n", c0, c1);
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
	if (scp_reset_type == RESET_TYPE_WDT)
		scp_show_last_regs();
	wait_scp_ready_to_reboot();
	/*
	 *   scp_ready:
	 *   SCP_PLATFORM_STOP  = 0,
	 *   SCP_PLATFORM_READY = 1,
	 */
	scp_ready[SCP_A_ID] = 0;
	scp_dvfs_cali_ready = 0;

	/* wake lock AP*/
	__pm_stay_awake(scp_reset_lock);

	if (scp_dvfs_feature_enable())
		scp_resource_req(SCP_REQ_26M);

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

	if (scp_dvfs_feature_enable()) {
		pr_debug("[SCP] %s(): scp_pll_ctrl_set\n", __func__);
		scp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
	}

	pr_notice("[SCP] %s(): scp_reset_type %d\n", __func__, scp_reset_type);
	/* scp reset by CMD, WDT or awake fail */
#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		if ((scp_reset_type == RESET_TYPE_TIMEOUT) ||
			(scp_reset_type == RESET_TYPE_AWAKE)) {
			scp_do_rstn_set(0);
			pr_notice("[SCP] rstn core0 %x core1 %x\n",
			readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
		} else {
			scp_do_rstn_set(1); /* write CORE_REBOOT_OK to SCP_GPR_CORE0_REBOOT */
			pr_notice("[SCP] rstn core0 %x core1 %x\n",
			readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
		}
	} else {
#else
	{
#endif
	if ((scp_reset_type == RESET_TYPE_TIMEOUT) ||
		(scp_reset_type == RESET_TYPE_AWAKE)) {
		/* stop scp */
		writel(1, R_CORE0_SW_RSTN_SET);
		writel(1, R_CORE1_SW_RSTN_SET);
		dsb(SY); /* may take lot of time */
		pr_notice("[SCP] rstn core0 %x core1 %x\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
	} else {
		/* reset type scp WDT or CMD*/
		/* make sure scp is in idle state */
		scp_reset_wait_timeout();
		writel(1, R_CORE0_SW_RSTN_SET);
		writel(1, R_CORE1_SW_RSTN_SET);
		writel(CORE_REBOOT_OK, SCP_GPR_CORE0_REBOOT);
		writel(CORE_REBOOT_OK, SCP_GPR_CORE1_REBOOT);
		dsb(SY); /* may take lot of time */
		pr_notice("[SCP] rstn core0 %x core1 %x\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
	}
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

#if SCP_RESERVED_MEM && IS_ENABLED(CONFIG_OF_RESERVED_MEM)
	if (scpreg.secure_dump) {
		pr_notice("[SCP] start scp\n");
		/* Setup dram reserved address and size for scp*/
		/* start scp */
		scp_do_rstn_clr();
		pr_notice("[SCP] rstn core0 %x\n", readl(R_CORE0_SW_RSTN_CLR));
	} else {
#else
	{
#endif
	/* Setup dram reserved address and size for scp*/
	writel((unsigned int)scp_mem_base_phys, DRAM_RESV_ADDR_REG);
	writel((unsigned int)scp_mem_size, DRAM_RESV_SIZE_REG);
	/* start scp */
	pr_notice("[SCP] start scp\n");
	writel(1, R_CORE0_SW_RSTN_CLR);
	pr_notice("[SCP] rstn core0 %x\n", readl(R_CORE0_SW_RSTN_CLR));
	dsb(SY); /* may take lot of time */
	}
	atomic_set(&scp_reset_status, RESET_STATUS_START_KICK);
#if SCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&scp_ready_timer[SCP_A_ID].tl, jiffies + SCP_READY_TIMEOUT);
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
	return scp_resource_status;
}

#if SCP_RECOVERY_SUPPORT
void scp_region_info_init(void)
{
	/*get scp loader/firmware info from scp sram*/
	scp_region_info = (SCP_TCM + SCP_REGION_INFO_OFFSET);
	pr_debug("[SCP] scp_region_info = %px\n", scp_region_info);
	memcpy_from_scp(&scp_region_info_copy,
		scp_region_info, sizeof(scp_region_info_copy));
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
	scp_reset_lock = wakeup_source_register(NULL, "scp reset wakelock");
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

	pr_notice("[SCP] scp_ap_dram_virt map: 0x%x + 0x%x\n",
		scp_region_info_copy.ap_dram_start,
		scp_region_info_copy.ap_dram_size);
	}
#endif
}

static int scp_feature_table_probe(struct platform_device *pdev)
{
	enum {
		feaure_tbl_item_size = 3
	};
	int i, ret, feature_num, feature_id, frequency, core_id;

	feature_num = of_property_count_u32_elems(
			pdev->dev.of_node, "scp_feature_tbl")
			/ 3;
	if (feature_num <= 0) {
		pr_notice("[SCP] scp_feature_tbl not found\n");
		return -1;
	}

	for (i = 0; i < feature_num; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"scp_feature_tbl",
			i * feaure_tbl_item_size,
			&feature_id);

		if (ret) {
			pr_notice("[SCP] %s: can't get feature id(%d):line %d\n",
				__func__, i, __LINE__);
			return -1;
		}

		/* because feature_table data member is bit-field */
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"scp_feature_tbl",
			i * feaure_tbl_item_size + 1,
			&frequency);

		if (ret) {
			pr_notice("[SCP] %s: can't get frequency(%d):%d\n",
				__func__, i, __LINE__);
			return -1;
		}
		feature_table[feature_id].freq = frequency;

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"scp_feature_tbl",
			i * feaure_tbl_item_size + 2,
			&core_id);

		if (ret) {
			pr_notice("[SCP] %s: can't get core_id(%d):%d\n",
				__func__, i, __LINE__);
			return -1;
		}
		feature_table[feature_id].sys_id = core_id;
	}
	return 0;
}

static bool scp_ipi_table_init(struct mtk_mbox_device *scp_mboxdev, struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i, ret, mbox_id, recv_opt;
	of_property_read_u32(pdev->dev.of_node, "mbox_count"
						, &scp_mboxdev->count);
	if (!scp_mboxdev->count) {
		pr_notice("[SCP] mbox count not found\n");
		return false;
	}

	scp_mboxdev->send_count = of_property_count_u32_elems(
				pdev->dev.of_node, "send_table")
				/ send_item_num;
	if (scp_mboxdev->send_count <= 0) {
		pr_notice("[SCP] scp send table not found\n");
		return false;
	}

	scp_mboxdev->recv_count = of_property_count_u32_elems(
				pdev->dev.of_node, "recv_table")
				/ recv_item_num;
	if (scp_mboxdev->recv_count <= 0) {
		pr_notice("[SCP] scp recv table not found\n");
		return false;
	}
	/* alloc and init scp_mbox_info */
	scp_mboxdev->info_table = vzalloc(sizeof(struct mtk_mbox_info) * scp_mboxdev->count);
	if (!scp_mboxdev->info_table) {
		pr_notice("[SCP]%s: vmlloc info table fail:%d\n", __func__, __LINE__);
		return false;
	}
	scp_mbox_info = scp_mboxdev->info_table;
	for (i = 0; i < scp_mboxdev->count; ++i) {
		scp_mbox_info[i].id = i;
		scp_mbox_info[i].slot = 64;
		scp_mbox_info[i].enable = 1;
		scp_mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	scp_mboxdev->pin_send_table = vzalloc(sizeof(struct mtk_mbox_pin_send) * scp_mboxdev->send_count);
	if (!scp_mboxdev->pin_send_table) {
		pr_notice("[SCP]%s: vmlloc send table fail:%d\n", __func__, __LINE__);
		return false;
	}
	scp_mbox_pin_send = scp_mboxdev->pin_send_table;
	for (i = 0; i < scp_mboxdev->send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num,
				&scp_mbox_pin_send[i].chan_id);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[SCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_send[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 2,
				&scp_mbox_pin_send[i].msg_size);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
	}
	/* alloc and init recv table */
	scp_mboxdev->pin_recv_table = vzalloc(sizeof(struct mtk_mbox_pin_recv) * scp_mboxdev->recv_count);
	if (!scp_mboxdev->pin_recv_table) {
		pr_notice("[SCP]%s: vmlloc recv table fail:%d\n", __func__, __LINE__);
		return false;
	}
	scp_mbox_pin_recv = scp_mboxdev->pin_recv_table;
	for (i = 0; i < scp_mboxdev->recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num,
				&scp_mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[SCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_recv[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 2,
				&scp_mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			pr_notice("[SCP]%s:Cannot get recv opt (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		scp_mbox_pin_recv[i].recv_opt = recv_opt;
	}


	/* wrapper_ipi_init */
	if (!of_get_property(pdev->dev.of_node, "legacy_table", NULL)) {
		pr_notice("[SCP]%s: wrapper_ipi don't exist\n", __func__);
		return true;
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 0, &scp_ipi_legacy_id[0].out_id_0);
	if (ret) {
		pr_notice("[SCP]%s:Cannot get out_id_0\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 1, &scp_ipi_legacy_id[0].out_id_1);
	if (ret) {
		pr_notice("[SCP]%s:Cannot get out_id_1\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 2, &scp_ipi_legacy_id[0].in_id_0);
	if (ret) {
		pr_notice("[SCP]%s:Cannot get in_id_0\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 3, &scp_ipi_legacy_id[0].in_id_1);
	if (ret) {
		pr_notice("[SCP]%s:Cannot get in_id_1\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 4, &scp_ipi_legacy_id[0].out_size);
	if (ret) {
		pr_notice("[%s]:Cannot get out_size\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 5, &scp_ipi_legacy_id[0].in_size);
	if (ret) {
		pr_notice("[SCP]%s:Cannot get in_size\n", __func__);
	}
	scp_ipi_legacy_id[0].msg_0 = vzalloc(scp_ipi_legacy_id[0].in_size * MBOX_SLOT_SIZE);
	if (!scp_ipi_legacy_id[0].msg_0) {
		pr_notice("[SCP]%s: vmlloc legacy msg_0 fail\n", __func__);
		return false;
	}
	scp_ipi_legacy_id[0].msg_1 = vzalloc(scp_ipi_legacy_id[0].in_size * MBOX_SLOT_SIZE);
	if (!scp_ipi_legacy_id[0].msg_1) {
		pr_notice("[SCP]%s: vmlloc legacy msg_1 fail\n", __func__);
		return false;
	}
	return true;
}

static int scp_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct resource *res;
	const char *core_status = NULL;
	const char *scp_hwvoter = NULL;
	const char *secure_dump = NULL;
	const char *debug_dumptimeout = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.sram)) {
		pr_notice("[SCP] scpreg.sram error\n");
		return -1;
	}
	scpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[SCP] sram base = 0x%px %x\n"
		, scpreg.sram, scpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	scp_reg_base_phy = res->start & 0xfff00000;
	pr_notice("[SCP] scp_reg_base_phy = 0x%x\n", scp_reg_base_phy);
	scpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg)) {
		pr_notice("[SCP] scpreg.cfg error\n");
		return -1;
	}
	pr_debug("[SCP] cfg base = 0x%px\n", scpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	scpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.clkctrl)) {
		pr_notice("[SCP] scpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[SCP] clkctrl base = 0x%px\n", scpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	scpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core0)) {
		pr_debug("[SCP] scpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_core0 base = 0x%px\n", scpreg.cfg_core0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	scpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_core1)) {
		pr_debug("[SCP] scpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_core1 base = 0x%px\n", scpreg.cfg_core1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	scpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.bus_tracker)) {
		pr_debug("[SCP] scpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[SCP] bus_tracker base = 0x%px\n", scpreg.bus_tracker);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 6);
	scpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.l1cctrl)) {
		pr_debug("[SCP] scpreg.l1cctrl error\n");
		return -1;
	}
	pr_debug("[SCP] l1cctrl base = 0x%px\n", scpreg.l1cctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 7);
	scpreg.cfg_sec = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) scpreg.cfg_sec)) {
		pr_debug("[SCP] scpreg.cfg_sec error\n");
		return -1;
	}
	pr_debug("[SCP] cfg_sec base = 0x%px\n", scpreg.cfg_sec);


	of_property_read_u32(pdev->dev.of_node, "scp_sramSize"
						, &scpreg.scp_tcmsize);
	if (!scpreg.scp_tcmsize) {
		pr_notice("[SCP] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[SCP] scpreg.scp_tcmsize = %d\n", scpreg.scp_tcmsize);

	/* scp core 0 */
	if (of_property_read_string(pdev->dev.of_node, "core_0", &core_status))
		return -1;

	if (strcmp(core_status, "enable") != 0)
		pr_notice("[SCP] core_0 not enable\n");
	else {
		pr_debug("[SCP] core_0 enable\n");
		scp_enable[SCP_A_ID] = 1;
	}

	of_property_read_string(pdev->dev.of_node, "scp_hwvoter", &scp_hwvoter);
	if (scp_hwvoter) {
		if (strcmp(scp_hwvoter, "enable") != 0) {
			pr_notice("[SCP] scp_hwvoter not enable\n");
			scp_hwvoter_support = false;
		} else {
			pr_notice("[SCP] scp_hwvoter enable\n");
			scp_hwvoter_support = true;
		}
	} else {
		scp_hwvoter_support = false;
		pr_notice("[SCP] scp_hwvoter not support: %d\n", scp_hwvoter_support);
	}

	of_property_read_u32(pdev->dev.of_node, "core_nums"
						, &scpreg.core_nums);
	if (!scpreg.core_nums) {
		pr_notice("[SCP] core number not found\n");
		return -ENODEV;
	}
	pr_notice("[SCP] scpreg.core_nums = %d\n", scpreg.core_nums);

	of_property_read_u32(pdev->dev.of_node, "twohart"
						, &scpreg.twohart);
	pr_notice("[SCP] scpreg.twohart = %d\n", scpreg.twohart);

	/* secure_dump */
	scpreg.secure_dump = 0;
	if (!of_property_read_string(pdev->dev.of_node, "secure_dump", &secure_dump)) {
		if (!strncmp(secure_dump, "enable", strlen("enable"))) {
			pr_notice("[SCP] secure dump enabled\n");
			scpreg.secure_dump = 1;
		}
	}

	debug_dumptimeout_flag = 0;
	if (!of_property_read_string(pdev->dev.of_node, "debug_dumptimeout", &debug_dumptimeout)) {
		if (!strncmp(debug_dumptimeout, "enable", strlen("enable"))) {
			pr_notice("[SCP] debug dump timeout enabled\n");
			debug_dumptimeout_flag = 1;
		}
	}
	scpreg.irq0 = platform_get_irq_byname(pdev, "ipc0");
	if (scpreg.irq0 < 0)
		pr_notice("[SCP] get ipc0 irq failed\n");
	else {
		pr_debug("ipc0 %d\n", scpreg.irq0);
		ret = request_irq(scpreg.irq0, scp_A_irq_handler,
			IRQF_TRIGGER_NONE, "SCP IPC0", NULL);
		if (ret < 0)
			pr_notice("[SCP]ipc0 require fail %d %d\n",
				scpreg.irq0, ret);
		else {
			scp_A_irq0_tasklet.data = scpreg.irq0;
			ret = enable_irq_wake(scpreg.irq0);
			if (ret < 0)
				pr_notice("[SCP] ipc0 wake fail:%d,%d\n",
					scpreg.irq0, ret);
		}
	}

	scpreg.irq1 = platform_get_irq_byname(pdev, "ipc1");
	if (scpreg.irq1 < 0)
		pr_notice("[SCP] get ipc1 irq failed\n");
	else {
		pr_debug("ipc1 %d\n", scpreg.irq1);
		ret = request_irq(scpreg.irq1, scp_A_irq_handler,
			IRQF_TRIGGER_NONE, "SCP IPC1", NULL);
		if (ret < 0)
			pr_notice("[SCP]ipc1 require irq fail %d %d\n",
				scpreg.irq1, ret);
		else {
			scp_A_irq1_tasklet.data = scpreg.irq1;
			ret = enable_irq_wake(scpreg.irq1);
			if (ret < 0)
				pr_notice("[SCP] irq wake fail:%d,%d\n",
					scpreg.irq1, ret);
		}
	}

	/* probe mbox info from dts */
	if (!scp_ipi_table_init(&scp_mboxdev, pdev))
		return -ENODEV;
	/* create mbox dev */
	pr_debug("[SCP] mbox probe\n");
	for (i = 0; i < scp_mboxdev.count; i++) {
		scp_mbox_info[i].mbdev = &scp_mboxdev;
		ret = mtk_mbox_probe(pdev, scp_mbox_info[i].mbdev, i);
		if (ret < 0 || scp_mboxdev.info_table[i].irq_num < 0) {
			pr_notice("[SCP] mbox%d probe fail\n", i, ret);
			continue;
		}

		ret = enable_irq_wake(scp_mboxdev.info_table[i].irq_num);
		if (ret < 0) {
			pr_notice("[SCP]mbox%d enable irq fail\n", i, ret);
			continue;
		}
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
		pr_notice("[SCP] ipi_dev_register fail, ret %d\n", ret);

#if SCP_RESERVED_MEM && defined(CONFIG_OF)

	/*scp resvered memory*/
	pr_notice("[SCP] scp_reserve_memory_ioremap\n");
	ret = scp_reserve_memory_ioremap(pdev);
	if (ret) {
		pr_notice("[SCP]scp_reserve_memory_ioremap failed\n");
		return ret;
	}
#endif

	/* scp feature table probe */
	ret = scp_feature_table_probe(pdev);
	if (ret) {
		pr_notice("[SCP] feature_table_probe failed\n");
		return ret;
	}
	/* scp memorydump size probe */
	ret = memorydump_size_probe(pdev);
	if (ret)
		return ret;

	return ret;
}

static int scp_device_remove(struct platform_device *dev)
{
	if (scp_mbox_info) {
		kfree(scp_mbox_info);
		scp_mbox_info = NULL;
	}
	if (scp_mbox_pin_recv) {
		kfree(scp_mbox_pin_recv);
		scp_mbox_pin_recv = NULL;
	}
	if (scp_mbox_pin_send) {
		kfree(scp_mbox_pin_send);
		scp_mbox_pin_send = NULL;
	}

	return 0;
}

static int scpsys_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scpreg.scpsys = devm_ioremap_resource(dev, res);
	//pr_notice("[SCP] scpreg.scpsys = %x\n", scpreg.scpsys);
	if (IS_ERR((void const *) scpreg.scpsys)) {
		pr_notice("[SCP] scpreg.sram error\n");
		return -1;
	}
	return ret;
}

static int scpsys_device_remove(struct platform_device *dev)
{
	return 0;
}

static const struct dev_pm_ops scp_ipi_dbg_pm_ops = {
	.resume_noirq = scp_ipi_dbg_resume_noirq,
};

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
		.pm = &scp_ipi_dbg_pm_ops,
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
	pr_debug("[SCP2] %s begins\n", __func__);

	/* scp ready static flag initialise */
	for (i = 0; i < SCP_CORE_TOTAL ; i++) {
		scp_enable[i] = 0;
		scp_ready[i] = 0;
	}
	scp_dvfs_cali_ready = 0;

	scp_dvfs_init();
	wait_scp_dvfs_init_done();

	if (platform_driver_register(&mtk_scp_device))
		pr_notice("[SCP] scp probe fail\n");

	if (platform_driver_register(&mtk_scpsys_device))
		pr_notice("[SCP] scpsys probe fail\n");

	/* skip initial if dts status = "disable" */
	if (!scp_enable[SCP_A_ID]) {
		pr_notice("[SCP] scp disabled!!\n");
		goto err;
	}
	/* scp platform initialise */
	scp_region_info_init();
	pr_debug("[SCP] platform init\n");
	scp_awake_init();
	scp_workqueue = create_singlethread_workqueue("SCP_WQ");
	ret = scp_excep_init();
	if (ret) {
		pr_notice("[SCP]Excep Init Fail\n");
		goto err;
	}

	INIT_WORK(&scp_A_notify_work.work, scp_A_notify_ws);

	if (mbox_check_recv_table(IPI_IN_SCP_MPOOL_0))
		scp_legacy_ipi_init();
	else
		pr_info("Skip legacy ipi init\n");

	if (mbox_check_recv_table(IPI_IN_SCP_READY_0))
		mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_0,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready0);
	else
		pr_info("Dosen't support IPI_IN_SCP_READY_0");

	if (mbox_check_recv_table(IPI_IN_SCP_READY_1))
		mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_READY_1,
			(void *)scp_A_ready_ipi_handler, NULL, &msg_scp_ready1);
	else
		pr_info("Dosen't support IPI_IN_SCP_READY_1");

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_0,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info0);

	mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_ERROR_INFO_1,
			(void *)scp_err_info_handler, NULL, msg_scp_err_info1);

	ret = register_pm_notifier(&scp_pm_notifier_block);
	if (ret)
		pr_notice("[SCP] failed to register PM notifier %d\n", ret);

	/* scp sysfs initialise */
	pr_debug("[SCP] sysfs init\n");
	ret = create_files();
	if (unlikely(ret != 0)) {
		pr_notice("[SCP] create files failed\n");
		goto err;
	}

	/* scp hwvoter debug init */
	if (scp_hwvoter_support)
		scp_hw_voter_dbg_init();

#if SCP_LOGGER_ENABLE
	/* scp logger initialise */
	pr_debug("[SCP] logger init\n");
	/*create wq for scp logger*/
	scp_logger_workqueue = create_singlethread_workqueue("SCP_LOG_WQ");
	if (scp_logger_init(scp_get_reserve_mem_virt(SCP_A_LOGGER_MEM_ID),
			scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID)) == -1) {
		pr_notice("[SCP] scp_logger_init_fail\n");
		goto err;
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
		goto err;
#endif

	if (scp_dvfs_feature_enable())
		/* remember to release pll */
		scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);

	driver_init_done = true;
	reset_scp(SCP_ALL_ENABLE);

	if (scp_dvfs_feature_enable())
		scp_init_vcore_request();

	return ret;
err:
	if (scp_dvfs_feature_enable())
		/* remember to release pll */
		scp_pll_ctrl_set(PLL_DISABLE, CLK_26M);

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

	scp_dvfs_exit();

#if SCP_LOGGER_ENABLE
	scp_logger_uninit();
#endif

	free_irq(scpreg.irq0, NULL);
	free_irq(scpreg.irq1, NULL);
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
		del_timer(&scp_ready_timer[i].tl);
#endif
}

device_initcall_sync(scp_init);
module_exit(scp_exit);

MODULE_DESCRIPTION("MEDIATEK Module SCP driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");

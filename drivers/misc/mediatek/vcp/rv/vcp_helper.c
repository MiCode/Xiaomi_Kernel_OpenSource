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
#include "vcp_feature_define.h"
#include "vcp_err_info.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_dvfs.h"
#include "vcp_vcpctl.h"
#include "vcp.h"

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#include <linux/of_reserved_mem.h>
#include "vcp_reservedmem_define.h"
#endif

#if ENABLE_VCP_EMI_PROTECTION
#include "soc/mediatek/emi.h"
#endif

/* vcp mbox/ipi related */
#include <linux/soc/mediatek/mtk-mbox.h>
#include "vcp_ipi.h"

/* vcp semaphore timeout count definition */
#define SEMAPHORE_TIMEOUT 5000
#define SEMAPHORE_3WAY_TIMEOUT 5000
/* vcp ready timeout definition */
#define VCP_READY_TIMEOUT (3 * HZ) /* 30 seconds*/
#define VCP_A_TIMER 0

/* vcp ipi message buffer */
uint32_t msg_vcp_ready0, msg_vcp_ready1;
char msg_vcp_err_info0[40], msg_vcp_err_info1[40];

/* vcp ready status for notify*/
unsigned int vcp_ready[VCP_CORE_TOTAL];

/* vcp enable status*/
unsigned int vcp_enable[VCP_CORE_TOTAL];

/* vcp dvfs variable*/
unsigned int vcp_expected_freq;
unsigned int vcp_current_freq;
unsigned int vcp_dvfs_cali_ready;
unsigned int vcp_support;

/*vcp awake variable*/
int vcp_awake_counts[VCP_CORE_TOTAL];


unsigned int vcp_recovery_flag[VCP_CORE_TOTAL];
#define VCP_A_RECOVERY_OK	0x44
/*  vcp_reset_status
 *  0: vcp not in reset status
 *  1: vcp in reset status
 */
atomic_t vcp_reset_status = ATOMIC_INIT(RESET_STATUS_STOP);
unsigned int vcp_reset_by_cmd;
struct vcp_region_info_st *vcp_region_info;
/* shadow it due to sram may not access during sleep */
struct vcp_region_info_st vcp_region_info_copy;

struct vcp_work_struct vcp_sys_reset_work;
struct wakeup_source *vcp_reset_lock;

DEFINE_SPINLOCK(vcp_reset_spinlock);

/* l1c enable */
void __iomem *vcp_ap_dram_virt;
void __iomem *vcp_loader_virt;
void __iomem *vcp_regdump_virt;


phys_addr_t vcp_mem_base_phys;
phys_addr_t vcp_mem_base_virt;
phys_addr_t vcp_mem_size;
struct vcp_regs vcpreg;

unsigned char *vcp_send_buff[VCP_CORE_TOTAL];
unsigned char *vcp_recv_buff[VCP_CORE_TOTAL];

static struct workqueue_struct *vcp_workqueue;

static struct workqueue_struct *vcp_reset_workqueue;

#if VCP_LOGGER_ENABLE
static struct workqueue_struct *vcp_logger_workqueue;
#endif
#if VCP_BOOT_TIME_OUT_MONITOR
struct vcp_timer {
	struct timer_list tl;
	int tid;
};
static struct vcp_timer vcp_ready_timer[VCP_CORE_TOTAL];
#endif
static struct vcp_work_struct vcp_A_notify_work;

static unsigned int vcp_timeout_times;

static DEFINE_MUTEX(vcp_A_notify_mutex);
static DEFINE_MUTEX(vcp_feature_mutex);
static DEFINE_MUTEX(vcp_register_sensor_mutex);

char *core_ids[VCP_CORE_TOTAL] = {"VCP A"};
DEFINE_SPINLOCK(vcp_awake_spinlock);
/* set flag after driver initial done */
static bool driver_init_done;
struct vcp_ipi_irq {
	const char *name;
	int order;
	unsigned int irq_no;
};

struct vcp_ipi_irq vcp_ipi_irqs[] = {
	/* VCP IPC0 */
	{ "mediatek,vcp", 0, 0},
	/* VCP IPC1 */
	{ "mediatek,vcp", 1, 0},
	/* MBOX_0 */
	{ "mediatek,vcp", 2, 0},
	/* MBOX_1 */
	{ "mediatek,vcp", 3, 0},
	/* MBOX_2 */
	{ "mediatek,vcp", 4, 0},
	/* MBOX_3 */
	{ "mediatek,vcp", 5, 0},
	/* MBOX_4 */
	{ "mediatek,vcp", 6, 0},
};
#define IRQ_NUMBER  (sizeof(vcp_ipi_irqs)/sizeof(struct vcp_ipi_irq))

#define NUM_IO_DOMAINS 4
struct device *vcp_io_devs[NUM_IO_DOMAINS];

#undef pr_notice
#define pr_notice pr_info
#undef pr_debug
#define pr_debug pr_info

static int vcp_ipi_dbg_resume_noirq(struct device *dev)
{
	int i = 0;
	int ret = 0;
	bool state = false;

	for (i = 0; i < IRQ_NUMBER; i++) {
		ret = irq_get_irqchip_state(vcp_ipi_irqs[i].irq_no,
			IRQCHIP_STATE_PENDING, &state);
		if (!ret && state) {
			if (i < 2)
				pr_info("[VCP] ipc%d wakeup\n", i);
			else
				mt_print_vcp_ipi_id(i - 2);
			break;
		}
	}

	return 0;
}

/*
 * memory copy to vcp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_to_vcp(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}


/*
 * memory copy from vcp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_from_vcp(void *trg, const void __iomem *src, int size)
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
int get_vcp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), VCP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), VCP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[VCP] get vcp sema. %d TIMEOUT...!\n", flag);
	} else {
		pr_notice("[VCP] already hold vcp sema. %d\n", flag);
	}

	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(get_vcp_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_vcp_semaphore(int flag)
{
	int read_back;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), VCP_SEMAPHORE);
		read_back = (readl(VCP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[VCP] release vcp sema. %d failed\n", flag);
	} else {
		pr_notice("[VCP] try to release sema. %d not own by me\n", flag);
	}

	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(release_vcp_semaphore);


static BLOCKING_NOTIFIER_HEAD(vcp_A_notifier_list);
/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void vcp_A_register_notify(struct notifier_block *nb)
{
	mutex_lock(&vcp_A_notify_mutex);
	blocking_notifier_chain_register(&vcp_A_notifier_list, nb);

	pr_debug("[VCP] register vcp A notify callback..\n");

	if (is_vcp_ready(VCP_A_ID))
		nb->notifier_call(nb, VCP_EVENT_READY, NULL);
	mutex_unlock(&vcp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(vcp_A_register_notify);


/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void vcp_A_unregister_notify(struct notifier_block *nb)
{
	mutex_lock(&vcp_A_notify_mutex);
	blocking_notifier_chain_unregister(&vcp_A_notifier_list, nb);
	mutex_unlock(&vcp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(vcp_A_unregister_notify);


void vcp_schedule_work(struct vcp_work_struct *vcp_ws)
{
	queue_work(vcp_workqueue, &vcp_ws->work);
}

void vcp_schedule_reset_work(struct vcp_work_struct *vcp_ws)
{
	queue_work(vcp_reset_workqueue, &vcp_ws->work);
}


#if VCP_LOGGER_ENABLE
void vcp_schedule_logger_work(struct vcp_work_struct *vcp_ws)
{
	queue_work(vcp_logger_workqueue, &vcp_ws->work);
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
static void vcp_A_notify_ws(struct work_struct *ws)
{
	struct vcp_work_struct *sws =
		container_of(ws, struct vcp_work_struct, work);
	unsigned int vcp_notify_flag = sws->flags;


	if (vcp_notify_flag) {
		vcp_recovery_flag[VCP_A_ID] = VCP_A_RECOVERY_OK;

		writel(0xff, VCP_TO_SPM_REG); /* patch: clear SPM interrupt */
		mutex_lock(&vcp_A_notify_mutex);

#if VCP_RECOVERY_SUPPORT
		atomic_set(&vcp_reset_status, RESET_STATUS_STOP);
#endif
		vcp_ready[VCP_A_ID] = 1;

#if VCP_DVFS_INIT_ENABLE
		sync_ulposc_cali_data_to_vcp();
		/* release pll clock after vcp ulposc calibration */
		vcp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

		vcp_dvfs_cali_ready = 1;
		pr_debug("[VCP] notify blocking call\n");
		blocking_notifier_call_chain(&vcp_A_notifier_list
			, VCP_EVENT_READY, NULL);
		mutex_unlock(&vcp_A_notify_mutex);
	}


	/*clear reset status and unlock wake lock*/
	pr_debug("[VCP] clear vcp reset flag and unlock\n");

#if VCP_DVFS_INIT_ENABLE
	vcp_resource_req(VCP_REQ_RELEASE);
#endif
	/* register vcp dvfs*/
	msleep(2000);
	__pm_relax(vcp_reset_lock);
	vcp_register_feature(RTOS_FEATURE_ID);

}




#ifdef VCP_PARAMS_TO_VCP_SUPPORT
/*
 * Function/Space for kernel to pass static/initial parameters to vcp's driver
 * @return: 0 for success, positive for info and negtive for error
 *
 * Note: The function should be called before disabling 26M & resetting vcp.
 *
 * An example of function instance of sensor_params_to_vcp:

	int sensor_params_to_vcp(phys_addr_t addr_vir, size_t size)
	{
		int *params;

		params = (int *)addr_vir;
		params[0] = 0xaaaa;

		return 0;
	}
 */

static int params_to_vcp(void)
{
#ifdef CFG_SENSOR_PARAMS_TO_VCP_SUPPORT
	int ret = 0;

	vcp_region_info = (VCP_TCM + VCP_REGION_INFO_OFFSET);

	mt_reg_sync_writel(vcp_get_reserve_mem_phys(VCP_DRV_PARAMS_MEM_ID),
			&(vcp_region_info->ap_params_start));

	ret = sensor_params_to_vcp(
		vcp_get_reserve_mem_virt(VCP_DRV_PARAMS_MEM_ID),
		vcp_get_reserve_mem_size(VCP_DRV_PARAMS_MEM_ID));

	return ret;
#else
	/* return success, if sensor_params_to_vcp is not defined */
	return 0;
#endif
}
#endif

/*
 * mark notify flag to 1 to notify apps to start their tasks
 */
static void vcp_A_set_ready(void)
{
	pr_debug("[VCP] %s()\n", __func__);
#if VCP_BOOT_TIME_OUT_MONITOR
	del_timer(&vcp_ready_timer[VCP_A_ID].tl);
#endif
	vcp_A_notify_work.flags = 1;
	vcp_schedule_work(&vcp_A_notify_work);
}

/*
 * callback for reset timer
 * mark notify flag to 0 to generate an exception
 * @param data: unuse
 */
#if VCP_BOOT_TIME_OUT_MONITOR
static void vcp_wait_ready_timeout(struct timer_list *t)
{
#if VCP_RECOVERY_SUPPORT
	if (vcp_timeout_times < 10)
		vcp_send_reset_wq(RESET_TYPE_TIMEOUT);
#endif
	vcp_timeout_times++;
	pr_notice("[VCP] vcp_timeout_times=%x\n", vcp_timeout_times);
}
#endif

/*
 * handle notification from vcp
 * mark vcp is ready for running tasks
 * It is important to call vcp_ram_dump_init() in this IPI handler. This
 * timing is necessary to ensure that the region_info has been initialized.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static int vcp_A_ready_ipi_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	unsigned int vcp_image_size = *(unsigned int *)data;

	if (!vcp_ready[VCP_A_ID])
		vcp_A_set_ready();

	/*verify vcp image size*/
	if (vcp_image_size != VCP_A_TCM_SIZE) {
		pr_notice("[VCP]image size ERROR! AP=0x%x,VCP=0x%x\n",
					VCP_A_TCM_SIZE, vcp_image_size);
		WARN_ON(1);
	}

	pr_debug("[VCP] ramdump init\n");
	vcp_ram_dump_init();

	return 0;
}

/*
 * Handle notification from vcp.
 * Report error from VCP to other kernel driver.
 * @param id:   ipi id
 * @param prdata: ipi handler parameter
 * @param data: ipi data
 * @param len:  length of ipi data
 */
static void vcp_err_info_handler(int id, void *prdata, void *data,
				 unsigned int len)
{
	struct error_info *info = (struct error_info *)data;

	if (sizeof(*info) != len) {
		pr_notice("[VCP] error: incorrect size %d of error_info\n",
				len);
		WARN_ON(1);
		return;
	}

	/* Ensure the context[] is terminated by the NULL character. */
	info->context[ERR_MAX_CONTEXT_LEN - 1] = '\0';
	pr_notice("[VCP] Error_info: case id: %u\n", info->case_id);
	pr_notice("[VCP] Error_info: sensor id: %u\n", info->sensor_id);
	pr_notice("[VCP] Error_info: context: %s\n", info->context);
}


/*
 * @return: 1 if vcp is ready for running tasks
 */
unsigned int is_vcp_ready(enum vcp_core_id id)
{
	if (vcp_ready[id])
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(is_vcp_ready);

/*
 * reset vcp and create a timer waiting for vcp notify
 * apps to stop their tasks if needed
 * generate error if reset fail
 * NOTE: this function may be blocked
 *       and should not be called in interrupt context
 * @param reset:    bit[0-3]=0 for vcp enable, =1 for reboot
 *                  bit[4-7]=0 for All, =1 for vcp_A, =2 for vcp_B
 * @return:         0 if success
 */
int reset_vcp(int reset)
{
	mutex_lock(&vcp_A_notify_mutex);
	blocking_notifier_call_chain(&vcp_A_notifier_list, VCP_EVENT_STOP,
		NULL);
	mutex_unlock(&vcp_A_notify_mutex);
#if VCP_DVFS_INIT_ENABLE
	/* request pll clock before turn on vcp */
	vcp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif
	if (reset & 0x0f) { /* do reset */
		/* make sure vcp is in idle state */
		vcp_reset_wait_timeout();
	}
	if (vcp_enable[VCP_A_ID]) {
		/* write vcp reserved memory address/size to GRP1/GRP2
		 * to let vcp setup MPU
		 */
		writel((unsigned int)vcp_mem_base_phys, DRAM_RESV_ADDR_REG);
		writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG);
		writel(1, R_CORE0_SW_RSTN_CLR);  /* release reset */
		dsb(SY); /* may take lot of time */
#if VCP_BOOT_TIME_OUT_MONITOR
		vcp_ready_timer[VCP_A_ID].tl.expires = jiffies + VCP_READY_TIMEOUT;
		add_timer(&vcp_ready_timer[VCP_A_ID].tl);
#endif
	}
	pr_debug("[VCP] %s: done\n", __func__);
	return 0;
}

/*
 * TODO: what should we do when hibernation ?
 */
static int vcp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	int retval;

	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_debug("[VCP] %s: reboot\n", __func__);
		retval = reset_vcp(VCP_ALL_REBOOT);
		if (retval < 0) {
			retval = -EINVAL;
			pr_debug("[VCP] %s: reboot fail\n", __func__);
		}
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block vcp_pm_notifier_block = {
	.notifier_call = vcp_pm_event,
	.priority = 0,
};


static inline ssize_t vcp_A_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	if (vcp_ready[VCP_A_ID])
		return scnprintf(buf, PAGE_SIZE, "VCP A is ready\n");
	else
		return scnprintf(buf, PAGE_SIZE, "VCP A is not ready\n");
}

DEVICE_ATTR_RO(vcp_A_status);

static inline ssize_t vcp_A_reg_status_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	int len = 0;

	vcp_dump_last_regs();
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_status = %08x\n", c0_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc = %08x\n", c0_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr = %08x\n", c0_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp = %08x\n", c0_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_pc_latch = %08x\n", c0_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_lr_latch = %08x\n", c0_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_sp_latch = %08x\n", c0_m->sp_latch);
	if (!vcpreg.twohart)
		goto core1;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_pc = %08x\n", c0_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_lr = %08x\n", c0_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_sp = %08x\n", c0_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_pc_latch = %08x\n", c0_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_lr_latch = %08x\n", c0_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c0_t1_sp_latch = %08x\n", c0_t1_m->sp_latch);
core1:
	if (vcpreg.core_nums == 1)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_status = %08x\n", c1_m->status);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc = %08x\n", c1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr = %08x\n", c1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp = %08x\n", c1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_pc_latch = %08x\n", c1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_lr_latch = %08x\n", c1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_sp_latch = %08x\n", c1_m->sp_latch);
	if (!vcpreg.twohart)
		goto end;
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_pc = %08x\n", c1_t1_m->pc);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_lr = %08x\n", c1_t1_m->lr);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_sp = %08x\n", c1_t1_m->sp);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_pc_latch = %08x\n", c1_t1_m->pc_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_lr_latch = %08x\n", c1_t1_m->lr_latch);
	len += scnprintf(buf + len, PAGE_SIZE - len,
		"c1_t1_sp_latch = %08x\n", c1_t1_m->sp_latch);

end:
	return len;
}

DEVICE_ATTR_RO(vcp_A_reg_status);

static inline ssize_t vcp_A_db_test_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666) {
			vcp_aed(RESET_TYPE_CMD, VCP_A_ID);
			if (vcp_ready[VCP_A_ID])
				pr_debug("dumping VCP db\n");
			else
				pr_debug("VCP is not ready, try to dump EE\n");
		}
	}

	return count;
}

DEVICE_ATTR_WO(vcp_A_db_test);

#ifdef VCP_DEBUG_NODE_ENABLE
static ssize_t vcp_ee_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", vcp_ee_enable);
}

static ssize_t vcp_ee_enable_store(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		vcp_ee_enable = value;
		pr_debug("[VCP] vcp_ee_enable = %d(1:enable, 0:disable)\n"
				, vcp_ee_enable);
	}
	return n;
}
DEVICE_ATTR_RW(vcp_ee_enable);

static inline ssize_t vcp_A_awake_lock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (vcp_ready[VCP_A_ID]) {
		vcp_awake_lock((void *)VCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "VCP A awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "VCP A is not ready\n");
}

DEVICE_ATTR_RO(vcp_A_awake_lock);

static inline ssize_t vcp_A_awake_unlock_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{

	if (vcp_ready[VCP_A_ID]) {
		vcp_awake_unlock((void *)VCP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "VCP A awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "VCP A is not ready\n");
}

DEVICE_ATTR_RO(vcp_A_awake_unlock);

enum ipi_debug_opt {
	IPI_TRACKING_OFF,
	IPI_TRACKING_ON,
	IPIMON_SHOW,
};

static inline ssize_t vcp_ipi_test_show(struct device *kobj
			, struct device_attribute *attr, char *buf)
{
	unsigned int value = 0x5A5A;
	int ret;

	if (vcp_ready[VCP_A_ID]) {
		ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_TEST_0, 0, &value,
				   PIN_OUT_SIZE_TEST_0, 0);
		return scnprintf(buf, PAGE_SIZE
			, "VCP A ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "VCP A is not ready\n");
}

static inline ssize_t vcp_ipi_test_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int opt;

	if (kstrtouint(buf, 10, &opt) != 0)
		return -EINVAL;

	switch (opt) {
	case IPI_TRACKING_ON:
	case IPI_TRACKING_OFF:
		mtk_ipi_tracking(&vcp_ipidev, opt);
		break;
	case IPIMON_SHOW:
		ipi_monitor_dump(&vcp_ipidev);
		break;
	default:
		pr_info("cmd '%d' is not supported.\n", opt);
		break;
	}

	return n;
}

DEVICE_ATTR_RW(vcp_ipi_test);

#endif

#if VCP_RECOVERY_SUPPORT
void vcp_wdt_reset(int cpu_id)
{
	switch (cpu_id) {
	case 0:
		writel(V_INSTANT_WDT, R_CORE0_WDT_CFG);
		break;
	case 1:
		writel(V_INSTANT_WDT, R_CORE1_WDT_CFG);
		break;
	}
}
EXPORT_SYMBOL(vcp_wdt_reset);

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
	pr_notice("[VCP] %s: %8s\n", __func__, buf);
	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 666)
			vcp_wdt_reset(0);
		else if (value == 667)
			vcp_wdt_reset(1);
	}
	return count;
}

DEVICE_ATTR_WO(wdt_reset);

/*
 * trigger vcp reset manually (debug use)
 */
static ssize_t vcp_reset_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int magic, trigger, counts;

	if (sscanf(buf, "%d %d %d", &magic, &trigger, &counts) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", __func__, magic, trigger, counts);

	if (magic != 666)
		return -EINVAL;

	vcp_reset_counts = counts;
	if (trigger == 1) {
		vcp_reset_by_cmd = 1;
		vcp_send_reset_wq(RESET_TYPE_CMD);
	}
	return n;
}

DEVICE_ATTR_WO(vcp_reset);
/*
 * trigger wdt manually
 * debug use
 */

static ssize_t recovery_flag_show(struct device *dev
			, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", vcp_recovery_flag[VCP_A_ID]);
}
static ssize_t recovery_flag_store(struct device *dev
		, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (kstrtoint(buf, 10, &tmp) < 0) {
		pr_debug("vcp_recovery_flag error\n");
		return count;
	}
	vcp_recovery_flag[VCP_A_ID] = tmp;
	return count;
}

DEVICE_ATTR_RW(recovery_flag);

#endif

/******************************************************************************
 *****************************************************************************/
static struct miscdevice vcp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vcp",
	.fops = &vcp_A_log_file_ops
};


/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
static int create_files(void)
{
	int ret;

	ret = misc_register(&vcp_device);
	if (unlikely(ret != 0)) {
		pr_notice("[VCP] misc register failed\n");
		return ret;
	}

#if VCP_LOGGER_ENABLE
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_logger_wakeup_AP);
	if (unlikely(ret != 0))
		return ret;

#ifdef VCP_DEBUG_NODE_ENABLE
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_DEBUG_NODE_ENABLE

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_LOGGER_ENABLE

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_status);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(vcp_device.this_device
					, &bin_attr_vcp_dump);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_reg_status);
	if (unlikely(ret != 0))
		return ret;

	/*only support debug db test in engineer build*/
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_db_test);
	if (unlikely(ret != 0))
		return ret;

#ifdef VCP_DEBUG_NODE_ENABLE
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_ee_enable);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_awake_lock);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_A_awake_unlock);
	if (unlikely(ret != 0))
		return ret;

	/* VCP IPI Debug sysfs*/
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_ipi_test);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_DEBUG_NODE_ENABLE

#if VCP_RECOVERY_SUPPORT
	ret = device_create_file(vcp_device.this_device
					, &dev_attr_wdt_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_recovery_flag);
	if (unlikely(ret != 0))
		return ret;
#endif  // VCP_RECOVERY_SUPPORT

	ret = device_create_file(vcp_device.this_device,
					&dev_attr_log_filter);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcpctl);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

#if VCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)
#define VCP_MEM_RESERVED_KEY "mediatek,reserve-memory-vcp_share"
int vcp_reserve_mem_of_init(struct reserved_mem *rmem)
{
	pr_notice("[VCP]%s %pa %pa\n", __func__, &rmem->base, &rmem->size);
	vcp_mem_base_phys = (phys_addr_t) rmem->base;
	vcp_mem_size = (phys_addr_t) rmem->size;

	return 0;
}

RESERVEDMEM_OF_DECLARE(vcp_reserve_mem_init
			, VCP_MEM_RESERVED_KEY, vcp_reserve_mem_of_init);
#endif  // VCP_RESERVED_MEM && defined(CONFIG_OF_RESERVED_MEM)

phys_addr_t vcp_get_reserve_mem_phys(enum vcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_notice("[VCP] no reserve memory for %d", id);
		return 0;
	} else
		return vcp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_phys);

phys_addr_t vcp_get_reserve_mem_virt(enum vcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_notice("[VCP] no reserve memory for %d", id);
		return 0;
	} else
		return vcp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_virt);

phys_addr_t vcp_get_reserve_mem_size(enum vcp_reserve_mem_id_t id)
{
	if (id >= NUMS_MEM_ID) {
		pr_notice("[VCP] no reserve memory for %d", id);
		return 0;
	} else
		return vcp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_size);

#if VCP_RESERVED_MEM && defined(CONFIG_OF)
static int vcp_reserve_memory_ioremap(struct platform_device *pdev)
{
#define MEMORY_TBL_ELEM_NUM (2)
	unsigned int num = (unsigned int)(sizeof(vcp_reserve_mblock)
			/ sizeof(vcp_reserve_mblock[0]));
	enum vcp_reserve_mem_id_t id;
	phys_addr_t accumlate_memory_size = 0;
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	const char *mem_key;
	unsigned int vcp_mem_num = 0;
	unsigned int i, m_idx, m_size;
	int ret;

	if (num != NUMS_MEM_ID) {
		pr_notice("[VCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		BUG_ON(1);
		return -1;
	}
	/* Get reserved memory */
	ret = of_property_read_string(pdev->dev.of_node, "vcp_mem_key",
			&mem_key);
	if (ret) {
		pr_info("[VCP] cannot find property\n");
		return -EINVAL;
	}

	rmem_node = of_find_compatible_node(NULL, NULL, mem_key);

	if (!rmem_node) {
		pr_info("[VCP] no node for reserved memory\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("[VCP] cannot lookup reserved memory\n");
		return -EINVAL;
	}

	vcp_mem_base_phys = (phys_addr_t) rmem->base;
	vcp_mem_size = (phys_addr_t) rmem->size;

	pr_notice("[VCP] %s is called, 0x%x, 0x%x",
		__func__,
		(unsigned int)vcp_mem_base_phys,
		(unsigned int)vcp_mem_size);

	if ((vcp_mem_base_phys >= (0x90000000ULL)) ||
			 (vcp_mem_base_phys <= 0x0)) {
		/* The vcp remapped region is fixed, only
		 * 0x4000_0000ULL ~ 0x8FFF_FFFFULL is accessible.
		 */
		pr_notice("[VCP] Error: Wrong Address (0x%llx)\n",
			    (uint64_t)vcp_mem_base_phys);
		BUG_ON(1);
		return -1;
	}

	/* Set reserved memory table */
	vcp_mem_num = of_property_count_u32_elems(
				pdev->dev.of_node,
				"vcp_mem_tbl")
				/ MEMORY_TBL_ELEM_NUM;
	if (vcp_mem_num <= 0) {
		pr_notice("[VCP] vcp_mem_tbl not found\n");
		vcp_mem_num = 0;
	}

	for (i = 0; i < vcp_mem_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"vcp_mem_tbl",
				i * MEMORY_TBL_ELEM_NUM,
				&m_idx);
		if (ret) {
			pr_notice("Cannot get memory index(%d)\n", i);
			return -1;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"vcp_mem_tbl",
				(i * MEMORY_TBL_ELEM_NUM) + 1,
				&m_size);
		if (ret) {
			pr_notice("Cannot get memory size(%d)\n", i);
			return -1;
		}

		if (m_idx >= NUMS_MEM_ID) {
			pr_notice("[VCP] skip unexpected index, %d\n", m_idx);
			continue;
		}

		vcp_reserve_mblock[m_idx].size = m_size;
		pr_notice("@@@@ reserved: <%d  %d>\n", m_idx, m_size);
	}

	vcp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(vcp_mem_base_phys,
		vcp_mem_size);
	pr_debug("[VCP] rsrv_phy_base = 0x%llx, len:0x%llx\n",
		(uint64_t)vcp_mem_base_phys, (uint64_t)vcp_mem_size);
	pr_debug("[VCP] rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)vcp_mem_base_virt, (uint64_t)vcp_mem_size);

	for (id = 0; id < NUMS_MEM_ID; id++) {
		vcp_reserve_mblock[id].start_phys = vcp_mem_base_phys +
			accumlate_memory_size;
		vcp_reserve_mblock[id].start_virt = vcp_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += vcp_reserve_mblock[id].size;
#ifdef DEBUG
		pr_debug("[VCP] [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)vcp_reserve_mblock[id].start_phys,
			(uint64_t)vcp_reserve_mblock[id].start_virt,
			(uint64_t)vcp_reserve_mblock[id].size);
#endif  // DEBUG
	}
#ifdef VCP_DEBUG_NODE_ENABLE
	BUG_ON(accumlate_memory_size > vcp_mem_size);
#endif
#ifdef DEBUG
	for (id = 0; id < NUMS_MEM_ID; id++) {
		uint64_t start_phys = (uint64_t)vcp_get_reserve_mem_phys(id);
		uint64_t start_virt = (uint64_t)vcp_get_reserve_mem_virt(id);
		uint64_t len = (uint64_t)vcp_get_reserve_mem_size(id);

		pr_notice("[VCP][rsrv_mem-%d] phy:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_phys, start_phys + len - 1, len);
		pr_notice("[VCP][rsrv_mem-%d] vir:0x%llx - 0x%llx, len:0x%llx\n",
			id, start_virt, start_virt + len - 1, len);
	}
#endif  // DEBUG
	return 0;
}
#endif

#if ENABLE_VCP_EMI_PROTECTION
void set_vcp_mpu(void)
{
	struct emimpu_region_t md_region;

	mtk_emimpu_init_region(&md_region, MPU_REGION_ID_VCP_SMEM);
	mtk_emimpu_set_addr(&md_region, vcp_mem_base_phys,
		vcp_mem_base_phys + vcp_mem_size - 1);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D0,
		MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&md_region, MPU_DOMAIN_D3,
		MTK_EMIMPU_NO_PROTECTION);
	if (mtk_emimpu_set_protection(&md_region))
		pr_notice("[VCP]mtk_emimpu_set_protection fail\n");
	mtk_emimpu_free_region(&md_region);
}
#endif

void vcp_register_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/*prevent from access when vcp is down*/
	if (!vcp_ready[VCP_A_ID]) {
		pr_debug("[VCP] %s: not ready, vcp=%u\n", __func__,
			vcp_ready[VCP_A_ID]);
		return;
	}

	/* prevent from access when vcp dvfs cali isn't done */
	if (!vcp_dvfs_cali_ready) {
		pr_debug("[VCP] %s: dvfs cali not ready, vcp_dvfs_cali=%u\n",
		__func__, vcp_dvfs_cali_ready);
		return;
	}

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&vcp_feature_mutex);

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 1;
	}
#if VCP_DVFS_INIT_ENABLE
	vcp_expected_freq = vcp_get_freq();
#endif

	vcp_current_freq = readl(CURRENT_FREQ_REG);
	writel(vcp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when vcp is not down */
	if (vcp_ready[VCP_A_ID]) {
		if (vcp_current_freq != vcp_expected_freq) {
			/* set vcp freq. */
#if VCP_DVFS_INIT_ENABLE
			ret = vcp_request_freq();
#endif
			if (ret == -1) {
				pr_notice("[VCP]%s request_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_notice("[VCP]Not send VCP DVFS request because VCP is down\n");
		WARN_ON(1);
	}

	mutex_unlock(&vcp_feature_mutex);
}
EXPORT_SYMBOL_GPL(vcp_register_feature);

void vcp_deregister_feature(enum feature_id id)
{
	uint32_t i;
	int ret = 0;

	/* prevent from access when vcp is down */
	if (!vcp_ready[VCP_A_ID]) {
		pr_debug("[VCP] %s:not ready, vcp=%u\n", __func__,
			vcp_ready[VCP_A_ID]);
		return;
	}

	/* prevent from access when vcp dvfs cali isn't done */
	if (!vcp_dvfs_cali_ready) {
		pr_debug("[VCP] %s: dvfs cali not ready, vcp_dvfs_cali=%u\n",
		__func__, vcp_dvfs_cali_ready);
		return;
	}

	mutex_lock(&vcp_feature_mutex);

	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable = 0;
	}
#if VCP_DVFS_INIT_ENABLE
	vcp_expected_freq = vcp_get_freq();
#endif

	vcp_current_freq = readl(CURRENT_FREQ_REG);
	writel(vcp_expected_freq, EXPECTED_FREQ_REG);

	/* send request only when vcp is not down */
	if (vcp_ready[VCP_A_ID]) {
		if (vcp_current_freq != vcp_expected_freq) {
			/* set vcp freq. */
#if VCP_DVFS_INIT_ENABLE
			ret = vcp_request_freq();
#endif
			if (ret == -1) {
				pr_notice("[VCP] %s: req_freq fail\n", __func__);
				WARN_ON(1);
			}
		}
	} else {
		pr_notice("[VCP]Not send VCP DVFS request because VCP is down\n");
		WARN_ON(1);
	}

	mutex_unlock(&vcp_feature_mutex);
}
EXPORT_SYMBOL_GPL(vcp_deregister_feature);

/*vcp sensor type register*/
void vcp_register_sensor(enum feature_id id, enum vcp_sensor_id sensor_id)
{
	uint32_t i;

	/* prevent from access when vcp is down */
	if (!vcp_ready[VCP_A_ID])
		return;

	if (id != SENS_FEATURE_ID) {
		pr_debug("[VCP]register sensor id err");
		return;
	}
	/* because feature_table is a global variable
	 * use mutex lock to protect it from
	 * accessing in the same time
	 */
	mutex_lock(&vcp_register_sensor_mutex);
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].feature == sensor_id)
			sensor_type_table[i].enable = 1;
	}

	/* register sensor*/
	vcp_register_feature(id);
	mutex_unlock(&vcp_register_sensor_mutex);

}
/*vcp sensor type deregister*/
void vcp_deregister_sensor(enum feature_id id, enum vcp_sensor_id sensor_id)
{
	uint32_t i;

	/* prevent from access when vcp is down */
	if (!vcp_ready[VCP_A_ID])
		return;

	if (id != SENS_FEATURE_ID) {
		pr_debug("[VCP]deregister sensor id err");
		return;
	}
	/* because feature_table is a global variable
	 * use mutex lock to protect it from
	 * accessing in the same time
	 */
	mutex_lock(&vcp_register_sensor_mutex);
	for (i = 0; i < NUM_SENSOR_TYPE; i++) {
		if (sensor_type_table[i].feature == sensor_id)
			sensor_type_table[i].enable = 0;
	}
	/* deregister sensor*/
	vcp_deregister_feature(id);
	mutex_unlock(&vcp_register_sensor_mutex);
}

/*
 * apps notification
 */
void vcp_extern_notify(enum VCP_NOTIFY_EVENT notify_status)
{
	blocking_notifier_call_chain(&vcp_A_notifier_list, notify_status, NULL);
}

/*
 * reset awake counter
 */
void vcp_reset_awake_counts(void)
{
	int i;

	/* vcp ready static flag initialise */
	for (i = 0; i < VCP_CORE_TOTAL ; i++)
		vcp_awake_counts[i] = 0;
}

void vcp_awake_init(void)
{
	vcp_reset_awake_counts();
}

#if VCP_RECOVERY_SUPPORT
/*
 * vcp_set_reset_status, set and return vcp reset status function
 * return value:
 *   0: vcp not in reset status
 *   1: vcp in reset status
 */
unsigned int vcp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&vcp_reset_spinlock, spin_flags);
	if (atomic_read(&vcp_reset_status) == RESET_STATUS_START) {
		spin_unlock_irqrestore(&vcp_reset_spinlock, spin_flags);
		return 1;
	}
	/* vcp not in reset status, set it and return*/
	atomic_set(&vcp_reset_status, RESET_STATUS_START);
	spin_unlock_irqrestore(&vcp_reset_spinlock, spin_flags);
	return 0;
}

/******************************************************************************
 *****************************************************************************/
void print_clk_registers(void)
{
	void __iomem *cfg = vcpreg.cfg;
	void __iomem *clkctrl = vcpreg.clkctrl;
	void __iomem *cfg_core0 = vcpreg.cfg_core0;
	void __iomem *cfg_core1 = vcpreg.cfg_core1;

	unsigned int offset;
	unsigned int value;

	// 0x24000 ~ 0x24160 (inclusive)
	for (offset = 0x0000; offset <= 0x0160; offset += 4) {
		value = (unsigned int)readl(cfg + offset);
		pr_notice("[VCP] cfg[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x21000 ~ 0x210120 (inclusive)
	for (offset = 0x0000; offset < 0x0120; offset += 4) {
		value = (unsigned int)readl(clkctrl + offset);
		pr_notice("[VCP] clk[0x%04x]: 0x%08x\n", offset, value);
	}
	// 0x30000 ~ 0x30114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core0 + offset);
		pr_notice("[VCP] cfg_core0[0x%04x]: 0x%08x\n", offset, value);
	}
	if (vcpreg.core_nums == 1)
		return;
	// 0x40000 ~ 0x40114 (inclusive)
	for (offset = 0x0000; offset <= 0x0114; offset += 4) {
		value = (unsigned int)readl(cfg_core1 + offset);
		pr_notice("[VCP] cfg_core1[0x%04x]: 0x%08x\n", offset, value);
	}

}

void vcp_reset_wait_timeout(void)
{
	uint32_t core0_halt = 0;
	uint32_t core1_halt = 0;
	/* make sure vcp is in idle state */
	int timeout = 50; /* max wait 1s */

	while (timeout--) {
		core0_halt = readl(R_CORE0_STATUS) & B_CORE_HALT;
		core1_halt = vcpreg.core_nums == 2? readl(R_CORE1_STATUS) & B_CORE_HALT: 1;
		if (core0_halt && core1_halt) {
			/* VCP stops any activities
			 * and parks at wfi
			 */
			break;
		}
		mdelay(20);
	}

	if (timeout == 0)
		pr_notice("[VCP] reset timeout, still reset vcp\n");

}

/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
void vcp_sys_reset_ws(struct work_struct *ws)
{
	struct vcp_work_struct *sws = container_of(ws
					, struct vcp_work_struct, work);
	unsigned int vcp_reset_type = sws->flags;
	unsigned long spin_flags;

	pr_debug("[VCP] %s(): remain %d times\n", __func__, vcp_reset_counts);
	/*notify vcp functions stop*/
	pr_debug("[VCP] %s(): vcp_extern_notify\n", __func__);
	vcp_extern_notify(VCP_EVENT_STOP);
	/*
	 *   vcp_ready:
	 *   VCP_PLATFORM_STOP  = 0,
	 *   VCP_PLATFORM_READY = 1,
	 */
	vcp_ready[VCP_A_ID] = 0;
	vcp_dvfs_cali_ready = 0;

	/* wake lock AP*/
	__pm_stay_awake(vcp_reset_lock);
#if VCP_DVFS_INIT_ENABLE
	/* keep Univpll */
	vcp_resource_req(VCP_REQ_26M);
#endif

	/* print_clk and vcp_aed before pll enable to keep ori CLK_SEL */
	print_clk_registers();
	/*workqueue for vcp ee, vcp reset by cmd will not trigger vcp ee*/
	if (vcp_reset_by_cmd == 0) {
		pr_debug("[VCP] %s(): vcp_aed_reset\n", __func__);
		vcp_aed(vcp_reset_type, VCP_A_ID);
	}
	pr_debug("[VCP] %s(): disable logger\n", __func__);
	/* logger disable must after vcp_aed() */
	vcp_logger_init_set(0);

	pr_debug("[VCP] %s(): vcp_pll_ctrl_set\n", __func__);
	/*request pll clock before turn off vcp */
#if VCP_DVFS_INIT_ENABLE
	vcp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
#endif
	pr_notice("[VCP] %s(): vcp_reset_type %d\n", __func__, vcp_reset_type);
	/* vcp reset by CMD, WDT or awake fail */
	if ((vcp_reset_type == RESET_TYPE_TIMEOUT) ||
		(vcp_reset_type == RESET_TYPE_AWAKE)) {
		/* stop vcp */
		writel(1, R_CORE0_SW_RSTN_SET);
		writel(1, R_CORE1_SW_RSTN_SET);
		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %x core1 %x\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
	} else {
		/* reset type vcp WDT or CMD*/
		/* make sure vcp is in idle state */
		vcp_reset_wait_timeout();
		writel(1, R_CORE0_SW_RSTN_SET);
		writel(1, R_CORE1_SW_RSTN_SET);
		writel(CORE_REBOOT_OK, VCP_GPR_CORE0_REBOOT);
		writel(CORE_REBOOT_OK, VCP_GPR_CORE1_REBOOT);
		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %x core1 %x\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET));
	}

	/* vcp reset */
	vcp_sys_full_reset();

#ifdef VCP_PARAMS_TO_VCP_SUPPORT
	/* The function, sending parameters to vcp must be anchored before
	 * 1. disabling 26M, 2. resetting VCP
	 */
	if (params_to_vcp() != 0)
		return;
#endif

	spin_lock_irqsave(&vcp_awake_spinlock, spin_flags);
	vcp_reset_awake_counts();
	spin_unlock_irqrestore(&vcp_awake_spinlock, spin_flags);

	/* Setup dram reserved address and size for vcp*/
	writel((unsigned int)vcp_mem_base_phys, DRAM_RESV_ADDR_REG);
	writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG);
	/* start vcp */
	pr_notice("[VCP] start vcp\n");
	writel(1, R_CORE0_SW_RSTN_CLR);
	pr_notice("[VCP] rstn core0 %x\n", readl(R_CORE0_SW_RSTN_CLR));
	dsb(SY); /* may take lot of time */
#if VCP_BOOT_TIME_OUT_MONITOR
	mod_timer(&vcp_ready_timer[VCP_A_ID].tl, jiffies + VCP_READY_TIMEOUT);
#endif
	/* clear vcp reset by cmd flag*/
	vcp_reset_by_cmd = 0;
}


/*
 * schedule a work to reset vcp
 * @param type: exception type
 */
void vcp_send_reset_wq(enum VCP_RESET_TYPE type)
{
	vcp_sys_reset_work.flags = (unsigned int) type;
	vcp_sys_reset_work.id = VCP_A_ID;
	if (vcp_reset_counts > 0) {
		vcp_reset_counts--;
		vcp_schedule_reset_work(&vcp_sys_reset_work);
	}
}
#endif

int vcp_check_resource(void)
{
	/* called by lowpower related function
	 * main purpose is to ensure main_pll is not disabled
	 * because vcp needs main_pll to run at vcore 1.0 and 354Mhz
	 * return value:
	 * 1: main_pll shall be enabled
	 *    26M shall be enabled, infra shall be enabled
	 * 0: main_pll may disable, 26M may disable, infra may disable
	 */
	int vcp_resource_status = 0;
	return vcp_resource_status;
}

#if VCP_RECOVERY_SUPPORT
void vcp_region_info_init(void)
{
	/*get vcp loader/firmware info from vcp sram*/
	vcp_region_info = (VCP_TCM + VCP_REGION_INFO_OFFSET);
	pr_debug("[VCP] vcp_region_info = %p\n", vcp_region_info);
	memcpy_from_vcp(&vcp_region_info_copy,
		vcp_region_info, sizeof(vcp_region_info_copy));
}
#else
void vcp_region_info_init(void) {}
#endif

void vcp_recovery_init(void)
{
#if VCP_RECOVERY_SUPPORT
	/*create wq for vcp reset*/
	vcp_reset_workqueue = create_singlethread_workqueue("VCP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&vcp_sys_reset_work.work, vcp_sys_reset_ws);

	vcp_loader_virt = ioremap_wc(
		vcp_region_info_copy.ap_loader_start,
		vcp_region_info_copy.ap_loader_size);
	pr_debug("[VCP] loader image mem: virt:0x%llx - 0x%llx\n",
		(uint64_t)(phys_addr_t)vcp_loader_virt,
		(uint64_t)(phys_addr_t)vcp_loader_virt +
		(phys_addr_t)vcp_region_info_copy.ap_loader_size);
	/*init wake,
	 *this is for prevent vcp pll cpu clock disabled during reset flow
	 */
	vcp_reset_lock = wakeup_source_register(NULL, "vcp reset wakelock");
	/* init reset by cmd flag */
	vcp_reset_by_cmd = 0;

	vcp_regdump_virt = ioremap_wc(
			vcp_region_info_copy.regdump_start,
			vcp_region_info_copy.regdump_size);
	pr_debug("[VCP] vcp_regdump_virt map: 0x%x + 0x%x\n",
		vcp_region_info_copy.regdump_start,
		vcp_region_info_copy.regdump_size);

	if ((int)(vcp_region_info_copy.ap_dram_size) > 0) {
		/*if l1c enable, map it (include backup) */
		vcp_ap_dram_virt = ioremap_wc(
		vcp_region_info_copy.ap_dram_start,
		ROUNDUP(vcp_region_info_copy.ap_dram_size, 1024)*4);

	pr_debug("[VCP] vcp_ap_dram_virt map: 0x%x + 0x%x\n",
		vcp_region_info_copy.ap_dram_start,
		vcp_region_info_copy.ap_dram_size);
	}
#endif
}

static bool vcp_ipi_table_init(struct mtk_mbox_device *vcp_mboxdev, struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i, ret, mbox_id, recv_opt;
	of_property_read_u32(pdev->dev.of_node, "mbox_count"
						, &vcp_mboxdev->count);
	if (!vcp_mboxdev->count) {
		pr_notice("[VCP] mbox count not found\n");
		return false;
	}

	vcp_mboxdev->send_count = of_property_count_u32_elems(
				pdev->dev.of_node, "send_table")
				/ send_item_num;
	if (vcp_mboxdev->send_count <= 0) {
		pr_notice("[VCP] vcp send table not found\n");
		return false;
	}

	vcp_mboxdev->recv_count = of_property_count_u32_elems(
				pdev->dev.of_node, "recv_table")
				/ recv_item_num;
	if (vcp_mboxdev->recv_count <= 0) {
		pr_notice("[VCP] vcp recv table not found\n");
		return false;
	}
	/* alloc and init vcp_mbox_info */
	vcp_mboxdev->info_table = vzalloc(sizeof(struct mtk_mbox_info) * vcp_mboxdev->count);
	if (!vcp_mboxdev->info_table) {
		pr_notice("[VCP]%s: vmlloc info table fail:%d\n", __func__, __LINE__);
		return false;
	}
	vcp_mbox_info = vcp_mboxdev->info_table;
	for (i = 0; i < vcp_mboxdev->count; ++i) {
		vcp_mbox_info[i].id = i;
		vcp_mbox_info[i].slot = 64;
		vcp_mbox_info[i].enable = 1;
		vcp_mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	vcp_mboxdev->pin_send_table = vzalloc(sizeof(struct mtk_mbox_pin_send) * vcp_mboxdev->send_count);
	if (!vcp_mboxdev->pin_send_table) {
		pr_notice("[VCP]%s: vmlloc send table fail:%d\n", __func__, __LINE__);
		return false;
	}
	vcp_mbox_pin_send = vcp_mboxdev->pin_send_table;
	for (i = 0; i < vcp_mboxdev->send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num,
				&vcp_mbox_pin_send[i].chan_id);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_send[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"send_table",
				i * send_item_num + 2,
				&vcp_mbox_pin_send[i].msg_size);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
	}
	/* alloc and init recv table */
	vcp_mboxdev->pin_recv_table = vzalloc(sizeof(struct mtk_mbox_pin_recv) * vcp_mboxdev->recv_count);
	if (!vcp_mboxdev->pin_recv_table) {
		pr_notice("[VCP]%s: vmlloc recv table fail:%d\n", __func__, __LINE__);
		return false;
	}
	vcp_mbox_pin_recv = vcp_mboxdev->pin_recv_table;
	for (i = 0; i < vcp_mboxdev->recv_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num,
				&vcp_mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get ipi id (%d):%d\n", __func__, i,__LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 1,
				&mbox_id);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get mbox id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_recv[i].mbox = mbox_id;
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 2,
				&vcp_mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get pin size (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"recv_table",
				i * recv_item_num + 3,
				&recv_opt);
		if (ret) {
			pr_notice("[VCP]%s:Cannot get recv opt (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		vcp_mbox_pin_recv[i].recv_opt = recv_opt;
	}


	/* wrapper_ipi_init */
	if (!of_get_property(pdev->dev.of_node, "legacy_table", NULL)) {
		pr_notice("[VCP]%s: wrapper_ipi don't exist\n", __func__);
		return true;
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 0, &vcp_ipi_legacy_id[0].out_id_0);
	if (ret) {
		pr_notice("[VCP]%s:Cannot get out_id_0\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 1, &vcp_ipi_legacy_id[0].out_id_1);
	if (ret) {
		pr_notice("[VCP]%s:Cannot get out_id_1\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 2, &vcp_ipi_legacy_id[0].in_id_0);
	if (ret) {
		pr_notice("[VCP]%s:Cannot get in_id_0\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 3, &vcp_ipi_legacy_id[0].in_id_1);
	if (ret) {
		pr_notice("[VCP]%s:Cannot get in_id_1\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 4, &vcp_ipi_legacy_id[0].out_size);
	if (ret) {
		pr_notice("[%s]:Cannot get out_size\n", __func__);
	}
	ret = of_property_read_u32_index(pdev->dev.of_node,
			"legacy_table", 5, &vcp_ipi_legacy_id[0].in_size);
	if (ret) {
		pr_notice("[VCP]%s:Cannot get in_size\n", __func__);
	}
	vcp_ipi_legacy_id[0].msg_0 = vzalloc(vcp_ipi_legacy_id[0].in_size * MBOX_SLOT_SIZE);
	if (!vcp_ipi_legacy_id[0].msg_0) {
		pr_notice("[VCP]%s: vmlloc legacy msg_0 fail\n", __func__);
		return false;
	}
	vcp_ipi_legacy_id[0].msg_1 = vzalloc(vcp_ipi_legacy_id[0].in_size * MBOX_SLOT_SIZE);
	if (!vcp_ipi_legacy_id[0].msg_1) {
		pr_notice("[VCP]%s: vmlloc legacy msg_1 fail\n", __func__);
		return false;
	}
	return true;
}

static int vcp_io_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_debug("[VCP_IO] %s", __func__);

	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0 || vcp_support == 1) {
		pr_info("Bypass the VCP driver probe\n");
		return -1;
	}
	// VCP iommu devices
	vcp_io_devs[vcp_support-1] = dev;

	return 0;
}

static int vcp_io_device_remove(struct platform_device *dev)
{
	return 0;
}

static int vcp_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node;

	pr_debug("[VCP] %s", __func__);

	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0) {
		pr_info("Bypass the VCP driver probe\n");
		return 0;
	} else {
		// VCP iommu devices
		vcp_io_devs[vcp_support-1] = dev;
		if (vcp_support > 1)
			return 0;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vcpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.sram)) {
		pr_notice("[VCP] vcpreg.sram error\n");
		return -1;
	}
	vcpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[VCP] sram base = 0x%p %x\n"
		, vcpreg.sram, vcpreg.total_tcmsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	vcpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg)) {
		pr_notice("[VCP] vcpreg.cfg error\n");
		return -1;
	}
	pr_debug("[VCP] cfg base = 0x%p\n", vcpreg.cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	vcpreg.clkctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.clkctrl)) {
		pr_notice("[VCP] vcpreg.clkctrl error\n");
		return -1;
	}
	pr_debug("[VCP] clkctrl base = 0x%p\n", vcpreg.clkctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	vcpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core0)) {
		pr_debug("[VCP] vcpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core0 base = 0x%p\n", vcpreg.cfg_core0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	vcpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core1)) {
		pr_debug("[VCP] vcpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core1 base = 0x%p\n", vcpreg.cfg_core1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	vcpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.bus_tracker)) {
		pr_debug("[VCP] vcpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[VCP] bus_tracker base = 0x%p\n", vcpreg.bus_tracker);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 6);
	vcpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.l1cctrl)) {
		pr_debug("[VCP] vcpreg.l1cctrl error\n");
		return -1;
	}
	pr_debug("[VCP] l1cctrl base = 0x%p\n", vcpreg.l1cctrl);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 7);
	vcpreg.cfg_sec = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_sec)) {
		pr_debug("[VCP] vcpreg.cfg_sec error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_sec base = 0x%p\n", vcpreg.cfg_sec);


	of_property_read_u32(pdev->dev.of_node, "vcp_sramSize"
						, &vcpreg.vcp_tcmsize);
	if (!vcpreg.vcp_tcmsize) {
		pr_notice("[VCP] total_tcmsize not found\n");
		return -ENODEV;
	}
	pr_debug("[VCP] vcpreg.vcp_tcmsize = %d\n", vcpreg.vcp_tcmsize);

	/* vcp core 0 */
	if (of_property_read_string(pdev->dev.of_node, "core_0", &core_status))
		return -1;

	if (strcmp(core_status, "enable") != 0)
		pr_notice("[VCP] core_0 not enable\n");
	else {
		pr_debug("[VCP] core_0 enable\n");
		vcp_enable[VCP_A_ID] = 1;
	}

	of_property_read_u32(pdev->dev.of_node, "core_nums"
						, &vcpreg.core_nums);
	if (!vcpreg.core_nums) {
		pr_notice("[VCP] core number not found\n");
		return -ENODEV;
	}
	pr_notice("[VCP] vcpreg.core_nums = %d\n", vcpreg.core_nums);

	of_property_read_u32(pdev->dev.of_node, "twohart"
						, &vcpreg.twohart);
	pr_notice("[VCP] vcpreg.twohart = %d\n", vcpreg.twohart);


	vcpreg.irq0 = platform_get_irq_byname(pdev, "ipc0");
	if (vcpreg.irq0 < 0)
		pr_notice("[VCP] get ipc0 irq failed\n");
	else {
		pr_debug("ipc0 %d\n", vcpreg.irq0);
		ret = request_irq(vcpreg.irq0, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP IPC0", NULL);
		if (ret < 0)
			pr_notice("[VCP]ipc0 require fail %d %d\n",
				vcpreg.irq0, ret);
		else {
			ret = enable_irq_wake(vcpreg.irq0);
			if (ret < 0)
				pr_notice("[VCP] ipc0 wake fail:%d,%d\n",
					vcpreg.irq0, ret);
		}
	}

	vcpreg.irq1 = platform_get_irq_byname(pdev, "ipc1");
	if (vcpreg.irq1 < 0)
		pr_notice("[VCP] get ipc1 irq failed\n");
	else {
		pr_debug("ipc1 %d\n", vcpreg.irq1);
		ret = request_irq(vcpreg.irq1, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP IPC1", NULL);
		if (ret < 0)
			pr_notice("[VCP]ipc1 require irq fail %d %d\n",
				vcpreg.irq1, ret);
		else {
			ret = enable_irq_wake(vcpreg.irq1);
			if (ret < 0)
				pr_notice("[VCP] irq wake fail:%d,%d\n",
					vcpreg.irq1, ret);
		}
	}

	/* probe mbox info from dts */
	if (!vcp_ipi_table_init(&vcp_mboxdev, pdev))
		return -ENODEV;
	/* create mbox dev */
	pr_debug("[VCP] mbox probe\n");
	for (i = 0; i < vcp_mboxdev.count; i++) {
		vcp_mbox_info[i].mbdev = &vcp_mboxdev;
		ret = mtk_mbox_probe(pdev, vcp_mbox_info[i].mbdev, i);
		if (ret < 0 || vcp_mboxdev.info_table[i].irq_num < 0) {
			pr_notice("[VCP] mbox%d probe fail\n", i, ret);
			continue;
		}

		ret = enable_irq_wake(vcp_mboxdev.info_table[i].irq_num);
		if (ret < 0) {
			pr_notice("[VCP]mbox%d enable irq fail\n", i, ret);
			continue;
		}
		mbox_setup_pin_table(i);
	}

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (vcp_ipi_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
					      vcp_ipi_irqs[i].name);
		if (!node) {
			pr_info("[VCP] find '%s' node failed\n",
				vcp_ipi_irqs[i].name);
			continue;
		}
		vcp_ipi_irqs[i].irq_no =
			irq_of_parse_and_map(node, vcp_ipi_irqs[i].order);
		if (!vcp_ipi_irqs[i].irq_no)
			pr_info("[VCP] get '%s' fail\n", vcp_ipi_irqs[i].name);
	}

	ret = mtk_ipi_device_register(&vcp_ipidev, pdev, &vcp_mboxdev,
				      VCP_IPI_COUNT);
	if (ret)
		pr_notice("[VCP] ipi_dev_register fail, ret %d\n", ret);

#if VCP_RESERVED_MEM && defined(CONFIG_OF)

	/*vcp resvered memory*/
	pr_notice("[VCP] vcp_reserve_memory_ioremap\n");
	ret = vcp_reserve_memory_ioremap(pdev);
	if (ret) {
		pr_notice("[VCP]vcp_reserve_memory_ioremap failed\n");
		return ret;
	}
#endif
	pr_info("[VCP] %s done\n", __func__);

	return ret;
}

static int vcp_device_remove(struct platform_device *dev)
{
	if (vcp_mbox_info) {
		kfree(vcp_mbox_info);
		vcp_mbox_info = NULL;
	}
	if (vcp_mbox_pin_recv) {
		kfree(vcp_mbox_pin_recv);
		vcp_mbox_pin_recv = NULL;
	}
	if (vcp_mbox_pin_send) {
		kfree(vcp_mbox_pin_send);
		vcp_mbox_pin_send = NULL;
	}

	return 0;
}

static const struct dev_pm_ops vcp_ipi_dbg_pm_ops = {
	.resume_noirq = vcp_ipi_dbg_resume_noirq,
};

static const struct of_device_id vcp_of_ids[] = {
	{ .compatible = "mediatek,vcp", },
	{}
};

static struct platform_driver mtk_vcp_device = {
	.probe = vcp_device_probe,
	.remove = vcp_device_remove,
	.driver = {
		.name = "vcp",
		.owner = THIS_MODULE,
		.of_match_table = vcp_of_ids,
		.pm = &vcp_ipi_dbg_pm_ops,
	},
};

static const struct of_device_id vcp_vdec_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-vdec", },
	{}
};
static const struct of_device_id vcp_venc_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-venc", },
	{}
};
static const struct of_device_id vcp_work_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-work", },
	{}
};

static struct platform_driver mtk_vcp_io_vdec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_vdec ",
		.owner = THIS_MODULE,
		.of_match_table = vcp_vdec_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_venc = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_venc",
		.owner = THIS_MODULE,
		.of_match_table = vcp_venc_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_work = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_work",
		.owner = THIS_MODULE,
		.of_match_table = vcp_work_of_ids,
	},
};

/*
 * driver initialization entry point
 */
static int __init vcp_init(void)
{
	int ret = 0;
	int i = 0;
#if VCP_BOOT_TIME_OUT_MONITOR
	vcp_ready_timer[VCP_A_ID].tid = VCP_A_TIMER;
	timer_setup(&(vcp_ready_timer[VCP_A_ID].tl), vcp_wait_ready_timeout, 0);
	vcp_timeout_times = 0;
#endif
	/* vcp platform initialise */
	pr_info("[VCP] %s begins\n", __func__);

	/* vcp ready static flag initialise */
	for (i = 0; i < VCP_CORE_TOTAL ; i++) {
		vcp_enable[i] = 0;
		vcp_ready[i] = 0;
	}
	vcp_dvfs_cali_ready = 0;

	vcp_support = 1;
	if (platform_driver_register(&mtk_vcp_device)) {
		pr_info("[VCP] vcp probe fail\n");
		return -1;
	}

	if (platform_driver_register(&mtk_vcp_io_vdec)) {
		pr_info("[VCP] mtk_vcp_io_vdec probe fail\n");
		return -1;
	}
	if (platform_driver_register(&mtk_vcp_io_venc)) {
		pr_info("[VCP] mtk_vcp_io_venc probe fail\n");
		return -1;
	}
	if (platform_driver_register(&mtk_vcp_io_work)) {
		pr_info("[VCP] mtk_vcp_io_work probe fail\n");
		return -1;
	}

	if (!vcp_support) {
		return 0;
	}

#if VCP_DVFS_INIT_ENABLE
	vcp_dvfs_init();
	wait_vcp_dvfs_init_done();

	/* pll maybe gate, request pll before access any vcp reg/sram */
	vcp_pll_ctrl_set(PLL_ENABLE, CLK_26M);
	/* keep Univpll */
	vcp_resource_req(VCP_REQ_26M);
#endif /* VCP_DVFS_INIT_ENABLE */

	/* skip initial if dts status = "disable" */
	if (!vcp_enable[VCP_A_ID]) {
		pr_notice("[VCP] vcp disabled!!\n");
		goto err;
	}
	/* vcp platform initialise */
	vcp_region_info_init();
	pr_debug("[VCP] platform init\n");
	vcp_awake_init();
	vcp_workqueue = create_singlethread_workqueue("VCP_WQ");
	ret = vcp_excep_init();
	if (ret) {
		pr_notice("[VCP]Excep Init Fail\n");
		goto err;
	}

	INIT_WORK(&vcp_A_notify_work.work, vcp_A_notify_ws);

	vcp_legacy_ipi_init();

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_READY_0,
			(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready0);

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_READY_1,
			(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready1);

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_ERROR_INFO_0,
			(void *)vcp_err_info_handler, NULL, msg_vcp_err_info0);

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_ERROR_INFO_1,
			(void *)vcp_err_info_handler, NULL, msg_vcp_err_info1);

	ret = register_pm_notifier(&vcp_pm_notifier_block);
	if (ret)
		pr_notice("[VCP] failed to register PM notifier %d\n", ret);

	/* vcp sysfs initialise */
	pr_debug("[VCP] sysfs init\n");
	ret = create_files();
	if (unlikely(ret != 0)) {
		pr_notice("[VCP] create files failed\n");
		goto err;
	}

#if VCP_LOGGER_ENABLE
	/* vcp logger initialise */
	pr_debug("[VCP] logger init\n");
	/*create wq for vcp logger*/
	vcp_logger_workqueue = create_singlethread_workqueue("VCP_LOG_WQ");
	if (vcp_logger_init(vcp_get_reserve_mem_virt(VCP_A_LOGGER_MEM_ID),
			vcp_get_reserve_mem_size(VCP_A_LOGGER_MEM_ID)) == -1) {
		pr_notice("[VCP] vcp_logger_init_fail\n");
		goto err;
	}
#endif

#if ENABLE_VCP_EMI_PROTECTION
	set_vcp_mpu();
#endif

	vcp_recovery_init();

#ifdef VCP_PARAMS_TO_VCP_SUPPORT
	/* The function, sending parameters to vcp must be anchored before
	 * 1. disabling 26M, 2. resetting VCP
	 */
	if (params_to_vcp() != 0)
		goto err;
#endif

#if VCP_DVFS_INIT_ENABLE
	/* remember to release pll */
	vcp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif

	driver_init_done = true;
	reset_vcp(VCP_ALL_ENABLE);

#if VCP_DVFS_INIT_ENABLE
	vcp_init_vcore_request();
#endif /* VCP_DVFS_INIT_ENABLE */

	return ret;
err:
#if VCP_DVFS_INIT_ENABLE
	/* remember to release pll */
	vcp_pll_ctrl_set(PLL_DISABLE, CLK_26M);
#endif
	return -1;
}

/*
 * driver exit point
 */
static void __exit vcp_exit(void)
{
#if VCP_BOOT_TIME_OUT_MONITOR
	int i = 0;
#endif

#if VCP_DVFS_INIT_ENABLE
	vcp_dvfs_exit();
#endif

#if VCP_LOGGER_ENABLE
	vcp_logger_uninit();
#endif

	free_irq(vcpreg.irq0, NULL);
	free_irq(vcpreg.irq1, NULL);
	misc_deregister(&vcp_device);

	flush_workqueue(vcp_workqueue);
	destroy_workqueue(vcp_workqueue);

#if VCP_RECOVERY_SUPPORT
	flush_workqueue(vcp_reset_workqueue);
	destroy_workqueue(vcp_reset_workqueue);
#endif

#if VCP_LOGGER_ENABLE
	flush_workqueue(vcp_logger_workqueue);
	destroy_workqueue(vcp_logger_workqueue);
#endif

#if VCP_BOOT_TIME_OUT_MONITOR
	for (i = 0; i < VCP_CORE_TOTAL ; i++)
		del_timer(&vcp_ready_timer[i].tl);
#endif
}

device_initcall_sync(vcp_init);
module_exit(vcp_exit);

MODULE_DESCRIPTION("MEDIATEK Module VCP driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");

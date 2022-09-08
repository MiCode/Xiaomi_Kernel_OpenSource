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
#include <linux/of_platform.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpuidle.h>
//#include <mt-plat/sync_write.h>
//#include <mt-plat/aee.h>
#include <linux/delay.h>
#include <aee.h>
#include <soc/mediatek/smi.h>
#include "vcp_feature_define.h"
#include "vcp_err_info.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_vcpctl.h"
#include "vcp_status.h"
#include "vcp.h"
#include "clk-fmeter.h"

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
#define VCP_30MHZ 30000
#define VCP_READY_TIMEOUT (4 * HZ) /* 4 seconds*/
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
unsigned int vcp_support;
EXPORT_SYMBOL_GPL(vcp_support);

unsigned int vcp_dbg_log;

/* set flag after driver initial done */
bool driver_init_done;
EXPORT_SYMBOL_GPL(driver_init_done);

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

DEFINE_SPINLOCK(vcp_reset_spinlock);

/* l1c enable */
void __iomem *vcp_ap_dram_virt;
void __iomem *vcp_loader_virt;
void __iomem *vcp_regdump_virt;


phys_addr_t vcp_mem_base_phys;
phys_addr_t vcp_mem_base_virt;
phys_addr_t vcp_mem_size;
struct vcp_regs vcpreg;
struct clk *vcpsel;
struct clk *vcpclk;
struct clk *vcp26m;

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

#if VCP_BOOT_TIME_OUT_MONITOR
static unsigned int vcp_timeout_times;
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define vcp_aee_print(string, args...) do {\
	char vcp_name[100];\
	int ret;\
	ret = snprintf(vcp_name, 100, "[VCP] "string, ##args); \
	if (ret > 0)\
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
			vcp_name, "[VCP] error:"string, ##args); \
		pr_info("[VCP] error:"string, ##args);  \
	} while (0)
#else
#define vcp_aee_print(string, args...) \
	pr_info("[VCP] error:"string, ##args)

#endif

static DEFINE_MUTEX(vcp_pw_clk_mutex);
static DEFINE_MUTEX(vcp_A_notify_mutex);
static DEFINE_MUTEX(vcp_feature_mutex);

char *core_ids[VCP_CORE_TOTAL] = {"VCP A"};
DEFINE_SPINLOCK(vcp_awake_spinlock);
struct vcp_ipi_irq {
	const char *name;
	int order;
	unsigned int irq_no;
};

struct vcp_ipi_irq vcp_ipi_irqs[] = {
	/* VCP WDT */
	{ "mediatek,vcp", 0, 0},
	/* VCP reserved */
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
struct device *vcp_io_devs[VCP_IOMMU_DEV_NUM];

struct vcp_status_fp vcp_helper_fp = {
	.vcp_get_reserve_mem_phys	= vcp_get_reserve_mem_phys,
	.vcp_get_reserve_mem_virt	= vcp_get_reserve_mem_virt,
	.vcp_register_feature		= vcp_register_feature,
	.vcp_deregister_feature		= vcp_deregister_feature,
	.is_vcp_ready				= is_vcp_ready,
	.vcp_A_register_notify		= vcp_A_register_notify,
	.vcp_A_unregister_notify	= vcp_A_unregister_notify,
};

#undef pr_debug
#define pr_debug(fmt, arg...) do { \
		if (vcp_dbg_log) \
			pr_info(fmt, ##arg); \
	} while (0)

static void vcp_enable_dapc(void)
{
	struct arm_smccc_res res;
	unsigned long onoff;

	/* Turn on MMuP security */
	onoff = 1;
	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_MMUP_CONTROL,
		onoff, 0, 0, 0, 0, 0, 0, &res);
}

static void vcp_disable_dapc(void)
{
	struct arm_smccc_res res;
	unsigned long onoff;

	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
		MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
		1, 0, 0, 0, 0, 0, &res);
	dsb(SY); /* may take lot of time */

	/* Turn off MMuP security */
	onoff = 0;
	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_MMUP_CONTROL,
		onoff, 0, 0, 0, 0, 0, 0, &res);
}

static void vcp_enable_irqs(void)
{
	int i = 0;

	for (i = 0; i < vcp_mboxdev.count; i++)
		enable_irq(vcp_mboxdev.info_table[i].irq_num);

	enable_irq(vcpreg.irq0);
}

static void vcp_disable_irqs(void)
{
	int i = 0;

	disable_irq(vcpreg.irq0);

	for (i = 0; i < vcp_mboxdev.count; i++)
		disable_irq(vcp_mboxdev.info_table[i].irq_num);
}

static int vcp_ipi_dbg_resume_noirq(struct device *dev)
{
	int i = 0;
	int ret = 0;
	bool state = false;

	for (i = 0; i < IRQ_NUMBER; i++) {
		ret = irq_get_irqchip_state(vcp_ipi_irqs[i].irq_no,
			IRQCHIP_STATE_PENDING, &state);
		if (!ret && state) {
			pr_info("[VCP] ipc%d wakeup\n", i);
			break;
		}
	}

	return 0;
}

static void vcp_wait_awake_count(void)
{
	int i = 0;

	while (vcp_awake_counts[VCP_A_ID] != 0) {
		i += 1;
		mdelay(1);
		if (i > 200) {
			pr_info("wait vcp_awake_counts timeout %d\n", vcp_awake_counts[VCP_A_ID]);
			break;
		}
	}
	pr_info("[VCP] %s wait count: %d\n", __func__, i);
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
	/* for pm disable wait ready, no power check*/
	queue_work(vcp_workqueue, &vcp_ws->work);
}

void vcp_schedule_reset_work(struct vcp_work_struct *vcp_ws)
{
	if (mmup_enable_count() > 0)
		queue_work(vcp_reset_workqueue, &vcp_ws->work);
}

#if VCP_LOGGER_ENABLE
void vcp_schedule_logger_work(struct vcp_work_struct *vcp_ws)
{
	if (mmup_enable_count() > 0)
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

	vcp_recovery_flag[VCP_A_ID] = VCP_A_RECOVERY_OK;
	writel(0xff, VCP_TO_SPM_REG); /* patch: clear SPM interrupt */

	mutex_lock(&vcp_A_notify_mutex);
#if VCP_RECOVERY_SUPPORT
	atomic_set(&vcp_reset_status, RESET_STATUS_STOP);
#endif
	vcp_ready[VCP_A_ID] = 1;

	if (vcp_notify_flag && mmup_enable_count() > 0) {
		pr_debug("[VCP] notify blocking call\n");
		blocking_notifier_call_chain(&vcp_A_notifier_list
			, VCP_EVENT_READY, NULL);
	}
	mutex_unlock(&vcp_A_notify_mutex);

	/*clear reset status and unlock wake lock*/
	pr_debug("[VCP] clear vcp reset flag and unlock\n");
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
	vcp_dump_last_regs(mmup_enable_count());
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

#ifdef VCP_DEBUG_REMOVED
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
#endif

/*
 * @return: 1 if vcp is ready for running tasks
 */
void trigger_vcp_halt(enum vcp_core_id id)
{
	if (mmup_enable_count() && vcp_ready[id]) {
		/* trigger halt isr, force vcp enter wfi */
		writel(B_GIPC4_SETCLR_0, R_GIPC_IN_SET);
	}
}
EXPORT_SYMBOL_GPL(trigger_vcp_halt);

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
 * @return: generaltion count of vcp (reset count)
 */
unsigned int get_vcp_generation(void)
{
	return vcp_reset_counts;
}
EXPORT_SYMBOL_GPL(get_vcp_generation);

uint32_t vcp_wait_ready_sync(enum feature_id id)
{
	int i = 0;
	int j = 0;
	unsigned long c0, c1;

	c0 = readl(VCP_GPR_CORE0_REBOOT);
	c1 = vcpreg.core_nums == 2 ? readl(VCP_GPR_CORE1_REBOOT) :
		CORE_RDY_TO_REBOOT;

	if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT))
		return 0;

	while (!is_vcp_ready(VCP_A_ID)) {
		i += 5;
		mdelay(5);
		if (i > VCP_SYNC_TIMEOUT_MS) {
			vcp_dump_last_regs(1);
			for (j = 0; j < NUM_FEATURE_ID; j++)
				if (feature_table[j].enable)
					pr_info("[VCP] Active feature id %d cnt\n",
						j, feature_table[j].enable);
			mtk_smi_dbg_hang_detect("VCP");
			if (vcp_ee_enable)
				vcp_aee_print("wait ready timeout id %d\n", id);
			else
				pr_info("wait ready timeout id %d\n", id);
			break;
		}
	}

	return i;
}

extern unsigned int mt_get_fmeter_freq(unsigned int id, enum FMETER_TYPE type);
void vcp_enable_pm_clk(enum feature_id id)
{
	int ret = 0;

	if (!vcp_support)
		return;

	mutex_lock(&vcp_pw_clk_mutex);
	if (is_suspending) {
		pr_notice("[VCP] %s blocked %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_unlock(&vcp_pw_clk_mutex);
		while (is_suspending)
			usleep_range(10000, 20000);
		pr_notice("[VCP] %s exit %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_lock(&vcp_pw_clk_mutex);
	}

	if (pwclkcnt == 0) {
		ret = clk_prepare_enable(vcp26m);
		if (ret)
			pr_debug("[VCP] %s: clk_prepare_enable vcp26m\n", __func__);
		ret = clk_prepare_enable(vcpclk);
		if (ret)
			pr_debug("[VCP] %s: clk_prepare_enable vcpclk\n", __func__);
		ret = clk_prepare_enable(vcpsel);
		if (ret)
			pr_debug("[VCP] %s: clk_prepare_enable vcpsel\n", __func__);
		ret = pm_runtime_get_sync(vcp_io_devs[VCP_IOMMU_256MB1]);
		if (ret)
			pr_debug("[VCP] %s: pm_runtime_get_sync\n", __func__);

		vcp_enable_dapc();
		vcp_enable_irqs();

		if (!is_vcp_ready(VCP_A_ID))
			reset_vcp(VCP_ALL_ENABLE);
	}
	pwclkcnt++;
	pr_notice("[VCP] %s id %d done %d clk %d\n", __func__, id,
		pwclkcnt, mt_get_fmeter_freq(10, CKGEN));
	mutex_unlock(&vcp_pw_clk_mutex);

}
EXPORT_SYMBOL_GPL(vcp_enable_pm_clk);

void vcp_disable_pm_clk(enum feature_id id)
{
	int ret = 0;
	int i = 0;
	uint32_t waitCnt = 0;

	if (!vcp_support)
		return;

	mutex_lock(&vcp_pw_clk_mutex);
	if (is_suspending) {
		pr_notice("[VCP] %s blocked %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_unlock(&vcp_pw_clk_mutex);
		while (is_suspending)
			usleep_range(10000, 20000);
		pr_notice("[VCP] %s exit %d %d\n", __func__, pwclkcnt, is_suspending);
		mutex_lock(&vcp_pw_clk_mutex);
	}

	pr_notice("[VCP] %s id %d entered %d ready %d\n", __func__, id,
		pwclkcnt, is_vcp_ready(VCP_A_ID));
	pwclkcnt--;
	if (pwclkcnt == 0) {
#if VCP_RECOVERY_SUPPORT
		/* make sure all reset done */
		flush_workqueue(vcp_reset_workqueue);
#endif
		waitCnt = vcp_wait_ready_sync(id);
		vcp_disable_irqs();
		vcp_ready[VCP_A_ID] = 0;

		/* trigger halt isr, force vcp enter wfi */
		writel(B_GIPC4_SETCLR_1, R_GIPC_IN_SET);
		wait_vcp_wdt_irq_done();

		flush_workqueue(vcp_workqueue);
#if VCP_LOGGER_ENABLE
		vcp_logger_uninit();
		flush_workqueue(vcp_logger_workqueue);
#endif
#if VCP_BOOT_TIME_OUT_MONITOR
		del_timer(&vcp_ready_timer[VCP_A_ID].tl);
#endif
		vcp_wait_core_stop_timeout(1);
		pr_info("[VCP][Debug] bus_dbg_out[0x%x]: 0x%x, waitCnt=%u\n", VCP_BUS_DEBUG_OUT,
			readl(VCP_BUS_DEBUG_OUT), waitCnt);

		vcp_disable_dapc();
		mutex_lock(&vcp_A_notify_mutex);
		vcp_extern_notify(VCP_EVENT_STOP);
		mutex_unlock(&vcp_A_notify_mutex);

		vcp_wait_awake_count();

		ret = pm_runtime_put_sync(vcp_io_devs[VCP_IOMMU_256MB1]);
		if (ret)
			pr_debug("[VCP] %s: pm_runtime_put_sync\n", __func__);
		clk_disable_unprepare(vcpsel);
		clk_disable_unprepare(vcpclk);
		clk_disable_unprepare(vcp26m);
	}
	if (pwclkcnt < 0) {
		for (i = 0; i < NUM_FEATURE_ID; i++)
			pr_info("[VCP][Warning] %s Check feature id %d enable cnt %d\n",
				__func__, feature_table[i].feature, feature_table[i].enable);
		pwclkcnt = 0;
	}
	mutex_unlock(&vcp_pw_clk_mutex);

}
EXPORT_SYMBOL_GPL(vcp_disable_pm_clk);

static int vcp_pm_event(struct notifier_block *notifier
			, unsigned long pm_event, void *unused)
{
	int retval = 0;
	uint32_t waitCnt = 0;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&vcp_A_notify_mutex);
		vcp_extern_notify(VCP_EVENT_SUSPEND);
		mutex_unlock(&vcp_A_notify_mutex);

		mutex_lock(&vcp_pw_clk_mutex);
		pr_notice("[VCP] PM_SUSPEND_PREPARE entered %d %d\n", pwclkcnt, is_suspending);
		if ((!is_suspending) && pwclkcnt) {
			is_suspending = true;
#if VCP_RECOVERY_SUPPORT
			/* make sure all reset done */
			flush_workqueue(vcp_reset_workqueue);
#endif
			waitCnt = vcp_wait_ready_sync(RTOS_FEATURE_ID);
			vcp_disable_irqs();
			vcp_ready[VCP_A_ID] = 0;

			/* trigger halt isr, force vcp enter wfi */
			writel(B_GIPC4_SETCLR_1, R_GIPC_IN_SET);
			wait_vcp_wdt_irq_done();

			flush_workqueue(vcp_workqueue);
#if VCP_LOGGER_ENABLE
			vcp_logger_uninit();
			flush_workqueue(vcp_logger_workqueue);
#endif
#if VCP_BOOT_TIME_OUT_MONITOR
			del_timer(&vcp_ready_timer[VCP_A_ID].tl);
#endif
			vcp_wait_core_stop_timeout(1);
			vcp_disable_dapc();

			vcp_wait_awake_count();

			retval = pm_runtime_put_sync(vcp_io_devs[VCP_IOMMU_256MB1]);
			if (retval)
				pr_debug("[VCP] %s: pm_runtime_put_sync\n", __func__);
			clk_disable_unprepare(vcpsel);
			clk_disable_unprepare(vcpclk);
			clk_disable_unprepare(vcp26m);
		}
		is_suspending = true;
		mutex_unlock(&vcp_pw_clk_mutex);

		// SMC call to TFA / DEVAPC
		// arm_smccc_smc(MTK_SIP_KERNEL_VCP_CONTROL, MTK_TINYSYS_VCP_KERNEL_OP_XXX,
		// 0, 0, 0, 0, 0, 0, &res);
		pr_debug("[VCP] PM_SUSPEND_PREPARE ok, waitCnt=%u\n", waitCnt);

		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		mutex_lock(&vcp_pw_clk_mutex);
		pr_notice("[VCP] PM_POST_SUSPEND entered %d %d\n", pwclkcnt, is_suspending);
		if (is_suspending && pwclkcnt) {
			retval = clk_prepare_enable(vcp26m);
			if (retval)
				pr_debug("[VCP] %s: clk_prepare_enable vcp26m\n", __func__);
			retval = clk_prepare_enable(vcpclk);
			if (retval)
				pr_debug("[VCP] %s: clk_prepare_enable vcpclk\n", __func__);
			retval = clk_prepare_enable(vcpsel);
			if (retval)
				pr_debug("[VCP] %s: clk_prepare_enable vcpsel\n", __func__);
			retval = pm_runtime_get_sync(vcp_io_devs[VCP_IOMMU_256MB1]);
			if (retval)
				pr_debug("[VCP] %s: pm_runtime_get_sync\n", __func__);

			vcp_enable_dapc();
			vcp_enable_irqs();
#if VCP_RECOVERY_SUPPORT
			cpuidle_pause_and_lock();
			reset_vcp(VCP_ALL_SUSPEND);
			is_suspending = false;
			waitCnt = vcp_wait_ready_sync(RTOS_FEATURE_ID);
			cpuidle_resume_and_unlock();
#endif
		}
		is_suspending = false;
		mutex_unlock(&vcp_pw_clk_mutex);

		// SMC call to TFA / DEVAPC
		// arm_smccc_smc(MTK_SIP_KERNEL_VCP_CONTROL, MTK_TINYSYS_VCP_KERNEL_OP_XXX,
		// 0, 0, 0, 0, 0, 0, &res);
		pr_debug("[VCP] PM_POST_SUSPEND ok, waitCnt=%u\n", waitCnt);

		return NOTIFY_OK;
	case PM_POST_HIBERNATION:
		pr_debug("[VCP] %s: reboot\n", __func__);
		retval = reset_vcp(VCP_ALL_REBOOT);
		if (retval < 0) {
			retval = -EINVAL;
			pr_debug("[VCP] %s: reboot fail\n", __func__);
		}
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static struct notifier_block vcp_pm_notifier_block = {
	.notifier_call = vcp_pm_event,
	.priority = 0,
};

void vcp_set_clk(void)
{
	int ret, i;
	int clk_retry = 10;

	for (i = 0; i < clk_retry; i++) {
		ret = clk_set_parent(vcpsel, vcp26m);	/* guarantee status sync */
		if (ret)
			pr_notice("[VCP] %s: Failed to set parent %s of %s ret %d\n", __func__,
				__clk_get_name(vcp26m), __clk_get_name(vcpsel), ret);

		ret = clk_set_parent(vcpsel, vcpclk);
		if (ret)
			pr_notice("[VCP] %s: Failed to set parent %s of %s ret %d\n", __func__,
				__clk_get_name(vcpclk), __clk_get_name(vcpsel), ret);

		ret = mt_get_fmeter_freq(vcpreg.femter_ck, CKGEN);
		if (ret > VCP_30MHZ) {	/* fmeter 5% deviation */
			/* or fail dump something */
			break;
		}
	}

	if (ret < VCP_30MHZ)
		pr_notice("[VCP] %s: fail clk %d(%d) clk_retry %d\n", __func__,
		ret, mt_get_fmeter_freq(vcpreg.femter_ck, CKGEN), i);
}


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
	struct arm_smccc_res res;

	if (reset & 0x0f) { /* do reset */
		/* make sure vcp is in idle state */
		vcp_wait_core_stop_timeout(mmup_enable_count());
	}
	if (vcp_enable[VCP_A_ID]) {
		/* write vcp reserved memory address/size to GRP1/GRP2
		 * to let vcp setup MPU
		 */
		writel((unsigned int)VCP_PACK_IOVA(vcp_mem_base_phys), DRAM_RESV_ADDR_REG);
		writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG);
		vcp_set_clk();

#if VCP_BOOT_TIME_OUT_MONITOR
		vcp_ready_timer[VCP_A_ID].tl.expires = jiffies + VCP_READY_TIMEOUT;
		add_timer(&vcp_ready_timer[VCP_A_ID].tl);
#endif
		if (reset == VCP_ALL_SUSPEND) {
			arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
				MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
				0, 0, 0, 0, 0, 0, &res);
		} else {
			arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
				MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
				1, 0, 0, 0, 0, 0, &res);
		}

		pr_notice("[VCP] %s: CORE0_RSTN_CLR %x %x %x ret %lu clk %d\n", __func__,
			readl(DRAM_RESV_ADDR_REG), readl(DRAM_RESV_SIZE_REG),
			readl(R_CORE0_SW_RSTN_CLR), res.a0,
			mt_get_fmeter_freq(vcpreg.femter_ck, CKGEN));

	}
	pr_debug("[VCP] %s: done\n", __func__);

	return 0;
}

static inline ssize_t vcp_register_on_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0)
		if (value == 666)
			vcp_register_feature(RTOS_FEATURE_ID);

	return count;
}
DEVICE_ATTR_WO(vcp_register_on);

static inline ssize_t vcp_deregister_off_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value = 0;

	if (!buf || count == 0)
		return count;

	if (kstrtouint(buf, 10, &value) == 0)
		if (value == 666)
			vcp_deregister_feature(RTOS_FEATURE_ID);

	return count;
}
DEVICE_ATTR_WO(vcp_deregister_off);

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

	vcp_dump_last_regs(mmup_enable_count());
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
		vcp_dbg_log = value;
		pr_debug("[VCP] vcp_ee_enable(vcp_dbg_log) = %d(1:enable, 0:disable)\n"
				, vcp_ee_enable);
	}
	return n;
}
DEVICE_ATTR_RW(vcp_ee_enable);

#ifdef VCP_DEBUG_NODE_ENABLE
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

	ret = device_create_file(vcp_device.this_device
					, &dev_attr_vcp_ee_enable);
	if (unlikely(ret != 0))
		return ret;

#ifdef VCP_DEBUG_NODE_ENABLE
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
					&dev_attr_vcp_register_on);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(vcp_device.this_device,
					&dev_attr_vcp_deregister_off);
	if (unlikely(ret != 0))
		return ret;

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


struct device *vcp_get_io_device(enum VCP_IOMMU_DEV io_num)
{
	return ((io_num >= VCP_IOMMU_DEV_NUM) ? NULL:vcp_io_devs[io_num]);
}
EXPORT_SYMBOL_GPL(vcp_get_io_device);


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
	if (id < NUMS_MEM_ID)
		return vcp_reserve_mblock[id].size;

	pr_notice("[VCP] no reserve memory for %d", id);
	return 0;
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
	unsigned int vcp_mem_num = 0;
	unsigned int i = 0, m_idx = 0, m_size = 0;
	int ret;
#if VCP_IOMMU_ENABLE
	uint64_t iova_upper = 0;
	uint64_t iova_lower = 0xFFFFFFFFFFFFFFFF;
#else
	struct device_node *rmem_node;
	struct reserved_mem *rmem;
	const char *mem_key;
#endif

	if (num != NUMS_MEM_ID) {
		pr_notice("[VCP] number of entries of reserved memory %u / %u\n",
			num, NUMS_MEM_ID);
		BUG_ON(1);
		return -1;
	}

#if (!VCP_IOMMU_ENABLE)
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
	vcp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(vcp_mem_base_phys,
		vcp_mem_size);
	pr_debug("[VCP] rsrv_phy_base = 0x%llx, len:0x%llx\n",
		(uint64_t)vcp_mem_base_phys, (uint64_t)vcp_mem_size);
	pr_debug("[VCP] rsrv_vir_base = 0x%llx, len:0x%llx\n",
		(uint64_t)vcp_mem_base_virt, (uint64_t)vcp_mem_size);
#endif	//(!VCP_IOMMU_ENABLE)

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

#if VCP_IOMMU_ENABLE
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_info(&pdev->dev, "64-bit DMA enable failed\n");
		return ret;
	}
	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(64));
		if (ret)
			dev_info(&pdev->dev, "Failed to set DMA segment size\n");
	}

	for (id = 0; id < NUMS_MEM_ID; id++) {
		vcp_reserve_mblock[id].start_virt =
			(__u64)dma_alloc_coherent(&pdev->dev, vcp_reserve_mblock[id].size,
				&vcp_reserve_mblock[id].start_phys,
				GFP_KERNEL);
		accumlate_memory_size += vcp_reserve_mblock[id].size;

		if (vcp_reserve_mblock[id].start_phys < iova_lower)
			iova_lower = vcp_reserve_mblock[id].start_phys;
		if ((vcp_reserve_mblock[id].start_phys +  vcp_reserve_mblock[id].size) > iova_upper)
			iova_upper = vcp_reserve_mblock[id].start_phys +
				vcp_reserve_mblock[id].size;

		pr_debug("[VCP] [%d] iova:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)vcp_reserve_mblock[id].start_phys,
			(uint64_t)vcp_reserve_mblock[id].start_virt,
			(uint64_t)vcp_reserve_mblock[id].size);

	}
	vcp_mem_base_phys = (phys_addr_t)iova_lower;
	vcp_mem_size = (phys_addr_t)iova_upper - iova_lower;
#else
	for (id = 0; id < NUMS_MEM_ID; id++) {
		vcp_reserve_mblock[id].start_phys = vcp_mem_base_phys +
			accumlate_memory_size;
		vcp_reserve_mblock[id].start_virt = vcp_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += vcp_reserve_mblock[id].size;
		pr_debug("[VCP] [%d] phys:0x%llx, virt:0x%llx, len:0x%llx\n",
			id, (uint64_t)vcp_reserve_mblock[id].start_phys,
			(uint64_t)vcp_reserve_mblock[id].start_virt,
			(uint64_t)vcp_reserve_mblock[id].size);
	}
#ifdef VCP_DEBUG_NODE_ENABLE
	BUG_ON(accumlate_memory_size > vcp_mem_size);
#endif
#endif  // VCP_IOMMU_ENABLE

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

	/* because feature_table is a global variable,
	 * use mutex lock to protect it from accessing in the same time
	 */
	mutex_lock(&vcp_feature_mutex);
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id)
			feature_table[i].enable++;
	}
	vcp_enable_pm_clk(id);
	mutex_unlock(&vcp_feature_mutex);
}
EXPORT_SYMBOL_GPL(vcp_register_feature);

void vcp_deregister_feature(enum feature_id id)
{
	uint32_t i;

	mutex_lock(&vcp_feature_mutex);
	for (i = 0; i < NUM_FEATURE_ID; i++) {
		if (feature_table[i].feature == id) {
			if (feature_table[i].enable == 0) {
				pr_info("[VCP][Warning] %s unbalanced feature id %d enable cnt %d\n",
					__func__, id, feature_table[i].enable);
				mutex_unlock(&vcp_feature_mutex);
				return;
			}
			feature_table[i].enable--;
		}
	}
	vcp_disable_pm_clk(id);
	mutex_unlock(&vcp_feature_mutex);
}
EXPORT_SYMBOL_GPL(vcp_deregister_feature);

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

void vcp_wait_core_stop_timeout(int mmup_enable)
{
	uint32_t core0_halt = 0;
	uint32_t core1_halt = 0;
	uint32_t val = 0;
	/* make sure vcp is in idle state */
	int timeout = 500; /* max wait 0.5s */

	if (mmup_enable == 0) {
		pr_notice("[VCP] power off, do not wait reset done\n");
		return;
	}

	while (timeout--) {
		val = readl(R_CORE0_STATUS);
		core0_halt = (val == (B_CORE_GATED | B_HART0_HALT | B_HART1_HALT));
		val = readl(R_CORE1_STATUS);
		core1_halt = (vcpreg.core_nums == 2) ?
			(val == (B_CORE_GATED | B_HART0_HALT | B_HART1_HALT)) : 1;

		pr_debug("[VCP] debug CORE_STATUS vcp: 0x%x, 0x%x\n",
			readl(R_CORE0_STATUS), readl(R_CORE1_STATUS));
		if (core0_halt && core1_halt) {
			/* VCP stops any activities
			 * and parks at wfi
			 */
			break;
		}
		mdelay(1);
	}

	if (timeout == 0)
		pr_notice("[VCP] reset timeout, still reset vcp: 0x%x, 0x%x\n",
			readl(R_CORE0_STATUS), readl(R_CORE1_STATUS));

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
	struct arm_smccc_res res;

	pr_notice("[VCP] %s(): vcp_reset_type %d remain %x times, en_cnt %d\n",
		__func__, vcp_reset_type, vcp_reset_counts, mmup_enable_count());

	if (mmup_enable_count() == 0)
		return;

	/*
	 *   vcp_ready:
	 *   VCP_PLATFORM_STOP  = 0,
	 *   VCP_PLATFORM_READY = 1,
	 */
	vcp_ready[VCP_A_ID] = 0;

	/*workqueue for vcp ee, vcp reset by cmd will not trigger vcp ee*/
	if (vcp_reset_by_cmd == 0 && vcp_ee_enable) {
		vcp_aee_print("[VCP] %s(): vcp_reset_type %d remain %x times, encnt %d\n",
			__func__, vcp_reset_type, vcp_reset_counts, mmup_enable_count());
	}
	pr_debug("[VCP] %s(): disable logger\n", __func__);
	/* logger disable must after vcp_aed() */
	vcp_logger_init_set(0);

	/* vcp reset by CMD, WDT or awake fail */
	if ((vcp_reset_type == RESET_TYPE_TIMEOUT) ||
		(vcp_reset_type == RESET_TYPE_AWAKE)) {
		/* stop vcp */
		arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
			0, 0, 0, 0, 0, 0, &res);

		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %x core1 %x ret %lu\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET), res.a0);
	} else {
		/* reset type vcp WDT or CMD*/
		/* make sure vcp is in idle state */
		vcp_wait_core_stop_timeout(mmup_enable_count());
		arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_SET,
			1, 0, 0, 0, 0, 0, &res);

		dsb(SY); /* may take lot of time */
		pr_notice("[VCP] rstn core0 %x core1 %x ret %lu\n",
		readl(R_CORE0_SW_RSTN_SET), readl(R_CORE1_SW_RSTN_SET), res.a0);
	}

	/*notify vcp functions stop*/
	pr_debug("[VCP] %s(): vcp_extern_notify\n", __func__);

	mutex_lock(&vcp_A_notify_mutex);
	vcp_extern_notify(VCP_EVENT_STOP);
	mutex_unlock(&vcp_A_notify_mutex);

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
	writel((unsigned int)VCP_PACK_IOVA(vcp_mem_base_phys), DRAM_RESV_ADDR_REG);
	writel((unsigned int)vcp_mem_size, DRAM_RESV_SIZE_REG);
	vcp_set_clk();

	/* start vcp */
	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_RESET_RELEASE,
			1, 0, 0, 0, 0, 0, &res);

	pr_notice("[VCP] %s: CORE0_RSTN_CLR %x %x %x ret %lu clk %d\n", __func__,
		readl(DRAM_RESV_ADDR_REG), readl(DRAM_RESV_SIZE_REG),
		readl(R_CORE0_SW_RSTN_CLR), res.a0,
		mt_get_fmeter_freq(vcpreg.femter_ck, CKGEN));

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

#ifdef VCP_DEBUG_REMOVED
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
	u32 i, ret, mbox_id = 0, recv_opt = 0;
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

	return true;
}

static int vcp_io_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	pr_debug("[VCP_IO] %s", __func__);

	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0 || vcp_support == 1) {
		pr_info("Bypass the VCP driver probe\n");
		return -1;
	}
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_info(&pdev->dev, "64-bit DMA enable failed\n");
		return ret;
	}
	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(64));
		if (ret)
			dev_info(&pdev->dev, "Failed to set DMA segment size\n");
	}

	// VCP iommu devices
	vcp_io_devs[vcp_support-1] = dev;

	return 0;
}

static int vcp_io_device_remove(struct platform_device *dev)
{
	return 0;
}

void mbox_setup_pin_table(unsigned int mbox)
{
	int i, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;

	for (i = 0; i < vcp_mboxdev.send_count; i++) {
		if (mbox == vcp_mbox_pin_send[i].mbox) {
			vcp_mbox_pin_send[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_send[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_send[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_send[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_send[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_send[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < vcp_mboxdev.recv_count; i++) {
		if (mbox == vcp_mbox_pin_recv[i].mbox) {
			vcp_mbox_pin_recv[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_recv[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_recv[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_recv[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_recv[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_recv[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
	   (last_ofs + last_slot) > (vcp_mbox_info[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

static int vcp_device_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct resource *res;
	const char *core_status = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node;
	const char *clk_name;
	struct device_link	*link;
	struct device_node	*smi_node;
	struct platform_device	*psmi_com_dev;

	pr_debug("[VCP] %s", __func__);

	pm_runtime_enable(&pdev->dev);
	of_property_read_u32(pdev->dev.of_node, "vcp-support",
		 &vcp_support);
	if (vcp_support == 0) {
		pr_info("Bypass VCP driver probe\n");
		return 0;
	} else {
		// VCP iommu devices
		vcp_io_devs[vcp_support-1] = dev;
		if (vcp_support > 1)
			return 0;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_sram_base");
	vcpreg.sram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.sram)) {
		pr_notice("[VCP] vcpreg.sram error\n");
		return -1;
	}
	if (res == NULL) {
		pr_notice("[VCP] platform_get_resource_byname error\n");
		return -1;
	}
	vcpreg.total_tcmsize = (unsigned int)resource_size(res);
	pr_debug("[VCP] sram base = 0x%p %x\n"
		, vcpreg.sram, vcpreg.total_tcmsize);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg");
	vcpreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg)) {
		pr_notice("[VCP] vcpreg.cfg error\n");
		return -1;
	}
	pr_debug("[VCP] cfg base = 0x%p\n", vcpreg.cfg);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_core0");
	vcpreg.cfg_core0 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core0)) {
		pr_debug("[VCP] vcpreg.cfg_core0 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core0 base = 0x%p\n", vcpreg.cfg_core0);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_core1");
	vcpreg.cfg_core1 = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_core1)) {
		pr_debug("[VCP] vcpreg.cfg_core1 error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_core1 base = 0x%p\n", vcpreg.cfg_core1);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_bus_tracker");
	vcpreg.bus_tracker = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.bus_tracker)) {
		pr_debug("[VCP] vcpreg.bus_tracker error\n");
		return -1;
	}
	pr_debug("[VCP] bus_tracker base = 0x%p\n", vcpreg.bus_tracker);

#ifdef VCP_DEBUG_REMOVED
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_l1creg");
	vcpreg.l1cctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.l1cctrl)) {
		pr_debug("[VCP] vcpreg.l1cctrl error\n");
		return -1;
	}
	pr_debug("[VCP] l1cctrl base = 0x%p\n", vcpreg.l1cctrl);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_sec");
	vcpreg.cfg_sec = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_sec)) {
		pr_debug("[VCP] vcpreg.cfg_sec error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_sec base = 0x%p\n", vcpreg.cfg_sec);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vcp_cfgreg_mmu");
	vcpreg.cfg_mmu = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) vcpreg.cfg_mmu)) {
		pr_debug("[VCP] vcpreg.cfg_mmu error\n");
		return -1;
	}
	pr_debug("[VCP] cfg_mmu base = 0x%p\n", vcpreg.cfg_mmu);
#endif

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

	of_property_read_u32(pdev->dev.of_node, "femter_ck"
						, &vcpreg.femter_ck);
	of_property_read_u32(pdev->dev.of_node, "twohart"
						, &vcpreg.twohart);
	pr_notice("[VCP] vcpreg.twohart = %d,  vcpreg.femter_ck = %d\n",
		vcpreg.twohart, vcpreg.femter_ck);

	vcpreg.irq0 = platform_get_irq_byname(pdev, "wdt");
	if (vcpreg.irq0 < 0)
		pr_notice("[VCP] wdt irq not exist\n");
	else {
		pr_debug("wdt %d\n", vcpreg.irq0);
		ret = request_irq(vcpreg.irq0, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP wdt", NULL);
		if (ret < 0)
			pr_notice("[VCP] wdt require fail %d %d\n",
				vcpreg.irq0, ret);
		else {
			ret = enable_irq_wake(vcpreg.irq0);
			if (ret < 0)
				pr_notice("[VCP] wdt wake fail:%d,%d\n",
					vcpreg.irq0, ret);
		}
	}

	vcpreg.irq1 = platform_get_irq_byname(pdev, "reserved");
	if (vcpreg.irq1 < 0)
		pr_notice("[VCP] reserved irq not exist\n");
	else {
		pr_debug("reserved %d\n", vcpreg.irq1);
		ret = request_irq(vcpreg.irq1, vcp_A_irq_handler,
			IRQF_TRIGGER_NONE, "VCP reserved", NULL);
		if (ret < 0)
			pr_notice("[VCP] reserved require irq fail %d %d\n",
				vcpreg.irq1, ret);
		else {
			ret = enable_irq_wake(vcpreg.irq1);
			if (ret < 0)
				pr_notice("[VCP] reserved wake fail:%d,%d\n",
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
	/* device link to SMI for Dram iommu */
	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	psmi_com_dev = of_find_device_by_node(smi_node);
	if (psmi_com_dev) {
		link = device_link_add(dev, &psmi_com_dev->dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		pr_info("[VCP] device link to %s\n", dev_name(&psmi_com_dev->dev));
		if (!link) {
			dev_info(dev, "Unable to link Dram %s.\n",
				dev_name(&psmi_com_dev->dev));
			ret = -EINVAL;
			return ret;
		}
	}

	/* device link to SMI for SLB/Infra */
	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 1);
	psmi_com_dev = of_find_device_by_node(smi_node);
	if (psmi_com_dev) {
		link = device_link_add(dev, &psmi_com_dev->dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		pr_info("[VCP] device link to %s\n", dev_name(&psmi_com_dev->dev));
		if (!link) {
			dev_info(dev, "Unable to link SLB/Infra %s.\n",
				dev_name(&psmi_com_dev->dev));
			ret = -EINVAL;
			return ret;
		}
	}

	ret = of_property_read_string_index(
				pdev->dev.of_node, "clock-names", 0, &clk_name);
	vcpsel = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(vcpsel)) {
		pr_info("[VCP] Unable to devm_clk_get id: %d, name: %s\n",
			0, clk_name);
		return PTR_ERR(vcpsel);
	}
	ret = of_property_read_string_index(
				pdev->dev.of_node, "clock-names", 1, &clk_name);
	vcpclk = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(vcpclk)) {
		pr_info("[VCP] Unable to devm_clk_get id: %d, name: %s\n",
			1, clk_name);
		return PTR_ERR(vcpclk);
	}
	ret = of_property_read_string_index(
				pdev->dev.of_node, "clock-names", 2, &clk_name);
	vcp26m = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(vcp26m)) {
		pr_info("[VCP] Unable to devm_clk_get id: %d, name: %s\n",
			2, clk_name);
		return PTR_ERR(vcp26m);
	}

	pr_info("[VCP] %s done\n", __func__);

	return ret;
}

static int vcp_device_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

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

static int mtk_vcp_suspend(struct device *pdev)
{
	pr_notice("%s done", __func__);
	return 0;
}

static int mtk_vcp_resume(struct device *pdev)
{
	pr_notice("%s done", __func__);
	return 0;
}

static const struct dev_pm_ops mtk_vcp_pm_ops = {
	.suspend = mtk_vcp_suspend,
	.resume = mtk_vcp_resume,
};

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
		.pm = &mtk_vcp_pm_ops,
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
static const struct of_device_id vcp_ube_lat_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-ube-lat", },
	{}
};
static const struct of_device_id vcp_ube_core_of_ids[] = {
	{ .compatible = "mediatek,vcp-io-ube-core", },
	{}
};

static struct platform_driver mtk_vcp_io_vdec = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_vdec",
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

static struct platform_driver mtk_vcp_io_ube_lat = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_ube_lat",
		.owner = THIS_MODULE,
		.of_match_table = vcp_ube_lat_of_ids,
	},
};

static struct platform_driver mtk_vcp_io_ube_core = {
	.probe = vcp_io_device_probe,
	.remove = vcp_io_device_remove,
	.driver = {
		.name = "vcp_io_ube_core",
		.owner = THIS_MODULE,
		.of_match_table = vcp_ube_core_of_ids,
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
	vcp_support = 1;
	vcp_dbg_log = 0;

	/* vco io device initialise */
	for (i = 0; i < VCP_IOMMU_DEV_NUM; i++)
		vcp_io_devs[i] = NULL;

	vcp_set_fp(&vcp_helper_fp);

	if (platform_driver_register(&mtk_vcp_device)) {
		pr_info("[VCP] vcp probe fail\n");
		goto err_vcp;
	}

	if (platform_driver_register(&mtk_vcp_io_ube_lat)) {
		pr_info("[VCP] mtk_vcp_io_ube_lat  not exist\n");
		goto err_io_ube_lat;
	}
	if (platform_driver_register(&mtk_vcp_io_ube_core)) {
		pr_info("[VCP] mtk_vcp_io_ube_core not exist\n");
		goto err_io_ube_core;
	}
	if (platform_driver_register(&mtk_vcp_io_vdec)) {
		pr_info("[VCP] mtk_vcp_io_vdec probe fail\n");
		goto err_io_vdec;
	}
	if (platform_driver_register(&mtk_vcp_io_venc)) {
		pr_info("[VCP] mtk_vcp_io_venc probe fail\n");
		goto err_io_venc;
	}
	if (platform_driver_register(&mtk_vcp_io_work)) {
		pr_info("[VCP] mtk_vcp_io_work probe fail\n");
		goto err_io_work;
	}

	if (!vcp_support) {
		return 0;
	}

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

#ifdef VCP_DEBUG_REMOVED
	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_READY_0,
			(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready0);
#endif
	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_READY_1,
			(void *)vcp_A_ready_ipi_handler, NULL, &msg_vcp_ready1);
#ifdef VCP_DEBUG_REMOVED
	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_ERROR_INFO_0,
			(void *)vcp_err_info_handler, NULL, msg_vcp_err_info0);

	mtk_ipi_register(&vcp_ipidev, IPI_IN_VCP_ERROR_INFO_1,
			(void *)vcp_err_info_handler, NULL, msg_vcp_err_info1);
#endif
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
	vcp_disable_irqs();
	driver_init_done = true;
	is_suspending = false;
	pwclkcnt = 0;

	return ret;
err:
	platform_driver_unregister(&mtk_vcp_io_work);
err_io_work:
	platform_driver_unregister(&mtk_vcp_io_venc);
err_io_venc:
	platform_driver_unregister(&mtk_vcp_io_vdec);
err_io_vdec:
	platform_driver_unregister(&mtk_vcp_io_ube_core);
err_io_ube_core:
	platform_driver_unregister(&mtk_vcp_io_ube_lat);
err_io_ube_lat:
	platform_driver_unregister(&mtk_vcp_device);
err_vcp:
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
	platform_driver_unregister(&mtk_vcp_io_work);
	platform_driver_unregister(&mtk_vcp_io_venc);
	platform_driver_unregister(&mtk_vcp_io_vdec);
	platform_driver_unregister(&mtk_vcp_io_ube_core);
	platform_driver_unregister(&mtk_vcp_io_ube_lat);
	platform_driver_unregister(&mtk_vcp_device);
}

device_initcall_sync(vcp_init);
module_exit(vcp_exit);

MODULE_DESCRIPTION("MEDIATEK Module VCP driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");

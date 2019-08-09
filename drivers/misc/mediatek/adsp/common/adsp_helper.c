/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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
#include <linux/debugfs.h>
#include <linux/syscore_ops.h>
#ifdef wakelock
#include <linux/wakelock.h>
#endif
#include <linux/io.h>
#include <linux/clk.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "adsp_feature_define.h"
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"
#include "adsp_clk.h"
#include "adsp_service.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
//#include <mt-plat/mtk_memcfg.h>
#include "adsp_reservedmem_define.h"
#endif
#include <mtk_sys_timer.h>
#include <mtk_sys_timer_typedefs.h>
#include <mtk_spm_sleep.h>
#include <plat_debug_api.h>

/* adsp awake timout count definition*/
#define ADSP_AWAKE_TIMEOUT 5000
/* adsp semaphore timout count definition*/
#define SEMAPHORE_TIMEOUT 5000
/* adsp ready timout definition*/
#define ADSP_READY_TIMEOUT (40 * HZ) /* 40 seconds*/
#define ADSP_A_TIMER 0

/* adsp ready status for notify*/
unsigned int adsp_ready[ADSP_CORE_TOTAL];

/* adsp enable status*/
unsigned int adsp_enable[ADSP_CORE_TOTAL];

/* adsp dvfs variable*/
unsigned int adsp_expected_freq;
unsigned int adsp_current_freq;

#ifdef CFG_RECOVERY_SUPPORT
static unsigned int adsp_timeout_times;
static struct workqueue_struct *adsp_reset_workqueue;
#endif
unsigned int adsp_recovery_flag[ADSP_CORE_TOTAL];
atomic_t adsp_reset_status = ATOMIC_INIT(ADSP_RESET_STATUS_STOP);
unsigned int adsp_reset_by_cmd;
struct completion adsp_sys_reset_cp;
struct adsp_work_struct adsp_sys_reset_work;
struct wakeup_source adsp_reset_lock;
DEFINE_SPINLOCK(adsp_reset_spinlock);

phys_addr_t adsp_mem_base_phys;
phys_addr_t adsp_mem_base_virt;
phys_addr_t adsp_mem_size;

static struct adsp_mpu_info_t *adsp_mpu_info;

struct adsp_regs adspreg;

unsigned char *adsp_send_buff[ADSP_CORE_TOTAL];
unsigned char *adsp_recv_buff[ADSP_CORE_TOTAL];

static struct workqueue_struct *adsp_workqueue;
#if ADSP_BOOT_TIME_OUT_MONITOR
static struct timer_list adsp_ready_timer[ADSP_CORE_TOTAL];
#endif
static struct adsp_work_struct adsp_A_notify_work;
static struct adsp_work_struct adsp_timeout_work;

static DEFINE_MUTEX(adsp_A_notify_mutex);

char *adsp_core_ids[ADSP_CORE_TOTAL] = {"ADSP A"};
DEFINE_SPINLOCK(adsp_awake_spinlock);
/* set flag after driver initial done */
static bool driver_init_done;
unsigned char **adsp_swap_buf;
static struct dentry *adsp_debugfs;

struct mem_desc_t {
	u64 start;
	u64 size;
};

/*
 * memory copy to adsp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_to_adsp(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}


/*
 * memory copy from adsp sram
 * @param trg: trg address
 * @param src: src address
 * @param size: memory size
 */
void memcpy_from_adsp(void *trg, const void __iomem *src, int size)
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
int get_adsp_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&adsp_awake_spinlock, spin_flags);

	flag = (flag * 2) + 1;

	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 0) {
		writel((1 << flag), ADSP_SEMAPHORE);

		while (count != SEMAPHORE_TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), ADSP_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_debug("[ADSP] get adsp sema. %d TIMEOUT..!\n", flag);
	} else
		pr_debug("[ADSP] already hold adsp sema. %d\n", flag);

	spin_unlock_irqrestore(&adsp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(get_adsp_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return  1 :release sema success
 *        -1 :release sema fail
 */
int release_adsp_semaphore(int flag)
{
	int read_back;
	int ret = -1;
	unsigned long spin_flags;

	/* return 1 to prevent from access when driver not ready */
	if (!driver_init_done)
		return -1;

	/* spinlock context safe*/
	spin_lock_irqsave(&adsp_awake_spinlock, spin_flags);
	flag = (flag * 2) + 1;

	read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;

	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), ADSP_SEMAPHORE);
		read_back = (readl(ADSP_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_debug("[ADSP] %s %d failed\n", __func__, flag);
	} else
		pr_debug("[ADSP] %s %d not own by me\n", __func__, flag);

	spin_unlock_irqrestore(&adsp_awake_spinlock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(release_adsp_semaphore);


static BLOCKING_NOTIFIER_HEAD(adsp_A_notifier_list);
/*
 * register apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:   notifier block struct
 */
void adsp_A_register_notify(struct notifier_block *nb)
{
	mutex_lock(&adsp_A_notify_mutex);
	blocking_notifier_chain_register(&adsp_A_notifier_list, nb);

	pr_debug("[ADSP] register adsp A notify callback..\n");

	if (is_adsp_ready(ADSP_A_ID) == 1)
		nb->notifier_call(nb, ADSP_EVENT_READY, NULL);
	mutex_unlock(&adsp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(adsp_A_register_notify);
/*
 * unregister apps notification
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param nb:     notifier block struct
 */
void adsp_A_unregister_notify(struct notifier_block *nb)
{
	mutex_lock(&adsp_A_notify_mutex);
	blocking_notifier_chain_unregister(&adsp_A_notifier_list, nb);
	mutex_unlock(&adsp_A_notify_mutex);
}
EXPORT_SYMBOL_GPL(adsp_A_unregister_notify);

#ifdef CFG_RECOVERY_SUPPORT
static int adsp_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	adsp_read_status_release(event);
	return 0;
}

static struct notifier_block adsp_ready_notifier1 = {
	.notifier_call = adsp_event_receive,
	.priority = AUDIO_HAL_FEATURE_PRI,
};
#endif

void adsp_schedule_work(struct adsp_work_struct *adsp_ws)
{
	queue_work(adsp_workqueue, &adsp_ws->work);
}

#ifdef CFG_RECOVERY_SUPPORT

void adsp_extern_notify(enum ADSP_NOTIFY_EVENT notify_status)
{
	blocking_notifier_call_chain(&adsp_A_notifier_list,
				     notify_status, NULL);
}

void adsp_schedule_reset_work(struct adsp_work_struct *adsp_ws)
{
	queue_work(adsp_reset_workqueue, &adsp_ws->work);
}
#endif
/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_A_notify_ws(struct work_struct *ws)
{
	struct adsp_work_struct *sws = container_of(ws, struct adsp_work_struct,
						    work);
	unsigned int adsp_notify_flag = sws->flags;

	if (adsp_notify_flag) {
#ifdef CFG_RECOVERY_SUPPORT
		if (adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START) {
			mutex_lock(&adsp_A_notify_mutex);
			adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_OK;
			atomic_set(&adsp_reset_status, ADSP_RESET_STATUS_STOP);
			adsp_extern_notify(ADSP_EVENT_READY);
			adsp_deregister_feature(SYSTEM_FEATURE_ID);
			mutex_unlock(&adsp_A_notify_mutex);
			__pm_relax(&adsp_reset_lock);
		}
#endif
	}
}


/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_timeout_ws(struct work_struct *ws)
{
	struct adsp_work_struct *sws =
		container_of(ws, struct adsp_work_struct, work);
	unsigned int adsp_timeout_id = sws->id;

#ifdef CFG_RECOVERY_SUPPORT
	if (adsp_timeout_times < 5) {
		adsp_timeout_times++;
		__pm_relax(&adsp_reset_lock);
		pr_debug("%s(): cnt (%d)\n", __func__, adsp_timeout_times);
		adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE,
				   adsp_timeout_id);
	} else
		BUG_ON(1); /* reboot */
#else
	adsp_aed(EXCEP_BOOTUP, adsp_timeout_id);
#endif
}


void adsp_reset_ready(enum adsp_core_id id)
{
	adsp_ready[id] = 0;
}

/*
 * callback for reset timer
 * mark notify flag to 0 to generate an exception
 * @param data: unuse
 */
#if ADSP_BOOT_TIME_OUT_MONITOR
static void adsp_wait_ready_timeout(unsigned long data)
{
	pr_debug("%s(),timer data=%lu\n", __func__, data);
	/*data=0: ADSP A  ,  data=1: ADSP B*/
	adsp_timeout_work.flags = 0;
	adsp_timeout_work.id = ADSP_A_ID;
	adsp_schedule_work(&adsp_timeout_work);
}

#endif
/*
 * handle notification from adsp
 * mark adsp is ready for running tasks
 * @param id:   ipi id
 * @param data: ipi data
 * @param len:  length of ipi data
 */
void adsp_A_ready_ipi_handler(int id, void *data, unsigned int len)
{
	unsigned int adsp_image_size = *(unsigned int *)data;

	if (!adsp_ready[ADSP_A_ID]) {
#if ADSP_BOOT_TIME_OUT_MONITOR
		del_timer(&adsp_ready_timer[ADSP_A_ID]);
#endif
#ifdef CFG_RECOVERY_SUPPORT
		adsp_timeout_times = 0;
#endif
		/* set adsp ready flag and clear SPM interrupt */
		adsp_ready[ADSP_A_ID] = 1;
		writel(0x0, ADSP_TO_SPM_REG);

		/* send notification to work queue */
		adsp_A_notify_work.flags = 1;
		adsp_schedule_work(&adsp_A_notify_work);
	}
	/*verify adsp image size*/
	if (adsp_image_size != ADSP_A_TCM_SIZE) {
		pr_info("[ADSP]image size ERROR! AP=0x%x,ADSP=0x%x\n",
			ADSP_A_TCM_SIZE, adsp_image_size);
		WARN_ON(1);
	}
}


/*
 * @return: 1 if adsp is ready for running tasks
 */
int is_adsp_ready(enum adsp_core_id id)
{
	if (id >= ADSP_CORE_TOTAL)
		return -EINVAL;
#ifdef CFG_RECOVERY_SUPPORT
	/* exception */
	if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START ||
	    adsp_recovery_flag[ADSP_A_ID] == ADSP_RECOVERY_START)
		return -1;
#endif
	return adsp_ready[id];
}
EXPORT_SYMBOL_GPL(is_adsp_ready);


/*
 * power on adsp
 * generate error if power on fail
 * @return:         1 if success
 */

uint32_t adsp_power_on(uint32_t enable)
{
	if (enable) {
		adsp_enable_clock();
		adsp_sw_reset();
		adsp_set_clock_freq(CLK_DEFAULT_INIT_CK);
		adsp_A_send_spm_request(true);
	} else {
		adsp_set_clock_freq(CLK_DEFAULT_26M_CK);
	}
	pr_debug("-%s (%x)\n", __func__, enable);
	return 1;
}
EXPORT_SYMBOL_GPL(adsp_power_on);

/*
 * TODO: what should we do when hibernation ?
 */
static int adsp_pm_event(struct notifier_block *notifier,
			 unsigned long pm_event, void *unused)
{

	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_debug("[ADSP] %s ADSP reboot\n", __func__);
		adsp_reset();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_pm_notifier_block = {
	.notifier_call = adsp_pm_event,
	.priority = 0,
};


static inline ssize_t adsp_A_status_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int status = 0;
	char *adsp_status;

	adsp_enable_dsp_clk(true);
	status = readl(ADSP_A_SYS_STATUS);
	adsp_enable_dsp_clk(false);

	switch (status) {
	case ADSP_STATUS_ACTIVE:
		adsp_status = "ADSP A is active";
		break;
	case ADSP_STATUS_SUSPEND:
		adsp_status = "ADSP A is suspend";
		break;
	case ADSP_STATUS_SLEEP:
		adsp_status = "ADSP A is sleep";
		break;
	case ADSP_STATUS_RESET:
		adsp_status = "ADSP A is reset";
		break;
	default:
		adsp_status = "ADSP A in unknown status";
		break;
	}
	return scnprintf(buf, PAGE_SIZE, "%s\n", adsp_status);
}

DEVICE_ATTR(adsp_A_status, 0444, adsp_A_status_show, NULL);

static inline ssize_t adsp_A_reg_status_show(struct device *kobj,
					     struct device_attribute *attr,
					     char *buf)
{
	int len = 0;

	if (adsp_ready[ADSP_A_ID]) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_REBOOT:0x%x\n",
				 readl(ADSP_A_REBOOT));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_IO_CONFIG:0x%x\n",
				 readl(ADSP_A_IO_CONFIG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_SWINT_REG:0x%x\n",
				 readl(ADSP_SWINT_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_TO_HOST_REG:0x%x\n",
				 readl(ADSP_A_TO_HOST_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_TO_SPM_REG:0x%x\n",
				 readl(ADSP_TO_SPM_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_DVFSRC_STATE:0x%x\n",
				 readl(ADSP_A_DVFSRC_STATE));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_DVFSRC_REQ:0x%x\n",
				 readl(ADSP_A_DVFSRC_REQ));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_DDREN_REQ:0x%x\n",
				 readl(ADSP_A_DDREN_REQ));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_SPM_REQ:0x%x\n",
				 readl(ADSP_A_SPM_REQ));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_SPM_ACK:0x%x\n",
				 readl(ADSP_A_SPM_ACK));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_SEMAPHORE:0x%x\n",
				 readl(ADSP_SEMAPHORE));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_WDT_REG:0x%x\n",
				 readl(ADSP_A_WDT_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_WDT_DEBUG_PC_REG:0x%x\n",
				 readl(ADSP_A_WDT_DEBUG_PC_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_WDT_DEBUG_SP_REG:0x%x\n",
				 readl(ADSP_A_WDT_DEBUG_SP_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_CFGREG_RSV_RW_REG0:0x%x\n",
				 readl(ADSP_CFGREG_RSV_RW_REG0));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_CFGREG_RSV_RW_REG1:0x%x\n",
				 readl(ADSP_CFGREG_RSV_RW_REG1));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_DEBUG_PC_REG:0x%x\n",
				 readl(ADSP_A_DEBUG_PC_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_SLEEP_STATUS_REG:0x%x\n",
				 readl(ADSP_SLEEP_STATUS_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_CLK_CTRL_BASE:0x%x\n",
				 readl(ADSP_CLK_CTRL_BASE));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_A_SLEEP_DEBUG_REG:0x%x\n",
				 readl(ADSP_A_SLEEP_DEBUG_REG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_CLK_HIGH_CORE_CG:0x%x\n",
				 readl(ADSP_CLK_HIGH_CORE_CG));
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "[ADSP] ADSP_ADSP2SPM_VOL_LV:0x%x\n",
				 readl(ADSP_ADSP2SPM_VOL_LV));
		return len;
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP A is not ready\n");
}

DEVICE_ATTR(adsp_A_reg_status, 0444, adsp_A_reg_status_show, NULL);

#if ADSP_VCORE_TEST_ENABLE
static inline ssize_t adsp_vcore_request_show(struct device *kobj,
					      struct device_attribute *attr,
					      char *buf)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&adsp_awake_spinlock, spin_flags);
	adsp_current_freq = readl(ADSP_CURRENT_FREQ_REG);
	adsp_expected_freq = readl(ADSP_EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&adsp_awake_spinlock, spin_flags);
	pr_info("[ADSP] receive freq show\n");
	return scnprintf(buf, PAGE_SIZE, "ADSP freq. expect=%d, cur=%d\n",
			 adsp_expected_freq, adsp_current_freq);
}

static unsigned int pre_feature_req = 0xff;

static ssize_t adsp_vcore_request_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t n)
{
	unsigned int feature_req = 0;


	if (kstrtouint(buf, 0, &feature_req) == 0) {
		if (pre_feature_req == 4)
			adsp_deregister_feature(VCORE_TEST5_FEATURE_ID);
		if (pre_feature_req == 3)
			adsp_deregister_feature(VCORE_TEST4_FEATURE_ID);
		if (pre_feature_req == 2)
			adsp_deregister_feature(VCORE_TEST3_FEATURE_ID);
		if (pre_feature_req == 1)
			adsp_deregister_feature(VCORE_TEST2_FEATURE_ID);
		if (pre_feature_req == 0)
			adsp_deregister_feature(VCORE_TEST_FEATURE_ID);

		if (feature_req == 4) {
			adsp_register_feature(VCORE_TEST5_FEATURE_ID);
			pre_feature_req = 4;
		}
		if (feature_req == 3) {
			adsp_register_feature(VCORE_TEST4_FEATURE_ID);
			pre_feature_req = 3;
		}
		if (feature_req == 2) {
			adsp_register_feature(VCORE_TEST3_FEATURE_ID);
			pre_feature_req = 2;
		}
		if (feature_req == 1) {
			adsp_register_feature(VCORE_TEST2_FEATURE_ID);
			pre_feature_req = 1;
		}
		if (feature_req == 0) {
			adsp_register_feature(VCORE_TEST_FEATURE_ID);
			pre_feature_req = 0;
		}

		pr_debug("[ADSP] set freq: %d => %d\n", adsp_current_freq,
			  adsp_expected_freq);
	}
	return n;
}

DEVICE_ATTR(adsp_vcore_request, 0644, adsp_vcore_request_show,
	    adsp_vcore_request_store);
#endif

static inline ssize_t adsp_A_db_test_show(struct device *kobj,
					  struct device_attribute *attr,
					  char *buf)
{
#ifdef CFG_RECOVERY_SUPPORT
	adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, ADSP_A_ID);
#else
	adsp_aed(EXCEP_KERNEL, ADSP_A_ID);
#endif
	if (adsp_ready[ADSP_A_ID])
		return scnprintf(buf, PAGE_SIZE, "dumping ADSP A db\n");
	else
		return scnprintf(buf, PAGE_SIZE, "not ready, try dump EE\n");
}

DEVICE_ATTR(adsp_A_db_test, 0444, adsp_A_db_test_show, NULL);

static inline ssize_t adsp_awake_force_lock_show(struct device *kobj,
						 struct device_attribute *att,
						 char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_force_lock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake force lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_force_lock, 0444, adsp_awake_force_lock_show, NULL);

static inline ssize_t adsp_awake_force_unlock_show(struct device *kobj,
						   struct device_attribute *att,
						   char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_force_unlock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake force unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_force_unlock, 0444, adsp_awake_force_unlock_show, NULL);

static inline ssize_t adsp_awake_set_normal_show(struct device *kobj,
						 struct device_attribute *att,
						 char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_set_normal(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake set normal\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_set_normal, 0444, adsp_awake_set_normal_show, NULL);

static inline ssize_t adsp_awake_dump_list_show(struct device *kobj,
						struct device_attribute *att,
						char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_dump_list(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake dump list\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_dump_list, 0444, adsp_awake_dump_list_show, NULL);

static inline ssize_t adsp_awake_lock_show(struct device *kobj,
					   struct device_attribute *att,
					   char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_lock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_lock, 0444, adsp_awake_lock_show, NULL);

static inline ssize_t adsp_awake_unlock_show(struct device *kobj,
					     struct device_attribute *att,
					     char *buf)
{
	if (adsp_ready[ADSP_A_ID]) {
		adsp_awake_unlock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_awake_unlock, 0444, adsp_awake_unlock_show, NULL);

static inline ssize_t adsp_ipi_test_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	enum adsp_ipi_status ret;
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	adsp_register_feature(SYSTEM_FEATURE_ID);

	if (adsp_ready[ADSP_A_ID]) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
	}

	/*
	 * BE CAREFUL! this cmd shouldn't let adsp process over 1s.
	 * Otherwise, you should register other feature before.
	 */
	adsp_deregister_feature(SYSTEM_FEATURE_ID);

	return count;
}

static inline ssize_t adsp_ipi_test_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int value = 0x5A5A;
	enum adsp_ipi_status ret;

	if (adsp_ready[ADSP_A_ID]) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP ipi send ret=%d\n", ret);
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}

DEVICE_ATTR(adsp_ipi_test, 0644, adsp_ipi_test_show, adsp_ipi_test_store);

static inline ssize_t adsp_uart_switch_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	enum adsp_ipi_status ret;
	unsigned int value = 0;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	if (adsp_ready[ADSP_A_ID]) {
		ret = adsp_ipi_send(ADSP_IPI_TEST1, &value, sizeof(value),
				    0, ADSP_A_ID);
	}
	return count;
}

DEVICE_ATTR(adsp_uart_switch, 0220, NULL, adsp_uart_switch_store);
static inline ssize_t adsp_suspend_cmd_show(struct device *kobj,
					     struct device_attribute *attr,
					     char *buf)
{
	return adsp_dump_feature_state(buf, PAGE_SIZE);
}

static inline ssize_t adsp_suspend_cmd_store(struct device *kobj,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t id = 0;
	char *temp = NULL, *token1 = NULL, *token2 = NULL;
	char *pin = NULL;
	char delim[] = " ,\t\n";

	temp = kstrdup(buf, GFP_KERNEL);
	pin = temp;
	token1 = strsep(&pin, delim);
	token2 = strsep(&pin, delim);

	id = adsp_get_feature_index(token2);

	if ((strcmp(token1, "regi") == 0) && (id >= 0))
		adsp_register_feature(id);

	if ((strcmp(token1, "deregi") == 0) && (id >= 0))
		adsp_deregister_feature(id);

	kfree(temp);
	return count;
}

DEVICE_ATTR(adsp_suspend_cmd, 0644, adsp_suspend_cmd_show,
	    adsp_suspend_cmd_store);



/*
 * trigger wdt manually
 * debug use
 */
void adsp_wdt_reset(enum adsp_core_id cpu_id, int interval)
{
	int wdt_reg = 0;

	switch (cpu_id) {
	case ADSP_A_ID:
		writel(WDT_DIS_BIT, ADSP_A_WDT_REG);
		writel((interval&0xFFFFE), ADSP_WDT_TRIGGER);
		udelay(90);
		wdt_reg = readl(ADSP_A_WDT_REG);
		writel((WDT_EN_BIT | wdt_reg), ADSP_A_WDT_REG);
		udelay(90);
		wdt_reg = readl(ADSP_A_WDT_REG);
		/* restart wdt*/
		writel((wdt_reg & ~WDT_KICK_BIT), ADSP_A_WDT_REG);
		writel((wdt_reg | WDT_KICK_BIT), ADSP_A_WDT_REG);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(adsp_wdt_reset);

static ssize_t adsp_wdt_trigger(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int interval = 0;

	if (kstrtoint(buf, 10, &interval))
		return -EINVAL;
	pr_debug("%s: %d\n", __func__, interval);
	adsp_reset_by_cmd = 1;
	adsp_wdt_reset(ADSP_A_ID, interval);
	return count;
}

DEVICE_ATTR(adsp_wdt_reset, 0200, NULL, adsp_wdt_trigger);


#ifdef CFG_RECOVERY_SUPPORT

static ssize_t adsp_recovery_flag_r(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", adsp_recovery_flag[ADSP_A_ID]);
}

DEVICE_ATTR(adsp_recovery_flag, 0444, adsp_recovery_flag_r, NULL);

#endif


static struct miscdevice adsp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp",
	.fops = &adsp_A_drv_file_ops
};

static ssize_t adsp_debug_read(struct file *file, char __user *buf,
			       size_t count, loff_t *pos)
{
	char *buffer = NULL; /* for reduce kernel stack */
	int ret = 0;
	size_t n = 0, max_size;

	buffer = (void *)adsp_get_reserve_mem_virt(ADSP_A_DEBUG_DUMP_MEM_ID);
	max_size = (size_t)adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);

	n = strnlen(buffer, max_size);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	return ret;
}

static ssize_t adsp_debug_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	char buf[64];

	if (copy_from_user(buf, buffer, min(count, sizeof(buf))))
		return -EFAULT;

	if (adsp_register_feature(SYSTEM_FEATURE_ID) == 0) {
		adsp_ipi_send(ADSP_IPI_ADSP_TIMER, buf,
			min(count, sizeof(buf)), 0, ADSP_A_ID);
		adsp_deregister_feature(SYSTEM_FEATURE_ID);
	}

	return count;
}

static const struct file_operations adsp_debug_ops = {
	.open = simple_open,
	.read = adsp_debug_read,
	.write = adsp_debug_write,
};

/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
static int create_files(void)
{
	int ret;

	ret = misc_register(&adsp_device);

	if (unlikely(ret != 0)) {
		pr_err("[ADSP] misc register failed\n");
		return ret;
	}

#if ADSP_LOGGER_ENABLE
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_logger_wakeup_AP);
	if (unlikely(ret != 0))
		return ret;
#ifdef CONFIG_MTK_ENG_BUILD
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_mobile_log_UT);
	if (unlikely(ret != 0))
		return ret;
#endif
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_get_last_log);
	if (unlikely(ret != 0))
		return ret;
#endif
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_status);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(adsp_device.this_device,
				     &bin_attr_adsp_dump);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(adsp_device.this_device,
					&bin_attr_adsp_dump_ke);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_reg_status);

	if (unlikely(ret != 0))
		return ret;

	/*only support debug db test in engineer build*/
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_db_test);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_force_lock);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_force_unlock);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_set_normal);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_dump_list);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_lock);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_awake_unlock);

	if (unlikely(ret != 0))
		return ret;

	/* ADSP IPI Debug sysfs*/
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_ipi_test);
	if (unlikely(ret != 0))
		return ret;
	/* ADSP IPI Uart sysfs*/
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_uart_switch);
	if (unlikely(ret != 0))
		return ret;

	/* ADSP suspend/ resume debug */
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_suspend_cmd);
	if (unlikely(ret != 0))
		return ret;

	/* create debug file */
	adsp_debugfs = debugfs_create_file("audiodsp", S_IFREG | 0644, NULL,
					(void *)&adsp_device, &adsp_debug_ops);
	if (IS_ERR(adsp_debugfs))
		return PTR_ERR(adsp_debugfs);

#ifdef CFG_RECOVERY_SUPPORT
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_wdt_reset);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_recovery_flag);
	if (unlikely(ret != 0))
		return ret;

#endif


#if ADSP_VCORE_TEST_ENABLE
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_vcore_request);

	if (unlikely(ret != 0))
		return ret;
#endif

#if ADSP_TRAX
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_A_trax);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_bin_file(adsp_device.this_device,
				     &bin_attr_adsp_trax);

	if (unlikely(ret != 0))
		return ret;
#endif
#if ADSP_SLEEP_ENABLE

	/* ADSP sleep measure */
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_force_adsppll);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_spm_req);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_keep_adspll_on);
	if (unlikely(ret != 0))
		return ret;
#endif
	/* ADSP vcore/freq */
	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_dvfs_force_opp);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_dvfs_set_freq);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(adsp_device.this_device,
				 &dev_attr_adsp_dvfs_trigger_latmon);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}

phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID) {
		pr_debug("[ADSP] no reserve memory for %d", id);
		return 0;
	} else
		return adsp_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(adsp_get_reserve_mem_phys);

phys_addr_t adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID) {
		pr_debug("[ADSP] no reserve memory for %d", id);
		return 0;
	} else
		return adsp_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(adsp_get_reserve_mem_virt);

phys_addr_t adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id)
{
	if (id >= ADSP_NUMS_MEM_ID) {
		pr_debug("[ADSP] no reserve memory for %d", id);
		return 0;
	} else
		return adsp_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(adsp_get_reserve_mem_size);

static int adsp_reserve_memory_ioremap(void)
{
	enum adsp_reserve_mem_id_t id;
	phys_addr_t adsp_a_firmware_base_phys =
				*(unsigned int *)ADSP_A_MPUINFO_BUFFER;
	phys_addr_t accumlate_memory_size = 0;

	if (adsp_mem_base_phys != adsp_a_firmware_base_phys) {
		/* ADSP needs to check if memroy address got is as expected. */
		pr_debug("[ADSP] The allocated memory(0x%llx) size abnormal\n",
			adsp_mem_base_phys);
		/*should not call WARN_ON() here or there is no log, return -1
		 * instead.
		 */
		return -1;
	}
	pr_debug("[ADSP] phys:0x%llx - 0x%llx (0x%llx)\n", adsp_mem_base_phys,
		adsp_mem_base_phys + adsp_mem_size, adsp_mem_size);

	adsp_mem_base_virt = (phys_addr_t)(size_t)ioremap_wc(adsp_mem_base_phys,
								adsp_mem_size);

	/* SYSRAM_BASE_VIRTUAL */
	adspreg.sysram = (void __iomem *)adsp_mem_base_virt;
	pr_debug("[ADSP]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		 adsp_mem_base_virt,
		 adsp_mem_base_virt + adsp_mem_size,
		 adsp_mem_size);

	/* assign to each memroy block */
	for (id = 0; id < ADSP_NUMS_MEM_ID; id++) {
		adsp_reserve_mblock[id].start_phys = adsp_mem_base_phys +
							accumlate_memory_size;
		adsp_reserve_mblock[id].start_virt = adsp_mem_base_virt +
							accumlate_memory_size;
		accumlate_memory_size += adsp_reserve_mblock[id].size;
	}
	/* the reserved memory should be larger then expected memory
	 * or adsp_reserve_mblock does not match dts
	 */
	WARN_ON(accumlate_memory_size > adsp_mem_size);
#ifdef DEBUG
	for (id = 0; id < ADSP_NUMS_MEM_ID; id++) {
		pr_debug("[ADSP][id:%d] phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			id, adsp_get_reserve_mem_phys(id),
			adsp_get_reserve_mem_virt(id),
			adsp_get_reserve_mem_size(id));
	}
#endif
	return 0;
}

/* reference adsp_reservedmem_define.h */
void adsp_update_memory_protect_info(void)
{
	uint32_t adsp_region_size;

	adsp_mpu_info = ADSP_A_MPUINFO_BUFFER;
	adsp_region_size = adsp_mpu_info->adsp_mpu_prog_size;
#ifdef MPU_NONCACHEABLE_PATCH
	adsp_mpu_info->adsp_mpu_data_non_cache_addr =
			adsp_reserve_mblock[ADSP_MPU_NONCACHE_ID].start_phys;
	adsp_mpu_info->adsp_mpu_data_non_cache_size =
			adsp_mpu_info->adsp_mpu_prog_addr + adsp_region_size -
			adsp_mpu_info->adsp_mpu_data_non_cache_addr;
#else
	adsp_mpu_info->adsp_mpu_data_ro_addr =
			adsp_reserve_mblock[ADSP_MPU_DATA_RO_ID].start_phys;
	adsp_mpu_info->adsp_mpu_data_ro_size = adsp_region_size -
			adsp_mpu_info->adsp_mpu_prog_size -
			adsp_mpu_info->adsp_mpu_data_size;
#endif
}

void adsp_enable_dsp_clk(bool enable)
{
	if (enable) {
		pr_debug("enable dsp clk\n");
		adsp_enable_clock();
		/* writel(1 << 27, DSP_CLK_ADDRESS); */
	} else {
		pr_debug("disable dsp clk\n");
		adsp_disable_clock();
		/* writel(0 << 27, DSP_CLK_ADDRESS); */
	}
}
#ifdef CFG_RECOVERY_SUPPORT
/*
 * adsp_set_reset_status, set and return scp reset status function
 * return value:
 *   0: scp not in reset status
 *   1: scp in reset status
 */
unsigned int adsp_set_reset_status(void)
{
	unsigned long spin_flags;

	spin_lock_irqsave(&adsp_reset_spinlock, spin_flags);
	if (atomic_read(&adsp_reset_status) == ADSP_RESET_STATUS_START) {
		spin_unlock_irqrestore(&adsp_reset_spinlock, spin_flags);
		return 1;
	}
	/* adsp not in reset status, set it and return*/
	atomic_set(&adsp_reset_status, ADSP_RESET_STATUS_START);
	spin_unlock_irqrestore(&adsp_reset_spinlock, spin_flags);
	return 0;
}

/*
 * callback function for work struct
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
void adsp_sys_reset_ws(struct work_struct *ws)
{
	struct adsp_work_struct *sws = container_of(ws, struct adsp_work_struct,
						    work);
	unsigned int adsp_reset_type = sws->flags;
	/* adsp cfg reg,*/
	unsigned int adsp_reset_flag = 0;
	/* make sure adsp is in idle state */
	int timeout = 100; /* max wait 2s */

	/*set adsp not ready*/
	adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_START;
	adsp_ready[ADSP_A_ID] = 0;
	pr_info("%s(): adsp_extern_notify\n", __func__);
	adsp_register_feature(SYSTEM_FEATURE_ID);
	adsp_extern_notify(ADSP_EVENT_STOP);

	/* wake lock AP*/
	__pm_stay_awake(&adsp_reset_lock);

	/*workqueue for adsp ee, adsp reset by cmd will not trigger adsp ee*/

	if (adsp_reset_by_cmd == 0) {
		pr_info("%s(): adsp_aed_reset\n", __func__);
		if (adsp_reset_type == ADSP_RESET_TYPE_AWAKE)
			adsp_aed_reset(EXCEP_KERNEL, ADSP_A_ID);
		else
			adsp_aed_reset(EXCEP_RUNTIME, ADSP_A_ID);
		/*wait adsp ee finished in 10s*/
		if (wait_for_completion_interruptible_timeout(&adsp_sys_reset_cp
					, jiffies_to_msecs(10000)) == 0) {
			pr_info("%s: adsp ee time out\n", __func__);
			/*timeout check adsp status again*/
			if (is_adsp_ready(ADSP_A_ID) != -1)
				goto END;
		}
	}

	/* enable clock for access ADSP Reg*/
	adsp_enable_dsp_clk(true);
	/* dump bus status if has bus hang*/
	if (readl(ADSP_BUS_MON_BACKUP_BASE) == BUS_MON_STATE_1ST_STAGE ||
	    readl(ADSP_BUS_MON_BACKUP_BASE) == BUS_MON_STATE_2ND_STAGE) {
#ifdef CONFIG_MTK_EMI
		dump_emi_outstanding(); /* check infra, dump all info*/
		lastbus_timeout_dump(); /* check infra/peri, dump both info */
#endif
	}

	if (adsp_reset_type == ADSP_RESET_TYPE_AWAKE)
		pr_info("%s(): adsp awake fail, wait system back\n", __func__);

	/* make sure adsp is in idle state */
	while (--timeout) {
		if ((readl(ADSP_SLEEP_STATUS_REG) & ADSP_A_IS_WFI) &&
		    (readl(ADSP_DBG_PEND_CNT) == 0)) {
			adsp_reset();
			if (readl(ADSP_SLEEP_STATUS_REG) &
			    ADSP_A_IS_ACTIVE) {
				adsp_reset_flag = 1;
				break;
			}
		}
		msleep(20);
	}

	if (!adsp_reset_flag) {
		if (readl(ADSP_DBG_PEND_CNT))
			pr_info("%s(): failed, bypass and wait\n", __func__);
		else
			adsp_reset();
	}
#if ADSP_BOOT_TIME_OUT_MONITOR
	if (timer_pending(&adsp_ready_timer[ADSP_A_ID]) != 1) {
		adsp_ready_timer[ADSP_A_ID].expires =
					jiffies + ADSP_READY_TIMEOUT;
		add_timer(&adsp_ready_timer[ADSP_A_ID]);
	}
#endif
END:
	adsp_reset_by_cmd = 0;
	adsp_enable_dsp_clk(false);
}
/*
 */
void adsp_send_reset_wq(enum ADSP_RESET_TYPE type, enum adsp_core_id core_id)
{
	adsp_sys_reset_work.flags = (unsigned int) type;
	adsp_sys_reset_work.id = core_id;
	adsp_schedule_reset_work(&adsp_sys_reset_work);
}
#endif

//Liang: dvfs to check ADSP PLL?
int adsp_check_resource(void)
{
	/* called by lowpower related function
	 * main purpose is to ensure main_pll is not disabled
	 * because adsp needs main_pll to run at vcore 1.0 and 354Mhz
	 * return value:
	 * 1: main_pll shall be enabled,
	 *    26M shall be enabled, infra shall be enabled
	 * 0: main_pll may disable, 26M may disable, infra may disable
	 */
	int adsp_resource_status = 0;
#ifdef CONFIG_MACH_MT6799
	unsigned long spin_flags;

	spin_lock_irqsave(&adsp_awake_spinlock, spin_flags);
	adsp_current_freq = readl(ADSP_CURRENT_FREQ_REG);
	adsp_expected_freq = readl(ADSP_EXPECTED_FREQ_REG);
	spin_unlock_irqrestore(&adsp_awake_spinlock, spin_flags);

	if (adsp_expected_freq == FREQ_416MHZ ||
	    adsp_current_freq == FREQ_416MHZ)
		adsp_resource_status = 1;
	else
		adsp_resource_status = 0;
#endif

	return adsp_resource_status;
}

void adsp_recovery_init(void)
{
#ifdef CFG_RECOVERY_SUPPORT
	/*create wq for scp reset*/
	adsp_reset_workqueue = create_singlethread_workqueue("ADSP_RESET_WQ");
	/*init reset work*/
	INIT_WORK(&adsp_sys_reset_work.work, adsp_sys_reset_ws);
	/*init completion for identify adsp aed finished*/
	init_completion(&adsp_sys_reset_cp);
	wakeup_source_init(&adsp_reset_lock, "adsp reset wakelock");
	/* init reset by cmd flag*/
	adsp_reset_by_cmd = 0;
	adsp_recovery_flag[ADSP_A_ID] = ADSP_RECOVERY_OK;
#endif
}
static int adsp_system_sleep_suspend(struct device *dev)
{
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(ADSP_A_ID) == 1) || adsp_feature_is_active()) {
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_FREEZE);
		adsp_awake_unlock_adsppll(ADSP_A_ID, 1);
	}
	mutex_unlock(&adsp_suspend_mutex);
	return 0;
}

static int adsp_system_sleep_resume(struct device *dev)
{
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(ADSP_A_ID) == 1) || adsp_feature_is_active()) {
		/*wake adsp up*/
		adsp_awake_unlock_adsppll(ADSP_A_ID, 0);
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_UNFREEZE);
	}
	mutex_unlock(&adsp_suspend_mutex);

	return 0;
}

static int adsp_syscore_suspend(void)
{
	if ((is_adsp_ready(ADSP_A_ID) != 1) && !adsp_feature_is_active()) {
		adsp_bus_sleep_protect(true);
		spm_adsp_mem_protect();
	}
	return 0;
}

static void adsp_syscore_resume(void)
{
	if ((is_adsp_ready(ADSP_A_ID) != 1) && !adsp_feature_is_active()) {
		spm_adsp_mem_unprotect();
		adsp_bus_sleep_protect(false);
		/* release adsp sw_reset,
		 * let ap is able to write adsp cfg/dtcm
		 * no matter adsp is suspend.
		 */
		adsp_enable_clock();
		writel((ADSP_A_SW_RSTN | ADSP_A_SW_DBG_RSTN), ADSP_A_REBOOT);
		udelay(1);
		writel(0, ADSP_A_REBOOT);
		sys_timer_timesync_sync_adsp(SYS_TIMER_TIMESYNC_FLAG_UNFREEZE);
		adsp_disable_clock();
	}
}

static int adsp_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
#ifdef Liang_Check
	const char *core_status = NULL;
#endif
	u64	sysram[2] = {0, 0};
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adspreg.cfg = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) adspreg.cfg)) {
		pr_debug("[ADSP] adspreg.cfg error\n");
		return -1;
	}

	adspreg.cfgregsize = (unsigned int)resource_size(res);
	pr_debug("[ADSP] cfg base=0x%p, cfgregsize=%d\n", adspreg.cfg,
		adspreg.cfgregsize);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	adspreg.iram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) adspreg.iram)) {
		pr_debug("[ADSP] adspreg.iram error\n");
		return -1;
	}
	adspreg.i_tcmsize = (unsigned int)resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	adspreg.dram = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *) adspreg.dram)) {
		pr_debug("[ADSP] adspreg.dram error\n");
		return -1;
	}
	adspreg.d_tcmsize = (unsigned int)resource_size(res);
	adspreg.total_tcmsize = (unsigned int)adspreg.i_tcmsize +
				adspreg.d_tcmsize;
	pr_debug("[ADSP] iram base=0x%p %x dram base=0x%p %x, %x\n",
		 adspreg.iram, adspreg.i_tcmsize,
		 adspreg.dram, adspreg.d_tcmsize, adspreg.total_tcmsize);
	pr_debug("[ADSP] ipc =0x%p ostimer =0x%p ,mpu =0x%p\n",
		 ADSP_A_IPC_BUFFER, ADSP_A_OSTIMER_BUFFER,
		 ADSP_A_MPUINFO_BUFFER);
	adsp_dts_mapping();

	adspreg.clkctrl = adspreg.cfg + ADSP_CLK_CTRL_OFFSET;
	pr_debug("[ADSP] clkctrl base=0x%p\n", adspreg.clkctrl);
	adsp_clk_device_probe(&pdev->dev);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	adspreg.wdt_irq = res->start;
	pr_debug("[ADSP] adspreg.wdt_irq=%d\n", adspreg.wdt_irq);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	adspreg.ipc_irq = res->start;
	pr_debug("[ADSP] adspreg.ipc_irq=%d\n", adspreg.ipc_irq);

	of_property_read_u64_array(pdev->dev.of_node, "sysram",
			sysram, ARRAY_SIZE(sysram));
	adsp_mem_base_phys = (phys_addr_t) sysram[0];
	adsp_mem_size = (phys_addr_t)sysram[1]; /*SYSRAM_SIZE*/
	if (!adsp_mem_base_phys) {
		pr_debug("[ADSP] Sysram base address is not found\n");
		return -ENODEV;
	}
	pr_debug("[ADSP] adsp_mem_base_phys/adsp_mem_size =0x%llx/0x%llx\n",
		adsp_mem_base_phys, adsp_mem_size);

	adsp_enable[ADSP_A_ID] = 1;

	return ret;
}

static int adsp_device_remove(struct platform_device *pdev)
{
	adsp_clk_device_remove(&pdev->dev);
	return 0;
}

static const struct of_device_id adsp_of_ids[] = {
	{ .compatible = "mediatek,audio_dsp", },
	{}
};

static const struct dev_pm_ops adsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(adsp_system_sleep_suspend,
				adsp_system_sleep_resume)
};

static struct syscore_ops adsp_syscore_ops = {
	.resume = adsp_syscore_resume,
	.suspend = adsp_syscore_suspend,
};

static struct platform_driver mtk_adsp_device = {
	.probe = adsp_device_probe,
	.remove = adsp_device_remove,
	.driver = {
		.name = "adsp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_of_ids,
#endif
#ifdef CONFIG_PM
		.pm = &adsp_pm_ops,
#endif
	},
};
/*
 * driver initialization entry point
 */
static int __init adsp_init(void)
{
	int ret = 0;
	int i = 0;

	/* adsp ready static flag initialise */
	for (i = 0; i < ADSP_CORE_TOTAL ; i++) {
		adsp_enable[i] = 0;
		adsp_ready[i] = 0;
	}

	if (platform_driver_register(&mtk_adsp_device))
		pr_err("[ADSP] adsp probe fail\n");
	register_syscore_ops(&adsp_syscore_ops);

	adsp_power_on(true);
	/* need adsp power on to access DTCM  */
	ret = adsp_reserve_memory_ioremap();
	if (ret) {
		pr_err("[ADSP]adsp_reserve_memory_ioremap failed\n");
		return -1;
	}
	adsp_update_memory_protect_info();

	pr_debug("%s(-)\n", __func__);
	return ret;

}

static int __init adsp_module_init(void)
{
	int ret = 0;

#if ADSP_DVFS_INIT_ENABLE
	adsp_dvfs_init();
#endif

	/* adsp platform initialise */
	pr_debug("[ADSP] platform init\n");
	adsp_awake_init();
	adsp_workqueue = create_workqueue("ADSP_WQ");
	ret = adsp_excep_init();
	if (ret) {
		pr_debug("[ADSP] Excep Init Fail\n");
		return -1;
	}

	/* skip initial if dts status = "disable" */
	if (!adsp_enable[ADSP_A_ID]) {
		pr_err("[ADSP] adsp disabled!!\n");
		return -1;
	}
	/* adsp ipi initialise */
	adsp_send_buff[ADSP_A_ID] = kmalloc((size_t)SHARE_BUF_SIZE, GFP_KERNEL);
	if (!adsp_send_buff[ADSP_A_ID])
		return -1;

	adsp_recv_buff[ADSP_A_ID] = kmalloc((size_t)SHARE_BUF_SIZE, GFP_KERNEL);
	if (!adsp_recv_buff[ADSP_A_ID])
		return -1;

	INIT_WORK(&adsp_A_notify_work.work, adsp_A_notify_ws);
	INIT_WORK(&adsp_timeout_work.work, adsp_timeout_ws);

	adsp_suspend_init();

	adsp_A_irq_init();
	adsp_A_ipi_init();

	adsp_ipi_registration(ADSP_IPI_ADSP_A_READY, adsp_A_ready_ipi_handler,
			      "adsp_A_ready");

	/* adsp ramdump initialise */
	pr_debug("[ADSP] ramdump init\n");
	adsp_ram_dump_init();
	ret = register_pm_notifier(&adsp_pm_notifier_block);

	if (ret)
		pr_debug("[ADSP] failed to register PM notifier %d\n", ret);

	/* adsp sysfs initialise */
	pr_debug("[ADSP] sysfs init\n");
	ret = create_files();

	if (unlikely(ret != 0)) {
		pr_debug("[ADSP] create files failed\n");
		return -1;
	}

	/* adsp request irq */
	pr_debug("[ADSP] request_irq\n");
	ret = request_irq(adspreg.ipc_irq, adsp_A_irq_handler, IRQF_TRIGGER_LOW,
			  "ADSP A IPC2HOST", NULL);
	if (ret) {
		pr_debug("[ADSP] require ipc irq failed\n");
		return -1;
	}
	ret = request_irq(adspreg.wdt_irq, adsp_A_wdt_handler, IRQF_TRIGGER_LOW,
			  "ADSP A WDT", NULL);
	if (ret) {
		pr_debug("[ADSP] require wdt irq failed\n");
		return -1;
	}

#if ADSP_LOGGER_ENABLE
	/* adsp logger initialize */
	pr_debug("[ADSP] logger init\n");
	if (adsp_logger_init(adsp_get_reserve_mem_virt(ADSP_A_LOGGER_MEM_ID),
			     adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID))
			     == -1) {
		pr_debug("[ADSP] adsp_logger_init_fail\n");
		return -1;
	}
#endif

#if ADSP_TRAX
	/* adsp trax initialize */
	pr_debug("[ADSP] trax init\n");
	if (adsp_trax_init() == -1) {
		pr_debug("[ADSP] adsp_trax_init fail\n");
		return -1;
	}
#endif

#if ENABLE_ADSP_EMI_PROTECTION
	set_adsp_mpu();
#endif
#ifdef CFG_RECOVERY_SUPPORT
	adsp_recovery_init();
	adsp_A_register_notify(&adsp_ready_notifier1);
#endif
#if ADSP_DVFS_INIT_ENABLE
	wait_adsp_dvfs_init_done();
#endif

	driver_init_done = true;
	/* Liang release Run stall */
	adsp_release_runstall(true);

#if ADSP_BOOT_TIME_OUT_MONITOR
	init_timer(&adsp_ready_timer[ADSP_A_ID]);
	adsp_ready_timer[ADSP_A_ID].expires =
				jiffies + ADSP_READY_TIMEOUT;
	adsp_ready_timer[ADSP_A_ID].function =
				&adsp_wait_ready_timeout;
	adsp_ready_timer[ADSP_A_ID].data =
				(unsigned long)ADSP_A_TIMER;
	add_timer(&adsp_ready_timer[ADSP_A_ID]);
#endif
	pr_debug("[ADSP] driver_init_done\n");

	return ret;
}
subsys_initcall(adsp_init);
module_init(adsp_module_init);
/*
 * driver exit point
 */
static void __exit adsp_exit(void)
{
#if ADSP_BOOT_TIME_OUT_MONITOR
	int i = 0;
#endif

#if ADSP_DVFS_INIT_ENABLE
	adsp_dvfs_exit();
#endif

	/*adsp ipi de-initialise*/
	kfree(adsp_send_buff[ADSP_A_ID]);
	kfree(adsp_recv_buff[ADSP_A_ID]);


	free_irq(adspreg.wdt_irq, NULL);
	free_irq(adspreg.ipc_irq, NULL);

	misc_deregister(&adsp_device);
	debugfs_remove(adsp_debugfs);

	flush_workqueue(adsp_workqueue);
	/*adsp_logger_cleanup();*/
	destroy_workqueue(adsp_workqueue);
#ifdef CFG_RECOVERY_SUPPORT
	flush_workqueue(adsp_reset_workqueue);
	destroy_workqueue(adsp_reset_workqueue);
#endif
#if ADSP_BOOT_TIME_OUT_MONITOR
	for (i = 0; i < ADSP_CORE_TOTAL ; i++)
		del_timer(&adsp_ready_timer[i]);
#endif
	kfree(adsp_swap_buf);
}
module_exit(adsp_exit);

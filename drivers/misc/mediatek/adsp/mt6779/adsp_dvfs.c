// SPDX-License-Identifier: GPL-2.0
//
// adsp_dvfs.c --  Mediatek ADSP DVFS control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Celine Liu <Celine.liu@mediatek.com>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#ifdef wakelock
#include <linux/wakelock.h>
#endif
#include <linux/io.h>
//#include <mt-plat/upmu_common.h>
#include <mtk_spm_sleep.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#include <linux/uaccess.h>
#include "adsp_ipi.h"
#include <adsp_ipi_queue.h>
#include <audio_ipi_platform.h>
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"
#include "adsp_clk.h"
#include "adsp_reg.h"
#include <adsp_timesync.h>

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt)     "[adsp_dvfs]: " fmt

#define ADSP_DBG(fmt, arg...) pr_debug(fmt, ##arg)
#define ADSP_INFO(fmt, arg...) pr_info(fmt, ##arg)

#define DRV_Reg32(addr)           readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val)   DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val)   DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

/* new implement for adsp */
static DEFINE_MUTEX(adsp_timer_mutex);
DEFINE_MUTEX(adsp_suspend_mutex);

static struct timer_list adsp_suspend_timer;
static struct adsp_work_t adsp_suspend_work;
int adsp_is_suspend;

static int adsp_is_force_freq;
static int adsp_is_force_trigger_latmon;

#define ADSP_DELAY_OFF_TIMEOUT          (1 * HZ) /* 1 second */
#define ADSP_DELAY_TIMER                (0)
#define FORCE_ADSPPLL_BASE              (0x10000000)
#define SPM_REQ_BASE                    (0x20000000)
#define ADSPPLL_ON_BASE                 (0x30000000)
#define ADD_FREQ_CMD                    (0x11110000)
#define DEL_FREQ_CMD                    (0x22220000)
#define QUERY_FREQ_CMD                  (0xFFFFFFFF)
#define ADSP_FREQ_MAX_VALUE             (630)    /* 0.725V 70MCPS for sys */
#define TRIGGER_LATMON_IRQ_CMD          (0x00010000)

/* Private Macro ---------------------------------------------------------*/
/* The register must sync with ctrl-boards.S in Tinysys */
#define CREG_BOOTUP_MARK   (ADSP_CFGREG_RSV_RW_REG0)
/* if equal, bypass clear bss and some init */
#define MAGIC_PATTERN      (0xfafafafa)

struct wakeup_source *adsp_suspend_lock;

static inline ssize_t adsp_force_adsppll_store(struct device *kobj,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t value = 0, param = 0;
	enum adsp_ipi_status  ret;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	switch (value) {
	case 480:
		param = FORCE_ADSPPLL_BASE+0x01E0;
		break;
	case 694:
		param = FORCE_ADSPPLL_BASE+0x02B6;
		break;
	case 800:
		param = FORCE_ADSPPLL_BASE+0x0320;
		break;
	default:
		break;
	}
	pr_debug("[%s]force in %d(%x)\n", __func__, value, param);

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_DVFS_SLEEP, &param,
			sizeof(param), 0, ADSP_A_ID);
	}
	return count;
}

static inline ssize_t adsp_spm_req_store(struct device *kobj,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t value = 0, param;
	enum adsp_ipi_status  ret;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	param = SPM_REQ_BASE + value;
	pr_debug("[%s]set SPM req in %d(%x)\n", __func__, value, param);

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_DVFS_SLEEP, &param,
			sizeof(param), 0, ADSP_A_ID);
	}
	return count;
}

static inline ssize_t adsp_keep_adsppll_on_store(struct device *kobj,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	uint32_t value = 0, param;
	enum adsp_ipi_status  ret;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	param = ADSPPLL_ON_BASE + value;
	pr_debug("[%s] %d(%x)\n", __func__, value, param);

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		ret = adsp_ipi_send(ADSP_IPI_DVFS_SLEEP, &param,
				    sizeof(param), 0, ADSP_A_ID);
	}
	return count;
}

static inline ssize_t adsp_dvfs_force_opp_show(struct device *kobj,
					       struct device_attribute *attr,
					       char *buf)
{
	return  scnprintf(buf, PAGE_SIZE, "force freq status = %d\n",
			  adsp_is_force_freq);
}

static inline ssize_t adsp_dvfs_force_opp_store(struct device *kobj,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned int value = 0;
	enum adsp_ipi_status  ret = ADSP_IPI_ERROR;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	if (is_adsp_ready(ADSP_A_ID) == 1) {
		if (value == 255) {
			/* release force freq mode */
			adsp_is_force_freq = 0;
			ret = adsp_ipi_send(ADSP_IPI_DVFS_FIX_OPP_SET, &value,
					    sizeof(value), 0, ADSP_A_ID);
		} else if (value == 0 || value == 1) {
			/* force adsp freq at specific value */
			adsp_is_force_freq = 1;
			ret = adsp_ipi_send(ADSP_IPI_DVFS_FIX_OPP_SET, &value,
					    sizeof(value), 0, ADSP_A_ID);
		}
	}
	return count;
}

static inline ssize_t adsp_dvfs_set_freq_show(struct device *kobj,
					      struct device_attribute *attr,
					      char *buf)
{
	unsigned int value = QUERY_FREQ_CMD;
	enum adsp_ipi_status  ret;

	ret = adsp_ipi_send(ADSP_IPI_DVFS_SET_FREQ, &value,
			    sizeof(value), 0, ADSP_A_ID);
	/* FIXME: ToDo, get MCPS and freq result from ADSP */
	return scnprintf(buf, PAGE_SIZE, "Running on level %d, Total MCPS=%d\n",
			 0xFF, 0xFF);
}

static inline ssize_t adsp_dvfs_set_freq_store(struct device *kobj,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	char *temp = NULL, *token1 = NULL, *token2 = NULL;
	char *pin = NULL;
	char delim[] = " ,";
	unsigned int value = 0;
	enum adsp_ipi_status  ret;


	temp = kstrdup(buf, GFP_KERNEL);
	pin = temp;
	token1 = strsep(&pin, delim);
	if (token1 == NULL)
		return -EINVAL;
	token2 = strsep(&pin, delim);
	if (token2 == NULL)
		return -EINVAL;

	if (kstrtoint(token2, 10, &value))
		return -EINVAL;
	if (value >= ADSP_FREQ_MAX_VALUE)
		return -EINVAL;

	if (strcmp(token1, "add") == 0)
		value |= ADD_FREQ_CMD;
	if (strcmp(token1, "del") == 0)
		value |= DEL_FREQ_CMD;

	ret = adsp_ipi_send(ADSP_IPI_DVFS_SET_FREQ, &value,
			    sizeof(value), 0, ADSP_A_ID);

	kfree(temp);
	return count;
}

static inline ssize_t adsp_dvfs_trigger_latmon_show(struct device *kobj,
					struct device_attribute *attr,
					char *buf)
{
	return  scnprintf(buf, PAGE_SIZE, "force trigger latmon irq = %d\n",
			  adsp_is_force_trigger_latmon);
}

static inline ssize_t adsp_dvfs_trigger_latmon_store(struct device *kobj,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int value = 0;
	enum adsp_ipi_status  ret = ADSP_IPI_ERROR;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	value |= TRIGGER_LATMON_IRQ_CMD;
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		if (value == (TRIGGER_LATMON_IRQ_CMD | 0x0)) {
			/* release force freq mode */
			adsp_is_force_trigger_latmon = 0;
			ret = adsp_ipi_send(ADSP_IPI_DVFS_FIX_OPP_SET, &value,
					    sizeof(value), 0, ADSP_A_ID);
		} else if (value == (TRIGGER_LATMON_IRQ_CMD | 0x1)) {
			/* force adsp freq at specific value */
			adsp_is_force_trigger_latmon = 1;
			ret = adsp_ipi_send(ADSP_IPI_DVFS_FIX_OPP_SET, &value,
					    sizeof(value), 0, ADSP_A_ID);
		}
	}
	return count;
}

DEVICE_ATTR_WO(adsp_force_adsppll);
DEVICE_ATTR_WO(adsp_spm_req);
DEVICE_ATTR_WO(adsp_keep_adsppll_on);
DEVICE_ATTR_RW(adsp_dvfs_force_opp);
DEVICE_ATTR_RW(adsp_dvfs_set_freq);
DEVICE_ATTR_RW(adsp_dvfs_trigger_latmon);

static struct attribute *adsp_dvfs_attrs[] = {
#if ADSP_SLEEP_ENABLE
	&dev_attr_adsp_force_adsppll.attr,
	&dev_attr_adsp_spm_req.attr,
	&dev_attr_adsp_keep_adsppll_on.attr,
#endif
	&dev_attr_adsp_dvfs_force_opp.attr,
	&dev_attr_adsp_dvfs_set_freq.attr,
	&dev_attr_adsp_dvfs_trigger_latmon.attr,
	NULL,
};

struct attribute_group adsp_dvfs_attr_group = {
	.attrs = adsp_dvfs_attrs,
};

static bool is_adsp_suspend(void)
{
	return (readl(ADSP_A_SYS_STATUS) == ADSP_STATUS_SUSPEND)
		&& (readl(ADSP_SLEEP_STATUS_REG) & ADSP_A_IS_WFI)
		&& (readl(ADSP_SLEEP_STATUS_REG) & ADSP_A_AXI_BUS_IS_IDLE);
}

DEFINE_MUTEX(adsp_sw_reset_mutex);
static DEFINE_SPINLOCK(adsp_sw_reset_spinlock);
void adsp_sw_reset(void)
{
	unsigned long flags;

	mutex_lock(&adsp_sw_reset_mutex);
	spin_lock_irqsave(&adsp_sw_reset_spinlock, flags);
	writel((ADSP_A_SW_RSTN | ADSP_A_SW_DBG_RSTN), ADSP_A_REBOOT);
	udelay(1);
	writel(0, ADSP_A_REBOOT);
	spin_unlock_irqrestore(&adsp_sw_reset_spinlock, flags);
	mutex_unlock(&adsp_sw_reset_mutex);
}

void adsp_release_runstall(uint32_t release)
{
	uint32_t reg_value;

	reg_value = readl(ADSP_HIFI3_IO_CONFIG);
	if (release) {
		writel(reg_value & ~(ADSP_RELEASE_RUNSTALL),
		       ADSP_HIFI3_IO_CONFIG);
	} else {
		writel(reg_value | (ADSP_RELEASE_RUNSTALL),
		       ADSP_HIFI3_IO_CONFIG);
	}
}

void adsp_set_clock_freq(enum adsp_clk clk)
{
	switch (clk) {
	case CLK_ADSP_CLK26M:
	case CLK_TOP_MMPLL_D4:
	case CLK_TOP_ADSPPLL_D6:
		adsp_set_top_mux(clk);
		break;
	case CLK_TOP_ADSPPLL_D4:
		adsp_set_top_mux(clk);
		break;
	default:
		break;
	}
}

void adsp_A_send_spm_request(uint32_t enable)
{
	int timeout = 1000;

	pr_debug("req spm source & ddr enable(%d)\n", enable);
	if (enable) {
		/* request infra/26M/apsrc/v18/ ddr resource */
		writel(readl(ADSP_A_SPM_REQ) | ADSP_A_SPM_SRC_BITS,
		       ADSP_A_SPM_REQ);
		/* hw auto on/off ddren, disable now due to latency concern */
//		writel(ADSP_A_DDR_REQ_SEL, ADSP_A_DDREN_REQ);
		writel(readl(ADSP_A_DDREN_REQ) | ADSP_A_DDR_ENABLE,
		       ADSP_A_DDREN_REQ);

		/*make sure SPM return ack*/
		while (--timeout) {
			if (readl(ADSP_A_SPM_ACK) ==
			    ((ADSP_A_DDR_ENABLE << 4) | ADSP_A_SPM_SRC_BITS))
		/* hw auto on/off ddren */
//			if (readl(ADSP_A_SPM_ACK) == (ADSP_A_SPM_SRC_BITS))
				break;
			udelay(10);
			if (timeout == 0)
				pr_err("[ADSP] timeout: cannot get SPM ack\n");
		}
	} else {
		writel(readl(ADSP_A_DDREN_REQ) & ~(ADSP_A_DDR_ENABLE),
		       ADSP_A_DDREN_REQ);
		writel(readl(ADSP_A_SPM_REQ) & ~ADSP_A_SPM_SRC_BITS,
		       ADSP_A_SPM_REQ);
		/*make sure SPM return ack*/
		while (--timeout) {
			if (readl(ADSP_A_SPM_ACK) == 0x0)
				break;
			udelay(10);
			if (timeout == 0)
				pr_err("[ADSP] timeout: cannot get SPM ack\n");
		}
	}
}

static void adsp_delay_off_handler(struct timer_list *list)
{
	if (!adsp_feature_is_active())
		queue_work(adsp_workqueue, &adsp_suspend_work.work);
	__pm_relax(adsp_suspend_lock);
}

void adsp_start_suspend_timer(void)
{
	mutex_lock(&adsp_timer_mutex);
	adsp_suspend_timer.function = adsp_delay_off_handler;
	//adsp_suspend_timer.data = (unsigned long)ADSP_DELAY_TIMER;
	adsp_suspend_timer.expires =
		jiffies + ADSP_DELAY_OFF_TIMEOUT;
	if (timer_pending(&adsp_suspend_timer))
		pr_debug("adsp_suspend_timer has set\n");
	else {
		__pm_stay_awake(adsp_suspend_lock);    /* wake lock AP */
		add_timer(&adsp_suspend_timer);
	}
	mutex_unlock(&adsp_timer_mutex);
}

void adsp_stop_suspend_timer(void)
{
	mutex_lock(&adsp_timer_mutex);
	if (timer_pending(&adsp_suspend_timer)) {
		del_timer(&adsp_suspend_timer);
		__pm_relax(adsp_suspend_lock);
	}
	mutex_unlock(&adsp_timer_mutex);
}

/*
 * callback function for work struct
 * @param ws:   work struct
 */

static void adsp_suspend_ws(struct work_struct *ws)
{
	struct adsp_work_t *sws = container_of(ws, struct adsp_work_t, work);
	enum adsp_core_id core_id = sws->id;

	mutex_lock(&adsp_feature_mutex);
	if (!adsp_feature_is_active())
		adsp_suspend(core_id);
	mutex_unlock(&adsp_feature_mutex);
}

#if ADSP_ITCM_MONITOR
static unsigned int adsp_itcm_gtable[9216]; //ADSP_A_ITCM_SIZE/4
#endif

#if ADSP_DTCM_MONITOR
static unsigned int adsp_dtcm_gtable[8192]; //ADPS_A_DTCM_SIZE/4
#endif

void adsp_sram_reset_init(void)
{
#if ADSP_ITCM_MONITOR
	memcpy((void *)(ADSP_A_ITCM),
	       (void *)adsp_itcm_gtable,
	       (size_t)ADSP_A_ITCM_SIZE);
#endif
#if ADSP_DTCM_MONITOR
	memcpy((void *)(ADSP_A_DTCM),
	       (void *)adsp_dtcm_gtable,
	       (size_t)(ADSP_A_DTCM_SIZE - ADSP_A_DTCM_SHARE_SIZE));
#endif
}


void adsp_sram_gtable_init(void)
{
#if ADSP_ITCM_MONITOR
	memcpy((void *)adsp_itcm_gtable,
	       (void *)(ADSP_A_ITCM),
	       (size_t)ADSP_A_ITCM_SIZE);
#endif
#if ADSP_DTCM_MONITOR
	memcpy((void *)adsp_dtcm_gtable,
	       (void *)(ADSP_A_DTCM),
	       (size_t)ADSP_A_DTCM_SIZE);
#endif

}

void adsp_sram_gtable_backup(void)
{
#if ADSP_DTCM_MONITOR
	memcpy((void *)adsp_dtcm_gtable,
	       (void *)(ADSP_A_DTCM),
	       (size_t)ADSP_A_DTCM_SIZE);
#endif
}

int adsp_sram_gtable_check(void)
{
	int ret = 0;
	int need_assert = 0;
#if (ADSP_ITCM_MONITOR || ADSP_DTCM_MONITOR)
	int i;
	u32 __iomem *s;
	void *tcm_src = NULL;
#endif


#if ADSP_ITCM_MONITOR
	tcm_src = vmalloc(sizeof(adsp_itcm_gtable));
	if (!tcm_src)
		return -1;

	memcpy_fromio(tcm_src, (void *)(ADSP_A_ITCM),
		      (size_t)ADSP_A_ITCM_SIZE);
	ret = memcmp((void *)adsp_itcm_gtable,
		     (void *)tcm_src,
		     sizeof(adsp_itcm_gtable));
	if (ret) {
		pr_notice("[%s]memcmp adsp_itcm_gtable != ITCM, ret %d\n",
			  __func__, ret);
		s = (void *)(ADSP_A_ITCM);
		for (i = 0; i < ADSP_A_ITCM_SIZE / 4; i++) {
			if (adsp_itcm_gtable[i] != *s) {
				pr_notice("[%s]adsp_itcm_gtable[%d](0x%x) != ITCM(0x%x)\n",
					  __func__, i, adsp_itcm_gtable[i], *s);
				*s = adsp_itcm_gtable[i];
			}
			s++;
		}
		need_assert = 1;
	}
	if (tcm_src != NULL) {
		vfree(tcm_src);
		tcm_src = NULL;
	}
#endif
#if ADSP_DTCM_MONITOR
	tcm_src = vmalloc(sizeof(adsp_dtcm_gtable));
	if (!tcm_src)
		return -1;

	memcpy_fromio(tcm_src, (void *)(ADSP_A_DTCM),
		      (size_t)ADSP_A_DTCM_SIZE);
	ret = memcmp((void *)adsp_dtcm_gtable,
		     (void *)tcm_src,
		     (size_t)ADSP_A_DTCM_SIZE - ADSP_A_DTCM_SHARE_SIZE);
	if (ret) {
		pr_notice("[%s]memcmp adsp_dtcm_gtable != DTCM, ret %d\n",
			  __func__, ret);
		s = (void *)(ADSP_A_DTCM);
		for (i = 0;
		     i < (ADSP_A_DTCM_SIZE - ADSP_A_DTCM_SHARE_SIZE) / 4;
		     i++) {
			if (adsp_dtcm_gtable[i] != *s) {
				pr_notice("[%s]adsp_dtcm_gtable[%d](0x%x) != DTCM(0x%x)\n",
					  __func__, i, adsp_dtcm_gtable[i], *s);
				*s = adsp_dtcm_gtable[i];
			}
			s++;
		}
		need_assert = 1;
	}
	if (tcm_src != NULL) {
		vfree(tcm_src);
		tcm_src = NULL;
	}
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
	if (need_assert)
		aee_kernel_exception_api(__FILE__,
					 __LINE__,
					 DB_OPT_DEFAULT,
					 "[ADSP]",
					 "ASSERT: adsp tcm check fail!!");
#endif
	return ret;
}

int adsp_suspend_init(void)
{
	INIT_WORK(&adsp_suspend_work.work, adsp_suspend_ws);
	adsp_suspend_lock = wakeup_source_register(NULL,
						   "adsp suspend wakelock");
	timer_setup(&adsp_suspend_timer, adsp_delay_off_handler, 0);
	adsp_is_suspend = 0;
	adsp_sram_gtable_init();
	adsp_sram_gtable_backup();
	return 0;
}

/* actually execute suspend flow, which cannot be disabled.
 * retry the resume flow after adsp_ready = 0.
 */
void adsp_suspend(enum adsp_core_id core_id)
{
	enum adsp_ipi_status ret;
	int value = 0;
	int timeout = 20000;
#if ADSP_DVFS_PROFILE
	ktime_t begin, end;
#endif
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(core_id) == 1) && !adsp_is_suspend) {
#if ADSP_DVFS_PROFILE
		begin = ktime_get();
#endif
		adsp_timesync_suspend(0);

		ret = scp_send_msg_to_queue(AUDIO_OPENDSP_USE_HIFI3_A,
					    ADSP_IPI_DVFS_SUSPEND,
					    &value, sizeof(value), 100);

		while (--timeout && !is_adsp_suspend())
			usleep_range(100, 200);

		if (!is_adsp_suspend()) {
			pr_info("[%s]wait adsp suspend timeout ret(%d,%d)\n",
				__func__, ret, timeout);
#ifdef CFG_RECOVERY_SUPPORT
			adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, core_id);
#else
			adsp_aed(EXCEP_KERNEL, core_id);
#endif
		} else {
			adsp_release_runstall(false);
			adsp_sram_gtable_backup();
			adsp_power_on(false);
			adsp_is_suspend = 1;
		}
		adsp_reset_ready(core_id);
#if ADSP_DVFS_PROFILE
		end = ktime_get();
		pr_debug("[%s]latency = %lld us, ret(%d)\n",
			 __func__, ktime_us_delta(end, begin), ret);
#endif
	}
	mutex_unlock(&adsp_suspend_mutex);
}


int adsp_resume(void)
{
	int retry = 20000;
	int ret = 0;
	int tcm_ret = 0;
#if ADSP_DVFS_PROFILE
	ktime_t begin, end;
#endif
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(ADSP_A_ID) == 0) && adsp_is_suspend) {
#if ADSP_DVFS_PROFILE
		begin = ktime_get();
#endif
		adsp_power_on(true);
		tcm_ret = adsp_sram_gtable_check();
		/* To indicate only main_lite() is needed. */
		writel(MAGIC_PATTERN, CREG_BOOTUP_MARK);
		DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);
		adsp_release_runstall(true);

		/* busy waiting until adsp is ready */
		while (--retry && (is_adsp_ready(ADSP_A_ID) != 1))
			usleep_range(100, 200);

		if (is_adsp_ready(ADSP_A_ID) != 1) {
			pr_warn("[%s]wait for adsp ready timeout\n", __func__);
			/* something wrong , dump adsp */
#ifdef CFG_RECOVERY_SUPPORT
			adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, ADSP_A_ID);
#else
			adsp_aed(EXCEP_KERNEL, ADSP_A_ID);
#endif
			ret = -ETIME;
		}
		adsp_timesync_resume();
		adsp_is_suspend = 0;
#if ADSP_DVFS_PROFILE
		end = ktime_get();
		pr_debug("[%s]latency = %lld us, tcm_check(%d), ret(%d)\n",
			 __func__, ktime_us_delta(end, begin), tcm_ret, ret);
#endif
	}
	mutex_unlock(&adsp_suspend_mutex);
	return ret;
}

void adsp_reset(void)
{
	uint32_t reg_val = 0;
#if ADSP_DVFS_PROFILE
	ktime_t begin = ktime_get();
	ktime_t end;
#endif
	adsp_reset_ready(ADSP_A_ID);
	adsp_release_runstall(false);

	adsp_power_on(false);
	adsp_power_on(true);

	writel(0, CREG_BOOTUP_MARK);
	writel(0x0, ADSP_A_IRQ_EN);
	reg_val = readl(ADSP_A_WDT_REG) & ~(WDT_EN_BIT);
	writel(reg_val, ADSP_A_WDT_REG);

	/** TCM back to initial state **/
	adsp_sram_reset_init();

	dsb(SY);
	adsp_release_runstall(true);
	adsp_is_suspend = 0;
#if ADSP_DVFS_PROFILE
	end = ktime_get();
	pr_debug("[%s]latency = %lld us\n",
		 __func__, ktime_us_delta(end, begin));
#endif
}

void mt_adsp_dvfs_ipi_init(void)
{
}
int __init adsp_dvfs_init(void)
{
	adsp_is_force_freq = 0;
	adsp_is_force_trigger_latmon = 0;

	adspreg.active_clksrc = CLK_TOP_ADSPPLL_D6;

	return 0;
}
void __exit adsp_dvfs_exit(void)
{
}


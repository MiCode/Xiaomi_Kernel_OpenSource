// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

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
#include <mt-plat/mtk_secure_api.h>
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
struct completion adsp_suspend_cp;
struct completion adsp_resume_cp;
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
/* if equal, bypass clear bss and some init */
#define MAGIC_PATTERN      (0xfafafafa)

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
	token2 = strsep(&pin, delim);

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
void adsp_sw_reset(enum adsp_core_id core_id)
{
	unsigned long flags;
	uint32_t      reg_value, rstn_bits;

	if (core_id == ADSP_A_ID)
		rstn_bits = (ADSP_A_SW_RSTN | ADSP_A_SW_DBG_RSTN);
	else if (core_id == ADSP_B_ID)
		rstn_bits = (ADSP_B_SW_RSTN | ADSP_B_SW_DBG_RSTN);
	mutex_lock(&adsp_sw_reset_mutex);
	spin_lock_irqsave(&adsp_sw_reset_spinlock, flags);
	reg_value = readl(ADSP_CFGREG_SW_RSTN);
	writel(reg_value | rstn_bits, ADSP_CFGREG_SW_RSTN);
	udelay(1);
	writel(reg_value, ADSP_CFGREG_SW_RSTN);
	spin_unlock_irqrestore(&adsp_sw_reset_spinlock, flags);
	mutex_unlock(&adsp_sw_reset_mutex);
}

void adsp_release_runstall(enum adsp_core_id core_id, uint32_t release)
{
	uint32_t reg_value;
	uint32_t runstall_bit = (core_id == ADSP_A_ID) ?
				ADSP_A_RUNSTALL : ADSP_B_RUNSTALL;

	reg_value = readl(ADSP_HIFI3_IO_CONFIG);
	if (release) {
		writel(reg_value & ~(runstall_bit),
		       ADSP_HIFI3_IO_CONFIG);
	} else {
		writel(reg_value | runstall_bit,
		       ADSP_HIFI3_IO_CONFIG);
	}
}

void adsp_A_send_spm_request(uint32_t enable)
{
	int timeout = 1000;

	pr_debug("req spm source & ddr enable(%d)\n", enable);
	if (enable) {
		/* request infra/26M/apsrc/v18/ ddr resource */
		writel(readl(ADSP_A_DDREN_REQ) | ADSP_SPM_SRC_BITS,
		       ADSP_A_DDREN_REQ);
		/* hw auto on/off ddren, disable now due to latency concern */
//		writel(ADSP_A_DDR_REQ_SEL, ADSP_A_DDREN_REQ);

		/*make sure SPM return ack*/
		while (--timeout) {
		/* hw auto on/off ddren */
			if (readl(ADSP_SPM_ACK) == ADSP_SPM_SRC_BITS)
				break;
			udelay(10);
			if (timeout == 0)
				pr_err("[ADSP] timeout: cannot get SPM ack\n");
		}
	} else {
		writel(readl(ADSP_A_DDREN_REQ) & ~(ADSP_SPM_SRC_BITS),
		       ADSP_A_DDREN_REQ);
		/*make sure SPM return ack*/
		while (--timeout) {
			if (readl(ADSP_SPM_ACK) == 0x0)
				break;
			udelay(10);
			if (timeout == 0)
				pr_err("[ADSP] timeout: cannot get SPM ack\n");
		}
	}
}

static void adsp_delay_off_handler(unsigned long data)
{
	if (!adsp_feature_is_active())
		queue_work(adsp_workqueue, &adsp_suspend_work.work);
}

void adsp_start_suspend_timer(void)
{
	mutex_lock(&adsp_timer_mutex);
	adsp_suspend_timer.function = adsp_delay_off_handler;
	adsp_suspend_timer.data = (unsigned long)ADSP_DELAY_TIMER;
	adsp_suspend_timer.expires =
		jiffies + ADSP_DELAY_OFF_TIMEOUT;
	if (timer_pending(&adsp_suspend_timer))
		pr_debug("adsp_suspend_timer has set\n");
	else
		add_timer(&adsp_suspend_timer);
	mutex_unlock(&adsp_timer_mutex);
}

void adsp_stop_suspend_timer(void)
{
	mutex_lock(&adsp_timer_mutex);
	if (timer_pending(&adsp_suspend_timer))
		del_timer(&adsp_suspend_timer);
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

#if ADSP_CFG_MONITOR
static unsigned int adsp_cfg_gtable[5312];
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
#if ADSP_CFG_MONITOR
	/* Enable ADSP CLK and UART to avoid bus hang */
	clk_cfg = readl(ADSP_CLK_CTRL_BASE);
	uart_cfg = readl(ADSP_UART_CTRL);
	writel(readl(ADSP_CLK_CTRL_BASE) | ADSP_CLK_UART_EN,
		       ADSP_CLK_CTRL_BASE);
	writel(readl(ADSP_UART_CTRL) | ADSP_UART_RST_N |
		       ADSP_UART_BCLK_CG, ADSP_UART_CTRL);

	memcpy((void *)adsp_cfg_gtable,
	       (void *)(ADSP_A_CFG),
	       (size_t)21248);

	/* Restore ADSP CLK and UART setting */
	writel(clk_cfg, ADSP_CLK_CTRL_BASE);
	writel(uart_cfg, ADSP_UART_CTRL);
#endif
}

int adsp_sram_gtable_check(void)
{
	int ret = 0;
	int i;
	int need_assert = 0;
	u32 __iomem *s;
#if ADSP_ITCM_MONITOR
	ret = memcmp((void *)adsp_itcm_gtable,
		     (void *)(ADSP_A_ITCM),
		     (size_t)ADSP_A_ITCM_SIZE);
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
#endif
#if ADSP_DTCM_MONITOR
	ret = memcmp((void *)adsp_dtcm_gtable,
		     (void *)(ADSP_A_DTCM),
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
#endif
#if ADSP_CFG_MONITOR
	ret = memcmp((void *)adsp_cfg_gtable,
		     (void *)(ADSP_A_CFG),
		     (size_t)21248);
	if (ret) {
		pr_notice("[%s]memcmp adsp_cfg_gtable != CFG, ret %d\n",
			  __func__, ret);
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

void adsp_suspend_ipi_handler(int id, void *data, unsigned int len)
{
	if (!adsp_is_suspend)
		complete(&adsp_suspend_cp);
}

int adsp_suspend_init(void)
{
	INIT_WORK(&adsp_suspend_work.work, adsp_suspend_ws);
	init_timer(&adsp_suspend_timer);
	init_completion(&adsp_suspend_cp);
	init_completion(&adsp_resume_cp);
	adsp_is_suspend = 0;
	adsp_sram_gtable_init();
	adsp_sram_gtable_backup();

	adsp_ipi_registration(ADSP_IPI_DVFS_SUSPEND, adsp_suspend_ipi_handler,
			      "adsp_suspend_ack");
	return 0;
}

/* actually execute suspend flow, which cannot be disabled.
 * retry the resume flow after adsp_ready = 0.
 */
void adsp_suspend(enum adsp_core_id core_id)
{
	enum adsp_ipi_status ret;
	int value = 0;
#if ADSP_DVFS_PROFILE
	ktime_t begin, end;
#endif
	mutex_lock(&adsp_suspend_mutex);
	if ((is_adsp_ready(core_id) == 1) && !adsp_is_suspend) {
#if ADSP_DVFS_PROFILE
		begin = ktime_get();
#endif
		ret = scp_send_msg_to_queue(AUDIO_OPENDSP_USE_HIFI3_A,
					    ADSP_IPI_DVFS_SUSPEND,
					    &value, sizeof(value), 100);

		wait_for_completion_timeout(&adsp_suspend_cp,
					    msecs_to_jiffies(2000));

		if (!is_adsp_suspend()) {
			pr_warn("[%s]wait adsp suspend timeout\n", __func__);
#ifdef CFG_RECOVERY_SUPPORT
			adsp_send_reset_wq(ADSP_RESET_TYPE_AWAKE, core_id);
#else
			adsp_aed(EXCEP_KERNEL, core_id);
#endif
		} else {
			adsp_release_runstall(ADSP_A_ID, false);
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
		writel(MAGIC_PATTERN, ADSP_A_BOOTUP_MARK);
		writel(MAGIC_PATTERN, ADSP_B_BOOTUP_MARK);
		DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);
		adsp_release_runstall(ADSP_A_ID, true);
		adsp_release_runstall(ADSP_B_ID, true);

		/* busy waiting until adsp is ready */
		wait_for_completion_timeout(&adsp_resume_cp,
					    msecs_to_jiffies(2000));

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
	int itcm_ret = 0;
#if ADSP_DVFS_PROFILE
	ktime_t begin, end;
#endif
#if ADSP_DVFS_PROFILE
	begin = ktime_get();
#endif
	adsp_reset_ready(ADSP_A_ID);
	adsp_reset_ready(ADSP_B_ID);
	adsp_release_runstall(ADSP_A_ID, false);
	adsp_release_runstall(ADSP_B_ID, false);

	adsp_power_on(false);
	adsp_power_on(true);

	writel(0, ADSP_A_BOOTUP_MARK);
	writel(0x0, ADSP_A_IRQ_EN);
	writel(0x0, ADSP_B_IRQ_EN);
	DRV_ClrReg32(ADSP_A_WDT_REG, WDT_EN_BIT);

	/** TCM back to initial state **/
	adsp_sram_reset_init();

	dsb(SY);
	adsp_release_runstall(ADSP_A_ID, true);
	adsp_is_suspend = 0;
#if ADSP_DVFS_PROFILE
	end = ktime_get();
	pr_debug("[%s]latency = %lld us, itcm_check(%d)\n",
		 __func__, ktime_us_delta(end, begin),
		 itcm_ret);
#endif
}

void mt_adsp_dvfs_ipi_init(void)
{
}
int __init adsp_dvfs_init(void)
{
	adsp_is_force_freq = 0;
	adsp_is_force_trigger_latmon = 0;
	return 0;
}
void __exit adsp_dvfs_exit(void)
{
}


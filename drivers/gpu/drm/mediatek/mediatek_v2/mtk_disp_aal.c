// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif
#define MT65XX_LED_MODE_NONE (0)
#define MT65XX_LED_MODE_CUST_LCM (4)

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"
#include "mtk_drm_mmp.h"

#undef pr_fmt
#define pr_fmt(fmt) "[disp_aal]" fmt

// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *aal1_default_comp;
static struct task_struct *aal_sof_irq_event_task;

//For 120Hz rotation issue
struct timespec64 start, end;

/* To enable debug log: */
/* # echo aal_dbg:1 > /sys/kernel/debug/dispsys */
int aal_dbg_en;

static DECLARE_WAIT_QUEUE_HEAD(g_aal_hist_wq);
static DECLARE_WAIT_QUEUE_HEAD(g_aal_sof_irq_wq);
static DEFINE_SPINLOCK(g_aal_clock_lock);
static DEFINE_SPINLOCK(g_aal_hist_lock);
static DEFINE_SPINLOCK(g_aal_irq_en_lock);

static struct DISP_AAL_HIST g_aal_hist = {
	.serviceFlags = 0,
	.backlight = -1,
	.essStrengthIndex = ESS_LEVEL_BY_CUSTOM_LIB,
	.ess_enable = ESS_EN_BY_CUSTOM_LIB,
	.dre_enable = DRE_EN_BY_CUSTOM_LIB
};

static struct DISP_AAL_HIST g_aal_hist_db;
//static ddp_module_notify g_ddp_notify;
static atomic_t g_aal0_hist_available = ATOMIC_INIT(0);
static atomic_t g_aal1_hist_available = ATOMIC_INIT(0);
static atomic_t g_aal0_dre20_hist_is_ready = ATOMIC_INIT(0);
static atomic_t g_aal1_dre20_hist_is_ready = ATOMIC_INIT(0);
static atomic_t g_aal_sof_irq_available = ATOMIC_INIT(0);
static atomic_t g_aal_is_init_regs_valid = ATOMIC_INIT(0);
static atomic_t g_aal_backlight_notified = ATOMIC_INIT(1023);
static atomic_t g_aal_initialed = ATOMIC_INIT(0);
static atomic_t g_aal_allowPartial = ATOMIC_INIT(0);
static atomic_t g_aal_force_enable_irq = ATOMIC_INIT(0);
static atomic_t g_led_mode = ATOMIC_INIT(MT65XX_LED_MODE_NONE);
static atomic_t g_aal_force_relay = ATOMIC_INIT(0);
static atomic_t g_aal_eof_irq = ATOMIC_INIT(0);
static atomic_t g_aal1_eof_irq = ATOMIC_INIT(0);
static atomic_t g_aal_first_frame = ATOMIC_INIT(1);
static atomic_t g_aal1_first_frame = ATOMIC_INIT(1);
static atomic_t g_aal_dre30_write = ATOMIC_INIT(1);
static atomic_t g_aal_interrupt_enabled = ATOMIC_INIT(0);
static atomic_t g_force_delay_check_trig = ATOMIC_INIT(0);
static struct workqueue_struct *aal_flip_wq;
static struct workqueue_struct *aal_refresh_wq;

enum AAL_UPDATE_HIST {
	UPDATE_NONE = 0,
	UPDATE_SINGLE,
	UPDATE_MULTIPLE
};
/* #define DRE3_IN_DISP_AAL */
/* HW specified */
#define AAL_DRE_HIST_START	(1152)
#define AAL_DRE_HIST_END	(4220)
#define AAL_DRE_GAIN_START	(4224)
#define AAL_DRE_GAIN_END	(6396)

static DEFINE_SPINLOCK(g_aal_dre3_gain_lock);
static atomic_t g_aal_force_hist_apb = ATOMIC_INIT(0);
static atomic_t g_aal1_force_hist_apb = ATOMIC_INIT(0);
static atomic_t g_aal_dre_halt = ATOMIC_INIT(0);
static atomic_t g_aal_dre_hw_init = ATOMIC_INIT(0);
static atomic_t g_aal1_dre_hw_init = ATOMIC_INIT(0);

static struct DISP_DRE30_INIT g_aal_init_dre30;
static struct DISP_DRE30_PARAM g_aal_gain;
static struct DISP_DRE30_PARAM g_aal_gain_db;
static struct DISP_DRE30_HIST g_aal_dre30_hist;
static struct DISP_DRE30_HIST g_aal_dre30_hist_db;

static atomic_t g_aal_change_to_dre30 = ATOMIC_INIT(0);
static atomic_t g_aal_dre_config = ATOMIC_INIT(0);
static atomic_t g_aal1_dre_config = ATOMIC_INIT(0);
#define AAL_SRAM_SOF 1
#define AAL_SRAM_EOF 0
static u32 aal_sram_method = AAL_SRAM_SOF;

static DECLARE_WAIT_QUEUE_HEAD(g_aal_size_wq);
static bool g_aal_get_size_available;
static struct DISP_AAL_DISPLAY_SIZE g_aal_size;
static struct DISP_AAL_DISPLAY_SIZE g_dual_aal_size;

static atomic_t g_aal_panel_type = ATOMIC_INIT(CONFIG_BY_CUSTOM_LIB);
static int g_aal_ess_level = ESS_LEVEL_BY_CUSTOM_LIB;
static int g_aal_dre_en = DRE_EN_BY_CUSTOM_LIB;
static int g_aal_ess_en = ESS_EN_BY_CUSTOM_LIB;
static int g_aal_ess_level_cmd_id;
static int g_aal_dre_en_cmd_id;
static int g_aal_ess_en_cmd_id;
#define aal_min(a, b)			(((a) < (b)) ? (a) : (b))

static bool isDualPQ;
enum AAL_IOCTL_CMD {
	INIT_REG = 0,
	SET_PARAM,
	FLIP_SRAM,
	BYPASS_AAL,
	SET_CLARITY_REG
};

struct dre3_node {
	struct device *dev;
	void __iomem *va;
	phys_addr_t pa;
	struct clk *clk;
};

struct mtk_disp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	struct dre3_node dre3_hw;
	atomic_t dirty_frame_retrieved;
	atomic_t is_clock_on;
	const struct mtk_disp_aal_data *data;
	struct work_struct aal_flip_task;
	struct work_struct aal_refresh_task;
};

struct mtk_aal_feature_option {
	unsigned int mtk_aal_support;
	unsigned int mtk_dre30_support;
};

static struct mtk_disp_aal *g_aal_data;
static struct mtk_disp_aal *g_aal1_data;
static struct mtk_aal_feature_option *g_aal_fo;
static DEFINE_MUTEX(g_aal_sram_lock);
static bool gDre30Enabled;
static bool gPrvDre30Enabled;
static unsigned int g_aal_dre30_en;

// for Display Clarity
static int g_aal_clarity_support;
static int g_tdshp_clarity_support;
int g_disp_clarity_support;
static struct DISP_CLARITY_REG *g_disp_clarity_regs;
static DEFINE_MUTEX(g_clarity_lock);

static inline struct mtk_disp_aal *comp_to_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_aal, ddp_comp);
}

static inline phys_addr_t mtk_aal_dre3_pa(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.pa : comp->regs_pa;
}

static inline void __iomem *mtk_aal_dre3_va(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	return (aal_data->dre3_hw.dev) ? aal_data->dre3_hw.va : comp->regs;
}

static void mtk_aal_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

#define AALERR(fmt, arg...) pr_notice("[ERR]%s:" fmt, __func__, ##arg)

static bool debug_flow_log;
#define AALFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		pr_notice("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define AALAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		pr_notice("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_write_cmdq_log;
#define AALWC_LOG(fmt, arg...) do { \
	if (debug_write_cmdq_log) \
		pr_notice("[WC]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_irq_log;
#define AALIRQ_LOG(fmt, arg...) do { \
	if (debug_irq_log) \
		pr_notice("[IRQ]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_dump_clarity_regs;
#define AALCRT_LOG(fmt, arg...) do { \
	if (debug_dump_clarity_regs) \
		pr_notice("[Clarity Debug]%s:" fmt, __func__, ##arg); \
	} while (0)

/* config register which might have extra DRE3 aal hw */
static inline s32 basic_cmdq_write(struct cmdq_pkt *handle,
	struct mtk_ddp_comp *comp, u32 offset, u32 value, u32 mask)
{
	if (g_aal_fo->mtk_dre30_support) {
		s32 result;
		struct mtk_disp_aal *aal_data = comp_to_aal(comp);
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		result = cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + offset, value, mask);
		if (result) {
			AALERR("write reg fail, offset:%#x\n", offset);
			return result;
		}
		AALWC_LOG("write 0x%03x with 0x%08x (0x%08x)\n",
			offset, value, mask);
		if (aal_data->dre3_hw.dev)
			result = cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + offset, value, mask);
		return result;
	} else {
		return cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + offset, value, mask);
	}
}

static int disp_aal_get_cust_led(void)
{
	struct device_node *led_node = NULL;
	int ret = 0;
	int led_mode;
	int pwm_config[5] = { 0 };

	led_node = of_find_compatible_node(NULL, NULL,
	"mediatek,lcd-backlight");
	if (!led_node) {
		ret = -1;
		pr_notice("Cannot find LED node from dts\n");
	} else {
		ret = of_property_read_u32(led_node, "led_mode", &led_mode);
		if (!ret)
			atomic_set(&g_led_mode, led_mode);
		else
			pr_notice("led dts can not get led mode data.\n");

		ret = of_property_read_u32_array(led_node,
	    "pwm_config", pwm_config, ARRAY_SIZE(pwm_config));
	}

	if (ret)
		pr_notice("get pwm cust info fail\n");
	pr_notice("%s mode=%u\n", __func__, atomic_read(&g_led_mode));

	return ret;
}


#define LOG_INTERVAL_TH 200
#define LOG_BUFFER_SIZE 4
static char g_aal_log_buffer[256] = "";
static int g_aal_log_index;
struct timespec64 g_aal_log_prevtime = {0};

bool disp_aal_is_support(void)
{
	if (g_aal_fo->mtk_aal_support)
		return true;
	else
		return false;
}

static void disp_aal_set_interrupt(struct mtk_ddp_comp *comp, int enable)
{
	struct mtk_disp_aal *aal_data = NULL;

	if (comp != NULL)
		aal_data = comp_to_aal(comp);
	else
		aal_data = comp_to_aal(default_comp);

	if (!disp_aal_is_support()) {
		AALIRQ_LOG("aal is not support\n");
		return;
	}

	if (enable && (atomic_read(&g_aal_force_relay) != 1 ||
		m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS] ||
		m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])) {
		/* Enable output frame end interrupt */
		if (comp == NULL)
			writel(0x2, default_comp->regs + DISP_AAL_INTEN);
		else
			writel(0x2, comp->regs + DISP_AAL_INTEN);

		atomic_set(&g_aal_interrupt_enabled, 1);
		AALIRQ_LOG("interrupt enabled\n");
	} else if (!enable) {
		if (atomic_read(&aal_data->dirty_frame_retrieved) == 1) {
			if (comp == NULL)
				writel(0x0, default_comp->regs + DISP_AAL_INTEN);
			else
				writel(0x0, comp->regs + DISP_AAL_INTEN);

			atomic_set(&g_aal_interrupt_enabled, 0);
			AALIRQ_LOG("interrupt disabled");
		} //else {
			/* Dirty histogram was not retrieved. */
			/* Only if the dirty hist was retrieved, */
			/* interrupt can be disabled. */
			/* Continue interrupt until AALService can get */
			/* the latest histogram. */
		//}
	}
}

static unsigned long timevaldiff(struct timespec64 *starttime,
	struct timespec64 *finishtime)
{
	unsigned long msec;

	msec = (finishtime->tv_sec-starttime->tv_sec)*1000;
	msec += (finishtime->tv_nsec-starttime->tv_nsec) / NSEC_PER_USEC / 1000;

	return msec;
}

static void disp_aal_notify_backlight_log(int bl_1024)
{
	struct timespec64 aal_time;
	unsigned long diff_mesc = 0;
	unsigned long tsec;
	unsigned long tusec;

	ktime_get_ts64(&aal_time);
	tsec = (unsigned long)aal_time.tv_sec % 100;
	tusec = (unsigned long)aal_time.tv_nsec / NSEC_PER_USEC / 1000;

	diff_mesc = timevaldiff(&g_aal_log_prevtime, &aal_time);
	if (!debug_api_log)
		return;
	pr_notice("time diff = %lu\n", diff_mesc);

	if (diff_mesc > LOG_INTERVAL_TH) {
		if (g_aal_log_index == 0) {
			pr_notice("%s: %d/1023\n", __func__, bl_1024);
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
					"%s, %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			pr_notice("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	} else {
		if (g_aal_log_index == 0) {
			sprintf(g_aal_log_buffer,
					"%s %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
					"%s, %d/1023 %03lu.%03lu", __func__,
					bl_1024, tsec, tusec);
			g_aal_log_index += 1;
		}

		if ((g_aal_log_index >= LOG_BUFFER_SIZE) || (bl_1024 == 0)) {
			pr_notice("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	}

	memcpy(&g_aal_log_prevtime, &aal_time, sizeof(struct timespec64));
}

void disp_aal_refresh_by_kernel(void)
{
	unsigned long flags, clockflags;

	if (atomic_read(&g_aal_is_init_regs_valid) == 1) {
		spin_lock_irqsave(&g_aal_irq_en_lock, flags);
		atomic_set(&g_aal_force_enable_irq, 1);

		spin_lock_irqsave(&g_aal_clock_lock, clockflags);
		if (atomic_read(&g_aal_data->is_clock_on) != 1)
			AALFLOW_LOG("aal clock is off\n");
		else if (isDualPQ && atomic_read(&g_aal1_data->is_clock_on) != 1)
			AALFLOW_LOG("aal1 clock is off\n");
		else
			disp_aal_set_interrupt(NULL, true);
		spin_unlock_irqrestore(&g_aal_clock_lock, clockflags);

		spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);
		/* Backlight or Kernel API latency should be smallest */
		if (atomic_read(&g_force_delay_check_trig) == 1)
			mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		else
			mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
	}
}

void disp_aal_notify_backlight_changed(int trans_backlight, int max_backlight)
{
	unsigned long flags;
	unsigned int service_flags;
	int prev_backlight;

	if (default_comp == NULL || default_comp->mtk_crtc == NULL) {
		AALERR("%s null pointer!\n", __func__);
		return;
	}

	AALAPI_LOG("%d/%d\n", trans_backlight, max_backlight);
	disp_aal_notify_backlight_log(trans_backlight);
	//disp_aal_exit_idle(__func__, 1);

	// FIXME
	//max_backlight = disp_pwm_get_max_backlight(DISP_PWM0);
	AALAPI_LOG("max_backlight = %d", max_backlight);

	if (trans_backlight > max_backlight)
		trans_backlight = max_backlight;

	prev_backlight = atomic_read(&g_aal_backlight_notified);
	atomic_set(&g_aal_backlight_notified, trans_backlight);

	service_flags = 0;
	if ((prev_backlight == 0) && (prev_backlight != trans_backlight))
		service_flags = AAL_SERVICE_FORCE_UPDATE;

	if (trans_backlight == 0) {
		mtk_leds_brightness_set("lcd-backlight", 0, 0, (0X1<<SET_BACKLIGHT_LEVEL));
		/* set backlight = 0 may be not from AAL, */
		/* we have to let AALService can turn on backlight */
		/* on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (atomic_read(&g_aal_is_init_regs_valid) == 0 ||
		(atomic_read(&g_aal_force_relay) == 1 &&
		!m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS] &&
		!m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])) {
		/* AAL Service is not running */

		mtk_leds_brightness_set("lcd-backlight", trans_backlight,
					0, (0X1<<SET_BACKLIGHT_LEVEL));
	}

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist.backlight = trans_backlight;
	g_aal_hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);
	// always notify aal service for LED changed
	mtk_drm_idlemgr_kick(__func__, &default_comp->mtk_crtc->base, 1);

	disp_aal_refresh_by_kernel();
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event_to_aal(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_conf_info *led_conf;

	led_conf = (struct led_conf_info *)v;

	switch (event) {
	case LED_BRIGHTNESS_CHANGED:
		if (m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS]) {
			trans_level = led_conf->cdev.brightness;

			disp_aal_notify_backlight_changed(trans_level,
				led_conf->cdev.max_brightness);
		} else {
			trans_level = (
				led_conf->max_hw_brightness
				* led_conf->cdev.brightness
				+ (led_conf->cdev.max_brightness / 2))
				/ led_conf->cdev.max_brightness;
			if (led_conf->cdev.brightness != 0 &&
				trans_level == 0)
				trans_level = 1;

			disp_aal_notify_backlight_changed(trans_level,
				led_conf->max_hw_brightness);
		}

		AALAPI_LOG("brightness changed: %d(%d)\n",
			trans_level, led_conf->cdev.brightness);
		led_conf->aal_enable = 1;
		break;
	case LED_STATUS_SHUTDOWN:
		if (m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])
			disp_aal_notify_backlight_changed(0,
				led_conf->cdev.max_brightness);
		else
			disp_aal_notify_backlight_changed(0,
				led_conf->max_hw_brightness);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event_to_aal,
};
#endif

int mtk_drm_ioctl_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	int ret = 0;
	unsigned long flags;
	int *enabled = (int *)data;
	int retry = 5;

	AALFLOW_LOG("%d\n", *enabled);

	if (*enabled) {
		mtk_drm_idlemgr_kick(__func__,
				&default_comp->mtk_crtc->base, 1);
		if (atomic_read(&g_force_delay_check_trig) == 1)
			mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		else
			mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);

		while ((atomic_read(&aal_data->is_clock_on) != 1) && (retry != 0)) {
			usleep_range(500, 1000);
			retry--;
		}
	}

	spin_lock_irqsave(&g_aal_irq_en_lock, flags);
	if (atomic_read(&g_aal_force_enable_irq) == 1) {
		if (*enabled == 0)
			AALFLOW_LOG("force enable aal ieq 0 -> 1\n");
		*enabled = 1;
	}
	//if (spin_trylock_irqsave(&g_aal_clock_lock, clockflags)) {
		if (atomic_read(&aal_data->is_clock_on) != 1) {
			AALFLOW_LOG("clock is off\n");
			ret = -EFAULT;
		} else
			disp_aal_set_interrupt(NULL, *enabled);
		//spin_unlock_irqrestore(&g_aal_clock_lock, clockflags);
	//}
	spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);
/*
 *	if (*enabled) {
 *		if (g_aal_fo->mtk_dre30_support) {
 *			struct drm_crtc *crtc = private->crtc[0];
 *			mtk_crtc_user_cmd(crtc, comp, EVENTCTL, data);
 *			mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
 *		} else {
 *			mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
 *		}
 *	}
 */
/*	if (*enabled) {
 *		mtk_drm_idlemgr_kick(__func__,
 *			&default_comp->mtk_crtc->base, 1);
 *		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
 *	}
 */

	return ret;
}

static void mtk_crtc_user_cmd_work(struct work_struct *work_item)
{
	mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
}

static void mtk_disp_aal_refresh_trigger(struct work_struct *work_item)
{
	AALFLOW_LOG("start");

	if (atomic_read(&g_force_delay_check_trig) == 1)
		mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
	else
		mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
}

void disp_aal_flip_sram(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	const char *caller)
{
	u32 hist_apb = 0, hist_int = 0, sram_cfg = 0;
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

	if (!g_aal_fo->mtk_dre30_support || !gDre30Enabled)
		return;

	if (aal_sram_method != AAL_SRAM_SOF)
		return;

	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_AAL0) {
			if (atomic_read(&g_aal_dre_config) == 1 &&
				!atomic_read(&g_aal_first_frame)) {
				AALFLOW_LOG("[SRAM] %s g_aal_dre_config not 0 in %s",
						mtk_dump_comp_str_id(comp->id), caller);
				return;
			}

			atomic_set(&g_aal_dre_config, 1);
			if (atomic_cmpxchg(&g_aal_force_hist_apb, 0, 1) == 0) {
				hist_apb = 0;
				hist_int = 1;
			} else if (atomic_cmpxchg(&g_aal_force_hist_apb, 1, 0) == 1) {
				hist_apb = 1;
				hist_int = 0;
			} else {
				AALERR("[SRAM] Error when get hist_apb in %s", caller);
			}
		} else if (comp->id == DDP_COMPONENT_AAL1) {
			if (atomic_read(&g_aal1_dre_config) == 1 &&
				!atomic_read(&g_aal1_first_frame)) {
				AALFLOW_LOG("[SRAM] %s g_aal1_dre_config not 0 in %s",
						mtk_dump_comp_str_id(comp->id), caller);
				return;
			}
			atomic_set(&g_aal1_dre_config, 1);
			if (atomic_cmpxchg(&g_aal1_force_hist_apb, 0, 1) == 0) {
				hist_apb = 0;
				hist_int = 1;
			} else if (atomic_cmpxchg(&g_aal1_force_hist_apb, 1, 0) == 1) {
				hist_apb = 1;
				hist_int = 0;
			} else {
				AALERR("[SRAM] Error when get hist_apb in %s", caller);
			}
		}
	} else {
		if (atomic_read(&g_aal_dre_config) == 1 &&
				!atomic_read(&g_aal_first_frame)) {
			AALFLOW_LOG("[SRAM] g_aal_dre_config not 0 in %s", caller);
			return;
		}
		atomic_set(&g_aal_dre_config, 1);
		if (atomic_cmpxchg(&g_aal_force_hist_apb, 0, 1) == 0) {
			hist_apb = 0;
			hist_int = 1;
		} else if (atomic_cmpxchg(&g_aal_force_hist_apb, 1, 0) == 1) {
			hist_apb = 1;
			hist_int = 0;
		} else {
			AALERR("[SRAM] Error when get hist_apb in %s", caller);
		}
	}
	sram_cfg = (hist_int << 6)|(hist_apb << 5)|(1 << 4);
	AALFLOW_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x comp_id[%d] in %s",
		hist_apb, hist_int, sram_cfg, comp->id, caller);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_SRAM_CFG, sram_cfg, (0x7 << 4));
}

static void mtk_aal_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	AALFLOW_LOG("+ comd id :%d\n", comp->id);
	if (disp_aal_is_support() == true &&
		atomic_read(&g_aal_force_relay) != 1) {
		AALFLOW_LOG("Enable AAL histogram\n");
		// Enable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x3 << 1, (0x3 << 1));
	} else {
		AALFLOW_LOG("Disable AAL histogram\n");
		// Disable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x0 << 1, (0x3 << 1));
	}

	/* get lcd-backlight mode from dts */
	if (atomic_read(&g_led_mode) == MT65XX_LED_MODE_NONE)
		disp_aal_get_cust_led();

	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_AAL0) {
			atomic_set(&g_aal0_hist_available, 0);
			atomic_set(&g_aal0_dre20_hist_is_ready, 0);
		} else if (comp->id == DDP_COMPONENT_AAL1) {
			atomic_set(&g_aal1_hist_available, 0);
			atomic_set(&g_aal1_dre20_hist_is_ready, 0);
		}
	} else {
		atomic_set(&g_aal0_hist_available, 0);
		atomic_set(&g_aal0_dre20_hist_is_ready, 0);
	}
	atomic_set(&g_aal_sof_irq_available, 0);
	atomic_set(&g_aal_eof_irq, 0);
	atomic_set(&g_aal1_eof_irq, 0);
	atomic_set(&aal_data->dirty_frame_retrieved, 1);
	AALFLOW_LOG("led mode: %d-\n", atomic_read(&g_led_mode));
}

static bool debug_bypass_alg_mode;
static void mtk_aal_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = 0;
	int width = cfg->w, height = cfg->h;

	if (comp->mtk_crtc->is_dual_pipe) {
		isDualPQ = true;
		width = cfg->w / 2;
	} else {
		isDualPQ = false;
		width = cfg->w;
	}

	AALFLOW_LOG("(w,h)=(%d,%d)+, %d\n",
		width, height, g_aal_get_size_available);

	g_aal_size.height = height;
	g_aal_size.width = width;
	g_dual_aal_size.height = height;
	g_dual_aal_size.width = cfg->w;
	g_aal_size.isdualpipe = isDualPQ;
	g_dual_aal_size.isdualpipe = isDualPQ;
	if (g_aal_get_size_available == false) {
		g_aal_get_size_available = true;
		wake_up_interruptible(&g_aal_size_wq);
		AALFLOW_LOG("size available: (w,h)=(%d,%d)+\n", width, height);
	}
	val = (width << 16) | (height);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_OUTPUT_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
		(0 << 16) | 0, ~0);

	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, (0x1 << 8), (0x1 << 8));
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, (0x0 << 8), (0x1 << 8));
	else
		DDPINFO("Display AAL's bit is : %u\n", cfg->bpc);

	if (atomic_read(&g_aal_force_relay) == 1) {
		// Set reply mode
		AALFLOW_LOG("g_aal_force_relay\n");
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 1, 1);
	} else {
		// Disable reply mode
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0, 1);
	}

	if (g_aal_fo->mtk_dre30_support && g_disp_clarity_support) {
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + MDP_AAL_DRE_BILATERAL_STATUS_CTRL,
			0x3 << 1, 0x3 << 1);
	}

	mtk_aal_init(comp, cfg, handle);
	//disp_aal_flip_sram(comp, handle, __func__);

	AALWC_LOG("AAL_CFG=0x%x  compid:%d\n",
		readl(comp->regs + DISP_AAL_CFG), comp->id);
}

static void disp_aal_wait_hist(void)
{
	int ret = 0;

	if (isDualPQ) {
		if ((atomic_read(&g_aal0_hist_available) == 0) ||
				(atomic_read(&g_aal1_hist_available) == 0))
			ret = wait_event_interruptible(g_aal_hist_wq,
					(atomic_read(&g_aal0_hist_available) == 1) &&
					(atomic_read(&g_aal1_hist_available) == 1));
		AALFLOW_LOG("aal0 and aal1 hist_available = 1, waken up, ret = %d", ret);
	} else if (atomic_read(&g_aal0_hist_available) == 0) {
		AALFLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(g_aal_hist_wq,
				atomic_read(&g_aal0_hist_available) == 1);
		AALFLOW_LOG("hist_available = 1, waken up, ret = %d", ret);
	} else
		AALFLOW_LOG("hist_available = 0");
}

static void disp_aal_dump_clarity_regs(uint32_t pipe)
{
	if (debug_dump_clarity_regs && (pipe == 0)) {
		DDPMSG("%s(aal0): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			g_aal_hist.aal0_clarity[0], g_aal_hist.aal0_clarity[1],
			g_aal_hist.aal0_clarity[2], g_aal_hist.aal0_clarity[3],
			g_aal_hist.aal0_clarity[4], g_aal_hist.aal0_clarity[5],
			g_aal_hist.aal0_clarity[6]);
		DDPMSG("%s(tdshp0): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			g_aal_hist.tdshp0_clarity[0], g_aal_hist.tdshp0_clarity[2],
			g_aal_hist.tdshp0_clarity[4], g_aal_hist.tdshp0_clarity[6],
			g_aal_hist.tdshp0_clarity[8], g_aal_hist.tdshp0_clarity[10],
			g_aal_hist.tdshp0_clarity[11]);
	} else if (debug_dump_clarity_regs && (pipe == 1)) {
		DDPMSG("%s(aal1): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			g_aal_hist.aal1_clarity[0], g_aal_hist.aal1_clarity[1],
			g_aal_hist.aal1_clarity[2], g_aal_hist.aal1_clarity[3],
			g_aal_hist.aal1_clarity[4], g_aal_hist.aal1_clarity[5],
			g_aal_hist.aal1_clarity[6]);
		DDPMSG("%s(tdshp2): clarity readback: [%d, %d, %d, %d, %d, %d, %d]\n",
			__func__,
			g_aal_hist.tdshp1_clarity[0], g_aal_hist.tdshp1_clarity[2],
			g_aal_hist.tdshp1_clarity[4], g_aal_hist.tdshp1_clarity[6],
			g_aal_hist.tdshp1_clarity[8], g_aal_hist.tdshp1_clarity[10],
			g_aal_hist.tdshp1_clarity[11]);
	}
}

static bool disp_aal_read_single_hist(struct mtk_ddp_comp *comp)
{
	bool read_success = true;
	int i;
	struct mtk_ddp_comp *disp_tdshp;

	void __iomem *dre3_va = mtk_aal_dre3_va(comp);
	// get disp_tdshp component
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (comp->id == DDP_COMPONENT_AAL0)
		disp_tdshp = priv->ddp_comp[DDP_COMPONENT_TDSHP0];
	else
		disp_tdshp = priv->ddp_comp[DDP_COMPONENT_TDSHP2];

	if (((comp->id == DDP_COMPONENT_AAL0) && (atomic_read(&g_aal_eof_irq) == 0)) ||
		((comp->id == DDP_COMPONENT_AAL1) && (atomic_read(&g_aal1_eof_irq) == 0)))
		return false;

	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_AAL0) {
			for (i = 0; i < AAL_HIST_BIN; i++) {
				g_aal_hist.aal0_maxHist[i] = readl(comp->regs +
						DISP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				g_aal_hist.aal0_yHist[i] = readl(comp->regs +
						DISP_Y_HISTOGRAM_00 + (i << 2));
			}
			read_success = disp_color_reg_get(comp, DISP_COLOR_TWO_D_W1_RESULT,
					&g_aal_hist.aal0_colorHist);

			// for Display Clarity
			if (g_disp_clarity_support) {
				for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
					g_aal_hist.aal0_clarity[i] = readl(dre3_va +
						MDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
				}

				for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
					g_aal_hist.tdshp0_clarity[i] = readl(disp_tdshp->regs +
						MDP_TDSHP_STATUS_00 + (i << 2));
				}

				disp_aal_dump_clarity_regs(0);
			}
		} else if (comp->id == DDP_COMPONENT_AAL1) {
			for (i = 0; i < AAL_HIST_BIN; i++) {
				g_aal_hist.aal1_maxHist[i] = readl(comp->regs +
						DISP_AAL_STATUS_00 + (i << 2));
			}
			for (i = 0; i < AAL_HIST_BIN; i++) {
				g_aal_hist.aal1_yHist[i] = readl(comp->regs +
						DISP_Y_HISTOGRAM_00 + (i << 2));
			}
			read_success = disp_color_reg_get(comp, DISP_COLOR_TWO_D_W1_RESULT,
					&g_aal_hist.aal1_colorHist);

			// for Display Clarity
			if (g_disp_clarity_support) {
				for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
					g_aal_hist.aal1_clarity[i] = readl(dre3_va +
						MDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
				}

				for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
					g_aal_hist.tdshp1_clarity[i] = readl(disp_tdshp->regs +
						MDP_TDSHP_STATUS_00 + (i << 2));
				}

				disp_aal_dump_clarity_regs(1);
			}
		}
	} else {
		for (i = 0; i < AAL_HIST_BIN; i++) {
			g_aal_hist.aal0_maxHist[i] = readl(comp->regs +
					DISP_AAL_STATUS_00 + (i << 2));
		}
		for (i = 0; i < AAL_HIST_BIN; i++) {
			g_aal_hist.aal0_yHist[i] = readl(comp->regs +
					DISP_Y_HISTOGRAM_00 + (i << 2));
		}
		read_success = disp_color_reg_get(comp, DISP_COLOR_TWO_D_W1_RESULT,
				&g_aal_hist.aal0_colorHist);

		// for Display Clarity
		if (g_disp_clarity_support) {
			for (i = 0; i < MDP_AAL_CLARITY_READBACK_NUM; i++) {
				g_aal_hist.aal0_clarity[i] = readl(dre3_va +
					MDP_AAL_DRE_BILATERAL_STATUS_00 + (i << 2));
			}

			for (i = 0; i < DISP_TDSHP_CLARITY_READBACK_NUM; i++) {
				g_aal_hist.tdshp0_clarity[i] = readl(disp_tdshp->regs +
					MDP_TDSHP_STATUS_00 + (i << 2));
			}

			disp_aal_dump_clarity_regs(0);
		}
	}

	if (comp->id == DDP_COMPONENT_AAL0)
		atomic_set(&g_aal_eof_irq, 0);
	if (comp->id == DDP_COMPONENT_AAL1)
		atomic_set(&g_aal1_eof_irq, 0);

	return read_success;
}

static int disp_aal_copy_hist_to_user(struct DISP_AAL_HIST *hist)
{
	unsigned long flags;
	int ret = 0;

	if (hist == NULL) {
		AALERR("%s DstHist is NULL\n", __func__);
		return -1;
	}

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_aal_hist_lock, flags);

	if (g_aal_fo->mtk_dre30_support && gDre30Enabled)
		memcpy(&g_aal_dre30_hist_db, &g_aal_dre30_hist,
			sizeof(g_aal_dre30_hist));

	g_aal_hist.panel_type = atomic_read(&g_aal_panel_type);
	g_aal_hist.essStrengthIndex = g_aal_ess_level;
	g_aal_hist.ess_enable = g_aal_ess_en;
	g_aal_hist.dre_enable = g_aal_dre_en;

	if (isDualPQ) {
		g_aal_hist.srcWidth = g_dual_aal_size.width;
		g_aal_hist.srcHeight = g_dual_aal_size.height;
	} else {
		g_aal_hist.srcWidth = g_aal_size.width;
		g_aal_hist.srcHeight = g_aal_size.height;
	}

	memcpy(&g_aal_hist_db, &g_aal_hist, sizeof(g_aal_hist));

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	if (g_aal_fo->mtk_dre30_support && gDre30Enabled)
		g_aal_hist_db.dre30_hist = g_aal_init_dre30.dre30_hist_addr;

	memcpy(hist, &g_aal_hist_db, sizeof(g_aal_hist_db));

	if (g_aal_fo->mtk_dre30_support && gDre30Enabled)
		ret = copy_to_user(AAL_U32_PTR(g_aal_init_dre30.dre30_hist_addr),
			&g_aal_dre30_hist_db, sizeof(g_aal_dre30_hist_db));

	g_aal_hist.serviceFlags = 0;
	atomic_set(&g_aal0_hist_available, 0);
	atomic_set(&g_aal1_hist_available, 0);
	atomic_set(&g_aal0_dre20_hist_is_ready, 0);
	atomic_set(&g_aal1_dre20_hist_is_ready, 0);

	AALFLOW_LOG("%s set g_aal_force_enable_irq to 0 +\n", __func__);
	atomic_set(&g_aal_force_enable_irq, 0);

	return ret;
}

void dump_hist(struct DISP_AAL_HIST *data)
{
	int i = 0;

	pr_notice("aal0_maxHist:\n");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
			data->aal0_maxHist[i*10 + 0], data->aal0_maxHist[i*10 + 1],
			data->aal0_maxHist[i*10 + 2], data->aal0_maxHist[i*10 + 3],
			data->aal0_maxHist[i*10 + 4], data->aal0_maxHist[i*10 + 5],
			data->aal0_maxHist[i*10 + 6], data->aal0_maxHist[i*10 + 7],
			data->aal0_maxHist[i*10 + 9], data->aal0_maxHist[i*10 + 9]);
	}
	pr_notice("%d %d %d", data->aal0_maxHist[30], data->aal0_maxHist[31],
			data->aal0_maxHist[32]);

	pr_notice("aal0_yHist:\n");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
			data->aal0_yHist[i*10 + 0], data->aal0_yHist[i*10 + 1],
			data->aal0_yHist[i*10 + 2], data->aal0_yHist[i*10 + 3],
			data->aal0_yHist[i*10 + 4], data->aal0_yHist[i*10 + 5],
			data->aal0_yHist[i*10 + 6], data->aal0_yHist[i*10 + 7],
			data->aal0_yHist[i*10 + 9], data->aal0_yHist[i*10 + 9]);
	}
	pr_notice("%d %d %d", data->aal0_yHist[30], data->aal0_yHist[31],
			data->aal0_yHist[32]);
	if (isDualPQ) {
		pr_notice("aal1_maxHist:\n");
		for (i = 0; i < 3; i++) {
			pr_notice("%d %d %d %d %d %d %d %d %d %d",
				data->aal1_maxHist[i*10 + 0], data->aal1_maxHist[i*10 + 1],
				data->aal1_maxHist[i*10 + 2], data->aal1_maxHist[i*10 + 3],
				data->aal1_maxHist[i*10 + 4], data->aal1_maxHist[i*10 + 5],
				data->aal1_maxHist[i*10 + 6], data->aal1_maxHist[i*10 + 7],
				data->aal1_maxHist[i*10 + 9], data->aal1_maxHist[i*10 + 9]);
		}
		pr_notice("%d %d %d", data->aal1_maxHist[30], data->aal1_maxHist[31],
				data->aal1_maxHist[32]);
		pr_notice("aal1_yHist:\n");
		for (i = 0; i < 3; i++) {
			pr_notice("%d %d %d %d %d %d %d %d %d %d",
				data->aal1_yHist[i*10 + 0], data->aal1_yHist[i*10 + 1],
				data->aal1_yHist[i*10 + 2], data->aal1_yHist[i*10 + 3],
				data->aal1_yHist[i*10 + 4], data->aal1_yHist[i*10 + 5],
				data->aal1_yHist[i*10 + 6], data->aal1_yHist[i*10 + 7],
				data->aal1_yHist[i*10 + 9], data->aal1_yHist[i*10 + 9]);
		}
		pr_notice("%d %d %d", data->aal1_yHist[30], data->aal1_yHist[31],
			data->aal1_yHist[32]);
	}

	pr_notice("serviceFlags:%u, backlight: %d, colorHist: %d\n",
			data->serviceFlags, data->backlight, data->aal0_colorHist);
	pr_notice("requestPartial:%d, panel_type: %u\n",
			data->requestPartial, data->panel_type);
	pr_notice("essStrengthIndex:%d, ess_enable: %d, dre_enable: %d\n",
			data->essStrengthIndex, data->ess_enable,
			data->dre_enable);
}

static bool debug_dump_aal_hist;
int mtk_drm_ioctl_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	disp_aal_wait_hist();
	if (disp_aal_copy_hist_to_user((struct DISP_AAL_HIST *) data) < 0)
		return -EFAULT;
	if (debug_dump_aal_hist)
		dump_hist(data);
	return 0;
}

static void disp_aal_dre3_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	const struct DISP_AAL_INITREG *init_regs)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);
	int dre_alg_mode = 1;

	AALFLOW_LOG("start, bitShift: %d  compId%d\n", aal_data->data->bitShift, comp->id);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
		(g_aal_size.width - 1) << (aal_data->data->bitShift), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_01,
		(init_regs->dre_blk_y_num << 5) | init_regs->dre_blk_x_num,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_02,
		(init_regs->dre_blk_height << (aal_data->data->bitShift)) |
		init_regs->dre_blk_width, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_04,
		(init_regs->dre_flat_length_slope << 13) |
		init_regs->dre_flat_length_th, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_00,
		(init_regs->dre_s_upper << 24) |
		(init_regs->dre_s_lower << 16) |
		(init_regs->dre_y_upper << 8) | init_regs->dre_y_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_01,
		(init_regs->dre_h_slope << 24) |
		(init_regs->dre_s_slope << 20) |
		(init_regs->dre_y_slope << 16) |
		(init_regs->dre_h_upper << 8) | init_regs->dre_h_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_ALPHA_BLEND_00,
		(init_regs->dre_y_alpha_shift_bit << 25) |
		(init_regs->dre_y_alpha_base << 16) |
		(init_regs->dre_x_alpha_shift_bit << 9) |
		init_regs->dre_x_alpha_base, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_05,
		init_regs->dre_blk_area, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_06,
		init_regs->dre_blk_area_min, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_07,
		(g_aal_size.height - 1) << (aal_data->data->bitShift), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_SRAM_CFG,
		init_regs->hist_bin_type, 0x1);
#if defined(DRE3_IN_DISP_AAL)
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DUAL_PIPE_INFO_00,
		(0 << 13) | 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DUAL_PIPE_INFO_01,
		((init_regs->dre_blk_x_num-1) << 13) |
		(init_regs->dre_blk_width-1), ~0);
#else
	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_AAL0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_00,
				(0x0 << 23) | (0x1 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre0_blk_num_x_end << 15) |
				(init_regs->dre0_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);
		} else if (comp->id == DDP_COMPONENT_AAL1) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				dre3_pa + MDP_AAL_TILE_00,
				(0x1 << 23) | (0x0 << 22) |
				(0x1 << 21) | (0x1 << 20) |
				(init_regs->dre1_blk_num_x_end << 15) |
				(init_regs->dre1_blk_num_x_start << 10) |
				(init_regs->blk_num_y_end << 5) |
				init_regs->blk_num_y_start, ~0);
		}
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + MDP_AAL_TILE_00,
			(0x1 << 21) | (0x1 << 20) |
			(init_regs->blk_num_x_end << 15) |
			(init_regs->blk_num_x_start << 10) |
			(init_regs->blk_num_y_end << 5) |
			init_regs->blk_num_y_start, ~0);
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_01,
		(init_regs->blk_cnt_x_end << (aal_data->data->bitShift)) |
		init_regs->blk_cnt_x_start, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_02,
		(init_regs->blk_cnt_y_end << (aal_data->data->bitShift)) |
		init_regs->blk_cnt_y_start, ~0);
#endif
	/* Change to Local DRE version */
	if (debug_bypass_alg_mode)
		dre_alg_mode = 0;
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_CFG_MAIN,
		(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));

if (comp->mtk_crtc->is_dual_pipe) {
	if (comp->id == DDP_COMPONENT_AAL0)
		atomic_or(0x1, &g_aal_change_to_dre30);
} else
	atomic_or(0x1, &g_aal_change_to_dre30);
}

#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20) | ((v1) << 10) | (v0))
static struct DISP_AAL_INITREG g_aal_init_regs;
static int disp_aal_write_init_regs(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle)
{
	int ret = -EFAULT;

	if (atomic_read(&g_aal_is_init_regs_valid) == 1) {
		struct DISP_AAL_INITREG *init_regs = &g_aal_init_regs;

		int i, j = 0;
		int *gain;

		gain = init_regs->cabc_gainlmt;
		basic_cmdq_write(handle, comp, DISP_AAL_DRE_MAPPING_00,
			(init_regs->dre_map_bypass << 4), 1 << 4);

		for (i = 0; i <= 10; i++) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(i),
				CABC_GAINLMT(gain[j], gain[j + 1], gain[j + 2]),
				~0);
			j += 3;
		}

		if (g_aal_fo->mtk_dre30_support)
			disp_aal_dre3_config(comp, handle, init_regs);

		AALFLOW_LOG("init done\n");
		ret = 0;
	}

	return ret;
}

#define PRINT_INIT_REG(x1) pr_notice("[INIT]%s=0x%x\n", #x1, data->x1)
void dump_init_reg(struct DISP_AAL_INITREG *data)
{
	PRINT_INIT_REG(dre_s_lower);
	PRINT_INIT_REG(dre_s_upper);
	PRINT_INIT_REG(dre_y_lower);
	PRINT_INIT_REG(dre_y_upper);
	PRINT_INIT_REG(dre_h_lower);
	PRINT_INIT_REG(dre_h_upper);
	PRINT_INIT_REG(dre_h_slope);
	PRINT_INIT_REG(dre_s_slope);
	PRINT_INIT_REG(dre_y_slope);
	PRINT_INIT_REG(dre_x_alpha_base);
	PRINT_INIT_REG(dre_x_alpha_shift_bit);
	PRINT_INIT_REG(dre_y_alpha_base);
	PRINT_INIT_REG(dre_y_alpha_shift_bit);
	PRINT_INIT_REG(dre_blk_x_num);
	PRINT_INIT_REG(dre_blk_y_num);
	PRINT_INIT_REG(dre_blk_height);
	PRINT_INIT_REG(dre_blk_width);
	PRINT_INIT_REG(dre_blk_area);
	PRINT_INIT_REG(dre_blk_area_min);
	PRINT_INIT_REG(hist_bin_type);
	PRINT_INIT_REG(dre_flat_length_slope);
	PRINT_INIT_REG(dre_flat_length_th);
	PRINT_INIT_REG(blk_num_x_start);
	PRINT_INIT_REG(blk_num_x_end);
	PRINT_INIT_REG(dre0_blk_num_x_start);
	PRINT_INIT_REG(dre0_blk_num_x_end);
	PRINT_INIT_REG(dre1_blk_num_x_start);
	PRINT_INIT_REG(dre1_blk_num_x_end);
	PRINT_INIT_REG(blk_cnt_x_start);
	PRINT_INIT_REG(blk_cnt_x_end);
	PRINT_INIT_REG(blk_num_y_start);
	PRINT_INIT_REG(blk_num_y_end);
	PRINT_INIT_REG(blk_cnt_y_start);
	PRINT_INIT_REG(blk_cnt_y_end);
}

static bool debug_dump_init_reg = true;
static int disp_aal_set_init_reg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct DISP_AAL_INITREG *user_regs)
{
	int ret = -EFAULT;
	struct DISP_AAL_INITREG *init_regs;

	if (!disp_aal_is_support())
		return ret;

	init_regs = &g_aal_init_regs;

	memcpy(init_regs, user_regs, sizeof(*init_regs));
	if (debug_dump_init_reg)
		dump_init_reg(init_regs);

	atomic_set(&g_aal_is_init_regs_valid, 1);

	AALFLOW_LOG("Set init reg: %lu\n", sizeof(*init_regs));
	AALFLOW_LOG("init_reg.dre_map_bypass:%d\n", init_regs->dre_map_bypass);
	ret = disp_aal_write_init_regs(comp, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		struct mtk_ddp_comp *comp_aal1 = priv->ddp_comp[DDP_COMPONENT_AAL1];

		ret = disp_aal_write_init_regs(comp_aal1, handle);
	}

	AALFLOW_LOG("ret = %d\n", ret);

	return ret;
}

int mtk_drm_ioctl_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct drm_crtc *crtc = private->crtc[0];

	g_aal_data->crtc = crtc;

	return mtk_crtc_user_cmd(crtc, comp, INIT_REG, data);
}

static struct DISP_AAL_PARAM g_aal_param;
struct DISP_AAL_ESS20_SPECT_PARAM g_aal_ess20_spect_param = {0, 0, (0x01<<SET_BACKLIGHT_LEVEL)};

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | \
	((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))

static int disp_aal_write_dre3_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	unsigned long flags;

	AALFLOW_LOG("\n");
	if (atomic_read(&g_aal_change_to_dre30) == 0x3) {
		if (copy_from_user(&g_aal_gain_db,
			      AAL_U32_PTR(param->dre30_gain),
			      sizeof(g_aal_gain_db)) == 0) {

			spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
			memcpy(&g_aal_gain, &g_aal_gain_db,
				sizeof(g_aal_gain));
			spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
			atomic_set(&g_aal_dre30_write, 1);
		}
	}

	return 0;
}

static int disp_aal_write_dre_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	const int *gain;

	gain = param->DREGainFltStatus;
	if (g_aal_ess20_spect_param.flag & 0x3)
		CRTC_MMP_MARK(0, aal_ess20_curve, comp->id, 0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(0),
	    DRE_REG_2(gain[0], 0, gain[1], 14), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(1),
		DRE_REG_2(gain[2], 0, gain[3], 13), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(2),
		DRE_REG_2(gain[4], 0, gain[5], 12), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(3),
		DRE_REG_2(gain[6], 0, gain[7], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(4),
		DRE_REG_2(gain[8], 0, gain[9], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(5),
		DRE_REG_2(gain[10], 0, gain[11], 11), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(6),
		DRE_REG_3(gain[12], 0, gain[13], 11, gain[14], 22), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(7),
		DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(8),
		DRE_REG_3(gain[18], 0, gain[19], 10, gain[20], 20), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(9),
		DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(10),
		DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
	/* Write dre curve to different register */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_DRE_FLT_FORCE(11),
	    DRE_REG_2(gain[27], 0, gain[28], 9), ~0);

	return 0;
}

static int disp_aal_write_cabc_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;

	AALFLOW_LOG("\n");
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_CABC_00,
		1 << 31, 1 << 31);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_AAL_CABC_02,
		param->cabc_fltgain_force, 0x3ff);

	gain = param->cabc_gainlmt;
	for (i = 0; i <= 10; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CABC_GAINLMT_TBL(i),
			CABC_GAINLMT(gain[0], gain[1], gain[2]), ~0);
		gain += 3;
	}

	return 0;
}

static int disp_aal_write_param_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
// From mt6885, on DRE3.5+ESS mode, ESS function was
// controlled by DREGainFltStatus, not cabc_gainlmt, so need to
// set DREGainFltStatus to hw whether DRE3.5 or 2.5
	disp_aal_write_dre_to_reg(comp, handle, param);
	if (g_aal_fo->mtk_dre30_support && gDre30Enabled) {
		disp_aal_write_dre3_to_reg(comp, handle, param);
		disp_aal_write_cabc_to_reg(comp, handle, param);
	} else {
#ifndef NOT_SUPPORT_CABC_HW
		disp_aal_write_cabc_to_reg(comp, handle, param);
#endif
	}

	return 0;
}

void dump_param(const struct DISP_AAL_PARAM *param)
{
	int i = 0;

	pr_notice("DREGainFltStatus: ");
	for (i = 0; i < 2; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
		param->DREGainFltStatus[i*10 + 0],
		param->DREGainFltStatus[i*10 + 1],
		param->DREGainFltStatus[i*10 + 2],
		param->DREGainFltStatus[i*10 + 3],
		param->DREGainFltStatus[i*10 + 4],
		param->DREGainFltStatus[i*10 + 5],
		param->DREGainFltStatus[i*10 + 6],
		param->DREGainFltStatus[i*10 + 7],
		param->DREGainFltStatus[i*10 + 8],
		param->DREGainFltStatus[i*10 + 9]);
	}
	pr_notice("%d %d %d %d %d %d %d %d %d",
		param->DREGainFltStatus[20], param->DREGainFltStatus[21],
		param->DREGainFltStatus[22], param->DREGainFltStatus[23],
		param->DREGainFltStatus[24], param->DREGainFltStatus[25],
		param->DREGainFltStatus[26], param->DREGainFltStatus[27],
		param->DREGainFltStatus[28]);

	pr_notice("cabc_gainlmt: ");
	for (i = 0; i < 3; i++) {
		pr_notice("%d %d %d %d %d %d %d %d %d %d",
		param->cabc_gainlmt[i*10 + 0],
		param->cabc_gainlmt[i*10 + 1],
		param->cabc_gainlmt[i*10 + 2],
		param->cabc_gainlmt[i*10 + 3],
		param->cabc_gainlmt[i*10 + 4],
		param->cabc_gainlmt[i*10 + 5],
		param->cabc_gainlmt[i*10 + 6],
		param->cabc_gainlmt[i*10 + 7],
		param->cabc_gainlmt[i*10 + 8],
		param->cabc_gainlmt[i*10 + 9]);
	}
	pr_notice("%d %d %d",
		param->cabc_gainlmt[30], param->cabc_gainlmt[31],
		param->cabc_gainlmt[32]);

	pr_notice("cabc_fltgain_force: %d, FinalBacklight: %d",
		param->cabc_fltgain_force, param->FinalBacklight);
	pr_notice("allowPartial: %d, refreshLatency: %d",
		param->allowPartial, param->refreshLatency);
}

static bool debug_dump_input_param;
int disp_aal_set_param(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		struct DISP_AAL_PARAM *param)
{
	int ret = -EFAULT;
	u64 time_use = 0;

	if (debug_dump_input_param)
		dump_param(&g_aal_param);
	//For 120Hz rotation issue
	ktime_get_ts64(&end);
	time_use = (end.tv_sec-start.tv_sec) * 1000000
		+ (end.tv_nsec-start.tv_nsec) / NSEC_PER_USEC;
	//pr_notice("set_param time_use is %lu us\n",time_use);
	// tbd. to be fixd
	if (time_use < 260) {
		// Workaround for 120hz rotation,do not let
		//aal command too fast,else it will merged with
		//DISP commmand and caused trigger loop clear EOF
		//before config loop.The DSI EOF has 100 us later then
		//RDMA EOF,and the worst DISP config time is 153us,
		//so if intervel less than 260 should delay
		usleep_range(260-time_use, 270-time_use);
	}

	ret = disp_aal_write_param_to_reg(comp, handle, &g_aal_param);
	//disp_aal_flip_sram(comp, handle, __func__);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		struct mtk_ddp_comp *comp_aal1 = priv->ddp_comp[DDP_COMPONENT_AAL1];

		ret = disp_aal_write_param_to_reg(comp_aal1, handle, &g_aal_param);
		//disp_aal_flip_sram(comp_aal1, handle, __func__);

	}
/* FIXME
 *	if (ret == 0)
 *		ret |= disp_pwm_set_backlight_cmdq(DISP_PWM0,
 *			backlight_value, cmdq);
 */
// FIXME

	return ret;
}

#define PRINT_AAL_REG(x1, x2, x3, x4) \
	pr_notice("[2]0x%x=0x%x 0x%x=0x%x 0x%x=0x%x 0x%x=0x%x\n", \
		x1, readl(comp->regs + x1), x2, readl(comp->regs + x2), \
		x3, readl(comp->regs + x3), x4, readl(comp->regs + x4))

#define PRINT_AAL3_REG(x1, x2, x3, x4) \
	pr_notice("[3]0x%x=0x%x 0x%x=0x%x 0x%x=0x%x 0x%x=0x%x\n", \
		x1, readl(dre3_va + x1), x2, readl(dre3_va + x2), \
		x3, readl(dre3_va + x3), x4, readl(dre3_va + x4))
bool dump_reg(struct mtk_ddp_comp *comp, bool locked)
{
	unsigned long flags = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool dump_success = false;

	if (locked || spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
		if (atomic_read(&aal_data->is_clock_on)) {
			PRINT_AAL_REG(0x0, 0x8, 0x10, 0x20);
			PRINT_AAL_REG(0x30, 0xFC, 0x160, 0x200);
			PRINT_AAL_REG(0x204, 0x20C, 0x3B4, 0x45C);
			PRINT_AAL_REG(0x460, 0x464, 0x468, 0x4D8);
			PRINT_AAL_REG(0x4DC, 0x500, 0x224, 0x504);
			if (g_aal_fo->mtk_dre30_support) {
				void __iomem *dre3_va = mtk_aal_dre3_va(comp);

				PRINT_AAL3_REG(0x0, 0x8, 0x10, 0x20);
				PRINT_AAL3_REG(0x30, 0x34, 0x38, 0xC4);
				PRINT_AAL3_REG(0xC8, 0xF4, 0xF8, 0x200);
				PRINT_AAL3_REG(0x204, 0x45C, 0x460, 0x464);
				PRINT_AAL3_REG(0x468, 0x46C, 0x470, 0x474);
				PRINT_AAL3_REG(0x478, 0x480, 0x484, 0x488);
				PRINT_AAL3_REG(0x48C, 0x490, 0x494, 0x498);
				PRINT_AAL3_REG(0x49C, 0x4B4, 0x4B8, 0x4BC);
				PRINT_AAL3_REG(0x4D4, 0x4EC, 0x4F0, 0x53C);
			}
			dump_success = true;
		} else
			AALIRQ_LOG("clock is not enabled\n");
		if (!locked)
			spin_unlock_irqrestore(&g_aal_clock_lock, flags);
	} else
		AALIRQ_LOG("clock lock is locked\n");
	return dump_success;
}

int mtk_drm_ioctl_aal_set_ess20_spect_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct DISP_AAL_ESS20_SPECT_PARAM *param = (struct DISP_AAL_ESS20_SPECT_PARAM *) data;

	memcpy(&g_aal_ess20_spect_param, param, sizeof(*param));
	AALAPI_LOG("[aal_kernel]ELVSSPN = %d, flag = %d",
		g_aal_ess20_spect_param.ELVSSPN, g_aal_ess20_spect_param.flag);

	return ret;
}

static bool debug_skip_set_param;
int mtk_drm_ioctl_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct drm_crtc *crtc = private->crtc[0];
	int backlight_value = 0;
	struct DISP_AAL_PARAM *param = (struct DISP_AAL_PARAM *) data;
	bool delay_refresh = false;

	if (debug_skip_set_param) {
		pr_notice("skip_set_param for debug\n");
		return ret;
	}
	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	memcpy(&g_aal_param, param, sizeof(*param));
	backlight_value = g_aal_param.FinalBacklight;
	/* set cabc gain zero when detect backlight */
	/* setting equal to zero */
	if (backlight_value == 0)
		g_aal_param.cabc_fltgain_force = 0;

	mutex_lock(&g_aal_sram_lock);
	ret = mtk_crtc_user_cmd(crtc, comp, SET_PARAM, data);
	mutex_unlock(&g_aal_sram_lock);

	atomic_set(&g_aal_allowPartial, g_aal_param.allowPartial);

	if (atomic_read(&g_aal_backlight_notified) == 0)
		backlight_value = 0;

	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (g_aal_param.silky_bright_flag == 0) {
			AALAPI_LOG("backlight_value = %d, silky_bright_flag = %d",
				backlight_value, g_aal_param.silky_bright_flag);
			mtk_leds_brightness_set("lcd-backlight", backlight_value,
					g_aal_ess20_spect_param.ELVSSPN,
					g_aal_ess20_spect_param.flag);
		}
	} else if (m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS]) {
		//if (pre_bl != cur_bl)
		AALAPI_LOG("gian = %d, backlight = %d",
			g_aal_param.silky_bright_gain[0], backlight_value);
		mtk_trans_gain_to_gamma(crtc, &g_aal_param.silky_bright_gain[0],
			backlight_value, (void *)&g_aal_ess20_spect_param);
	} else {
		AALAPI_LOG("%d", backlight_value);
		mtk_leds_brightness_set("lcd-backlight", backlight_value,
					g_aal_ess20_spect_param.ELVSSPN,
					g_aal_ess20_spect_param.flag);
	}
	g_aal_ess20_spect_param.ELVSSPN = 0;
	g_aal_ess20_spect_param.flag = (0x01<<SET_BACKLIGHT_LEVEL);
	AALFLOW_LOG("delay refresh: %d", g_aal_param.refreshLatency);
	if (g_aal_param.refreshLatency == 33)
		delay_refresh = true;
	mtk_crtc_check_trigger(comp->mtk_crtc, delay_refresh, true);
	return ret;
}

static DEFINE_SPINLOCK(g_aal_get_irq_lock);

static void disp_aal_clear_irq(struct mtk_ddp_comp *comp,
		bool cleared)
{
	unsigned int intsta;
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	/* Check current irq status */
	do {
		intsta = readl(comp->regs + DISP_AAL_INTSTA);

		if (spin_trylock_irqsave(&g_aal_get_irq_lock, flags)) {
			writel(intsta & ~0x3, comp->regs + DISP_AAL_INTSTA);
			spin_unlock_irqrestore(&g_aal_get_irq_lock, flags);
		}
	} while (0);

	atomic_set(&aal_data->dirty_frame_retrieved, 1);
	/*
	 * no need per-frame wakeup.
	 * We stop interrupt until next frame dirty.
	 */
	if (cleared == true) {
		if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
			if (atomic_read(&aal_data->is_clock_on) != 1)
				AALIRQ_LOG("clock is off\n");
			else
				disp_aal_set_interrupt(comp, false);
			spin_unlock_irqrestore(&g_aal_clock_lock,
					flags);
		}
	}

	AALIRQ_LOG("AAL Module, process:(%d)  compID:%d\n", cleared, comp->id);
}

static bool debug_skip_dre3_irq;
static bool debug_dump_reg_irq;
static int dump_blk_x = -1;
static int dump_blk_y = -1;

#define AAL_DRE_BLK_NUM			(16)
#define AAL_BLK_MAX_ALLOWED_NUM		(128)
#define AAL_DRE3_POINT_NUM		(17)
#define AAL_DRE_GAIN_POINT16_START	(512)

#define DRE_POLL_SLEEP_TIME_US	(10)
#define DRE_MAX_POLL_TIME_US	(1000)

static inline bool disp_aal_reg_poll(struct mtk_ddp_comp *comp,
	unsigned long addr, unsigned int value, unsigned int mask)
{
	bool return_value = false;
	unsigned int reg_value = 0;
	unsigned int polling_time = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	do {
		reg_value = readl(dre3_va + addr);

		if ((reg_value & mask) == value) {
			return_value = true;
			break;
		}

		udelay(DRE_POLL_SLEEP_TIME_US);
		polling_time += DRE_POLL_SLEEP_TIME_US;
	} while (polling_time < DRE_MAX_POLL_TIME_US);

	return return_value;
}

static bool disp_aal_read_dre3(struct mtk_ddp_comp *comp,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int hist_offset;
	int arry_offset = 0;
	unsigned int read_value;
	int dump_start = -1;
	u32 dump_table[6] = {0};

	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Read Global histogram for ESS */
	if (disp_aal_read_single_hist(comp) != true)
		return false;

	AALIRQ_LOG("start\n");
	if (dump_blk_x >= 0 && dump_blk_x < 16
		&& dump_blk_y >= 0 && dump_blk_y < 8)
		dump_start = 6 * (dump_blk_x + dump_blk_y * 16);

	/* Read Local histogram for DRE 3 */
	for (hist_offset = aal_data->data->aal_dre_hist_start;
		hist_offset <= aal_data->data->aal_dre_hist_end;
			hist_offset += 4) {

		writel(hist_offset, dre3_va + DISP_AAL_SRAM_RW_IF_2);
		read_value = readl(dre3_va + DISP_AAL_SRAM_RW_IF_3);

		if (arry_offset >= AAL_DRE30_HIST_REGISTER_NUM)
			return false;
		if (dump_start >= 0 && arry_offset >= dump_start
			&& arry_offset < (dump_start + 6))
			dump_table[arry_offset-dump_start] = read_value;
		if (comp->mtk_crtc->is_dual_pipe) {
			if (comp->id == DDP_COMPONENT_AAL0)
				g_aal_dre30_hist.aal0_dre_hist[arry_offset++] = read_value;
			else if (comp->id == DDP_COMPONENT_AAL1)
				g_aal_dre30_hist.aal1_dre_hist[arry_offset++] = read_value;
		} else {
			g_aal_dre30_hist.aal0_dre_hist[arry_offset++] = read_value;
		}
	}
	if (comp->mtk_crtc->is_dual_pipe) {
		int i = 0, j = 0;

		if (comp->id == DDP_COMPONENT_AAL0) {
			for (i = 0; i < 8; i++) {
				g_aal_hist.MaxHis_denominator_pipe0[i] = readl(dre3_va +
					MDP_AAL_DUAL_PIPE00 + (i << 2));
			}
			for (j = 0; j < 8; j++) {
				g_aal_hist.MaxHis_denominator_pipe0[j+i] = readl(dre3_va +
					MDP_AAL_DUAL_PIPE08 + (j << 2));
			}
		} else if (comp->id == DDP_COMPONENT_AAL1) {
			for (i = 0; i < 8; i++) {
				g_aal_hist.MaxHis_denominator_pipe1[i] = readl(dre3_va +
					MDP_AAL_DUAL_PIPE00 + (i << 2));
			}
			for (j = 0; j < 8; j++) {
				g_aal_hist.MaxHis_denominator_pipe1[j+i] = readl(dre3_va +
					MDP_AAL_DUAL_PIPE08 + (j << 2));
			}
		}
	}

	if (dump_start >= 0)
		pr_notice("[DRE3][HIST][%d-%d] %08x %08x %08x %08x %08x %08x\n",
			dump_blk_x, dump_blk_y,
			dump_table[0], dump_table[1], dump_table[2],
			dump_table[3], dump_table[4], dump_table[5]);

	return true;
}
static bool disp_aal_write_dre3(struct mtk_ddp_comp *comp)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;

	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Write Local Gain Curve for DRE 3 */
	AALIRQ_LOG("start\n");
	writel(aal_data->data->aal_dre_gain_start,
		dre3_va + DISP_AAL_SRAM_RW_IF_0);
	if (disp_aal_reg_poll(comp, DISP_AAL_SRAM_STATUS,
		(0x1 << 16), (0x1 << 16)) != true) {
		AALERR("DISP_AAL_SRAM_STATUS ERROR\n");
		return false;
	}
	for (gain_offset = aal_data->data->aal_dre_gain_start;
		gain_offset <= aal_data->data->aal_dre_gain_end;
			gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = g_aal_gain.dre30_gain[arry_offset++];
		writel(gain_offset, dre3_va + DISP_AAL_SRAM_RW_IF_0);
		writel(write_value, dre3_va + DISP_AAL_SRAM_RW_IF_1);
	}
	return true;
}

static void disp_aal_update_dre3_sram(struct mtk_ddp_comp *comp,
	 bool check_sram)
{
	bool result = false;
	unsigned long flags;
	int dre_blk_x_num, dre_blk_y_num;
	unsigned int read_value;
	int hist_apb = 0, hist_int = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	AALIRQ_LOG("[g_aal_first_frame = %d", atomic_read(&g_aal_first_frame));
	AALIRQ_LOG("[g_aal0_hist_available = %d",
			atomic_read(&g_aal0_hist_available));
	AALIRQ_LOG("[g_aal_eof_irq = %d", atomic_read(&g_aal_eof_irq));
	AALIRQ_LOG("[g_aal1_eof_irq = %d", atomic_read(&g_aal1_eof_irq));

	// reset g_aal_eof_irq to 0 when first frame,
	// to avoid timing issue
	if ((comp->id == DDP_COMPONENT_AAL0) &&
		(atomic_read(&g_aal_first_frame) == 1))
		atomic_set(&g_aal_eof_irq, 0);

	if ((comp->id == DDP_COMPONENT_AAL1) &&
		(atomic_read(&g_aal1_first_frame) == 1))
		atomic_set(&g_aal1_eof_irq, 0);

	CRTC_MMP_EVENT_START(0, aal_dre30_rw, comp->id, 0);
	if (check_sram) {
		read_value = readl(dre3_va + DISP_AAL_SRAM_CFG);
		hist_apb = (read_value >> 5) & 0x1;
		hist_int = (read_value >> 6) & 0x1;
		AALIRQ_LOG("[SRAM] hist_apb(%d) hist_int(%d) 0x%08x in (SOF) compID:%d",
			hist_apb, hist_int, read_value, comp->id);
		if (comp->mtk_crtc->is_dual_pipe) {
			if (comp->id == DDP_COMPONENT_AAL1) {
				if (hist_int != atomic_read(&g_aal1_force_hist_apb))
					AALIRQ_LOG("dre3 aal1: SRAM config %d != %d config?",
						hist_int, atomic_read(&g_aal1_force_hist_apb));
			} else if (comp->id == DDP_COMPONENT_AAL0) {
				if (hist_int != atomic_read(&g_aal_force_hist_apb))
					AALIRQ_LOG("dre3 aal0: SRAM config %d != %d config?",
						hist_int, atomic_read(&g_aal_force_hist_apb));
			}
		} else if (hist_int != atomic_read(&g_aal_force_hist_apb))
			AALIRQ_LOG("dre3: SRAM config %d != %d config?",
				hist_int, atomic_read(&g_aal_force_hist_apb));
	}
	read_value = readl(dre3_va + DISP_AAL_DRE_BLOCK_INFO_01);
	dre_blk_x_num = aal_min(AAL_DRE_BLK_NUM, read_value & 0x1F);
	dre_blk_y_num =	aal_min(AAL_BLK_MAX_ALLOWED_NUM/dre_blk_x_num,
		    (read_value >> 5) & 0x1F);
	if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
		result = disp_aal_read_dre3(comp,
			dre_blk_x_num, dre_blk_y_num);
		if (result) {
			g_aal_dre30_hist.dre_blk_x_num = dre_blk_x_num;
			g_aal_dre30_hist.dre_blk_y_num = dre_blk_y_num;
			if (comp->mtk_crtc->is_dual_pipe) {
				if (comp->id == DDP_COMPONENT_AAL0)
					atomic_set(&g_aal0_hist_available, 1);
				else if (comp->id == DDP_COMPONENT_AAL1)
					atomic_set(&g_aal1_hist_available, 1);
			} else
				atomic_set(&g_aal0_hist_available, 1);
		}
		spin_unlock_irqrestore(&g_aal_hist_lock, flags);
		if (result) {
			AALIRQ_LOG("wake_up_interruptible");
			wake_up_interruptible(&g_aal_hist_wq);
		} else {
			AALIRQ_LOG("result fail");
		}
	}

	CRTC_MMP_MARK(0, aal_dre30_rw, comp->id, 1);

	if (spin_trylock_irqsave(&g_aal_dre3_gain_lock, flags)) {
		/* Write DRE 3.0 gain */
		if (comp->mtk_crtc->is_dual_pipe) {
			if (!atomic_read(&g_aal_first_frame) &&
				!atomic_read(&g_aal1_first_frame))
				disp_aal_write_dre3(comp);
		} else {
			if (!atomic_read(&g_aal_first_frame))
				disp_aal_write_dre3(comp);
		}

		spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
	}
	CRTC_MMP_EVENT_END(0, aal_dre30_rw, comp->id, 2);
}

static void disp_aal_dre3_irq_handle(struct mtk_ddp_comp *comp)
{
	int hist_apb = 0, hist_int = 0;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Only process AAL0 in single module state */
	disp_aal_clear_irq(comp, false);

	if (atomic_read(&g_aal_change_to_dre30) != 0x3)
		return;

	if (debug_dump_reg_irq)
		debug_dump_reg_irq = !dump_reg(comp, true);

	if (debug_skip_dre3_irq) {
		pr_notice("skip dre3 irq for debug\n");
		return;
	}

	if (aal_sram_method == AAL_SRAM_EOF &&
		atomic_read(&g_aal_dre_halt) == 0) {
		if (comp->mtk_crtc->is_dual_pipe && comp->id == DDP_COMPONENT_AAL1) {
			if (atomic_cmpxchg(&g_aal1_force_hist_apb, 0, 1) == 0) {
				hist_apb = 0;
				hist_int = 1;
			} else if (atomic_cmpxchg(&g_aal1_force_hist_apb, 1, 0) == 1) {
				hist_apb = 1;
				hist_int = 0;
			} else {
				AALERR("Error when get hist_apb irq_handler\n");
				return;
			}
		} else {
			if (atomic_cmpxchg(&g_aal_force_hist_apb, 0, 1) == 0) {
				hist_apb = 0;
				hist_int = 1;
			} else if (atomic_cmpxchg(&g_aal_force_hist_apb, 1, 0) == 1) {
				hist_apb = 1;
				hist_int = 0;
			} else {
				AALERR("Error when get hist_apb irq_handler\n");
				return;
			}
		}
		AALIRQ_LOG("[SRAM] %s hist_apb (%d) hist_int (%d) in(EOF) comp:%d",
			__func__, hist_apb, hist_int, comp->id);

		mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG,
			(hist_int << 6)|(hist_apb << 5)|(1 << 4), (0x7 << 4));
		atomic_set(&g_aal_dre_halt, 1);
		disp_aal_update_dre3_sram(comp, false);
		atomic_set(&g_aal_dre_halt, 0);
	} else if (aal_sram_method == AAL_SRAM_SOF) {
		if (mtk_drm_is_idle(&(comp->mtk_crtc->base))) {
			AALIRQ_LOG("[SRAM] when idle, operate SRAM in (EOF) comp id:%d ", comp->id);
			disp_aal_update_dre3_sram(comp, false);
		}
	}
}

static void disp_aal_set_init_dre30(struct DISP_DRE30_INIT *user_regs)
{
	struct DISP_DRE30_INIT *init_dre3;

	init_dre3 = &g_aal_init_dre30;

	memcpy(init_dre3, user_regs, sizeof(*init_dre3));
	/* Modify DRE3.0 config flag */
	atomic_or(0x2, &g_aal_change_to_dre30);
}

static void ddp_aal_dre3_write_curve_full(struct mtk_ddp_comp *comp)
{
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG,
		(1 << 6)|(0 << 5)|(1 << 4), (0x7 << 4));
	disp_aal_write_dre3(comp);
	mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG,
		(0 << 6)|(1 << 5)|(1 << 4), (0x7 << 4));
	disp_aal_write_dre3(comp);
	if (comp->mtk_crtc->is_dual_pipe && comp->id == DDP_COMPONENT_AAL1)
		atomic_set(&g_aal1_force_hist_apb, 0);
	else
		atomic_set(&g_aal_force_hist_apb, 0);
}

static bool write_block(const unsigned int *dre3_gain,
	const int block_x, const int block_y, const int dre_blk_x_num)
{
	bool return_value = false;
	uint32_t block_offset = 4 * (block_y * dre_blk_x_num + block_x);

	do {
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[0] & 0xff) |
			((dre3_gain[1] & 0xff) << 8) |
			((dre3_gain[2] & 0xff) << 16) |
			((dre3_gain[3] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[4] & 0xff) |
			((dre3_gain[5] & 0xff) << 8) |
			((dre3_gain[6] & 0xff) << 16) |
			((dre3_gain[7] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[8] & 0xff) |
			((dre3_gain[9] & 0xff) << 8) |
			((dre3_gain[10] & 0xff) << 16) |
			((dre3_gain[11] & 0xff) << 24));

		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			break;
		g_aal_gain.dre30_gain[block_offset++] =
			((dre3_gain[12] & 0xff) |
			((dre3_gain[13] & 0xff) << 8) |
			((dre3_gain[14] & 0xff) << 16) |
			((dre3_gain[15] & 0xff) << 24));

		return_value = true;
	} while (0);

	return return_value;
}

static bool write_curve16(const unsigned int *dre3_gain,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int32_t blk_x, blk_y;
	const int32_t blk_num_max = dre_blk_x_num * dre_blk_y_num;
	unsigned int write_value = 0x0;
	uint32_t bit_shift = 0;
	uint32_t block_offset = AAL_DRE_GAIN_POINT16_START;

	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			write_value |=
				((dre3_gain[16] & 0xff) << (8*bit_shift));
			bit_shift++;

			if (bit_shift >= 4) {
				if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
					return false;
				g_aal_gain.dre30_gain[block_offset++] =
					write_value;

				write_value = 0x0;
				bit_shift = 0;
			}
		}
	}

	if ((blk_num_max>>2)<<2 != blk_num_max) {
		/* configure last curve */
		if (block_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		g_aal_gain.dre30_gain[block_offset] = write_value;
	}

	return true;
}

static void disp_aal_dre3_init(struct mtk_ddp_comp *comp)
{
	const int dre_blk_x_num = 8;
	const int dre_blk_y_num = 16;
	unsigned long flags;
	int blk_x, blk_y, curve_point;
	unsigned int dre3_gain[AAL_DRE3_POINT_NUM];

	AALFLOW_LOG("start\n");
	for (curve_point = 0; curve_point < AAL_DRE3_POINT_NUM;
		curve_point++) {
		/* assign initial gain curve */
		dre3_gain[curve_point] = aal_min(255, 16 * curve_point);
	}

	spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
	for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
		for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
			/* write each block dre curve */
			write_block(dre3_gain, blk_x, blk_y, dre_blk_x_num);
		}
	}
	/* write each block dre curve last point */
	write_curve16(dre3_gain, dre_blk_x_num, dre_blk_y_num);

	ddp_aal_dre3_write_curve_full(comp);
	spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
}

static void disp_aal_single_pipe_hist_update(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool read_success = false;

	do {
		CRTC_MMP_EVENT_START(0, aal_dre20_rh, comp->id, 0);
		intsta = readl(comp->regs + DISP_AAL_INTSTA);
		/* Only process end of frame state */
		if ((intsta & 0x2) == 0x0) {
			AALERR("break\n");
			break;
		}

		if (spin_trylock_irqsave(&g_aal_get_irq_lock, flags)) {
			writel(intsta & ~0x3, comp->regs + DISP_AAL_INTSTA);
			spin_unlock_irqrestore(&g_aal_get_irq_lock, flags);
		}

		/* Allow to disable interrupt */
		atomic_set(&aal_data->dirty_frame_retrieved, 1);

		CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 1);
		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			read_success = disp_aal_read_single_hist(comp);

			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			if (read_success == true) {
				if (comp->id == DDP_COMPONENT_AAL0) {
					atomic_set(&g_aal0_dre20_hist_is_ready, 1);
					AALIRQ_LOG("DDP_COMPONENT_AAL0 read_success = %d\n",
						read_success);

					if (comp->mtk_crtc->is_dual_pipe) {
						if ((atomic_read(&g_aal_first_frame) == 1) &&
							(atomic_read(&g_aal1_first_frame) == 1)) {
							atomic_set(&g_aal_first_frame, 0);
							atomic_set(&g_aal1_first_frame, 0);
							queue_work(aal_refresh_wq,
								&g_aal_data->aal_refresh_task);
						}
					} else {
						if (atomic_read(&g_aal_first_frame) == 1) {
							atomic_set(&g_aal_first_frame, 0);
							queue_work(aal_refresh_wq,
								&g_aal_data->aal_refresh_task);
						}
					}
				}
				if (comp->id == DDP_COMPONENT_AAL1) {
					atomic_set(&g_aal1_dre20_hist_is_ready, 1);
					AALIRQ_LOG("DDP_COMPONENT_AAL1 read_success = %d\n",
						read_success);
				}
			}
		} else {
			/*
			 * Histogram was not be retrieved, but it's OK.
			 * Another interrupt will come until histogram available
			 * See: disp_aal_set_interrupt()
			 */
		}
		CRTC_MMP_MARK(0, aal_dre20_rh, comp->id, 2);
		if (atomic_read(&g_aal_is_init_regs_valid) == 0) {
			/*
			 * AAL service is not running, not need per-frame wakeup
			 * We stop interrupt until next frame dirty.
			 */
			if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
				if (atomic_read(&aal_data->is_clock_on) != 1)
					AALIRQ_LOG("clock is off\n");
				else
					disp_aal_set_interrupt(comp, false);
				spin_unlock_irqrestore(&g_aal_clock_lock,
					flags);
			}
		}
		CRTC_MMP_EVENT_END(0, aal_dre20_rh, comp->id, 3);
	} while (0);
}

int mtk_drm_ioctl_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	if (g_aal_fo->mtk_dre30_support) {
		AALFLOW_LOG("\n");
		disp_aal_set_init_dre30((struct DISP_DRE30_INIT *) data);
	} else {
		AALFLOW_LOG("DRE30 not support\n");
	}

	return 0;
}


static int disp_aal_wait_size(unsigned long timeout)
{
	int ret = 0;

	if (g_aal_get_size_available == false) {
		ret = wait_event_interruptible(g_aal_size_wq,
		g_aal_get_size_available == true);
		pr_notice("size_available = 1, Waken up, ret = %d\n",
			ret);
	} else {
		/* If g_aal_get_size_available is already set, */
		/* means AALService was delayed */
		pr_notice("size_available = 0\n");
	}
	return ret;
}


int mtk_drm_ioctl_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct DISP_AAL_DISPLAY_SIZE *dst =
		(struct DISP_AAL_DISPLAY_SIZE *)data;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];

	AALFLOW_LOG("\n");
	disp_aal_wait_size(60);

	if (comp == NULL || comp->mtk_crtc == NULL) {
		AALERR("%s null pointer!\n", __func__);
		return -1;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		memcpy(dst, &g_dual_aal_size, sizeof(g_dual_aal_size));
	else
		memcpy(dst, &g_aal_size, sizeof(g_aal_size));

	return 0;
}

int mtk_drm_ioctl_clarity_set_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct drm_crtc *crtc = private->crtc[0];

	AALCRT_LOG(": enter set clarity regs\n");

	return mtk_crtc_user_cmd(crtc, comp, SET_CLARITY_REG, data);
}

static void dumpAlgAALClarityRegOutput(struct DISP_CLARITY_REG clarityHWReg)
{
	AALCRT_LOG("bilateral_range_flt_slope[%d] dre_bilateral_activate_blending_A[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_slope,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_A);

	AALCRT_LOG("dre_bilateral_activate_blending_B[%d] dre_bilateral_activate_blending_C[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_B,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_C);

	AALCRT_LOG("dre_bilateral_activate_blending_D[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_D);
	AALCRT_LOG("dre_bilateral_activate_blending_wgt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_activate_blending_wgt_gain);

	AALCRT_LOG("dre_bilateral_blending_en[%d] dre_bilateral_blending_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_en,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_wgt);

	AALCRT_LOG("dre_bilateral_blending_wgt_mode[%d] dre_bilateral_size_blending_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_wgt_mode,
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_size_blending_wgt);

	AALCRT_LOG("bilateral_custom_range_flt1 [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_4);

	AALCRT_LOG("bilateral_custom_range_flt1 [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_4);

	AALCRT_LOG("bilateral_custom_range_flt1 [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_4);

	AALCRT_LOG("bilateral_custom_range_flt2 [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_0,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_1,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_2,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_3,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_4);

	AALCRT_LOG("bilateral_contrary_blending_wgt[%d] bilateral_custom_range_flt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_contrary_blending_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt_gain);

	AALCRT_LOG("bilateral_custom_range_flt_slope[%d] bilateral_range_flt_gain[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt_slope,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_gain);

	AALCRT_LOG("bilateral_size_blending_wgt[%d] bilateral_contrary_blending_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_size_blending_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_contrary_blending_out_wgt);

	AALCRT_LOG("bilateral_custom_range_flt1_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt1_out_wgt);
	AALCRT_LOG("bilateral_custom_range_flt2_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_custom_range_flt2_out_wgt);

	AALCRT_LOG("bilateral_range_flt_out_wgt[%d] bilateral_size_blending_out_wgt[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.bilateral_range_flt_out_wgt,
		clarityHWReg.mdp_aal_clarity_regs.bilateral_size_blending_out_wgt);

	AALCRT_LOG("dre_bilateral_blending_region_protection_en[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_blending_region_protection_en);
	AALCRT_LOG("dre_bilateral_region_protection_activate_A[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_A);

	AALCRT_LOG("dre_bilateral_region_protection_activate_B[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_B);
	AALCRT_LOG("dre_bilateral_region_protection_activate_C[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_C);

	AALCRT_LOG("dre_bilateral_region_protection_activate_D[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_D);
	AALCRT_LOG("dre_bilateral_region_protection_input_shift_bit[%d]\n",
		clarityHWReg.mdp_aal_clarity_regs.dre_bilateral_region_protection_input_shift_bit);
}

static void dumpAlgTDSHPRegOutput(struct DISP_CLARITY_REG clarityHWReg)
{
	AALCRT_LOG("tdshp_gain_high[%d] tdshp_gain_mid[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.tdshp_gain_high,
		clarityHWReg.disp_tdshp_clarity_regs.tdshp_gain_mid);

	AALCRT_LOG("mid_coef_v_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_4);

	AALCRT_LOG("mid_coef_v_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_4);

	AALCRT_LOG("mid_coef_v_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_4);

	AALCRT_LOG("mid_coef_h_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_v_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_h_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_rd_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [0_0 ~ 0_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [1_0 ~ 1_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_4);

	AALCRT_LOG("high_coef_ld_custom_range_flt [2_0 ~ 2_4] = [%d, %d, %d, %d, %d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_0,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_1,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_2,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_3,
		clarityHWReg.disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_4);

	AALCRT_LOG("mid_negative_offset[%d] mid_positive_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.mid_negative_offset,
		clarityHWReg.disp_tdshp_clarity_regs.mid_positive_offset);
	AALCRT_LOG("high_negative_offset[%d] high_positive_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_negative_offset,
		clarityHWReg.disp_tdshp_clarity_regs.high_positive_offset);

	AALCRT_LOG("D_active_parameter_N_gain[%d] D_active_parameter_N_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_N_offset);
	AALCRT_LOG("D_active_parameter_P_gain[%d] D_active_parameter_P_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.D_active_parameter_P_offset);

	AALCRT_LOG("High_active_parameter_N_gain[%d] High_active_parameter_N_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_N_offset);
	AALCRT_LOG("High_active_parameter_P_gain[%d] High_active_parameter_P_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.High_active_parameter_P_offset);

	AALCRT_LOG("L_active_parameter_N_gain[%d] L_active_parameter_N_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_N_offset);
	AALCRT_LOG("L_active_parameter_P_gain[%d] L_active_parameter_P_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.L_active_parameter_P_offset);

	AALCRT_LOG("Mid_active_parameter_N_gain[%d] Mid_active_parameter_N_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_N_gain,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_N_offset);
	AALCRT_LOG("Mid_active_parameter_P_gain[%d] Mid_active_parameter_P_offset[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_P_gain,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_active_parameter_P_offset);

	AALCRT_LOG("SIZE_PARA_BIG_HUGE[%d] SIZE_PARA_MEDIUM_BIG[%d] SIZE_PARA_SMALL_MEDIUM[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_BIG_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_MEDIUM_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARA_SMALL_MEDIUM);

	AALCRT_LOG("high_auto_adaptive_weight_HUGE[%d] high_size_adaptive_weight_HUGE[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_HUGE);
	AALCRT_LOG("Mid_auto_adaptive_weight_HUGE[%d] Mid_size_adaptive_weight_HUGE[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_HUGE,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_HUGE);

	AALCRT_LOG("high_auto_adaptive_weight_BIG[%d] high_size_adaptive_weight_BIG[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_BIG);
	AALCRT_LOG("Mid_auto_adaptive_weight_BIG[%d] Mid_size_adaptive_weight_BIG[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_BIG,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_BIG);

	AALCRT_LOG("high_auto_adaptive_weight_MEDIUM[%d]high_size_adaptive_weight_MEDIUM[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_MEDIUM,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_MEDIUM);
	AALCRT_LOG("Mid_auto_adaptive_weight_MEDIUM[%d] Mid_size_adaptive_weight_MEDIUM[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_MEDIUM,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_MEDIUM);

	AALCRT_LOG("high_auto_adaptive_weight_SMALL[%d] high_size_adaptive_weight_SMALL[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.high_auto_adaptive_weight_SMALL,
		clarityHWReg.disp_tdshp_clarity_regs.high_size_adaptive_weight_SMALL);
	AALCRT_LOG("Mid_auto_adaptive_weight_SMALL[%d] Mid_size_adaptive_weight_SMALL[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_SMALL,
		clarityHWReg.disp_tdshp_clarity_regs.Mid_size_adaptive_weight_SMALL);

	AALCRT_LOG("FILTER_HIST_EN[%d] FREQ_EXTRACT_ENHANCE[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.FILTER_HIST_EN,
		clarityHWReg.disp_tdshp_clarity_regs.FREQ_EXTRACT_ENHANCE);
	AALCRT_LOG("freq_D_weighting[%d] freq_H_weighting[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.freq_D_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_H_weighting);

	AALCRT_LOG("freq_L_weighting[%d] freq_M_weighting[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.freq_L_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_M_weighting);
	AALCRT_LOG("freq_D_final_weighting[%d] freq_L_final_weighting[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.freq_D_final_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_L_final_weighting);

	AALCRT_LOG("freq_M_final_weighting[%d] freq_WH_final_weighting[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.freq_M_final_weighting,
		clarityHWReg.disp_tdshp_clarity_regs.freq_WH_final_weighting);
	AALCRT_LOG("SIZE_PARAMETER[%d] chroma_high_gain[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.SIZE_PARAMETER,
		clarityHWReg.disp_tdshp_clarity_regs.chroma_high_gain);

	AALCRT_LOG("chroma_high_index[%d] chroma_low_gain[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.chroma_high_index,
		clarityHWReg.disp_tdshp_clarity_regs.chroma_low_gain);
	AALCRT_LOG("chroma_low_index[%d] luma_high_gain[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.chroma_low_index,
		clarityHWReg.disp_tdshp_clarity_regs.luma_high_gain);

	AALCRT_LOG("luma_high_index[%d] luma_low_gain[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.luma_high_index,
		clarityHWReg.disp_tdshp_clarity_regs.luma_low_gain);
	AALCRT_LOG("luma_low_index[%d] Chroma_adaptive_mode[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.luma_low_index,
		clarityHWReg.disp_tdshp_clarity_regs.Chroma_adaptive_mode);

	AALCRT_LOG("Chroma_shift[%d] Luma_adaptive_mode[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Chroma_shift,
		clarityHWReg.disp_tdshp_clarity_regs.Luma_adaptive_mode);
	AALCRT_LOG("Luma_shift[%d] class_0_positive_gain[%d] class_0_negative_gain[%d]",
		clarityHWReg.disp_tdshp_clarity_regs.Luma_shift,
		clarityHWReg.disp_tdshp_clarity_regs.class_0_positive_gain,
		clarityHWReg.disp_tdshp_clarity_regs.class_0_negative_gain);
}

static int mtk_disp_clarity_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_CLARITY_REG *clarity_regs)
{
	int ret = 0;
	struct mtk_ddp_comp *comp_tdshp;

	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

	if (comp->id == DDP_COMPONENT_AAL0)
		comp_tdshp = priv->ddp_comp[DDP_COMPONENT_TDSHP0];
	else
		comp_tdshp = priv->ddp_comp[DDP_COMPONENT_TDSHP2];

	if (clarity_regs == NULL)
		return -1;

	// aal clarity set registers
	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 1);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATEAL,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_impulse_noise_en << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_detect_en << 8 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_slope << 4 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_flt_en << 1 |
		clarity_regs->mdp_aal_clarity_regs.have_bilateral_filter << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_Blending_00,
		(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_D << 27 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_C << 23 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_B << 19 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_A << 15 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_activate_blending_wgt_gain << 11 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt_mode << 9 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_wgt << 4 |
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_en << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_Blending_01,
		clarity_regs->mdp_aal_clarity_regs.dre_bilateral_size_blending_wgt << 0, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT1_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_00,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_0_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_01,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_1_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_CUST_FLT2_02,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_4 << 24 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_3 << 18 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_2 << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_1 << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_2_0 << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_FLT_CONFIG,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_wgt << 12 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_slope << 6 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt_gain << 3 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_gain << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_FREQ_BLENDING,
		(clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt2_out_wgt << 20 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_custom_range_flt1_out_wgt << 15 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_range_flt_out_wgt << 10 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_size_blending_out_wgt << 5 |
		clarity_regs->mdp_aal_clarity_regs.bilateral_contrary_blending_out_wgt << 0), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, dre3_pa + MDP_AAL_DRE_BILATERAL_REGION_PROTECTION,
	(clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_input_shift_bit << 25 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_D << 21 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_C << 13 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_B << 5 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_region_protection_activate_A << 1 |
	clarity_regs->mdp_aal_clarity_regs.dre_bilateral_blending_region_protection_en << 0), ~0);

	// tdshp clarity set registers
	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_00,
		(clarity_regs->disp_tdshp_clarity_regs.tdshp_gain_high << 8 |
		clarity_regs->disp_tdshp_clarity_regs.tdshp_gain_mid << 16), 0xFFFF << 8);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MIDBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.mid_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_V_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_v_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_H_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_h_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_RD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_rd_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_00,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_0 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_1 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_2 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_3 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_01,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_0_4 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_0 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_1 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_2 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_02,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_3 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_1_4 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_0 << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_1 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + HIGHBAND_COEF_LD_CUST_FLT1_03,
		(clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_2 << 0 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_3 << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_coef_ld_custom_range_flt_2_4 << 16), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.mid_negative_offset << 0 |
		clarity_regs->disp_tdshp_clarity_regs.mid_positive_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.high_negative_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.high_positive_offset << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_D,
		(clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.D_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_H,
		(clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.High_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_L,
		(clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.L_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + ACTIVE_PARA_FREQ_M,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_N_offset << 8 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_offset << 16 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_active_parameter_P_gain << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_SIZE_PARA,
		(clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_SMALL_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_MEDIUM_BIG << 6 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARA_BIG_HUGE << 12), 0x3FFFF);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_HUGE << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_HUGE << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_HUGE << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_HUGE << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_BIG,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_BIG << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_BIG << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_BIG << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_BIG << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_MEDIUM << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_MEDIUM << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_MEDIUM << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_MEDIUM << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL,
		(clarity_regs->disp_tdshp_clarity_regs.Mid_size_adaptive_weight_SMALL << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Mid_auto_adaptive_weight_SMALL << 5 |
		clarity_regs->disp_tdshp_clarity_regs.high_size_adaptive_weight_SMALL << 10 |
		clarity_regs->disp_tdshp_clarity_regs.high_auto_adaptive_weight_SMALL << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_CFG,
		(clarity_regs->disp_tdshp_clarity_regs.FREQ_EXTRACT_ENHANCE << 12 |
		clarity_regs->disp_tdshp_clarity_regs.FILTER_HIST_EN << 16),
		((0x1 << 16) | (0x1 << 12)));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_H_weighting << 4 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_weighting << 8 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_weighting << 12), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL,
		(clarity_regs->disp_tdshp_clarity_regs.freq_M_final_weighting << 0 |
		clarity_regs->disp_tdshp_clarity_regs.freq_D_final_weighting << 5 |
		clarity_regs->disp_tdshp_clarity_regs.freq_L_final_weighting << 10 |
		clarity_regs->disp_tdshp_clarity_regs.freq_WH_final_weighting << 15), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + LUMA_CHROMA_PARAMETER,
		(clarity_regs->disp_tdshp_clarity_regs.luma_low_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.luma_low_index << 3 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_index << 8 |
		clarity_regs->disp_tdshp_clarity_regs.luma_high_gain << 13 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_gain << 16 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_low_index << 19 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_index << 24 |
		clarity_regs->disp_tdshp_clarity_regs.chroma_high_gain << 29), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp_tdshp->regs_pa + SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH,
		(clarity_regs->disp_tdshp_clarity_regs.Luma_adaptive_mode << 0 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_adaptive_mode << 1 |
		clarity_regs->disp_tdshp_clarity_regs.SIZE_PARAMETER << 2 |
		clarity_regs->disp_tdshp_clarity_regs.Luma_shift << 12 |
		clarity_regs->disp_tdshp_clarity_regs.Chroma_shift << 15),
		(1 << 15) | (1 << 12) | (1 << 2) | (1 << 1) | (1 << 0));

	cmdq_pkt_write(handle, comp->cmdq_base, comp_tdshp->regs_pa + CLASS_0_2_GAIN,
		(clarity_regs->disp_tdshp_clarity_regs.class_0_positive_gain << 0 |
		clarity_regs->disp_tdshp_clarity_regs.class_0_negative_gain << 5),
		(1 << 5) | (1 << 0));

	CRTC_MMP_MARK(0, clarity_set_regs, comp->id, 2);

	return ret;
}

static void mtk_aal_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	if (g_aal_fo->mtk_dre30_support) {
		int dre_alg_mode = 0;
		phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

		if (atomic_read(&g_aal_change_to_dre30) & 0x1)
			dre_alg_mode = 1;
		if (debug_bypass_alg_mode)
			dre_alg_mode = 0;
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + DISP_AAL_CFG_MAIN,
			(0x0 << 3) | (dre_alg_mode << 4), (1 << 3) | (1 << 4));
	}
	AALFLOW_LOG("\n");
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x1, ~0);

	// for Display Clarity
	if (g_disp_clarity_regs != NULL) {
		mutex_lock(&g_clarity_lock);
		mtk_disp_clarity_set_reg(comp, handle, g_disp_clarity_regs);
		mutex_unlock(&g_clarity_lock);
	}
}

static void mtk_aal_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x0, ~0);
}

static void mtk_aal_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	AALFLOW_LOG("bypass: %d\n", bypass);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CFG,
		bypass, 0x1);
	atomic_set(&g_aal_force_relay, bypass);

	if (bypass == 0) // Enable AAL Histogram
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x3 << 1, (0x3 << 1));

}

static int mtk_aal_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	AALFLOW_LOG("cmd: %d\n", cmd);
	switch (cmd) {
	case INIT_REG:
		if (disp_aal_set_init_reg(comp, handle,
			(struct DISP_AAL_INITREG *) data) < 0) {
			AALERR("INIT_REG: fail\n");
			return -EFAULT;
		}
		break;
	case SET_PARAM:
		if (disp_aal_set_param(comp, handle,
			(struct DISP_AAL_PARAM *) data) < 0) {
			AALERR("SET_PARAM: fail\n");
			return -EFAULT;
		}
		break;
	case FLIP_SRAM:
		disp_aal_flip_sram(comp, handle, __func__);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_aal1 = priv->ddp_comp[DDP_COMPONENT_AAL1];

			disp_aal_flip_sram(comp_aal1, handle, __func__);
		}
		break;
	case BYPASS_AAL:
	{
		int *value = data;

		mtk_aal_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_aal1 = priv->ddp_comp[DDP_COMPONENT_AAL1];

			mtk_aal_bypass(comp_aal1, *value, handle);
		}

	}
		break;
	case SET_CLARITY_REG:
	{
		if (!g_disp_clarity_regs) {
			g_disp_clarity_regs = kmalloc(sizeof(struct DISP_CLARITY_REG), GFP_KERNEL);
			if (g_disp_clarity_regs == NULL) {
				DDPMSG("%s: no memory\n", __func__);
				return -EFAULT;
			}
		}

		if (data == NULL)
			return -EFAULT;

		mutex_lock(&g_clarity_lock);
		memcpy(g_disp_clarity_regs, (struct DISP_CLARITY_REG *)data,
			sizeof(struct DISP_CLARITY_REG));

		dumpAlgAALClarityRegOutput(*g_disp_clarity_regs);
		dumpAlgTDSHPRegOutput(*g_disp_clarity_regs);

		CRTC_MMP_EVENT_START(0, clarity_set_regs, 0, 0);
		if (mtk_disp_clarity_set_reg(comp, handle, g_disp_clarity_regs) < 0) {
			DDPMSG("[Pipe0] %s: clarity_set_reg failed\n", __func__);
			mutex_unlock(&g_clarity_lock);

			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_aal1 = priv->ddp_comp[DDP_COMPONENT_AAL1];

			if (mtk_disp_clarity_set_reg(comp_aal1, handle,
					g_disp_clarity_regs) < 0) {
				DDPMSG("[Pipe1] %s: clarity_set_reg failed\n", __func__);
				mutex_unlock(&g_clarity_lock);

				return -EFAULT;
			}
		}
		CRTC_MMP_EVENT_END(0, clarity_set_regs, 0, 3);
		mutex_unlock(&g_clarity_lock);

		if (atomic_read(&g_force_delay_check_trig) == 1)
			mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		else
			mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
	}
		break;
	default:
		AALERR("error cmd: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

#define DRE_FLT_NUM	(12)
#define CABC_GAINLMT_NUM (11)
struct aal_backup { /* structure for backup AAL register value */
	unsigned int DRE_MAPPING;
	unsigned int DRE_FLT_FORCE[DRE_FLT_NUM];
	unsigned int CABC_00;
	unsigned int CABC_02;
	unsigned int CABC_GAINLMT[CABC_GAINLMT_NUM];
#if defined(DRE3_IN_DISP_AAL)
	unsigned int DRE_BLOCK_INFO_00;
	unsigned int DRE_BLOCK_INFO_01;
	unsigned int DRE_BLOCK_INFO_02;
	unsigned int DRE_BLOCK_INFO_04;
	unsigned int DRE_BLOCK_INFO_05;
	unsigned int DRE_BLOCK_INFO_06;
	unsigned int DRE_BLOCK_INFO_07;
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
#endif
	unsigned int AAL_CFG;
	unsigned int AAL_INTEN;
};
static struct aal_backup g_aal_backup;

static void ddp_aal_dre3_backup(struct mtk_ddp_comp *comp)
{
#if defined(DRE3_IN_DISP_AAL)
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	g_aal_backup.DRE_BLOCK_INFO_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_00);
	g_aal_backup.DRE_BLOCK_INFO_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_01);
	g_aal_backup.DRE_BLOCK_INFO_02 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_02);
	g_aal_backup.DRE_BLOCK_INFO_04 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_04);
	g_aal_backup.DRE_CHROMA_HIST_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_00);
	g_aal_backup.DRE_CHROMA_HIST_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_01);
	g_aal_backup.DRE_ALPHA_BLEND_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_ALPHA_BLEND_00);
	g_aal_backup.DRE_BLOCK_INFO_05 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_05);
	g_aal_backup.DRE_BLOCK_INFO_06 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_06);
	g_aal_backup.DRE_BLOCK_INFO_07 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_07);
	g_aal_backup.SRAM_CFG =
		readl(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG);
	g_aal_backup.DUAL_PIPE_INFO_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00);
	g_aal_backup.DUAL_PIPE_INFO_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01);
#endif
}

static void ddp_aal_dre_backup(struct mtk_ddp_comp *comp)
{
	int i;

	g_aal_backup.DRE_MAPPING =
		readl(comp->regs + DISP_AAL_DRE_MAPPING_00);

	for (i = 0; i < DRE_FLT_NUM; i++)
		g_aal_backup.DRE_FLT_FORCE[i] =
			readl(comp->regs + DISP_AAL_DRE_FLT_FORCE(i));

}

static void ddp_aal_cabc_backup(struct mtk_ddp_comp *comp)
{
#ifndef NOT_SUPPORT_CABC_HW
	int i;

	g_aal_backup.CABC_00 = readl(comp->regs + DISP_AAL_CABC_00);
	g_aal_backup.CABC_02 = readl(comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		g_aal_backup.CABC_GAINLMT[i] =
		    readl(comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_cfg_backup(struct mtk_ddp_comp *comp)
{
	g_aal_backup.AAL_CFG = readl(comp->regs + DISP_AAL_CFG);
	if (comp->id == DDP_COMPONENT_AAL0)
		g_aal_backup.AAL_INTEN = readl(comp->regs + DISP_AAL_INTEN);
}

static void ddp_aal_backup(struct mtk_ddp_comp *comp)
{
	AALFLOW_LOG("\n");
	ddp_aal_cabc_backup(comp);
	ddp_aal_dre_backup(comp);
	ddp_aal_dre3_backup(comp);
	ddp_aal_cfg_backup(comp);
	atomic_set(&g_aal_initialed, 1);
}

static void ddp_aal_dre3_restore(struct mtk_ddp_comp *comp)
{
#if defined(DRE3_IN_DISP_AAL)
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_00,
		g_aal_backup.DRE_BLOCK_INFO_00 & (0x1FFF << 13), 0x1FFF << 13);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_01,
		g_aal_backup.DRE_BLOCK_INFO_01, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_02,
		g_aal_backup.DRE_BLOCK_INFO_02, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_04,
		g_aal_backup.DRE_BLOCK_INFO_04 & (0x3FF << 13), 0x3FF << 13);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_00,
		g_aal_backup.DRE_CHROMA_HIST_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_CHROMA_HIST_01,
		g_aal_backup.DRE_CHROMA_HIST_01 & 0xFFFF, 0xFFFF);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_ALPHA_BLEND_00,
		g_aal_backup.DRE_ALPHA_BLEND_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_05,
		g_aal_backup.DRE_BLOCK_INFO_05, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_06,
		g_aal_backup.DRE_BLOCK_INFO_06, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DRE_BLOCK_INFO_07,
		g_aal_backup.DRE_BLOCK_INFO_07, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG,
		g_aal_backup.SRAM_CFG, 0x1);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00,
		g_aal_backup.DUAL_PIPE_INFO_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01,
		g_aal_backup.DUAL_PIPE_INFO_01, ~0);
#endif

	if (g_aal_fo->mtk_dre30_support) {
		unsigned long flags;

		spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
		ddp_aal_dre3_write_curve_full(comp);
		spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
	}
}

static void ddp_aal_dre_restore(struct mtk_ddp_comp *comp)
{
	int i;

	writel(g_aal_backup.DRE_MAPPING,
		comp->regs + DISP_AAL_DRE_MAPPING_00);

	for (i = 0; i < DRE_FLT_NUM; i++)
		writel(g_aal_backup.DRE_FLT_FORCE[i],
			comp->regs + DISP_AAL_DRE_FLT_FORCE(i));
}

static void ddp_aal_cabc_restore(struct mtk_ddp_comp *comp)
{
#ifndef NOT_SUPPORT_CABC_HW
	int i;

	writel(g_aal_backup.CABC_00, comp->regs + DISP_AAL_CABC_00);
	writel(g_aal_backup.CABC_02, comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		writel(g_aal_backup.CABC_GAINLMT[i],
			comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_cfg_restore(struct mtk_ddp_comp *comp)
{
	writel(g_aal_backup.AAL_CFG, comp->regs + DISP_AAL_CFG);
	if (comp->id == DDP_COMPONENT_AAL0)
		writel(g_aal_backup.AAL_INTEN, comp->regs + DISP_AAL_INTEN);
}

static void ddp_aal_restore(struct mtk_ddp_comp *comp)
{
	if (atomic_read(&g_aal_initialed) != 1)
		return;

	AALFLOW_LOG("\n");
	ddp_aal_cabc_restore(comp);
	ddp_aal_dre_restore(comp);
	ddp_aal_dre3_restore(comp);
	ddp_aal_cfg_restore(comp);
}

static bool debug_skip_first_br;
static void mtk_aal_prepare(struct mtk_ddp_comp *comp)
{
	int ret = 0;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool first_restore = (atomic_read(&aal_data->is_clock_on) == 0);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&aal_data->is_clock_on, 1);
	if (comp->id == DDP_COMPONENT_AAL0) {
		atomic_set(&g_aal_data->is_clock_on, 1);
		atomic_set(&g_aal_first_frame, 1);
	}
	if (comp->id == DDP_COMPONENT_AAL1) {
		atomic_set(&g_aal1_data->is_clock_on, 1);
		atomic_set(&g_aal1_first_frame, 1);
	}
	AALFLOW_LOG("[aal_data, g_aal_data] addr[%x, %x] val[%d, %d]\n",
			&aal_data->is_clock_on, &g_aal_data->is_clock_on,
			atomic_read(&aal_data->is_clock_on),
			atomic_read(&g_aal_data->is_clock_on));

	if (g_aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk) {
			ret = clk_prepare(aal_data->dre3_hw.clk);
			if (ret < 0)
				DDPPR_ERR("failed to prepare dre3_hw.clk\n");
		}
	}
	if (!first_restore && !debug_skip_first_br)
		return;

	/* Bypass shadow register and read shadow register */
	if (aal_data->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, AAL_BYPASS_SHADOW,
			DISP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);

	ddp_aal_restore(comp);

	if (g_aal_fo->mtk_dre30_support) {
		if (comp->mtk_crtc->is_dual_pipe) {
			if (comp->id == DDP_COMPONENT_AAL0) {
				if (atomic_cmpxchg(&g_aal_dre_hw_init, 0, 1) == 0)
					disp_aal_dre3_init(comp);
			} else if (comp->id == DDP_COMPONENT_AAL1) {
				if (atomic_cmpxchg(&g_aal1_dre_hw_init, 0, 1) == 0)
					disp_aal_dre3_init(comp);
			}
		} else
			if (atomic_cmpxchg(&g_aal_dre_hw_init, 0, 1) == 0)
				disp_aal_dre3_init(comp);
	}
}

static void mtk_aal_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	unsigned long flags;
	bool first_backup = (atomic_read(&aal_data->is_clock_on) == 1);

	AALFLOW_LOG("\n");
	spin_lock_irqsave(&g_aal_clock_lock, flags);
	atomic_set(&aal_data->is_clock_on, 0);
	if (comp->id == DDP_COMPONENT_AAL0)
		atomic_set(&g_aal_data->is_clock_on, 0);

	if (comp->id == DDP_COMPONENT_AAL1)
		atomic_set(&g_aal1_data->is_clock_on, 0);

	spin_unlock_irqrestore(&g_aal_clock_lock, flags);
	if (first_backup || debug_skip_first_br)
		ddp_aal_backup(comp);
	//disp_aal_clear_irq(comp, true);
	mtk_ddp_comp_clk_unprepare(comp);
	if (g_aal_fo->mtk_dre30_support) {
		if (aal_data->dre3_hw.clk)
			clk_unprepare(aal_data->dre3_hw.clk);
	}
}

void mtk_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	AALFLOW_LOG("\n");
	mtk_aal_config(comp, cfg, handle);
}

int mtk_aal_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	unsigned long flags;
	uint32_t force_delay_trigger;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (cmd == FRAME_DIRTY) {
		AALFLOW_LOG("FRAME_DIRTY comp id:%d\n", comp->id);
		if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
			if (atomic_read(&aal_data->is_clock_on) != 1)
				AALIRQ_LOG("clock is off\n");
			else
				disp_aal_set_interrupt(NULL, true);
			spin_unlock_irqrestore(&g_aal_clock_lock, flags);
		}
	} else if (cmd == FORCE_TRIG_CTL) {
		force_delay_trigger = *(uint32_t *)params;
		atomic_set(&g_force_delay_check_trig, force_delay_trigger);
	}
	AALFLOW_LOG("end\n");
	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_aal_funcs = {
	.config = mtk_aal_config,
	.first_cfg = mtk_aal_first_cfg,
	.start = mtk_aal_start,
	.stop = mtk_aal_stop,
	.bypass = mtk_aal_bypass,
	.user_cmd = mtk_aal_user_cmd,
	.io_cmd = mtk_aal_io_cmd,
	.prepare = mtk_aal_prepare,
	.unprepare = mtk_aal_unprepare,
};

static int mtk_disp_aal_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_aal_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_aal_component_ops = {
	.bind	= mtk_disp_aal_bind,
	.unbind = mtk_disp_aal_unbind,
};

void mtk_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem  *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, 0x200, 0x10);
}

void disp_aal_on_end_of_frame(struct mtk_ddp_comp *comp)
{
	//For 120Hz rotation issue
	ktime_get_ts64(&start);

	if (comp->id == DDP_COMPONENT_AAL0)
		atomic_set(&g_aal_eof_irq, 1);
	if (comp->id == DDP_COMPONENT_AAL1)
		atomic_set(&g_aal1_eof_irq, 1);

	if (atomic_read(&g_aal_force_relay) == 1) {
		disp_aal_clear_irq(comp, true);
		return;
	}

	if (g_aal_fo->mtk_dre30_support && gDre30Enabled)
		disp_aal_dre3_irq_handle(comp);
	else
		disp_aal_single_pipe_hist_update(comp);

	AALIRQ_LOG("[SRAM] clean dre_config in (EOF)  comp->id = %d", comp->id);
	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_AAL0)
			atomic_set(&g_aal_dre_config, 0);
		else if (comp->id == DDP_COMPONENT_AAL1)
			atomic_set(&g_aal1_dre_config, 0);
	} else
		atomic_set(&g_aal_dre_config, 0);
}

static void disp_aal_wait_sof_irq(void)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data;
	int ret = 0;
	int aal_lock = 0;
	int retry = 5;

	if (atomic_read(&g_aal_sof_irq_available) == 0) {
		AALFLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(g_aal_sof_irq_wq,
				atomic_read(&g_aal_sof_irq_available) == 1);
		AALFLOW_LOG("sof_irq_available = 1, waken up, ret = %d", ret);
	} else {
		AALFLOW_LOG("sof_irq_available = 0");
		return;
	}

	CRTC_MMP_EVENT_START(0, aal_sof_thread, 0, 0);

	aal_data = comp_to_aal(default_comp);
	AALIRQ_LOG("[SRAM] g_aal_dre_config(%d) in SOF",
			atomic_read(&g_aal_dre_config));
	mutex_lock(&g_aal_sram_lock);
	spin_lock_irqsave(&g_aal_clock_lock, flags);
	if (atomic_read(&aal_data->is_clock_on) != 1)
		AALIRQ_LOG("clock is off\n");
	else
		disp_aal_update_dre3_sram(default_comp, true);
	spin_unlock_irqrestore(&g_aal_clock_lock,
			flags);
	if (!isDualPQ)
		mutex_unlock(&g_aal_sram_lock);
	CRTC_MMP_MARK(0, aal_sof_thread, 0, 1);

	if (isDualPQ) {
		struct mtk_disp_aal *aal1_data = comp_to_aal(aal1_default_comp);

		AALIRQ_LOG("[SRAM] g_aal1_dre_config(%d) in SOF",
			atomic_read(&g_aal1_dre_config));
		while ((!aal_lock) && (retry != 0)) {
			aal_lock = spin_trylock_irqsave(&g_aal_clock_lock, flags);
			if (!aal_lock) {
				usleep_range(500, 1000);
				retry--;
			}
		}
		if (aal_lock) {
			if (atomic_read(&aal1_data->is_clock_on) != 1)
				AALIRQ_LOG("aal1 clock is off\n");
			else
				disp_aal_update_dre3_sram(aal1_default_comp, true);

			spin_unlock_irqrestore(&g_aal_clock_lock,
				flags);
		}
		mutex_unlock(&g_aal_sram_lock);
		CRTC_MMP_MARK(0, aal_sof_thread, 0, 2);

		if (atomic_read(&g_aal_first_frame) == 1 &&
			atomic_read(&g_aal1_first_frame) == 1) {
			mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
			mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
			atomic_set(&g_aal_first_frame, 0);
			atomic_set(&g_aal1_first_frame, 0);

			return;
		}
	} else {
		if (atomic_read(&g_aal_first_frame) == 1) {
			AALIRQ_LOG("aal_refresh_task");
			mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
			mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
			atomic_set(&g_aal_first_frame, 0);

			return;
		}
	}

	if (atomic_read(&g_aal_interrupt_enabled) == 1) {
		if (default_comp->mtk_crtc->is_dual_pipe) {
			if (!atomic_read(&g_aal_first_frame) &&
				!atomic_read(&g_aal1_first_frame))
				mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
		} else {
			if (!atomic_read(&g_aal_first_frame))
				mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
		}
	}

	if (atomic_read(&g_aal_dre30_write) == 1) {
		mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		atomic_set(&g_aal_dre30_write, 0);
	}
	CRTC_MMP_EVENT_END(0, aal_sof_thread, 0, 3);
}

void disp_aal_on_start_of_frame(void)
{
	if (!default_comp)
		return;

	if (!g_aal_fo->mtk_dre30_support || !gDre30Enabled) {
		if (isDualPQ) {
			if (atomic_read(&g_aal0_dre20_hist_is_ready) &&
				atomic_read(&g_aal1_dre20_hist_is_ready)) {
				atomic_set(&g_aal0_hist_available, 1);
				atomic_set(&g_aal1_hist_available, 1);
				wake_up_interruptible(&g_aal_hist_wq);
			}
		} else {
			if (atomic_read(&g_aal0_dre20_hist_is_ready)) {
				atomic_set(&g_aal0_hist_available, 1);
				wake_up_interruptible(&g_aal_hist_wq);
			}
		}
		return;
	}

	if (atomic_read(&g_aal_force_relay) == 1 &&
		!m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS] &&
		!m_new_pq_persist_property[DISP_PQ_GAMMA_SILKY_BRIGHTNESS])
		return;
	if (atomic_read(&g_aal_change_to_dre30) != 0x3)
		return;
	if (aal_sram_method != AAL_SRAM_SOF)
		return;

	if (!atomic_read(&g_aal_sof_irq_available)) {
		atomic_set(&g_aal_sof_irq_available, 1);
		wake_up_interruptible(&g_aal_sof_irq_wq);
	}
}

static int mtk_aal_sof_irq_trigger(void *data)
{
	while (!kthread_should_stop()) {
		disp_aal_wait_sof_irq();
		atomic_set(&g_aal_sof_irq_available, 0);
	}

	return 0;
}

static irqreturn_t mtk_disp_aal_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	unsigned int val = 0;
	irqreturn_t ret = IRQ_NONE;
	struct mtk_disp_aal *priv = dev_id;
	struct mtk_ddp_comp *comp = &priv->ddp_comp;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (atomic_read(&aal_data->is_clock_on) == 1)
		val = readl(comp->regs + DISP_AAL_INTSTA);

	DRM_MMP_MARK(IRQ, irq, val);
	DRM_MMP_MARK(aal0, val, 0);

	spin_lock_irqsave(&g_aal_clock_lock, flags);
	if (atomic_read(&aal_data->is_clock_on) != 1)
		AALIRQ_LOG("clock is off\n");
	else {
		disp_aal_on_end_of_frame(comp);
		if (mtk_crtc->is_dual_pipe) {
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *dev_priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp1 = dev_priv->ddp_comp[DDP_COMPONENT_AAL1];

			if (comp1)
				disp_aal_on_end_of_frame(comp1);
		}
		ret = IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&g_aal_clock_lock, flags);

	DRM_MMP_MARK(aal0, val, 1);

	return ret;
}

static int mtk_disp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_aal *priv;
	enum mtk_ddp_comp_id comp_id;
	struct device_node *tdshp_node;
	int ret, irq;
	struct device_node *dre3_dev_node;
	struct platform_device *dre3_pdev;
	struct resource dre3_res;
	struct sched_param param = {.sched_priority = 85 };
	struct cpumask mask;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_AAL);
	if ((int)comp_id < 0) {
		AALERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}
	if (comp_id == DDP_COMPONENT_AAL0)
		g_aal_data = priv;
	if (comp_id == DDP_COMPONENT_AAL1)
		g_aal1_data = priv;
	atomic_set(&priv->dirty_frame_retrieved, 1);
	atomic_set(&priv->is_clock_on, 0);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	if (comp_id == DDP_COMPONENT_AAL0) {
		g_aal_fo = devm_kzalloc(dev, sizeof(*g_aal_fo), GFP_KERNEL);
		if (g_aal_fo == NULL)
			return -ENOMEM;

		// for Display Clarity
		tdshp_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_tdshp0");
		if (!of_property_read_u32(tdshp_node, "mtk-tdshp-clarity-support",
			&g_tdshp_clarity_support)) {
			DDPMSG("disp_tdshp: mtk_tdshp_clarity_support = %d\n",
				g_tdshp_clarity_support);
		}

		if (of_property_read_u32(dev->of_node, "mtk_aal_support",
			&g_aal_fo->mtk_aal_support)) {
			AALERR("comp_id: %d, mtk_aal_support = %d\n",
				comp_id, g_aal_fo->mtk_aal_support);
			g_aal_fo->mtk_aal_support = 0;
		}

		if (of_property_read_u32(dev->of_node, "mtk_dre30_support",
			&g_aal_fo->mtk_dre30_support)) {
			AALERR("comp_id: %d, mtk_dre30_support = %d\n",
				comp_id, g_aal_fo->mtk_dre30_support);
			g_aal_fo->mtk_dre30_support = 0;
		} else {
			if (g_aal_fo->mtk_dre30_support) {
				if (of_property_read_u32(dev->of_node, "aal_dre3_en",
					&g_aal_dre30_en)) {
					gDre30Enabled = true;
				} else
					gDre30Enabled = (g_aal_dre30_en == 1) ? true : false;

				// for Display Clarity
				if (!of_property_read_u32(dev->of_node, "mtk-aal-clarity-support",
					&g_aal_clarity_support))
					DDPMSG("mtk_aal_clarity_support = %d\n",
						g_aal_clarity_support);

				if ((g_aal_clarity_support == 1)
						&& (g_tdshp_clarity_support == 1)) {
					g_disp_clarity_support = 1;
					DDPMSG("%s: display clarity support = %d\n",
						__func__, g_disp_clarity_support);
				}
			}
		}
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_aal_funcs);
	if (ret) {
		AALERR("Failed to initialize component: %d\n", ret);
		return ret;
	}
	if (!default_comp && comp_id == DDP_COMPONENT_AAL0)
		default_comp = &priv->ddp_comp;

	if (!aal1_default_comp && comp_id == DDP_COMPONENT_AAL1)
		aal1_default_comp = &priv->ddp_comp;

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	if (comp_id == DDP_COMPONENT_AAL0) {
		ret = devm_request_irq(dev, irq, mtk_disp_aal_irq_handler,
			IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
		if (ret)
			dev_err(dev, "devm_request_irq fail: %d\n", ret);
	}

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	do {
		if (!g_aal_fo->mtk_dre30_support) {
			pr_notice("[debug] dre30 is not support\n");
			break;
		}
		dre3_dev_node = of_parse_phandle(
			pdev->dev.of_node, "aal_dre3", 0);
		if (dre3_dev_node)
			pr_notice("found dre3 aal node, it's another hw\n");
		else
			break;
		dre3_pdev = of_find_device_by_node(dre3_dev_node);
		if (dre3_pdev)
			pr_notice("found dre3 aal device, it's another hw\n");
		else
			break;
		of_node_put(dre3_dev_node);
		priv->dre3_hw.dev = &dre3_pdev->dev;
		priv->dre3_hw.va = of_iomap(dre3_pdev->dev.of_node, 0);
		if (!priv->dre3_hw.va) {
			pr_notice("cannot found allocate dre3 va!\n");
			break;
		}
		ret = of_address_to_resource(
			dre3_pdev->dev.of_node, 0, &dre3_res);
		if (ret) {
			pr_notice("cannot found allocate dre3 resource!\n");
			break;
		}
		priv->dre3_hw.pa = dre3_res.start;
		if (comp_id == DDP_COMPONENT_AAL0) {
			priv->dre3_hw.clk = of_clk_get_by_name(
				dre3_dev_node, "DRE3_AAL0");

		} else if (comp_id == DDP_COMPONENT_AAL1) {
			priv->dre3_hw.clk = of_clk_get_by_name(
				dre3_dev_node, "DRE3_AAL1");
		}
		if (IS_ERR(priv->dre3_hw.clk)) {
			pr_notice("fail @ dre3 clock. name:%s\n",
				"DRE3_AAL0");
			break;
		}
		pr_notice("dre3 dev:%p va:%p pa:%pa", priv->dre3_hw.dev,
			priv->dre3_hw.va, &priv->dre3_hw.pa);
	} while (0);

	ret = component_add(dev, &mtk_disp_aal_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (comp_id == DDP_COMPONENT_AAL0)
		mtk_leds_register_notifier(&leds_init_notifier);
#endif

	if (comp_id == DDP_COMPONENT_AAL0) {
		aal_flip_wq = create_singlethread_workqueue("aal_flip_sram");
		INIT_WORK(&g_aal_data->aal_flip_task, mtk_crtc_user_cmd_work);

		aal_refresh_wq = create_singlethread_workqueue("aal_refresh_trigger");
		INIT_WORK(&g_aal_data->aal_refresh_task, mtk_disp_aal_refresh_trigger);

		aal_sof_irq_event_task =
			kthread_create(mtk_aal_sof_irq_trigger,
				NULL, "aal_sof");
		cpumask_setall(&mask);
		cpumask_clear_cpu(0, &mask);
		set_cpus_allowed_ptr(aal_sof_irq_event_task, &mask);
		if (sched_setscheduler(aal_sof_irq_event_task, SCHED_RR, &param))
			pr_notice("aal_sof_irq_event_task setschedule fail");
		wake_up_process(aal_sof_irq_event_task);
	}

	AALFLOW_LOG("-\n");
	return ret;
}

static int mtk_disp_aal_remove(struct platform_device *pdev)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_aal_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (priv->ddp_comp.id == DDP_COMPONENT_AAL0)
		mtk_leds_unregister_notifier(&leds_init_notifier);
#endif

	return 0;
}

static const struct mtk_disp_aal_data mt6885_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.aal_dre_hist_start = 1152,
	.aal_dre_hist_end   = 4220,
	.aal_dre_gain_start = 4224,
	.aal_dre_gain_end   = 6396,
	.bitShift = 13,
};

static const struct mtk_disp_aal_data mt6873_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6853_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6833_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6983_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6895_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6879_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6855_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6985_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct mtk_disp_aal_data mt6886_aal_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.aal_dre_hist_start = 1536,
	.aal_dre_hist_end   = 4604,
	.aal_dre_gain_start = 4608,
	.aal_dre_gain_end   = 6780,
	.bitShift = 16,
};

static const struct of_device_id mtk_disp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-disp-aal",
	  .data = &mt6885_aal_driver_data},
	{ .compatible = "mediatek,mt6873-disp-aal",
	  .data = &mt6873_aal_driver_data},
	{ .compatible = "mediatek,mt6853-disp-aal",
	  .data = &mt6853_aal_driver_data},
	{ .compatible = "mediatek,mt6833-disp-aal",
	  .data = &mt6833_aal_driver_data},
	{ .compatible = "mediatek,mt6983-disp-aal",
	  .data = &mt6983_aal_driver_data},
	{ .compatible = "mediatek,mt6895-disp-aal",
	  .data = &mt6895_aal_driver_data},
	{ .compatible = "mediatek,mt6879-disp-aal",
	  .data = &mt6879_aal_driver_data},
	{ .compatible = "mediatek,mt6855-disp-aal",
	  .data = &mt6855_aal_driver_data},
	{ .compatible = "mediatek,mt6985-disp-aal",
	  .data = &mt6985_aal_driver_data},
	{ .compatible = "mediatek,mt6886-disp-aal",
	  .data = &mt6886_aal_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_aal_driver_dt_match);

struct platform_driver mtk_disp_aal_driver = {
	.probe		= mtk_disp_aal_probe,
	.remove		= mtk_disp_aal_remove,
	.driver		= {
		.name	= "mediatek-disp-aal",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_aal_driver_dt_match,
	},
};

/* Legacy AAL_SUPPORT_KERNEL_API */
void disp_aal_set_lcm_type(unsigned int panel_type)
{
	unsigned long flags;

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	atomic_set(&g_aal_panel_type, panel_type);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	AALAPI_LOG("panel_type = %d", panel_type);
}

#define AAL_CONTROL_CMD(ID, CONTROL) (ID << 16 | CONTROL)
void disp_aal_set_ess_level(int level)
{
	unsigned long flags;
	int level_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_ess_level_cmd_id += 1;
	g_aal_ess_level_cmd_id = g_aal_ess_level_cmd_id % 64;
	level_command = AAL_CONTROL_CMD(g_aal_ess_level_cmd_id, level);

	g_aal_ess_level = level_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_refresh_by_kernel();
	AALAPI_LOG("level = %d (cmd = 0x%x)", level, level_command);
}

void disp_aal_set_ess_en(int enable)
{
	unsigned long flags;
	int enable_command = 0;
	int level_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_ess_en_cmd_id += 1;
	g_aal_ess_en_cmd_id = g_aal_ess_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(g_aal_ess_en_cmd_id, enable);

	g_aal_ess_en = enable_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_refresh_by_kernel();
	AALAPI_LOG("en = %d (cmd = 0x%x) level = 0x%08x (cmd = 0x%x)",
		enable, enable_command, ESS_LEVEL_BY_CUSTOM_LIB, level_command);
}

void disp_aal_set_dre_en(int enable)
{
	unsigned long flags;
	int enable_command = 0;

	spin_lock_irqsave(&g_aal_hist_lock, flags);

	g_aal_dre_en_cmd_id += 1;
	g_aal_dre_en_cmd_id = g_aal_dre_en_cmd_id % 64;
	enable_command = AAL_CONTROL_CMD(g_aal_dre_en_cmd_id, enable);

	g_aal_dre_en = enable_command;

	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_refresh_by_kernel();
	AALAPI_LOG("en = %d (cmd = 0x%x)", enable, enable_command);
}

void disp_aal_debug(const char *opt)
{
	pr_notice("[debug]: %s\n", opt);
	if (strncmp(opt, "setparam:", 9) == 0) {
		debug_skip_set_param = strncmp(opt + 9, "skip", 4) == 0;
		pr_notice("[debug] skip_set_param=%d\n",
			debug_skip_set_param);
	} else if (strncmp(opt, "dre3irq:", 8) == 0) {
		debug_skip_dre3_irq = strncmp(opt + 8, "skip", 4) == 0;
		pr_notice("[debug] skip_dre3_irq=%d\n",
			debug_skip_dre3_irq);
	} else if (strncmp(opt, "dre3algmode:", 12) == 0) {
		debug_bypass_alg_mode = strncmp(opt + 12, "bypass", 6) == 0;
		pr_notice("[debug] bypass_alg_mode=%d\n",
			debug_bypass_alg_mode);
	} else if (strncmp(opt, "dumpregirq", 10) == 0) {
		debug_dump_reg_irq = true;
		pr_notice("[debug] debug_dump_reg_irq=%d\n",
			debug_dump_reg_irq);
	} else if (strncmp(opt, "dumpdre3hist:", 13) == 0) {
		if (sscanf(opt + 13, "%d %d",
			&dump_blk_x, &dump_blk_y) == 2)
			pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n",
				dump_blk_x, dump_blk_y);
		else
			pr_notice("[debug] dump_blk parse fail\n");
	} else if (strncmp(opt, "first_br:", 9) == 0) {
		debug_skip_first_br = strncmp(opt + 9, "skip", 4) == 0;
		pr_notice("[debug] skip_first_br=%d\n",
			debug_skip_first_br);
	} else if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[debug] debug_flow_log=%d\n",
			debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_api_log=%d\n",
			debug_api_log);
	} else if (strncmp(opt, "write_cmdq_log:", 15) == 0) {
		debug_write_cmdq_log = strncmp(opt + 15, "1", 1) == 0;
		pr_notice("[debug] debug_write_cmdq_log=%d\n",
			debug_write_cmdq_log);
	} else if (strncmp(opt, "irq_log:", 8) == 0) {
		debug_irq_log = strncmp(opt + 8, "1", 1) == 0;
		pr_notice("[debug] debug_irq_log=%d\n",
			debug_irq_log);
	} else if (strncmp(opt, "dump_aal_hist:", 14) == 0) {
		debug_dump_aal_hist = strncmp(opt + 14, "1", 1) == 0;
		pr_notice("[debug] debug_dump_aal_hist=%d\n",
			debug_dump_aal_hist);
	} else if (strncmp(opt, "dump_input_param:", 17) == 0) {
		debug_dump_input_param = strncmp(opt + 17, "1", 1) == 0;
		pr_notice("[debug] debug_dump_input_param=%d\n",
			debug_dump_input_param);
	} else if (strncmp(opt, "set_ess_level:", 14) == 0) {
		int debug_ess_level;

		if (sscanf(opt + 14, "%d", &debug_ess_level) == 1) {
			pr_notice("[debug] ess_level=%d\n", debug_ess_level);
			disp_aal_set_ess_level(debug_ess_level);
		} else
			pr_notice("[debug] set_ess_level failed\n");
	} else if (strncmp(opt, "set_ess_en:", 11) == 0) {
		bool debug_ess_en;

		debug_ess_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_ess_en=%d\n", debug_ess_en);
		disp_aal_set_ess_en(debug_ess_en);
	} else if (strncmp(opt, "set_dre_en:", 11) == 0) {
		bool debug_dre_en;

		debug_dre_en = !strncmp(opt + 11, "1", 1);
		pr_notice("[debug] debug_dre_en=%d\n", debug_dre_en);
		disp_aal_set_dre_en(debug_dre_en);
	} else if (strncmp(opt, "aal_sram_method:", 16) == 0) {
		bool aal_align_eof;

		if (g_aal_fo->mtk_dre30_support) {
			aal_align_eof = !strncmp(opt + 11, "0", 1);
			aal_sram_method = aal_align_eof ? AAL_SRAM_EOF : AAL_SRAM_SOF;
			pr_notice("[debug] aal_sram_method=%d\n", aal_sram_method);
		} else {
			pr_notice("[debug] dre30 is not support\n");
		}
	} else if (strncmp(opt, "dump_clarity_regs:", 18) == 0) {
		debug_dump_clarity_regs = strncmp(opt + 18, "1", 1) == 0;
		pr_notice("[debug] debug_dump_clarity_regs=%d\n",
			debug_dump_clarity_regs);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		pr_notice("[debug] skip_set_param=%d\n",
			debug_skip_set_param);
		pr_notice("[debug] skip_dre3_irq=%d\n",
			debug_skip_dre3_irq);
		pr_notice("[debug] bypass_alg_mode=%d\n",
			debug_bypass_alg_mode);
		pr_notice("[debug] debug_dump_reg_irq=%d\n",
			debug_dump_reg_irq);
		pr_notice("[debug] dump_blk_x=%d dump_blk_y=%d\n",
			dump_blk_x, dump_blk_y);
		pr_notice("[debug] skip_first_br=%d\n",
			debug_skip_first_br);
		pr_notice("[debug] debug_flow_log=%d\n",
			debug_flow_log);
		pr_notice("[debug] debug_api_log=%d\n",
			debug_api_log);
		pr_notice("[debug] debug_write_cmdq_log=%d\n",
			debug_write_cmdq_log);
		pr_notice("[debug] debug_irq_log=%d\n",
			debug_irq_log);
		pr_notice("[debug] debug_dump_aal_hist=%d\n",
			debug_dump_aal_hist);
		pr_notice("[debug] debug_dump_input_param=%d\n",
			debug_dump_input_param);
		pr_notice("[debug] debug_ess_level=%d\n", g_aal_ess_level);
		pr_notice("[debug] debug_ess_en=%d\n", g_aal_ess_en);
		pr_notice("[debug] debug_dre_en=%d\n", g_aal_dre_en);
		pr_notice("[debug] debug_dump_clarity_regs=%d\n", debug_dump_clarity_regs);
	}
}

void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	if (atomic_read(&g_aal_force_relay) == bypass)
		return;
	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_AAL, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

int mtk_drm_ioctl_aal_set_trigger_state(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	unsigned long flags;
	unsigned int dre3EnState;

	struct DISP_AAL_TRIG_STATE *trigger_state = (struct DISP_AAL_TRIG_STATE *)data;

	dre3EnState = trigger_state->dre3_en_state;

	AALFLOW_LOG("dre3EnState: 0x%x, trigger_state: %d, ali: %d, aliThres: %d\n",
			dre3EnState,
			trigger_state->dre_frm_trigger,
			trigger_state->curAli, trigger_state->aliThreshold);

	if (dre3EnState & 0x2) {
		AALFLOW_LOG("dre change to open!\n");

		spin_lock_irqsave(&g_aal_hist_lock, flags);
		if ((g_aal_fo->mtk_dre30_support) && (!gDre30Enabled)) {
			gPrvDre30Enabled = gDre30Enabled;
			gDre30Enabled = true;
		}
		spin_unlock_irqrestore(&g_aal_hist_lock, flags);

		if (gPrvDre30Enabled != gDre30Enabled) {
			// need flip sram to get local histogram
			mtk_crtc_user_cmd(g_aal_data->crtc, default_comp, FLIP_SRAM, NULL);
			gPrvDre30Enabled = gDre30Enabled;
		}

		trigger_state->dre3_krn_flag = gDre30Enabled ? 1 : 0;

		return 0;
	}

	if (dre3EnState & 0x1) {
		spin_lock_irqsave(&g_aal_hist_lock, flags);
		if (trigger_state->dre_frm_trigger == 0) {
			if ((g_aal_fo->mtk_dre30_support) && gDre30Enabled) {
				gDre30Enabled = false;

				AALFLOW_LOG("dre change to close!\n");
			}
		}

		trigger_state->dre3_krn_flag = gDre30Enabled ? 1 : 0;
		spin_unlock_irqrestore(&g_aal_hist_lock, flags);
	}

	return 0;
}

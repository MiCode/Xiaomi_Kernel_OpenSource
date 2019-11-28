/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/module.h>

#ifdef CONFIG_LEDS_MTK_DISP
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <mtk_leds_drv.h>
#include <leds-mtk-disp.h>
#elif defined CONFIG_LEDS_MTK_PWM
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <mtk_leds_drv.h>
#include <leds-mtk-pwm.h>
#else
#define mt_leds_brightness_set(x, y) do { } while (0)
#define MT65XX_LED_MODE_NONE (0)
#define MT65XX_LED_MODE_CUST_LCM (4)
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_color.h"

// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;

/* To enable debug log: */
/* # echo aal_dbg:1 > /sys/kernel/debug/dispsys */
int aal_dbg_en;

static DECLARE_WAIT_QUEUE_HEAD(g_aal_hist_wq);
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
static atomic_t g_aal_hist_available = ATOMIC_INIT(0);
static atomic_t g_aal_is_init_regs_valid = ATOMIC_INIT(0);
static atomic_t g_aal_backlight_notified = ATOMIC_INIT(1023);
static atomic_t g_aal_initialed = ATOMIC_INIT(0);
static atomic_t g_aal_allowPartial = ATOMIC_INIT(0);
static atomic_t g_aal_force_enable_irq = ATOMIC_INIT(0);
static atomic_t g_led_mode = ATOMIC_INIT(MT65XX_LED_MODE_NONE);
static atomic_t g_aal_force_relay = ATOMIC_INIT(0);

enum AAL_UPDATE_HIST {
	UPDATE_NONE = 0,
	UPDATE_SINGLE,
	UPDATE_MULTIPLE
};
#if defined(CONFIG_MTK_DRE30_SUPPORT)
/* #define DRE3_IN_DISP_AAL */
/* HW specified */
#define AAL_DRE_HIST_START	(1152)
#define AAL_DRE_HIST_END	(4220)
#define AAL_DRE_GAIN_START	(4224)
#define AAL_DRE_GAIN_END	(6396)

static DEFINE_SPINLOCK(g_aal_dre3_gain_lock);
static atomic_t g_aal_force_hist_apb = ATOMIC_INIT(0);
static atomic_t g_aal_dre_halt = ATOMIC_INIT(0);
static atomic_t g_aal_dre_hw_init = ATOMIC_INIT(0);

static struct DISP_DRE30_INIT g_aal_init_dre30;
static struct DISP_DRE30_PARAM g_aal_gain;
static struct DISP_DRE30_PARAM g_aal_gain_db;
static struct DISP_DRE30_HIST g_aal_dre30_hist;
static struct DISP_DRE30_HIST g_aal_dre30_hist_db;

static DECLARE_WAIT_QUEUE_HEAD(g_aal_size_wq);
static bool g_aal_get_size_available;
static struct DISP_AAL_DISPLAY_SIZE g_aal_size;
static atomic_t g_aal_change_to_dre30 = ATOMIC_INIT(0);
#endif	/* CONFIG_MTK_DRE30_SUPPORT */

static atomic_t g_aal_panel_type = ATOMIC_INIT(CONFIG_BY_CUSTOM_LIB);
static int g_aal_ess_level = ESS_LEVEL_BY_CUSTOM_LIB;
static int g_aal_dre_en = DRE_EN_BY_CUSTOM_LIB;
static int g_aal_ess_en = ESS_EN_BY_CUSTOM_LIB;
#if 0
static int g_aal_ess_level_cmd_id;
static int g_aal_dre_en_cmd_id;
static int g_aal_ess_en_cmd_id;
#endif
#define aal_min(a, b)			(((a) < (b)) ? (a) : (b))

enum AAL_IOCTL_CMD {
	INIT_REG = 0,
	SET_PARAM
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
};
static struct mtk_disp_aal *g_aal_data;

static inline struct mtk_disp_aal *comp_to_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_aal, ddp_comp);
}

#if defined(CONFIG_MTK_DRE30_SUPPORT)
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
		value |= data;
	}
	writel(value, address);
}
#endif

/* config register which might have extra DRE3 aal hw */
static inline s32 basic_cmdq_write(struct cmdq_pkt *handle,
	struct mtk_ddp_comp *comp, u32 offset, u32 value, u32 mask)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	s32 result;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

	result = cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + offset, value, mask);
	if (result) {
		DDPMSG("AAL: write reg fail, offset:%#x\n", offset);
		return result;
	}
	DDPINFO("%s - write 0x%03x with 0x%08x (0x%08x)\n",
		__func__, offset, value, mask);
	if (aal_data->dre3_hw.dev)
		result = cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + offset, value, mask);
	return result;
#else
	return cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + offset, value, mask);
#endif
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
		DDPPR_ERR("Cannot find LED node from dts\n");
	} else {
		ret = of_property_read_u32(led_node, "led_mode", &led_mode);
		if (!ret)
			atomic_set(&g_led_mode, led_mode);
		else
			DDPINFO("led dts can not get led mode data.\n");

		ret = of_property_read_u32_array(led_node,
	    "pwm_config", pwm_config, ARRAY_SIZE(pwm_config));
	}

	if (ret)
		DDPPR_ERR("get pwm cust info fail");
	DDPINFO("%s mode=%u", __func__, atomic_read(&g_led_mode));

	return ret;
}


#define LOG_INTERVAL_TH 200
#define LOG_BUFFER_SIZE 4
static char g_aal_log_buffer[256] = "";
static int g_aal_log_index;
struct timeval g_aal_log_prevtime = {0};

bool disp_aal_is_support(void)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	return true;
#else
	return false;
#endif		// CONFIG_MTK_AAL_SUPPORT
}

static void disp_aal_set_interrupt(struct mtk_ddp_comp *comp, int enable)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (!disp_aal_is_support()) {
		DDPPR_ERR("%s: aal is not support\n", __func__);
		return;
	}

	if (enable && atomic_read(&g_aal_force_relay) != 1) {
		/* Enable output frame end interrupt */
		if (comp == NULL)
			writel(0x2, default_comp->regs + DISP_AAL_INTEN);
		else
			writel(0x2, comp->regs + DISP_AAL_INTEN);
		DDPINFO("%s: interrupt enabled\n", __func__);
	} else if (!enable) {
		if (atomic_read(&aal_data->dirty_frame_retrieved) == 1) {
			if (comp == NULL)
				writel(0x0,
					default_comp->regs + DISP_AAL_INTEN);
			else
				writel(0x0, comp->regs + DISP_AAL_INTEN);
			DDPINFO("%s: interrupt disabled", __func__);
		} //else {
			/* Dirty histogram was not retrieved. */
			/* Only if the dirty hist was retrieved, */
			/* interrupt can be disabled. */
			/* Continue interrupt until AALService can get */
			/* the latest histogram. */
		//}
	}
}

static unsigned long timevaldiff(struct timeval *starttime,
	struct timeval *finishtime)
{
	unsigned long msec;

	msec = (finishtime->tv_sec-starttime->tv_sec)*1000;
	msec += (finishtime->tv_usec-starttime->tv_usec)/1000;

	return msec;
}

static void disp_aal_notify_backlight_log(int bl_1024)
{
	struct timeval aal_time;
	unsigned long diff_mesc = 0;
	unsigned long tsec;
	unsigned long tusec;

	do_gettimeofday(&aal_time);
	tsec = (unsigned long)aal_time.tv_sec % 100;
	tusec = (unsigned long)aal_time.tv_usec / 1000;

	diff_mesc = timevaldiff(&g_aal_log_prevtime, &aal_time);
	DDPINFO("time diff = %lu", diff_mesc);

	if (diff_mesc > LOG_INTERVAL_TH) {
		if (g_aal_log_index == 0) {
			DDPINFO("%s: %d/1023\n", __func__, bl_1024);
		} else {
			sprintf(g_aal_log_buffer + strlen(g_aal_log_buffer),
				"%s, %d/1023 %03lu.%03lu", __func__,
				bl_1024, tsec, tusec);
			DDPINFO("%s\n", g_aal_log_buffer);
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
			DDPINFO("%s\n", g_aal_log_buffer);
			g_aal_log_index = 0;
		}
	}

	memcpy(&g_aal_log_prevtime, &aal_time, sizeof(struct timeval));
}

void disp_aal_notify_backlight_changed(int bl_1024)
{
	unsigned long flags, clockflags;
	int max_backlight = 0;
	unsigned int service_flags;

	DDPINFO("%s: %d/1023", __func__, bl_1024);
	disp_aal_notify_backlight_log(bl_1024);
	//disp_aal_exit_idle(__func__, 1);

	// FIXME
	//max_backlight = disp_pwm_get_max_backlight(DISP_PWM0);
	max_backlight = 1024;
	if (bl_1024 > max_backlight)
		bl_1024 = max_backlight;

	atomic_set(&g_aal_backlight_notified, bl_1024);

	service_flags = 0;
	if (bl_1024 == 0) {
		mt_leds_brightness_set("lcd-backlight", 0);
		/* set backlight = 0 may be not from AAL, */
		/* we have to let AALService can turn on backlight */
		/* on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (atomic_read(&g_aal_is_init_regs_valid) == 0 ||
		atomic_read(&g_aal_force_relay) == 1) {
		/* AAL Service is not running */

		mt_leds_brightness_set("lcd-backlight", bl_1024);
	}

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist.backlight = bl_1024;
	g_aal_hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	if (atomic_read(&g_aal_is_init_regs_valid) == 1) {
		spin_lock_irqsave(&g_aal_irq_en_lock, flags);
		atomic_set(&g_aal_force_enable_irq, 1);

		if (spin_trylock_irqsave(&g_aal_clock_lock, clockflags)) {
			if (atomic_read(&g_aal_data->is_clock_on) != 1)
				DDPPR_ERR("%s: clock is off\n", __func__);
			else
				disp_aal_set_interrupt(NULL, true);
			spin_unlock_irqrestore(&g_aal_clock_lock, clockflags);
		}

		spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);
		/* Backlight latency should be as smaller as possible */
		mtk_crtc_check_trigger(default_comp->mtk_crtc, false);
	}
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_classdev *led_cdev;

	led_cdev = (struct led_classdev *)v;

	switch (event) {
	case 1:
		trans_level = (
			(((1 << 10) - 1) * led_cdev->brightness
			+ ((led_cdev->max_brightness) / 2))
			/ (led_cdev->max_brightness));

		disp_aal_notify_backlight_changed(trans_level);
		DDPINFO("%s: brightness changed: %d, %d\n",
			__func__, led_cdev->brightness, trans_level);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event,
};
#endif

int mtk_drm_ioctl_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	int ret = 0;
	unsigned long flags, clockflags;
	int *enabled = (int *)data;

	DDPINFO("%s: %d\n", __func__, *enabled);

	spin_lock_irqsave(&g_aal_irq_en_lock, flags);
	if (atomic_read(&g_aal_force_enable_irq) == 1) {
		if (*enabled == 0)
			DDPINFO("%s: force enable aal ieq 0 -> 1\n", __func__);
		*enabled = 1;
	}
	if (spin_trylock_irqsave(&g_aal_clock_lock, clockflags)) {
		if (atomic_read(&aal_data->is_clock_on) != 1) {
			DDPPR_ERR("%s: clock is off\n", __func__);
			ret = -EFAULT;
		} else
			disp_aal_set_interrupt(comp, *enabled);
		spin_unlock_irqrestore(&g_aal_clock_lock, clockflags);
	}
	spin_unlock_irqrestore(&g_aal_irq_en_lock, flags);

	if (*enabled)
		mtk_crtc_check_trigger(comp->mtk_crtc, false);

	return ret;
}

static void mtk_aal_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPINFO("%s +\n", __func__);
	if (disp_aal_is_support() == true &&
		atomic_read(&g_aal_force_relay) != 1) {
		DDPINFO("%s Enable AAL histogram\n", __func__);
		// Enable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x3 << 1, (0x3 << 1));
	} else {
		DDPINFO("%s Disable AAL histogram\n", __func__);
		// Disable AAL histogram, engine
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_CFG, 0x0 << 1, (0x3 << 1));
	}

	/* get lcd-backlight mode from dts */
	if (atomic_read(&g_led_mode) == MT65XX_LED_MODE_NONE)
		disp_aal_get_cust_led();

	atomic_set(&g_aal_hist_available, 0);
	atomic_set(&aal_data->dirty_frame_retrieved, 1);
	DDPINFO("%s led mode: %d-\n", __func__, atomic_read(&g_led_mode));
}

static void mtk_aal_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = 0;
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	int dre_alg_mode = 0;
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);
#endif
	DDPINFO("%s +\n", __func__);
	//if (pConfig->dst_dirty)
	{
		int width = cfg->w, height = cfg->h;

#if defined(CONFIG_MTK_DRE30_SUPPORT)
		if (g_aal_get_size_available == false) {
			g_aal_size.height = height;
			g_aal_size.width = width;
			g_aal_get_size_available = true;
			wake_up_interruptible(&g_aal_size_wq);
		}
#endif
		DDPINFO("%s Write SIZE/OUTPUT reg\n", __func__);
		val = (width << 16) | (height);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_SIZE, val, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_SIZE, val, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_AAL_OUTPUT_OFFSET,
			(0 << 16) | 0, ~0);

#if defined(CONFIG_MTK_DRE30_SUPPORT)
		if (atomic_read(&g_aal_change_to_dre30) & 0x1)
			dre_alg_mode = 1;
		cmdq_pkt_write(handle, comp->cmdq_base,
			dre3_pa + DISP_AAL_CFG_MAIN,
			dre_alg_mode << 4, 1 << 4);
#endif

		if (atomic_read(&g_aal_force_relay) == 1) {
			// Set reply mode
			DDPINFO("%s g_aal_force_relay\n", __func__);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CFG, 1, 1);
		} else {
			// Disable reply mode
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_AAL_CFG, 0, 1);
		}
		mtk_aal_init(comp, cfg, handle);

		basic_cmdq_write(handle, comp,
			DISP_AAL_EN, 0x1, 0x1);
		DDPINFO("%s: AAL_CFG=0x%x\n", __func__,
			readl(comp->regs + DISP_AAL_CFG));
	}

/*
 * if ((pConfig->ovl_dirty || pConfig->rdma_dirty))
 *		disp_aal_notify_frame_dirty(module);
 */
}

static void disp_aal_wait_hist(void)
{
	int ret = 0;

	if (atomic_read(&g_aal_hist_available) == 0) {
		DDPINFO("%s wait_event_interruptible\n", __func__);
		ret = wait_event_interruptible(g_aal_hist_wq,
				atomic_read(&g_aal_hist_available) == 1);
		DDPINFO("%s hist_available = 1, waken up, ret = %d",
				__func__, ret);
	} else
		DDPINFO("hist_available = 0");
}

static bool disp_aal_read_single_hist(struct mtk_ddp_comp *comp)
{
	bool read_success = true;
	int i;

	for (i = 0; i < AAL_HIST_BIN; i++) {
		g_aal_hist.maxHist[i] = readl(comp->regs +
				DISP_AAL_STATUS_00 + (i << 2));
	}
	for (i = 0; i < AAL_HIST_BIN; i++) {
		g_aal_hist.yHist[i] = readl(comp->regs +
				DISP_Y_HISTOGRAM_00 + (i << 2));
	}
	read_success = disp_color_reg_get(comp, DISP_COLOR_TWO_D_W1_RESULT,
						&g_aal_hist.colorHist);

	return read_success;
}

static int disp_aal_copy_hist_to_user(struct DISP_AAL_HIST *hist)
{
	unsigned long flags;

	if (hist == NULL) {
		DDPPR_ERR("%s DstHist is NULL\n", __func__);
		return -1;
	}

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_aal_hist_lock, flags);
	if (g_aal_hist.backlight == -1)
		g_aal_hist.backlight = 40;
	memcpy(&g_aal_hist_db, &g_aal_hist, sizeof(g_aal_hist));

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	memcpy(&g_aal_dre30_hist_db, &g_aal_dre30_hist,
		sizeof(g_aal_dre30_hist));
#endif

	g_aal_hist.panel_type = atomic_read(&g_aal_panel_type);
	g_aal_hist.essStrengthIndex = g_aal_ess_level;
	g_aal_hist.ess_enable = g_aal_ess_en;
	g_aal_hist.dre_enable = g_aal_dre_en;

	g_aal_hist.serviceFlags = 0;
	atomic_set(&g_aal_hist_available, 0);
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	do {
#if defined(CONFIG_MTK_DRE30_SUPPORT)
		g_aal_hist_db.dre30_hist = g_aal_init_dre30.dre30_hist_addr;
#endif
		memcpy(hist, &g_aal_hist_db, sizeof(g_aal_hist_db));

#if defined(CONFIG_MTK_DRE30_SUPPORT)
		copy_to_user(AAL_U32_PTR(g_aal_init_dre30.dre30_hist_addr),
			&g_aal_dre30_hist_db, sizeof(g_aal_dre30_hist_db));
#endif
	} while (0);

	DDPINFO("%s set g_aal_force_enable_irq to 0 +\n", __func__);
	atomic_set(&g_aal_force_enable_irq, 0);

	return 0;
}

#ifdef DUMPAAL
void dumpAALHist(struct DISP_AAL_HIST *data)
{
	int i = 0;

	DDPINFO("maxHist:\n");
	for (i = 0; i < 3; i++) {
		DDPINFO("%d %d %d %d %d %d %d %d %d %d",
			data->maxHist[i*10 + 0], data->maxHist[i*10 + 1],
			data->maxHist[i*10 + 2], data->maxHist[i*10 + 3],
			data->maxHist[i*10 + 4], data->maxHist[i*10 + 5],
			data->maxHist[i*10 + 6], data->maxHist[i*10 + 7],
			data->maxHist[i*10 + 9], data->maxHist[i*10 + 9]);
	}
	DDPINFO("%d %d %d", data->maxHist[30], data->maxHist[31],
			data->maxHist[32]);
	DDPINFO("yHist:\n");
	for (i = 0; i < 3; i++) {
		DDPINFO("%d %d %d %d %d %d %d %d %d %d",
			data->yHist[i*10 + 0], data->yHist[i*10 + 1],
			data->yHist[i*10 + 2], data->yHist[i*10 + 3],
			data->yHist[i*10 + 4], data->yHist[i*10 + 5],
			data->yHist[i*10 + 6], data->yHist[i*10 + 7],
			data->yHist[i*10 + 9], data->yHist[i*10 + 9]);
	}
	DDPINFO("%d %d %d", data->yHist[30], data->yHist[31],
			data->yHist[32]);
	DDPINFO("serviceFlags:%u, backlight: %d, colorHist: %d\n",
			data->serviceFlags, data->backlight, data->colorHist);
	DDPINFO("requestPartial:%d, dre30_hist: 0x%x, panel_type: %u\n",
			data->requestPartial, data->dre30_hist,
			data->panel_type);
	DDPINFO("essStrengthIndex:%d, ess_enable: %d, dre_enable: %d\n",
			data->essStrengthIndex, data->ess_enable,
			data->dre_enable);
}
#endif

int mtk_drm_ioctl_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	disp_aal_wait_hist();
	if (disp_aal_copy_hist_to_user((struct DISP_AAL_HIST *) data) < 0)
		return -EFAULT;
#ifdef DUMPAAL
	dumpAALHist(data);
#endif
	return 0;
}

#if defined(CONFIG_MTK_DRE30_SUPPORT)
static void disp_aal_dre3_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	const struct DISP_AAL_INITREG *init_regs)
{
	phys_addr_t dre3_pa = mtk_aal_dre3_pa(comp);

	DDPINFO("%s: start", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_00,
		init_regs->act_win_x_end << 13, 0x1FFF << 13);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_01,
		(init_regs->dre_blk_y_num << 5) | init_regs->dre_blk_x_num,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_02,
		(init_regs->dre_blk_height << 13) | init_regs->dre_blk_width,
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_BLOCK_INFO_04,
		(init_regs->dre_flat_length_slope << 13), 0x3FF << 13);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_00,
		(init_regs->dre_s_upper << 24) |
		(init_regs->dre_s_lower << 16) |
		(init_regs->dre_y_upper << 8) | init_regs->dre_y_lower, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_DRE_CHROMA_HIST_01,
		(init_regs->dre_h_upper << 8) | init_regs->dre_h_lower,
		0xFFFF);
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
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_00, 0x003400e0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_01, 0x000ee000, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + MDP_AAL_TILE_02, 0x001de000, ~0);
#endif
	/* Change to Local DRE version */
	cmdq_pkt_write(handle, comp->cmdq_base,
		dre3_pa + DISP_AAL_CFG_MAIN,
		1 << 4, 1 << 4);

	atomic_or(0x1, &g_aal_change_to_dre30);
}
#endif /* CONFIG_MTK_DRE30_SUPPORT */

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

#if defined(CONFIG_MTK_DRE30_SUPPORT)
		disp_aal_dre3_config(comp, handle, init_regs);
#endif
		DDPINFO("%s: init done", __func__);
		ret = 0;
	}

	return ret;
}

static int disp_aal_set_init_reg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle, struct DISP_AAL_INITREG *user_regs)
{
	int ret = -EFAULT;
	struct DISP_AAL_INITREG *init_regs;

	if (disp_aal_is_support() == false)
		return ret;

	init_regs = &g_aal_init_regs;

	memcpy(init_regs, user_regs, sizeof(*init_regs));

	atomic_set(&g_aal_is_init_regs_valid, 1);

	DDPINFO("Set init reg: %lu", sizeof(*init_regs));
	DDPINFO("init_reg.dre_map_bypass:%d", init_regs->dre_map_bypass);
	ret = disp_aal_write_init_regs(comp, handle);

	DDPINFO("ret = %d", ret);

	return ret;
}

int mtk_drm_ioctl_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_crtc_user_cmd(crtc, comp, INIT_REG, data);
}

static struct DISP_AAL_PARAM g_aal_param;

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | \
	((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) \
	(((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))

#if defined(CONFIG_MTK_DRE30_SUPPORT)
static int disp_aal_write_dre3_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	unsigned long flags;

	DDPINFO("%s\n", __func__);
	if (atomic_read(&g_aal_change_to_dre30) == 0x3) {
		if (copy_from_user(&g_aal_gain_db,
			      AAL_U32_PTR(param->dre30_gain),
			      sizeof(g_aal_gain_db)) == 0) {

			spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
			memcpy(&g_aal_gain, &g_aal_gain_db,
				sizeof(g_aal_gain));
			spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
		}
	}

	return 0;
}
#else
static int disp_aal_write_dre_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	const int *gain;

	gain = param->DREGainFltStatus;
#if defined(CONFIG_MACH_MT6885)
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
#endif

	return 0;
}
#endif /* CONFIG_MTK_DRE30_SUPPORT */
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
static int disp_aal_write_cabc_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;

	DDPINFO("%s\n", __func__);
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
#endif				/* not define NOT_SUPPORT_CABC_HW */

static int disp_aal_write_param_to_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, const struct DISP_AAL_PARAM *param)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	disp_aal_write_dre3_to_reg(comp, handle, param);
	disp_aal_write_cabc_to_reg(comp, handle, param);
#else
	disp_aal_write_dre_to_reg(comp, handle, param);
#ifndef NOT_SUPPORT_CABC_HW
	disp_aal_write_cabc_to_reg(comp, handle, param);
#endif
#endif

	return 0;
}

#ifdef DUMPAAL
void dumpAALParam(const struct DISP_AAL_PARAM *param)
{
	int i = 0;

	DDPINFO("DREGainFltStatus: ");
	for (i = 0; i < 2; i++) {
		DDPINFO("%d %d %d %d %d %d %d %d %d %d",
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
	DDPINFO("%d %d %d %d %d %d %d %d %d",
		param->DREGainFltStatus[20], param->DREGainFltStatus[21],
		param->DREGainFltStatus[22], param->DREGainFltStatus[23],
		param->DREGainFltStatus[24], param->DREGainFltStatus[25],
		param->DREGainFltStatus[26], param->DREGainFltStatus[27],
		param->DREGainFltStatus[28]);

	DDPINFO("cabc_gainlmt: ");
	for (i = 0; i < 3; i++) {
		DDPINFO("%d %d %d %d %d %d %d %d %d %d",
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
	DDPINFO("%d %d %d",
		param->cabc_gainlmt[30], param->cabc_gainlmt[31],
		param->cabc_gainlmt[32]);

	DDPINFO("cabc_fltgain_force: %d, FinalBacklight: %d",
		param->cabc_fltgain_force, param->FinalBacklight);
	DDPINFO("allowPartial: %d, refreshLatency: %d",
		param->allowPartial, param->refreshLatency);
}
#endif

int disp_aal_set_param(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		struct DISP_AAL_PARAM *param)
{
	int ret = -EFAULT;

#ifdef DUMPAAL
	dumpAALParam(&g_aal_param);
#endif
	ret = disp_aal_write_param_to_reg(comp, handle, &g_aal_param);

/* FIXME
 *	if (ret == 0)
 *		ret |= disp_pwm_set_backlight_cmdq(DISP_PWM0,
 *			backlight_value, cmdq);
 */
// FIXME

	return ret;
}

#ifdef DUMPAAL
#define PRINT_AAL_REG(x1, x2, x3, x4) \
	pr_notice("0x%x=0x%x 0x%x=0x%x 0x%x=0x%x 0x%x=0x%x", \
		x1, readl(comp->regs + x1), x2, readl(comp->regs + x2), \
		x3, readl(comp->regs + x3), x4, readl(comp->regs + x4))
void dumpAALReg(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
		if (atomic_read(&aal_data->is_clock_on)) {
			PRINT_AAL_REG(0x0, 0x8, 0x10, 0x20);
			PRINT_AAL_REG(0x30, 0xFC, 0x160, 0x200);
			PRINT_AAL_REG(0x204, 0x20C, 0x3B4, 0x45C);
			PRINT_AAL_REG(0x460, 0x464, 0x468, 0x4D8);
			PRINT_AAL_REG(0x4DC, 0x500, 0x224, 0x504);
		}
		spin_unlock_irqrestore(&g_aal_clock_lock, flags);
	}
}
#endif

int mtk_drm_ioctl_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_AAL0];
	struct drm_crtc *crtc = private->crtc[0];
	int backlight_value = 0;
	struct DISP_AAL_PARAM *param = (struct DISP_AAL_PARAM *) data;

	/* Not need to protect g_aal_param, */
	/* since only AALService can set AAL parameters. */
	memcpy(&g_aal_param, param, sizeof(*param));
	backlight_value = g_aal_param.FinalBacklight;
	/* set cabc gain zero when detect backlight */
	/* setting equal to zero */
	if (backlight_value == 0)
		g_aal_param.cabc_fltgain_force = 0;

	ret = mtk_crtc_user_cmd(crtc, comp, SET_PARAM, data);

	atomic_set(&g_aal_allowPartial, g_aal_param.allowPartial);

	if (atomic_read(&g_aal_backlight_notified) == 0)
		backlight_value = 0;

	mt_leds_brightness_set("lcd-backlight", backlight_value);
	mtk_crtc_check_trigger(comp->mtk_crtc, false);
#ifdef DUMPAAL
	dumpAALReg(comp);
#endif
	return ret;
}

static int mtk_aal_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case INIT_REG:
		if (disp_aal_set_init_reg(comp, handle,
			(struct DISP_AAL_INITREG *) data) < 0) {
			DDPPR_ERR("%s INIT_REG: fail\n", __func__);
			return -EFAULT;
		}
		break;
	case SET_PARAM:
		if (disp_aal_set_param(comp, handle,
			(struct DISP_AAL_PARAM *) data) < 0) {
			DDPPR_ERR("%s SET_PARAM: fail\n", __func__);
			return -EFAULT;
		}
		break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
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
				DDPPR_ERR("%s: clock is off\n",
					__func__);
			else
				disp_aal_set_interrupt(comp, false);
			spin_unlock_irqrestore(&g_aal_clock_lock,
					flags);
		}
	}

	DDPINFO("AAL Module, process:(%d)", cleared);
}

#if defined(CONFIG_MTK_DRE30_SUPPORT)
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

static inline bool disp_aal_sram_write(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int value)
{
	bool return_value = false;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	do {
		writel(addr, dre3_va + DISP_AAL_SRAM_RW_IF_0);

		if (disp_aal_reg_poll(comp, DISP_AAL_SRAM_STATUS,
				(0x1 << 16), (0x1 << 16)) != true)
			break;

		writel(value, dre3_va + DISP_AAL_SRAM_RW_IF_1);

		return_value = true;
	} while (0);

	return return_value;
}

static inline bool disp_aal_sram_read(struct mtk_ddp_comp *comp,
	unsigned int addr, unsigned int *value)
{
	bool return_value = false;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	do {
		writel(addr, dre3_va + DISP_AAL_SRAM_RW_IF_2);

		if (disp_aal_reg_poll(comp, DISP_AAL_SRAM_STATUS,
				(0x1 << 17), (0x1 << 17)) != true)
			break;

		*value = readl(dre3_va + DISP_AAL_SRAM_RW_IF_3);

		return_value = true;
	} while (0);

	return return_value;
}

static bool disp_aal_read_dre3(struct mtk_ddp_comp *comp,
	const int dre_blk_x_num, const int dre_blk_y_num)
{
	int hist_offset;
	int arry_offset = 0;
	unsigned int read_value;

	/* Read Global histogram for ESS */
	if (disp_aal_read_single_hist(comp) != true)
		return false;

	/* Read Local histogram for DRE 3 */
	for (hist_offset = AAL_DRE_HIST_START; hist_offset <= AAL_DRE_HIST_END;
		hist_offset += 4) {
		if (disp_aal_sram_read(comp, hist_offset, &read_value) != true)
			return false;

		if (arry_offset >= AAL_DRE30_HIST_REGISTER_NUM)
			return false;
		g_aal_dre30_hist.dre_hist[arry_offset++] = read_value;
	}

	return true;
}

static bool disp_aal_write_dre3(struct mtk_ddp_comp *comp)
{
	int gain_offset;
	int arry_offset = 0;
	unsigned int write_value;

	/* Write Local Gain Curve for DRE 3 */
	DDPINFO("%s start\n", __func__);
	for (gain_offset = AAL_DRE_GAIN_START; gain_offset <= AAL_DRE_GAIN_END;
		gain_offset += 4) {
		if (arry_offset >= AAL_DRE30_GAIN_REGISTER_NUM)
			return false;
		write_value = g_aal_gain.dre30_gain[arry_offset++];

		if (!disp_aal_sram_write(comp, gain_offset, write_value))
			return false;
	}

	return true;
}

static void disp_aal_dre3_irq_handle(struct mtk_ddp_comp *comp)
{
	bool read_success = false;
	unsigned long flags;
	int dre_blk_x_num, dre_blk_y_num;
	unsigned int read_value;
	int hist_apb, hist_int;
	void __iomem *dre3_va = mtk_aal_dre3_va(comp);

	/* Only process AAL0 in single module state */
	disp_aal_clear_irq(comp, false);

	if (atomic_read(&g_aal_change_to_dre30) != 0x3)
		return;

	if (atomic_read(&g_aal_dre_halt) == 0) {
		if (atomic_cmpxchg(&g_aal_force_hist_apb, 0, 1) == 0) {
			hist_apb = 0;
			hist_int = 1;
		} else if (atomic_cmpxchg(&g_aal_force_hist_apb, 1, 0) == 1) {
			hist_apb = 1;
			hist_int = 0;
		} else {
			DDPPR_ERR("Error when get hist_apb irq_handler");
			return;
		}
		DDPINFO("%s: hist_apb (%d), hist_int (%d) in irq_handler",
			__func__, hist_apb, hist_int);

		mtk_aal_write_mask(dre3_va + DISP_AAL_SRAM_CFG,
			(hist_int << 6)|(hist_apb << 5)|(1 << 4), (0x7 << 4));
		atomic_set(&g_aal_dre_halt, 1);

		read_value = readl(dre3_va + DISP_AAL_DRE_BLOCK_INFO_01);
		dre_blk_x_num = aal_min(AAL_DRE_BLK_NUM,
			read_value & 0x1F);
		dre_blk_y_num =
			aal_min(AAL_BLK_MAX_ALLOWED_NUM/dre_blk_x_num,
				(read_value >> 5) & 0x1F);

		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			read_success = disp_aal_read_dre3(comp,
				dre_blk_x_num, dre_blk_y_num);
			if (read_success == true) {
				g_aal_dre30_hist.dre_blk_x_num =
					dre_blk_x_num;
				g_aal_dre30_hist.dre_blk_y_num =
					dre_blk_y_num;
				atomic_set(&g_aal_hist_available, 1);
			}
			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			if (read_success == true)
				wake_up_interruptible(&g_aal_hist_wq);
		}

		if (spin_trylock_irqsave(&g_aal_dre3_gain_lock,
			flags)) {
			/* Write DRE 3.0 gain */
			disp_aal_write_dre3(comp);
			spin_unlock_irqrestore(&g_aal_dre3_gain_lock,
				flags);
		}

		atomic_set(&g_aal_dre_halt, 0);
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

	DDPINFO("%s start\n", __func__);
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
#else
static void disp_aal_single_pipe_hist_update(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	bool read_success = false;

	do {
		intsta = readl(comp->regs + DISP_AAL_INTSTA);
		DDPINFO("%s: AAL Module, intsta: 0x%x", __func__, intsta);
		DDPINFO("%s: AAL_CFG=0x%x\n", __func__,
			readl(comp->regs + DISP_AAL_CFG));
		/* Only process end of frame state */
		if ((intsta & 0x2) == 0x0) {
			DDPPR_ERR("%s: break", __func__);
			break;
		}

		if (spin_trylock_irqsave(&g_aal_get_irq_lock, flags)) {
			writel(intsta & ~0x3, comp->regs + DISP_AAL_INTSTA);
			spin_unlock_irqrestore(&g_aal_get_irq_lock, flags);
		}

		/* Allow to disable interrupt */
		DDPINFO("%s: set dirty_frame_retrieved to 1", __func__);
		atomic_set(&aal_data->dirty_frame_retrieved, 1);

		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			read_success = disp_aal_read_single_hist(comp);

			if (read_success == true)
				atomic_set(&g_aal_hist_available, 1);

			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			DDPINFO("%s: wake_up_interruptible g_aal_hist_wq: %d",
					__func__, read_success);
			if (read_success == true)
				wake_up_interruptible(&g_aal_hist_wq);
		} else {
			/*
			 * Histogram was not be retrieved, but it's OK.
			 * Another interrupt will come until histogram available
			 * See: disp_aal_set_interrupt()
			 */
		}

		if (atomic_read(&g_aal_is_init_regs_valid) == 0) {
			/*
			 * AAL service is not running, not need per-frame wakeup
			 * We stop interrupt until next frame dirty.
			 */
			DDPINFO("%s: set disp_aal_set_interrupt to 0",
					__func__);
			if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
				if (atomic_read(&aal_data->is_clock_on) != 1) {
					DDPPR_ERR("%s: clock is off\n",
						__func__);
				} else
					disp_aal_set_interrupt(comp, false);
				spin_unlock_irqrestore(&g_aal_clock_lock,
					flags);
			}
		}
	} while (0);
}

#endif /* CONFIG_MTK_DRE30_SUPPORT */

int mtk_drm_ioctl_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	DDPINFO("%s\n", __func__);
	disp_aal_set_init_dre30((struct DISP_DRE30_INIT *) data);
#else
	DDPINFO("%s DRE30 not support\n", __func__);
#endif
	return 0;
}

#if defined(CONFIG_MTK_DRE30_SUPPORT)
static int disp_aal_wait_size(unsigned long timeout)
{
	int ret = 0;

	if (g_aal_get_size_available == false) {
		ret = wait_event_interruptible(g_aal_size_wq,
		g_aal_get_size_available == true);
		DDPINFO("size_available = 1, Waken up, ret = %d\n",
			ret);
	} else {
		/* If g_aal_get_size_available is already set, */
		/* means AALService was delayed */
		DDPINFO("size_available = 0\n");
	}
	return ret;
}
#endif

int mtk_drm_ioctl_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	struct DISP_AAL_DISPLAY_SIZE *dst =
		(struct DISP_AAL_DISPLAY_SIZE *)data;

	DDPINFO("%s\n", __func__);
	disp_aal_wait_size(60);
	memcpy(dst, &g_aal_size, sizeof(g_aal_size));
#else
	DDPINFO("%s DRE30 not support\n", __func__);
#endif
	return 0;
}

static void mtk_aal_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x1, ~0);
}

static void mtk_aal_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	basic_cmdq_write(handle, comp, DISP_AAL_EN, 0x0, ~0);
}

static void mtk_aal_bypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
#if 1
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_AAL_CFG,
		0x1, 0x1);
#else
	DDPINFO("%s is ignored\n", __func__);
#endif
}

#define DRE_FLT_NUM	(11)
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
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
#endif
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
	g_aal_backup.SRAM_CFG =
		readl(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG);
	/* FIXME */
	g_aal_backup.DUAL_PIPE_INFO_00 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00);
	g_aal_backup.DUAL_PIPE_INFO_01 =
		readl(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01);
#endif	/* CONFIG_MTK_DRE30_SUPPORT */
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
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
	int i;

	g_aal_backup.CABC_00 = readl(comp->regs + DISP_AAL_CABC_00);
	g_aal_backup.CABC_02 = readl(comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		g_aal_backup.CABC_GAINLMT[i] =
		    readl(comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_backup(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	ddp_aal_cabc_backup(comp);
	ddp_aal_dre_backup(comp);
	ddp_aal_dre3_backup(comp);
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
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_SRAM_CFG,
		g_aal_backup.SRAM_CFG, 0x1);

	/* FIXME */
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_00,
		g_aal_backup.DUAL_PIPE_INFO_00, ~0);
	mtk_aal_write_mask(aal_data->dre3_hw.va + DISP_AAL_DUAL_PIPE_INFO_01,
		g_aal_backup.DUAL_PIPE_INFO_01, ~0);
#endif

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	unsigned long flags;

	spin_lock_irqsave(&g_aal_dre3_gain_lock, flags);
	ddp_aal_dre3_write_curve_full(comp);
	spin_unlock_irqrestore(&g_aal_dre3_gain_lock, flags);
#endif	/* CONFIG_MTK_DRE30_SUPPORT */
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
#if defined(CONFIG_MTK_DRE30_SUPPORT) || !defined(NOT_SUPPORT_CABC_HW)
	int i;

	writel(g_aal_backup.CABC_00, comp->regs + DISP_AAL_CABC_00);
	writel(g_aal_backup.CABC_02, comp->regs + DISP_AAL_CABC_02);

	for (i = 0; i < CABC_GAINLMT_NUM; i++)
		writel(g_aal_backup.CABC_GAINLMT[i],
			comp->regs + DISP_AAL_CABC_GAINLMT_TBL(i));
#endif	/* not define NOT_SUPPORT_CABC_HW */
}

static void ddp_aal_restore(struct mtk_ddp_comp *comp)
{
	if (atomic_read(&g_aal_initialed) != 1)
		return;

	DDPINFO("%s\n", __func__);
	ddp_aal_cabc_restore(comp);
	ddp_aal_dre_restore(comp);
	ddp_aal_dre3_restore(comp);
}

static void mtk_aal_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&aal_data->is_clock_on, 1);

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	if (aal_data->dre3_hw.clk)
		clk_prepare(aal_data->dre3_hw.clk);
	if (atomic_cmpxchg(&g_aal_dre_hw_init, 0, 1) == 0)
		disp_aal_dre3_init(comp);
#endif
	ddp_aal_restore(comp);
}

static void mtk_aal_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);
	unsigned long flags;

	DDPINFO("%s\n", __func__);
	spin_lock_irqsave(&g_aal_clock_lock, flags);
	atomic_set(&aal_data->is_clock_on, 0);
	spin_unlock_irqrestore(&g_aal_clock_lock, flags);
	ddp_aal_backup(comp);
	//disp_aal_clear_irq(comp, true);
	mtk_ddp_comp_clk_unprepare(comp);
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	if (aal_data->dre3_hw.clk)
		clk_unprepare(aal_data->dre3_hw.clk);
#endif
}

void mtk_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	mtk_aal_config(comp, cfg, handle);
}

int mtk_aal_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	      enum mtk_ddp_io_cmd cmd, void *params)
{
	unsigned long flags;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (cmd == FRAME_DIRTY) {
		DDPINFO("%s: FRAME_DIRTY\n", __func__);
		if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
			if (atomic_read(&aal_data->is_clock_on) != 1)
				DDPINFO("%s: clock is off\n", __func__);
			else
				disp_aal_set_interrupt(comp, true);
			spin_unlock_irqrestore(&g_aal_clock_lock, flags);
		}
	}
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

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, 0x200, 0x10);
}

void disp_aal_on_end_of_frame(struct mtk_ddp_comp *comp)
{

	if (atomic_read(&g_aal_force_relay) == 1) {
		disp_aal_clear_irq(comp, true);
		return;
	}

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	disp_aal_dre3_irq_handle(comp);
#else
	disp_aal_single_pipe_hist_update(comp);
#endif	/* CONFIG_MTK_DRE30_SUPPORT */
}

static irqreturn_t mtk_disp_aal_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	struct mtk_disp_aal *priv = dev_id;
	struct mtk_ddp_comp *comp = &priv->ddp_comp;
	struct mtk_disp_aal *aal_data = comp_to_aal(comp);

	if (spin_trylock_irqsave(&g_aal_clock_lock, flags)) {
		if (atomic_read(&aal_data->is_clock_on) != 1)
			DDPPR_ERR("%s: clock is off\n", __func__);
		else {
			disp_aal_on_end_of_frame(comp);
			ret = IRQ_HANDLED;
		}
		spin_unlock_irqrestore(&g_aal_clock_lock, flags);
	}

	return ret;
}

static int mtk_disp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_aal *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret, irq;
#if defined(CONFIG_MTK_DRE30_SUPPORT)
	struct device_node *dre3_dev_node;
	struct platform_device *dre3_pdev;
	struct resource dre3_res;
#endif

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	g_aal_data = priv;
	atomic_set(&priv->dirty_frame_retrieved, 1);
	atomic_set(&priv->is_clock_on, 0);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_AAL);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_aal_funcs);
	if (ret) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}
	if (!default_comp)
		default_comp = &priv->ddp_comp;

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_disp_aal_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	pm_runtime_enable(dev);

#if defined(CONFIG_MTK_DRE30_SUPPORT)
	do {
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
			DDPMSG("fail to allocate dre3 va!\n");
			break;
		}
		ret = of_address_to_resource(
			dre3_pdev->dev.of_node, 0, &dre3_res);
		if (ret) {
			DDPMSG("fail to allocate dre3 resource!\n");
			break;
		}
		priv->dre3_hw.pa = dre3_res.start;
		priv->dre3_hw.clk = of_clk_get_by_name(
			dre3_dev_node, "DRE3_AAL0");
		if (IS_ERR(priv->dre3_hw.clk)) {
			DDPMSG("fail @ dre3 clock. name:%s\n",
				"DRE3_AAL0");
			break;
		}
		pr_notice("dre3 dev:%p va:%p pa:%pa", priv->dre3_hw.dev,
			priv->dre3_hw.va, &priv->dre3_hw.pa);
	} while (0);
#endif

	ret = component_add(dev, &mtk_disp_aal_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_register_notifier(&leds_init_notifier);
#endif


	DDPINFO("%s -\n", __func__);
	return ret;
}

static int mtk_disp_aal_remove(struct platform_device *pdev)
{
	struct mtk_disp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_aal_component_ops);
	pm_runtime_disable(&pdev->dev);

	if (priv->dre3_hw.dev)
		pm_runtime_disable(priv->dre3_hw.dev);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	mtk_leds_unregister_notifier(&leds_init_notifier);
#endif

	return 0;
}

static const struct of_device_id mtk_disp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-disp-aal",},
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

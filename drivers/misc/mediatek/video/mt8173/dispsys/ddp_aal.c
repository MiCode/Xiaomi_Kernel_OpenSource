/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
/*#include <linux/leds-mt65xx.h> */
#include <linux/string.h>
#include "cmdq_record.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include "ddp_path.h"
#include "ddp_gamma.h"
#include "ddp_log.h"
#include <primary_display.h>
#include "leds_drv.h"


/* To enable debug log: */
/* # echo aal_dbg:1 > /sys/kernel/debug/dispsys */
int aal_dbg_en = 0;

#define AAL_ERR(fmt, arg...) DDPERR("[AAL] " fmt "\n", ##arg)
#define AAL_DBG(fmt, arg...) \
	do { if (aal_dbg_en) DDPDBG("[AAL] " fmt "\n", ##arg); } while (0)

static int disp_aal_write_param_to_reg(cmdqRecHandle cmdq, const DISP_AAL_PARAM *param);

static DECLARE_WAIT_QUEUE_HEAD(g_aal_hist_wq);
static DEFINE_SPINLOCK(g_aal_hist_lock);
static DISP_AAL_HIST g_aal_hist = {
	.serviceFlags = 0,
	.backlight = -1,
	.fps = 60
};

static DISP_AAL_HIST g_aal_hist_db;
static ddp_module_notify g_ddp_notify;
static volatile int g_aal_hist_available;
static volatile int g_aal_dirty_frame_retrieved = 1;
static volatile int g_aal_is_init_regs_valid;

static int disp_aal_init(DISP_MODULE_ENUM module, int width, int height, void *cmdq)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	/* Enable AAL histogram, engine */
	DISP_REG_MASK(cmdq, DISP_AAL_CFG, 0x3 << 8, (0x3 << 8) | 0x1);

	disp_aal_write_init_regs(cmdq);
#endif

	g_aal_hist_available = 0;
	g_aal_dirty_frame_retrieved = 1;

	return 0;
}


static void disp_aal_trigger_refresh(void)
{
	if (g_ddp_notify != NULL)
		g_ddp_notify(DISP_MODULE_AAL, DISP_PATH_EVENT_TRIGGER);
}


static void disp_aal_set_interrupt(int enabled)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	if (enabled) {
		if (DISP_REG_GET(DISP_AAL_EN) == 0)
			AAL_DBG("[WARNING] DISP_AAL_EN not enabled!");

		/* Enable output frame end interrupt */
		DISP_CPU_REG_SET(DISP_AAL_INTEN, 0x2);
		AAL_DBG("Interrupt enabled");
	} else {
		if (g_aal_dirty_frame_retrieved) {
			DISP_CPU_REG_SET(DISP_AAL_INTEN, 0x0);
			AAL_DBG("Interrupt disabled");
		} else {	/* Dirty histogram was not retrieved */
			/* Only if the dirty hist was retrieved, interrupt can be disabled.
			   Continue interrupt until AALService can get the latest histogram. */
		}
	}

#else
	AAL_ERR("AAL driver is not enabled");
#endif
}


static void disp_aal_notify_frame_dirty(void)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	unsigned long flags;

	AAL_DBG("disp_aal_notify_frame_dirty()");

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	/* Interrupt can be disabled until dirty histogram is retrieved */
	g_aal_dirty_frame_retrieved = 0;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	disp_aal_set_interrupt(1);
#endif
}


static int disp_aal_wait_hist(unsigned long timeout)
{
	int ret = 0;

	AAL_DBG("disp_aal_wait_hist: available = %d", g_aal_hist_available);

	if (!g_aal_hist_available) {
		ret = wait_event_interruptible(g_aal_hist_wq, (g_aal_hist_available != 0));
		AAL_DBG("disp_aal_wait_hist: waken up, ret = %d", ret);
	} else {
		/* If g_aal_hist_available is already set, means AALService was delayed */
	}

	return ret;
}


void disp_aal_on_end_of_frame(void)
{
#ifdef CONFIG_MTK_AAL_SUPPORT
	unsigned int intsta;
	int i;
	unsigned long flags;

	intsta = DISP_REG_GET(DISP_AAL_INTSTA);

	AAL_DBG("disp_aal_on_end_of_frame: intsta: 0x%x", intsta);
	if (intsta & 0x2) {	/* End of frame */
		if (spin_trylock_irqsave(&g_aal_hist_lock, flags)) {
			DISP_CPU_REG_SET(DISP_AAL_INTSTA, (intsta & ~0x3));

			for (i = 0; i < AAL_HIST_BIN; i++)
				g_aal_hist.maxHist[i] = DISP_REG_GET(DISP_AAL_STATUS_00 + (i << 2));
			g_aal_hist_available = 1;

			/* Allow to disable interrupt */
			g_aal_dirty_frame_retrieved = 1;

			spin_unlock_irqrestore(&g_aal_hist_lock, flags);

			if (!g_aal_is_init_regs_valid) {
				/*
				 * AAL service is not running, not need per-frame wakeup.
				 * We stop interrupt until next frame dirty.
				 */
				disp_aal_set_interrupt(0);
			}

			wake_up_interruptible(&g_aal_hist_wq);
		} else {
			/*
			 * Histogram was not be retrieved, but it's OK.
			 * Another interrupt will come until histogram available
			 * See: disp_aal_set_interrupt()
			 */
		}
	}
#else
	/*
	 * We will not wake up AAL unless signals
	 */
#endif
}


void disp_aal_notify_backlight_changed(int bl_1024)
{
	unsigned long flags;
	int max_backlight;
	unsigned int service_flags;

	DDPDBG("disp_aal_notify_backlight_changed: %d/1023", bl_1024);

	max_backlight = disp_pwm_get_max_backlight(DISP_PWM0);
	if (bl_1024 > max_backlight)
		bl_1024 = max_backlight;

	service_flags = 0;
	if (bl_1024 == 0) {
		backlight_brightness_set(0);
		/* set backlight = 0 may be not from AAL, we have to let AALService
		   can turn on backlight on phone resumption */
		service_flags = AAL_SERVICE_FORCE_UPDATE;
	} else if (!g_aal_is_init_regs_valid) {
		/* AAL Service is not running */
		DDPDBG("aal service is not running\n");
		backlight_brightness_set(bl_1024);
	}

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	g_aal_hist.backlight = bl_1024;
	g_aal_hist.serviceFlags |= service_flags;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);

	if (g_aal_is_init_regs_valid) {
		disp_aal_set_interrupt(1);
		disp_aal_trigger_refresh();
	}
}


static int disp_aal_copy_hist_to_user(DISP_AAL_HIST __user *hist)
{
	unsigned long flags;
	int ret = -EFAULT;

	/* We assume only one thread will call this function */

	spin_lock_irqsave(&g_aal_hist_lock, flags);
	memcpy(&g_aal_hist_db, &g_aal_hist, sizeof(DISP_AAL_HIST));
	g_aal_hist.serviceFlags = 0;
	g_aal_hist_available = 0;
	spin_unlock_irqrestore(&g_aal_hist_lock, flags);
#ifdef MTK_120HZ_SUPPORT
	g_aal_hist.fps = primary_display_get_lcm_refresh_rate();
#endif
	if (copy_to_user(hist, &g_aal_hist_db, sizeof(DISP_AAL_HIST)) == 0)
		ret = 0;

	AAL_DBG("disp_aal_copy_hist_to_user: %d", ret);

	return ret;
}


#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20) | ((v1) << 10) | (v0))


static DISP_AAL_INITREG g_aal_init_regs;
static DISP_AAL_PARAM g_aal_param;


static int disp_aal_set_init_reg(DISP_AAL_INITREG __user *user_regs, void *cmdq)
{
	int ret = -EFAULT;
#ifdef CONFIG_MTK_AAL_SUPPORT
	DISP_AAL_INITREG *init_regs;

	init_regs = &g_aal_init_regs;

	ret = copy_from_user(init_regs, user_regs, sizeof(DISP_AAL_INITREG));
	if (ret == 0) {
		g_aal_is_init_regs_valid = 1;
		ret = disp_aal_write_init_regs(cmdq);
	} else {
		AAL_ERR("disp_aal_set_init_reg: copy_from_user() failed");
	}

	AAL_DBG("disp_aal_set_init_reg: %d", ret);
#else
	AAL_ERR("disp_aal_set_init_reg: AAL not supported");
#endif

	return ret;
}


int disp_aal_write_init_regs(void *cmdq)
{
	int ret = -EFAULT;

	if (g_aal_is_init_regs_valid) {
		DISP_AAL_INITREG *init_regs = &g_aal_init_regs;

		int i, j;
		int *gain;

		DISP_REG_MASK(cmdq, DISP_AAL_DRE_MAPPING_00, (init_regs->dre_map_bypass << 4),
			      1 << 4);

		gain = init_regs->cabc_gainlmt;
		j = 0;
		for (i = 0; i <= 10; i++) {
			DISP_REG_SET(cmdq, DISP_AAL_CABC_GAINLMT_TBL(i),
				     CABC_GAINLMT(gain[j], gain[j + 1], gain[j + 2]));
			j += 3;
		}

		AAL_DBG("disp_aal_write_init_regs: done");
		ret = 0;
	}

	return ret;
}


int disp_aal_set_param(DISP_AAL_PARAM __user *param, void *cmdq)
{
	int ret = -EFAULT;
	int backlight_value = 0;

	/* Not need to protect g_aal_param, since only AALService
	   can set AAL parameters. */
	if (copy_from_user(&g_aal_param, param, sizeof(DISP_AAL_PARAM)) == 0) {
		backlight_value = g_aal_param.FinalBacklight;
#ifdef CONFIG_MTK_AAL_SUPPORT
		/* set cabc gain zero when detect backlight setting equal to zero */
		if (backlight_value == 0)
			g_aal_param.cabc_fltgain_force = 0;
#endif
		ret = disp_aal_write_param_to_reg(cmdq, &g_aal_param);
	}

	if (ret == 0)
		ret |= disp_pwm_set_backlight_cmdq(DISP_PWM0, backlight_value, cmdq);

	AAL_DBG("disp_aal_set_param(CABC = %d, DRE[0,8] = %d,%d): ret = %d",
		g_aal_param.cabc_fltgain_force, g_aal_param.DREGainFltStatus[0],
		g_aal_param.DREGainFltStatus[8], ret);

	backlight_brightness_set(backlight_value);

	disp_aal_trigger_refresh();

	return ret;
}


#define DRE_REG_2(v0, off0, v1, off1)           (((v1) << (off1)) | ((v0) << (off0)))
#define DRE_REG_3(v0, off0, v1, off1, v2, off2) (((v2) << (off2)) | (v1 << (off1)) | ((v0) << (off0)))

static int disp_aal_write_param_to_reg(cmdqRecHandle cmdq, const DISP_AAL_PARAM *param)
{
	int i;
	const int *gain;

	gain = param->DREGainFltStatus;
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_GAIN_FILTER_00, 1 << 8, 1 << 8);	/* dre_gain_force_en */
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(0), DRE_REG_2(gain[0], 0, gain[1], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(1), DRE_REG_2(gain[2], 0, gain[3], 12), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(2), DRE_REG_2(gain[4], 0, gain[5], 11), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(3),
		      DRE_REG_3(gain[6], 0, gain[7], 11, gain[8], 21), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(4),
		      DRE_REG_3(gain[9], 0, gain[10], 10, gain[11], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(5),
		      DRE_REG_3(gain[12], 0, gain[13], 10, gain[14], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(6),
		      DRE_REG_3(gain[15], 0, gain[16], 10, gain[17], 20), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(7),
		      DRE_REG_3(gain[18], 0, gain[19], 9, gain[20], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(8),
		      DRE_REG_3(gain[21], 0, gain[22], 9, gain[23], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(9),
		      DRE_REG_3(gain[24], 0, gain[25], 9, gain[26], 18), ~0);
	DISP_REG_MASK(cmdq, DISP_AAL_DRE_FLT_FORCE(10), DRE_REG_2(gain[27], 0, gain[28], 9), ~0);

	DISP_REG_MASK(cmdq, DISP_AAL_CABC_00, 1 << 31, 1 << 31);
	DISP_REG_MASK(cmdq, DISP_AAL_CABC_02, ((1 << 26) | param->cabc_fltgain_force),
		      ((1 << 26) | 0x3ff));

	gain = param->cabc_gainlmt;
	for (i = 0; i <= 10; i++) {
		DISP_REG_MASK(cmdq, DISP_AAL_CABC_GAINLMT_TBL(i),
			      CABC_GAINLMT(gain[0], gain[1], gain[2]), ~0);
		gain += 3;
	}

	return 0;
}


static int aal_config(DISP_MODULE_ENUM module, disp_ddp_path_config *pConfig, void *cmdq)
{
	if (pConfig->dst_dirty) {
		int width, height;

		width = pConfig->dst_w;
		height = pConfig->dst_h;

		DISP_REG_SET(cmdq, DISP_AAL_SIZE, (width << 16) | height);
		DISP_REG_MASK(cmdq, DISP_AAL_CFG, 0x0, 0x1);	/* Disable relay mode */

		disp_aal_init(module, width, height, cmdq);

		DISP_REG_MASK(cmdq, DISP_AAL_EN, 0x1, 0x1);

		disp_gamma_init(DISP_GAMMA0, width, height, cmdq);

		AAL_DBG("AAL_CFG = 0x%x, AAL_SIZE = 0x%x(%d, %d)",
			DISP_REG_GET(DISP_AAL_CFG), DISP_REG_GET(DISP_AAL_SIZE), width, height);
	}

	if (pConfig->ovl_dirty || pConfig->rdma_dirty)
		disp_aal_notify_frame_dirty();

	return 0;
}


static int aal_clock_on(DISP_MODULE_ENUM module, void *cmq_handle)
{
	ddp_module_clock_enable(MM_CLK_DISP_AAL, true);
	AAL_DBG("aal_clock_on CG 0x%x", DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	return 0;
}

static int aal_clock_off(DISP_MODULE_ENUM module, void *cmq_handle)
{
	AAL_DBG("aal_clock_off");
	ddp_module_clock_enable(MM_CLK_DISP_AAL, false);
	return 0;
}

static int aal_init(DISP_MODULE_ENUM module, void *cmq_handle)
{
	aal_clock_on(module, cmq_handle);
	return 0;
}

static int aal_deinit(DISP_MODULE_ENUM module, void *cmq_handle)
{
	aal_clock_off(module, cmq_handle);
	return 0;
}

static int aal_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
	g_ddp_notify = notify;
	return 0;
}

int aal_bypass(DISP_MODULE_ENUM module, int bypass)
{
	int relay = 0;

	if (bypass)
		relay = 1;

	DISP_REG_MASK(NULL, DISP_AAL_CFG, relay, 0x1);

	AAL_DBG("aal_bypass(bypass = %d)", bypass);

	return 0;
}

static int aal_io(DISP_MODULE_ENUM module, int msg, unsigned long arg, void *cmdq)
{
	int ret = 0;

	switch (msg) {
	case DISP_IOCTL_AAL_EVENTCTL:
		{
			int enabled;

			if (copy_from_user(&enabled, (void *)arg, sizeof(enabled))) {
				AAL_ERR("DISP_IOCTL_AAL_EVENTCTL: copy_from_user() failed");
				return -EFAULT;
			}

			disp_aal_set_interrupt(enabled);

			if (enabled)
				disp_aal_trigger_refresh();

			break;
		}
	case DISP_IOCTL_AAL_GET_HIST:
		{
			disp_aal_wait_hist(60);

			if (disp_aal_copy_hist_to_user((DISP_AAL_HIST *) arg) < 0) {
				AAL_ERR("DISP_IOCTL_AAL_GET_HIST: copy_to_user() failed");
				return -EFAULT;
			}
			break;
		}
	case DISP_IOCTL_AAL_INIT_REG:
		{
			if (disp_aal_set_init_reg((DISP_AAL_INITREG *) arg, cmdq) < 0) {
				AAL_ERR("DISP_IOCTL_AAL_INIT_REG: failed");
				return -EFAULT;
			}
			break;
		}
	case DISP_IOCTL_AAL_SET_PARAM:
		{
			if (disp_aal_set_param((DISP_AAL_PARAM *) arg, cmdq) < 0) {
				AAL_ERR("DISP_IOCTL_AAL_SET_PARAM: failed");
				return -EFAULT;
			}
			break;
		}
	}

	return ret;
}


DDP_MODULE_DRIVER ddp_driver_aal = {
	.init = aal_init,
	.deinit = aal_deinit,
	.config = aal_config,
	.start = NULL,
	.trigger = NULL,
	.stop = NULL,
	.reset = NULL,
	.power_on = aal_clock_on,
	.power_off = aal_clock_off,
	.is_idle = NULL,
	.is_busy = NULL,
	.dump_info = NULL,
	.bypass = aal_bypass,
	.build_cmdq = NULL,
	.set_lcm_utils = NULL,
	.set_listener = aal_set_listener,
	.cmd = aal_io,
};


/*  ----------------------------------------------------------------------
	Test code
	Will not be linked in user build.
	---------------------------------------------------------------------- */

#define AAL_TLOG(fmt, arg...) DDPMSG("[AAL] " fmt "\n", ##arg)

static void aal_test_en(const char *cmd)
{
	int en = ((cmd[0] == '0') ? 0 : 1);

	DISP_REG_SET(NULL, DISP_AAL_EN, en);
	AAL_TLOG("EN = %d, read = %d", en, DISP_REG_GET(DISP_AAL_EN));
}


static void aal_dump_histogram(void)
{
	unsigned long flags;
	DISP_AAL_HIST *hist;
	int i;

	hist = kmalloc(sizeof(DISP_AAL_HIST), GFP_KERNEL);
	if (hist != NULL) {
		spin_lock_irqsave(&g_aal_hist_lock, flags);
		memcpy(hist, &g_aal_hist, sizeof(DISP_AAL_HIST));
		spin_unlock_irqrestore(&g_aal_hist_lock, flags);

		for (i = 0; i + 8 < AAL_HIST_BIN; i += 8) {
			AAL_TLOG("Hist[%d..%d] = %6d %6d %6d %6d %6d %6d %6d %6d",
				 i, i + 7, hist->maxHist[i], hist->maxHist[i + 1],
				 hist->maxHist[i + 2], hist->maxHist[i + 3], hist->maxHist[i + 4],
				 hist->maxHist[i + 5], hist->maxHist[i + 6], hist->maxHist[i + 7]);
		}
		for (; i < AAL_HIST_BIN; i++)
			AAL_TLOG("Hist[%d] = %6d", i, hist->maxHist[i]);

		kfree(hist);
	}
}


static void aal_test_ink(const char *cmd)
{
	int en = (cmd[0] - '0');
	const unsigned long cabc_04 = DISP_AAL_CABC_00 + 0x4 * 4;

	switch (en) {
	case 1:
		DISP_REG_SET(NULL, cabc_04, (1 << 31) | (511 << 18));
		break;
	case 2:
		DISP_REG_SET(NULL, cabc_04, (1 << 31) | (511 << 9));
		break;
	case 3:
		DISP_REG_SET(NULL, cabc_04, (1 << 31) | (511 << 0));
		break;
	case 4:
		DISP_REG_SET(NULL, cabc_04, (1 << 31) | (511 << 18) | (511 << 9) | 511);
		break;
	default:
		DISP_REG_SET(NULL, cabc_04, 0);
		break;
	}

	disp_aal_trigger_refresh();
}


void aal_test(const char *cmd, char *debug_output)
{
	debug_output[0] = '\0';
	AAL_TLOG("aal_test(%s)", cmd);

	if (strncmp(cmd, "en:", 3) == 0) {
		aal_test_en(cmd + 3);
	} else if (strncmp(cmd, "histogram", 5) == 0) {
		aal_dump_histogram();
	} else if (strncmp(cmd, "ink:", 4) == 0) {
		aal_test_ink(cmd + 4);
	} else if (strncmp(cmd, "bypass:", 7) == 0) {
		int bypass = (cmd[7] == '1');

		aal_bypass(DISP_MODULE_AAL, bypass);
	}
}

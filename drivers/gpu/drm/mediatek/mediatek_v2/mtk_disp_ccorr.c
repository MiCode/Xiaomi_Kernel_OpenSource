// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_color.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_helper.h"
#include "platform/mtk_drm_6789.h"

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif

#define DISP_REG_CCORR_EN (0x000)
#define DISP_REG_CCORR_INTEN                     (0x008)
#define DISP_REG_CCORR_INTSTA                    (0x00C)
#define DISP_REG_CCORR_CFG (0x020)
#define DISP_REG_CCORR_SIZE (0x030)
#define DISP_REG_CCORR_COLOR_OFFSET_0	(0x100)
#define DISP_REG_CCORR_COLOR_OFFSET_1	(0x104)
#define DISP_REG_CCORR_COLOR_OFFSET_2	(0x108)
#define CCORR_COLOR_OFFSET_MASK	(0x3FFFFFF)

#define DISP_REG_CCORR_SHADOW (0x0A0)
#define CCORR_READ_WORKING		BIT(0)
#define CCORR_BYPASS_SHADOW		BIT(2)

#define CCORR_12BIT_MASK				0x0fff
#define CCORR_13BIT_MASK				0x1fff

#define CCORR_INVERSE_GAMMA   (0)
#define CCORR_BYASS_GAMMA      (1)

#define CCORR_REG(idx) (idx * 4 + 0x80)
#define CCORR_CLIP(val, min, max) ((val >= max) ? \
	max : ((val <= min) ? min : val))

static unsigned int g_ccorr_8bit_switch[DISP_CCORR_TOTAL];
static unsigned int g_ccorr_relay_value[DISP_CCORR_TOTAL];

struct drm_mtk_ccorr_caps disp_ccorr_caps;
static int ccorr_offset_base = 1024;
static int ccorr_max_negative = -2048;
static int ccorr_max_positive = 2047;
static int ccorr_fullbit_mask = 0x0fff;
static int ccorr_offset_mask = 14;
unsigned int disp_ccorr_number;
unsigned int disp_ccorr_linear;
bool disp_aosp_ccorr;
static bool g_prim_ccorr_force_linear;
static bool g_prim_ccorr_pq_nonlinear;

#define index_of_ccorr(module) ((module == DDP_COMPONENT_CCORR0) ? 0 : \
		((module == DDP_COMPONENT_CCORR1) ? 1 : \
		((module == DDP_COMPONENT_CCORR2) ? 2 : 3)))

static bool bypass_color0, bypass_color1;

static atomic_t g_ccorr_is_clock_on[DISP_CCORR_TOTAL] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0) };

static atomic_t g_irq_backlight_change = ATOMIC_INIT(0);

static struct DRM_DISP_CCORR_COEF_T *g_disp_ccorr_coef[DISP_CCORR_TOTAL] = {
	NULL };
static int g_ccorr_color_matrix[DISP_CCORR_TOTAL][3][3] = {
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	} };
static int g_ccorr_prev_matrix[DISP_CCORR_TOTAL][3][3] = {
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	} };
static int g_rgb_matrix[DISP_CCORR_TOTAL][3][3] = {
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	},
	{
		{1024, 0, 0},
		{0, 1024, 0},
		{0, 0, 1024}
	} };
static struct DRM_DISP_CCORR_COEF_T g_multiply_matrix_coef;
static int g_disp_ccorr_without_gamma;
static int g_disp_ccorr_temp_linear;

static DECLARE_WAIT_QUEUE_HEAD(g_ccorr_get_irq_wq);
//static DEFINE_SPINLOCK(g_ccorr_get_irq_lock);
static DEFINE_SPINLOCK(g_ccorr_clock_lock);
static atomic_t g_ccorr_get_irq = ATOMIC_INIT(0);

/* FOR TRANSITION */
static DEFINE_SPINLOCK(g_pq_bl_change_lock);
static int g_old_pq_backlight;
static int g_pq_backlight;
static int g_pq_backlight_db;
static atomic_t g_ccorr_is_init_valid = ATOMIC_INIT(0);

static DEFINE_MUTEX(g_ccorr_global_lock);
// For color conversion bug fix
//static bool need_offset;
#define OFFSET_VALUE (1024)

/* TODO */
/* static ddp_module_notify g_ccorr_ddp_notify; */

// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *ccorr1_default_comp;

static int disp_ccorr_write_coef_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock);
/* static void ccorr_dump_reg(void); */

enum CCORR_IOCTL_CMD {
	SET_CCORR = 0,
	SET_INTERRUPT,
	BYPASS_CCORR
};

struct mtk_disp_ccorr {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_ccorr_data *data;
};

static inline struct mtk_disp_ccorr *comp_to_ccorr(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ccorr, ddp_comp);
}

static void disp_ccorr_multiply_3x3(unsigned int ccorrCoef[3][3],
	int color_matrix[3][3], unsigned int resultCoef[3][3])
{
	int temp_Result;
	int signedCcorrCoef[3][3];
	int i, j;

	/* convert unsigned 12 bit ccorr coefficient to signed 12 bit format */
	for (i = 0; i < 3; i += 1) {
		for (j = 0; j < 3; j += 1) {
			if (ccorrCoef[i][j] > ccorr_max_positive) {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j] - (ccorr_offset_base<<2);
			} else {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j];
			}
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPINFO("signedCcorrCoef[%d][0-2] = {%d, %d, %d}\n", i,
			signedCcorrCoef[i][0],
			signedCcorrCoef[i][1],
			signedCcorrCoef[i][2]);
	}

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][0] +
		signedCcorrCoef[0][1]*color_matrix[1][0] +
		signedCcorrCoef[0][2]*color_matrix[2][0]) / ccorr_offset_base);
	resultCoef[0][0] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][1] +
		signedCcorrCoef[0][1]*color_matrix[1][1] +
		signedCcorrCoef[0][2]*color_matrix[2][1]) / ccorr_offset_base);
	resultCoef[0][1] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][2] +
		signedCcorrCoef[0][1]*color_matrix[1][2] +
		signedCcorrCoef[0][2]*color_matrix[2][2]) / ccorr_offset_base);
	resultCoef[0][2] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][0] +
		signedCcorrCoef[1][1]*color_matrix[1][0] +
		signedCcorrCoef[1][2]*color_matrix[2][0]) / ccorr_offset_base);
	resultCoef[1][0] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][1] +
		signedCcorrCoef[1][1]*color_matrix[1][1] +
		signedCcorrCoef[1][2]*color_matrix[2][1]) / ccorr_offset_base);
	resultCoef[1][1] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][2] +
		signedCcorrCoef[1][1]*color_matrix[1][2] +
		signedCcorrCoef[1][2]*color_matrix[2][2]) / ccorr_offset_base);
	resultCoef[1][2] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][0] +
		signedCcorrCoef[2][1]*color_matrix[1][0] +
		signedCcorrCoef[2][2]*color_matrix[2][0]) / ccorr_offset_base);
	resultCoef[2][0] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][1] +
		signedCcorrCoef[2][1]*color_matrix[1][1] +
		signedCcorrCoef[2][2]*color_matrix[2][1]) / ccorr_offset_base);
	resultCoef[2][1] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][2] +
		signedCcorrCoef[2][1]*color_matrix[1][2] +
		signedCcorrCoef[2][2]*color_matrix[2][2]) / ccorr_offset_base);
	resultCoef[2][2] = CCORR_CLIP(temp_Result, ccorr_max_negative, ccorr_max_positive) &
		ccorr_fullbit_mask;

	for (i = 0; i < 3; i += 1) {
		DDPINFO("resultCoef[%d][0-2] = {0x%x, 0x%x, 0x%x}\n", i,
			resultCoef[i][0],
			resultCoef[i][1],
			resultCoef[i][2]);
	}
}

static int disp_ccorr_color_matrix_to_dispsys(struct drm_device *dev)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;

	// All Support 3*4 matrix on drm architecture
	if ((disp_ccorr_number == 1) && (disp_ccorr_linear&0x01)
		&& (!g_prim_ccorr_force_linear))
		ret = mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_PQ_34_COLOR_MATRIX", 0);
	else
		ret = mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_PQ_34_COLOR_MATRIX", 1);

	return ret;
}

static int disp_ccorr_write_coef_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DRM_DISP_CCORR_COEF_T *ccorr, *multiply_matrix;
	int ret = 0;
	unsigned int id = index_of_ccorr(comp->id);
	unsigned int temp_matrix[3][3];
	unsigned int cfg_val;
	int i, j;

	if (lock)
		mutex_lock(&g_ccorr_global_lock);

	ccorr = g_disp_ccorr_coef[id];
	DDPINFO("%s:ccorr id:%d,aosp ccorr:%d,nonlinear:%d\n", __func__, id,
		disp_aosp_ccorr, g_disp_ccorr_without_gamma);
	if (ccorr == NULL) {
		DDPINFO("%s: [%d] is not initialized\n", __func__, id);
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	//if (id == 0) {
		multiply_matrix = &g_multiply_matrix_coef;
		if (((g_prim_ccorr_force_linear && (disp_ccorr_linear&0x01)) ||
			(g_prim_ccorr_pq_nonlinear && (disp_ccorr_linear == 0x0))) &&
			(disp_ccorr_number == 1)) {
			disp_ccorr_multiply_3x3(ccorr->coef, g_ccorr_color_matrix[id],
				temp_matrix);
			disp_ccorr_multiply_3x3(temp_matrix, g_rgb_matrix[id],
				multiply_matrix->coef);
		} else {
			if (disp_aosp_ccorr) {
				disp_ccorr_multiply_3x3(ccorr->coef, g_ccorr_color_matrix[id],
					multiply_matrix->coef);//AOSP multiply
			} else {
				disp_ccorr_multiply_3x3(ccorr->coef, g_rgb_matrix[id],
					multiply_matrix->coef);//PQ service multiply
			}
		}
		ccorr = multiply_matrix;

		ccorr->offset[0] = g_disp_ccorr_coef[id]->offset[0];
		ccorr->offset[1] = g_disp_ccorr_coef[id]->offset[1];
		ccorr->offset[2] = g_disp_ccorr_coef[id]->offset[2];
	//}

	// For 6885 need to left shift one bit
	if (disp_ccorr_caps.ccorr_bit == 12) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				ccorr->coef[i][j] = ccorr->coef[i][j]<<1;
	}

	if (handle == NULL) {
		/* use CPU to write */
		writel(1, comp->regs + DISP_REG_CCORR_EN);
		cfg_val = 0x2 | g_ccorr_relay_value[id] |
			     (g_disp_ccorr_without_gamma << 2) |
				(g_ccorr_8bit_switch[id] << 10);
		writel(cfg_val, comp->regs + DISP_REG_CCORR_CFG);
		writel(((ccorr->coef[0][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[0][1] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(0));
		writel(((ccorr->coef[0][2] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][0] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(1));
		writel(((ccorr->coef[1][1] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][2] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(2));
		writel(((ccorr->coef[2][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[2][1] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(3));
		writel(((ccorr->coef[2][2] & CCORR_13BIT_MASK) << 16),
			comp->regs + CCORR_REG(4));
		/* Ccorr Offset */
		writel(((ccorr->offset[0] & CCORR_COLOR_OFFSET_MASK) |
			(0x1 << 31)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_0);
		writel(((ccorr->offset[1] & CCORR_COLOR_OFFSET_MASK)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_1);
		writel(((ccorr->offset[2] & CCORR_COLOR_OFFSET_MASK)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_2);
	} else {
		/* use CMDQ to write */

		cfg_val = 0x2 | g_ccorr_relay_value[index_of_ccorr(comp->id)] |
				(g_disp_ccorr_without_gamma << 2 |
				(g_ccorr_8bit_switch[id] << 10));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_CFG, cfg_val, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(0),
			((ccorr->coef[0][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[0][1] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(1),
			((ccorr->coef[0][2] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][0] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(2),
			((ccorr->coef[1][1] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][2] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(3),
			((ccorr->coef[2][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[2][1] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(4),
			((ccorr->coef[2][2] & CCORR_13BIT_MASK) << 16), ~0);
		/* Ccorr Offset */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_0,
			(ccorr->offset[0] & CCORR_COLOR_OFFSET_MASK) |
			(0x1 << 31), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_1,
			(ccorr->offset[1] & CCORR_COLOR_OFFSET_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_2,
			(ccorr->offset[2] & CCORR_COLOR_OFFSET_MASK), ~0);
	}

	DDPINFO("%s: finish\n", __func__);
ccorr_write_coef_unlock:
	if (lock)
		mutex_unlock(&g_ccorr_global_lock);

	return ret;
}

void disp_ccorr_on_end_of_frame(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;
	unsigned long flags;
	unsigned int index = index_of_ccorr(comp->id);

	spin_lock_irqsave(&g_ccorr_clock_lock, flags);

	if (atomic_read(&g_ccorr_is_clock_on[index]) != 1) {
		DDPINFO("%s: clock is off. enabled:%d\n", __func__, 0);

		spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
		return;
	}
	intsta = readl(comp->regs + DISP_REG_CCORR_INTSTA);

	if (intsta & 0x2) {	/* End of frame */
		// Clear irq
		writel(intsta & ~0x3, comp->regs
			+ DISP_REG_CCORR_INTSTA);

		if (index == 0) {
			atomic_set(&g_ccorr_get_irq, 1);
			wake_up_interruptible(&g_ccorr_get_irq_wq);
		}
	}
	spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
}

static void disp_ccorr_set_interrupt(struct mtk_ddp_comp *comp,
					int enabled)
{
	if (default_comp == NULL)
		default_comp = comp;

	if (!enabled && (g_old_pq_backlight != g_pq_backlight))
		g_old_pq_backlight = g_pq_backlight;
	else
		mtk_crtc_user_cmd(&(comp->mtk_crtc->base), comp,
			SET_INTERRUPT, &enabled);
}

static void disp_ccorr_clear_irq_only(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;
	unsigned long flags;
	unsigned int index = index_of_ccorr(comp->id);

	DDPDBG("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	if (spin_trylock_irqsave(&g_ccorr_clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		if (atomic_read(&g_ccorr_is_clock_on[index]) != 1) {
			DDPINFO("%s: clock is off. enabled:%d\n", __func__, 0);

			spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
			DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
				__func__, __LINE__);
			return;
		}
		intsta = readl(comp->regs + DISP_REG_CCORR_INTSTA);

		DDPINFO("%s: intsta: 0x%x\n", __func__, intsta);

		if (intsta & 0x2) { /* End of frame */
			writel(intsta & ~0x3, comp->regs
					+ DISP_REG_CCORR_INTSTA);
		}
		spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
		DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
			__func__, __LINE__);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave -- ",
			__func__, __LINE__);
	}


	/* disable interrupt */
	//disp_ccorr_set_interrupt(comp, 0);

	DDPDBG("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	if (spin_trylock_irqsave(&g_ccorr_clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		if (atomic_read(&g_ccorr_is_clock_on[index]) != 1) {
			DDPINFO("%s: clock is off. enabled:%d\n", __func__, 0);

			spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
			DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
				__func__, __LINE__);
			return;
		}

		{
			/* Disable output frame end interrupt */
			writel(0x0, comp->regs + DISP_REG_CCORR_INTEN);
			DDPINFO("%s: Interrupt disabled\n", __func__);
		}
			spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
			DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
				__func__, __LINE__);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave -- ",
			__func__, __LINE__);
	}


}

static irqreturn_t mtk_disp_ccorr_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ccorr *priv = dev_id;
	struct mtk_ddp_comp *ccorr = &priv->ddp_comp;

	disp_ccorr_on_end_of_frame(ccorr);

	return IRQ_HANDLED;
}

static int disp_ccorr_wait_irq(struct drm_device *dev, unsigned long timeout)
{
	int ret = 0;

	if (atomic_read(&g_ccorr_get_irq) == 0) {
		DDPDBG("%s: wait_event_interruptible ++ ", __func__);
		ret = wait_event_interruptible(g_ccorr_get_irq_wq,
			atomic_read(&g_ccorr_get_irq) == 1);
		DDPDBG("%s: wait_event_interruptible -- ", __func__);
		DDPINFO("%s: get_irq = 1, waken up", __func__);
		DDPINFO("%s: get_irq = 1, ret = %d", __func__, ret);
		if (atomic_read(&g_irq_backlight_change))
			atomic_set(&g_irq_backlight_change, 0);
	} else {
		/* If g_ccorr_get_irq is already set, */
		/* means PQService was delayed */
		DDPINFO("%s: get_irq = 0", __func__);
	}

	atomic_set(&g_ccorr_get_irq, 0);

	return ret;
}

static int disp_pq_copy_backlight_to_user(int *backlight)
{
	unsigned long flags;
	int ret = 0;

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_pq_bl_change_lock, flags);
	g_pq_backlight_db = g_pq_backlight;
	spin_unlock_irqrestore(&g_pq_bl_change_lock, flags);

	memcpy(backlight, &g_pq_backlight_db, sizeof(int));

	DDPINFO("%s: %d\n", __func__, ret);

	return ret;
}

void disp_pq_notify_backlight_changed(int bl_1024)
{
	unsigned long flags;

	spin_lock_irqsave(&g_pq_bl_change_lock, flags);
	g_old_pq_backlight = g_pq_backlight;
	g_pq_backlight = bl_1024;
	spin_unlock_irqrestore(&g_pq_bl_change_lock, flags);

	if (atomic_read(&g_ccorr_is_init_valid) != 1)
		return;

	DDPINFO("%s: %d\n", __func__, bl_1024);

	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (default_comp != NULL &&
			g_ccorr_relay_value[index_of_ccorr(default_comp->id)] != 1) {
			disp_ccorr_set_interrupt(default_comp, 1);

			if (default_comp != NULL &&
					default_comp->mtk_crtc != NULL)
				mtk_crtc_check_trigger(default_comp->mtk_crtc, false,
					true);

			atomic_set(&g_irq_backlight_change, 1);
			DDPINFO("%s: trigger refresh when backlight changed", __func__);
		}
	} else {
		if (default_comp != NULL && (g_old_pq_backlight == 0 || bl_1024 == 0)) {
			disp_ccorr_set_interrupt(default_comp, 1);

			if (default_comp != NULL &&
					default_comp->mtk_crtc != NULL)
				mtk_crtc_check_trigger(default_comp->mtk_crtc, false,
					true);

			atomic_set(&g_irq_backlight_change, 1);
			DDPINFO("%s: trigger refresh when backlight ON/Off", __func__);
		}
	}
}
EXPORT_SYMBOL(disp_pq_notify_backlight_changed);

static int disp_ccorr_set_coef(
	const struct DRM_DISP_CCORR_COEF_T *user_color_corr,
	struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	int ret = 0;
	struct DRM_DISP_CCORR_COEF_T *ccorr, *old_ccorr;
	int id = index_of_ccorr(comp->id);

	ccorr = kmalloc(sizeof(struct DRM_DISP_CCORR_COEF_T), GFP_KERNEL);
	if (ccorr == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_color_corr == NULL) {
		ret = -EFAULT;
		kfree(ccorr);
	} else {
		memcpy(ccorr, user_color_corr,
			sizeof(struct DRM_DISP_CCORR_COEF_T));

		if (id < DISP_CCORR_TOTAL) {
			mutex_lock(&g_ccorr_global_lock);

			old_ccorr = g_disp_ccorr_coef[id];
			g_disp_ccorr_coef[id] = ccorr;
			/* if ((g_disp_ccorr_coef[id]->offset[0] == 0) &&
				(g_disp_ccorr_coef[id]->offset[1] == 0) &&
				(g_disp_ccorr_coef[id]->offset[2] == 0) &&
				need_offset) {
				DDPINFO("%s:need change offset", __func__);
				g_disp_ccorr_coef[id]->offset[0] =
						(ccorr_offset_base << 1) << ccorr_offset_mask;
				g_disp_ccorr_coef[id]->offset[1] =
						(ccorr_offset_base << 1) << ccorr_offset_mask;
				g_disp_ccorr_coef[id]->offset[2] =
						(ccorr_offset_base << 1) << ccorr_offset_mask;
			}
			*/
			DDPINFO("%s: Set module(%d) coef", __func__, id);
			if (disp_aosp_ccorr)
				disp_aosp_ccorr = false;

			ret = disp_ccorr_write_coef_reg(comp, handle, 0);

			mutex_unlock(&g_ccorr_global_lock);

			if (old_ccorr != NULL)
				kfree(old_ccorr);

			mtk_crtc_check_trigger(comp->mtk_crtc, false, false);
		} else {
			DDPPR_ERR("%s: invalid ID = %d\n", __func__, id);
			ret = -EFAULT;
			kfree(ccorr);
		}
	}

	return ret;
}

static int mtk_disp_ccorr_set_interrupt(struct mtk_ddp_comp *comp, void *data)
{
	int enabled = *((int *)data);
	unsigned long flags;
	unsigned int index = index_of_ccorr(comp->id);
	int ret = 0;

	DDPDBG("%s @ %d......... spin_lock_irqsave ++ %d\n", __func__, __LINE__, index);
	spin_lock_irqsave(&g_ccorr_clock_lock, flags);
	DDPDBG("%s @ %d......... spin_lock_irqsave -- ",
		__func__, __LINE__);
	if (atomic_read(&g_ccorr_is_clock_on[index]) != 1) {
		DDPINFO("%s: clock is off. enabled:%d\n",
			__func__, enabled);

		spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
		DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
			__func__, __LINE__);
		return ret;
	}

	if (enabled || g_old_pq_backlight != g_pq_backlight) {
		if (readl(comp->regs + DISP_REG_CCORR_EN) == 0) {
			/* Print error message */
			DDPINFO("[WARNING] DISP_REG_CCORR_EN not enabled!\n");
		}
		/* Enable output frame end interrupt */
		writel(0x2, comp->regs + DISP_REG_CCORR_INTEN);
		DDPINFO("%s: Interrupt enabled\n", __func__);
	} else {
		/* Disable output frame end interrupt */
		writel(0x0, comp->regs + DISP_REG_CCORR_INTEN);
		DDPINFO("%s: Interrupt disabled\n", __func__);
	}
	spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
	DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
		__func__, __LINE__);
	return ret;
}

int disp_ccorr_set_color_matrix(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int32_t matrix[16], int32_t hint, bool fte_flag)
{
	int ret = 0;
	int i, j;
	int ccorr_without_gamma = 0;
	bool need_refresh = false;
	bool identity_matrix = true;
	unsigned int id = index_of_ccorr(comp->id);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct DRM_DISP_CCORR_COEF_T *ccorr;

	if (handle == NULL) {
		DDPPR_ERR("%s: cmdq can not be NULL\n", __func__);
		return -EFAULT;
	}
	if (identity_matrix && (disp_ccorr_number == 1) &&
			(!g_prim_ccorr_force_linear) && (disp_ccorr_linear & 0x01))
		return ret;

	if (g_disp_ccorr_coef[id] == NULL) {
		ccorr = kmalloc(sizeof(struct DRM_DISP_CCORR_COEF_T), GFP_KERNEL);
		if (ccorr == NULL) {
			DDPPR_ERR("%s: no memory\n", __func__);
			return -EFAULT;
		}
		g_disp_ccorr_coef[id] = ccorr;
	}

	mutex_lock(&g_ccorr_global_lock);


	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			/* Copy Color Matrix */
			g_ccorr_color_matrix[id][i][j] = matrix[j*4 + i];

			/* early jump out */
			if (ccorr_without_gamma == 1)
				continue;

			if (i == j && g_ccorr_color_matrix[id][i][j] != ccorr_offset_base) {
				ccorr_without_gamma = 1;
				identity_matrix = false;
			} else if (i != j && g_ccorr_color_matrix[id][i][j] != 0) {
				ccorr_without_gamma = 1;
				identity_matrix = false;
			}
		}
	}

	// hint: 0: identity matrix; 1: arbitraty matrix
	// fte_flag: true: gpu overlay && hwc not identity matrix
	// arbitraty matrix maybe identity matrix or color transform matrix;
	// only when set identity matrix and not gpu overlay, open display color
	DDPINFO("hint: %d, identity: %d, fte_flag: %d, bypass: color0:%d color1:%d",
		hint, identity_matrix, fte_flag, bypass_color0, bypass_color1);
	if (((hint == 0) || ((hint == 1) && identity_matrix)) && (!fte_flag)) {
		if (id == 0) {
			if (bypass_color0 == true) {
				struct mtk_ddp_comp *comp_color0 =
					priv->ddp_comp[DDP_COMPONENT_COLOR0];
				ddp_color_bypass_color(comp_color0, false, handle);
				bypass_color0 = false;
			}
		} else if (id == 1 || id == 2) {
			if (bypass_color1 == true) {
				struct mtk_ddp_comp *comp_color1 =
					priv->ddp_comp[DDP_COMPONENT_COLOR1];
				ddp_color_bypass_color(comp_color1, false, handle);
				bypass_color1 = false;
			}
		} else {
			DDPINFO("%s, id is invalid!\n", __func__);
		}
	} else {
		if (id == 0) {
			if ((bypass_color0 == false) && (disp_ccorr_number == 1)
				&& (!(disp_ccorr_linear & 0x01))) {
				struct mtk_ddp_comp *comp_color0 =
					priv->ddp_comp[DDP_COMPONENT_COLOR0];
				ddp_color_bypass_color(comp_color0, true, handle);
				bypass_color0 = true;
			}
		} else if (id == 1 || id == 2) {
			if ((bypass_color1 == false) && (disp_ccorr_number == 1)
				&& (!(disp_ccorr_linear & 0x01))) {
				struct mtk_ddp_comp *comp_color1 =
					priv->ddp_comp[DDP_COMPONENT_COLOR1];
				ddp_color_bypass_color(comp_color1, true, handle);
				bypass_color1 = true;
			}
		} else {
			DDPINFO("%s, id is invalid!\n", __func__);
		}
	}

	// offset part
/*	if ((matrix[12] != 0) || (matrix[13] != 0) || (matrix[14] != 0))
		need_offset = true;
	else
		need_offset = false;
*/

	g_disp_ccorr_coef[id]->offset[0] = (matrix[12] << 1) << ccorr_offset_mask;
	g_disp_ccorr_coef[id]->offset[1] = (matrix[13] << 1) << ccorr_offset_mask;
	g_disp_ccorr_coef[id]->offset[2] = (matrix[14] << 1) << ccorr_offset_mask;

	//if only ccorr0 hw exist and aosp forece linear or
	//pq force nonlinear,id should be 0, g_disp_ccorr_coef
	//should be PQ ioctl data, so no need to set value here

	if (!(((g_prim_ccorr_force_linear && (disp_ccorr_linear&0x01)) ||
		(g_prim_ccorr_pq_nonlinear && (disp_ccorr_linear == 0x0))) &&
		(disp_ccorr_number == 1))) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++) {
				g_disp_ccorr_coef[id]->coef[i][j] = 0;
				if (i == j)
					g_disp_ccorr_coef[id]->coef[i][j] = ccorr_offset_base;
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPDBG("g_ccorr_color_matrix[%d][0-2] = {%d, %d, %d}\n",
				i,
				g_ccorr_color_matrix[id][i][0],
				g_ccorr_color_matrix[id][i][1],
				g_ccorr_color_matrix[id][i][2]);
	}

	DDPDBG("g_ccorr_color_matrix offset {%d, %d, %d}, hint: %d\n",
		g_disp_ccorr_coef[id]->offset[0],
		g_disp_ccorr_coef[id]->offset[1],
		g_disp_ccorr_coef[id]->offset[2], hint);

	g_disp_ccorr_without_gamma = ccorr_without_gamma;
	g_disp_ccorr_temp_linear = g_disp_ccorr_without_gamma;

	disp_aosp_ccorr = true;
	disp_ccorr_write_coef_reg(comp, handle, 0);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			if (g_ccorr_prev_matrix[id][i][j]
				!= g_ccorr_color_matrix[id][i][j]) {
				/* refresh when matrix changed */
				need_refresh = true;
			}
			/* Copy Color Matrix */
			g_ccorr_prev_matrix[id][i][j] = g_ccorr_color_matrix[id][i][j];
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPINFO("g_ccorr_color_matrix[%d][0-2] = {%d, %d, %d}\n",
				i,
				g_ccorr_color_matrix[id][i][0],
				g_ccorr_color_matrix[id][i][1],
				g_ccorr_color_matrix[id][i][2]);
	}

	DDPINFO("g_disp_ccorr_without_gamma: [%d], need_refresh: [%d]\n",
		g_disp_ccorr_without_gamma, need_refresh);

	mutex_unlock(&g_ccorr_global_lock);

	if (need_refresh == true && comp->mtk_crtc != NULL)
		mtk_crtc_check_trigger(comp->mtk_crtc, false, false);

	return ret;
}

int disp_ccorr_set_RGB_Gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	int r, int g, int b)
{
	int ret;
	unsigned int id = index_of_ccorr(comp->id);

	mutex_lock(&g_ccorr_global_lock);
	g_rgb_matrix[id][0][0] = r;
	g_rgb_matrix[id][1][1] = g;
	g_rgb_matrix[id][2][2] = b;

	DDPINFO("%s: r[%d], g[%d], b[%d]", __func__, r, g, b);
	ret = disp_ccorr_write_coef_reg(comp, NULL, 0);
	mutex_unlock(&g_ccorr_global_lock);

	return ret;
}

int mtk_drm_ioctl_set_ccorr(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CCORR0];
	struct drm_crtc *crtc = private->crtc[0];
	struct DRM_DISP_CCORR_COEF_T *ccorr_config = data;
	int ret;

	if (ccorr_config->hw_id == DRM_DISP_CCORR1) {
		comp = private->ddp_comp[DDP_COMPONENT_CCORR1];
		g_disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;
	} else if (disp_ccorr_linear&0x01) {
		g_disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;
	} else {
		g_disp_ccorr_without_gamma = CCORR_BYASS_GAMMA;
		g_prim_ccorr_pq_nonlinear = true;
	}

	if (m_new_pq_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {

		ret = mtk_crtc_user_cmd(crtc, comp, SET_CCORR, data);

		if ((ccorr_config->silky_bright_flag) == 1 &&
			ccorr_config->FinalBacklight != 0) {
			DDPINFO("brightness = %d, silky_bright_flag = %d",
				ccorr_config->FinalBacklight,
				ccorr_config->silky_bright_flag);
			mtk_leds_brightness_set("lcd-backlight",
				ccorr_config->FinalBacklight);
		}

		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);

		return ret;
	} else {
		return mtk_crtc_user_cmd(crtc, comp, SET_CCORR, data);
	}
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event_to_pq(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_conf_info *led_conf;

	led_conf = (struct led_conf_info *)v;

	switch (event) {
	case LED_BRIGHTNESS_CHANGED:
		trans_level = led_conf->cdev.brightness;

		disp_pq_notify_backlight_changed(trans_level);
		DDPINFO("%s: brightness changed: %d(%d)\n",
			__func__, trans_level, led_conf->cdev.brightness);
		break;
	case LED_STATUS_SHUTDOWN:
		disp_pq_notify_backlight_changed(0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event_to_pq,
};
#endif

int mtk_drm_ioctl_ccorr_eventctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CCORR0];
	int ret = 0;
	/* TODO: dual pipe */
	int *enabled = data;

	if (enabled || g_old_pq_backlight != g_pq_backlight)
		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);

	//mtk_crtc_user_cmd(crtc, comp, EVENTCTL, data);
	DDPINFO("ccorr_eventctl, enabled = %d\n", *enabled);

	if ((!atomic_read(&g_irq_backlight_change)) || (*enabled == 1))
		disp_ccorr_set_interrupt(comp, *enabled);

	return ret;
}

int mtk_drm_ioctl_ccorr_get_irq(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;

	atomic_set(&g_ccorr_is_init_valid, 1);

	disp_ccorr_wait_irq(dev, 60);

	if (disp_pq_copy_backlight_to_user((int *) data) < 0) {
		DDPPR_ERR("%s: failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

int mtk_drm_ioctl_support_color_matrix(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;
	struct DISP_COLOR_TRANSFORM *color_transform;
	bool support_matrix = true;
	bool identity_matrix = true;
	int i, j;

	if (data == NULL) {
		support_matrix = false;
		ret = -EFAULT;
		DDPINFO("unsupported matrix");
		return ret;
	}

	color_transform = data;

	// Support matrix:
	// AOSP is 4x3 matrix. Offset is located at 4th row (not zero)

	for (i = 0 ; i < 3; i++) {
		if (color_transform->matrix[i][3] != 0) {
			for (i = 0 ; i < 4; i++) {
				DDPINFO("unsupported:[%d][0-3]:[%d %d %d %d]",
					i,
					color_transform->matrix[i][0],
					color_transform->matrix[i][1],
					color_transform->matrix[i][2],
					color_transform->matrix[i][3]);
			}
			support_matrix = false;
			ret = -EFAULT;
			return ret;
		}
	}
	if (support_matrix) {
		ret = 0; //Zero: support color matrix.
		for (i = 0 ; i < 3; i++)
			for (j = 0 ; j < 3; j++)
				if ((i == j) &&
					(color_transform->matrix[i][j] != ccorr_offset_base))
					identity_matrix = false;
	}

	//if only one ccorr and ccorr0 is linear, AOSP matrix unsupport
	if ((disp_ccorr_number == 1) && (disp_ccorr_linear&0x01)
		&& (!identity_matrix) && (!g_prim_ccorr_force_linear))
		ret = -EFAULT;
	else
		ret = 0;

	return ret;
}

int mtk_get_ccorr_caps(struct drm_mtk_ccorr_caps *ccorr_caps)
{
	memcpy(ccorr_caps, &disp_ccorr_caps, sizeof(disp_ccorr_caps));
	return 0;
}

int mtk_set_ccorr_caps(struct drm_mtk_ccorr_caps *ccorr_caps)
{
	if (ccorr_caps->ccorr_linear != disp_ccorr_caps.ccorr_linear) {
		disp_ccorr_caps.ccorr_linear = ccorr_caps->ccorr_linear;
		disp_ccorr_linear = disp_ccorr_caps.ccorr_linear;
		DDPINFO("%s:update ccorr 0 linear by DORA API\n", __func__);
	}
	return 0;
}

static void mtk_ccorr_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	DDPINFO("%s\n", __func__);

	if (cfg->source_bpc == 8)
		g_ccorr_8bit_switch[index_of_ccorr(comp->id)] = 1;
	else if (cfg->source_bpc == 10)
		g_ccorr_8bit_switch[index_of_ccorr(comp->id)] = 0;
	else
		DDPINFO("Disp CCORR's bit is : %u\n", cfg->bpc);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_SIZE,
		       (width << 16) | cfg->h, ~0);
}

static void mtk_ccorr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	disp_aosp_ccorr = false;
	g_disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;

	if (disp_ccorr_number == 2) {
		if (comp->id == DDP_COMPONENT_CCORR0) {
			disp_aosp_ccorr = true;
			g_disp_ccorr_without_gamma = g_disp_ccorr_temp_linear;
		}

		if ((comp->mtk_crtc->is_dual_pipe) &&
			(comp->id == DDP_COMPONENT_CCORR2)) {
			disp_aosp_ccorr = true;
			g_disp_ccorr_without_gamma = g_disp_ccorr_temp_linear;
		}

	} else if (!(disp_ccorr_linear & 0x01)) {
		disp_aosp_ccorr = true;
		g_disp_ccorr_without_gamma = g_disp_ccorr_temp_linear;
	}

	disp_ccorr_write_coef_reg(comp, handle, 1);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_EN, 0x1, 0x1);
}

static void mtk_ccorr_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_CFG, bypass, 0x1);
	g_ccorr_relay_value[index_of_ccorr(comp->id)] = bypass;
}

static int mtk_ccorr_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_CCORR:
	{
		struct DRM_DISP_CCORR_COEF_T *config = data;
		struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);

		if (disp_ccorr_set_coef(config,
			comp, handle) < 0) {
			DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_ccorr1 = priv->ddp_comp[DDP_COMPONENT_CCORR1];
			if (ccorr->data->single_pipe_ccorr_num == 2)
				comp_ccorr1 = priv->ddp_comp[DDP_COMPONENT_CCORR3];

			if (disp_ccorr_set_coef(config, comp_ccorr1, handle) < 0) {
				DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
				return -EFAULT;
			}
		}

	}
	break;

	case SET_INTERRUPT:
	{
		struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);

		mtk_disp_ccorr_set_interrupt(comp, data);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_ccorr1 = priv->ddp_comp[DDP_COMPONENT_CCORR1];
			if (ccorr->data->single_pipe_ccorr_num == 2)
				comp_ccorr1 = priv->ddp_comp[DDP_COMPONENT_CCORR2];

			mtk_disp_ccorr_set_interrupt(comp_ccorr1, data);
		}
	}
	break;

	case BYPASS_CCORR:
	{
		int *value = data;
		int i, ccorr_num = 1;
		struct mtk_ddp_comp *comp_ccorr[4] = { NULL };

		struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct drm_crtc *crtc = &mtk_crtc->base;
		struct mtk_drm_private *priv = crtc->dev->dev_private;

		comp_ccorr[0] = comp;

		if ((ccorr->data->single_pipe_ccorr_num == 2) &&
			comp->mtk_crtc->is_dual_pipe) {
			ccorr_num = 4;
			comp_ccorr[1] = priv->ddp_comp[DDP_COMPONENT_CCORR1];
			comp_ccorr[2] = priv->ddp_comp[DDP_COMPONENT_CCORR2];
			comp_ccorr[3] = priv->ddp_comp[DDP_COMPONENT_CCORR3];
		} else if ((ccorr->data->single_pipe_ccorr_num == 2) ||
			comp->mtk_crtc->is_dual_pipe) {
			ccorr_num = 2;
			comp_ccorr[1] = priv->ddp_comp[DDP_COMPONENT_CCORR1];
		}

		for (i = 0; i < ccorr_num; i++) {
			if (comp_ccorr[i] != NULL)
				mtk_ccorr_bypass(comp_ccorr[i], *value, handle);
		}
	}
	break;

	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

struct ccorr_backup {
	unsigned int REG_CCORR_CFG;
};
static struct ccorr_backup g_ccorr_backup;

static void ddp_ccorr_backup(struct mtk_ddp_comp *comp)
{
	g_ccorr_backup.REG_CCORR_CFG =
		readl(comp->regs + DISP_REG_CCORR_CFG);
}

static void ddp_ccorr_restore(struct mtk_ddp_comp *comp)
{
	writel(g_ccorr_backup.REG_CCORR_CFG, comp->regs + DISP_REG_CCORR_CFG);
}

static void mtk_ccorr_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);

	DDPINFO("%s\n", __func__);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_ccorr_is_clock_on[index_of_ccorr(comp->id)], 1);

	/* Bypass shadow register and read shadow register */
	if (ccorr->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, CCORR_BYPASS_SHADOW,
			DISP_REG_CCORR_SHADOW, CCORR_BYPASS_SHADOW);
	ddp_ccorr_restore(comp);
}

static void mtk_ccorr_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	disp_ccorr_clear_irq_only(comp);

	DDPINFO("%s @ %d......... spin_lock_irqsave ++ ", __func__, __LINE__);
	spin_lock_irqsave(&g_ccorr_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_lock_irqsave -- ", __func__, __LINE__);
	atomic_set(&g_ccorr_is_clock_on[index_of_ccorr(comp->id)], 0);
	spin_unlock_irqrestore(&g_ccorr_clock_lock, flags);
	DDPDBG("%s @ %d......... spin_unlock_irqrestore ", __func__, __LINE__);
	wake_up_interruptible(&g_ccorr_get_irq_wq); // wake up who's waiting isr
	ddp_ccorr_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);

	DDPINFO("%s\n", __func__);

}

static const struct mtk_ddp_comp_funcs mtk_disp_ccorr_funcs = {
	.config = mtk_ccorr_config,
	.start = mtk_ccorr_start,
	.bypass = mtk_ccorr_bypass,
	.user_cmd = mtk_ccorr_user_cmd,
	.prepare = mtk_ccorr_prepare,
	.unprepare = mtk_ccorr_unprepare,
};

static int mtk_disp_ccorr_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	disp_ccorr_color_matrix_to_dispsys(drm_dev);
	return 0;
}

static void mtk_disp_ccorr_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ccorr_component_ops = {
	.bind	= mtk_disp_ccorr_bind,
	.unbind = mtk_disp_ccorr_unbind,
};

void mtk_ccorr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int  mtk_update_ccorr_base(void)
{
	int g, i, j;

	if (disp_ccorr_caps.ccorr_bit == 12)
		return 0;

	ccorr_offset_base = 2048;
	ccorr_max_negative = ccorr_offset_base*(-2);
	ccorr_max_positive = (ccorr_offset_base*2)-1;
	ccorr_fullbit_mask = 0x1fff;
	ccorr_offset_mask = 13;

	for (g = 0; g < DISP_CCORR_TOTAL; g++)
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++) {
				if (i == j) {
					g_ccorr_color_matrix[g][i][j] = ccorr_offset_base;
					g_ccorr_prev_matrix[g][i][j] = ccorr_offset_base;
					g_rgb_matrix[g][i][j] = ccorr_offset_base;
				}
			}
	return 0;
}

static void mtk_get_ccorr_property(struct device_node *node)
{
	int ret;
	int ccorr0_force_linear = 0;

	ret = of_property_read_u32(node, "ccorr_bit", &disp_ccorr_caps.ccorr_bit);
	if (ret)
		DDPPR_ERR("read ccorr_bit failed\n");

	ret = of_property_read_u32(node, "ccorr_num_per_pipe", &disp_ccorr_caps.ccorr_number);
	if (ret)
		DDPPR_ERR("read ccorr_number failed\n");

	ret = of_property_read_u32(node, "ccorr_linear_per_pipe", &disp_ccorr_caps.ccorr_linear);
	if (ret)
		DDPPR_ERR("read ccorr_linear failed\n");

	ret = of_property_read_u32(node, "ccorr_prim_force_linear", &ccorr0_force_linear);
	if (ret)
		DDPPR_ERR("read ccorr_prim_force_linear failed\n");

	DDPINFO("%s:ccorr_bit:%d,ccorr_number:%d,ccorr_linear:%d,ccorr0 force linear:%d\n",
		__func__, disp_ccorr_caps.ccorr_bit, disp_ccorr_caps.ccorr_number,
		disp_ccorr_caps.ccorr_linear, ccorr0_force_linear);

	disp_ccorr_number = disp_ccorr_caps.ccorr_number;
	disp_ccorr_linear = disp_ccorr_caps.ccorr_linear;

	if (ccorr0_force_linear == 0x1)
		g_prim_ccorr_force_linear = true;
	else
		g_prim_ccorr_force_linear = false;

	mtk_update_ccorr_base();

}

static int mtk_disp_ccorr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ccorr *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CCORR);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	if (comp_id == DDP_COMPONENT_CCORR0) {
		disp_ccorr_caps.ccorr_bit = 12;
		disp_ccorr_caps.ccorr_number = 1;
		disp_ccorr_caps.ccorr_linear = 0x01;
		g_prim_ccorr_force_linear = false;
		g_prim_ccorr_pq_nonlinear = false;
		mtk_get_ccorr_property(dev->of_node);
	}

	if (!default_comp && comp_id == DDP_COMPONENT_CCORR0)
		default_comp = &priv->ddp_comp;
	if (!ccorr1_default_comp &&
		comp_id == DDP_COMPONENT_CCORR1)
		ccorr1_default_comp = &priv->ddp_comp;

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ccorr_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_disp_ccorr_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED,
			       dev_name(dev), priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ccorr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (comp_id == DDP_COMPONENT_CCORR0)
		mtk_leds_register_notifier(&leds_init_notifier);
#endif
	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_ccorr_remove(struct platform_device *pdev)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ccorr_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (priv->ddp_comp.id == DDP_COMPONENT_CCORR0)
		mtk_leds_unregister_notifier(&leds_init_notifier);
#endif
	return 0;
}

static const struct mtk_disp_ccorr_data mt6779_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.single_pipe_ccorr_num = 1,
};

static const struct mtk_disp_ccorr_data mt6885_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
	.single_pipe_ccorr_num = 1,
};

static const struct mtk_disp_ccorr_data mt6873_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 1,
};

static const struct mtk_disp_ccorr_data mt6853_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 2,
};

static const struct mtk_disp_ccorr_data mt6833_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 1,
};

static const struct mtk_disp_ccorr_data mt6983_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 2,
};

static const struct mtk_disp_ccorr_data mt6895_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 2,
};

static const struct mtk_disp_ccorr_data mt6879_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 2,
};

static const struct mtk_disp_ccorr_data mt6855_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
	.single_pipe_ccorr_num = 1,
};

static const struct of_device_id mtk_disp_ccorr_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-ccorr",
	  .data = &mt6779_ccorr_driver_data},
	{ .compatible = "mediatek,mt6789-disp-ccorr",
	  .data = &mt6789_ccorr_driver_data},
	{ .compatible = "mediatek,mt6885-disp-ccorr",
	  .data = &mt6885_ccorr_driver_data},
	{ .compatible = "mediatek,mt6873-disp-ccorr",
	  .data = &mt6873_ccorr_driver_data},
	{ .compatible = "mediatek,mt6853-disp-ccorr",
	  .data = &mt6853_ccorr_driver_data},
	{ .compatible = "mediatek,mt6833-disp-ccorr",
	  .data = &mt6833_ccorr_driver_data},
	{ .compatible = "mediatek,mt6983-disp-ccorr",
	  .data = &mt6983_ccorr_driver_data},
	{ .compatible = "mediatek,mt6895-disp-ccorr",
	  .data = &mt6895_ccorr_driver_data},
	{ .compatible = "mediatek,mt6879-disp-ccorr",
	  .data = &mt6879_ccorr_driver_data},
	{ .compatible = "mediatek,mt6855-disp-ccorr",
	  .data = &mt6855_ccorr_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_ccorr_driver_dt_match);

struct platform_driver mtk_disp_ccorr_driver = {
	.probe = mtk_disp_ccorr_probe,
	.remove = mtk_disp_ccorr_remove,
	.driver = {

			.name = "mediatek-disp-ccorr",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ccorr_driver_dt_match,
		},
};

void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	if (g_ccorr_relay_value[index_of_ccorr(default_comp->id)] == bypass &&
		g_ccorr_relay_value[index_of_ccorr(ccorr1_default_comp->id)] == bypass)
		return;
	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_CCORR, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

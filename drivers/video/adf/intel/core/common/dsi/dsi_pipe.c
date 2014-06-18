/*
 * ann_dsi_pipe.c
 *
 *  Created on: May 23, 2014
 *      Author: root
 */

#include <drm/drm_mode.h>

#include "core/common/dsi/dsi_pipe.h"
#include "core/common/dsi/dsi_dbi.h"
#include "core/common/dsi/dsi_dpi.h"

#define KSEL_CRYSTAL_19 1
#define KSEL_BYPASS_19 5
#define KSEL_BYPASS_25 6
#define KSEL_BYPASS_83_100 7

#define MRFLD_M_MIN	    21
#define MRFLD_M_MAX	    180
static const u32 mrfld_m_converts[] = {
	/* M configuration table from 9-bit LFSR table */
	224, 368, 440, 220, 366, 439, 219, 365, 182, 347,	/* 21 - 30 */
	173, 342, 171, 85, 298, 149, 74, 37, 18, 265,	/* 31 - 40 */
	388, 194, 353, 432, 216, 108, 310, 155, 333, 166,	/* 41 - 50 */
	83, 41, 276, 138, 325, 162, 337, 168, 340, 170,	/* 51 - 60 */
	341, 426, 469, 234, 373, 442, 221, 110, 311, 411,	/* 61 - 70 */
	461, 486, 243, 377, 188, 350, 175, 343, 427, 213,	/* 71 - 80 */
	106, 53, 282, 397, 354, 227, 113, 56, 284, 142,	/* 81 - 90 */
	71, 35, 273, 136, 324, 418, 465, 488, 500, 506,	/* 91 - 100 */
	253, 126, 63, 287, 399, 455, 483, 241, 376, 444,	/* 101 - 110 */
	478, 495, 503, 251, 381, 446, 479, 239, 375, 443,	/* 111 - 120 */
	477, 238, 119, 315, 157, 78, 295, 147, 329, 420,	/* 121 - 130 */
	210, 105, 308, 154, 77, 38, 275, 137, 68, 290,	/* 131 - 140 */
	145, 328, 164, 82, 297, 404, 458, 485, 498, 249,	/* 141 - 150 */
	380, 190, 351, 431, 471, 235, 117, 314, 413, 206,	/* 151 - 160 */
	103, 51, 25, 12, 262, 387, 193, 96, 48, 280,	/* 161 - 170 */
	396, 198, 99, 305, 152, 76, 294, 403, 457, 228,	/* 171 - 180 */
};

struct clock_info {
	int dot;
	/* Multiplier */
	int m;
	/* Post divisor */
	int p1;
};

static bool get_ref_clock(struct intel_pipe *pipe, int ksel,
		int core_freq, int *ref_clk, int *clk_n, int *clk_p2)
{
	if (!pipe) {
		pr_err("%s: invalid pipe\n", __func__);
		return false;
	}

	switch (ksel) {
	case KSEL_CRYSTAL_19:
	case KSEL_BYPASS_19:
		*ref_clk = 19200;

		if (pipe->primary)
			*clk_n = 1, *clk_p2 = 8;
		else
			*clk_n = 1, *clk_p2 = 10;
		break;
	case KSEL_BYPASS_25:
		*ref_clk = 25000;

		if (pipe->primary)
			*clk_n = 1, *clk_p2 = 8;
		else
			*clk_n = 1, *clk_p2 = 10;
		break;
	case KSEL_BYPASS_83_100:
		if (core_freq == 166)
			*ref_clk = 83000;
		else if (core_freq == 100 || core_freq == 200)
			*ref_clk = 100000;

		if (pipe->primary)
			*clk_n = 4, *clk_p2 = 8;
		else
			*clk_n = 4, *clk_p2 = 10;
		break;
	default:
		pr_err("%s: invalid reference clock.\n", __func__);
		return false;
	}

	return true;
}

static int dsi_pipe_setup_clocking(struct intel_pipe *pipe, int pixel_clk)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	struct dsi_context *ctx = &config->ctx;
	struct dsi_panel *panel = dsi_pipe->panel;
	int ref_clk = 0, clk_n = 0, clk_p2 = 0, byte_clk = 1, clk_tmp = 0;
	int m_conv = 0;
	u32 pll = 0, fp = 0;
	struct clock_info clock;

	if (!get_ref_clock(pipe, KSEL_CRYSTAL_19, 200, &ref_clk,
			   &clk_n, &clk_p2)) {
		pr_err("%s: failed to get reference clock\n", __func__);
		return -EINVAL;
	}

	if (pipe->primary)
		byte_clk = 3;

	clk_tmp = pixel_clk * clk_n * clk_p2 * byte_clk;

	/*
	 * FIXME: Hard code the divisors' value for JDI panel, and need to
	 * calculate them according to the DSI PLL HAS spec.
	 */
	memset(&clock, 0, sizeof(clock));
	switch (panel->panel_id) {
	case JDI_7x12_CMD:
		/* JDI 7x12 CMD */
		clock.dot = 576000;
		clock.p1 = 4;
		clock.m = 142;
		break;
	case JDI_7x12_VID:
		/* JDI 7x12 VID */
		clock.dot = 576000;
		clock.p1 = 5;
		clock.m = 130;
		break;
	case SHARP_10x19_CMD:
		/* Sharp 10x19 CMD */
		clock.dot = 801600;
		clock.p1 = 3;
		clock.m = 137;
		break;
	case SHARP_10x19_DUAL_CMD:
		/* Sharp 10x19 DUAL CMD */
		/* FIXME */
		clock.dot = 801600;
		clock.p1 = 3;
		clock.m = 125;
		break;
	case SHARP_25x16_VID:
		/* Sharp 25x16 VID */
		clock.dot = 0;
		clock.p1 = 3;
		clock.m = 140;
		break;
	case SHARP_25x16_CMD:
		/* Sharp 25x16 CMD */
		clock.dot = 0;
		clock.p1 = 3;
		clock.m = 138;
		break;
	default:
		pr_err("%s: unsupported panel %u\n", __func__, panel->panel_id);
		return -EOPNOTSUPP;
	}

	m_conv = mrfld_m_converts[(clock.m - MRFLD_M_MIN)];

	/* Write the N1 & M1 parameters into DSI_PLL_DIV_REG */
	fp = (clk_n / 2) << 16;
	fp |= m_conv;

	if (pipe->primary) {
		/* Enable DSI PLL clocks for DSI0 rather than CCK. */
		pll |= _CLK_EN_PLL_DSI0;
		pll &= ~_CLK_EN_CCK_DSI0;
		/* Select DSI PLL as the source of the mux input clocks. */
		pll &= ~_DSI_MUX_SEL_CCK_DSI0;
	} else
		pll |= VCO_SELECT_83_3_MHZ_REF_CLK;

	/* FIXME: consider dual panels. */
	/* if (is_mipi2 || is_dual_dsi(dev)) { */
	if (is_dual_link(config)) {
		/* Enable DSI PLL clocks for DSI1 rather than CCK. */
		pll |= _CLK_EN_PLL_DSI1;
		pll &= ~_CLK_EN_CCK_DSI1;
		/* Select DSI PLL as the source of the mux input clocks. */
		pll &= ~_DSI_MUX_SEL_CCK_DSI1;
	}

	/* compute bitmask from p1 value */
	pll |= (1 << (clock.p1 - 2)) << _P1_POST_DIV_SHIFT;

	if (pipe->primary) {
		ctx->dpll = pll;
		ctx->fp = fp;
	}

	return 0;
}

bool dsi_pipe_enable_clocking(struct dsi_pipe *pipe)
{
	struct dsi_config *config = &pipe->config;
	struct dsi_context *ctx = &config->ctx;
	u32 gunit_val = 0x0;
	u32 retry;

	if (IS_ANN())
		intel_mid_msgbus_write32(CCK_PORT,
				FUSE_OVERRIDE_FREQ_CNTRL_REG5,
				CKESC_GATE_EN | CKDP1X_GATE_EN |
				DISPLAY_FRE_EN | 0x2);

	/* Prepare DSI  PLL register before enabling */
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, 0);
	gunit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	gunit_val &= ~(DPLL_VCO_ENABLE | _DSI_LDO_EN
			|_CLK_EN_MASK | _DSI_MUX_SEL_CCK_DSI0 |
			_DSI_MUX_SEL_CCK_DSI1);
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, gunit_val);
	udelay(1);

	/* Program PLL */

	/*
	 * first set up the dpll and fp variables
	 * dpll - will contain the following information
	 *      - the clock source - DSI vs HFH vs LFH PLL
	 *      - what clocks should be running DSI0, DSI1
	 *      - and the divisor.
	 */

	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, ctx->fp);
	gunit_val &= ~_P1_POST_DIV_MASK;	/*clear the divisor bit*/
	/*
	 * the ctx->dpll contains the divisor that we need to use as well as
	 * which clocks need to start up
	 */
	gunit_val |= ctx->dpll;
	/* We want to clear the LDO enable when programming*/
	gunit_val &= ~_DSI_LDO_EN;
	/* Enable the DSI PLL */
	gunit_val |= DPLL_VCO_ENABLE;

	/* For the CD clock (clock used by Display controller), we need to set
	 * the DSI_CCK_PLL_SELECT bit (bit 11). This should already be set. But
	 * setting it just in case
	 */
	/* When using HFH PLL. */
	/* gunit_val |= _DSI_CCK_PLL_SELECT; */

	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, gunit_val);

	/* Wait for DSI PLL lock */
	retry = 10000;
	gunit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	while (((gunit_val & _DSI_PLL_LOCK) != _DSI_PLL_LOCK) && (--retry)) {
		udelay(3);
		gunit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
		if (!retry % 1000)
			pr_err("DSI PLL taking too long to lock - retry = %d\n",
			      10000 - retry);
	}
	if (retry == 0) {
		pr_err("DSI PLL fails to lock\n");
		return false;
	}

	return true;
}

bool dsi_pipe_disable_clocking(struct dsi_pipe *pipe)
{
	u32 gunit_val = 0x0;

	/* Disable PLL*/
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_DIV_REG, 0);

	gunit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	gunit_val &= ~_CLK_EN_MASK;
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, gunit_val);
	udelay(1);
	gunit_val &= ~DPLL_VCO_ENABLE;
	gunit_val |= _DSI_LDO_EN;
	intel_mid_msgbus_write32(CCK_PORT, DSI_PLL_CTRL_REG, gunit_val);
	udelay(1);

	gunit_val = intel_mid_msgbus_read32(CCK_PORT, DSI_PLL_CTRL_REG);
	if ((gunit_val & _DSI_PLL_LOCK) == _DSI_PLL_LOCK) {
		pr_err("DSI PLL fails to Unlock\n");
		return false;
	}

	return true;
}

static void dsi_pipe_suspend(struct intel_dc_component *component)
{
	struct intel_pipe *pipe = to_intel_pipe(component);
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_context *ctx = &dsi_pipe->config.ctx;

	/*power gate the power rail directly*/
	dsi_pipe->ops.power_off(dsi_pipe);
}

static void dsi_pipe_resume(struct intel_dc_component *component)
{
	struct intel_pipe *pipe = to_intel_pipe(component);
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_context *ctx = &dsi_pipe->config.ctx;

/*	mutex_lock(&dsi_pipe->config.ctx_lock);

	if (!(ctx->pipeconf & PIPEACONF_ENABLE)) {
		mutex_unlock(&dsi_pipe->config.ctx_lock);
		return;
	}

	pr_info("%s: id = %d\n", __func__, component->idx);

	mutex_unlock(&dsi_pipe->config.ctx_lock);
*/
	dsi_pipe->ops.power_on(dsi_pipe);
}

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static int dsi_set_brightness(struct intel_pipe *pipe, int level)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = NULL;
	struct dsi_config *config = NULL;
	struct dsi_context *ctx = NULL;
	int err = 0;

	if (!dsi_pipe) {
		pr_err("%s: invalid DSI interface", __func__);
		return -EINVAL;
	}

	panel = dsi_pipe->panel;
	if (!panel || !panel->ops || !panel->ops->set_brightness) {
		pr_err("%s: invalid panel\n", __func__);
		return -EINVAL;
	}

	config = &dsi_pipe->config;
	ctx = &config->ctx;

	mutex_lock(&config->ctx_lock);
	dsi_dsr_forbid_locked(dsi_pipe);

	ctx->backlight_level = level;
	err = panel->ops->set_brightness(dsi_pipe, level);

	dsi_dsr_allow_locked(dsi_pipe);
	mutex_unlock(&config->ctx_lock);

	return err;
}
#endif

static int dsi_pipe_hw_init(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = dsi_pipe->panel;
	int err;

	if (!panel || !panel->ops) {
		pr_err("%s: invalid panel\n", __func__);
		err = -EINVAL;
		goto out_err;
	}

	/* FIXME: check for DBI, as reset isn't necessary here. */
	if (panel->info.dsi_type == DSI_DBI) {
		/*reset panel*/
		if (!panel->ops->reset) {
			pr_err("%s: reset panel operation is missing\n",
			       __func__);
			err = -ENODEV;
			goto out_err;
		}

		err = panel->ops->reset(dsi_pipe);
		if (err) {
			pr_err("%s: failed to reset panel\n", __func__);
			goto out_err;
		}
	}

	/*detect panel connection status*/
	if (!panel->ops->detect) {
		pr_err("%s: detect panel operation is missing\n", __func__);
		err = -ENODEV;
		goto out_err;
	}

	err = panel->ops->detect(dsi_pipe);
	if (err) {
		pr_err("%s: failed to detect panel connection\n", __func__);
		goto out_err;
	}

	/*init dsi controller*/
	if (!panel->ops->dsi_controller_init) {
		pr_err("%s: controller init operation is missing\n", __func__);
		err = -ENODEV;
		goto out_err;
	}

	panel->ops->dsi_controller_init(dsi_pipe);

	/* WA: power down islands turned on by firmware */
	power_island_put(OSPM_DISPLAY_A | OSPM_DISPLAY_C | OSPM_DISPLAY_MIO);

	return 0;
out_err:
	return err;
}

static void dsi_pipe_hw_deinit(struct intel_pipe *pipe)
{

}

static void dsi_get_modelist(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **modelist, size_t *n_modes)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;

	mutex_lock(&config->ctx_lock);
	*modelist = &config->perferred_mode;
	*n_modes = 1;
	mutex_unlock(&config->ctx_lock);
}

static void dsi_get_preferred_mode(struct intel_pipe *pipe,
		struct drm_mode_modeinfo **mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;

	mutex_lock(&config->ctx_lock);
	*mode = &config->perferred_mode;
	mutex_unlock(&config->ctx_lock);
}

static bool dsi_is_screen_connected(struct intel_pipe *pipe)
{
	return true;
}

static int dsi_dpms(struct intel_pipe *pipe, u8 state)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	int err = 0;

	if (!config)
		return -EINVAL;

	mutex_lock(&config->ctx_lock);

	switch (state) {
	case DRM_MODE_DPMS_ON:
		err = dsi_pipe->ops.power_on(dsi_pipe);
		break;
	case DRM_MODE_DPMS_OFF:
		err = dsi_pipe->ops.power_off(dsi_pipe);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	default:
		mutex_unlock(&config->ctx_lock);
		pr_err("%s: unsupported dpms mode\n", __func__);
		return -EOPNOTSUPP;
	}

	mutex_unlock(&config->ctx_lock);
	return err;
}

static int dsi_modeset(struct intel_pipe *pipe,
		struct drm_mode_modeinfo *mode)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = &dsi_pipe->config;
	int pixel_clk = 0;
	int err = 0;

	if (!mode) {
		pr_err("%s: invalid mode\n", __func__);
		err = -EINVAL;
		return err;
	}

	if (!config) {
		pr_err("%s: invalid DSI config\n", __func__);
		err = -EINVAL;
		return err;
	}

	mutex_lock(&config->ctx_lock);

	if (config->lane_count)
		pixel_clk = mode->clock / config->lane_count;
	else
		pixel_clk = mode->clock;

	err = dsi_pipe_setup_clocking(pipe, pixel_clk);
	if (err)
		goto out_err;

	err = dsi_pipe->ops.mode_set(dsi_pipe, mode);
	if (err) {
		pr_err("%s: failed to set mode\n", __func__);
		goto out_err;
	}

out_err:
	mutex_unlock(&config->ctx_lock);
	return err;
}

static int dsi_get_screen_size(struct intel_pipe *pipe,
		u16 *width_mm, u16 *height_mm)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = dsi_pipe->panel;
	struct panel_info pi;
	int err;

	if (!panel || !panel->ops || !panel->ops->get_panel_info) {
		pr_err("%s: failed to get panel info\n", __func__);
		err = -ENODEV;
		goto out_err;
	}

	panel->ops->get_panel_info(&pi);

	*width_mm = pi.width_mm;
	*height_mm = pi.height_mm;

	return 0;
out_err:
	return err;
}

static void dsi_on_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.on_post)
		dsi_pipe->ops.on_post(dsi_pipe);
}

static void dsi_pre_post(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.pre_post)
		dsi_pipe->ops.pre_post(dsi_pipe);
}

static u32 dsi_get_supported_events(struct intel_pipe *pipe)
{
	return INTEL_PIPE_EVENT_VSYNC;
}

static int dsi_set_event(struct intel_pipe *pipe, u8 event, bool enabled)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.set_event)
		return dsi_pipe->ops.set_event(dsi_pipe, event, enabled);
	else
		return -EOPNOTSUPP;
}

/**
 * FIXME: hardware vsync counter failed to work on ANN. use static SW
 * counter for now.
 */
static u32 vsync_counter;

#define VSYNC_COUNT_MAX_MASK 0xffffff

static void dsi_get_events(struct intel_pipe *pipe, u32 *events)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	u8 idx = pipe->base.idx;
	u32 dc_events = REG_READ(IIR);
	u32 event_bit;

	*events = 0;

	pr_debug("%s: IIR = %#x\n", __func__, dc_events);

	switch (idx) {
	case 0:
		event_bit = IIR_PIPEA_EVENT;
		break;
	case 2:
		event_bit = IIR_PIPEC_EVENT;
		break;
	default:
		pr_err("%s: invalid pipe index %d\n", __func__, idx);
		return;
	}

	if (!(dc_events & event_bit))
		return;

	/* Clear the 1st level interrupt. */
	REG_WRITE(IIR, dc_events & (IIR_PIPEA_EVENT | IIR_PIPEC_EVENT));

	if (dsi_pipe->ops.get_events)
		dsi_pipe->ops.get_events(dsi_pipe, events);

	/**
	 * FIXME: should use hardware vsync counter.
	 */
	if (*events & INTEL_PIPE_EVENT_VSYNC) {
		if (++vsync_counter > VSYNC_COUNT_MAX_MASK)
			vsync_counter = 0;
	}
}

u32 dsi_get_vsync_counter(struct intel_pipe *pipe, u32 interval)
{
	u32 count;
	u32 max_count_mask = VSYNC_COUNT_MAX_MASK;

	/* NOTE: PIPEFRAMEHIGH & PIPEFRAMEPIXEL regs are RESERVED in ANN DC. */
#if 0
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_registers *regs = &dsi_pipe->config.regs;
	u32 high1, high2, low;

	if (!(PIPEACONF_ENABLE & REG_READ(regs->pipeconf_reg))) {
		pr_err("%s: pipe was disabled\n", __func__);
		return 0;
	}

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((REG_READ(regs->pipeframehigh_reg) &
			PIPE_FRAME_HIGH_MASK) >> PIPE_FRAME_HIGH_SHIFT);
		low =  ((REG_READ(regs->pipeframepixel_reg) &
			PIPE_FRAME_LOW_MASK) >> PIPE_FRAME_LOW_SHIFT);
		high2 = ((REG_READ(regs->pipeframehigh_reg) &
			PIPE_FRAME_HIGH_MASK) >>  PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	count = (high1 << 8) | low;
#endif
	count = vsync_counter;
	count |= (~max_count_mask);
	count += interval;
	count &= max_count_mask;

	pr_debug("%s: count = %#x\n", __func__, count);

	return count;
}

/* Handle more device custom events. */
static void dsi_handle_events(struct intel_pipe *pipe, u32 events)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);

	if (dsi_pipe->ops.handle_events)
		dsi_pipe->ops.handle_events(dsi_pipe, events);
}

static struct intel_pipe_ops dsi_base_ops = {
	.base = {
		.suspend = dsi_pipe_suspend,
		.resume = dsi_pipe_resume,
	},
	.hw_init = dsi_pipe_hw_init,
	.hw_deinit = dsi_pipe_hw_deinit,
	.get_preferred_mode = dsi_get_preferred_mode,
	.is_screen_connected = dsi_is_screen_connected,
	.get_modelist = dsi_get_modelist,
	.dpms = dsi_dpms,
	.modeset = dsi_modeset,
	.get_screen_size = dsi_get_screen_size,
	.pre_post = dsi_pre_post,
	.on_post = dsi_on_post,
	.get_supported_events = dsi_get_supported_events,
	.set_event = dsi_set_event,
	.get_events = dsi_get_events,
	.get_vsync_counter = dsi_get_vsync_counter,
	.handle_events = dsi_handle_events,
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	.set_brightness = dsi_set_brightness,
#endif
};

void dsi_pipe_destroy(struct dsi_pipe *pipe)
{
	struct dsi_panel *panel;
	if (pipe) {
		panel = pipe->panel;
		if (panel) {
			switch (panel->info.dsi_type) {
			case DSI_DBI:
				dsi_dsr_destroy(&pipe->config);
				break;
			case DSI_DPI:
				dsi_sdo_destroy(&pipe->config);
				break;
			default:
				break;
			}
		}

		dsi_pkg_sender_destroy(&pipe->sender);
		dsi_config_destroy(&pipe->config);
	}
}

int dsi_pipe_init(struct dsi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx, u32 gtt_phy_addr)
{
	struct dsi_panel *panel;
	int err;

	if (!pipe || !primary_plane)
		return -EINVAL;

	memset(pipe, 0, sizeof(struct dsi_pipe));

	/*get panel*/
	panel = get_dsi_panel();
	if (!panel)
		return -ENODEV;

	/*init config*/
	err = dsi_config_init(&pipe->config, panel, idx);
	if (err)
		goto err;

	/*init dsi interface ops*/
	switch (panel->info.dsi_type) {
	case DSI_DBI:
		pipe->ops.power_on = dbi_pipe_power_on;
		pipe->ops.power_off = dbi_pipe_power_off;
		pipe->ops.mode_set = dbi_pipe_mode_set;
		pipe->ops.pre_post = dbi_pipe_pre_post;
		pipe->ops.on_post = dbi_pipe_on_post;
		pipe->ops.set_event = dbi_pipe_set_event;
		pipe->ops.get_events = dbi_pipe_get_events;
		pipe->ops.handle_events = NULL;
		break;
	case DSI_DPI:
		pipe->ops.power_on = dpi_pipe_power_on;
		pipe->ops.power_off = dpi_pipe_power_off;
		pipe->ops.mode_set = dpi_pipe_mode_set;
		pipe->ops.pre_post = dpi_pipe_pre_post;
		pipe->ops.set_event = dpi_pipe_set_event;
		pipe->ops.get_events = dpi_pipe_get_events;
		pipe->ops.handle_events = dpi_pipe_handle_events;
		break;
	default:
		goto err;
	}

	/*init sender*/
	err = dsi_pkg_sender_init(&pipe->sender, gtt_phy_addr,
		panel->info.dsi_type, idx);
	if (err)
		goto err;

	switch (panel->info.dsi_type) {
	case DSI_DBI:
		err = dsi_dsr_init(pipe);
		if (err)
			goto err;

		/* dsi_dsr_enable(pipe); */
		break;
	case DSI_DPI:
		err = dsi_sdo_init(pipe);
		if (err)
			goto err;

		/* dsi_sdo_enable(pipe); */
		break;
	default:
		goto err;
	}

	pipe->panel = panel;

	err = intel_pipe_init(&pipe->base, dev, idx, true, INTEL_PIPE_DSI,
		primary_plane, &dsi_base_ops, "dsi_pipe");
	if (err)
		goto err;

	return 0;
err:
	dsi_pipe_destroy(pipe);
	return err;
}

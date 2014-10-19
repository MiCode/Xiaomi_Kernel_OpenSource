/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Author:
 *		Deepak S <deepak.s@intel.com>
 */

#include <linux/uaccess.h>
#include <drm/i915_drm.h>
#include <drm/i915_dpst.h>
#include <video/adf.h>
#include <intel_adf_device.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dpst.h>

/*
 * DPST (Display Power Savings Technology) is a power savings features
 * which reduces the backlight while enhancing the image such that the
 * user does not perceive any difference in the image quality. The backlight
 * reduction can provide power savings
 *
 * The DPST IOCTL implemented in this file can be used by the DPST a user-mode
 * module. The IOCTL provides methods to initialize the DPST hardware,
 * manage DPST interrupts, and to apply the new backlight and image enhancement
 * values.
 *
 * The user mode module will initialize the DPST hardware when it starts up.
 * The kernel will notify user mode module of any DPST histogram interrupts.
 * When the user mode module receives a notification of these interrupts, it
 * will query the kernel for all the DPST histogram data. Using this data,
 * the user mode module will calculate new backlight and image enhancement
 * values and provides those values to the kernel to program into the DPST
 * hardware.
 */

static struct intel_dc_config *g_config;

static u32
vlv_get_backlight(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_config *config = NULL;
	struct dsi_context *ctx = NULL;

	config = &dsi_pipe->config;
	ctx = &config->ctx;

	return ctx->backlight_level;
}

static void
vlv_panel_set_backlight(struct intel_pipe *pipe, int level)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_panel *panel = dsi_pipe->panel;

	dsi_pipe->ops.set_brightness(level);
	panel->ops->set_brightness(dsi_pipe, level);
}

static bool
vlv_dpst_save_conn_config(struct intel_dc_config *config)
{
	struct intel_pipe *pipe;
	size_t n_intfs = config->n_pipes;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	int i;

	for (i = 0; i < n_intfs; i++) {
		pipe = config->pipes[i];
		if (pipe && pipe->ops && pipe->ops->is_screen_connected)
			if (pipe->primary_plane &&
					pipe->type == INTEL_PIPE_DSI) {
				vlv_config->dpst.pipe = pipe->base.idx;
				return true;
			}
	}
	return false;
}

u32
vlv_dpst_get_brightness(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	struct intel_pipe *pipe;
	int ret = 0;

	if (!vlv_config->dpst.enabled)
		return 0;

	vlv_dpst_save_conn_config(config);
	pipe = config->pipes[vlv_config->dpst.pipe];

	/* return the last (non-dpst) set backlight level */
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	ret = pipe->ops->get_brightness(pipe);
#endif
	return ret;

}

/* called by multi-process, be cautious to avoid race condition */
void
vlv_dpst_set_brightness(struct intel_pipe *pipe, u32 brightness_val)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(g_config);
	u32 backlight_level = brightness_val;

	if (!vlv_config->dpst.enabled)
		return;

	/* Calculate the backlight after it has been reduced by "dpst
	 * blc adjustment" percent . blc_adjustment value is stored
	 * after multiplying by 100, so we have to divide by 100 2nd time
	 * to get to the correct value */
	backlight_level = ((brightness_val *
				vlv_config->dpst.blc_adjustment)/100)/100;
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	vlv_panel_set_backlight(pipe, backlight_level);
#endif
}

static int
vlv_dpst_clear_hist_interrupt(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	REG_WRITE(vlv_config->dpst.reg.blm_hist_guard,
		       REG_READ(vlv_config->dpst.reg.blm_hist_guard) |
		       HISTOGRAM_EVENT_STATUS);
	return 0;
}

static int
vlv_dpst_enable_hist_interrupt(struct vlv_dc_config *vlv_config)
{
	struct intel_pipe  *pipe = g_config->pipes[vlv_config->dpst.pipe];
	u32 blm_hist_ctl;

	pipe->dpst_enabled = true;
	vlv_config->dpst.enabled = true;
	vlv_config->dpst.blc_adjustment = DPST_MAX_FACTOR;

	/* Enable histogram logic to collect data */
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl |= IE_HISTOGRAM_ENABLE | HSV_INTENSITY_MODE;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);

	/* Wait for VBLANK since the histogram enabling logic takes affect
	 * at the next vblank */

	/*
	 * FixMe: Call VBlank interface
	 * */

	/* Clear pending interrupt bit. Clearing the pending interrupt bit
	 * must be not be done at the same time as enabling the
	 * interrupt. */
	REG_WRITE(vlv_config->dpst.reg.blm_hist_guard,
		       REG_READ(vlv_config->dpst.reg.blm_hist_guard) |
		       HISTOGRAM_EVENT_STATUS);

	/* Enable histogram interrupts */
	REG_WRITE(vlv_config->dpst.reg.blm_hist_guard,
		       REG_READ(vlv_config->dpst.reg.blm_hist_guard) |
		       HISTOGRAM_INTERRUPT_ENABLE);

	/* DPST interrupt in DE_IER is enabled in irq_postinstall */

	return 0;
}

static int
vlv_dpst_disable_hist_interrupt(struct vlv_dc_config *vlv_config)
{
	struct intel_pipe  *pipe = g_config->pipes[vlv_config->dpst.pipe];
	u32 blm_hist_guard, blm_hist_ctl;

	vlv_config->dpst.enabled = false;
	pipe->dpst_enabled = false;
	vlv_config->dpst.blc_adjustment = DPST_MAX_FACTOR;

	/* Disable histogram interrupts. It is OK to clear pending interrupts
	 * and disable interrupts at the same time. */
	blm_hist_guard = REG_READ(vlv_config->dpst.reg.blm_hist_guard);
	blm_hist_guard |= HISTOGRAM_EVENT_STATUS; /* clear pending interrupts */
	blm_hist_guard &= ~HISTOGRAM_INTERRUPT_ENABLE;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_guard, blm_hist_guard);

	/* Disable histogram logic */
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl &= ~IE_HISTOGRAM_ENABLE;
	blm_hist_ctl &= ~IE_MOD_TABLE_ENABLE;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);

	/* DPST interrupt in DE_IER register is disabled in irq_uninstall */

	/* Setting blc level to what it would be without dpst adjustment */

	 vlv_panel_set_backlight(pipe, vlv_get_backlight(pipe));

	return 0;
}

static int
vlv_dpst_set_user_enable(struct intel_dc_config *config, bool enable)
{
	int ret = 0;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	vlv_config->dpst.user_enable = enable;

	if (enable) {
		if (!vlv_config->dpst.kernel_disable &&
				!vlv_config->dpst.enabled)
			return vlv_dpst_enable_hist_interrupt(vlv_config);
	} else {
		/* User disabling invalidates any saved settings */
		vlv_config->dpst.saved.is_valid = false;

		/* Avoid warning messages */
		if (vlv_config->dpst.enabled)
			ret = vlv_dpst_disable_hist_interrupt(vlv_config);
	}

	return ret;
}

static int
vlv_dpst_apply_luma(struct intel_dc_config *config,
		struct dpst_initialize_context *ioctl_data)
{
	u32 diet_factor, i;
	u32 blm_hist_ctl;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	struct intel_pipe  *pipe = config->pipes[vlv_config->dpst.pipe];

	/* This is an invalid call if we are disabled by the user
	 * If pipe_mismatch is true, the luma data is calculated from the
	 * histogram from old pipe, ignore it */
	if (!vlv_config->dpst.user_enable
			|| vlv_config->dpst.pipe_mismatch)
		return -EINVAL;

	/* This is not an invalid call if we are disabled by the kernel,
	 * because kernel disabling is transparent to the user and can easily
	 * occur before user has completed in-progress adjustments. If in fact
	 * we are disabled by the kernel, we must store the incoming values for
	 * later restore. Image enhancement values are stored on the hardware,
	 * because they will be safely ignored if the table is not enabled. */

	/* Setup register to access image enhancement value from
	 * index 0.*/
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl |= BIN_REG_FUNCTION_SELECT_IE;
	blm_hist_ctl &= ~BIN_REGISTER_INDEX_MASK;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);

	/* Program the image enhancement data passed from user mode. */
	for (i = 0; i < DPST_DIET_ENTRY_COUNT; i++) {
		diet_factor = ioctl_data->ie_container.
			dpst_ie_st.factor_present[i] * 0x200 / 10000;
		REG_WRITE(vlv_config->dpst.reg.blm_hist_bin, diet_factor);
	}

	if (vlv_config->dpst.kernel_disable) {
		vlv_config->dpst.saved.is_valid = true;
		vlv_config->dpst.saved.blc_adjustment =
			ioctl_data->ie_container.dpst_blc_factor;
		return 0;
	}

	/* Backlight settings */
	vlv_config->dpst.blc_adjustment =
		ioctl_data->ie_container.dpst_blc_factor;

	 vlv_dpst_set_brightness(pipe, vlv_get_backlight(pipe));

	 /* Enable Image Enhancement Table */
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl |= IE_MOD_TABLE_ENABLE | ENHANCEMENT_MODE_MULT;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);

	return 0;
}

static void
vlv_dpst_save_luma(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	/* Only save if user mode has indeed applied valid settings which
	 * we determine by checking that the IE mod table was enabled */
	if (!(REG_READ(vlv_config->dpst.reg.blm_hist_ctl) &
				IE_MOD_TABLE_ENABLE))
		return;

	/* IE mod table entries are saved in the hardware even if the table
	 * is disabled, so we only need to save the backlight adjustment */
	vlv_config->dpst.saved.is_valid = true;
	vlv_config->dpst.saved.blc_adjustment = vlv_config->dpst.blc_adjustment;
}

static void
vlv_dpst_restore_luma(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	u32 blm_hist_ctl;
	struct intel_pipe  *pipe = config->pipes[vlv_config->dpst.pipe];

	/* Only restore if valid settings were previously saved */
	if (!vlv_config->dpst.saved.is_valid)
		return;

	vlv_config->dpst.blc_adjustment = vlv_config->dpst.saved.blc_adjustment;

	vlv_dpst_set_brightness(pipe, vlv_get_backlight(pipe));

	/* IE mod table entries are saved in the hardware even if the table
	 * is disabled, so we only need to re-enable the table */
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl |= IE_MOD_TABLE_ENABLE | ENHANCEMENT_MODE_MULT;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);
}

static int
vlv_dpst_get_bin_data(struct intel_dc_config *config,
		struct dpst_initialize_context *ioctl_data)
{
	u32 blm_hist_ctl, blm_hist_bin;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	u32 ioctl_type = ioctl_data->dpst_ioctl_type;
	int index;
	struct dpst_histogram_status *hist_stat =
		&(ioctl_data->hist_status);
	struct dpst_histogram_status_legacy *hist_stat_legacy =
		&(ioctl_data->hist_status_legacy);

	/* We may be disabled by request from kernel or user. Kernel mode
	 * disablement is without user mode knowledge. Kernel mode disablement
	 * can occur between the signal to user and user's follow-up call to
	 * retrieve the data, so return the data as usual. User mode
	 * disablement makes this an invalid call, so return error. */
	if (!vlv_config->dpst.enabled && !vlv_config->dpst.user_enable)
		return -EINVAL;
	else if (!vlv_config->dpst.enabled &&
			ioctl_type == DPST_GET_BIN_DATA) {
		/* Convey to user that dpst has been disabled
		 * from kernel. */
		ioctl_data->hist_status.dpst_disable = 1;
		return 0;
	}

	/* Setup register to access bin data from index 0 */
	blm_hist_ctl = REG_READ(vlv_config->dpst.reg.blm_hist_ctl);
	blm_hist_ctl = blm_hist_ctl & ~(BIN_REGISTER_INDEX_MASK |
						BIN_REG_FUNCTION_SELECT_IE);
	REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl, blm_hist_ctl);

	/* Read all bin data */
	for (index = 0; index < HIST_BIN_COUNT; index++) {
		blm_hist_bin = REG_READ(vlv_config->dpst.reg.blm_hist_bin);

		if (!(blm_hist_bin & BUSY_BIT)) {
			if (ioctl_type == DPST_GET_BIN_DATA)
				hist_stat->histogram_bins.status[index]
				   = blm_hist_bin &
				    vlv_config->dpst.reg.blm_hist_bin_count_mask;
			else
				hist_stat_legacy->histogram_bins.status[index]
				   = blm_hist_bin &
				    vlv_config->dpst.reg.blm_hist_bin_count_mask;
		} else {
			/* Engine is busy. Reset index to 0 to grab
			 * fresh histogram data */
			index = -1;
			blm_hist_ctl = REG_READ(
					vlv_config->dpst.reg.blm_hist_ctl);
			blm_hist_ctl = blm_hist_ctl &
				       ~BIN_REGISTER_INDEX_MASK;
			REG_WRITE(vlv_config->dpst.reg.blm_hist_ctl,
				       blm_hist_ctl);
		}
	}

	return 0;
}

static u32
vlv_dpst_get_resolution(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	struct drm_mode_modeinfo *mode;
	struct intel_pipe *pipe = config->pipes[vlv_config->dpst.pipe];

	if (!pipe || !pipe->ops || !pipe->ops->get_preferred_mode)
		return 0;

	/* Get information about current display mode */
	pipe->ops->get_preferred_mode(pipe, &mode);

	return  mode->hdisplay * mode->vdisplay;
}

static int
vlv_dpst_update_registers(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	vlv_config->dpst.reg.blm_hist_ctl = VLV_BLC_HIST_CTL(PIPE_A);
	vlv_config->dpst.reg.blm_hist_guard = VLV_BLC_HIST_GUARD(PIPE_A);
	vlv_config->dpst.reg.blm_hist_bin = VLV_BLC_HIST_BIN(PIPE_A);
	vlv_config->dpst.reg.blm_hist_bin_count_mask = BIN_COUNT_MASK_4M;
	return 0;
};

static bool
vlv_dpst_update_context(struct intel_dc_config *config)
{
	u32 cur_resolution, blm_hist_guard, gb_threshold;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	cur_resolution = vlv_dpst_get_resolution(config);

	if (!cur_resolution)
		return false;

	if (vlv_config->dpst.init_image_res != cur_resolution) {
		pr_err("DPST does not support resolution switch on the fly");
		return false;
	}

	gb_threshold = (DEFAULT_GUARDBAND_VAL * cur_resolution)/1000;

	if (vlv_dpst_update_registers(config))
		return false;

	/* Setup guardband delays and threshold */
	blm_hist_guard = REG_READ(vlv_config->dpst.reg.blm_hist_guard);
	blm_hist_guard |= (vlv_config->dpst.gb_delay << 22)
			| gb_threshold;
	REG_WRITE(vlv_config->dpst.reg.blm_hist_guard, blm_hist_guard);

	return true;
}

void
vlv_dpst_display_off()
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(g_config);
	struct intel_pipe *pipe = g_config->pipes[vlv_config->dpst.pipe];

	/* Check if dpst is user enabled*/
	if (!vlv_config->dpst.user_enable)
		return;

	mutex_lock(&vlv_config->dpst.ioctl_lock);
	/* Set the flag to reject all the subsequent DPST ioctls
	 * till the Display is turned on again
	 */
	vlv_config->dpst.display_off = true;
	vlv_dpst_disable_hist_interrupt(vlv_config);
	mutex_unlock(&vlv_config->dpst.ioctl_lock);
	/* Send a fake signal to user, so that the user can be notified
	 * to reset the dpst context, to avoid any mismatch of blc_adjusment
	 * between user and kernel on resume. */
	vlv_dpst_irq_handler(pipe);
}

void
vlv_dpst_display_on()
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(g_config);

	if (!vlv_config->dpst.user_enable
			|| !vlv_dpst_save_conn_config(g_config))
		return;

	mutex_lock(&vlv_config->dpst.ioctl_lock);

	if (vlv_dpst_update_context(g_config)
			&& !vlv_config->dpst.kernel_disable)
		vlv_dpst_enable_hist_interrupt(vlv_config);

	vlv_config->dpst.display_off = false;
	mutex_unlock(&vlv_config->dpst.ioctl_lock);
}

static int
vlv_dpst_context_init(struct dpst_initialize_context *ioctl_data)
{
	struct pid *cur_pid;
	struct intel_dc_config *config = g_config;
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);

	vlv_config->dpst.signal = ioctl_data->init_data.sig_num;
	vlv_config->dpst.gb_delay = ioctl_data->init_data.gb_delay;
	vlv_config->dpst.pipe_mismatch = false;

	/* Store info needed to talk to user mode */
	cur_pid = get_task_pid(current, PIDTYPE_PID);
	put_pid(vlv_config->dpst.pid);
	vlv_config->dpst.pid = cur_pid;
	vlv_config->dpst.signal = ioctl_data->init_data.sig_num;

	if (!vlv_dpst_save_conn_config(g_config))
		return -EINVAL;

	ioctl_data->init_data.image_res = vlv_dpst_get_resolution(config);
	vlv_config->dpst.init_image_res = ioctl_data->init_data.image_res;

	if (!vlv_dpst_update_context(config))
		return -EINVAL;

	/* Init is complete so request enablement */
	return vlv_dpst_set_user_enable(config, true);
}

void
vlv_dpst_irq_handler(struct intel_pipe *pipe)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(g_config);

	/* Check if user-mode need to be aware of this interrupt */
	if (pipe->base.idx != vlv_config->dpst.pipe)
		return;

	/* reset to false when get the interrupt on current pipe */
	vlv_config->dpst.pipe_mismatch = false;

	/* Notify user mode of the interrupt */
	if (vlv_config->dpst.pid != NULL) {
		if (kill_pid_info(vlv_config->dpst.signal, SEND_SIG_FORCED,
							vlv_config->dpst.pid)) {
			put_pid(vlv_config->dpst.pid);
			vlv_config->dpst.pid = NULL;
		}
	}
}

int
vlv_dpst_context(unsigned long args)
{
	struct dpst_initialize_context *ioctl_data = NULL;
	int ret = -EINVAL;
	struct intel_dc_config *config = g_config;
	struct vlv_dc_config *vlv_config;

	if (!g_config)
		return -EINVAL;

	vlv_config = to_vlv_dc_config(config);

	/* Can be called from multiple usermode, prevent race condition */
	mutex_lock(&vlv_config->dpst.ioctl_lock);

	ioctl_data = (struct dpst_initialize_context __user *) args;

	/* If Display is currently off (could be power gated also),
	 * don't service the ioctls other than GET_BIN_DATA
	 */
	if (vlv_config->dpst.display_off &&
		(ioctl_data->dpst_ioctl_type != DPST_GET_BIN_DATA_LEGACY &&
			ioctl_data->dpst_ioctl_type != DPST_GET_BIN_DATA)) {
		pr_err("Display is off\n");
		mutex_unlock(&vlv_config->dpst.ioctl_lock);
		return -EINVAL;
	}

	switch (ioctl_data->dpst_ioctl_type) {
	case DPST_ENABLE:
		ret = vlv_dpst_set_user_enable(config, true);
	break;

	case DPST_DISABLE:
		ret = vlv_dpst_set_user_enable(config, false);
	break;

	case DPST_INIT_DATA:
		ret = vlv_dpst_context_init(ioctl_data);
	break;

	case DPST_GET_BIN_DATA_LEGACY:
	case DPST_GET_BIN_DATA:
		ret = vlv_dpst_get_bin_data(config, ioctl_data);
	break;

	case DPST_APPLY_LUMA:
		ret = vlv_dpst_apply_luma(config, ioctl_data);
	break;

	case DPST_RESET_HISTOGRAM_STATUS:
		ret = vlv_dpst_clear_hist_interrupt(config);
	break;

	default:
		pr_err("Invalid DPST ioctl type\n");
	break;
	}

	mutex_unlock(&vlv_config->dpst.ioctl_lock);
	return ret;
}

int
vlv_dpst_set_kernel_disable(struct vlv_dc_config *vlv_config, bool disable)
{
	struct intel_dc_config *config = g_config;
	struct intel_pipe *pipe = g_config->pipes[vlv_config->dpst.pipe];
	int ret = 0;

	mutex_lock(&vlv_config->dpst.ioctl_lock);

	vlv_config->dpst.kernel_disable = disable;

	if (disable && vlv_config->dpst.enabled) {
		vlv_dpst_save_luma(config);
		ret = vlv_dpst_disable_hist_interrupt(vlv_config);
	} else if (!disable && vlv_config->dpst.user_enable) {
		ret = vlv_dpst_enable_hist_interrupt(vlv_config);
		if (!ret)
			vlv_dpst_restore_luma(config);
	}

	mutex_unlock(&vlv_config->dpst.ioctl_lock);

	/* Send a fake signal to user, so that the user can be notified
	 * to reset the dpst context, to avoid any mismatch of blc_adjusment
	 * between user and kernel on resume. */
	vlv_dpst_irq_handler(pipe);

	return ret;
}


void vlv_dpst_init(struct intel_dc_config *config)
{
	struct vlv_dc_config *vlv_config = to_vlv_dc_config(config);
	mutex_init(&vlv_config->dpst.ioctl_lock);
	g_config = config;
}

void vlv_dpst_teardown(void)
{
	g_config = NULL;
}

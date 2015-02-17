/*
 * Copyright © 2014 Intel Corporation
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
 * Authors:
 *	Deepak S <deepak.s@intel.com>
 *
 */

#include "intel_drv.h"
#include <drm/i915_adf.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "i915_adf_wrapper.h"
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

volatile bool g_adf_ready = false;

#ifdef CONFIG_ADF_INTEL

/* Standard MMIO read, non-posted */
#define SB_MRD_NP	0x00
/* Standard MMIO write, non-posted */
#define SB_MWR_NP	0x01
/* Private register read, double-word addressing, non-posted */
#define SB_CRRDDA_NP	0x06
/* Private register write, double-word addressing, non-posted */
#define SB_CRWRDA_NP	0x07

/* Global for adf driver to get at the current i915 device. */
static struct drm_i915_private *i915_adf_dev;

void i915_adf_wrapper_init(struct drm_i915_private *dev_priv)
{
	i915_adf_dev = dev_priv;

	/*
	 * rpm_get for vlv due is to HW WA, we do not do rpm_get in driver load
	 * In chv, WA is not applicable so we ignore this as rpm_get.
	 */
	if (IS_VALLEYVIEW(dev_priv->dev) && !IS_CHERRYVIEW(dev_priv->dev))
		intel_runtime_pm_get(dev_priv);
}

void i915_adf_wrapper_teardown(void)
{
	i915_adf_dev = NULL;
}

void set_adf_ready(void)
{
	g_adf_ready = true;
}

/**
 * i915_adf_driver_initialize - Adf driver calls this function to check if
 * kernel paramter for ADF Enable is set or notice
 */
int i915_adf_driver_initialize(void)
{
	if (!i915_adf_dev)
		return 0;

	return i915.enable_intel_adf;
}
EXPORT_SYMBOL(i915_adf_driver_initialize);

struct pci_dev *i915_adf_get_pci_dev(void)
{
	if (!i915_adf_dev)
		return NULL;

	return i915_adf_dev->dev->pdev;
}
EXPORT_SYMBOL(i915_adf_get_pci_dev);

struct i2c_adapter *intel_adf_get_gmbus_adapter(u8 port)
{
	/* port -1 to map pin pair to gmbus index */
	return ((port >= GMBUS_PORT_SSC) && (port <= GMBUS_PORT_DPD)) ?
		&i915_adf_dev->gmbus[port - 1].adapter : NULL;
}
EXPORT_SYMBOL(intel_adf_get_gmbus_adapter);

void intel_adf_display_rpm_get(void)
{
	struct drm_i915_private *dev_priv;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	if (IS_VALLEYVIEW(dev_priv->dev) && !IS_CHERRYVIEW(dev_priv->dev))
		intel_runtime_pm_get(dev_priv);

	intel_display_power_get(dev_priv, POWER_DOMAIN_INIT);
}
EXPORT_SYMBOL(intel_adf_display_rpm_get);

void intel_adf_display_rpm_put(void)
{
	struct drm_i915_private *dev_priv;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	if (IS_VALLEYVIEW(dev_priv->dev) && !IS_CHERRYVIEW(dev_priv->dev))
		intel_runtime_pm_put(dev_priv);

	intel_display_set_init_power(dev_priv, false);
	intel_display_power_put(dev_priv, POWER_DOMAIN_INIT);
}
EXPORT_SYMBOL(intel_adf_display_rpm_put);

/**
 * intel_adf_pci_sideband_rw - Interface to allow ADF driver read/write to intel sideband.
 */
void intel_adf_pci_sideband_rw(u32 operation, u32 port, u32 reg, u32 *val)
{
	struct drm_i915_private *dev_priv;
	u32 opcode;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	opcode = (operation == INTEL_SIDEBAND_REG_READ) ?
				SB_CRRDDA_NP : SB_CRWRDA_NP;

	mutex_lock(&dev_priv->dpio_lock);
	vlv_adf_sideband_rw(dev_priv, PCI_DEVFN(2, 0), port, opcode, reg, val);
	mutex_unlock(&dev_priv->dpio_lock);
}
EXPORT_SYMBOL(intel_adf_pci_sideband_rw);

/**
 * intel_adf_pci_sideband_rw_no_lock - Interface to allow ADF driver read/write
 * to intel sideband with no mutex protection. Mutex acquirement before calling
 * this function is callers responsibility.
 */
void intel_adf_pci_sideband_rw_no_lock(u32 operation, u32 port,
							u32 reg, u32 *val)
{
	struct drm_i915_private *dev_priv;
	u32 opcode;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	opcode = (operation == INTEL_SIDEBAND_REG_READ) ?
				SB_CRRDDA_NP : SB_CRWRDA_NP;

	vlv_adf_sideband_rw(dev_priv, PCI_DEVFN(2, 0), port, opcode, reg, val);
}
EXPORT_SYMBOL(intel_adf_pci_sideband_rw_no_lock);

/**
 * intel_adf_dpio_mutex - Interface to allow ADF driver lock/unlock dpio mutex.
 */
void intel_adf_dpio_mutex(bool acquire)
{
	struct drm_i915_private *dev_priv;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	if (acquire)
		mutex_lock(&dev_priv->dpio_lock);
	else
		mutex_unlock(&dev_priv->dpio_lock);
}
EXPORT_SYMBOL(intel_adf_dpio_mutex);

/**
 * intel_adf_dpio_sideband_rw - Interface to allow ADF driver read/write to intel sideband.
 */
void intel_adf_dpio_sideband_rw(u32 operation, u32 port, u32 reg, u32 *val)
{
	struct drm_i915_private *dev_priv;
	u32 opcode;

	if (!i915_adf_dev)
		return;

	dev_priv = i915_adf_dev;

	opcode = (operation == INTEL_SIDEBAND_REG_READ) ?
				SB_CRRDDA_NP : SB_CRWRDA_NP;

	mutex_lock(&dev_priv->dpio_lock);
	vlv_adf_sideband_rw(dev_priv, DPIO_DEVFN, port, opcode, reg, val);
	mutex_unlock(&dev_priv->dpio_lock);
}
EXPORT_SYMBOL(intel_adf_dpio_sideband_rw);

void intel_adf_get_dsi_vbt_data(void **vbt_data, struct drm_display_mode **mode)
{
	if (!i915_adf_dev)
		return;

	*vbt_data = (void *) &i915_adf_dev->vbt.dsi;
	*mode = i915_adf_dev->vbt.lfp_lvds_vbt_mode;
}
EXPORT_SYMBOL(intel_adf_get_dsi_vbt_data);

u32 intel_adf_get_pwm_vbt_data(void)
{
	return i915_adf_dev->vbt.backlight.pwm_freq_hz;
}
EXPORT_SYMBOL(intel_adf_get_pwm_vbt_data);

u8 intel_adf_get_platform_id(void)
{
	if (!i915_adf_dev)
		return 0;

	return i915_adf_dev->info.gen;
}
EXPORT_SYMBOL(intel_adf_get_platform_id);

/* To get DSI port no from VBT */
u16 intel_get_dsi_port_frm_vbt(void)
{
	return i915_adf_dev->vbt.dsi.port;
}
EXPORT_SYMBOL(intel_get_dsi_port_frm_vbt);

void intel_get_vbt_disp_conf(void **child_dev, int *child_dev_num)
{
	*child_dev = (void *)i915_adf_dev->vbt.child_dev;
	*child_dev_num = i915_adf_dev->vbt.child_dev_num;
}
EXPORT_SYMBOL(intel_get_vbt_disp_conf);

unsigned short *intel_get_vbt_pps_delays(void)
{
	return (unsigned short *)&i915_adf_dev->vbt.edp_pps;
}
EXPORT_SYMBOL(intel_get_vbt_pps_delays);

int i915_adf_simple_buffer_alloc(u16 w, u16 h, u8 bpp, struct dma_buf **dma_buf,
				u32 *offset, u32 *pitch)
{
	struct drm_i915_gem_object *obj;
	struct drm_i915_private *dev_priv;
	struct drm_device *dev;
	int size, ret = 0;

	if (!i915_adf_dev) {
		ret = -1;
		goto out;
	}

	dev_priv = i915_adf_dev;
	dev = dev_priv->dev;

	/* we don't do packed 24bpp */
	if (bpp == 24)
		bpp = 32;

	*pitch = ALIGN(w * DIV_ROUND_UP(bpp, 8), 64);

	size = *pitch * h;
	size = ALIGN(size, PAGE_SIZE);

	obj = i915_gem_alloc_object(dev, size);
	if (!obj) {
		ret = -ENOMEM;
		goto out;
	}

	ret = intel_pin_and_fence_fb_obj(dev, obj, NULL);
	if (ret)
		goto out_unref;

	*dma_buf = i915_gem_prime_export(dev, &obj->base, O_RDWR);

	return ret;

out_unref:
	drm_gem_object_unreference(&obj->base);
out:
	return ret;
}
EXPORT_SYMBOL(i915_adf_simple_buffer_alloc);

u32 intel_get_vbt_drrs_support(void)
{
	return i915_adf_dev->vbt.drrs_type;
}
EXPORT_SYMBOL(intel_get_vbt_drrs_support);

u32 intel_get_vbt_drrs_min_vrefresh(void)
{
	return i915_adf_dev->vbt.drrs_min_vrefresh;
}
EXPORT_SYMBOL(intel_get_vbt_drrs_min_vrefresh);
#else
int intel_adf_context_on_event(void) { return 0; }

/* ADF register calls for audio driver */
int adf_hdmi_audio_register(void *drv, void *had_data) { return 0; }
int adf_hdmi_audio_setup(void *callbacks, void *r_ops, void *q_ops) { return 0; }
#endif

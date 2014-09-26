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
}

void i915_adf_wrapper_teardown(void)
{
	i915_adf_dev = NULL;
}

void set_adf_ready(void)
{
	g_adf_ready = true;
}

struct pci_dev *i915_adf_get_pci_dev(void)
{
	if (!i915_adf_dev)
		return NULL;

	return i915_adf_dev->dev->pdev;
}
EXPORT_SYMBOL(i915_adf_get_pci_dev);

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


#endif

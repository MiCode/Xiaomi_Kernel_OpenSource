/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/i915_adf.h>
#include "intel_adf.h"
#include "core/intel_adf_drv.h"

#define DRIVER_NAME "intel_adf_drv"

struct intel_adf_context *adf_ctx;

static int intel_adf_init(void)
{
	struct pci_dev *i915_pci_dev;
	pr_err("ADF: %s\n", __func__);

	i915_pci_dev = i915_adf_get_pci_dev();
	adf_ctx = intel_adf_context_create(i915_pci_dev);
	if (IS_ERR(adf_ctx)) {
		pr_err("%s:failed to create ADF context\n", __func__);
		return -EINVAL;
	}

	pr_err("ADF: %s\n", __func__);
	return 0;
}

static void intel_adf_exit(void)
{
	intel_adf_context_destroy(adf_ctx);
}

late_initcall(intel_adf_init);
module_exit(intel_adf_exit);
MODULE_LICENSE("GPL and additional rights");

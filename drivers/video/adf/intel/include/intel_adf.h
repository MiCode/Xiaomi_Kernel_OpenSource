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

#ifndef INTEL_ADF_H_
#define INTEL_ADF_H_

#include "core/intel_dc_config.h"
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
#include "core/common/backlight_dev.h"
#endif
#include "intel_adf_device.h"
#include "intel_adf_interface.h"
#include "intel_adf_overlay_engine.h"
#if defined(CONFIG_ADF_FBDEV)
#include "intel_adf_fbdev.h"
#endif
#include "uapi/intel_adf.h"

struct intel_adf_context {
	struct intel_dc_config *dc_config;
	struct intel_adf_device *dev;
	struct intel_adf_interface *intfs;
	size_t n_intfs;
	struct intel_adf_overlay_engine *engs;
	size_t n_engs;
#if defined(CONFIG_ADF_FBDEV)
	struct adf_fbdev *fbdevs;
	size_t n_fbdevs;
#endif
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	struct backlight_device *bl_dev;
#endif
};

extern struct intel_adf_context *intel_adf_context_create(
	struct pci_dev *pdev, void *pg);
extern void intel_adf_context_destroy(struct intel_adf_context *ctx);
extern int intel_adf_context_on_event(void);

#endif /* INTEL_ADF_H_ */

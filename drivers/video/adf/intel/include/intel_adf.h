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
#include <video/intel_adf.h>

/* Custom IOCTL for Intel platforms */
#define INTEL_ADF_DPST_CONTEXT		ADF_IOCTL_NR_CUSTOM

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

extern struct intel_adf_context *intel_adf_context_create(struct pci_dev *pdev);
extern void intel_adf_context_destroy(struct intel_adf_context *ctx);
extern int intel_adf_context_on_event(void);
extern int intel_adf_map_dma_to_flip(unsigned long args);
extern int intel_adf_unmap_dma_to_flip(unsigned long args);
#endif /* INTEL_ADF_H_ */

/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * This module exists for the express purpose of removing memory
 * via the msm memory-remove mechanism (see
 * Documentation/devicetree/bindings/arm/msm/memory-reserve.txt). Compiling
 * this module into a kernel is essentially the means by which any
 * nodes in the device tree with compatible =
 * "qcom,msm-mem-hole" will be "activated", thus providing a
 * convenient mechanism for enabling/disabling memory removal
 * (qcom,memory-*).
 */

#include <linux/module.h>

#define MSM_MEM_HOLE_COMPAT_STR	"qcom,msm-mem-hole"

EXPORT_COMPAT(MSM_MEM_HOLE_COMPAT_STR);

/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#if !defined(MHL_DEVICE_CFG_H)
#define MHL_DEVICE_CFG_H

#include "si_app_devcap.h"

/*
 * This file contains SiI8620 driver and device configuration information
 *
 */


/*
 * Determine XDEVCAPS configurations allowed by this driver
 */

#if (INCLUDE_HID == 1)
#define XDEVCAP_VAL_DEV_ROLES		(MHL_XDC_DEV_HOST | MHL_XDC_HID_HOST)
#else
#define XDEVCAP_VAL_DEV_ROLES		MHL_XDC_DEV_HOST
#endif

#define XDEVCAP_VAL_LOG_DEV_MAPX	MHL_XDC_LD_PHONE

#endif /* if !defined(MHL_DEVICE_CFG_H) */

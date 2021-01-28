/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 Synaptics Incorporated.
 */

#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

/* Pre-defined definition */

/* Register */
#define FD_ADDR_MAX	0xE9
#define FD_ADDR_MIN	0xDD
#define FD_BYTE_COUNT	6
/*all custom setting has removed to *.dtsi and {project}_defconfig*/
#ifdef CONFIG_SYNA_RMI4_AUTO_UPDATE
#define TPD_UPDATE_FIRMWARE
#endif

#define VELOCITY_CUSTOM
#define TPD_VELOCITY_CUSTOM_X 15
#define TPD_VELOCITY_CUSTOM_Y 15

/* #define TPD_HAVE_CALIBRATION */
/* #define TPD_CALIBRATION_MATRIX  {2680,0,0,0,2760,0,0,0}; */
/* #define TPD_WARP_START */
/* #define TPD_WARP_END */

/* #define TPD_RESET_ISSUE_WORKAROUND */
/* #define TPD_MAX_RESET_COUNT 3 */
/* #define TPD_WARP_Y(y) ( TPD_Y_RES - 1 - y ) */
/* #define TPD_WARP_X(x) ( x ) */

#endif				/* TOUCHPANEL_H__ */

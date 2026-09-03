/**
 * SPRD ep device driver in host side for Spreadtrum SoCs
 *
 * Copyright (C) 2022 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used to control ep device driver in host side for
 * Spreadtrum SoCs.
 */

#ifndef __SPRD_USBPINMUX_H
#define __SPRD_USBPINMUX_H

#define	MUX_MODE	1

#if IS_ENABLED(CONFIG_SPRD_USBPINMUX)
int sprd_usbmux_check_mode(void);
#else
static inline int sprd_usbmux_check_mode(void)
{
	return 0;
}
#endif

#endif

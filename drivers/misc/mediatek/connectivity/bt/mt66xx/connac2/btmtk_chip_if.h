/**
 *  Copyright (c) 2018 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __BTMTK_CHIP_IF_H__
#define __BTMTK_CHIP_IF_H__

/* default not support */
#define SUPPORT_BT_THREAD	(0)

#ifdef CHIP_IF_USB
#include "btmtk_usb.h"
#elif defined(CHIP_IF_SDIO)
#include "btmtk_sdio.h"
#elif defined(CHIP_IF_UART)
#include "btmtk_uart.h"
#elif defined(CHIP_IF_BTIF)
#include "btmtk_btif.h"
#endif

int btmtk_cif_register(void);
int btmtk_cif_deregister(void);

#endif /* __BTMTK_CHIP_IF_H__ */

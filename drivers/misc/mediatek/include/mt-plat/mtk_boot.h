/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_BOOT_H__
#define __MTK_BOOT_H__
#include "mtk_boot_common.h"
#include "mtk_boot_reason.h"
#include "mtk_chip.h"

/*META COM port type*/
enum meta_com_type {
	META_UNKNOWN_COM = 0,
	META_UART_COM,
	META_USB_COM
};

extern enum meta_com_type get_meta_com_type(void);
extern unsigned int get_meta_com_id(void);
extern unsigned int get_meta_uart_port(void);

#endif

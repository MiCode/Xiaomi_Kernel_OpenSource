/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __UT_TUI_MAIN_H_
#define __UT_TUI_MAIN_H_

#include "teei_ioc.h"
#define UT_TUI_CLIENT_DEV "utr_tui"


struct ut_tui_data_u32 {
	unsigned int datalen;
	unsigned int data;
};
struct ut_tui_data_u64 {
	unsigned int datalen;
	unsigned long data;
};

#define UT_TUI_DISPLAY_COMMAND_U64 \
	_IOWR(UT_TUI_CLIENT_IOC_MAGIC, 0x01, struct ut_tui_data_u64)
#define UT_TUI_NOTICE_COMMAND_U64 \
	_IOWR(UT_TUI_CLIENT_IOC_MAGIC, 0x02, struct ut_tui_data_u64)


#define UT_TUI_DISPLAY_COMMAND_U32 \
	_IOWR(UT_TUI_CLIENT_IOC_MAGIC, 0x01, struct ut_tui_data_u32)
#define UT_TUI_NOTICE_COMMAND_U32 \
	_IOWR(UT_TUI_CLIENT_IOC_MAGIC, 0x02, struct ut_tui_data_u32)

extern unsigned long tui_notice_message_buff;
extern unsigned long tui_display_message_buff;
#endif

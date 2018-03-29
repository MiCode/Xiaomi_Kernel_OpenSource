/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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

#ifndef TUI_IOCTL_H_
#define TUI_IOCTL_H_



/* Response header */
struct tlc_tui_response_t {
	uint32_t	id;
	uint32_t	return_code;
};

/* Command IDs */
#define TLC_TUI_CMD_NONE                0
#define TLC_TUI_CMD_START_ACTIVITY      1
#define TLC_TUI_CMD_STOP_ACTIVITY       2

/* Return codes */
#define TLC_TUI_OK                  0
#define TLC_TUI_ERROR               1
#define TLC_TUI_ERR_UNKNOWN_CMD     2


/*
 * defines for the ioctl TUI driver module function call from user space.
 */
#define TUI_DEV_NAME		"t-base-tui"

#define TUI_IO_MAGIC		't'

#define TUI_IO_NOTIFY	_IOW(TUI_IO_MAGIC, 1, uint32_t)
#define TUI_IO_WAITCMD	_IOR(TUI_IO_MAGIC, 2, uint32_t)
#define TUI_IO_ACK	_IOW(TUI_IO_MAGIC, 3, struct tlc_tui_response_t)

#endif /* TUI_IOCTL_H_ */

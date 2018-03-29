/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCCI_TTY_H__
#define __CCCI_TTY_H__

#define  CCCI_TTY_MODEM      0
#define  CCCI_TTY_META       1
#define  CCCI_TTY_IPC         2
#define  CCCI_TTY_ICUSB      3

struct buffer_control_tty_t {
	unsigned read;
	unsigned write;
	unsigned length;
};

struct shared_mem_tty_t {
	struct buffer_control_tty_t rx_control;
	struct buffer_control_tty_t tx_control;
	unsigned char buffer[0];	/*  [RX | TX] */
	/* unsigned char            *tx_buffer; */
};

extern void ccci_reset_buffers(struct shared_mem_tty_t *shared_mem, int size);
extern int __init ccci_tty_init(int);
extern void __exit ccci_tty_exit(int);

#endif				/*  __CCCI_TTY_H__ */

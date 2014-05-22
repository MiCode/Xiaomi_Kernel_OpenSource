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

#ifndef _MHL_RBP_INPUTDEV_H_
#define _MHL_RBP_INPUTDEV_H_

struct mhl_dev_context;

#define RBP_CALL_ANSWER				0x01
#define RBP_CALL_END				0x02
#define RBP_CALL_TOGGLE				0x03
#define RBP_CALL_MUTE				0x04
#define RBP_CALL_DECLINE			0x05
#define RBP_OCTOTHORPE				0x06
#define RBP_ASTERISK				0x07
#define RBP_ROTATE_CLKWISE			0x20
#define RBP_ROTATE_COUNTERCLKWISE		0x21
#define RBP_SCREEN_PAGE_NEXT			0x30
#define RBP_SCREEN_PAGE_PREV			0x31
#define RBP_SCREEN_PAGE_UP			0x32
#define RBP_SCREEN_PAGE_DN			0x33
#define RBP_SCREEN_PAGE_LEFT			0x34
#define RBP_SCREEN_PAGE_RIGHT			0x35

int generate_rbp_input_event(struct mhl_dev_context *dev_context,
	uint8_t rbp_buttoncode);

#endif	/* #ifndef _MHL_RBP_INPUTDEV_H_ */

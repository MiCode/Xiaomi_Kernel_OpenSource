/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $34 header.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 * There is only one function $34 for each RMI4 sensor. This will be
 * the function that is used to reflash the firmware and get the
 * boot loader address and the boot image block size.
 *
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */
#ifndef _RMI_FUNCTION_34_H
#define _RMI_FUNCTION_34_H

/* define fn $34 commands */
#define WRITE_FW_BLOCK            2
#define ERASE_ALL                 3
#define READ_CONFIG_BLOCK         5
#define WRITE_CONFIG_BLOCK        6
#define ERASE_CONFIG              7
#define ENABLE_FLASH_PROG         15
#define DISABLE_FLASH_PROG        16

void FN_34_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs);
int FN_34_config(struct rmi_function_info *rmifninfo);
int FN_34_init(struct rmi_function_device *function_device);
int FN_34_detect(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
void FN_34_attention(struct rmi_function_info *rmifninfo);

#endif

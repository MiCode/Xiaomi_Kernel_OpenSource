/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $01 header.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 * There is only one function $01 for each RMI4 sensor. This will be
 * the function that is used to set sensor control and configurations
 * and check the interrupts to find the source function that is interrupting.
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
#ifndef _RMI_FUNCTION_01_H
#define _RMI_FUNCTION_01_H

void FN_01_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs);
int FN_01_config(struct rmi_function_info *rmifninfo);
int FN_01_init(struct rmi_function_device *function_device);
int FN_01_detect(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
void FN_01_attention(struct rmi_function_info *rmifninfo);
#endif

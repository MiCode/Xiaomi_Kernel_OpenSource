/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $11 header.
 * Copyright (c) 2007 - 2010, Synaptics Incorporated
 *
 * For every RMI4 function that has a data source - like 2D sensors,
 * buttons, LEDs, GPIOs, etc. - the user will create a new rmi_function_xx.c
 * file and add these functions to perform the config(), init(), report()
 * and detect() functionality. The function pointers are then srored under
 * the RMI function info and these functions will automatically be called by
 * the global config(), init(), report() and detect() functions that will
 * loop through all data sources and call the data sources functions using
 * these functions pointed to by the function ptrs.
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
#ifndef _RMI_FUNCTION_05_H
#define _RMI_FUNCTION_05_H

void FN_05_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs);
int FN_05_config(struct rmi_function_info *rmifninfo);
int FN_05_init(struct rmi_function_device *function_device);
int FN_05_detect(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
/* No attention function for F05 */
#endif

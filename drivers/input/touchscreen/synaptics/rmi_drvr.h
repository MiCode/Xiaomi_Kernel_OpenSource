/**
 *
 * Synaptics Register Mapped Interface (RMI4) RMI Driver Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
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

#include "rmi.h"

#ifndef _RMI_DRVR_H
#define _RMI_DRVR_H

#include <linux/input/rmi_platformdata.h>

/*  RMI4 Protocol Support
 */

struct rmi_phys_driver {
	char *name;
	int (*write)(struct rmi_phys_driver *physdrvr, unsigned short address,
			char data);
	int (*read)(struct rmi_phys_driver *physdrvr, unsigned short address,
			char *buffer);
	int (*write_multiple)(struct rmi_phys_driver *physdrvr,
			unsigned short address, char *buffer, int length);
	int (*read_multiple)(struct rmi_phys_driver *physdrvr, unsigned short address,
			char *buffer, int length);
	void (*attention)(struct rmi_phys_driver *physdrvr, int instance);
	bool polling_required;
	int irq;

	/* Standard kernel linked list implementation.
	*  Documentation on how to use it can be found at
	*  http://isis.poly.edu/kulesh/stuff/src/klist/.
	*/
	struct list_head drivers;
	struct rmi_sensor_driver *sensor;
	struct module *module;
};

int rmi_read(struct rmi_sensor_driver *sensor, unsigned short address, char *dest);
int rmi_write(struct rmi_sensor_driver *sensor, unsigned short address,
		unsigned char data);
int rmi_read_multiple(struct rmi_sensor_driver *sensor, unsigned short address,
		char *dest, int length);
int rmi_write_multiple(struct rmi_sensor_driver *sensor, unsigned short address,
		unsigned char *data, int length);
int rmi_register_sensor(struct rmi_phys_driver *physdrvr,
						 struct rmi_sensordata *sensordata);
int rmi_unregister_sensors(struct rmi_phys_driver *physdrvr);

/* Utility routine to set bits in a register. */
int rmi_set_bits(struct rmi_sensor_driver *sensor, unsigned short address, unsigned char bits);
/* Utility routine to clear bits in a register. */
int rmi_clear_bits(struct rmi_sensor_driver *sensor, unsigned short address, unsigned char bits);
/* Utility routine to set the value of a bit field in a register. */
int rmi_set_bit_field(struct rmi_sensor_driver *sensor, unsigned short address,
					  unsigned char field_mask, unsigned char bits);

/* Set this to 1 to turn on code used in detecting buffer leaks. */
#define RMI_ALLOC_STATS 1

#if RMI_ALLOC_STATS
extern int appallocsrmi;
extern int rfiallocsrmi;
extern int fnallocsrmi;

#define INC_ALLOC_STAT(X)   (X##allocsrmi++)
#define DEC_ALLOC_STAT(X)   \
	do { \
		if (X##allocsrmi) X##allocsrmi--; \
		else printk(KERN_DEBUG "Too many " #X " frees\n"); \
	} while (0)
#define CHECK_ALLOC_STAT(X) \
	do { \
		if (X##allocsrmi) \
			printk(KERN_DEBUG "Left over " #X " buffers: %d\n", \
					X##allocsrmi); \
	} while (0)
#else
#define INC_ALLOC_STAT(X) do { } while (0)
#define DEC_ALLOC_STAT(X) do { } while (0)
#define CHECK_ALLOC_STAT(X) do { } while (0)
#endif

#endif

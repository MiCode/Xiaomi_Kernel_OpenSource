/**
 *
 * Synaptics RMI platform data definitions for use in board files.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *############################################################################
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
 *############################################################################
 */

#if !defined(_RMI_PLATFORMDATA_H)
#define _RMI_PLATFORMDATA_H

#define RMI_F01_INDEX 0x01
#define RMI_F11_INDEX 0x11
#define RMI_F19_INDEX 0x19
#define RMI_F34_INDEX 0x34


/* A couple of structs that are useful for frequently occuring constructs,such
 * as coordinate origin offsets or coordinate clipping values.
 */
struct rmi_XY_pair {
	int x;
	int y;
};

struct rmi_range {
	int min;
	int max;
};

/* This contains sensor specific data that is not specialized to I2C or SPI.
 */
struct rmi_sensordata {
	/* This will be called from rmi_register_sensor(). You can use it
	 * to set up gpios, IRQs, and other platform specific infrastructure.
	 */
	int (*rmi_sensor_setup)(void);

	/* This will be called when the sensor is unloaded.  Use this to
	 * release gpios, IRQs, and other platform specific infrastructure.
	 */
	void (*rmi_sensor_teardown)(void);

	/* Use this to specify non-default settings on a per function basis.
	 */
	struct rmi_functiondata_list *perfunctiondata;
};

/* This contains the per-function customization for a given function.We store
 * the data this way in order to avoid allocating a large sparse array
 * typically
 * only a few functions are present on a sensor, and even fewer will be have
 * custom settings.  There is a very small penalty paid for doing a linear
 * search through the list to find a given function's data, but since the list
 * is typically very short and is searched only at system boot time, this is
 * considered acceptable.
 *
 * When adding new fields to a functiondata struct, please follow these rules:
 *     - Where possible, use 0 to indicate that the value should be defaulted.
 *       This works pretty well for bools, ints, and chars.
 *     - Where this is not practical (for example, in coordinate offsets or
 *       range clipping), use a pointer.  Set that pointer to null to indicate
 *       that the value should be defaulted.
 */
struct rmi_functiondata {
	unsigned char	function_index;
	void		*data;
};

/* This can be included in the platformdata for SPI or I2C RMI4 devices to
 * customize the settings of the functions on a given sensor.
 */
struct rmi_functiondata_list {
	unsigned char	count;	/* Number of elements in the array */
	struct		rmi_functiondata *functiondata;
};

struct rmi_f01_functiondata {
	/* What this does is product specific.  For most, but not all, RMI4
	 * devices, you can set this to true in order to request the device
	 * report data at half the usual rate.  This can be useful on slow
	 * CPUs that don't have the resources to process data at the usual
	 * rate.  However, the meaning of this field is product specific, and
	 * you should consult the product spec for your sensor to find out
	 * what this will do.
	 */
	bool	nonstandard_report_rate;
};

struct rmi_f11_functiondata {
	bool		swap_axes;
	bool		flipX;
	bool		flipY;
	int		button_height;
	struct		rmi_XY_pair *offset;
	struct		rmi_range *clipX;
	struct		rmi_range *clipY;
};

struct rmi_button_map {
	unsigned char nbuttons;
	unsigned char *map;
};

struct rmi_f19_functiondata {
	struct rmi_button_map *button_map;
};

#endif

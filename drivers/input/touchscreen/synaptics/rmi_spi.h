/**
 *
 * Register Mapped Interface SPI Physical Layer Driver Header File.
 * Copyright (C) 2008-2011, Synaptics Incorporated
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

#if !defined(_RMI_SPI_H)
#define _RMI_SPI_H

#include <linux/input/rmi_platformdata.h>

#define RMI_CHIP_VER_3	0
#define RMI_CHIP_VER_4	1

#define RMI_SUPPORT (RMI_CHIP_VER_3|RMI_CHIP_VER_4)

#define RMI4_SPI_DRIVER_NAME "rmi4_ts"
#define RMI4_SPI_DEVICE_NAME "rmi4_ts"

/** Platform-specific configuration data.
 * This structure is used by the platform-specific driver to designate
 * specific information about the hardware.  A platform client may supply
 * an array of these to the rmi_phys_spi driver.
 */
struct rmi_spi_platformdata {
	int chip;

	/* The number of the irq.  Set to zero if polling is required. */
	int irq;

	/* The type of the irq (e.g., IRQF_TRIGGER_FALLING).  Only valid if
	* irq != 0 */
	int irq_type;

	/* Use this to specify platformdata that is not I2C specific. */
	struct rmi_sensordata *sensordata;
};

#endif

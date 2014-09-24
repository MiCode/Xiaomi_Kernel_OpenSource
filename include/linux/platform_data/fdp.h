/* -------------------------------------------------------------------------
 * Copyright (C) 2013-2014 Inside Secure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------
 * Copyright (C) 2014-2016, Intel Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */

/*
 * FDP driver module.
 *
 */

#include <linux/ioctl.h>

 /*
  * If set to 0, a free major number will be allocated for the driver.
  *   If set to a value different from 0, the specified value will be used as
  * a major value
  */

#define FIELDSPEAK_MAJOR          0

/*
 * Use 'm' as magic number
 *
 * May be customized if conflict occurs
 */

#define FIELDSPEAK_IOC_MAGIC     'o'

/**
  * OPEN_NFC_IOC_RESET is used to request Open NFC driver to reset
  * the NFC Controller.
  *
  * Note : The reset IOCTL is asynchronous.
  * The Open NFC stack checks the completion of
  * the reset by selecting the fd for writing (using select()).
  */

#define FIELDSPEAK_IOC_RESET       _IO(FIELDSPEAK_IOC_MAGIC, 0)


#define FIELDSPEAK_MAX_IOCTL_VALUE   0


/* NFC controller (FDP) GPIOs */
#define NFC_RESET_GPIO               0
#define NFC_HOST_INT_GPIO            1

struct fdp_i2c_platform_data {
	unsigned int irq_gpio;
	unsigned int rst_gpio;
	unsigned int max_i2c_xfer_size;
};

/* EOF */

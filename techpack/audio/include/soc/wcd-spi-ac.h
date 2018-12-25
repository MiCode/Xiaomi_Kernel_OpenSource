/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __WCD_SPI_AC_H__
#define __WCD_SPI_AC_H__

#include <linux/types.h>
#include <linux/bitops.h>

enum wcd_spi_acc_req {
	WCD_SPI_ACCESS_REQUEST,
	WCD_SPI_ACCESS_RELEASE,
	WCD_SPI_ACCESS_MAX,
};

#define WCD_SPI_AC_DATA_TRANSFER	BIT(0)
#define WCD_SPI_AC_CONCURRENCY		BIT(1)
#define WCD_SPI_AC_REMOTE_DOWN		BIT(2)
#define WCD_SPI_AC_SVC_OFFLINE		BIT(3)
#define WCD_SPI_AC_UNINITIALIZED	BIT(4)

#if defined(CONFIG_WCD_SPI_AC)
int wcd_spi_access_ctl(struct device *dev,
		       enum wcd_spi_acc_req req,
		       u32 reason);
#else
int wcd_spi_access_ctl(struct device *dev,
		       enum wcd_spi_acc_req req,
		       u32 reason)
{
	return 0;
}
#endif /* end of CONFIG_WCD_SPI_AC */

#endif /* end of __WCD_SPI_AC_H__ */

/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#if IS_ENABLED(CONFIG_WCD_SPI_AC)
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

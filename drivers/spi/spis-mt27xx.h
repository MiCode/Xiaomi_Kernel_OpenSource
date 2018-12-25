/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/spi/spi.h>

#ifndef ____LINUX_PLATFORM_DATA_SPIS_MTK_H
#define ____LINUX_PLATFORM_DATA_SPIS_MTK_H
int mtk_spis_transfer_one(struct spi_device *spi,
			  struct spi_transfer *spis_trans);
void mtk_spis_wait_for_transfer_done(struct spi_device *spi);
int mtk_spis_create_attribute(struct device *dev);
#endif

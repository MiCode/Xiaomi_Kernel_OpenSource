/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SPI_GENI_QCOM_HEADER___
#define __SPI_GENI_QCOM_HEADER___

struct spi_geni_qcom_ctrl_data {
	u32 spi_cs_clk_delay;
	u32 spi_inter_words_delay;
};

/*2019.11.30 longcheer wanghan add start*/
/******************************************************************************
 * *This functionis for get spi_geni_master->dev
 * *spi_master: struct spi_device ->master
 * *return: spi_geni_master->dev
 ******************************************************************************/
struct device *lct_get_spi_geni_master_dev(struct spi_master *spi);
/*2019.11.30 longcheer wanghan add end*/

#endif /*__SPI_GENI_QCOM_HEADER___*/

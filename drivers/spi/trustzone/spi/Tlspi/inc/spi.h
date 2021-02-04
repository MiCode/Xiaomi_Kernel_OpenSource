/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if 1
enum tl_spi_cpol {
	TL_SPI_CPOL_0,
	TL_SPI_CPOL_1
};

enum tl_spi_cpha {
	TL_SPI_CPHA_0,
	TL_SPI_CPHA_1
};

enum tl_spi_mlsb {
	TL_SPI_LSB,
	TL_SPI_MSB
};

enum tl_spi_endian {
	TL_SPI_LENDIAN,
	TL_SPI_BENDIAN
};

enum tl_spi_transfer_mode {
	TL_FIFO_TRANSFER,
	TL_DMA_TRANSFER,
	TL_OTHER1,
	TL_OTHER2,
};

enum tl_spi_pause_mode {
	TL_PAUSE_MODE_DISABLE,
	TL_PAUSE_MODE_ENABLE
};
enum tl_spi_finish_intr {
	TL_FINISH_INTR_DIS,
	TL_FINISH_INTR_EN,
};

enum tl_spi_deassert_mode {
	TL_DEASSERT_DISABLE,
	TL_DEASSERT_ENABLE
};

enum tl_spi_ulthigh {
	TL_ULTRA_HIGH_DISABLE,
	TL_ULTRA_HIGH_ENABLE
};

enum tl_spi_tckdly {
	TL_TICK_DLY0,
	TL_TICK_DLY1,
	TL_TICK_DLY2,
	TL_TICK_DLY3
};

struct mt_tl_chip_conf {
	uint32_t tl_setuptime;
	uint32_t tl_holdtime;
	uint32_t tl_high_time;
	uint32_t tl_low_time;
	uint32_t tl_cs_idletime;
	uint32_t tl_ulthgh_thrsh;
	enum tl_spi_cpol cpol;
	enum tl_spi_cpha cpha;
	enum tl_spi_mlsb tx_mlsb;
	enum tl_spi_mlsb rx_mlsb;
	enum tl_spi_endian tx_endian;
	enum tl_spi_endian rx_endian;
	enum tl_spi_transfer_mode com_mod;
	enum tl_spi_pause_mode pause;
	enum tl_spi_finish_intr finish_intr;
	enum tl_spi_deassert_mode deassert;
	enum tl_spi_ulthigh ulthigh;
	enum tl_spi_tckdly tckdly;
};
#endif

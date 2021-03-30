/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 */

#ifndef __WCD_SPI_REGISTERS_H__
#define __WCD_SPI_REGISTERS_H__

#include <linux/regmap.h>

#define WCD_SPI_SLAVE_SANITY         (0x00)
#define WCD_SPI_SLAVE_DEVICE_ID      (0x04)
#define WCD_SPI_SLAVE_STATUS         (0x08)
#define WCD_SPI_SLAVE_CONFIG         (0x0c)
#define WCD_SPI_SLAVE_SW_RESET       (0x10)
#define WCD_SPI_SLAVE_IRQ_STATUS     (0x14)
#define WCD_SPI_SLAVE_IRQ_EN         (0x18)
#define WCD_SPI_SLAVE_IRQ_CLR        (0x1c)
#define WCD_SPI_SLAVE_IRQ_FORCE      (0x20)
#define WCD_SPI_SLAVE_TX             (0x24)
#define WCD_SPI_SLAVE_TEST_BUS_DATA  (0x2c)
#define WCD_SPI_SLAVE_TEST_BUS_CTRL  (0x30)
#define WCD_SPI_SLAVE_SW_RST_IRQ     (0x34)
#define WCD_SPI_SLAVE_CHAR_CFG       (0x38)
#define WCD_SPI_SLAVE_CHAR_DATA_MOSI (0x3c)
#define WCD_SPI_SLAVE_CHAR_DATA_CS_N (0x40)
#define WCD_SPI_SLAVE_CHAR_DATA_MISO (0x44)
#define WCD_SPI_SLAVE_TRNS_BYTE_CNT  (0x4c)
#define WCD_SPI_SLAVE_TRNS_LEN       (0x50)
#define WCD_SPI_SLAVE_FIFO_LEVEL     (0x54)
#define WCD_SPI_SLAVE_GENERICS       (0x58)
#define WCD_SPI_SLAVE_EXT_BASE_ADDR  (0x5c)
#define WCD_SPI_MAX_REGISTER         (0x5F)

#endif /* End __WCD_SPI_REGISTERS_H__ */

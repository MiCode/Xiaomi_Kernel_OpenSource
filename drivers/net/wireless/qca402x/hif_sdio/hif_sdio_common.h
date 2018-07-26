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

/* This file was originally distributed by Qualcomm Atheros, Inc.
 * before Copyright ownership was assigned to the Linux Foundation.
 */

#ifndef _HIF_SDIO_COMMON_H_
#define _HIF_SDIO_COMMON_H_

/* The purpose of these blocks is to amortize SDIO command setup time
 * across multiple bytes of data. In byte mode, we must issue a command
 * for each byte. In block mode, we issue a command (8B?) for each
 * BLOCK_SIZE bytes.
 *
 * Every mailbox read/write must be padded to this block size. If the
 * value is too large, we spend more time sending padding bytes over
 * SDIO. If the value is too small we see less benefit from amortizing
 * the cost of a command across data bytes.
 */
#define HIF_DEFAULT_IO_BLOCK_SIZE 256

#define FIFO_TIMEOUT_AND_CHIP_CONTROL 0x00000868
#define FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_OFF 0xFFFEFFFF
#define FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_ON 0x10000

/* Vendor Specific Driver Strength Settings */
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_ADDR 0xf2
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_MASK 0x0e
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_A 0x02
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_C 0x04
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_D 0x08

#endif /* _HIF_SDIO_COMMON_H_ */

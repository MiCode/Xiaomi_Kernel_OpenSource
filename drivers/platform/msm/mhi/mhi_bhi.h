/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MHI_BHI_H
#define _MHI_BHI_H
#include "mhi.h"

/* BHI Offsets */
#define BHI_BHIVERSION_MINOR                               (0x00)
#define BHI_BHIVERSION_MAJOR                               (0x04)
#define BHI_IMGADDR_LOW                                    (0x08)
#define BHI_IMGADDR_HIGH                                   (0x0C)
#define BHI_IMGSIZE                                        (0x10)
#define BHI_RSVD1                                          (0x14)
#define BHI_IMGTXDB                                        (0x18)
#define BHI_RSVD2                                          (0x1C)
#define BHI_INTVEC                                         (0x20)
#define BHI_RSVD3                                          (0x24)
#define BHI_EXECENV                                        (0x28)
#define BHI_STATUS                                         (0x2C)
#define BHI_ERRCODE                                        (0x30)
#define BHI_ERRDBG1                                        (0x34)
#define BHI_ERRDBG2                                        (0x38)
#define BHI_ERRDBG3                                        (0x3C)
#define BHI_SERIALNUM                                      (0x40)
#define BHI_SBLANTIROLLVER                                 (0x44)
#define BHI_NUMSEG                                         (0x48)
#define BHI_MSMHWID(n)                                     (0x4C + 0x4 * (n))
#define BHI_OEMPKHASH(n)                                   (0x64 + 0x4 * (n))
#define BHI_RSVD5                                          (0xC4)
#define BHI_STATUS_MASK					   (0xC0000000)
#define BHI_STATUS_SHIFT				   (30)
#define BHI_STATUS_ERROR				   (3)
#define BHI_STATUS_SUCCESS				   (2)
#define BHI_STATUS_RESET				   (0)

#define BHI_MAJOR_VERSION 0x0
#define BHI_MINOR_VERSION 0x1

#define MSMHWID_NUMDWORDS 6    /* Number of dwords that make the MSMHWID */
#define OEMPKHASH_NUMDWORDS 24 /* Number of dwords that make the OEM PK HASH */

#define BHI_READBUF_SIZE sizeof(bhi_info_type)

#define BHI_MAX_IMAGE_SIZE (256 * 1024)

#define BHI_POLL_SLEEP_TIME 1000
#define BHI_POLL_NR_RETRIES 10

int bhi_probe(struct mhi_pcie_dev_info *mhi_pcie_device);

#endif

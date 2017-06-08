/* Copyright (c) 2014, 2016-2017 The Linux Foundation. All rights reserved.
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

/* BHIE Offsets */
#define BHIE_OFFSET (0x0124) /* BHIE register space offset from BHI base */
#define BHIE_MSMSOCID_OFFS (BHIE_OFFSET + 0x0000)
#define BHIE_TXVECADDR_LOW_OFFS (BHIE_OFFSET + 0x002C)
#define BHIE_TXVECADDR_HIGH_OFFS (BHIE_OFFSET + 0x0030)
#define BHIE_TXVECSIZE_OFFS (BHIE_OFFSET + 0x0034)
#define BHIE_TXVECDB_OFFS (BHIE_OFFSET + 0x003C)
#define BHIE_TXVECDB_SEQNUM_BMSK (0x3FFFFFFF)
#define BHIE_TXVECDB_SEQNUM_SHFT (0)
#define BHIE_TXVECSTATUS_OFFS (BHIE_OFFSET + 0x0044)
#define BHIE_TXVECSTATUS_SEQNUM_BMSK (0x3FFFFFFF)
#define BHIE_TXVECSTATUS_SEQNUM_SHFT (0)
#define BHIE_TXVECSTATUS_STATUS_BMSK (0xC0000000)
#define BHIE_TXVECSTATUS_STATUS_SHFT (30)
#define BHIE_TXVECSTATUS_STATUS_RESET (0x00)
#define BHIE_TXVECSTATUS_STATUS_XFER_COMPL (0x02)
#define BHIE_TXVECSTATUS_STATUS_ERROR (0x03)
#define BHIE_RXVECADDR_LOW_OFFS (BHIE_OFFSET + 0x0060)
#define BHIE_RXVECADDR_HIGH_OFFS (BHIE_OFFSET + 0x0064)
#define BHIE_RXVECSIZE_OFFS (BHIE_OFFSET + 0x0068)
#define BHIE_RXVECDB_OFFS (BHIE_OFFSET + 0x0070)
#define BHIE_RXVECDB_SEQNUM_BMSK (0x3FFFFFFF)
#define BHIE_RXVECDB_SEQNUM_SHFT (0)
#define BHIE_RXVECSTATUS_OFFS (BHIE_OFFSET + 0x0078)
#define BHIE_RXVECSTATUS_SEQNUM_BMSK (0x3FFFFFFF)
#define BHIE_RXVECSTATUS_SEQNUM_SHFT (0)
#define BHIE_RXVECSTATUS_STATUS_BMSK (0xC0000000)
#define BHIE_RXVECSTATUS_STATUS_SHFT (30)
#define BHIE_RXVECSTATUS_STATUS_RESET (0x00)
#define BHIE_RXVECSTATUS_STATUS_XFER_COMPL (0x02)
#define BHIE_RXVECSTATUS_STATUS_ERROR (0x03)

#define BHI_MAJOR_VERSION 0x0
#define BHI_MINOR_VERSION 0x1

#define MSMHWID_NUMDWORDS 6    /* Number of dwords that make the MSMHWID */
#define OEMPKHASH_NUMDWORDS 24 /* Number of dwords that make the OEM PK HASH */

#define BHI_READBUF_SIZE sizeof(bhi_info_type)

#define BHI_MAX_IMAGE_SIZE (256 * 1024)
#define BHI_DEFAULT_ALIGNMENT (0x1000)

#define BHI_POLL_SLEEP_TIME_MS 100
#define BHI_POLL_TIMEOUT_MS 2000
#define BHIE_RDDM_DELAY_TIME_US (1000)

int bhi_probe(struct mhi_device_ctxt *mhi_dev_ctxt);
void bhi_firmware_download(struct work_struct *work);
int bhi_rddm(struct mhi_device_ctxt *mhi_dev_ctxt, bool in_panic);

#endif

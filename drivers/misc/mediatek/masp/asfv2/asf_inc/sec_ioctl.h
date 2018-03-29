/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SEC_IOCTL_H
#define SEC_IOCTL_H

/* use 's' as magic number */
#define SEC_IOC_MAGIC       's'

/* random id */
#define SEC_GET_RANDOM_ID               _IOR(SEC_IOC_MAGIC,  1, unsigned int)

/* secure boot init */
#define SEC_BOOT_INIT                   _IOR(SEC_IOC_MAGIC,  2, unsigned int)
#define SEC_BOOT_IS_ENABLED             _IOR(SEC_IOC_MAGIC,  3, unsigned int)

/* secure seccfg process */
#define SEC_SECCFG_DECRYPT              _IOR(SEC_IOC_MAGIC,  4, unsigned int)
#define SEC_SECCFG_ENCRYPT              _IOR(SEC_IOC_MAGIC,  5, unsigned int)

/* secure usbdl */
#define SEC_USBDL_IS_ENABLED            _IOR(SEC_IOC_MAGIC,  6, unsigned int)

/* HACC HW */
#define SEC_HACC_CONFIG                  _IOR(SEC_IOC_MAGIC,  7, unsigned int)
#define SEC_HACC_LOCK                    _IOR(SEC_IOC_MAGIC,  8, unsigned int)
#define SEC_HACC_UNLOCK                  _IOR(SEC_IOC_MAGIC,  9, unsigned int)
#define SEC_HACC_ENABLE_CLK              _IOR(SEC_IOC_MAGIC, 10, unsigned int)

/* secure boot check */
#define SEC_BOOT_PART_CHECK_ENABLE      _IOR(SEC_IOC_MAGIC, 11, unsigned int)
#define SEC_BOOT_NOTIFY_MARK_STATUS     _IOR(SEC_IOC_MAGIC, 12, unsigned int)
#define SEC_BOOT_NOTIFY_PASS            _IOR(SEC_IOC_MAGIC, 13, unsigned int)
#define SEC_BOOT_NOTIFY_FAIL            _IOR(SEC_IOC_MAGIC, 14, unsigned int)
#define SEC_BOOT_NOTIFY_RMSDUP_DONE     _IOR(SEC_IOC_MAGIC, 15, unsigned int)
#define SEC_BOOT_NOTIFY_STATUS          _IOR(SEC_IOC_MAGIC, 19, unsigned int)

/* rom info */
#define SEC_READ_ROM_INFO               _IOR(SEC_IOC_MAGIC, 16, unsigned int)

/* META */
#define SEC_NVRAM_HW_ENCRYPT            _IOR(SEC_IOC_MAGIC, 17, unsigned int)
#define SEC_NVRAM_HW_DECRYPT            _IOR(SEC_IOC_MAGIC, 18, unsigned int)

/* HEVC */
#define SEC_HEVC_EOP                    _IOR(SEC_IOC_MAGIC, 20, unsigned int)
#define SEC_HEVC_DOP                    _IOR(SEC_IOC_MAGIC, 21, unsigned int)

#define SEC_IOC_MAXNR       (22)

#define SEC_DEV             "/dev/sec"

#endif				/* end of SEC_IOCTL_H */

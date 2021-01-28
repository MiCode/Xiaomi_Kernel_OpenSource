/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __DEVMPU_H__
#define __DEVMPU_H__

#include <devmpu_mt.h>

// general utils
#define DEVMPU_MASK             (~(DEVMPU_PAGE_SIZE-1))
#define DEVMPU_ALIGN_UP(x)      ((x + DEVMPU_PAGE_SIZE - 1) & DEVMPU_MASK)
#define DEVMPU_ALIGN_DOWN(x)    ((x) & DEVMPU_MASK)

/**
 * Print DeviceMPU violation info.
 * @vio_addr: the physical address where the access violation is raised
 * @vio_id: the ID of the master triggering violation
 * @vio_domain: the domain of the master triggering violation
 * @vio_is_write: indicate the violation is triggered by read or write
 * @from_emimpu: relies on EMI MPU for complete violation info?
 *               (for the compatibility to old DeviceMPU design)
 * NOTE:
 * when from_emimpu is set to false, the value of vio_pa/vio_id/vio_is_write
 * are ignored as the true value will be retrieved from DeviceMPU instead of
 * EMI MPU.
 *
 * Return: 0 on success, -1 if any error
 */
int devmpu_print_violation(uint64_t vio_addr, uint32_t vio_id,
		uint32_t vio_domain, uint32_t vio_is_write, bool from_emimpu);
#endif // __DEVMPU_H__

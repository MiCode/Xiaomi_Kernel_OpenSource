/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVMPU_H__
#define __DEVMPU_H__

void devmpu_vio_clear(unsigned int emi_id);

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
		uint32_t vio_domain, uint32_t vio_rw, bool from_emimpu);

#endif /* __DEVMPU_H__ */

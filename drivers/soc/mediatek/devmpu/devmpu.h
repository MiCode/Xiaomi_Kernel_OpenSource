/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __DEVMPU_H__
#define __DEVMPU_H__

/**
 * Print DeviceMPU violation info.
 * @vio_addr: the physical address where the access violation is raised
 * @vio_id: the ID of the master triggering violation
 * @vio_domain: the domain of the master triggering violation
 * @vio_is_write: indicate the violation is triggered by read or write
 *
 * Return: 0 on success, -1 if any error
 */
int devmpu_print_violation(uint64_t vio_addr, uint32_t vio_id,
		uint32_t vio_domain, uint32_t vio_rw);

#endif /* __DEVMPU_H__ */

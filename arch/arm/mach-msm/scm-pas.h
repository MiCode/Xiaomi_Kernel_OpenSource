/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#include <mach/msm_bus_board.h>

#ifndef __MSM_SCM_PAS_H
#define __MSM_SCM_PAS_H

enum pas_id {
	PAS_MODEM,
	PAS_Q6,
	PAS_DSPS,
	PAS_TZAPPS,
	PAS_MODEM_SW,
	PAS_MODEM_FW,
	PAS_WCNSS,
	PAS_SECAPP,
	PAS_GSS,
	PAS_VIDC,
};

#ifdef CONFIG_MSM_PIL
extern int pas_init_image(enum pas_id id, const u8 *metadata, size_t size);
extern int pas_mem_setup(enum pas_id id, u32 start_addr, u32 len);
extern int pas_auth_and_reset(enum pas_id id);
extern int pas_shutdown(enum pas_id id);
extern int pas_supported(enum pas_id id);
extern void scm_pas_init(enum msm_bus_fabric_master_type id);
#else
static inline int pas_init_image(enum pas_id id, const u8 *metadata,
		size_t size)
{
	return 0;
}
static inline int pas_mem_setup(enum pas_id id, u32 start_addr, u32 len)
{
	return 0;
}
static inline int pas_auth_and_reset(enum pas_id id)
{
	return 0;
}
static inline int pas_shutdown(enum pas_id id)
{
	return 0;
}
static inline int pas_supported(enum pas_id id)
{
	return 0;
}
static inline void scm_pas_init(enum msm_bus_fabric_master_type id)
{
}
#endif

#endif

/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_SMSM_H_
#define _ARCH_ARM_MACH_MSM_SMSM_H_

#include <soc/qcom/smem.h>

enum {
	SMSM_APPS_STATE,
	SMSM_MODEM_STATE,
	SMSM_Q6_STATE,
	SMSM_APPS_DEM,
	SMSM_WCNSS_STATE = SMSM_APPS_DEM,
	SMSM_MODEM_DEM,
	SMSM_DSPS_STATE = SMSM_MODEM_DEM,
	SMSM_Q6_DEM,
	SMSM_POWER_MASTER_DEM,
	SMSM_TIME_MASTER_DEM,
};
extern uint32_t SMSM_NUM_ENTRIES;

/*
 * Ordered by when processors adopted the SMSM protocol.  May not be 1-to-1
 * with SMEM PIDs, despite initial expectations.
 */
enum {
	SMSM_APPS = SMEM_APPS,
	SMSM_MODEM = SMEM_MODEM,
	SMSM_Q6 = SMEM_Q6,
	SMSM_WCNSS,
	SMSM_DSPS,
};
extern uint32_t SMSM_NUM_HOSTS;

#define SMSM_INIT              0x00000001
#define SMSM_SMDINIT           0x00000008
#define SMSM_RPCINIT           0x00000020
#define SMSM_RESET             0x00000040
#define SMSM_TIMEWAIT          0x00000400
#define SMSM_TIMEINIT          0x00000800
#define SMSM_PROC_AWAKE        0x00001000
#define SMSM_SMD_LOOPBACK      0x00800000

#define SMSM_USB_PLUG_UNPLUG    0x00002000

#define SMSM_A2_POWER_CONTROL  0x00000002
#define SMSM_A2_POWER_CONTROL_ACK  0x00000800

#ifdef CONFIG_MSM_SMD
int smsm_change_state(uint32_t smsm_entry,
		      uint32_t clear_mask, uint32_t set_mask);

/*
 * Changes the global interrupt mask.  The set and clear masks are re-applied
 * every time the global interrupt mask is updated for callback registration
 * and de-registration.
 *
 * The clear mask is applied first, so if a bit is set to 1 in both the clear
 * mask and the set mask, the result will be that the interrupt is set.
 *
 * @smsm_entry  SMSM entry to change
 * @clear_mask  1 = clear bit, 0 = no-op
 * @set_mask    1 = set bit, 0 = no-op
 *
 * @returns 0 for success, < 0 for error
 */
int smsm_change_intr_mask(uint32_t smsm_entry,
			  uint32_t clear_mask, uint32_t set_mask);
int smsm_get_intr_mask(uint32_t smsm_entry, uint32_t *intr_mask);
uint32_t smsm_get_state(uint32_t smsm_entry);
int smsm_state_cb_register(uint32_t smsm_entry, uint32_t mask,
	void (*notify)(void *, uint32_t old_state, uint32_t new_state),
	void *data);
int smsm_state_cb_deregister(uint32_t smsm_entry, uint32_t mask,
	void (*notify)(void *, uint32_t, uint32_t), void *data);

#else
static inline int smsm_change_state(uint32_t smsm_entry,
		      uint32_t clear_mask, uint32_t set_mask)
{
	return -ENODEV;
}

/*
 * Changes the global interrupt mask.  The set and clear masks are re-applied
 * every time the global interrupt mask is updated for callback registration
 * and de-registration.
 *
 * The clear mask is applied first, so if a bit is set to 1 in both the clear
 * mask and the set mask, the result will be that the interrupt is set.
 *
 * @smsm_entry  SMSM entry to change
 * @clear_mask  1 = clear bit, 0 = no-op
 * @set_mask    1 = set bit, 0 = no-op
 *
 * @returns 0 for success, < 0 for error
 */
static inline int smsm_change_intr_mask(uint32_t smsm_entry,
			  uint32_t clear_mask, uint32_t set_mask)
{
	return -ENODEV;
}

static inline int smsm_get_intr_mask(uint32_t smsm_entry, uint32_t *intr_mask)
{
	return -ENODEV;
}
static inline uint32_t smsm_get_state(uint32_t smsm_entry)
{
	return 0;
}
static inline int smsm_state_cb_register(uint32_t smsm_entry, uint32_t mask,
	void (*notify)(void *, uint32_t old_state, uint32_t new_state),
	void *data)
{
	return -ENODEV;
}
static inline int smsm_state_cb_deregister(uint32_t smsm_entry, uint32_t mask,
	void (*notify)(void *, uint32_t, uint32_t), void *data)
{
	return -ENODEV;
}
static inline void smsm_reset_modem(unsigned mode)
{
}
static inline void smsm_reset_modem_cont(void)
{
}
static inline void smd_sleep_exit(void)
{
}
static inline int smsm_check_for_modem_crash(void)
{
	return -ENODEV;
}
#endif
#endif

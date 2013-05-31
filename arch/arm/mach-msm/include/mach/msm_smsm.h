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

#include <linux/notifier.h>

#include <mach/msm_smem.h>

#if defined(CONFIG_MSM_N_WAY_SMSM)
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
#else
enum {
	SMSM_APPS_STATE = 1,
	SMSM_MODEM_STATE = 3,
	SMSM_NUM_ENTRIES,
};
#endif

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
#define SMSM_OSENTERED         0x00000002
#define SMSM_SMDWAIT           0x00000004
#define SMSM_SMDINIT           0x00000008
#define SMSM_RPCWAIT           0x00000010
#define SMSM_RPCINIT           0x00000020
#define SMSM_RESET             0x00000040
#define SMSM_RSA               0x00000080
#define SMSM_RUN               0x00000100
#define SMSM_PWRC              0x00000200
#define SMSM_TIMEWAIT          0x00000400
#define SMSM_TIMEINIT          0x00000800
#define SMSM_PROC_AWAKE        0x00001000
#define SMSM_WFPI              0x00002000
#define SMSM_SLEEP             0x00004000
#define SMSM_SLEEPEXIT         0x00008000
#define SMSM_OEMSBL_RELEASE    0x00010000
#define SMSM_APPS_REBOOT       0x00020000
#define SMSM_SYSTEM_POWER_DOWN 0x00040000
#define SMSM_SYSTEM_REBOOT     0x00080000
#define SMSM_SYSTEM_DOWNLOAD   0x00100000
#define SMSM_PWRC_SUSPEND      0x00200000
#define SMSM_APPS_SHUTDOWN     0x00400000
#define SMSM_SMD_LOOPBACK      0x00800000
#define SMSM_RUN_QUIET         0x01000000
#define SMSM_MODEM_WAIT        0x02000000
#define SMSM_MODEM_BREAK       0x04000000
#define SMSM_MODEM_CONTINUE    0x08000000
#define SMSM_SYSTEM_REBOOT_USR 0x20000000
#define SMSM_SYSTEM_PWRDWN_USR 0x40000000
#define SMSM_UNKNOWN           0x80000000

#define SMSM_WKUP_REASON_RPC	0x00000001
#define SMSM_WKUP_REASON_INT	0x00000002
#define SMSM_WKUP_REASON_GPIO	0x00000004
#define SMSM_WKUP_REASON_TIMER	0x00000008
#define SMSM_WKUP_REASON_ALARM	0x00000010
#define SMSM_WKUP_REASON_RESET	0x00000020
#define SMSM_USB_PLUG_UNPLUG    0x00002000
#define SMSM_A2_RESET_BAM      0x00004000

#define SMSM_VENDOR             0x00020000

#define SMSM_A2_POWER_CONTROL  0x00000002
#define SMSM_A2_POWER_CONTROL_ACK  0x00000800

#define SMSM_WLAN_TX_RINGS_EMPTY 0x00000200
#define SMSM_WLAN_TX_ENABLE	0x00000400

#define SMSM_SUBSYS2AP_STATUS         0x00008000


enum {
	SMEM_APPS_Q6_SMSM = 3,
	SMEM_Q6_APPS_SMSM = 5,
	SMSM_NUM_INTR_MUX = 8,
};

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
void smsm_print_sleep_info(uint32_t sleep_delay, uint32_t sleep_limit,
	uint32_t irq_mask, uint32_t wakeup_reason, uint32_t pending_irqs);
void smsm_reset_modem(unsigned mode);
void smsm_reset_modem_cont(void);
void smd_sleep_exit(void);


int smsm_check_for_modem_crash(void);

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
static inline void smsm_print_sleep_info(uint32_t sleep_delay,
	uint32_t sleep_limit, uint32_t irq_mask, uint32_t wakeup_reason,
	uint32_t pending_irqs)
{
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

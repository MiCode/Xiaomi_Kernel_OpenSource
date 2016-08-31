/*
 * arch/arm/mach-tegra/baseband-xmm-power.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef BASEBAND_XMM_POWER_H
#define BASEBAND_XMM_POWER_H

#include <linux/pm.h>
#include <linux/suspend.h>

#define TEGRA_EHCI_DEVICE "/sys/devices/platform/tegra-ehci.1/ehci_power"

#define XMM_MODEM_VER_1121	0x1121
#define XMM_MODEM_VER_1130	0x1130

/* shared between baseband-xmm-* modules so they can agree on same
 * modem configuration
 */
extern unsigned long modem_ver;
extern unsigned long modem_flash;
extern unsigned long modem_pm;

enum baseband_type {
	BASEBAND_XMM,
};

struct baseband_power_platform_data {
	enum baseband_type baseband_type;
	struct platform_device* (*hsic_register)(struct platform_device *);
	void (*hsic_unregister)(struct platform_device **);
	struct platform_device *ehci_device;
	union {
		struct {
			int mdm_reset;
			int mdm_on;
			int ap2mdm_ack;
			int mdm2ap_ack;
			int ap2mdm_ack2;
			int mdm2ap_ack2;
			struct platform_device *device;
		} generic;
		struct {
			int bb_rst;
			int bb_on;
			int ipc_bb_wake;
			int ipc_ap_wake;
			int ipc_hsic_active;
			int ipc_hsic_sus_req;
			struct platform_device *hsic_device;
		} xmm;
	} modem;
};

enum baseband_xmm_power_work_state_t {
	BBXMM_WORK_UNINIT,
	BBXMM_WORK_INIT,
	/* initialize flash modem */
	BBXMM_WORK_INIT_FLASH_STEP1,
	/* initialize flash (with power management support) modem */
	BBXMM_WORK_INIT_FLASH_PM_STEP1,
	/* initialize flashless (with power management support) modem */
	BBXMM_WORK_INIT_FLASHLESS_PM_STEP1,
	BBXMM_WORK_INIT_FLASHLESS_PM_STEP2,
	BBXMM_WORK_INIT_FLASHLESS_PM_STEP3,
	BBXMM_WORK_INIT_FLASHLESS_PM_STEP4,
};

struct xmm_power_data {
	/* xmm modem state */
	enum baseband_xmm_power_work_state_t state;
	struct baseband_power_platform_data *pdata;
	struct work_struct work;
	struct platform_device *hsic_device;
	wait_queue_head_t bb_wait;
	/* host wakeup gpio state*/
	unsigned int hostwake;
};

enum baseband_xmm_powerstate_t {
	BBXMM_PS_L0	= 0,
	BBXMM_PS_L2	= 1,
	BBXMM_PS_L0TOL2	= 2,
	BBXMM_PS_L2TOL0	= 3,
	BBXMM_PS_UNINIT	= 4,
	BBXMM_PS_INIT	= 5,
	BBXMM_PS_L3	= 6,
	BBXMM_PS_LAST	= -1,
};

enum ipc_ap_wake_state_t {
	IPC_AP_WAKE_UNINIT,
	IPC_AP_WAKE_IRQ_READY,
	IPC_AP_WAKE_INIT1,
	IPC_AP_WAKE_INIT2,
	IPC_AP_WAKE_L,
	IPC_AP_WAKE_H,
};

irqreturn_t xmm_power_ipc_ap_wake_irq(int value);

void baseband_xmm_set_power_status(unsigned int status);
extern struct xmm_power_data xmm_power_drv_data;

#endif  /* BASEBAND_XMM_POWER_H */

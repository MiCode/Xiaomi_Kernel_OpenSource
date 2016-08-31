/*
 * arch/arm/mach-tegra/include/mach/tegra_bb.h
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
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

#ifndef __MACH_TEGRA_BB_H
#define __MACH_TEGRA_BB_H

#define EMC_DSR		(1 << 0)

struct tegra_bb_platform_data {
	/* Baseband->AP IRQ */
	unsigned int bb_irq;
	unsigned int mem_req_soon;
};

/*
 * tegra_bb_register_ipc: register callback for IPC notification
 * @param struct tegra_bb device pointer
 * @param void (*cb)(voif *data) callback function
 * @param void * callback data
 * @return none
 */
void tegra_bb_register_ipc(struct platform_device *dev,
			   void (*cb)(void *data), void *cb_data);
/*
 * tegra_bb_generate_ipc: generate IPC
 * @param struct tegra_bb device pointer
 * @return none
 */
void tegra_bb_generate_ipc(struct platform_device *dev);

/*
 * tegra_bb_abort_ipc: Clear AP2BB irq
 * @param struct tegra_bb device pointer
 * @return none
 */
void tegra_bb_abort_ipc(struct platform_device *dev);

/*
 * tegra_bb_clear_ipc: clear BB2AP IPC irq status
 * @param struct tegra_bb device pointer
 * @return none
 */
void tegra_bb_clear_ipc(struct platform_device *dev);

/*
 * tegra_bb_check_ipc: check if IRQ is cleared by BB
 * @param struct tegra_bb device pointer
 * @return 1 if IRQ is cleared - 0 otherwise
 */
int tegra_bb_check_ipc(struct platform_device *dev);

/*
 * tegra_bb_check_bb2ap_ipc: check if BB IPC IRQ is pending
 * @param none
 * @return 1 if IRQ is pending - 0 otherwise
 */
int tegra_bb_check_bb2ap_ipc(void);

/*
 * tegra_bb_set_ipc_serial: store SHM serial number for sysfs access
 * @param struct tegra_bb device pointer
 * @param serial pointer to SHM serial
 * @return none
 */
void tegra_bb_set_ipc_serial(struct platform_device *pdev, char *serial);

/* tegra_bb_set_emc_floor: set EMC frequency floor for BBC
 * @param unsigned int frequency
 * @return none
 */
void tegra_bb_set_emc_floor(struct device *dev, unsigned long freq, u32 flags);

#endif

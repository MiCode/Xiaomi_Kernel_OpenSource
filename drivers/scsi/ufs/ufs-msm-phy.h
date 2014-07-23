/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef UFS_MSM_PHY_H_
#define UFS_MSM_PHY_H_

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/msm-bus.h>

#include "ufshcd.h"
#include "unipro.h"
#include "ufs-msm.h"

#define UFS_MSM_PHY_CAL_ENTRY(reg, val)	\
	{				\
		.reg_offset = reg,	\
		.cfg_value = val,	\
	}

#define UFS_MSM_PHY_NAME_LEN	30

struct ufs_msm_phy_stored_attributes {
	u32 att;
	u32 value;
};

struct ufs_msm_phy_calibration {
	u32 reg_offset;
	u32 cfg_value;
};

struct ufs_msm_phy {
	struct list_head list;
	struct device *dev;
	void __iomem *mmio;
	struct clk *tx_iface_clk;
	struct clk *rx_iface_clk;
	bool is_iface_clk_enabled;
	struct clk *ref_clk_src;
	struct clk *ref_clk_parent;
	struct clk *ref_clk;
	bool is_ref_clk_enabled;
	struct ufs_msm_phy_vreg vdda_pll;
	struct ufs_msm_phy_vreg vdda_phy;
	unsigned int quirks;
	u8 host_ctrl_rev_major;
	u16 host_ctrl_rev_minor;
	u16 host_ctrl_rev_step;

	/*
	 * As part of UFS power management, UFS link would be put in hibernate
	 * and UFS device would be put in SLEEP mode as part of runtime/system
	 * suspend callback. But when system goes into suspend with VDD
	 * minimization, UFS PHY states are being reset which means UFS link
	 * hibernate exit command on system resume would fail.
	 * If this quirk is enabled then above issue is workaround by saving
	 * the UFS PHY state information before system goes into suspend and
	 * restoring the saved state information during system resume but
	 * before executing the hibern8 exit command.
	 * Note that this quirk will help restoring the PHY state if even when
	 * link in not kept in hibern8 during suspend.
	 *
	 * Here is the list of steps to save/restore the configuration:
	 * Before entering into system suspend:
	 *	1. Read Critical PCS SWI Registers  + less critical PHY CSR
	 *	2. Read RMMI Attributes
	 * Enter into system suspend
	 * After exiting from system suspend:
	 *	1. Set UFS_PHY_SOFT_RESET bit in UFS_CFG1 register of the UFS
	 *	   Controller
	 *	2. Write 0x01 to the UFS_PHY_POWER_DOWN_CONTROL register in the
	 *	   UFS PHY
	 *	3. Write back the values of the PHY SWI registers
	 *	4. Clear UFS_PHY_SOFT_RESET bit in UFS_CFG1 register of the UFS
	 *	   Controller
	 *	5. Write 0x01 to the UFS_PHY_PHY_START in the UFS PHY. This will
	 *	   start the PLL calibration and bring-up of the PHY.
	 *	6. Write back the values to the PHY RMMI Attributes
	 *	7. Wait for UFS_PHY_PCS_READY_STATUS[0] to be '1'
	 */
	#define MSM_UFS_PHY_QUIRK_CFG_RESTORE		(1 << 0)

	/*
	* If UFS PHY power down is deasserted and power is restored to analog
	* circuits, the rx_sigdet can glitch. If the glitch is wide enough,
	* it can trigger the digital logic to think it saw a DIF-N and cause
	* it to exit Hibern8. Disabling the rx_sigdet during power-up masks
	* the glitch.
	*/
	#define MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE	(1 << 1)

	/*
	* If UFS link is put into Hibern8 and if UFS PHY analog hardware is
	* power collapsed (by clearing UFS_PHY_POWER_DOWN_CONTROL), Hibern8
	* exit might fail even after powering on UFS PHY analog hardware.
	* Enabling this quirk will help to solve above issue by doing
	* custom PHY settings just before PHY analog power collapse.
	*/
	#define MSM_UFS_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE	(1 << 2)

	char name[UFS_MSM_PHY_NAME_LEN];
	struct ufs_msm_phy_calibration *cached_regs;
	int cached_regs_table_size;
	bool is_powered_on;
	struct ufs_msm_phy_specific_ops *phy_spec_ops;
};

/**
 * struct ufs_msm_phy_specific_ops - set of pointers to functions which have a
 * specific implementation per phy. Each UFS phy, should implement
 * those functions according to its spec and requirements
 * @calibrate_phy: pointer to a function that calibrate the phy
 * @start_serdes: pointer to a function that starts the serdes
 * @save_configuration: pointer to a function that saves phy
 * configuration
 * @is_physical_coding_sublayer_ready: pointer to a function that
 * checks pcs readiness
 * @set_tx_lane_enable: pointer to a function that enable tx lanes
 * @power_control: pointer to a function that controls analog rail of phy
 * and writes to QSERDES_RX_SIGDET_CNTRL attribute
 */
struct ufs_msm_phy_specific_ops {
	int (*calibrate_phy) (struct ufs_msm_phy *phy);
	void (*start_serdes) (struct ufs_msm_phy *phy);
	void (*save_configuration)(struct ufs_msm_phy *phy);
	int (*is_physical_coding_sublayer_ready) (struct ufs_msm_phy *phy);
	void (*set_tx_lane_enable) (struct ufs_msm_phy *phy, u32 val);
	void (*power_control) (struct ufs_msm_phy *phy, bool val);
};

int ufs_msm_phy_init_vreg(struct phy *phy,
		struct ufs_msm_phy_vreg *vreg, const char *name);
int ufs_msm_phy_cfg_vreg(struct phy *phy,
			struct ufs_msm_phy_vreg *vreg, bool on);
int ufs_msm_phy_enable_vreg(struct phy *phy,
			struct ufs_msm_phy_vreg *vreg);
int ufs_msm_phy_disable_vreg(struct phy *phy,
			struct ufs_msm_phy_vreg *vreg);
int ufs_msm_phy_enable_ref_clk(struct phy *phy);
void ufs_msm_phy_disable_ref_clk(struct phy *phy);
int ufs_msm_phy_enable_iface_clk(struct phy *phy);
void ufs_msm_phy_disable_iface_clk(struct phy *phy);
void ufs_msm_phy_restore_swi_regs(struct phy *phy);
int ufs_msm_phy_link_startup_post_change(struct phy *phy,
			struct ufs_hba *hba);
int ufs_msm_phy_base_init(struct platform_device *pdev,
			struct ufs_msm_phy *ufs_msm_phy_ops);
int ufs_msm_phy_is_cfg_restore_quirk_enabled(struct phy *phy);
struct ufs_msm_phy *get_ufs_msm_phy(struct phy *generic_phy);
int ufs_msm_phy_start_serdes(struct phy *generic_phy);
int ufs_msm_phy_set_tx_lane_enable(struct phy *generic_phy, u32 tx_lanes);
int ufs_msm_phy_calibrate_phy(struct phy *generic_phy);
int ufs_msm_phy_is_pcs_ready(struct phy *generic_phy);
int ufs_msm_phy_save_configuration(struct phy *generic_phy);
void ufs_msm_phy_save_controller_version(struct phy *generic_phy,
			u8 major, u16 minor, u16 step);
int ufs_msm_phy_power_on(struct phy *generic_phy);
int ufs_msm_phy_power_off(struct phy *generic_phy);
int ufs_msm_phy_exit(struct phy *generic_phy);
int ufs_msm_phy_init_clks(struct phy *generic_phy,
			struct ufs_msm_phy *phy_common);
int ufs_msm_phy_init_vregulators(struct phy *generic_phy,
			struct ufs_msm_phy *phy_common);
int ufs_msm_phy_remove(struct phy *generic_phy,
		       struct ufs_msm_phy *ufs_msm_phy);
struct phy *ufs_msm_phy_generic_probe(struct platform_device *pdev,
			struct ufs_msm_phy *common_cfg,
			struct phy_ops *ufs_msm_phy_gen_ops,
			struct ufs_msm_phy_specific_ops *phy_spec_ops);
int ufs_msm_phy_calibrate(struct ufs_msm_phy *ufs_msm_phy,
			struct ufs_msm_phy_calibration *tbl_A, int tbl_size_A,
			struct ufs_msm_phy_calibration *tbl_B, int tbl_size_B,
			int rate);
#endif

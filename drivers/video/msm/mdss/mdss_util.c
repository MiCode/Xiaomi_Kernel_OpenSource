
/* Copyright (c) 2007-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/interrupt.h>
#include "mdss_mdp.h"

struct mdss_hw *mdss_irq_handlers[MDSS_MAX_HW_BLK];
static DEFINE_SPINLOCK(mdss_lock);

int mdss_register_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (!hw || hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return -EINVAL;

	ndx_bit = BIT(hw->hw_ndx);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (!mdss_irq_handlers[hw->hw_ndx])
		mdss_irq_handlers[hw->hw_ndx] = hw;
	else
		pr_err("panel %d's irq at %pK is already registered\n",
			hw->hw_ndx, hw->irq_handler);
	spin_unlock_irqrestore(&mdss_lock, irq_flags);

	return 0;
}

void mdss_enable_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	if (!mdss_irq_handlers[hw->hw_ndx]) {
		pr_err("failed. First register the irq then enable it.\n");
		return;
	}

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Enable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			hw->irq_info->irq_ena, hw->irq_info->irq_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (hw->irq_info->irq_mask & ndx_bit) {
		pr_debug("MDSS HW ndx=%d is already set, mask=%x\n",
				hw->hw_ndx, hw->irq_info->irq_mask);
	} else {
		hw->irq_info->irq_mask |= ndx_bit;
		if (!hw->irq_info->irq_ena) {
			hw->irq_info->irq_ena = true;
			enable_irq(hw->irq_info->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}

void mdss_disable_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			hw->irq_info->irq_ena, hw->irq_info->irq_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (!(hw->irq_info->irq_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set\n", hw->hw_ndx);
	} else {
		hw->irq_info->irq_mask &= ~ndx_bit;
		if (hw->irq_info->irq_mask == 0) {
			hw->irq_info->irq_ena = false;
			disable_irq_nosync(hw->irq_info->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}

/* called from interrupt context */
void mdss_disable_irq_nosync(struct mdss_hw *hw)
{
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			hw->irq_info->irq_ena, hw->irq_info->irq_mask);

	spin_lock(&mdss_lock);
	if (!(hw->irq_info->irq_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set\n", hw->hw_ndx);
	} else {
		hw->irq_info->irq_mask &= ~ndx_bit;
		if (hw->irq_info->irq_mask == 0) {
			hw->irq_info->irq_ena = false;
			disable_irq_nosync(hw->irq_info->irq);
		}
	}
	spin_unlock(&mdss_lock);
}

int mdss_irq_dispatch(u32 hw_ndx, int irq, void *ptr)
{
	struct mdss_hw *hw;
	int rc = -ENODEV;

	spin_lock(&mdss_lock);
	hw = mdss_irq_handlers[hw_ndx];
	spin_unlock(&mdss_lock);

	if (hw)
		rc = hw->irq_handler(irq, hw->ptr);

	return rc;
}

void mdss_enable_irq_wake(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	if (!mdss_irq_handlers[hw->hw_ndx]) {
		pr_err("failed. First register the irq then enable it.\n");
		return;
	}

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Enable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			hw->irq_info->irq_wake_ena,
			hw->irq_info->irq_wake_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (hw->irq_info->irq_wake_mask & ndx_bit) {
		pr_debug("MDSS HW ndx=%d is already set, mask=%x\n",
				hw->hw_ndx, hw->irq_info->irq_wake_mask);
	} else {
		hw->irq_info->irq_wake_mask |= ndx_bit;
		if (!hw->irq_info->irq_wake_ena) {
			hw->irq_info->irq_wake_ena = true;
			enable_irq_wake(hw->irq_info->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}

void mdss_disable_irq_wake(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			hw->irq_info->irq_wake_ena,
			hw->irq_info->irq_wake_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (!(hw->irq_info->irq_wake_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set\n", hw->hw_ndx);
	} else {
		hw->irq_info->irq_wake_mask &= ~ndx_bit;
		if (hw->irq_info->irq_wake_ena) {
			hw->irq_info->irq_wake_ena = false;
			disable_irq_wake(hw->irq_info->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}

static bool check_display(char *param_string)
{
	char *str = NULL;
	bool display_disable = false;

	str = strnstr(param_string, ";", MDSS_MAX_PANEL_LEN);
	if (!str)
		return display_disable;

	str = strnstr(str, ":", MDSS_MAX_PANEL_LEN);
	if (!str)
		return display_disable;
	else if (str[1] == '1')
		display_disable = 1;

	return display_disable;
}

struct mdss_util_intf mdss_util = {
	.register_irq = mdss_register_irq,
	.enable_irq = mdss_enable_irq,
	.disable_irq = mdss_disable_irq,
	.enable_wake_irq = mdss_enable_irq_wake,
	.disable_wake_irq = mdss_disable_irq_wake,
	.disable_irq_nosync = mdss_disable_irq_nosync,
	.irq_dispatch = mdss_irq_dispatch,
	.get_iommu_domain = NULL,
	.iommu_attached = NULL,
	.iommu_ctrl = NULL,
	.bus_bandwidth_ctrl = NULL,
	.bus_scale_set_quota = NULL,
	.panel_intf_type = NULL,
	.panel_intf_status = NULL,
	.mdp_probe_done = false,
	.param_check = check_display,
	.display_disabled = false
};

struct mdss_util_intf *mdss_get_util_intf()
{
	return &mdss_util;
}
EXPORT_SYMBOL(mdss_get_util_intf);

/* This routine should only be called from interrupt context */
bool mdss_get_irq_enable_state(struct mdss_hw *hw)
{
	bool is_irq_enabled;

	spin_lock(&mdss_lock);
	is_irq_enabled = hw->irq_info->irq_ena;
	spin_unlock(&mdss_lock);

	return is_irq_enabled;
}

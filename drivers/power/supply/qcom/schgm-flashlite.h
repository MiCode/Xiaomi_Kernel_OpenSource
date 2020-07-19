/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SCHGM_FLASHLITE_H__
#define __SCHGM_FLASHLITE_H__

#include <linux/bitops.h>

#define SCHGM_FLASH_BASE			0xA600

#define SCHGM_FLASH_STATUS_2_REG		(SCHGM_FLASH_BASE + 0x07)
#define VREG_OK_BIT				BIT(4)

#define SCHGM_FLASH_STATUS_3_REG		(SCHGM_FLASH_BASE + 0x08)
#define FLASH_STATE_MASK			GENMASK(2, 0)
#define FLASH_ERROR_VAL				0x7

#define SCHGM_FLASH_INT_RT_STS_REG		(SCHGM_FLASH_BASE + 0x10)

#define SCHGM_FLASH_STATUS_5_REG		(SCHGM_FLASH_BASE + 0x0B)

#define SCHGM_FLASH_S2_LATCH_RESET_CMD_REG	(SCHGM_FLASH_BASE + 0x44)
#define FLASH_S2_LATCH_RESET_BIT		BIT(0)

#define SCHGM_FLASH_CONTROL_REG			(SCHGM_FLASH_BASE + 0x60)
#define SOC_LOW_FOR_FLASH_EN_BIT		BIT(7)
#define TEMP_DIE_REG_H_DERATE_EN_BIT		BIT(3)
#define TEMP_DIE_REG_L_DERATE_EN_BIT		BIT(2)

#define SCHGM_TORCH_PRIORITY_CONTROL_REG	(SCHGM_FLASH_BASE + 0x63)
#define TORCH_PRIORITY_CONTROL_BIT		BIT(0)

#define SCHG_L_FLASH_FLASH_FAULT_CFG		(SCHGM_FLASH_BASE + 0x64)
#define CFG_FLASH_USB_COLLAPSE_BIT		BIT(7)

#define SCHGM_SOC_BASED_FLASH_DERATE_TH_CFG_REG	(SCHGM_FLASH_BASE + 0x67)

#define SCHGM_SOC_BASED_FLASH_DISABLE_TH_CFG_REG \
						(SCHGM_FLASH_BASE + 0x68)

enum torch_mode {
	TORCH_BUCK_MODE = 0,
	TORCH_BOOST_MODE,
};

int schgm_flashlite_get_vreg_ok(struct smb_charger *chg, int *val);
void schgm_flashlite_torch_priority(struct smb_charger *chg,
					enum torch_mode mode);
int schgm_flashlite_config_usbin_collapse(struct smb_charger *chg,
						bool enable);
int schgm_flashlite_init(struct smb_charger *chg);
bool is_flashlite_active(struct smb_charger *chg);

irqreturn_t schgm_flashlite_default_irq_handler(int irq, void *data);
irqreturn_t schgm_flashlite_ilim2_irq_handler(int irq, void *data);
irqreturn_t schgm_flashlite_state_change_irq_handler(int irq, void *data);
#endif /* __SCHGM_FLASHLITE_H__ */

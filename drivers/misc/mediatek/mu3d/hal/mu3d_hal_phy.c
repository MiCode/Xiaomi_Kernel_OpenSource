/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mu3d_hal_osal.h"
#include "mu3d_hal_phy.h"
#include "mu3d_hal_usb_drv.h"
#include "mtk-phy.h"

/**
 * mu3d_hal_phy_scan - u3 phy clock phase scan
 *
 */
DEV_INT32 mu3d_hal_phy_scan(DEV_INT32 latch_val, DEV_UINT8 driving)
{
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	DEV_INT32 count, fset_phase_val, recov_cnt, link_error_count, U0_count;
	DEV_UINT8 phase_val;
	/* DEV_UINT8 driving; */

	/* disable ip power down,disable U2/U3 ip power down. */
	mu3d_hal_ssusb_en();
	/* mu3d_hal_pdn_dis(); */
	u3phy_ops->change_pipe_phase(u3phy, 0, 0);
	os_writel(U3D_PIPE_LATCH_SELECT, latch_val);	/* set tx/rx latch sel */

	/* driving = 2; */
	u3phy_ops->change_pipe_phase(u3phy, driving, 0);
	phase_val = 0;
	count = 0;
	fset_phase_val = TRUE;

	while (TRUE) {

		if (fset_phase_val) {
			u3phy_ops->change_pipe_phase(u3phy, driving, phase_val);
			mu3d_hal_rst_dev();
			os_ms_delay(50);
			os_writel(U3D_USB3_CONFIG, USB3_EN);
			os_writel(U3D_PIPE_LATCH_SELECT, latch_val);	/* set tx/rx latch sel */
			fset_phase_val = FALSE;
			U0_count = 0;
			link_error_count = 0;
			recov_cnt = 0;
			count = 0;
		}
		os_ms_delay(50);
		count++;
		recov_cnt = os_readl(U3D_RECOVERY_COUNT);	/* read U0 recovery count */
		link_error_count = os_readl(U3D_LINK_ERR_COUNT);	/* read link error count */
		if ((os_readl(U3D_LINK_STATE_MACHINE) & LTSSM) == STATE_U0_STATE) {	/* enter U0 state */
			U0_count++;
		}
		if (U0_count > ENTER_U0_TH) {	/* link up */
			os_ms_delay(1000);	/* 1s */
			recov_cnt = os_readl(U3D_RECOVERY_COUNT);
			link_error_count = os_readl(U3D_LINK_ERR_COUNT);
			os_writel(U3D_RECOVERY_COUNT, CLR_RECOV_CNT);	/* clear recovery count */
			os_writel(U3D_LINK_ERR_COUNT, CLR_LINK_ERR_CNT);	/* clear link error count */
			pr_debug("[PASS] Link Error Count=%d, Recovery Count=%d\n",
			     link_error_count, recov_cnt);
			pr_debug("I2C(0x%02x) : [0x%02x], I2C(0x%02x) : [0x%02x]\n",
			     U3_PHY_I2C_PCLK_DRV_REG,
			     _U3Read_Reg(U3_PHY_I2C_PCLK_DRV_REG),
			     U3_PHY_I2C_PCLK_PHASE_REG,
			     _U3Read_Reg(U3_PHY_I2C_PCLK_PHASE_REG));
			pr_debug("Reg(0x130) : [0x%02x], PhaseDelay[0x%02x], Driving[0x%02x], Latch[0x%02x]\n",
			     os_readl(U3D_PIPE_LATCH_SELECT), phase_val, driving, latch_val);

			phase_val++;
			fset_phase_val = TRUE;
		} else if ((os_readl(U3D_LINK_STATE_MACHINE) & LTSSM) == STATE_DISABLE) {	/* link fail */
			pr_debug("[FAIL] STATE_DISABLE, PhaseDelay[0x%02x]\n", phase_val);
			phase_val++;
			fset_phase_val = TRUE;
		} else if (count > MAX_TIMEOUT_COUNT) {	/* link timeout */
			pr_debug("[FAIL] TIMEOUT, PhaseDelay[0x%02x]\n", phase_val);
			phase_val++;
			fset_phase_val = TRUE;
		}
		if (phase_val > MAX_PHASE_RANGE) {
			/* reset device */
			mu3d_hal_rst_dev();
			os_ms_delay(50);
			/* disable ip power down,disable U2/U3 ip power down. */
			mu3d_hal_ssusb_en();
			/* mu3d_hal_pdn_dis(); */
			os_ms_delay(10);

			break;
		}
	}
#endif

	return 0;
}

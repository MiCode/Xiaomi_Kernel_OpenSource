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
#define _MTK_USB_DRV_EXT_
#include "mu3d_hal_usb_drv.h"
#undef _MTK_USB_DRV_EXT_
#include "mu3d_hal_hw.h"
#include "mu3d_hal_qmu_drv.h"
#include "mu3d_hal_comm.h"
#include "mtk-phy.h"

#ifdef SUPPORT_U3
#include <linux/module.h>
unsigned int musb_hal_speed = 1;
module_param_named(hal_speed, musb_hal_speed, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "USB super speed support for hal layer");
#endif

struct USB_REQ *mu3d_hal_get_req(int ep_num, enum USB_DIR dir)
{
	int ep_index = 0;

	if (dir == USB_TX)
		ep_index = ep_num;
	else if (dir == USB_RX)
		ep_index = ep_num + MAX_EP_NUM;
	else
		WARN_ON(1);

	return &g_u3d_req[ep_index];
}

/**
 * mu3d_hal_pdn_dis - disable ssusb power down & clock gated
 *
 */
void mu3d_hal_pdn_dis(void)
{
	os_clrmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
#ifdef SUPPORT_U3
	os_clrmsk(U3D_SSUSB_U3_CTRL_0P,
		  (SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_U2_CG_EN));
#endif
	os_clrmsk(U3D_SSUSB_U2_CTRL_0P,
		  (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_U2_CG_EN));
}

/**
 * mu3d_hal_ssusb_en - disable ssusb power down & enable u2/u3 ports
 *
 */
void mu3d_hal_ssusb_en(void)
{
	os_printk(K_INFO, "%s\n", __func__);

	os_clrmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	os_clrmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);

#ifdef SUPPORT_U3
	os_clrmsk(U3D_SSUSB_U3_CTRL_0P,
		  (SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_HOST_SEL));
#endif

#ifdef SUPPORT_OTG
	os_setmsk(U3D_SSUSB_U2_CTRL_0P, (SSUSB_U2_PORT_OTG_MAC_AUTO_SEL | SSUSB_U2_PORT_OTG_SEL));
	os_clrmsk(U3D_SSUSB_U2_CTRL_0P, (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN));
#else
	os_clrmsk(U3D_SSUSB_U2_CTRL_0P,
		  (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_HOST_SEL));
#endif

	os_setmsk(U3D_SSUSB_REF_CK_CTRL,
		  (SSUSB_REF_MAC_CK_GATE_EN | SSUSB_REF_PHY_CK_GATE_EN | SSUSB_REF_CK_GATE_EN |
		   SSUSB_REF_MAC3_CK_GATE_EN));

	/* check U3D sys125,u3 mac,u2 mac clock status. */
	mu3d_hal_check_clk_sts();
}

void _ex_mu3d_hal_ssusb_en(void)
{
	os_printk(K_DEBUG, "%s\n", __func__);

	/* trigger HW full reset explicitly */
	os_setmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);

	os_clrmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	os_clrmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
#ifdef SUPPORT_U3
	if (musb_hal_speed)
		os_clrmsk(U3D_SSUSB_U3_CTRL_0P,
				(SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_HOST_SEL));
#endif
	os_clrmsk(U3D_SSUSB_U2_CTRL_0P,
		  (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_HOST_SEL));

	os_setmsk(U3D_SSUSB_REF_CK_CTRL,
		  (SSUSB_REF_MAC_CK_GATE_EN | SSUSB_REF_PHY_CK_GATE_EN | SSUSB_REF_CK_GATE_EN |
		   SSUSB_REF_MAC3_CK_GATE_EN));

	/* check U3D sys125,u3 mac,u2 mac clock status. */
	mu3d_hal_check_clk_sts();
}


/**
 * mu3d_hal_dft_reg - apply default register settings
 */
void mu3d_hal_dft_reg(void)
{
	unsigned int tmp;

	/* set sys_ck related registers */
#ifdef NEVER
	/* sys_ck = OSC 125MHz/2 = 62.5MHz */
	os_setmsk(U3D_SSUSB_SYS_CK_CTRL, SSUSB_SYS_CK_DIV2_EN);
	/* U2 MAC sys_ck = ceil(62.5) = 63 */
	os_writelmsk(U3D_USB20_TIMING_PARAMETER, 63, TIME_VALUE_1US);
#ifdef SUPPORT_U3
	/* U3 MAC sys_ck = ceil(62.5) = 63 */
	os_writelmsk(U3D_TIMING_PULSE_CTRL, 63, CNT_1US_VALUE);
#endif
#endif

	/* USB 2.0 related */
	/* sys_ck */
	os_writelmsk(U3D_USB20_TIMING_PARAMETER, U3D_MAC_SYS_CK, TIME_VALUE_1US);
	/* ref_ck */
	os_writelmsk(U3D_SSUSB_U2_PHY_PLL, U3D_MAC_REF_CK, SSUSB_U2_PORT_1US_TIMER);
	/* spec: >600ns */
	tmp = div_and_rnd_up(600, (1000 / U3D_MAC_REF_CK)) + 1;
	os_writelmsk(U3D_SSUSB_IP_PW_CTRL0,
		     (tmp << SSUSB_IP_U2_ENTER_SLEEP_CNT_OFST), SSUSB_IP_U2_ENTER_SLEEP_CNT);

	/* USB 3.0 related */
#ifdef SUPPORT_U3
	/* sys_ck */
	os_writelmsk(U3D_TIMING_PULSE_CTRL, U3D_MAC_SYS_CK, CNT_1US_VALUE);
	/* ref_ck */
	os_writelmsk(U3D_REF_CK_PARAMETER, U3D_MAC_REF_CK, REF_1000NS);
	/* spec: >=300ns */
	tmp = div_and_rnd_up(300, (1000 / U3D_MAC_REF_CK));
	os_writelmsk(U3D_UX_EXIT_LFPS_TIMING_PARAMETER,
		     (tmp << RX_UX_EXIT_LFPS_REF_OFST), RX_UX_EXIT_LFPS_REF);
#endif

#ifdef NEVER
	/* set ref_ck related registers */
	/* U2 ref_ck = OSC 20MHz/2 = 10MHz */
	os_writelmsk(U3D_SSUSB_U2_PHY_PLL, 10, SSUSB_U2_PORT_1US_TIMER);
	/* >600ns */
	os_writelmsk(U3D_SSUSB_IP_PW_CTRL0,
		     (7 << SSUSB_IP_U2_ENTER_SLEEP_CNT_OFST), SSUSB_IP_U2_ENTER_SLEEP_CNT);
#ifdef SUPPORT_U3
	/* U3 ref_ck = 20MHz/2 = 10MHz */
	os_writelmsk(U3D_REF_CK_PARAMETER, 10, REF_1000NS);
	/* >=300ns */
	os_writelmsk(U3D_UX_EXIT_LFPS_TIMING_PARAMETER,
		     (3 << RX_UX_EXIT_LFPS_REF_OFST), RX_UX_EXIT_LFPS_REF);
#endif
#endif				/* NEVER */

	/* code to override HW default values, FPGA ONLY */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* enable debug probe */
	os_writel(U3D_SSUSB_PRB_CTRL0, 0xffff);
#endif

	/* USB 2.0 related */
	/* response STALL to host if LPM BESL value is not in supporting range */
	os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL | LPM_BESLD_STALL));


	/* USB 3.0 related */
#ifdef SUPPORT_U3
#ifdef U2_U3_SWITCH_AUTO
	os_setmsk(U3D_USB2_TEST_MODE, U2U3_AUTO_SWITCH);
#endif

	/* device responses to u3_exit from host automatically */
	os_writel(U3D_LTSSM_CTRL, os_readl(U3D_LTSSM_CTRL) & ~SOFT_U3_EXIT_EN);

#ifndef CONFIG_FPGA_EARLY_PORTING
	os_writel(U3D_PIPE_LATCH_SELECT, 0);
#endif

#endif

#if ISO_UPDATE_TEST & ISO_UPDATE_MODE
	os_setmsk(U3D_POWER_MANAGEMENT, ISO_UPDATE);
#else
	os_clrmsk(U3D_POWER_MANAGEMENT, ISO_UPDATE);
#endif

#ifdef DIS_ZLP_CHECK_CRC32
	/* disable CRC check of ZLP, for compatibility concern */
	os_writel(U3D_LINK_CAPABILITY_CTRL, ZLP_CRC32_CHK_DIS);
#endif
}

/**
 * mu3d_hal_rst_dev - reset all device modules
 *
 */
void mu3d_hal_rst_dev(void)
{
	int ret;

	os_printk(K_DEBUG, "%s\n", __func__);

#if 0
	os_writel(U3D_SSUSB_DEV_RST_CTRL,
		  SSUSB_DEV_SW_RST | SSUSB_DEV_SW_BMU_RST | SSUSB_DEV_SW_QMU_RST |
		  SSUSB_DEV_SW_DRAM_RST);
	os_writel(U3D_SSUSB_DEV_RST_CTRL, 0);
#else
	os_writel(U3D_SSUSB_DEV_RST_CTRL, SSUSB_DEV_SW_RST);
	os_writel(U3D_SSUSB_DEV_RST_CTRL, 0);
#endif

	mu3d_hal_check_clk_sts();

	ret =
	    wait_for_value(U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_QMU_RST_B_STS, SSUSB_DEV_QMU_RST_B_STS,
			   1, 10);
	if (ret == RET_FAIL)
		os_printk(K_ERR, "SSUSB_DEV_QMU_RST_B_STS NG\n");

	ret =
	    wait_for_value(U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_BMU_RST_B_STS, SSUSB_DEV_BMU_RST_B_STS,
			   1, 10);
	if (ret == RET_FAIL)
		os_printk(K_ERR, "SSUSB_DEV_BMU_RST_B_STS NG\n");

	ret = wait_for_value(U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_RST_B_STS, SSUSB_DEV_RST_B_STS, 1, 10);
	if (ret == RET_FAIL)
		os_printk(K_ERR, "SSUSB_DEV_RST_B_STS NG\n");
}

/**
 * mu3d_hal_check_clk_sts - check sys125,u3 mac,u2 mac clock status
 *
 */
int mu3d_hal_check_clk_sts(void)
{
	int ret;

	os_printk(K_DEBUG, "%s\n", __func__);

	ret =
	    wait_for_value(U3D_SSUSB_IP_PW_STS1, SSUSB_SYS125_RST_B_STS, SSUSB_SYS125_RST_B_STS, 1,
			   10);
	if (ret == RET_FAIL)
		os_printk(K_ERR, "SSUSB_SYS125_RST_B_STS NG\n");

	/* do not check when SSUSB_U2_PORT_PDN = 1, because U2 port stays in reset state */
	if (!(os_readl(U3D_SSUSB_U2_CTRL_0P) & SSUSB_U2_PORT_PDN)) {
		ret =
		    wait_for_value(U3D_SSUSB_IP_PW_STS2, SSUSB_U2_MAC_SYS_RST_B_STS,
				   SSUSB_U2_MAC_SYS_RST_B_STS, 1, 10);
		if (ret == RET_FAIL)
			os_printk(K_ERR, "SSUSB_U2_MAC_SYS_RST_B_STS NG\n");
	}
#ifdef SUPPORT_U3
	/* do not check when SSUSB_U3_PORT_PDN = 1, because U3 port stays in reset state */
	if (musb_hal_speed && (!(os_readl(U3D_SSUSB_U3_CTRL_0P) & SSUSB_U3_PORT_PDN))) {
		ret =
		    wait_for_value(U3D_SSUSB_IP_PW_STS1, SSUSB_U3_MAC_RST_B_STS,
				   SSUSB_U3_MAC_RST_B_STS, 1, 100);
		if (ret == RET_FAIL)
			os_printk(K_ERR,
				  "SSUSB_U3_MAC_RST_B_STS NG, U3D_SSUSB_IP_PW_STS1 is 0x%x\n",
				  os_readl(U3D_SSUSB_IP_PW_STS1));
	}
#endif

	os_printk(K_DEBUG, "check clk pass!!\n");
	return RET_SUCCESS;
}

/**
 * mu3d_hal_link_up - u3d link up
 *
 */
int mu3d_hal_link_up(int latch_val)
{
	mu3d_hal_ssusb_en();
	mu3d_hal_rst_dev();
	os_ms_delay(50);
	os_writel(U3D_USB3_CONFIG, USB3_EN);
	os_writel(U3D_PIPE_LATCH_SELECT, latch_val);	/* set tx/rx latch sel */

	return 0;
}

/**
 * mu3d_hal_initr_dis - disable all interrupts
 *
 */
void mu3d_hal_initr_dis(void)
{
	/* Disable Level 1 interrupts */
	os_writel(U3D_LV1IECR, 0xFFFFFFFF);

	/* Disable Endpoint interrupts */
	os_writel(U3D_EPIECR, 0xFFFFFFFF);

	/* Disable DMA interrupts */
	os_writel(U3D_DMAIECR, 0xFFFFFFFF);

#ifdef SUPPORT_OTG
	/* Disable TxQ/RxQ Done interrupts */
	os_writel(U3D_QIECR0, 0xFFFFFFFF);

	/* Disable TxQ/RxQ Empty/Checksum/Length/ZLP Error indication */
	os_writel(U3D_QIECR1, 0xFFFFFFFF);

	/* Disable TxQ/RxQ Empty Indication */
	os_writel(U3D_QEMIECR, 0xFFFFFFFF);

	/* Disable Interrupt of TxQ GPD Checksum/Data Buffer Length Error */
	os_writel(U3D_TQERRIECR0, 0xFFFFFFFF);

	/* Disable Interrupt of RxQ GPD Checksum/Data Buffer Length Error */
	os_writel(U3D_RQERRIECR0, 0xFFFFFFFF);

	/* Disable RxQ ZLP Error indication */
	os_writel(U3D_RQERRIECR1, 0xFFFFFFFF);
#endif
}

void mu3d_hal_clear_intr(void)
{
	os_printk(K_ERR, "%s\n", __func__);

	/* Clear EP0 and Tx/Rx EPn interrupts status */
	os_writel(U3D_EPISR, 0xFFFFFFFF);

	/* Clear EP0 and Tx/Rx EPn DMA interrupts status */
	os_writel(U3D_DMAISR, 0xFFFFFFFF);

#ifdef SUPPORT_OTG
	/* Clear TxQ/RxQ Done interrupts status */
	os_writel(U3D_QISAR0, 0xFFFFFFFF);

	/* Clear TxQ/RxQ Empty/Checksum/Length/ZLP Error indication status */
	os_writel(U3D_QISAR1, 0xFFFFFFFF);

	/* Clear TxQ/RxQ Empty indication */
	os_writel(U3D_QEMIR, 0xFFFFFFFF);

	/* Clear Interrupt of RxQ GPD Checksum/Data Buffer Length Error */
	os_writel(U3D_TQERRIR0, 0xFFFFFFFF);

	/* Clear Interrupt of RxQ GPD Checksum/Data Buffer Length Error */
	os_writel(U3D_RQERRIR0, 0xFFFFFFFF);

	/* Clear RxQ ZLP Error indication */
	os_writel(U3D_RQERRIR1, 0xFFFFFFFF);
#endif

	/* Clear speed change event */
	os_writel(U3D_DEV_LINK_INTR, 0xFFFFFFFF);

	/* Clear U2 USB common interrupt status */
	os_writel(U3D_COMMON_USB_INTR, 0xFFFFFFFF);

#ifdef SUPPORT_U3
	/* Clear U3 LTSSM interrupt status */
	if (musb_hal_speed)
		os_writel(U3D_LTSSM_INTR, 0xFFFFFFFF);
#endif
}

/**
 * mu3d_hal_system_intr_en - enable system global interrupt
 *
 */
void mu3d_hal_system_intr_en(void)
{
	unsigned short int_en;
#ifdef SUPPORT_U3
	unsigned int ltssm_int_en;
#endif

	os_printk(K_ERR, "%s\n", __func__);

	/* Disable All endpoint interrupt */
	os_writel(U3D_EPIECR, os_readl(U3D_EPIER));

	/* Disable All DMA interrput */
	os_writel(U3D_DMAIECR, os_readl(U3D_DMAIER));

	/* Disable U2 common USB interrupts */
	os_writel(U3D_COMMON_USB_INTR_ENABLE, 0x00);

	/* Clear U2 common USB interrupts status */
	os_writel(U3D_COMMON_USB_INTR, os_readl(U3D_COMMON_USB_INTR));

	/* Enable U2 common USB interrupt */
	int_en = SUSPEND_INTR_EN | RESUME_INTR_EN | RESET_INTR_EN | CONN_INTR_EN | DISCONN_INTR_EN
	    | VBUSERR_INTR_EN | LPM_INTR_EN | LPM_RESUME_INTR_EN;
	os_writel(U3D_COMMON_USB_INTR_ENABLE, int_en);

#ifdef SUPPORT_U3
	/* Disable U3 LTSSM interrupts */
	os_writel(U3D_LTSSM_INTR_ENABLE, 0x00);
	os_printk(K_ERR, "U3D_LTSSM_INTR: %x\n", os_readl(U3D_LTSSM_INTR));

	/* Clear U3 LTSSM interrupts */
	os_writel(U3D_LTSSM_INTR, os_readl(U3D_LTSSM_INTR));

	/* Enable U3 LTSSM interrupts */
	ltssm_int_en =
	    SS_INACTIVE_INTR_EN | SS_DISABLE_INTR_EN | COMPLIANCE_INTR_EN | LOOPBACK_INTR_EN |
	    HOT_RST_INTR_EN | WARM_RST_INTR_EN | RECOVERY_INTR_EN | ENTER_U0_INTR_EN |
	    ENTER_U1_INTR_EN | ENTER_U2_INTR_EN | ENTER_U3_INTR_EN | EXIT_U1_INTR_EN |
	    EXIT_U2_INTR_EN | EXIT_U3_INTR_EN | RXDET_SUCCESS_INTR_EN | VBUS_RISE_INTR_EN |
	    VBUS_FALL_INTR_EN | U3_LFPS_TMOUT_INTR_EN | U3_RESUME_INTR_EN;
	os_writel(U3D_LTSSM_INTR_ENABLE, ltssm_int_en);
#endif

#ifdef SUPPORT_OTG
	/* os_writel(U3D_SSUSB_OTG_INT_EN, 0x0); */
	os_printk(K_ERR, "U3D_SSUSB_OTG_STS: %x\n", os_readl(U3D_SSUSB_OTG_STS));
	os_writel(U3D_SSUSB_OTG_STS_CLR, os_readl(U3D_SSUSB_OTG_STS));
	os_writel(U3D_SSUSB_OTG_INT_EN,
		  os_readl(U3D_SSUSB_OTG_INT_EN) | SSUSB_VBUS_CHG_INT_B_EN |
		  SSUSB_CHG_B_ROLE_B_INT_EN | SSUSB_CHG_A_ROLE_B_INT_EN |
		  SSUSB_ATTACH_B_ROLE_INT_EN);
#endif

#ifdef USE_SSUSB_QMU
	/* Enable QMU interrupt. */
	os_writel(U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		  RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
#endif

	/* Enable EP0 DMA interrupt */
	os_writel(U3D_DMAIESR, EP0DMAIESR);

	/* Enable speed change interrupt */
	os_writel(U3D_DEV_LINK_INTR_ENABLE, SSUSB_DEV_SPEED_CHG_INTR_EN);
}

void _ex_mu3d_hal_system_intr_en(void)
{
	unsigned short int_en;
#ifdef SUPPORT_U3
	unsigned int ltssm_int_en;
#endif

	os_printk(K_DEBUG, "%s\n", __func__);

	/* Disable All endpoint interrupt */
	os_writel(U3D_EPIECR, os_readl(U3D_EPIER));

	/* Disable All DMA interrput */
	os_writel(U3D_DMAIECR, os_readl(U3D_DMAIER));

	/* Disable U2 common USB interrupts */
	os_writel(U3D_COMMON_USB_INTR_ENABLE, 0x00);

	/* Clear U2 common USB interrupts status */
	os_writel(U3D_COMMON_USB_INTR, os_readl(U3D_COMMON_USB_INTR));

	/* Enable U2 common USB interrupt */
	int_en = SUSPEND_INTR_EN | RESUME_INTR_EN | RESET_INTR_EN | CONN_INTR_EN | DISCONN_INTR_EN
	    | VBUSERR_INTR_EN | LPM_INTR_EN | LPM_RESUME_INTR_EN;
	os_writel(U3D_COMMON_USB_INTR_ENABLE, int_en);

#ifdef SUPPORT_U3
	if (musb_hal_speed) {
		/* Disable U3 LTSSM interrupts */
		os_writel(U3D_LTSSM_INTR_ENABLE, 0x00);
		os_printk(K_ERR, "U3D_LTSSM_INTR: %x\n", os_readl(U3D_LTSSM_INTR));

		/* Clear U3 LTSSM interrupts */
		os_writel(U3D_LTSSM_INTR, os_readl(U3D_LTSSM_INTR));

		/* Enable U3 LTSSM interrupts */
		ltssm_int_en =
			SS_INACTIVE_INTR_EN | SS_DISABLE_INTR_EN | COMPLIANCE_INTR_EN | LOOPBACK_INTR_EN |
			HOT_RST_INTR_EN | WARM_RST_INTR_EN | RECOVERY_INTR_EN | ENTER_U0_INTR_EN |
			ENTER_U1_INTR_EN | ENTER_U2_INTR_EN | ENTER_U3_INTR_EN | EXIT_U1_INTR_EN |
			EXIT_U2_INTR_EN | EXIT_U3_INTR_EN | RXDET_SUCCESS_INTR_EN | VBUS_RISE_INTR_EN |
			VBUS_FALL_INTR_EN | U3_LFPS_TMOUT_INTR_EN | U3_RESUME_INTR_EN;
		os_writel(U3D_LTSSM_INTR_ENABLE, ltssm_int_en);
	}
#endif

#if 0
#ifdef SUPPORT_OTG
	/* os_writel(U3D_SSUSB_OTG_INT_EN, 0x0); */
	os_printk(K_ERR, "U3D_SSUSB_OTG_STS: %x\n", os_readl(U3D_SSUSB_OTG_STS));
	os_writel(U3D_SSUSB_OTG_STS_CLR, os_readl(U3D_SSUSB_OTG_STS));
	os_writel(U3D_SSUSB_OTG_INT_EN,
		  os_readl(U3D_SSUSB_OTG_INT_EN) | SSUSB_VBUS_CHG_INT_B_EN |
		  SSUSB_CHG_B_ROLE_B_INT_EN | SSUSB_CHG_A_ROLE_B_INT_EN |
		  SSUSB_ATTACH_B_ROLE_INT_EN);
#endif
#endif

#ifdef USE_SSUSB_QMU
	/* Enable QMU interrupt. */
	os_writel(U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		  RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
#endif

	/* Enable EP0 DMA interrupt */
	os_writel(U3D_DMAIESR, EP0DMAIESR);

	/* Enable speed change interrupt */
	os_writel(U3D_DEV_LINK_INTR_ENABLE, SSUSB_DEV_SPEED_CHG_INTR_EN);
}

/**
 * mu3d_hal_resume - power mode resume
 *
 */
void mu3d_hal_resume(void)
{
#ifdef SUPPORT_U3
	if (os_readl(U3D_DEVICE_CONF) & HW_USB2_3_SEL) {	/* SS */
		mu3d_hal_pdn_ip_port(1, 0, 1, 0);

		os_setmsk(U3D_LINK_POWER_CONTROL, UX_EXIT);
	} else
#endif
	{			/* hs fs */
		mu3d_hal_pdn_ip_port(1, 0, 0, 1);

		os_setmsk(U3D_POWER_MANAGEMENT, RESUME);
		os_ms_delay(10);
		os_clrmsk(U3D_POWER_MANAGEMENT, RESUME);
	}
}

/**
 * mu3d_hal_u2dev_connect - u2 device softconnect
 *
 */
void mu3d_hal_u2dev_connect(void)
{
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | SOFT_CONN);
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | SUSPENDM_ENABLE);
	os_printk(K_INFO, "SOFTCONN = 1\n");
}

void mu3d_hal_u2dev_disconn(void)
{
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) & ~SOFT_CONN);
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) & ~SUSPENDM_ENABLE);
	os_printk(K_INFO, "SOFTCONN = 0\n");
}

/**
 * mu3d_hal_u3dev_en - enable U3D SS dev
 *
 */
void mu3d_hal_dump_register(void)
{
	void __iomem *i;

	pr_debug("\n\n[mu3d_hal_dump_register]\n");
	pr_debug("SSUSB_DEV_BASE ==================>\n");

	for (i = SSUSB_DEV_BASE; i <= SSUSB_DEV_BASE + 0xC; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x20; i <= SSUSB_DEV_BASE + 0x24; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x30; i <= SSUSB_DEV_BASE + 0x34; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x40; i <= SSUSB_DEV_BASE + 0x44; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x50; i <= SSUSB_DEV_BASE + 0x50; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x70; i <= SSUSB_DEV_BASE + 0x74; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x80; i <= SSUSB_DEV_BASE + 0x9C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0xC0; i <= SSUSB_DEV_BASE + 0xEC; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x100; i <= SSUSB_DEV_BASE + 0x10C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x400; i <= SSUSB_DEV_BASE + 0x410; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0x700; i <= SSUSB_DEV_BASE + 0x71C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0xC04; i <= SSUSB_DEV_BASE + 0xC10; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0xC20; i <= SSUSB_DEV_BASE + 0xC3C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_DEV_BASE + 0xC84; i <= SSUSB_DEV_BASE + 0xC84; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_DEV_BASE <==================\n");


	pr_debug("SSUSB_USB2_CSR ==================>\n");
	for (i = SSUSB_USB2_CSR_BASE + 0x0000; i <= SSUSB_USB2_CSR_BASE + 0x0060; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_USB2_CSR <==================\n");


	pr_debug("SSUSB_SIFSLV_IPPC_BASE ==================>\n");
	for (i = SSUSB_SIFSLV_IPPC_BASE + 0x0000; i <= SSUSB_SIFSLV_IPPC_BASE + 0x002C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_SIFSLV_IPPC_BASE + 0x0030; i <= SSUSB_SIFSLV_IPPC_BASE + 0x0038; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_SIFSLV_IPPC_BASE + 0x0050; i <= SSUSB_SIFSLV_IPPC_BASE + 0x0050; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_SIFSLV_IPPC_BASE + 0x007C; i <= SSUSB_SIFSLV_IPPC_BASE + 0x00A4; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_SIFSLV_IPPC_BASE <==================\n");

	pr_debug("SSUSB_EPCTL_CSR_BASE ==================>\n");
	for (i = SSUSB_EPCTL_CSR_BASE + 0x0000; i <= SSUSB_EPCTL_CSR_BASE + 0x0028; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_EPCTL_CSR_BASE + 0x0030; i <= SSUSB_EPCTL_CSR_BASE + 0x0030; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_EPCTL_CSR_BASE + 0x0040; i <= SSUSB_EPCTL_CSR_BASE + 0x0048; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_EPCTL_CSR_BASE + 0x0050; i <= SSUSB_EPCTL_CSR_BASE + 0x0054; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_EPCTL_CSR_BASE + 0x0060; i <= SSUSB_EPCTL_CSR_BASE + 0x0064; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_EPCTL_CSR_BASE <==================\n");

	pr_debug("SSUSB_USB3_SYS_CSR_BASE ==================>\n");
	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x0200; i <= SSUSB_USB3_SYS_CSR_BASE + 0x022C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x0240; i <= SSUSB_USB3_SYS_CSR_BASE + 0x0244; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x0290; i <= SSUSB_USB3_SYS_CSR_BASE + 0x029C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x02A0; i <= SSUSB_USB3_SYS_CSR_BASE + 0x02AC; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x02B0; i <= SSUSB_USB3_SYS_CSR_BASE + 0x02B8; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_SYS_CSR_BASE + 0x02C0; i <= SSUSB_USB3_SYS_CSR_BASE + 0x02DC; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_USB3_SYS_CSR_BASE <==================\n");

	pr_debug("SSUSB_USB3_MAC_CSR ==================>\n");
	for (i = SSUSB_USB3_MAC_CSR_BASE + 0x0000; i <= SSUSB_USB3_MAC_CSR_BASE + 0x001C; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_MAC_CSR_BASE + 0x0050; i <= SSUSB_USB3_MAC_CSR_BASE + 0x0054; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_MAC_CSR_BASE + 0x007C; i <= SSUSB_USB3_MAC_CSR_BASE + 0x00B0; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	for (i = SSUSB_USB3_MAC_CSR_BASE + 0x0010C; i <= SSUSB_USB3_MAC_CSR_BASE + 0x0148; i += 0x4)
		pr_debug("[0x%lx] 0x%x\n", (unsigned long)i, os_readl(i));

	pr_debug("SSUSB_USB3_MAC_CSR <==================\n");

	pr_debug("\n[mu3d_hal_dump_register]\n\n\n");
}

void mu3d_hal_u3dev_en(void)
{
	/*
	 * Enable super speed function.
	 */
	os_writel(U3D_USB3_CONFIG, USB3_EN);
	os_printk(K_INFO, "USB3_EN = 1\n");
}

/**
 * mu3d_hal_u3dev_dis - disable U3D SS dev
 *
 */
void mu3d_hal_u3dev_dis(void)
{
#ifdef SUPPORT_U3
	/*
	 * If usb3_en =0 => LTSSM will go to SS.Disable state.
	 */
	if (musb_hal_speed) {
		os_writel(U3D_USB3_CONFIG, 0);
		os_printk(K_INFO, "USB3_EN = 0\n");
	}
#endif
}

/**
 * mu3d_hal_set_speed - enable ss or connect to hs/fs
 *@args - arg1: speed
 */
void mu3d_hal_set_speed(enum USB_SPEED speed)
{
#ifndef EXT_VBUS_DET
	os_writel(U3D_MISC_CTRL, 0);
#else
	os_writel(U3D_MISC_CTRL, 0x3);
#endif

	pr_debug("mu3d_hal_set_speed speed=%d\n", speed);

	/* clear ltssm state */
	if (speed == SSUSB_SPEED_FULL) {
		os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) & ~HS_ENABLE);
		mu3d_hal_u2dev_connect();
		g_u3d_setting.speed = SSUSB_SPEED_FULL;
	} else if (speed == SSUSB_SPEED_HIGH) {
		os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | HS_ENABLE);
		mu3d_hal_u2dev_connect();
		g_u3d_setting.speed = SSUSB_SPEED_HIGH;
	}
#ifdef SUPPORT_U3
	else if (speed == SSUSB_SPEED_SUPER) {
		g_u3d_setting.speed = SSUSB_SPEED_SUPER;
		mu3d_hal_u2dev_disconn();
		mu3d_hal_u3dev_en();
	}
#endif
	else {
		os_printk(K_ALET, "Unsupported speed!!\n");
		WARN_ON(1);
	}
}

/**
 * mu3d_hal_pdn_cg_en - enable U2/U3 pdn & cg
 *@args -
 */
void mu3d_hal_pdn_cg_en(void)
{
#ifdef POWER_SAVING_MODE
	unsigned char speed = (os_readl(U3D_DEVICE_CONF) & SSUSB_DEV_SPEED);

	os_printk(K_INFO, "%s\n", __func__);

	switch (speed) {
	case SSUSB_SPEED_SUPER:
		os_printk(K_NOTICE, "Device: SUPER SPEED LOW POWER\n");
		os_setmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN);
		break;
	case SSUSB_SPEED_HIGH:
		os_printk(K_NOTICE, "Device: HIGH SPEED LOW POWER\n");
#ifdef SUPPORT_U3
		os_setmsk(U3D_SSUSB_U3_CTRL_0P, SSUSB_U3_PORT_PDN);
#endif
		break;
	case SSUSB_SPEED_FULL:
		os_printk(K_NOTICE, "Device: FULL SPEED LOW POWER\n");
#ifdef SUPPORT_U3
		os_setmsk(U3D_SSUSB_U3_CTRL_0P, SSUSB_U3_PORT_PDN);
#endif
		break;
	default:
		os_printk(K_NOTICE, "[ERROR] Are you kidding!?!?\n");
		break;
	}
#endif
}

void mu3d_hal_pdn_ip_port(unsigned char on, unsigned char touch_dis, unsigned char u3, unsigned char u2)
{
#ifdef POWER_SAVING_MODE
	os_printk(K_INFO, "%s on=%d, touch_dis=%d, u3=%d, u2=%d\n", __func__, on, touch_dis, u3,
		  u2);
	if (on) {
		os_clrmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);

#ifdef SUPPORT_U3
		if (u3)
			os_clrmsk(U3D_SSUSB_U3_CTRL_0P, SSUSB_U3_PORT_PDN
				  | ((touch_dis) ? SSUSB_U3_PORT_DIS : 0));
#endif
		if (u2)
			os_clrmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN
				  | ((touch_dis) ? SSUSB_U2_PORT_DIS : 0));

		while (!(os_readl(U3D_SSUSB_IP_PW_STS1) & SSUSB_SYSPLL_STABLE))
			;
	} else {
#ifdef SUPPORT_U3
		if (u3)
			os_setmsk(U3D_SSUSB_U3_CTRL_0P, SSUSB_U3_PORT_PDN
				  | ((touch_dis) ? SSUSB_U3_PORT_DIS : 0));
#endif
		if (u2)
			os_setmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN
				  | ((touch_dis) ? SSUSB_U2_PORT_DIS : 0));

		os_setmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	}
#else
	os_printk(K_INFO, "%s Does NOT support IP power down\n", __func__);
#endif
}

/**
 * mu3d_hal_det_speed - detect device speed
 *@args - arg1: speed
 */
void mu3d_hal_det_speed(enum USB_SPEED speed, unsigned char det_speed)
{
	unsigned char temp;
	unsigned short cnt_down = 10000;

	pr_debug("===Start polling===\n");

	/* #ifdef EXT_VBUS_DET */
	if (!det_speed) {
		while (!(os_readl(U3D_DEV_LINK_INTR) & SSUSB_DEV_SPEED_CHG_INTR)) {
			os_ms_delay(1);
			cnt_down--;

			if (cnt_down == 0) {
				pr_debug("===Polling FAIL===\n");
				return;
			}
		}
	} else {
		while (!(os_readl(U3D_DEV_LINK_INTR) & SSUSB_DEV_SPEED_CHG_INTR))
			;
	}
	/* #else */
	/* while(!(os_readl(U3D_DEV_LINK_INTR) & SSUSB_DEV_SPEED_CHG_INTR)); */
	/* #endif */

	pr_debug("===Polling End===\n");

	os_writel(U3D_DEV_LINK_INTR, SSUSB_DEV_SPEED_CHG_INTR);

	temp = (os_readl(U3D_DEVICE_CONF) & SSUSB_DEV_SPEED);
	switch (temp) {
	case SSUSB_SPEED_SUPER:
		os_printk(K_EMERG, "Device: SUPER SPEED\n");
		break;
	case SSUSB_SPEED_HIGH:
		os_printk(K_EMERG, "Device: HIGH SPEED\n");
		break;
	case SSUSB_SPEED_FULL:
		os_printk(K_EMERG, "Device: FULL SPEED\n");
		break;
	case SSUSB_SPEED_INACTIVE:
		os_printk(K_EMERG, "Device: INACTIVE\n");
		break;
	}

	if (temp != speed) {
		os_printk(K_EMERG, "desired speed: %d, detected speed: %d\n", speed, temp);
		os_ms_delay(5000);
		/* while(1); */
	}
}

/**
 * mu3d_hal_write_fifo - pio write one packet
 *@args - arg1: ep number, arg2: data length,  arg3: data buffer, arg4: max packet size
 */
int mu3d_hal_write_fifo(int ep_num, int length, unsigned char *buf, int maxp)
{
	unsigned int residue;
	unsigned int count;
	unsigned int temp;

	os_printk(K_DEBUG, "%s epnum=%d, len=%d, buf=%p, maxp=%d\n", __func__, ep_num, length, buf,
		  maxp);

	count = residue = length;

	while (residue > 0) {
		if (residue == 1) {
			temp = ((*buf) & 0xFF);
			os_writeb(USB_FIFO(ep_num), temp);
			buf += 1;
			residue -= 1;
		} else if (residue == 2) {
			temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
			os_writew(USB_FIFO(ep_num), temp);
			buf += 2;
			residue -= 2;
		} else if (residue == 3) {
			temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
			os_writew(USB_FIFO(ep_num), temp);
			buf += 2;

			temp = ((*buf) & 0xFF);
			os_writeb(USB_FIFO(ep_num), temp);
			buf += 1;
			residue -= 3;
		} else {
			temp =
			    ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00) +
			    (((*(buf + 2)) << 16) & 0xFF0000) + (((*(buf + 3)) << 24) & 0xFF000000);
			os_writel(USB_FIFO(ep_num), temp);
			buf += 4;
			residue -= 4;
		}
	}

	if (ep_num == 0) {
		if (count == 0) {
			os_printk(K_DEBUG, "USB_EP0_DATAEND %8X+\n", os_readl(U3D_EP0CSR));
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_DATAEND | EP0_TXPKTRDY);
			os_printk(K_DEBUG, "USB_EP0_DATAEND %8X-\n", os_readl(U3D_EP0CSR));
		} else {
#ifdef AUTOSET
			if (count < maxp) {
				os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_TXPKTRDY);
				os_printk(K_DEBUG, "EP0_TXPKTRDY\n");
			}
#else
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_TXPKTRDY);
#endif
		}
	} else {
		if (count == 0) {
			USB_WriteCsr32(U3D_TX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
		} else {
#ifdef AUTOSET
			if (count < maxp) {
				USB_WriteCsr32(U3D_TX1CSR0, ep_num,
					       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
				os_printk(K_DEBUG, "short packet\n");
			}
#else
			USB_WriteCsr32(U3D_TX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
#endif
		}
	}
	return count;
}

/**
 * mu3d_hal_read_fifo - pio read one packet
 *@args - arg1: ep number,  arg2: data buffer
 */
DEV_INT32 mu3d_hal_read_fifo(int ep_num, unsigned char *buf)
{
	unsigned short count, residue;
	unsigned int temp;
	unsigned char *bp = buf;

	if (ep_num == 0)
		residue = count = os_readl(U3D_RXCOUNT0);
	else
		residue = count = (USB_ReadCsr32(U3D_RX1CSR3, ep_num) >> EP_RX_COUNT_OFST);

	os_printk(K_DEBUG, "%s, req->buf=%p, cnt=%d\n", __func__, buf, count);

	while (residue > 0) {
		temp = os_readl(USB_FIFO(ep_num));

		/*Store the first byte */
		*bp = temp & 0xFF;

		/*Store the 2nd byte, If have */
		if (residue > 1)
			*(bp + 1) = (temp >> 8) & 0xFF;

		/*Store the 3rd byte, If have */
		if (residue > 2)
			*(bp + 2) = (temp >> 16) & 0xFF;

		/*Store the 4th byte, If have */
		if (residue > 3)
			*(bp + 3) = (temp >> 24) & 0xFF;

		if (residue > 4) {
			bp = bp + 4;
			residue = residue - 4;
		} else {
			residue = 0;
		}
	}

#ifdef AUTOCLEAR
	if (ep_num == 0) {
		if (!count) {
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_RXPKTRDY);
			os_printk(K_DEBUG, "EP0_RXPKTRDY\n");
		}
	} else {
		if (!count) {
			USB_WriteCsr32(U3D_RX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_RX1CSR0, ep_num) | RX_RXPKTRDY);
			os_printk(K_ALET, "ZLP\n");
		}
	}
#else
	if (ep_num == 0) {
		os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_RXPKTRDY);
		os_printk(K_DEBUG, "EP0_RXPKTRDY\n");
	} else {
		USB_WriteCsr32(U3D_RX1CSR0, ep_num,
			       USB_ReadCsr32(U3D_RX1CSR0, ep_num) | RX_RXPKTRDY);
		os_printk(K_DEBUG, "RX_RXPKTRDY\n");
	}
#endif
	return count;
}

/**
 * mu3d_hal_write_fifo_burst - pio write n packets with polling buffer full (epn only)
 *@args - arg1: ep number, arg2: u3d req
 */
int mu3d_hal_write_fifo_burst(int ep_num, int length, unsigned char *buf,
				    int maxp)
{
	unsigned int residue, count, actual;
	unsigned int temp;
	unsigned char *bp;

	os_printk(K_DEBUG, "%s ep_num=%d, length=%d, buf=%p, maxp=%d\n", __func__, ep_num, length,
		  buf, maxp);

#if (BUS_MODE == PIO_MODE)
	/* Here is really tricky, need to print this log to pass EPn PIO loopback
	 * No time to figure out why. Sorry~
	 */
	pr_debug("write_fifo_burst=ep_num=%d, length=%d, buf=%p, maxp=%d\n", ep_num, length, buf,
		 maxp);
#endif

	actual = 0;

#ifdef AUTOSET
	while (!(USB_ReadCsr32(U3D_TX1CSR0, ep_num) & TX_FIFOFULL)) {
#endif

		if (length - actual > maxp)
			count = residue = maxp;
		else
			count = residue = length - actual;

		bp = buf + actual;

		while (residue > 0) {

			if (residue == 1) {
				temp = ((*buf) & 0xFF);
				os_writeb(USB_FIFO(ep_num), temp);
				buf += 1;
				residue -= 1;
			} else if (residue == 2) {
				temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
				os_writew(USB_FIFO(ep_num), temp);
				buf += 2;
				residue -= 2;
			} else if (residue == 3) {
				temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
				os_writew(USB_FIFO(ep_num), temp);
				buf += 2;

				temp = ((*buf) & 0xFF);
				os_writeb(USB_FIFO(ep_num), temp);
				buf += 1;
				residue -= 3;
			} else {
				temp =
				    ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00) +
				    (((*(buf + 2)) << 16) & 0xFF0000) +
				    (((*(buf + 3)) << 24) & 0xFF000000);
				os_writel(USB_FIFO(ep_num), temp);
				buf += 4;
				residue -= 4;
			}
		}
#ifdef NEVER
		while (residue > 0) {

			if (residue == 1) {
				temp = ((*bp) & 0xFF);
				os_writel(U3D_RISC_SIZE, RISC_SIZE_1B);
				unit = 1;
			} else if (residue == 2) {
				temp = ((*bp) & 0xFF) + (((*(bp + 1)) << 8) & 0xFF00);
				os_writel(U3D_RISC_SIZE, RISC_SIZE_2B);
				unit = 2;
			} else if (residue == 3) {
				temp = ((*bp) & 0xFF) + (((*(bp + 1)) << 8) & 0xFF00);
				os_writel(U3D_RISC_SIZE, RISC_SIZE_2B);
				unit = 2;
			} else {
				temp =
				    ((*bp) & 0xFF) + (((*(bp + 1)) << 8) & 0xFF00) +
				    (((*(bp + 2)) << 16) & 0xFF0000) +
				    (((*(bp + 3)) << 24) & 0xFF000000);
				unit = 4;
			}
			os_writel(USB_FIFO(ep_num), temp);
			bp = bp + unit;
			residue -= unit;
		}
		if (os_readl(U3D_RISC_SIZE) != RISC_SIZE_4B)
			os_writel(U3D_RISC_SIZE, RISC_SIZE_4B);
#endif				/* NEVER */
		actual += count;

		if (length == 0) {
			USB_WriteCsr32(U3D_TX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
			return actual;
		}
		/*WARNING:UNNECESSARY_ELSE: else is not generally useful after a break or return*/
		/*else*/
		{
#ifdef AUTOSET
			if ((count < maxp) && (count > 0)) {
				USB_WriteCsr32(U3D_TX1CSR0, ep_num,
					       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
				os_printk(K_DEBUG, "short packet\n");
				return actual;
			}
			if (count == 0)
				return actual;
#else
			USB_WriteCsr32(U3D_TX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_TX1CSR0, ep_num) | TX_TXPKTRDY);
#endif
		}
#ifdef AUTOSET
	}
#endif
	return actual;
}

/**
 * mu3d_hal_read_fifo_burst - pio read n packets with polling buffer empty (epn only)
 *@args - arg1: ep number, arg2: data buffer
 */
int mu3d_hal_read_fifo_burst(int ep_num, unsigned char *buf)
{
	unsigned short count, residue;
	unsigned int temp, actual;
	unsigned char *bp;

	os_printk(K_INFO, "mu3d_hal_read_fifo_burst\n");
	os_printk(K_ALET, "req->buf=%p\n", buf);
	actual = 0;
#ifdef AUTOCLEAR
	while (!(USB_ReadCsr32(U3D_RX1CSR0, ep_num) & RX_FIFOEMPTY)) {
#endif
		residue = count = (USB_ReadCsr32(U3D_RX1CSR3, ep_num) >> EP_RX_COUNT_OFST);
		os_printk(K_INFO, "count :%d ; req->actual :%d\n", count, actual);
		bp = buf + actual;

		while (residue > 0) {
			temp = os_readl(USB_FIFO(ep_num));
			*bp = temp & 0xFF;
			*(bp + 1) = (temp >> 8) & 0xFF;
			*(bp + 2) = (temp >> 16) & 0xFF;
			*(bp + 3) = (temp >> 24) & 0xFF;
			bp = bp + 4;
			if (residue > 4)
				residue = residue - 4;
			else
				residue = 0;
		}
		actual += count;

#ifdef AUTOCLEAR
		if (!count) {
			USB_WriteCsr32(U3D_RX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_RX1CSR0, ep_num) | RX_RXPKTRDY);
			os_printk(K_ALET, "zlp\n");
			os_printk(K_ALET, "actual :%d\n", actual);
			return actual;
		}
#else
		if (ep_num == 0) {
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | EP0_RXPKTRDY);
		} else {
			USB_WriteCsr32(U3D_RX1CSR0, ep_num,
				       USB_ReadCsr32(U3D_RX1CSR0, ep_num) | RX_RXPKTRDY);
		}
#endif
#ifdef AUTOCLEAR
	}
#endif

	return actual;
}


/**
* mu3d_hal_unfigured_ep -
 *@args -
 */
void mu3d_hal_unfigured_ep(void)
{
	unsigned int i, tx_ep_num, rx_ep_num;
	struct USB_EP_SETTING *ep_setting;

	os_printk(K_DEBUG, "%s\n", __func__);

	g_TxFIFOadd = USB_TX_FIFO_START_ADDRESS;
	g_RxFIFOadd = USB_RX_FIFO_START_ADDRESS;

#ifdef HARDCODE_EP
	tx_ep_num = MAX_QMU_EP;	/* os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM; */
	rx_ep_num = MAX_QMU_EP;	/* (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8; */
#else
	tx_ep_num = os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM;
	rx_ep_num = (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8;
#endif

	for (i = 1; i <= tx_ep_num; i++) {
		USB_WriteCsr32(U3D_TX1CSR0, i, USB_ReadCsr32(U3D_TX1CSR0, i) & (~0x7FF));
		USB_WriteCsr32(U3D_TX1CSR1, i, 0);
		USB_WriteCsr32(U3D_TX1CSR2, i, 0);
		ep_setting = &g_u3d_setting.ep_setting[i];
		ep_setting->fifoaddr = 0;
		ep_setting->enabled = 0;
	}

	for (i = 1; i <= rx_ep_num; i++) {
		USB_WriteCsr32(U3D_RX1CSR0, i, USB_ReadCsr32(U3D_RX1CSR0, i) & (~0x7FF));
		USB_WriteCsr32(U3D_RX1CSR1, i, 0);
		USB_WriteCsr32(U3D_RX1CSR2, i, 0);
		ep_setting = &g_u3d_setting.ep_setting[i + MAX_EP_NUM];
		ep_setting->fifoaddr = 0;
		ep_setting->enabled = 0;
	}
}

/**
* mu3d_hal_unfigured_ep_num -
 *@args -
 */
void mu3d_hal_unfigured_ep_num(unsigned char ep_num, enum USB_DIR dir)
{
	struct USB_EP_SETTING *ep_setting;

	os_printk(K_DEBUG, "%s %d\n", __func__, ep_num);

	if (dir == USB_TX) {
		USB_WriteCsr32(U3D_TX1CSR0, ep_num, USB_ReadCsr32(U3D_TX1CSR0, ep_num) & (~0x7FF));
		USB_WriteCsr32(U3D_TX1CSR1, ep_num, 0);
		USB_WriteCsr32(U3D_TX1CSR2, ep_num, 0);
		ep_setting = &g_u3d_setting.ep_setting[ep_num];
		ep_setting->enabled = 0;
	} else {
		USB_WriteCsr32(U3D_RX1CSR0, ep_num, USB_ReadCsr32(U3D_RX1CSR0, ep_num) & (~0x7FF));
		USB_WriteCsr32(U3D_RX1CSR1, ep_num, 0);
		USB_WriteCsr32(U3D_RX1CSR2, ep_num, 0);
		ep_setting = &g_u3d_setting.ep_setting[ep_num + MAX_EP_NUM];
		ep_setting->enabled = 0;
	}
}

/**
* mu3d_hal_ep_enable - configure ep
*@args - arg1: ep number, arg2: dir, arg3: transfer type, arg4: max packet size, arg5: interval,
* arg6: slot, arg7: burst, arg8: mult
*/
void _ex_mu3d_hal_ep_enable(unsigned char ep_num, enum USB_DIR dir, enum TRANSFER_TYPE type, int maxp,
			    char interval, char slot, char burst, char mult)
{
	int ep_index = 0;
	int used_before;
	unsigned char fifosz = 0, max_pkt, binterval;
	int csr0, csr1, csr2;
	struct USB_EP_SETTING *ep_setting;
	unsigned char update_FIFOadd = 0;

	os_printk(K_INFO, "%s\n", __func__);

	/*TODO: Enable in future. */
	/*Enable Burst, NumP=0, EoB */
	/* os_writel( U3D_USB3_EPCTRL_CAP, os_readl(U3D_USB3_EPCTRL_CAP) | TX_NUMP_0_EN | SET_EOB_EN | TX_BURST_EN); */

	/*
	 * Default value of U3D_USB3_EPCTRL_CAP
	 * 1. tx_burst_en 1'b1
	 * 2. set_eob_en 1'b0
	 * 3. usb3_iso_crc_chk_dis 1'b1
	 * 4. send_stall_clr_pp_en 1'b1
	 * 5. tx_nump_0_en 1'b0
	 */

	if (slot > MAX_SLOT) {
		os_printk(K_ALET, "[ERROR]Configure wrong slot number(MAX=%d, Not=%d)\n", MAX_SLOT,
			  slot);
		slot = MAX_SLOT;
	}

	if (type == USB_CTRL) {
		ep_setting = &g_u3d_setting.ep_setting[0];
		ep_setting->fifosz = maxp;
		ep_setting->maxp = maxp;
		csr0 = os_readl(U3D_EP0CSR) & EP0_W1C_BITS;
		csr0 |= maxp;
		os_writel(U3D_EP0CSR, csr0);

		os_setmsk(U3D_USB2_RX_EP_DATAERR_INTR, BIT16);	/* EP0 data error interrupt */
		return;
	}

	if (dir == USB_TX)
		ep_index = ep_num;
	else if (dir == USB_RX)
		ep_index = ep_num + MAX_EP_NUM;
	else
		WARN_ON(1);

	ep_setting = &g_u3d_setting.ep_setting[ep_index];
	used_before = ep_setting->fifoaddr;

	if (ep_setting->enabled)
		return;

	binterval = interval;
	if (dir == USB_TX) {
		if ((g_TxFIFOadd + maxp * (slot + 1) > os_readl(U3D_CAP_EPNTXFFSZ))
		    && (!used_before)) {
			os_printk(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			os_printk(K_ALET, "g_FIFOadd :%x\n", g_TxFIFOadd);
			os_printk(K_ALET, "maxp :%d\n", maxp);
			os_printk(K_ALET, "mult :%d\n", slot);
			WARN_ON(1);
		}
	} else {
		if ((g_RxFIFOadd + maxp * (slot + 1) > os_readl(U3D_CAP_EPNRXFFSZ))
		    && (!used_before)) {
			os_printk(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			os_printk(K_ALET, "g_FIFOadd :%x\n", g_RxFIFOadd);
			os_printk(K_ALET, "maxp :%d\n", maxp);
			os_printk(K_ALET, "mult :%d\n", slot);
			WARN_ON(1);
		}
	}

	ep_setting->transfer_type = type;

	if (dir == USB_TX) {
		if (!ep_setting->fifoaddr) {
			ep_setting->fifoaddr = g_TxFIFOadd;
			update_FIFOadd = 1;
		}
	} else {
		if (!ep_setting->fifoaddr) {
			ep_setting->fifoaddr = g_RxFIFOadd;
			update_FIFOadd = 1;
		}
	}

	ep_setting->fifosz = maxp;
	ep_setting->maxp = maxp;
	ep_setting->dir = dir;
	ep_setting->enabled = 1;

	/*
	 * Indicate the Tx/Rx FIFO size of 2^n bytes, (ex: value 10 means 2^10 = 1024 bytes.)
	 * It means the value of Tx/RxFIFOSz _MUST_ be a power of 2 (2^n).
	 * It can _NOT_ be 5,48,180 the kind of values.
	 * TX/RXFIFOSEGSIZE should be equal or bigger than 4. The Tx/RxFIFO
	 * size of 2^n bytes also should be equal
	 * or bigger than TX/RXMAXPKTSZ. This EndPoint occupy total memory size
	 * (TX/RX_SLOT + 1 )*2^TX/RXFIFOSEGSIZE bytes.
	 */
	if (maxp <= 16)
		fifosz = USB_FIFOSZ_SIZE_16;
	else if (maxp <= 32)
		fifosz = USB_FIFOSZ_SIZE_32;
	else if (maxp <= 64)
		fifosz = USB_FIFOSZ_SIZE_64;
	else if (maxp <= 128)
		fifosz = USB_FIFOSZ_SIZE_128;
	else if (maxp <= 256)
		fifosz = USB_FIFOSZ_SIZE_256;
	else if (maxp <= 512)
		fifosz = USB_FIFOSZ_SIZE_512;
	else if (maxp <= 32768)
		fifosz = USB_FIFOSZ_SIZE_1024;
	else {
		os_printk(K_ERR, "%s Wrong MAXP\n", __func__);
		fifosz = USB_FIFOSZ_SIZE_1024;
	}

	if (dir == USB_TX) {
		/* CSR0 */
		csr0 = USB_ReadCsr32(U3D_TX1CSR0, ep_num) & ~TX_TXMAXPKTSZ;
		csr0 |= (maxp & TX_TXMAXPKTSZ);

#ifdef USE_SSUSB_QMU
		/* from SSUSB Device Programming Guide(Revision 0.1) page26
		 * TX EP Setting of QMU
		 * TXnCSR0.TxMaxPktSz
		 * TXnCSR1.SS_BURST/TxType/TxSlot
		 * TXnCSR1.Tx_Max_Pkt/Tx_Mult (ISO Only)
		 * TXnCSR2.TxFIFOAddr/TxFIFOSegSize
		 * TXnCSR2.TxBInterval (SS ISO/INT Only)
		 * TxnCSR0.AutoSet = 0
		 * TxnCSR0.DMARegEn = 1
		 */
		csr0 &= ~TX_AUTOSET;
		csr0 |= TX_DMAREQEN;
#else
#ifdef AUTOSET
		csr0 |= TX_AUTOSET;
#endif
		/*Disable DMA, Use PIO mode */
		csr0 &= ~TX_DMAREQEN;
#endif

		/* CSR1 */
		max_pkt = (burst + 1) * (mult + 1) - 1;
		csr1 = (burst & SS_TX_BURST);
		csr1 |= (slot << TX_SLOT_OFST) & TX_SLOT;
		csr1 |= (max_pkt << TX_MAX_PKT_OFST) & TX_MAX_PKT;
		csr1 |= (mult << TX_MULT_OFST) & TX_MULT;

		/* CSR2 */
		csr2 = (ep_setting->fifoaddr >> 4) & TXFIFOADDR;
		csr2 |= (fifosz << TXFIFOSEGSIZE_OFST) & TXFIFOSEGSIZE;

		if (type == USB_BULK) {
			csr1 |= TYPE_BULK;
		} else if (type == USB_INTR) {
			csr1 |= TYPE_INT;
			csr2 |= (binterval << TXBINTERVAL_OFST) & TXBINTERVAL;
		} else if (type == USB_ISO) {
			csr1 |= TYPE_ISO;
			csr2 |= (binterval << TXBINTERVAL_OFST) & TXBINTERVAL;
		}
#ifdef USE_SSUSB_QMU
		/*Disable Endpoint interrupt */
		os_setmsk(U3D_EPIECR, (BIT0 << ep_num));
		/* Enable QMU */
		os_setmsk(U3D_QGCSR, QMU_TX_EN(ep_num));
		/* Enable QMU Done interrupt */
		os_setmsk(U3D_QIESR0, QMU_TX_EN(ep_num));
#endif
		USB_WriteCsr32(U3D_TX1CSR0, ep_num, csr0);
		USB_WriteCsr32(U3D_TX1CSR1, ep_num, csr1);
		USB_WriteCsr32(U3D_TX1CSR2, ep_num, csr2);

		os_printk(K_DEBUG, "[CSR]U3D_TX%dCSR0 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_TX1CSR0, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_TX%dCSR1 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_TX1CSR1, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_TX%dCSR2 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_TX1CSR2, ep_num));

	} else if (dir == USB_RX) {
		/* CSR0 */
		csr0 = USB_ReadCsr32(U3D_RX1CSR0, ep_num) & ~RX_RXMAXPKTSZ;
		csr0 |= (maxp & RX_RXMAXPKTSZ);

#ifdef USE_SSUSB_QMU
		/* from SSUSB Device Programming Guide(Revision 0.1) page32
		 * RX EP Setting of QMU
		 * RXnCSR0.RxMaxPktSz
		 * RXnCSR1.SS_BURST/RxType/RxSlot
		 * RXnCSR1.Rx_Max_Pkt/Rx_Mult (ISO Only)
		 * RXnCSR2.RxFIFOAddr/RxFIFOSegSize
		 * RXnCSR2.RxBInterval (SS ISO/INT Only)
		 * RxnCSR0.AutoClear = 0
		 * RxnCSR0.DMARegEn = 1
		 */
		csr0 &= ~RX_AUTOCLEAR;
		csr0 |= RX_DMAREQEN;
#else
#ifdef AUTOCLEAR
		csr0 |= RX_AUTOCLEAR;
#endif
		/*Disable DMA, Use PIO mode */
		csr0 &= ~RX_DMAREQEN;
#endif

		/* CSR1 */
		max_pkt = (burst + 1) * (mult + 1) - 1;
		csr1 = (burst & SS_RX_BURST);
		csr1 |= (slot << RX_SLOT_OFST) & RX_SLOT;
		csr1 |= (max_pkt << RX_MAX_PKT_OFST) & RX_MAX_PKT;
		csr1 |= (mult << RX_MULT_OFST) & RX_MULT;

		/* CSR2 */
		csr2 = (ep_setting->fifoaddr >> 4) & RXFIFOADDR;
		csr2 |= (fifosz << RXFIFOSEGSIZE_OFST) & RXFIFOSEGSIZE;

		if (type == USB_BULK) {
			csr1 |= TYPE_BULK;
		} else if (type == USB_INTR) {
			csr1 |= TYPE_INT;
			csr2 |= (binterval << RXBINTERVAL_OFST) & RXBINTERVAL;
		} else if (type == USB_ISO) {
			csr1 |= TYPE_ISO;
			csr2 |= (binterval << RXBINTERVAL_OFST) & RXBINTERVAL;
		}
#ifdef USE_SSUSB_QMU
		/*Disable Endpoint interrupt */
		os_setmsk(U3D_EPIECR, (BIT16 << ep_num));
		/* Enable QMU */
		os_setmsk(U3D_QGCSR, QMU_TX_EN(ep_num));
		/*Enable QMU Done interrupt */
		os_setmsk(U3D_QIESR0, QMU_RX_EN(ep_num));
#endif
		USB_WriteCsr32(U3D_RX1CSR0, ep_num, csr0);
		USB_WriteCsr32(U3D_RX1CSR1, ep_num, csr1);
		USB_WriteCsr32(U3D_RX1CSR2, ep_num, csr2);

		os_printk(K_DEBUG, "[CSR]U3D_RX%dCSR0 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_RX1CSR0, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_RX%dCSR1 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_RX1CSR1, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_RX%dCSR2 :%x\n", ep_num,
			  USB_ReadCsr32(U3D_RX1CSR2, ep_num));

		os_setmsk(U3D_USB2_RX_EP_DATAERR_INTR, BIT16 << ep_num);	/* EPn data error interrupt */
	} else {
		os_printk(K_ERR, "WHAT THE DIRECTION IS?!?!\n");
		WARN_ON(1);
	}

	if (update_FIFOadd == 1) {
		if (dir == USB_TX) {
			/* The minimum unit of FIFO address is _16_ bytes.
			 * So let the offset of each EP fifo address aligns _16_ bytes.
			 */
			int fifo_offset = 0;

			if ((maxp & 0xF))
				fifo_offset = ((maxp + 16) >> 4) << 4;
			else
				fifo_offset = maxp;

			g_TxFIFOadd += (fifo_offset * (slot + 1));
		} else {
			int fifo_offset = 0;

			if ((maxp & 0xF))
				fifo_offset = ((maxp + 16) >> 4) << 4;
			else
				fifo_offset = maxp;

			g_RxFIFOadd += (fifo_offset * (slot + 1));
		}
	}
}

void mu3d_hal_ep_enable(unsigned char ep_num, enum USB_DIR dir, enum TRANSFER_TYPE type, int maxp,
			char interval, char slot, char burst, char mult)
{
	int ep_index = 0;
	int used_before;
	unsigned char fifosz = 0, max_pkt, binterval;
	int csr0, csr1, csr2;
	struct USB_EP_SETTING *ep_setting;
	unsigned char update_FIFOadd = 0;

	/*Enable Burst, NumP=0, EoB */
	os_writel(U3D_USB3_EPCTRL_CAP,
		  os_readl(U3D_USB3_EPCTRL_CAP) | TX_NUMP_0_EN | SET_EOB_EN | TX_BURST_EN);

	if (slot > MAX_SLOT) {
		os_printk(K_ALET,
			  "!!!!!!!!!!!!!!Configure wrong slot number!!!!!!!!!(MAX=%d, Not=%d)\n",
			  MAX_SLOT, slot);
		slot = MAX_SLOT;
	}

	if (type == USB_CTRL) {

		ep_setting = &g_u3d_setting.ep_setting[0];
		ep_setting->fifosz = maxp;
		ep_setting->maxp = maxp;
		csr0 = os_readl(U3D_EP0CSR) & EP0_W1C_BITS;
		csr0 |= maxp;
		os_writel(U3D_EP0CSR, csr0);

		os_setmsk(U3D_USB2_RX_EP_DATAERR_INTR, BIT16);	/* EP0 data error interrupt */
		return;
	}

	if (dir == USB_TX)
		ep_index = ep_num;
	else if (dir == USB_RX)
		ep_index = ep_num + MAX_EP_NUM;
	else
		WARN_ON(1);

	ep_setting = &g_u3d_setting.ep_setting[ep_index];
	used_before = ep_setting->fifoaddr;

	if (ep_setting->enabled)
		return;

	binterval = interval;
	if (dir == USB_TX) {
		if ((g_TxFIFOadd + maxp * (slot + 1) > os_readl(U3D_CAP_EPNTXFFSZ))
		    && (!used_before)) {
			os_printk(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			os_printk(K_ALET, "g_FIFOadd :%x\n", g_TxFIFOadd);
			os_printk(K_ALET, "maxp :%d\n", maxp);
			os_printk(K_ALET, "mult :%d\n", slot);
			WARN_ON(1);
		}
	} else {
		if ((g_RxFIFOadd + maxp * (slot + 1) > os_readl(U3D_CAP_EPNRXFFSZ))
		    && (!used_before)) {
			os_printk(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			os_printk(K_ALET, "g_FIFOadd :%x\n", g_RxFIFOadd);
			os_printk(K_ALET, "maxp :%d\n", maxp);
			os_printk(K_ALET, "mult :%d\n", slot);
			WARN_ON(1);
		}
	}

	ep_setting->transfer_type = type;

	if (dir == USB_TX) {
		if (!ep_setting->fifoaddr) {
			ep_setting->fifoaddr = g_TxFIFOadd;
			update_FIFOadd = 1;
		}
	} else {
		if (!ep_setting->fifoaddr) {
			ep_setting->fifoaddr = g_RxFIFOadd;
			update_FIFOadd = 1;
		}
	}

	ep_setting->fifosz = maxp;
	ep_setting->maxp = maxp;
	ep_setting->dir = dir;
	ep_setting->enabled = 1;

	if (maxp <= 16)
		fifosz = USB_FIFOSZ_SIZE_16;
	else if (maxp <= 32)
		fifosz = USB_FIFOSZ_SIZE_32;
	else if (maxp <= 64)
		fifosz = USB_FIFOSZ_SIZE_64;
	else if (maxp <= 128)
		fifosz = USB_FIFOSZ_SIZE_128;
	else if (maxp <= 256)
		fifosz = USB_FIFOSZ_SIZE_256;
	else if (maxp <= 512)
		fifosz = USB_FIFOSZ_SIZE_512;
	else if (maxp <= 32768)
		fifosz = USB_FIFOSZ_SIZE_1024;
	else {
		os_printk(K_ERR, "%s Wrong MAXP\n", __func__);
		fifosz = USB_FIFOSZ_SIZE_1024;
	}

	if (dir == USB_TX) {
		/* CSR0 */
		csr0 = USB_ReadCsr32(U3D_TX1CSR0, ep_num) & ~TX_TXMAXPKTSZ;
		csr0 |= (maxp & TX_TXMAXPKTSZ);
#if (BUS_MODE == PIO_MODE)
#ifdef AUTOSET
		csr0 |= TX_AUTOSET;
#endif
		csr0 &= ~TX_DMAREQEN;
#endif

		/* CSR1 */
		max_pkt = (burst + 1) * (mult + 1) - 1;
		csr1 = (burst & SS_TX_BURST);
		csr1 |= (slot << TX_SLOT_OFST) & TX_SLOT;
		csr1 |= (max_pkt << TX_MAX_PKT_OFST) & TX_MAX_PKT;
		csr1 |= (mult << TX_MULT_OFST) & TX_MULT;

		/* CSR2 */
		csr2 = (ep_setting->fifoaddr >> 4) & TXFIFOADDR;
		csr2 |= (fifosz << TXFIFOSEGSIZE_OFST) & TXFIFOSEGSIZE;

		if (type == USB_BULK) {
			csr1 |= TYPE_BULK;
		} else if (type == USB_INTR) {
			csr1 |= TYPE_INT;
			csr2 |= (binterval << TXBINTERVAL_OFST) & TXBINTERVAL;
		} else if (type == USB_ISO) {
			csr1 |= TYPE_ISO;
			csr2 |= (binterval << TXBINTERVAL_OFST) & TXBINTERVAL;
		}
#ifdef USE_SSUSB_QMU
		os_writel(U3D_EPIECR, os_readl(U3D_EPIECR) | (BIT0 << ep_num));
#endif
		USB_WriteCsr32(U3D_TX1CSR0, ep_num, csr0);
		USB_WriteCsr32(U3D_TX1CSR1, ep_num, csr1);
		USB_WriteCsr32(U3D_TX1CSR2, ep_num, csr2);

		os_printk(K_DEBUG, "[CSR]U3D_TX1CSR0 :%x\n", USB_ReadCsr32(U3D_TX1CSR0, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_TX1CSR1 :%x\n", USB_ReadCsr32(U3D_TX1CSR1, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_TX1CSR2 :%x\n", USB_ReadCsr32(U3D_TX1CSR2, ep_num));

	} else if (dir == USB_RX) {
		/* CSR0 */
		csr0 = USB_ReadCsr32(U3D_RX1CSR0, ep_num) & ~RX_RXMAXPKTSZ;
		csr0 |= (maxp & RX_RXMAXPKTSZ);
#if (BUS_MODE == PIO_MODE)
#ifdef AUTOCLEAR
		csr0 |= RX_AUTOCLEAR;
#endif
		csr0 &= ~RX_DMAREQEN;
#endif

		/* CSR1 */
		max_pkt = (burst + 1) * (mult + 1) - 1;
		csr1 = (burst & SS_RX_BURST);
		csr1 |= (slot << RX_SLOT_OFST) & RX_SLOT;
		csr1 |= (max_pkt << RX_MAX_PKT_OFST) & RX_MAX_PKT;
		csr1 |= (mult << RX_MULT_OFST) & RX_MULT;

		/* CSR2 */
		csr2 = (ep_setting->fifoaddr >> 4) & RXFIFOADDR;
		csr2 |= (fifosz << RXFIFOSEGSIZE_OFST) & RXFIFOSEGSIZE;

		if (type == USB_BULK) {
			csr1 |= TYPE_BULK;
		} else if (type == USB_INTR) {
			csr1 |= TYPE_INT;
			csr2 |= (binterval << RXBINTERVAL_OFST) & RXBINTERVAL;
		} else if (type == USB_ISO) {
			csr1 |= TYPE_ISO;
			csr2 |= (binterval << RXBINTERVAL_OFST) & RXBINTERVAL;
		}
#ifdef USE_SSUSB_QMU
		os_writel(U3D_EPIECR, os_readl(U3D_EPIECR) | (BIT16 << ep_num));
#endif
		USB_WriteCsr32(U3D_RX1CSR0, ep_num, csr0);
		USB_WriteCsr32(U3D_RX1CSR1, ep_num, csr1);
		USB_WriteCsr32(U3D_RX1CSR2, ep_num, csr2);

		os_printk(K_DEBUG, "[CSR]U3D_RX1CSR0 :%x\n", USB_ReadCsr32(U3D_RX1CSR0, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_RX1CSR1 :%x\n", USB_ReadCsr32(U3D_RX1CSR1, ep_num));
		os_printk(K_DEBUG, "[CSR]U3D_RX1CSR2 :%x\n", USB_ReadCsr32(U3D_RX1CSR2, ep_num));

		os_setmsk(U3D_USB2_RX_EP_DATAERR_INTR, BIT16 << ep_num);	/* EPn data error interrupt */
	} else {
		os_printk(K_ERR, "WHAT THE DIRECTION IS?!?!\n");
		WARN_ON(1);
	}

	if (update_FIFOadd == 1) {
		if (dir == USB_TX) {
			if (maxp == 1023)
				g_TxFIFOadd += (1024 * (slot + 1));
			else
				g_TxFIFOadd += (maxp * (slot + 1));
		} else {
			if (maxp == 1023)
				g_RxFIFOadd += (1024 * (slot + 1));
			else
				g_RxFIFOadd += (maxp * (slot + 1));
		}
	}
}

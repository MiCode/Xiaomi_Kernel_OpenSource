/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/prefetch.h>
#include <linux/usb/phy.h>
#include <linux/usb/usb_phy_generic.h>


#include <linux/input.h>

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#endif

#include "musb_core.h"
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "mu3d_hal_qmu_drv.h"

#ifdef CONFIG_SSUSB_PROJECT_PHY
#include <mu3phy/mtk-phy-asic.h>
#endif
#include "xhci-mtk.h"

#include "ssusb_io.h"


static struct musb_fifo_cfg mtu3d_cfg[] __initdata = {
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 2, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 2, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 3, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 3, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 4, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 4, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 5, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 5, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 6, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 6, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 7, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 7, .style = FIFO_RX, .maxpacket = 1024,},
	{.hw_ep_num = 8, .style = FIFO_TX, .maxpacket = 1024,},
	{.hw_ep_num = 8, .style = FIFO_RX, .maxpacket = 1024,},
};

#define CHIP_SW_VER_01 0


static int mtu3d_set_power(struct usb_phy *x, unsigned mA);

static inline void mtu3d_u3_ltssm_intr_handler(struct musb *musb, u32 dwLtssmValue)
{
	static u32 soft_conn_num;
	void __iomem *mbase = musb->mac_base;

	if (dwLtssmValue & SS_DISABLE_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: SS_DISABLE_INTR [%d] & Set SOFT_CONN = 1\n",
			 soft_conn_num++);
		/* enable U2 link. after host reset, HS/FS EP0 configuration is applied in musb_g_reset */
		mu3d_clrmsk(musb->sif_base, U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN);
		mu3d_setmsk(mbase, U3D_POWER_MANAGEMENT, SOFT_CONN);
	}

	if (dwLtssmValue & ENTER_U0_INTR) {
		soft_conn_num = 0;
		/* do not apply U3 EP0 setting again, if the speed is already U3 */
		/* LTSSM may go to recovery and back to U0 */
		if (musb->g.speed != USB_SPEED_SUPER) {
			mu3d_dbg(K_INFO, "LTSSM: ENTER_U0_INTR %d\n", musb->g.speed);
			musb_conifg_ep0(musb);
		}
	}

	if (dwLtssmValue & VBUS_FALL_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: VBUS_FALL_INTR\n");
		/* mu3d_hal_pdn_ip_port(1, 1, 1, 1); */
		mu3d_hal_u3dev_dis(musb);
	}

	if (dwLtssmValue & VBUS_RISE_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: VBUS_RISE_INTR\n");
		mu3d_hal_u3dev_en(musb);
	}

	if (dwLtssmValue & ENTER_U3_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: ENTER_U3_INTR\n");
		/* mu3d_hal_pdn_ip_port(0, 0, 1, 0); */
	}

	if (dwLtssmValue & EXIT_U3_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: EXIT_U3_INTR\n");
		/* mu3d_hal_pdn_ip_port(1, 0, 1, 0); */
	}

	if (dwLtssmValue & U3_RESUME_INTR) {
		mu3d_dbg(K_INFO, "LTSSM: RESUME_INTR\n");
		/* mu3d_hal_pdn_ip_port(1, 0, 1, 0); */
		mu3d_setmsk(mbase, U3D_LINK_POWER_CONTROL, UX_EXIT);
	}


	/*7.5.12.2 Hot Reset Requirements
	 * 1. A downstream port shall reset its Link Error Count as defined in Section 7.4.2.
	 * 2. A downstream port shall reset its PM timers and the associated U1 and U2 timeout values to zero.
	 * 3. The port Configuration information shall remain unchanged (refer to Section 8.4.6 for details).
	 * 4. The port shall maintain its transmitter specifications defined in Table 6-10.
	 * 5. The port shall maintain its low-impedance receiver termination (RRX-DC) defined in Table 6-13.
	 */
	if (dwLtssmValue & HOT_RST_INTR) {
		int link_err_cnt;
		int timeout_val;

		mu3d_dbg(K_INFO, "LTSSM: HOT_RST_INTR\n");
		/* Clear link error count */
		link_err_cnt = mu3d_readl(mbase, U3D_LINK_ERR_COUNT);
		mu3d_dbg(K_INFO, "LTSSM: link_err_cnt=%x\n", link_err_cnt);
		mu3d_writel(mbase, U3D_LINK_ERR_COUNT, CLR_LINK_ERR_CNT);

		/* Clear U1 & U2 Enable */
		mu3d_clrmsk(mbase, U3D_LINK_POWER_CONTROL,
			    (SW_U1_ACCEPT_ENABLE | SW_U2_ACCEPT_ENABLE));

		musb->bU1Enabled = 0;
		musb->bU2Enabled = 0;

		/* Reset U1 & U2 timeout value */
		timeout_val = mu3d_readl(mbase, U3D_LINK_UX_INACT_TIMER);
		mu3d_dbg(K_INFO, "LTSSM: timer_val =%x\n", timeout_val);
		timeout_val &= ~(U1_INACT_TIMEOUT_VALUE | DEV_U2_INACT_TIMEOUT_VALUE);
		mu3d_writel(mbase, U3D_LINK_UX_INACT_TIMER, timeout_val);
	}

	if (dwLtssmValue & SS_INACTIVE_INTR)
		mu3d_dbg(K_INFO, "LTSSM: SS_INACTIVE_INTR\n");
	if (dwLtssmValue & RECOVERY_INTR)
		mu3d_dbg(K_DEBUG, "LTSSM: RECOVERY_INTR\n");

	/* A completion of a Warm Reset shall result in the following.
	 * 1. A downstream port shall reset its Link Error Count.
	 * 2. Port configuration information of an upstream port shall be reset to default values. Refer to
	 *       Sections 8.4.5 and 8.4.6 for details.
	 * 3. The PHY level variables (such as Rx equalization settings) shall be reinitialized or retrained.
	 * 4. The LTSSM of a port shall transition to U0 through RxDetect and Polling.
	 */
	if (dwLtssmValue & WARM_RST_INTR) {
		int link_err_cnt;

		mu3d_dbg(K_INFO, "LTSSM: WARM_RST_INTR\n");
		/* Clear link error count */
		link_err_cnt = mu3d_readl(mbase, U3D_LINK_ERR_COUNT);
		mu3d_dbg(K_INFO, "LTSSM: link_err_cnt=%x\n", link_err_cnt);
		mu3d_writel(mbase, U3D_LINK_ERR_COUNT, CLR_LINK_ERR_CNT);
	}

	if (dwLtssmValue & ENTER_U2_INTR)
		mu3d_dbg(K_DEBUG, "LTSSM: ENTER_U2_INTR\n");
	if (dwLtssmValue & ENTER_U1_INTR)
		mu3d_dbg(K_DEBUG, "LTSSM: ENTER_U1_INTR\n");
	if (dwLtssmValue & RXDET_SUCCESS_INTR)
		mu3d_dbg(K_INFO, "LTSSM: RXDET_SUCCESS_INTR\n");

}

static inline void mtu3d_u2_common_intr_handler(struct musb *musb, u32 dwIntrUsbValue)
{
	if (dwIntrUsbValue & DISCONN_INTR) {
		/* mu3d_hal_pdn_ip_port(1, 0, 1, 1); */

		mu3d_dbg(K_NOTICE, "[U2 DISCONN_INTR] Set SOFT_CONN=0\n");
		mu3d_clrmsk(musb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);

		/*TODO-J: ADD musb_g_disconnect(musb);?? */
	}

	if (dwIntrUsbValue & LPM_INTR) {
		mu3d_dbg(K_NOTICE, "[U2 LPM interrupt]\n");

		/* if (!((os_readl(U3D_POWER_MANAGEMENT) & LPM_HRWE))) { */
		/* mu3d_hal_pdn_ip_port(0, 0, 0, 1); */
		/* } */
	}

	if (dwIntrUsbValue & LPM_RESUME_INTR) {
		if (!(mu3d_readl(musb->mac_base, U3D_POWER_MANAGEMENT) & LPM_HRWE)) {
			/* mu3d_hal_pdn_ip_port(1, 0, 0, 1); */

			mu3d_setmsk(musb->mac_base, U3D_USB20_MISC_CONTROL, LPM_U3_ACK_EN);
		}
	}

	if (dwIntrUsbValue & SUSPEND_INTR) {
		mu3d_dbg(K_NOTICE, "[U2 SUSPEND_INTR]\n");
		/* mu3d_hal_pdn_ip_port(0, 0, 0, 1); */
	}

	if (dwIntrUsbValue & RESUME_INTR) {
		mu3d_dbg(K_NOTICE, "[U2 RESUME_INTR]\n");
		/* mu3d_hal_pdn_ip_port(1, 0, 0, 1); */
	}

	if (dwIntrUsbValue & RESET_INTR)
		mu3d_dbg(K_NOTICE, "[U2 RESET_INTR]\n");

}

static inline void mtu3d_link_intr_handler(struct musb *musb, u32 dwLinkIntValue)
{
	void __iomem *mbase = musb->mac_base;
	u32 dwTemp;

	dwTemp = mu3d_readl(mbase, U3D_DEVICE_CONF) & SSUSB_DEV_SPEED;
	/* mu3d_hal_pdn_cg_en(); */

	switch (dwTemp) {
	case SSUSB_SPEED_FULL:
		mu3d_dbg(K_ALET, "USB Speed = Full Speed\n");

#ifdef CONFIG_SSUSB_PROJECT_PHY
		/* Comment from CC Chou.
		 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
		 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
		 */
		mu3d_setmsk(musb->sif_base, U3D_U2PHYDCR1,
			    (0x1 << E60802_RG_USB20_SW_PLLMODE_OFST));

		/*BESLCK = 0 < BESLCK_U3 = 1 < BESLDCK = 15 */
		mu3d_writel(mbase, U3D_USB20_LPM_PARAMETER, 0x10f0);

		/*
		 * The default value of LPM_BESL_STALL and LPM_BESLD_STALL are 1.
		 * So Does _NOT_ need to set.
		 */
		/*os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL)); */
#else
		/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15 */
		mu3d_writel(mbase, U3D_USB20_LPM_PARAMETER, 0xa4f0);
		mu3d_setmsk(mbase, U3D_POWER_MANAGEMENT, (LPM_BESL_STALL | LPM_BESLD_STALL));
#endif
		break;

	case SSUSB_SPEED_HIGH:
		mu3d_dbg(K_ALET, "USB Speed = High Speed\n");

#ifdef CONFIG_SSUSB_PROJECT_PHY
		/* Comment from CC Chou.
		 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
		 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
		 */
		mu3d_setmsk(musb->sif_base, U3D_U2PHYDCR1,
			    (0x1 << E60802_RG_USB20_SW_PLLMODE_OFST));

		/*BESLCK = 0 < BESLCK_U3 = 1 < BESLDCK = 15 */
		mu3d_writel(mbase, U3D_USB20_LPM_PARAMETER, 0x10f0);
		/*
		 * The default value of LPM_BESL_STALL and LPM_BESLD_STALL are 1.
		 * So Does _NOT_ need to set.
		 */
		/*os_setmsk(U3D_POWER_MANAGEMENT, (LPM_BESL_STALL|LPM_BESLD_STALL)); */
#else
		/*BESLCK = 4 < BESLCK_U3 = 10 < BESLDCK = 15 */
		mu3d_writel(mbase, U3D_USB20_LPM_PARAMETER, 0xa4f0);
		mu3d_setmsk(mbase, U3D_POWER_MANAGEMENT, (LPM_BESL_STALL | LPM_BESLD_STALL));
#endif
		break;

	case SSUSB_SPEED_SUPER:
		mu3d_dbg(K_ALET, "USB Speed = Super Speed\n");
		break;

	default:
		mu3d_dbg(K_ALET, "USB Speed = Invalid\n");
		break;
	}
}


static inline void mtu3d_otg_isr(struct musb *musb, u32 dwOtgIntValue)
{
	void __iomem *sif_base = musb->sif_base;
	int vbus_pc = is_ssusb_connected_to_pc(musb->ssusb);
	int vvalid = mu3d_readl(musb->sif_base, U3D_SSUSB_OTG_STS) & SSUSB_VBUS_VALID;

	if (dwOtgIntValue & VBUS_CHG_INTR) {
		mu3d_dbg(K_DEBUG, "OTG: VBUS_CHG_INTR (vbus_pc: %d/vvalid: %d)\n", vbus_pc, vvalid);
		mu3d_setmsk(sif_base, U3D_SSUSB_OTG_STS_CLR, SSUSB_VBUS_INTR_CLR);

		if (!vvalid && (musb->g.speed != USB_SPEED_UNKNOWN))
			musb_g_disconnect(musb);
		else if (musb->g.speed == USB_SPEED_UNKNOWN)
			mu3d_dbg(K_DEBUG, "%s already disconnect\n", __func__);
	}
}


static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long flags;
	irqreturn_t retval = IRQ_HANDLED;
	struct musb *musb = __hci;
	void __iomem *mbase = musb->mac_base;

	u32 dwL1Value = 0;
	u32 dwIntrUsbValue = 0;
	u32 dwDmaIntrValue = 0;
	u32 dwIntrEPValue = 0;
	u16 wIntrTxValue = 0;
	u16 wIntrRxValue = 0;
	u32 dwLtssmValue = 0;
	u32 dwLinkIntValue = 0;
#if defined(USE_SSUSB_QMU)
	u32 wIntrQMUDoneValue = 0;
	u32 wIntrQMUValue = 0;
#endif
	u32 dwOtgIntValue = 0;


	spin_lock_irqsave(&musb->lock, flags);

	dwL1Value = mu3d_readl(mbase, U3D_LV1ISR) & mu3d_readl(mbase, U3D_LV1IER);

	if (dwL1Value & EP_CTRL_INTR) {
		u32 dwRxEpDataerrVal = mu3d_readl(mbase, U3D_USB2_RX_EP_DATAERR_INTR);

		if (dwRxEpDataerrVal != 0) {
			/* Write 1 clear */
			mu3d_writel(mbase, U3D_USB2_RX_EP_DATAERR_INTR, dwRxEpDataerrVal);
			mu3d_dbg(K_INFO, "===L1[%x] RxDataErr[%x]\n", dwL1Value,
				 (dwRxEpDataerrVal >> USB2_RX_EP_DATAERR_INTR_EN_OFST
				  && dwRxEpDataerrVal));
		}
		dwLinkIntValue =
		    mu3d_readl(mbase, U3D_DEV_LINK_INTR) & mu3d_readl(mbase,
								      U3D_DEV_LINK_INTR_ENABLE);
		if (dwLinkIntValue != 0) {
			/* Write 1 clear */
			mu3d_writel(mbase, U3D_DEV_LINK_INTR, dwLinkIntValue);
			mu3d_dbg(K_INFO, "===L1[%x] LinkInt[%x]\n", dwL1Value, dwLinkIntValue);
		}
	}

	if (dwL1Value & MAC2_INTR) {
		dwIntrUsbValue =
		    mu3d_readl(mbase, U3D_COMMON_USB_INTR) & mu3d_readl(mbase,
									U3D_COMMON_USB_INTR_ENABLE);
		/* Write 1 clear */
		mu3d_writel(mbase, U3D_COMMON_USB_INTR, dwIntrUsbValue);
		mu3d_dbg(K_INFO, "===L1[%x] U2[%x]\n", dwL1Value, dwIntrUsbValue);
	}

	if (dwL1Value & DMA_INTR) {
		dwDmaIntrValue = mu3d_readl(mbase, U3D_DMAISR) & mu3d_readl(mbase, U3D_DMAIER);
		/* Write 1 clear */
		mu3d_writel(mbase, U3D_DMAISR, dwDmaIntrValue);
		mu3d_dbg(K_INFO, "===L1[%x] DMA[%x]\n", dwL1Value, dwDmaIntrValue);
	}

	if (dwL1Value & MAC3_INTR) {
		dwLtssmValue =
		    mu3d_readl(mbase, U3D_LTSSM_INTR) & mu3d_readl(mbase, U3D_LTSSM_INTR_ENABLE);
		/* Write 1 clear */
		mu3d_writel(mbase, U3D_LTSSM_INTR, dwLtssmValue);
		mu3d_dbg(K_DEBUG, "===L1[%x] LTSSM[%x]\n", dwL1Value, dwLtssmValue);
	}
#ifdef USE_SSUSB_QMU
	if (dwL1Value & QMU_INTR) {
		wIntrQMUValue = mu3d_readl(mbase, U3D_QISAR1) & mu3d_readl(mbase, U3D_QIER1);
		wIntrQMUDoneValue = mu3d_readl(mbase, U3D_QISAR0) & mu3d_readl(mbase, U3D_QIER0);
		/* Write 1 clear */
		mu3d_writel(mbase, U3D_QISAR0, wIntrQMUDoneValue);
		qmu_dbg(K_DEBUG, "===L1[%x] QMUDone[Tx=%x,Rx=%x] QMU[%x]===\n",
			dwL1Value, ((wIntrQMUDoneValue & 0xFFFF) >> 1), wIntrQMUDoneValue >> 17,
			wIntrQMUValue);
	}
#endif

	if (dwL1Value & BMU_INTR) {
		dwIntrEPValue = mu3d_readl(mbase, U3D_EPISR) & mu3d_readl(mbase, U3D_EPIER);
		wIntrTxValue = dwIntrEPValue & 0xFFFF;
		wIntrRxValue = (dwIntrEPValue >> 16);
		mu3d_writel(mbase, U3D_EPISR, dwIntrEPValue);
		mu3d_dbg(K_DEBUG, "===L1[%x] Tx[%x] Rx[%x]===\n",
			 dwL1Value, wIntrTxValue, wIntrRxValue);

	}

	/*TODO: need to handle SetupEnd Interrupt!!! */

	/*--Handle each interrupt--*/
	if ((dwL1Value & (BMU_INTR | MAC2_INTR)) || dwIntrUsbValue) {
		musb->int_usb = dwIntrUsbValue;
		musb->int_tx = wIntrTxValue;
		musb->int_rx = wIntrRxValue;

		if (musb->int_usb || musb->int_tx || musb->int_rx)
			retval = musb_interrupt(musb);
		else
			mu3d_dbg(K_INFO, "===L1[%x] Nothing can do?? Tx[%x] Rx[%x] U2[%x]===\n",
				 dwL1Value, wIntrTxValue, wIntrRxValue, dwIntrUsbValue);
	}

	if (need_vbus_chg_int(musb)) {
		dwOtgIntValue = mu3d_readl(musb->sif_base, U3D_SSUSB_OTG_STS)
		    & mu3d_readl(musb->sif_base, U3D_SSUSB_OTG_INT_EN);
	}
#if defined(USE_SSUSB_QMU)
	if (wIntrQMUDoneValue) {
		if (musb->qmu_done_intr != 0) {
			musb->qmu_done_intr = wIntrQMUDoneValue | musb->qmu_done_intr;
			qmu_dbg(K_DEBUG, "Has not handle yet %x\n", musb->qmu_done_intr);
		} else
			musb->qmu_done_intr = wIntrQMUDoneValue;
		tasklet_schedule(&musb->qmu_done);
	}

	if (wIntrQMUValue)
		qmu_exception_interrupt(musb, wIntrQMUValue);
#endif

	if (dwLtssmValue)
		mtu3d_u3_ltssm_intr_handler(musb, dwLtssmValue);

	if (dwIntrUsbValue)
		mtu3d_u2_common_intr_handler(musb, dwIntrUsbValue);

	if (dwLinkIntValue & SSUSB_DEV_SPEED_CHG_INTR)
		mtu3d_link_intr_handler(musb, dwLinkIntValue);

	/* use OTG's vbus change interrupt */
	if (need_vbus_chg_int(musb) && dwOtgIntValue)
		mtu3d_otg_isr(musb, dwOtgIntValue);

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}


/*Turn on/off ADA_SSUSB_XTAL_CK 26MHz*/
static void ssusb_xtal_clock_enable(struct ssusb_mtk *ssusb)
{
	/*
	 * 1 *AP_PLL_CON0 =| 0x1 [0]=1: RG_LTECLKSQ_EN
	 * 2 Wait PLL stable (100us)
	 * 3 *AP_PLL_CON0 =| 0x2 [1]=1: RG_LTECLKSQ_LPF_EN
	 * 4 *AP_PLL_CON2 =| 0x1 [0]=1: DA_REF2USB_TX_EN
	 * 5 Wait PLL stable (100us)
	 * 6 *AP_PLL_CON2 =| 0x2 [1]=1: DA_REF2USB_TX_LPF_EN
	 * 7 *AP_PLL_CON2 =| 0x4 [2]=1: DA_REF2USB_TX_OUT_EN
	 */
	mu3d_setmsk(ssusb->uap_pll_con, UAP_PLL_CON0, CON0_RG_LTECLKSQ_EN);
	/*Wait 100 usec */
	udelay(100);

	mu3d_setmsk(ssusb->uap_pll_con, UAP_PLL_CON0, CON0_RG_LTECLKSQ_LPF_EN);
	mu3d_setmsk(ssusb->uap_pll_con, UAP_PLL_CON2, CON2_DA_REF2USB_TX_EN);

	/*Wait 100 usec */
	udelay(100);

	mu3d_setmsk(ssusb->uap_pll_con, UAP_PLL_CON2, CON2_DA_REF2USB_TX_LPF_EN);

	mu3d_setmsk(ssusb->uap_pll_con, UAP_PLL_CON2, CON2_DA_REF2USB_TX_OUT_EN);

}

static inline void ssusb_xtal_clock_disable(struct ssusb_mtk *ssusb)
{
	/*
	 * AP_PLL_CON2 &= 0xFFFFFFF8
	 * [2]=0: DA_REF2USB_TX_OUT_EN
	 * [1]=0: DA_REF2USB_TX_LPF_EN
	 * [0]=0: DA_REF2USB_TX_EN
	 */
	mu3d_clrmsk(ssusb->uap_pll_con, UAP_PLL_CON2, CON2_DA_REF2USB_TX_MASK);

}

static int ssusb_clks_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	ssusb_xtal_clock_enable(ssusb);
	pm_runtime_get_sync(ssusb->dev);
	ret = clk_prepare_enable(ssusb->scp_sys);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable top_usb30\n", __func__);
		goto scp_sys_err;
	}

	ret = clk_prepare_enable(ssusb->peri_usb0);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable peri-usb0\n", __func__);
		goto clken_usb0_err;
	}
	if (ssusb->p1_exist) {
		ret = clk_prepare_enable(ssusb->peri_usb1);
		if (ret) {
			mu3d_dbg(K_ERR, "%s failed to enable peri-usb1\n", __func__);
			goto clken_usb1_err;
		}
	}
	udelay(50);

	return 0;

clken_usb1_err:
	clk_disable_unprepare(ssusb->peri_usb0);

clken_usb0_err:
	clk_disable_unprepare(ssusb->scp_sys);
scp_sys_err:
	pm_runtime_put_sync(ssusb->dev);
	ssusb_xtal_clock_disable(ssusb);
	return -EINVAL;
}

static int ssusb_clks_disable(struct ssusb_mtk *ssusb)
{
	if (ssusb->p1_exist)
		clk_disable_unprepare(ssusb->peri_usb1);

	clk_disable_unprepare(ssusb->peri_usb0);
	if (ssusb->ic_version != CHIP_SW_VER_01) {
		clk_disable_unprepare(ssusb->scp_sys); /* only for ECO IC */
		pm_runtime_put_sync(ssusb->dev);
	}
	ssusb_xtal_clock_disable(ssusb);
	return 0;
}

static int peri_clks_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	ssusb_xtal_clock_enable(ssusb);

	ret = clk_prepare_enable(ssusb->peri_usb0);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable peri-usb0\n", __func__);
		goto clken_usb0_err;
	}
	if (ssusb->p1_exist) {
		ret = clk_prepare_enable(ssusb->peri_usb1);
		if (ret) {
			mu3d_dbg(K_ERR, "%s failed to enable peri-usb1\n", __func__);
			goto clken_usb1_err;
		}
	}
	udelay(50);

	return 0;

clken_usb1_err:
	clk_disable_unprepare(ssusb->peri_usb0);
clken_usb0_err:
	ssusb_xtal_clock_disable(ssusb);
	return -EINVAL;
}

static int peri_clks_disable(struct ssusb_mtk *ssusb)
{
	if (ssusb->p1_exist)
		clk_disable_unprepare(ssusb->peri_usb1);

	clk_disable_unprepare(ssusb->peri_usb0);
	ssusb_xtal_clock_disable(ssusb);
	return 0;
}


/*
 * on 8173:
 * avdd1.0v which comes from aio18v is poweron/off by rg_ssusbldo_bg_ldo_en;
 * vio18v is always keep on, meanwhile avdd10 is out range of regulator driver,
 * so operate register directly here.
 */
static int check_vusb10_ready(struct ssusb_mtk *ssusb)
{
	int count = 200;
	u32 val;

	while (count > 0) {
		/* no mare than 50us normally */
		val = mu3d_readl(ssusb->sif_base, U3D_USB30_PHYA_REGC);
		if (val & E60802_AD_VUSB10_READY)
			break;
		udelay(1);
	}
	return count > 0 ? 0 : -ENODEV;
}

static int ssusb_vusb10_enable(struct ssusb_mtk *ssusb)
{
	void __iomem *sif_base = ssusb->sif_base;
	u32 efuse_cal_val;
	u32 val;

	val = mu3d_readl(sif_base, U3D_USB30_PHYA_REGD);
	if (0 == (val & E60802_RG_SSUSBLDO_BG_LDO_EN))
		mu3d_setmsk(sif_base, U3D_USB30_PHYA_REGD, E60802_RG_SSUSBLDO_BG_LDO_EN);

	/*
	 * read efuse cal vule 0x10206534[19:16]
	 * write to PHYA 0x11290b34[11:8]
	 * to make avdd10 be closest to 1.0V
	 * index is 31 for 0x10206534
	 */
	efuse_cal_val = get_devinfo_with_index(31);
	efuse_cal_val = (efuse_cal_val >> 16) & 0xf;
	val = mu3d_readl(sif_base, U3D_USB30_PHYA_REGD);
	val &= ~E60802_RG_SSUSBLDO_CAL;
	val |= efuse_cal_val << E60802_RG_SSUSBLDO_CAL_OFST;
	mu3d_writel(sif_base, U3D_USB30_PHYA_REGD, val);
	mu3d_dbg(K_DEBUG, "efuse : 0x%x(0x%x), regd : 0x%x(0x%x)\n",
		 efuse_cal_val, get_devinfo_with_index(31), val,
		 mu3d_readl(sif_base, U3D_USB30_PHYA_REGD));

	return check_vusb10_ready(ssusb);
}

static inline void ssusb_vusb10_disable(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->sif_base, U3D_USB30_PHYA_REGD, E60802_RG_SSUSBLDO_BG_LDO_EN);
}


static int ssusb_regulators_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	ret = regulator_enable(ssusb->vusb33);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable vusb33\n", __func__);
		return ret;
	}
	if (ssusb->usbnet33) {
		ret = regulator_enable(ssusb->usbnet33);
		if (ret) {
			mu3d_dbg(K_ERR, "%s failed to enable usbnet33\n", __func__);
			regulator_disable(ssusb->vusb33);
			return ret;
		}
	}
	return 0;
}

static void ssusb_regulators_disable(struct ssusb_mtk *ssusb)
{
	regulator_disable(ssusb->vusb33);
	if (ssusb->usbnet33)
		regulator_disable(ssusb->usbnet33);
}


static int ssusb_phy_init(struct ssusb_mtk *ssusb)
{
	struct u3phy_reg_base u3p_reg;
	int ret = -1;

	mu3d_dbg(K_DEBUG, "%s\n", __func__);
	u3p_reg.sif_base = ssusb->sif_base;
	/* u3p_reg.sif2_base = musb->sif2_base; */
	u3p_reg.phy_num = ssusb->p1_exist ? 2 : 1;

	ret = ssusb_regulators_enable(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable regulators\n", __func__);
		return ret;
	}

	ret = ssusb_clks_enable(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable clks\n", __func__);
		goto clks_err;
	}
	/* clock is on by default on mt8173, so turn it off if not use */
	if (!ssusb->p1_exist) {
		clk_prepare_enable(ssusb->peri_usb1);
		clk_disable_unprepare(ssusb->peri_usb1);
	}

	/*
	 * if usb2-port0 100U current extracts from U3(RG_USB20_HS_100U_U3_EN = 1),
	 * enable vusb10, otherwise close it
	 */
	ret = ssusb_vusb10_enable(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable vusb10\n", __func__);
		goto vusb10_err;
	}
	mu3d_dbg(K_INFO, "%s: vusb33, vusb10 and clks are enabled\n", __func__);

	/* initialize PHY related data structure, param is dummy if not project phy */
	if (!u3phy)
		ret = u3phy_init(&u3p_reg);

	u3phy->u3p_ops->init(u3phy);
	u3phy->u3p_ops->usb_phy_recover(u3phy, 1 /*musb->is_clk_on */);

	/* USB 2.0 slew rate calibration */
	u3phy->u3p_ops->u2_slew_rate_calibration(u3phy);
	ssusb->is_power_on = 1;

	return 0;

vusb10_err:
	ssusb_clks_disable(ssusb);

clks_err:
	ssusb_regulators_disable(ssusb);
	return -EINVAL;
}

static void ssusb_phy_exit(struct ssusb_mtk *ssusb)
{
	ssusb_vusb10_disable(ssusb);
	ssusb_clks_disable(ssusb);
	ssusb_regulators_disable(ssusb);
	if (u3phy)
		u3phy_exit(NULL);
}

static int low_power_enter(struct ssusb_mtk *ssusb)
{
	mutex_lock(&ssusb->power_mutex);
	if (!ssusb->is_power_on)
		goto out;

	u3phy->u3p_ops->usb_phy_savecurrent(u3phy, 1);

	/*
	 * if disable 3.3v, BC11 will also be disabled at the same time.
	 * when keep it on, port0's electrostatic discharge will increase
	 * from 0.0002mA to 0.0149mA in suspend mode.
	 */
	if (SSUSB_STR_DEEP != ssusb->str_mode)
		peri_clks_disable(ssusb);
	else {
		ssusb_vusb10_disable(ssusb);
		ssusb_clks_disable(ssusb);
		ssusb_regulators_disable(ssusb);
	}

	ssusb->is_power_on = 0;

out:
	mutex_unlock(&ssusb->power_mutex);
	return 0;
}

static int low_power_exit(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	mutex_lock(&ssusb->power_mutex);
	if (ssusb->is_power_on)
		goto out;

	if (SSUSB_STR_DEEP != ssusb->str_mode) {
		peri_clks_enable(ssusb);
		goto restore_phy;
	}

	ret = ssusb_regulators_enable(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable ldos\n", __func__);
		goto out;
	}

	ret = ssusb_clks_enable(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable clks\n", __func__);
		ssusb_regulators_disable(ssusb);
		goto out;
	}
	ssusb_vusb10_enable(ssusb);

restore_phy:
	u3phy->u3p_ops->usb_phy_recover(u3phy, 1);
	/* USB 2.0 slew rate calibration */
	u3phy->u3p_ops->u2_slew_rate_calibration(u3phy);
	ssusb->is_power_on = 1;

out:
	mutex_unlock(&ssusb->power_mutex);
	return ret;
}

int ssusb_power_save(struct ssusb_mtk *ssusb)
{
	if (!ssusb->is_power_saving_mode)
		return 0;

	low_power_enter(ssusb);
	mu3d_dbg(K_DEBUG, "%s\n", __func__);
	return 0;
}

int ssusb_power_restore(struct ssusb_mtk *ssusb)
{
	if (!ssusb->is_power_saving_mode)
		return 0;

	low_power_exit(ssusb);
	mu3d_dbg(K_DEBUG, "%s\n", __func__);
	return 0;
}

static void ssusb_mac_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & mu3d) */
	mu3d_setmsk(ssusb->sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	mu3d_clrmsk(ssusb->sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}

static int mtu3d_musb_reg_init(struct musb *musb)
{
#if 0
	struct u3phy_reg_base u3p_reg;
	int ret = -1;

	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	u3p_reg.sif_base = musb->sif_base;
	/* u3p_reg.sif2_base = musb->sif2_base; */

#ifndef CONFIG_MTK_FPGA
	/* initialize PHY related data structure */
	if (!u3phy)
		ret = u3phy_init(&u3p_reg);
#else
	ret = 1;
#endif

	if (ret) {
#ifndef CONFIG_MTK_FPGA
		u3phy->u3p_ops->init(u3phy);
		musb->is_clk_on = 1;

		u3phy->u3p_ops->usb_phy_recover(u3phy, musb->is_clk_on);

		/* USB 2.0 slew rate calibration */
		u3phy->u3p_ops->u2_slew_rate_calibration(u3phy);
#endif

		/* disable ip power down, disable U2/U3 ip power down */
		mu3d_hal_ssusb_en(musb);

		/* reset U3D all dev module. */
		mu3d_hal_rst_dev(musb);
	} else {
		mu3d_dbg(K_ERR, "%s: PHY initialization fail!\n", __func__);
		ret = -ENODEV;
	}

	return ret;
#endif
	musb->is_clk_on = 1;

	/* disable ip power down, disable U2/U3 ip power down */
	mu3d_hal_ssusb_en(musb);

	/* reset U3D all dev module. */
	mu3d_hal_rst_dev(musb);
	return 0;
}

void mtu3d_musb_enable(struct musb *musb)
{
	mu3d_dbg(K_INFO, "%s\n", __func__);

}

void mtu3d_musb_disable(struct musb *musb)
{
	mu3d_dbg(K_INFO, "%s\n", __func__);

#ifdef CONFIG_SSUSB_PROJECT_PHY
	/* Comment from CC Chou.
	 * When detecting HS or FS and setting RG_USB20_SW_PLLMODE=1, It is OK to enter LPM L1 with BESL=0.
	 * When disconnecting, set RG_USB20_SW_PLLMODE=0 back.
	 */
	/* use phy hook function directly. yun */
	mu3d_clrmsk(musb->sif_base, U3D_U2PHYDCR1, E60802_RG_USB20_SW_PLLMODE);
#endif

}

static int mtu3d_musb_init(struct musb *musb)
{
	int retval;

	mu3d_dbg(K_DEBUG, "%s\n", __func__);


	usb_phy_generic_register();
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(musb->xceiv))
		goto unregister;
	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	retval = mtu3d_musb_reg_init(musb);
	if (retval < 0)
		goto put_phy;
	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	if (is_peripheral_enabled(musb))
		musb->xceiv->set_power = mtu3d_set_power;
	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	musb->isr = generic_interrupt;
	return 0;

put_phy:
	usb_put_phy(musb->xceiv);
unregister:

	return -ENODEV;

}

static int mtu3d_set_power(struct usb_phy *x, unsigned mA)
{
	mu3d_dbg(K_DEBUG, "%s\n", __func__);
	return 0;
}

static int mtu3d_musb_exit(struct musb *musb)
{
#ifdef NEVER
	struct device *dev = musb->controller;
	struct musb_hdrc_platform_data *plat = dev->platform_data;
#endif				/* NEVER */

	usb_put_phy(musb->xceiv);

	/* u3phy_exit(); */

	return 0;
}


static const struct musb_platform_ops mtu3d_ops = {
	.init = mtu3d_musb_init,
	.exit = mtu3d_musb_exit,

	.enable = mtu3d_musb_enable,
	.disable = mtu3d_musb_disable
};

static u64 usb_dmamask = DMA_BIT_MASK(32);

#if 0
static int ssusb_gadget_init(struct ssusb_mtk *ssusb)
{
	struct musb_hdrc_platform_data *pdata = ssusb->pdata;
	struct platform_device *mu3d;
	int ret;

	mu3d_dbg(K_DEBUG, "%s() %d\n", __func__, __LINE__);

	mu3d = platform_device_alloc(MUSB_DRIVER_NAME, PLATFORM_DEVID_NONE);
	if (!mu3d) {
		dev_err(ssusb->dev, "failed to allocate musb device\n");
		ret = -ENOMEM;
		goto err0;
	}

	mu3d->dev.parent = ssusb->dev;
	mu3d->dev.dma_mask = ssusb->dev->dma_mask;
	dma_set_coherent_mask(&mu3d->dev, ssusb->dev->coherent_dma_mask);
	ssusb->mu3d = mu3d;

	pdata->platform_ops = &mtu3d_ops;
	mu3d_dbg(K_DEBUG, "%s() %d\n", __func__, __LINE__);


	/* FIXME:
	 * the ep cfg should be placed at mt_dev.c.
	 * However, each item of the array is 8 showing from ICE.
	 * But the actul size of each item is 6 by sizeof().
	 * Move the array here, the array algin issue is fixed.
	 * So put here temp until we know the reason.
	 */
	pdata->config->fifo_cfg = mtu3d_cfg;
	pdata->config->fifo_cfg_size = ARRAY_SIZE(mtu3d_cfg);

	ret = platform_device_add_resources(mu3d, ssusb->mu3d_rscs, SSUSB_MU3D_RSCS_NUM);
	if (ret) {
		dev_err(ssusb->dev, "failed to add resources\n");
		goto err1;
	}

	ret = platform_device_add_data(mu3d, pdata, sizeof(*pdata));
	if (ret) {
		dev_err(ssusb->dev, "failed to add platform_data\n");
		goto err1;
	}
	mu3d_dbg(K_DEBUG, "%s() %d\n", __func__, __LINE__);

	ret = platform_device_add(mu3d);
	if (ret) {
		dev_err(ssusb->dev, "failed to register musb device\n");
		goto err1;
	}
	mu3d_dbg(K_DEBUG, "%s() %d register mu3d device ok\n", __func__, __LINE__);
	return 0;

err1:
	platform_device_put(mu3d);

err0:
	return ret;

}

void ssusb_gadget_exit(struct ssusb_mtk *ssusb)
{
	platform_device_unregister(ssusb->mu3d);
}
#endif

#ifdef CONFIG_OF
static int get_otg_iddig_eint_num(struct device_node *dn, u32 *outval)
{
	struct device_node *iddig_dn;
	u32 irq_num;

	iddig_dn = of_get_child_by_name(dn, "otg-iddig");
	if (NULL == iddig_dn) {
		mu3d_dbg(K_ERR, "failed to get otg-iddig sub node\n");
		return -ENODEV;
	}
	irq_num = irq_of_parse_and_map(iddig_dn, 0);
	if (irq_num <= 0) {
		mu3d_dbg(K_ERR, "invalid eint number - %d\n", irq_num);
		return -EINVAL;
	}
	mu3d_dbg(K_WARNIN, "eint number - %d\n", irq_num);
	*outval = irq_num;
	return 0;
}

static struct musb_hdrc_platform_data *mu3d_parse_dts(struct device_node *dn)
{
	struct musb_hdrc_platform_data *pdata;
	struct musb_hdrc_config *config;
	enum of_gpio_flags flags;
	/* u32 tmp; */
	int ret = 0;
	/* struct device_node      *np = pdev->dev.of_node; */
	/* u32 temp_id = 0; */
	/* unsigned long usb_mac; */

	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	mu3d_dbg(K_WARNIN, "%s: use device tree\n", __func__);
	/* usb_irq_number1 = irq_of_parse_and_map(pdev->dev.of_node, 0); */
	/* usb_mac = (unsigned long)of_iomap(pdev->dev.of_node, 0); */
	/* usb_phy_base = (unsigned long)of_iomap(pdev->dev.of_node, 1); */
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (NULL == pdata)
		goto pd_err;
	pdata->p0_gpio_num = -1;	/* will update if use */
	pdata->p1_gpio_num = -1;
	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (NULL == config)
		goto cfg_err;

	ret = of_property_read_u32(dn, "num-eps", &config->num_eps);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get num_eps\n");
		goto prp_err;
	}

	ret = of_property_read_u32(dn, "str-mode", &pdata->str_mode);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get str-mode\n");
		goto prp_err;
	}

	ret = of_property_read_u32(dn, "drv-mode", &pdata->drv_mode);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get drv-mode\n");
		goto prp_err;
	} else if (SSUSB_MODE_DEVICE == pdata->drv_mode) {
		mu3d_dbg(K_INFO, "only as DEVICE mode\n");
		goto prp_ok;
	}

	ret = of_property_read_u32(dn, "port-num", &pdata->port_num);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get port-num\n");
		goto prp_err;
	}

	ret = of_property_read_u32(dn, "wakeup-src", &pdata->wakeup_src);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get wakeup-src\n");
		goto prp_err;
	}

	/* to get more configs for HOST or DRD mode */
	ret = of_property_read_u32(dn, "p0-vbus-mode", &pdata->p0_vbus_mode);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get p0-vbus-mode\n");
		goto prp_err;
	} else if (SSUSB_VBUS_GPIO == pdata->p0_vbus_mode) {
		pdata->p0_gpio_num = of_get_named_gpio_flags(dn, "p0-gpio", 0, &flags);
		if (pdata->p0_gpio_num < 0) {
			mu3d_dbg(K_ERR, "failed to get p0-gpio\n");
			goto prp_err;
		}
		pdata->p0_gpio_active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
	}

	if (2 == pdata->port_num) {	/* port-num is max(u2-ports, u3-ports) */
		ret = of_property_read_u32(dn, "p1-vbus-mode", &pdata->p1_vbus_mode);
		if (ret) {
			mu3d_dbg(K_ERR, "failed to get p1-vbus-mode\n");
			goto prp_err;
		} else if (SSUSB_VBUS_GPIO == pdata->p1_vbus_mode) {
			pdata->p1_gpio_num = of_get_named_gpio_flags(dn, "p1-gpio", 0, &flags);
			if (pdata->p1_gpio_num < 0) {
				mu3d_dbg(K_ERR, "failed to get p1-gpio-num\n");
				goto prp_err;
			}
			pdata->p1_gpio_active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
		}
	}

	if (SSUSB_MODE_HOST == pdata->drv_mode) {
		mu3d_dbg(K_INFO, "only as HOST mode\n");
		goto prp_ok;
	}

	/* to get more configs for DRD mode */
	ret = of_property_read_u32(dn, "is-u3-otg", &pdata->is_u3_otg);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get is_u3_otg\n");
		goto prp_err;
	}
	ret = of_property_read_u32(dn, "otg-mode", &pdata->otg_mode);
	if (ret) {
		mu3d_dbg(K_ERR, "failed to get otg-mode\n");
		goto prp_err;
	} else if (is_eint_used(pdata->otg_mode)) {
		ret = get_otg_iddig_eint_num(dn, &pdata->eint_num);
		if (ret) {
			mu3d_dbg(K_ERR, "failed to get iddig-eint-num\n");
			goto prp_err;
		}
	} else if (SSUSB_OTG_MANUAL == pdata->otg_mode) {
		ret = of_property_read_u32(dn, "init-as-host", &pdata->otg_init_as_host);
		if (ret) {
			mu3d_dbg(K_ERR, "failed to get init-as-host\n");
			goto prp_err;
		}
	}

prp_ok:

	pdata->config = config;
	mu3d_dbg(K_WARNIN, "drv_mode:%d, str_mode:%d, num_eps:%d, ports:%d\n",
		 pdata->drv_mode, pdata->str_mode, config->num_eps, pdata->port_num);
	mu3d_dbg(K_WARNIN, "otg-mode:%d, init_to_host:%d, u3_otg:%d, wakeup_src:%d\n",
		 pdata->otg_mode, pdata->otg_init_as_host, pdata->is_u3_otg, pdata->wakeup_src);
	mu3d_dbg(K_WARNIN, "eint:%d, p0_vbus:%d, p0_gpio:%d, p1_vbus:%d, p1_gpio:%d\n",
		 pdata->eint_num, pdata->p0_vbus_mode, pdata->p0_gpio_num,
		 pdata->p1_vbus_mode, pdata->p1_gpio_num);

	return pdata;

prp_err:
	kfree(config);
cfg_err:
	kfree(pdata);
pd_err:
	return NULL;
}

static void mu3d_pdata_free(struct musb_hdrc_platform_data *pdata)
{
	kfree(pdata->config);
	kfree(pdata);
}

static void __iomem *get_regs_base(struct platform_device *pdev, int index)
{
	struct resource *res;
	void __iomem *regs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (NULL == res) {
		mu3d_dbg(K_ERR, "error to get rsrc %d\n", index);
		return NULL;
	}

	regs = ioremap_nocache(res->start, resource_size(res));
	if (NULL == regs) {
		mu3d_dbg(K_ERR, "error mapping memory for %d\n", index);
		return NULL;
	}
	mu3d_dbg(K_INFO, "index:%d - iomap:0x%p, res:%#lx, len:%#lx\n", index, regs,
		 (unsigned long)res->start, (unsigned long)resource_size(res));
	return regs;
}

static void put_regs_map(struct ssusb_mtk *ssusb)
{
	if (ssusb->mac_base)
		iounmap(ssusb->mac_base);
	if (ssusb->sif_base)
		iounmap(ssusb->sif_base);
}

static int get_ssusb_clks(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	struct clk *tmp;
	tmp = clk_get(&pdev->dev, "top_usb30");
	if (IS_ERR(tmp)) {
		mu3d_dbg(K_ERR, "error to get top_usb30\n");
		goto pwr_err;
	}
	ssusb->scp_sys = tmp;
	pm_runtime_enable(&pdev->dev);

	tmp = clk_get(&pdev->dev, "peri_usb0");
	if (IS_ERR(tmp)) {
		mu3d_dbg(K_ERR, "error to get peri_usb0\n");
		goto clk_usb0_err;
	}
	ssusb->peri_usb0 = tmp;

	tmp = clk_get(&pdev->dev, "peri_usb1");
	if (IS_ERR(tmp)) {
		mu3d_dbg(K_ERR, "error to get peri_usb1\n");
		goto clk_usb1_err;
	}
	ssusb->peri_usb1 = tmp;
	mu3d_dbg(K_INFO, "mu3d get clks ok\n");

	return 0;

clk_usb1_err:
	clk_put(ssusb->peri_usb0);

clk_usb0_err:
	clk_put(ssusb->scp_sys);
pwr_err:
	return -EINVAL;
}

static void put_ssusb_clks(struct ssusb_mtk *ssusb)
{
	if (ssusb->peri_usb0)
		clk_put(ssusb->peri_usb0);
	if (ssusb->peri_usb1)
		clk_put(ssusb->peri_usb1);
	if (ssusb->scp_sys)
		clk_put(ssusb->scp_sys);
}

static int get_regulators(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	int ret = -EINVAL;

	ssusb->vusb33 = regulator_get(&pdev->dev, "reg-vusb33");
	if (IS_ERR_OR_NULL(ssusb->vusb33)) {
		mu3d_dbg(K_ERR, "fail to get vusb33\n");
		ret = PTR_ERR(ssusb->vusb33);
		goto err;
	}
	mu3d_dbg(K_INFO, "mu3d get vusb33 ok\n");
	/* vusb33's regulator is fixed at voltage-3.3v, so can't reset new value agian */
#if 0
	ret = regulator_set_voltage(ssusb->vusb33, 3300000, 3300000);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to set vusb33 voltage:%d\n", __func__, ret);
		regulator_put(ssusb->vusb33);
		ssusb->vusb33 = NULL;
		goto err;
	}
#endif
	/* only box maybe use it , so is not error if it don't exists */
#if 0
	ssusb->usbnet33 = regulator_get(&pdev->dev, "reg-usbnet33");
	if (!IS_ERR_OR_NULL(ssusb->usbnet33)) {
		ret = regulator_set_voltage(ssusb->usbnet33, 3300000, 3300000);
		if (ret) {
			mu3d_dbg(K_ERR, "%s failed to set usbnet35 to 3.3v:%d\n", __func__, ret);
			goto un33_err;
		}
		mu3d_dbg(K_WARNIN, "mu3d get usbnet33 ok\n");
	} else
#endif
		ssusb->usbnet33 = NULL;

	return 0;


err:
	return ret;
}

static void put_regulators(struct ssusb_mtk *ssusb)
{
	regulator_put(ssusb->usbnet33);
	regulator_put(ssusb->vusb33);
}
#endif

static void get_host_ports(struct ssusb_mtk *ssusb)
{
	u32 xhci_cap;

	if (ssusb->p1_exist) {
		xhci_cap = mu3d_readl(ssusb->sif_base, U3D_SSUSB_IP_XHCI_CAP);
		ssusb->u2_ports = SSUSB_U2_PORT_NUM(xhci_cap);
		ssusb->u3_ports = SSUSB_U3_PORT_NUM(xhci_cap);
	} else {
		/* if there is only one host port on mt8173, it only supports usb2.0 */
		ssusb->u2_ports = 1;
		ssusb->u3_ports = 0;
	}
	mu3d_dbg(K_INFO, "use u2_ports:%d, u3_ports:%d in fact\n",
		 ssusb->u2_ports, ssusb->u3_ports);
}

static int get_one_resource(struct platform_device *pdev,
			    struct resource *rsc, unsigned int type, unsigned int num, char *name)
{
	struct resource *res;

	res = platform_get_resource(pdev, type, num);
	if (!res) {
		dev_err(&pdev->dev, "missing %s type: 0x%x, index:%d\n", name, type, num);
		return -ENODEV;
	}
	memcpy(rsc, res, sizeof(*res));
	mu3d_dbg(K_DEBUG, "%s: rsc - start:0x%x, end: 0x%x, flags: 0x%lx, name: %s\n",
		 __func__, (u32) rsc->start, (u32) rsc->end, rsc->flags, rsc->name);
	return 0;
}

static int get_vbus_gpio(struct vbus_ctrl_info *vbus, const char *label)
{
	int retval;

	if (SSUSB_VBUS_GPIO != vbus->vbus_mode)
		return 0;

	if (!gpio_is_valid(vbus->vbus_gpio_num)) {
		mu3d_dbg(K_ERR, "%s gpio-num is invalid:%d\n", label, vbus->vbus_gpio_num);
		return -ENODEV;
	}

	retval = gpio_request(vbus->vbus_gpio_num, label);
	if (retval) {
		mu3d_dbg(K_ERR, "fail to requeset %s :%d\n", label, vbus->vbus_gpio_num);
		return -EBUSY;
	}
	/* inactive by default */
	gpio_direction_output(vbus->vbus_gpio_num, vbus->gpio_active_low);

	mu3d_dbg(K_DEBUG, "%s :%d is ok\n", label, vbus->vbus_gpio_num);
	return 0;
}

/* all power & clocks can be disabled for iddig wakeup mode */
static void usb_wakeup_iddig_en(struct ssusb_mtk *ssusb, int polarity)
{
	u32 tmp;

	if (polarity)
		mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IDDIG_P);
	else
		mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IDDIG_P);

	tmp = mu3d_readl(ssusb->perisys, PERI_WK_CTRL1);
	tmp &= ~(UWK_CTL1_IDDIG_C(0xf));
	tmp |= UWK_CTL1_IDDIG_C(0x8);
	mu3d_writel(ssusb->perisys, PERI_WK_CTRL1, tmp);

	mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IDDIG_E);
	mu3d_dbg(K_INFO, "%s() WK_CTRL1[P9,E10,C11:14]=%#x\n", __func__,
		 mu3d_readl(ssusb->perisys, PERI_WK_CTRL1));
}

static inline void usb_wakeup_iddig_dis(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IDDIG_E);
}

/* only clocks can be turn off for ip-sleep wakeup mode */
static void usb_wakeup_ip_sleep_en(struct ssusb_mtk *ssusb, int polarity)
{
	u32 tmp;

	if (polarity)
		mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IS_P);
	else
		mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IS_P);

	tmp = mu3d_readl(ssusb->perisys, PERI_WK_CTRL1);
	tmp &= ~(UWK_CTL1_IS_C(0xf));
	tmp |= UWK_CTL1_IS_C(0x8);
	mu3d_writel(ssusb->perisys, PERI_WK_CTRL1, tmp);

	mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IS_E);
	mu3d_dbg(K_INFO, "%s() WK_CTRL1[P6,E25,C26:29]=%#x\n", __func__,
		 mu3d_readl(ssusb->perisys, PERI_WK_CTRL1));
}

static inline void usb_wakeup_ip_sleep_dis(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_IS_E);
}


/*
* for line-state wakeup mode, phy's power should not power-down
*/
static void usb_wakeup_p0_line_state_en(struct ssusb_mtk *ssusb, int polarity, int edge)
{
	u32 tmp;

	if (polarity)
		mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_0P_LS_P);
	else
		mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_0P_LS_P);

	tmp = mu3d_readl(ssusb->perisys, PERI_WK_CTRL1);
	tmp &= ~(UWK_CTL1_0P_LS_C(0xf));
	tmp |= UWK_CTL1_0P_LS_C(0x8);
	mu3d_writel(ssusb->perisys, PERI_WK_CTRL1, tmp);

	mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_0P_LS_E);

	mu3d_dbg(K_INFO, "%s() WK_CTRL1[P7,E20,C21:24]=%#x\n", __func__,
		 mu3d_readl(ssusb->perisys, PERI_WK_CTRL1));
}

static inline void usb_wakeup_p0_line_state_dis(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL1, UWK_CTL1_0P_LS_E);
}

static void usb_wakeup_p1_line_state_en(struct ssusb_mtk *ssusb)
{
	u32 tmp;

	tmp = mu3d_readl(ssusb->perisys, PERI_WK_CTRL0);
	tmp &= ~(UWK_CTL1_1P_LS_C(0xf));
	tmp |= UWK_CTL1_1P_LS_C(0x8);
	mu3d_writel(ssusb->perisys, PERI_WK_CTRL0, tmp);

	mu3d_setmsk(ssusb->perisys, PERI_WK_CTRL0, UWK_CTL1_1P_LS_E);

	mu3d_dbg(K_INFO, "%s() WK_CTRL0[E0,C1:4]=%#x\n", __func__,
		 mu3d_readl(ssusb->perisys, PERI_WK_CTRL0));
}

static inline void usb_wakeup_p1_line_state_dis(struct ssusb_mtk *ssusb)
{
	mu3d_clrmsk(ssusb->perisys, PERI_WK_CTRL0, UWK_CTL1_1P_LS_E);
}

static void usb_wakeup_enable(struct ssusb_mtk *ssusb)
{
	if (ssusb->wakeup_src & SSUSB_WK_IDDIG)
		usb_wakeup_iddig_en(ssusb, 0);
	if (ssusb->wakeup_src & SSUSB_WK_LINESTATE_0P)
		usb_wakeup_p0_line_state_en(ssusb, 0, 0);
	if (ssusb->wakeup_src & SSUSB_WK_LINESTATE_1P)
		usb_wakeup_p1_line_state_en(ssusb);
	/* TO FIXUP: always wakeup system even no device plugged-in */
	if (ssusb->wakeup_src & SSUSB_WK_IP_SLEEP) {
		/* make sure mu3d already stopped */

		ssusb_otg_iddig_dis(ssusb);
		ssusb_host_disable(ssusb);
		usb_wakeup_ip_sleep_en(ssusb, 0);
	}
}

static void usb_wakeup_disable(struct ssusb_mtk *ssusb)
{
	if (ssusb->wakeup_src & SSUSB_WK_IDDIG)
		usb_wakeup_iddig_dis(ssusb);
	if (ssusb->wakeup_src & SSUSB_WK_LINESTATE_0P)
		usb_wakeup_p0_line_state_dis(ssusb);
	if (ssusb->wakeup_src & SSUSB_WK_LINESTATE_1P)
		usb_wakeup_p1_line_state_dis(ssusb);
	if (ssusb->wakeup_src & SSUSB_WK_IP_SLEEP) {
		usb_wakeup_ip_sleep_dis(ssusb);
		ssusb_host_enable(ssusb);
		ssusb_otg_iddig_en(ssusb);
	}

}

static void usb_sleep_wakesrc_set(struct ssusb_mtk *ssusb)
{
#if 0
	if (ssusb->wakeup_src & (SSUSB_WK_IDDIG | SSUSB_WK_IP_SLEEP))
		spm_set_sleep_wakesrc(WAKE_SRC_USB_PDN, 1, 0);

	if (ssusb->wakeup_src & (SSUSB_WK_LINESTATE_0P | SSUSB_WK_LINESTATE_1P))
		spm_set_sleep_wakesrc(WAKE_SRC_USB_CD, 1, 0);
#endif
}

static void usb_sleep_wakesrc_clear(struct ssusb_mtk *ssusb)
{
#if 0
	if (ssusb->wakeup_src & (SSUSB_WK_IDDIG | SSUSB_WK_IP_SLEEP))
		spm_set_sleep_wakesrc(WAKE_SRC_USB_PDN, 0, 0);

	if (ssusb->wakeup_src & (SSUSB_WK_LINESTATE_0P | SSUSB_WK_LINESTATE_1P))
		spm_set_sleep_wakesrc(WAKE_SRC_USB_CD, 0, 0);
#endif
}


static int is_usb_wakeup_event(void)
{
#if 0
	struct mt_wake_event *usb_we;

	usb_we = spm_get_wakeup_event();
	if (usb_we && !strcmp(usb_we->domain, "SPM")) {
		if ((usb_we->code == __ffs(WAKE_SRC_USB_CD))
		    || (usb_we->code == __ffs(WAKE_SRC_USB_PDN))) {
			mu3d_dbg(K_INFO, "%s WEV_USB: %s@%d\n", __func__,
				 usb_we->domain, usb_we->code);
			return 1;
		}
	}
#endif
	return 0;
}

static inline int need_virtual_input_event(struct ssusb_mtk *ssusb)
{
	/*
	 * for iddig wakeup, port0 switch to host and will hold wakeloack at the same
	 * time, so no need to send virtual power-key if only port0's iddig wakeup source
	 * is enabled.
	 */
	return !!(ssusb->wakeup_src & (~SSUSB_WK_IDDIG));
}

static int attach_input_dev(struct ssusb_mtk *ssusb)
{
	struct input_dev *input = NULL;
	int ret = 0;

	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	if (!need_virtual_input_event(ssusb)) {
		ssusb->vi_dev = NULL;
		goto ignore_input_dev;
	}

	input = input_allocate_device();
	if (!input) {
		ret = -ENOMEM;
		goto err_free_mem;
	}

	/* Indicate that we generate key events */
	set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_POWER, input->keybit);

	input->name = "ssusb";
	ret = input_register_device(input);
	if (ret)
		goto err_free_mem;

	ssusb->vi_dev = input;

	mu3d_dbg(K_INFO, "%s, attach input device success!\n", __func__);

ignore_input_dev:
	if (ssusb->wakeup_src)
		usb_sleep_wakesrc_set(ssusb);

	return ret;

err_free_mem:
	input_free_device(input);
	return ret;
}

static int detach_input_dev(struct ssusb_mtk *ssusb)
{
	if (ssusb->vi_dev) {
		input_unregister_device(ssusb->vi_dev);
		ssusb->vi_dev = NULL;
	}
	if (ssusb->wakeup_src)
		usb_sleep_wakesrc_clear(ssusb);

	return 0;
}

static int report_virtual_key(struct ssusb_mtk *ssusb)
{
	struct input_dev *input = ssusb->vi_dev;

	/*
	 * if only port0's iddig-wakeup is enabled, no need to send virtual power-key;
	 * but if port0's iddig-wakeup and port1's linestate-wakeup are all enabled, and when
	 * iddig wake up system, we can't distinguish the sources, so sends a dummy power-key
	 * event anyway.
	 */
	if (!need_virtual_input_event(ssusb))
		return 0;

	mu3d_dbg(K_INFO, "%s send virtual POWER-KEY event!\n", __func__);

	input_report_key(input, KEY_POWER, 1);
	input_sync(input);

	input_report_key(input, KEY_POWER, 0);
	input_sync(input);

	return 0;
}

static void remove_dummy_wakeup_src(struct ssusb_mtk *ssusb)
{
	int ws_old = ssusb->wakeup_src;

	/*
	 * SSUSB_WK_LINESTATE_0P is only necessary for port0 which works on
	 * for SSUSB_OTG_MANUAL mode
	 */
	if ((ssusb->wakeup_src & SSUSB_WK_LINESTATE_0P)
	    && (ssusb->otg_switch.otg_mode != SSUSB_OTG_MANUAL))
		ssusb->wakeup_src &= ~SSUSB_WK_LINESTATE_0P;

	/*
	 * for SSUSB_OTG_MANUAL mode, there is no id-pin, so SSUSB_WK_IDDIG is dummy
	 */
	if ((ssusb->wakeup_src & SSUSB_WK_IDDIG)
	    && !is_eint_used(ssusb->otg_switch.otg_mode))
		ssusb->wakeup_src &= ~SSUSB_WK_IDDIG;

	if (!ssusb->p1_exist && (ssusb->wakeup_src & SSUSB_WK_LINESTATE_1P))
		ssusb->wakeup_src &= ~SSUSB_WK_LINESTATE_1P;
	mu3d_dbg(K_INFO, "usb wakeup source(new:%d/old:%d)\n", ssusb->wakeup_src, ws_old);
}

static void __iomem *get_ref_node_regs_base(struct device *dev, char *phandle)
{
	struct device_node *dn = dev->of_node;
	struct device_node *pll_dn;

	pll_dn = of_parse_phandle(dn, phandle, 0);
	if (!pll_dn) {
		dev_err(dev, "failed to get %s phandle in %s node\n", phandle,
			dev->of_node->full_name);
		return NULL;
	}

	return of_iomap(pll_dn, 0);
}


static int get_ssusb_rscs(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	struct musb_hdrc_platform_data *pdata = pdev->dev.platform_data;
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;
	struct vbus_ctrl_info *p0_vbus;
	struct vbus_ctrl_info *p1_vbus;
	int ret;


	ret = get_one_resource(pdev, &ssusb->xhci_rscs[0], IORESOURCE_MEM, 0, "xhci-mem");
	if (ret)
		goto err;

	ret = get_one_resource(pdev, &ssusb->mu3d_rscs[0], IORESOURCE_MEM, 1, "mu3d-mem");
	if (ret)
		goto err;
	ret = get_one_resource(pdev, &ssusb->mu3d_rscs[1], IORESOURCE_MEM, 2, "sif-mem");
	if (ret)
		goto err;
	/* ret = get_one_resource(pdev, &ssusb->mu3d_rscs[2], IORESOURCE_MEM, 3, "sif2-mem"); */
	/* if (ret) { */
	/* goto err; */
	/* } */

	ret = get_one_resource(pdev, &ssusb->xhci_rscs[1], IORESOURCE_IRQ, 0, "xhci-irq");
	if (ret)
		goto err;
	ret = get_one_resource(pdev, &ssusb->mu3d_rscs[2], IORESOURCE_IRQ, 1, "mu3d-irq");
	if (ret)
		goto err;
	/* ssusb->mu3d_irq = ssusb->mu3d_rscs[2].start; */
	ssusb->mu3d_irq = platform_get_irq(pdev, 1);
	if (ssusb->mu3d_irq <= 0) {
		mu3d_dbg(K_ERR, "fail to get irq number\n");
		ret = -ENODEV;
		goto err;
	}
	mu3d_dbg(K_INFO, "mu3d irq : %d\n", ssusb->mu3d_irq);
	ret = get_regulators(pdev, ssusb);
	if (ret < 0) {
		mu3d_dbg(K_ERR, "fail to get regulators\n");
		goto err;
	}

	/* cant put it in dts, resources conflict, so use it directly */
	/* ssusb->uap_pll_con = ioremap_nocache(UAP_PLL_CONX_BASE, UAP_PLL_CONX_LEN); */
	ssusb->uap_pll_con = get_ref_node_regs_base(&pdev->dev, "usb-xtal-clk");
	if (!ssusb->uap_pll_con) {
		mu3d_dbg(K_ERR, "mu3d fail to map pll regs\n");
		goto pll_err;
	}
	mu3d_dbg(K_INFO, "mu3d uap_pll_con: %p\n", ssusb->uap_pll_con);

	/* ssusb->perisys = ioremap_nocache(PERIFSYS_BASE, PERIFSYS_LEN); */
	ssusb->perisys = get_ref_node_regs_base(&pdev->dev, "usb-wakeup-ctrl");
	if (!ssusb->perisys) {
		mu3d_dbg(K_ERR, "mu3d fail to map perisys regs\n");
		goto peri_err;
	}

	mu3d_dbg(K_INFO, "mu3d perisys: %p\n", ssusb->perisys);

	ret = get_ssusb_clks(pdev, ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "mu3d fail to get clks\n");
		goto clks_err;
	}

	if ((SSUSB_MODE_DEVICE == pdata->drv_mode)
	    || (SSUSB_OTG_IDPIN == pdata->otg_mode)
	    || ((SSUSB_OTG_MANUAL == pdata->otg_mode) && !pdata->otg_init_as_host)) {
		/*
		 * when device only mode ,such as under FPGA env. system can't detect
		 * usb-cable plug, so need start mu3d directly. another case is pseudol-OTG
		 * (SSUSB_OTG_MANUAL & SSUSB_OTG_IDPIN).
		 */
		ssusb->start_mu3d = 1;
	}

	ssusb->drv_mode = pdata->drv_mode;
	ssusb->str_mode = pdata->str_mode;
	ssusb->wakeup_src = pdata->wakeup_src;
	otg_switch->otg_mode = pdata->otg_mode;
	otg_switch->is_init_as_host = pdata->otg_init_as_host;
	otg_switch->port0_u2 = 1;
	otg_switch->port0_u3 = !!pdata->is_u3_otg;
	otg_switch->iddig_eint_num = pdata->eint_num;
	p0_vbus = &otg_switch->p0_vbus;
	p0_vbus->vbus_mode = pdata->p0_vbus_mode;
	p0_vbus->vbus_gpio_num = pdata->p0_gpio_num;
	p0_vbus->gpio_active_low = pdata->p0_gpio_active_low;
	ret = get_vbus_gpio(p0_vbus, "p0_vbus_gpio");
	if (ret)
		goto p0_vbus_err;

	if (pdata->port_num > 1) {
		ssusb->p1_exist = 1;
		p1_vbus = &ssusb->p1_vbus;
		p1_vbus->vbus_mode = pdata->p1_vbus_mode;
		p1_vbus->vbus_gpio_num = pdata->p1_gpio_num;
		p1_vbus->gpio_active_low = pdata->p1_gpio_active_low;
		ret = get_vbus_gpio(p1_vbus, "p1_vbus_gpio");
		if (ret)
			goto p1_vbus_err;
	}
	remove_dummy_wakeup_src(ssusb);
	ssusb->is_power_saving_mode = !!(SSUSB_STR_NONE == ssusb->str_mode);
	mu3d_dbg(K_INFO, "is_power_saving_mode: %d\n", ssusb->is_power_saving_mode);

	return 0;

p1_vbus_err:
	if (SSUSB_VBUS_GPIO == p0_vbus->vbus_mode)
		gpio_free(p0_vbus->vbus_gpio_num);
p0_vbus_err:
	put_ssusb_clks(ssusb);
clks_err:
	iounmap(ssusb->perisys);
peri_err:
	iounmap(ssusb->uap_pll_con);
pll_err:
	put_regulators(ssusb);
err:
	return ret;
}

static int put_ssusb_rscs(struct ssusb_mtk *ssusb)
{
	struct vbus_ctrl_info *vbus;

	put_ssusb_clks(ssusb);
	put_regulators(ssusb);
	if (ssusb->uap_pll_con)
		iounmap(ssusb->uap_pll_con);
	if (ssusb->perisys)
		iounmap(ssusb->perisys);

	vbus = &ssusb->otg_switch.p0_vbus;
	if (SSUSB_VBUS_GPIO == vbus->vbus_mode)
		gpio_free(vbus->vbus_gpio_num);
	vbus = &ssusb->p1_vbus;
	if (ssusb->p1_exist && SSUSB_VBUS_GPIO == vbus->vbus_mode)
		gpio_free(vbus->vbus_gpio_num);

	return 0;
}


int __weak mt_get_chip_sw_ver(void)
{
	return 1;
}

static void ssusb_eco_fixup(struct ssusb_mtk *ssusb)
{
	ssusb->ic_version = mt_get_chip_sw_ver();
	mu3d_dbg(K_INFO, "%s ic_version :%x\n", __func__, ssusb->ic_version);

	if (ssusb->ic_version != CHIP_SW_VER_01) {
		/*
		 * sparse[0] = 1'b1: reg_fix_host_iso_tog_err, default not fixed up
		 * sparse[1] = 1'b1: reg_fix_dev_setupend_err, default not fixed up
		 */
		mu3d_setmsk(ssusb->sif_base, U3D_SSUSB_IP_SPARE0, 0x3);
	}
}

static int mtu3d_probe(struct platform_device *pdev)
{
	struct musb_hdrc_platform_data *pdata = pdev->dev.platform_data;
	struct ssusb_mtk *ssusb;
	int ret = -ENOMEM;

	mu3d_dbg(K_DEBUG, "%s() %d\n", __func__, __LINE__);


#ifdef CONFIG_OF
	pdata = mu3d_parse_dts(pdev->dev.of_node);
	if (NULL == pdata) {
		mu3d_dbg(K_ERR, "fail to parse dts of mu3d\n");
		goto of_err;
	}

	pdev->dev.dma_mask = &usb_dmamask;
	pdev->dev.coherent_dma_mask = usb_dmamask;

#endif
	/* all elements are set to ZERO as default value */
	ssusb = kzalloc(sizeof(*ssusb), GFP_KERNEL);
	if (!ssusb)
		goto free_pdata;

	ssusb->dev = &pdev->dev;
	ssusb->pdata = pdata;
	mutex_init(&ssusb->power_mutex);
	/* move below to dts parse function later. */
	pdata->platform_ops = &mtu3d_ops;
	pdata->config->fifo_cfg = mtu3d_cfg;
	pdata->config->fifo_cfg_size = ARRAY_SIZE(mtu3d_cfg);
	platform_set_drvdata(pdev, ssusb);	/* memcpy, should modify pdata later */

	mu3d_dbg(K_DEBUG, "%s() %d\n", __func__, __LINE__);

	ssusb->mac_base = get_regs_base(pdev, 1);
	if (NULL == ssusb->mac_base) {
		mu3d_dbg(K_ERR, "fail to get mu3d mac regs\n");
		goto free_ssusb;
	}

	ssusb->sif_base = get_regs_base(pdev, 2);
	if (NULL == ssusb->sif_base) {
		mu3d_dbg(K_ERR, "fail to get phy sif regs\n");
		goto put_regmap;
	}
	platform_device_add_data(pdev, pdata, sizeof(*pdata));

	ret = get_ssusb_rscs(pdev, ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "fail to get mu3d resources\n");
		goto put_regmap;
	}

	ret = attach_input_dev(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "fail to reg input device\n");
		goto put_rscs;
	}

	/* ip-reset and phy-init */
	ret = ssusb_phy_init(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "fail to init phy\n");
		goto put_input_dev;
	}
	ssusb_mac_sw_reset(ssusb);
	ssusb_otg_iddig_en(ssusb);
	ssusb_eco_fixup(ssusb);
	get_host_ports(ssusb);

	/* if (IS_ENABLED(CONFIG_USB_SSUSB_HOST)) */
	/* mode = SSUSB_MODE_HOST; */
	/* else if (IS_ENABLED(CONFIG_USB_SSUSB_GADGET)) */
	/* mode = SSUSB_MODE_DEVICE; */
	/* else */
	/* mode = SSUSB_MODE_DRD; */

	switch (ssusb->drv_mode) {
	case SSUSB_MODE_DEVICE:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize gadget\n");
			goto exit_phy;
		}
		break;
	case SSUSB_MODE_HOST:
		mtk_xhci_ip_init(ssusb);

		ret = ssusb_host_init(ssusb);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize host\n");
			goto exit_phy;
		}
		break;
	case SSUSB_MODE_DRD:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize gadget\n");
			goto exit_phy;
		}

		/* when it's power saving mode, use iddig to add/remove host platform_device */
		if (!ssusb->is_power_saving_mode) {
			mtk_xhci_ip_init(ssusb);
			ret = ssusb_host_init(ssusb);
			if (ret) {
				dev_err(&pdev->dev, "failed to initialize host\n");
				goto gadget_exit;
			}
		}

		mtk_otg_switch_init(ssusb);
		break;
	default:
		dev_err(&pdev->dev, "Unsupported mode of operation %d\n", ssusb->drv_mode);
		goto exit_phy;
	}

	mu3d_dbg(K_WARNIN, "%s() done!\n", __func__);

	return 0;

gadget_exit:
	ssusb_gadget_exit(ssusb);

exit_phy:
	ssusb_phy_exit(ssusb);

put_input_dev:
	detach_input_dev(ssusb);

put_rscs:
	put_ssusb_rscs(ssusb);

put_regmap:
	put_regs_map(ssusb);

free_ssusb:
	kfree(ssusb);

free_pdata:
#ifdef CONFIG_OF
	mu3d_pdata_free(pdata);
of_err:
#endif
	return ret;
}

static int mtu3d_remove(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

#ifdef CONFIG_OF
	mu3d_pdata_free(pdev->dev.platform_data);
#endif
	detach_input_dev(ssusb);

	if (ssusb->mu3di)
		ssusb_gadget_exit(ssusb);
	if (ssusb->xhci)
		platform_device_unregister(ssusb->xhci);
	mtk_otg_switch_exit(ssusb);
	ssusb_phy_exit(ssusb);
	put_ssusb_rscs(ssusb);
	put_regs_map(ssusb);
	kfree(ssusb);

	return 0;
}
extern int ssusb_set_vbus(struct vbus_ctrl_info *vbus, int is_on);
static void mtu3d_shutdown(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;
	struct vbus_ctrl_info *vbus = &otg_switch->p0_vbus;
	mu3d_dbg(K_ERR, "%s in \n", __func__);

	if (mtk_is_host_mode()) {
		ssusb_set_vbus(vbus, 0);
	}

       mu3d_dbg(K_ERR, "%s out \n", __func__);

}

#ifdef CONFIG_PM
static int mtu3d_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);
	struct musb *musb = ssusb->mu3di;



	/*
	 * J:I think it does _NOT_ have to do anything. Because when the phone/tablet
	 * with USB cable,the system does _NOT_ enter suspend mode. At the normal case,
	 * the USB driver calls PHY savecurrent, when USB cable is plugged out.
	 */
	if (ssusb->is_power_saving_mode)
		return 0;

	if ((SSUSB_MODE_DEVICE != ssusb->drv_mode) && SSUSB_STR_DEEP == ssusb->str_mode) {
		mtk_xhci_ip_exit(ssusb);
		mu3d_dbg(K_INFO, "%s deep suspend enter\n", __func__);
	}

	if (musb->start_mu3d && !mtk_is_host_mode())
		musb_stop(musb);

	low_power_enter(ssusb);
	if (ssusb->wakeup_src)
		usb_wakeup_enable(ssusb);

	mu3d_dbg(K_INFO, "%s\n", __func__);
	return 0;
}


static int mtu3d_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);
	struct musb *musb = ssusb->mu3di;
	int ret;

	if (ssusb->wakeup_src)
		usb_wakeup_disable(ssusb);

	if (ssusb->is_power_saving_mode)
		return 0;

	ret = low_power_exit(ssusb);
	if (ret) {
		mu3d_dbg(K_ERR, "%s failed to enable clks & ldo\n", __func__);
		return ret;
	}
	if ((SSUSB_MODE_DEVICE != ssusb->drv_mode) && SSUSB_STR_DEEP == ssusb->str_mode) {
		mtk_xhci_ip_init(ssusb);
		mu3d_dbg(K_INFO, "%s deep suspend exit\n", __func__);
	}

	if (musb->start_mu3d && !mtk_is_host_mode())
		musb_start(musb);

	if (is_usb_wakeup_event())
		report_virtual_key(ssusb);

	mu3d_dbg(K_INFO, "%s\n", __func__);

	return 0;
}

static const struct dev_pm_ops mtu3d_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtu3d_suspend, mtu3d_resume)
};

#define DEV_PM_OPS	(&mtu3d_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
/* static u64 usb_dmamask = DMA_BIT_MASK(32); */

static const struct of_device_id ssusb_of_match[] = {
	{.compatible = "mediatek,mt8173-ssusb",},
	{},
};

MODULE_DEVICE_TABLE(of, mu3d_of_match);

#endif


static struct platform_driver mtu3d_driver = {
	.probe = mtu3d_probe,
	.remove = mtu3d_remove,
	.shutdown = mtu3d_shutdown,
	.driver = {
		   .name = "musb-mtu3d",
		   .pm = DEV_PM_OPS,

#ifdef CONFIG_OF
		   .of_match_table = ssusb_of_match,
#endif
		   },
};

MODULE_DESCRIPTION("mtu3d MUSB Glue Layer");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mtu3d_driver);

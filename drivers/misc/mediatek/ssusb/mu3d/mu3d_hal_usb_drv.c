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

/* #include "mu3d_hal_osal.h" */
/* #define _MTK_USB_DRV_EXT_ */
#include "mu3d_hal_usb_drv.h"
/* #undef _MTK_USB_DRV_EXT_ */
#include "mu3d_hal_hw.h"
#include "mu3d_hal_qmu_drv.h"
/* #include "./new/mu3d_hal_comm.h" */
/* #include <linux/mu3phy/mtk-phy.h> */
#include "ssusb_io.h"


static struct usb_ep_setting g_u3d_ep_setting[2 * MAX_EP_NUM + 1];
static u32 g_TxFIFOadd;
static u32 g_RxFIFOadd;

int wait_for_value(void __iomem *base, int addr, int msk, int value, int ms_intvl, int count)
{
	u32 i;

	for (i = 0; i < count; i++) {
		if ((mu3d_readl(base, addr) & msk) == value)
			return 0;
		mdelay(ms_intvl);
	}

	return -EBUSY;
}


/**
 * mu3d_hal_check_clk_sts - check sys125,u3 mac,u2 mac clock status
 *
 */
int mu3d_hal_check_clk_sts(struct musb *musb)
{
	int ret;

	mu3d_dbg(K_ERR, "%s\n", __func__);

	ret = wait_for_value(musb->sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_SYS125_RST_B_STS,
			     SSUSB_SYS125_RST_B_STS, 1, 10);
	if (ret)
		mu3d_dbg(K_ERR, "SSUSB_SYS125_RST_B_STS NG\n");

	/* do not check when SSUSB_U2_PORT_PDN = 1, because U2 port stays in reset state */
	if (!(mu3d_readl(musb->sif_base, U3D_SSUSB_U2_CTRL_0P) & SSUSB_U2_PORT_PDN)) {
		ret =
		    wait_for_value(musb->sif_base, U3D_SSUSB_IP_PW_STS2, SSUSB_U2_MAC_SYS_RST_B_STS,
				   SSUSB_U2_MAC_SYS_RST_B_STS, 1, 10);
		if (ret)
			mu3d_dbg(K_ERR, "SSUSB_U2_MAC_SYS_RST_B_STS NG\n");
	}
#ifdef SUPPORT_U3
	/* do not check when SSUSB_U3_PORT_PDN = 1, because U3 port stays in reset state */
	if (!(mu3d_readl(musb->sif_base, U3D_SSUSB_U3_CTRL_0P) & SSUSB_U3_PORT_PDN)) {
		ret = wait_for_value(musb->sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_U3_MAC_RST_B_STS,
				     SSUSB_U3_MAC_RST_B_STS, 1, 10);
		if (ret)
			mu3d_dbg(K_ERR, "SSUSB_U3_MAC_RST_B_STS NG\n");
	}
#endif

	mu3d_dbg(K_CRIT, "check clk pass!!\n");
	return 0;
}

/**
 * mu3d_hal_ssusb_en - disable ssusb power down & enable u2/u3 ports
 *
 */
void mu3d_hal_ssusb_en(struct musb *musb)
{
	void __iomem *sif_base = musb->sif_base;

	mu3d_dbg(K_INFO, "%s\n", __func__);

	mu3d_clrmsk(sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	mu3d_clrmsk(sif_base, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
#ifdef SUPPORT_U3
	mu3d_clrmsk(sif_base, U3D_SSUSB_U3_CTRL_0P,
		    (SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_HOST_SEL));
#endif
	mu3d_clrmsk(sif_base, U3D_SSUSB_U2_CTRL_0P,
		    (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_HOST_SEL));
	mu3d_dbg(K_INFO, "%s 0x:%x = 0x%x\n", __func__, U3D_SSUSB_U2_CTRL_0P,
		 mu3d_readl(sif_base, U3D_SSUSB_U2_CTRL_0P));

	mu3d_setmsk(sif_base, U3D_SSUSB_REF_CK_CTRL,
		    (SSUSB_REF_MAC_CK_GATE_EN | SSUSB_REF_PHY_CK_GATE_EN | SSUSB_REF_CK_GATE_EN |
		     SSUSB_REF_MAC3_CK_GATE_EN));

	/* check U3D sys125,u3 mac,u2 mac clock status. */
	mu3d_hal_check_clk_sts(musb);
}

void mu3d_hal_ssusb_dis(struct musb *musb)
{
	void __iomem *sif_base = musb->sif_base;

	mu3d_dbg(K_INFO, "%s\n", __func__);


#ifdef SUPPORT_U3
	mu3d_setmsk(sif_base, U3D_SSUSB_U3_CTRL_0P, (SSUSB_U3_PORT_DIS | SSUSB_U3_PORT_PDN));
#endif
	mu3d_setmsk(sif_base, U3D_SSUSB_U2_CTRL_0P, (SSUSB_U2_PORT_DIS | SSUSB_U2_PORT_PDN));
	mu3d_setmsk(sif_base, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);

	mu3d_dbg(K_INFO, "%s 0x:%x = 0x%x\n", __func__, U3D_SSUSB_U2_CTRL_0P,
		 mu3d_readl(sif_base, U3D_SSUSB_U2_CTRL_0P));
}

/**
 * mu3d_hal_rst_dev - reset all device modules
 *
 */
void mu3d_hal_rst_dev(struct musb *musb)
{
	void __iomem *sif_base = musb->sif_base;
	int ret;

	mu3d_dbg(K_ERR, "%s\n", __func__);

	mu3d_writel(sif_base, U3D_SSUSB_DEV_RST_CTRL, SSUSB_DEV_SW_RST);
	mu3d_writel(sif_base, U3D_SSUSB_DEV_RST_CTRL, 0);

	mu3d_hal_check_clk_sts(musb);

	ret = wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_QMU_RST_B_STS,
			     SSUSB_DEV_QMU_RST_B_STS, 1, 10);
	if (ret)
		mu3d_dbg(K_ERR, "SSUSB_DEV_QMU_RST_B_STS NG\n");

	ret = wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_BMU_RST_B_STS,
			     SSUSB_DEV_BMU_RST_B_STS, 1, 10);
	if (ret)
		mu3d_dbg(K_ERR, "SSUSB_DEV_BMU_RST_B_STS NG\n");

	ret = wait_for_value(sif_base, U3D_SSUSB_IP_PW_STS1, SSUSB_DEV_RST_B_STS,
			     SSUSB_DEV_RST_B_STS, 1, 10);
	if (ret)
		mu3d_dbg(K_ERR, "SSUSB_DEV_RST_B_STS NG\n");
}


#ifdef CONFIG_MTK_FPGA
/**
 * mu3d_hal_link_up - u3d link up
 *
 */
int mu3d_hal_link_up(struct musb *musb, int latch_val)
{
	mu3d_hal_ssusb_en(musb);
	mu3d_hal_rst_dev(musb);
	mdelay(50);
	mu3d_writel(musb->mac_base, U3D_USB3_CONFIG, USB3_EN);
	mu3d_writel(musb->mac_base, U3D_PIPE_LATCH_SELECT, latch_val);	/* set tx/rx latch sel */

	return 0;
}
#endif

/**
 * mu3d_hal_initr_dis - disable all interrupts
 *
 */
void mu3d_hal_initr_dis(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;
	/* Disable Level 1 interrupts */
	mu3d_writel(mbase, U3D_LV1IECR, 0xFFFFFFFF);

	/* Disable Endpoint interrupts */
	mu3d_writel(mbase, U3D_EPIECR, 0xFFFFFFFF);

	/* Disable DMA interrupts */
	mu3d_writel(mbase, U3D_DMAIECR, 0xFFFFFFFF);

	/* Disable U2 common USB interrupts */
	mu3d_writel(mbase, U3D_COMMON_USB_INTR_ENABLE, 0x00);
}

void mu3d_hal_clear_intr(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;

	mu3d_dbg(K_ERR, "%s\n", __func__);

	/* Clear EP0 and Tx/Rx EPn interrupts status */
	mu3d_writel(mbase, U3D_EPISR, 0xFFFFFFFF);

	/* Clear EP0 and Tx/Rx EPn DMA interrupts status */
	mu3d_writel(mbase, U3D_DMAISR, 0xFFFFFFFF);

	/* Clear speed change event */
	mu3d_writel(mbase, U3D_DEV_LINK_INTR, 0xFFFFFFFF);

	/* Clear U2 USB common interrupt status */
	mu3d_writel(mbase, U3D_COMMON_USB_INTR, 0xFFFFFFFF);

	/* Clear U3 LTSSM interrupt status */
	mu3d_writel(mbase, U3D_LTSSM_INTR, 0xFFFFFFFF);
}

/**
 * mu3d_hal_system_intr_en - enable system global interrupt
 *
 */
void mu3d_hal_system_intr_en(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;
	u32 int_en;
#ifdef SUPPORT_U3
	u32 ltssm_int_en;
#endif
	mu3d_dbg(K_ERR, "%s\n", __func__);

	/* Disable All endpoint interrupt */
	mu3d_writel(mbase, U3D_EPIECR, mu3d_readl(mbase, U3D_EPIER));

	/* Disable All DMA interrput */
	mu3d_writel(mbase, U3D_DMAIECR, mu3d_readl(mbase, U3D_DMAIER));

	/* Disable U2 common USB interrupts */
	mu3d_writel(mbase, U3D_COMMON_USB_INTR_ENABLE, 0x00);

	/* Clear U2 common USB interrupts status */
	mu3d_writel(mbase, U3D_COMMON_USB_INTR, mu3d_readl(mbase, U3D_COMMON_USB_INTR));

	/* Enable U2 common USB interrupt */
	int_en = SUSPEND_INTR_EN | RESUME_INTR_EN | RESET_INTR_EN | CONN_INTR_EN | DISCONN_INTR_EN
	    | VBUSERR_INTR_EN | LPM_INTR_EN | LPM_RESUME_INTR_EN;
	mu3d_writel(mbase, U3D_COMMON_USB_INTR_ENABLE, int_en);

#ifdef SUPPORT_U3
	/* Disable U3 LTSSM interrupts */
	mu3d_writel(mbase, U3D_LTSSM_INTR_ENABLE, 0x00);
	mu3d_dbg(K_ERR, "U3D_LTSSM_INTR: %x\n", mu3d_readl(mbase, U3D_LTSSM_INTR));

	/* Clear U3 LTSSM interrupts */
	mu3d_writel(mbase, U3D_LTSSM_INTR, mu3d_readl(mbase, U3D_LTSSM_INTR));

	/* Enable U3 LTSSM interrupts */
	ltssm_int_en =
	    SS_INACTIVE_INTR_EN | SS_DISABLE_INTR_EN | COMPLIANCE_INTR_EN | LOOPBACK_INTR_EN |
	    HOT_RST_INTR_EN | WARM_RST_INTR_EN | RECOVERY_INTR_EN | ENTER_U0_INTR_EN |
	    ENTER_U1_INTR_EN | ENTER_U2_INTR_EN | ENTER_U3_INTR_EN | EXIT_U1_INTR_EN |
	    EXIT_U2_INTR_EN | EXIT_U3_INTR_EN | RXDET_SUCCESS_INTR_EN | VBUS_RISE_INTR_EN |
	    VBUS_FALL_INTR_EN | U3_LFPS_TMOUT_INTR_EN | U3_RESUME_INTR_EN;
	mu3d_writel(mbase, U3D_LTSSM_INTR_ENABLE, ltssm_int_en);
#endif

	if (need_vbus_chg_int(musb)) {
		mu3d_writel(musb->sif_base, U3D_SSUSB_OTG_INT_EN, 0x0);
		mu3d_dbg(K_ERR, "U3D_SSUSB_OTG_STS: %x\n",
			 mu3d_readl(musb->sif_base, U3D_SSUSB_OTG_STS));
		mu3d_writel(musb->sif_base, U3D_SSUSB_OTG_STS_CLR,
			    mu3d_readl(musb->sif_base, U3D_SSUSB_OTG_STS));
		mu3d_writel(musb->sif_base, U3D_SSUSB_OTG_INT_EN, SSUSB_VBUS_CHG_INT_B_EN);
	}
#ifdef USE_SSUSB_QMU
	/* Enable QMU interrupt. */
	mu3d_writel(mbase, U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		    RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
#endif

	/* Enable EP0 DMA interrupt */
	mu3d_writel(mbase, U3D_DMAIESR, EP0DMAIESR);

	/* Enable speed change interrupt */
	mu3d_writel(mbase, U3D_DEV_LINK_INTR_ENABLE, SSUSB_DEV_SPEED_CHG_INTR_EN);
}


/**
 * mu3d_hal_u2dev_connect - u2 device softconnect
 *
 */
void mu3d_hal_u2dev_connect(struct musb *musb)
{
	mu3d_setmsk(musb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);
	mu3d_setmsk(musb->mac_base, U3D_POWER_MANAGEMENT, SUSPENDM_ENABLE);
	mu3d_dbg(K_INFO, "SOFTCONN = 1\n");
}

void mu3d_hal_u2dev_disconn(struct musb *musb)
{
	mu3d_clrmsk(musb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);
	mu3d_clrmsk(musb->mac_base, U3D_POWER_MANAGEMENT, SUSPENDM_ENABLE);
	mu3d_dbg(K_INFO, "SOFTCONN = 0\n");
}

/**
 * mu3d_hal_u3dev_en - enable U3D SS dev
 *
 */
void mu3d_hal_u3dev_en(struct musb *musb)
{
	/*
	 * Enable super speed function.
	 */
	mu3d_writel(musb->mac_base, U3D_USB3_CONFIG, USB3_EN);
	mu3d_dbg(K_INFO, "USB3_EN = 1\n");
}

/**
 * mu3d_hal_u3dev_dis - disable U3D SS dev
 *
 */
void mu3d_hal_u3dev_dis(struct musb *musb)
{
	/*
	 * If usb3_en =0 => LTSSM will go to SS.Disable state.
	 */
	mu3d_writel(musb->mac_base, U3D_USB3_CONFIG, 0);
	mu3d_dbg(K_INFO, "USB3_EN = 0\n");
}

/**
 * mu3d_hal_pdn_cg_en - enable U2/U3 pdn & cg
 *@args -
 */
/* void mu3d_hal_pdn_cg_en(void) */
/* { */
/* } */

/* void mu3d_hal_pdn_ip_port(u8 on, u8 touch_dis, u8 u3, u8 u2) */
/* { */
/* } */

/**
 * mu3d_hal_write_fifo - pio write one packet
 *@args - arg1: ep number, arg2: data length,  arg3: data buffer, arg4: max packet size
 */
int mu3d_hal_write_fifo(struct musb_hw_ep *hw_ep, int ep_num, int length, u8 *buf, int maxp)
{
	struct musb *musb = hw_ep->musb;
	void __iomem *fifo = hw_ep->fifo;
	void __iomem *mbase = musb->mac_base;
	u32 residue;
	u32 count;
	u32 temp;

	mu3d_dbg(K_DEBUG, "%s epnum=%d, len=%d, buf=%p, maxp=%d\n", __func__, ep_num, length, buf,
		 maxp);

	count = residue = length;

	while (residue > 0) {

		if (residue == 1) {
			temp = ((*buf) & 0xFF);
			mu3d_writeb(fifo, 0, temp);
			buf += 1;
			residue -= 1;
		} else if (residue == 2) {
			temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
			mu3d_writew(fifo, 0, temp);
			buf += 2;
			residue -= 2;
		} else if (residue == 3) {
			temp = ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00);
			mu3d_writew(fifo, 0, temp);
			buf += 2;

			temp = ((*buf) & 0xFF);
			mu3d_writeb(fifo, 0, temp);
			buf += 1;
			residue -= 3;
		} else {
			temp =
			    ((*buf) & 0xFF) + (((*(buf + 1)) << 8) & 0xFF00) +
			    (((*(buf + 2)) << 16) & 0xFF0000) + (((*(buf + 3)) << 24) & 0xFF000000);
			mu3d_writel(fifo, 0, temp);
			buf += 4;
			residue -= 4;
		}
	}

	if (ep_num == 0) {
		if (count == 0) {
			mu3d_dbg(K_DEBUG, "USB_EP0_DATAEND %8X+\n", mu3d_readl(mbase, U3D_EP0CSR));
			mu3d_setmsk(mbase, U3D_EP0CSR, EP0_DATAEND | EP0_TXPKTRDY);
			mu3d_dbg(K_DEBUG, "USB_EP0_DATAEND %8X-\n", mu3d_readl(mbase, U3D_EP0CSR));
		} else {
#ifdef AUTOSET
			if (count < maxp) {
				mu3d_setmsk(mbase, U3D_EP0CSR, EP0_TXPKTRDY);
				mu3d_dbg(K_DEBUG, "EP0_TXPKTRDY\n");
			}
#else
			mu3d_setmsk(mbase, U3D_EP0CSR, EP0_TXPKTRDY);
#endif
		}
	} else {
		if (count == 0) {
			mu3d_setmsk(hw_ep->addr_txcsr0, 0, TX_TXPKTRDY);
		} else {
#ifdef AUTOSET
			if (count < maxp) {
				mu3d_setmsk(hw_ep->addr_txcsr0, 0, TX_TXPKTRDY);
				mu3d_dbg(K_DEBUG, "short packet\n");
			}
#else
			mu3d_setmsk(hw_ep->addr_txcsr0, 0, TX_TXPKTRDY);
#endif
		}
	}
	return count;
}

/**
 * mu3d_hal_read_fifo - pio read one packet
 *@args - arg1: ep number,  arg2: data buffer
 */
/* NOTE: The function should not be used to read SETUP */
int mu3d_hal_read_fifo(struct musb_hw_ep *hw_ep, int ep_num, u8 *buf)
{
	struct musb *musb = hw_ep->musb;
	void __iomem *mbase = musb->mac_base;
	u16 count, residue;
	u32 temp;
	u8 *bp = buf;

	if (ep_num == 0)
		residue = count = mu3d_readl(mbase, U3D_RXCOUNT0);
	else
		residue = count = (mu3d_readl(hw_ep->addr_rxcsr3, 0) >> EP_RX_COUNT_OFST);

	mu3d_dbg(K_DEBUG, "%s, req->buf=%p, cnt=%d\n", __func__, buf, count);

	while (residue > 0) {

		temp = mu3d_readl(hw_ep->fifo, 0);

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
			mu3d_setmsk(mbase, U3D_EP0CSR, EP0_RXPKTRDY);
			mu3d_dbg(K_DEBUG, "EP0_RXPKTRDY\n");
		}
	} else {
		if (!count) {
			mu3d_setmsk(hw_ep->addr_rxcsr0, 0, RX_RXPKTRDY);
			mu3d_dbg(K_ALET, "ZLP\n");
		}
	}
#else
	if (ep_num == 0) {
		mu3d_setmsk(mbase, U3D_EP0CSR, EP0_RXPKTRDY);
		mu3d_dbg(K_DEBUG, "EP0_RXPKTRDY\n");
	} else {
		mu3d_setmsk(hw_ep->addr_rxcsr0, 0, RX_RXPKTRDY);
		mu3d_dbg(K_DEBUG, "RX_RXPKTRDY\n");
	}
#endif
	return count;
}

/**
* mu3d_hal_unfigured_ep -
 *@args -
 */
void mu3d_hal_unfigured_ep(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;
	u32 i, tx_ep_num, rx_ep_num;
	struct usb_ep_setting *ep_setting;
	u32 tmp;

	g_TxFIFOadd = USB_TX_FIFO_START_ADDRESS;
	g_RxFIFOadd = USB_RX_FIFO_START_ADDRESS;

#ifdef HARDCODE_EP
	tx_ep_num = MAX_QMU_EP;	/* os_readl(U3D_CAP_EPINFO) & CAP_TX_EP_NUM; */
	rx_ep_num = MAX_QMU_EP;	/* (os_readl(U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8; */
#else
	tx_ep_num = mu3d_readl(mbase, U3D_CAP_EPINFO) & CAP_TX_EP_NUM;
	rx_ep_num = (mu3d_readl(mbase, U3D_CAP_EPINFO) & CAP_RX_EP_NUM) >> 8;
#endif

	for (i = 1; i <= tx_ep_num; i++) {
		tmp = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, i);
		tmp &= ~TX_TXMAXPKTSZ;
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, i, tmp);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR1, i, 0);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR2, i, 0);
		ep_setting = &g_u3d_ep_setting[i];
		ep_setting->fifoaddr = 0;
		ep_setting->enabled = 0;
	}

	for (i = 1; i <= rx_ep_num; i++) {
		tmp = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, i);
		tmp &= ~RX_RXMAXPKTSZ;
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, i, tmp);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR1, i, 0);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR2, i, 0);
		ep_setting = &g_u3d_ep_setting[i + MAX_EP_NUM];
		ep_setting->fifoaddr = 0;
		ep_setting->enabled = 0;
	}
}


void mu3d_hal_unfigured_ep_num(struct musb *musb, int ep_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;
	struct usb_ep_setting *ep_setting;
	u32 tmp;

	mu3d_dbg(K_INFO, "%s %d\n", __func__, ep_num);

	if (dir == USB_TX) {
		tmp = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, ep_num);
		tmp &= ~TX_TXMAXPKTSZ;
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, ep_num, tmp);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR1, ep_num, 0);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR2, ep_num, 0);
		ep_setting = &g_u3d_ep_setting[ep_num];
		/* ep_setting->fifoaddr = 0; */
		ep_setting->enabled = 0;
	} else {
		tmp = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, ep_num);
		tmp &= ~RX_RXMAXPKTSZ;
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, ep_num, tmp);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR1, ep_num, 0);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR2, ep_num, 0);
		ep_setting = &g_u3d_ep_setting[ep_num + MAX_EP_NUM];
		/* ep_setting->fifoaddr = 0; */
		ep_setting->enabled = 0;
	}
}

/**
* mu3d_hal_ep_enable - configure ep
*@args - arg1: ep number, arg2: dir, arg3: transfer type, arg4: max packet size, arg5: interval, arg6: slot, arg7: burst, arg8: mult
*/
void mu3d_hal_ep_enable(struct musb *musb, int ep_num, USB_DIR dir, TRANSFER_TYPE type, int maxp,
			int interval, int slot, int burst, int mult)
{
	int ep_index = 0;
	int used_before;
	u8 fifosz = 0, max_pkt, binterval;
	int csr0, csr1, csr2;
	struct usb_ep_setting *ep_setting;
	void __iomem *mbase = musb->mac_base;
	u8 update_FIFOadd = 0;

	mu3d_dbg(K_INFO, "%s\n", __func__);

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
		mu3d_dbg(K_ALET, "[ERROR]Configure wrong slot number(MAX=%d, Not=%d)\n", MAX_SLOT,
			 slot);
		slot = MAX_SLOT;
	}

	if (type == USB_CTRL) {
		ep_setting = &g_u3d_ep_setting[0];
		ep_setting->fifosz = maxp;
		ep_setting->maxp = maxp;
		csr0 = mu3d_readl(mbase, U3D_EP0CSR) & EP0_W1C_BITS;
		csr0 |= maxp;
		mu3d_writel(mbase, U3D_EP0CSR, csr0);

		mu3d_setmsk(mbase, U3D_USB2_RX_EP_DATAERR_INTR, BIT16);	/* EP0 data error interrupt */
		return;
	}

	if (dir == USB_TX)
		ep_index = ep_num;
	else if (dir == USB_RX)
		ep_index = ep_num + MAX_EP_NUM;
	else
		BUG_ON(1);

	ep_setting = &g_u3d_ep_setting[ep_index];
	used_before = ep_setting->fifoaddr;

	if (ep_setting->enabled)
		return;

	binterval = interval;
	if (dir == USB_TX) {
		if ((g_TxFIFOadd + maxp * (slot + 1) > mu3d_readl(mbase, U3D_CAP_EPNTXFFSZ))
		    && (!used_before)) {
			mu3d_dbg(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			mu3d_dbg(K_ALET, "g_FIFOadd :%x\n", g_TxFIFOadd);
			mu3d_dbg(K_ALET, "maxp :%d\n", maxp);
			mu3d_dbg(K_ALET, "mult :%d\n", slot);
			BUG_ON(1);
		}
	} else {
		if ((g_RxFIFOadd + maxp * (slot + 1) > mu3d_readl(mbase, U3D_CAP_EPNRXFFSZ))
		    && (!used_before)) {
			mu3d_dbg(K_ALET, "mu3d_hal_ep_enable, FAILED: sram exhausted\n");
			mu3d_dbg(K_ALET, "g_FIFOadd :%x\n", g_RxFIFOadd);
			mu3d_dbg(K_ALET, "maxp :%d\n", maxp);
			mu3d_dbg(K_ALET, "mult :%d\n", slot);
			BUG_ON(1);
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
		mu3d_dbg(K_ERR, "%s Wrong MAXP\n", __func__);
		fifosz = USB_FIFOSZ_SIZE_1024;
	}

	if (dir == USB_TX) {
		/* CSR0 */
		csr0 = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, ep_num);
		csr0 &= ~TX_TXMAXPKTSZ;
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
		mu3d_setmsk(mbase, U3D_EPIECR, (BIT0 << ep_num));
		/* Enable QMU */
		mu3d_setmsk(mbase, U3D_QGCSR, QMU_TX_EN(ep_num));
		/* Enable QMU Done interrupt */
		mu3d_setmsk(mbase, U3D_QIESR0, QMU_TX_EN(ep_num));
#endif
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, ep_num, csr0);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR1, ep_num, csr1);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR2, ep_num, csr2);

		mu3d_dbg(K_DEBUG, "[CSR]U3D_TX%dCSR0 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_TX1CSR0, ep_num));
		mu3d_dbg(K_DEBUG, "[CSR]U3D_TX%dCSR1 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_TX1CSR1, ep_num));
		mu3d_dbg(K_DEBUG, "[CSR]U3D_TX%dCSR2 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_TX1CSR2, ep_num));

	} else if (dir == USB_RX) {
		/* CSR0 */
		csr0 = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, ep_num);
		csr0 &= ~RX_RXMAXPKTSZ;
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
		mu3d_setmsk(mbase, U3D_EPIECR, (BIT16 << ep_num));
		/* Enable QMU */
		mu3d_setmsk(mbase, U3D_QGCSR, QMU_TX_EN(ep_num));
		/*Enable QMU Done interrupt */
		mu3d_setmsk(mbase, U3D_QIESR0, QMU_RX_EN(ep_num));
#endif
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, ep_num, csr0);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR1, ep_num, csr1);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR2, ep_num, csr2);

		mu3d_dbg(K_DEBUG, "[CSR]U3D_RX%dCSR0 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_RX1CSR0, ep_num));
		mu3d_dbg(K_DEBUG, "[CSR]U3D_RX%dCSR1 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_RX1CSR1, ep_num));
		mu3d_dbg(K_DEBUG, "[CSR]U3D_RX%dCSR2 :%x\n", ep_num,
			 mu3d_xcsr_readl(mbase, U3D_RX1CSR2, ep_num));

		mu3d_setmsk(mbase, U3D_USB2_RX_EP_DATAERR_INTR, BIT16 << ep_num);	/* EPn data error interrupt */
	} else {
		mu3d_dbg(K_ERR, "WHAT THE DIRECTION IS?!?!\n");
		BUG_ON(1);
	}

	if (update_FIFOadd == 1) {

		/* The minimum unit of FIFO address is _16_ bytes.
		 * So let the offset of each EP fifo address aligns _16_ bytes.*/
		int fifo_offset = 0;

		if ((maxp & 0xF))
			fifo_offset = (maxp + 16) & (~0xF);
		else
			fifo_offset = maxp;

		if (dir == USB_TX)
			g_TxFIFOadd += (fifo_offset * (slot + 1));
		else
			g_RxFIFOadd += (fifo_offset * (slot + 1));
	}
}

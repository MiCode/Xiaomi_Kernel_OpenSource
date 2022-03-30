/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __LINUX_MT6360_H
#define __LINUX_MT6360_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

struct mt6360_tcpc_platform_data {
	struct resource *irq_res;
	int irq_res_cnt;
};

enum mt6360_vend_int {
	MT6360_VEND_INT1 = 0,
	MT6360_VEND_INT2,
	MT6360_VEND_INT3,
	MT6360_VEND_INT4,
	MT6360_VEND_INT5,
	MT6360_VEND_INT_MAX,
};

enum mt6360_id_rupsel {
	MT6360_USBID_RUP500K,
	MT6360_USBID_RUP75K,
	MT6360_USBID_RUP5K,
	MT6360_USBID_RUP1K,
	MT6360_USBID_RUP_MAX,
};

/*show debug message or not */
#define ENABLE_MT6360_DBG	1

/* MT6360 Private RegMap */

#define MT6360_REG_PHY_CTRL1				(0x80)
#define MT6360_REG_PHY_CTRL2				(0x81)
#define MT6360_REG_PHY_CTRL3				(0x82)
#define MT6360_REG_PHY_CTRL4				(0x83)
#define MT6360_REG_PHY_CTRL5				(0x84)
#define MT6360_REG_PHY_CTRL6				(0x85)
#define MT6360_REG_PHY_CTRL7				(0x86)

#define MT6360_REG_CLK_CTRL1				(0x87)
#define MT6360_REG_CLK_CTRL2				(0x88)

#define MT6360_REG_PHY_CTRL8				(0x89)

#define MT6360_REG_CC1_CTRL1				(0x8A)
#define MT6360_REG_VCONN_CTRL1				(0x8C)

#define MT6360_REG_MODE_CTRL1				(0x8D)
#define MT6360_REG_MODE_CTRL2				(0x8F)
#define MT6360_REG_MODE_CTRL3				(0x90)

#define MT6360_REG_MT_MASK1				(0x91)
#define MT6360_REG_MT_MASK2				(0x92)
#define MT6360_REG_MT_MASK3				(0x93)
#define MT6360_REG_MT_MASK4				(0x94)
#define MT6360_REG_MT_MASK5				(0x95)

#define MT6360_REG_MT_INT1				(0x96)
#define MT6360_REG_MT_INT2				(0x97)
#define MT6360_REG_MT_INT3				(0x98)
#define MT6360_REG_MT_INT4				(0x99)
#define MT6360_REG_MT_INT5				(0x9A)

#define MT6360_REG_MT_ST1				(0x9B)
#define MT6360_REG_MT_ST2				(0x9C)
#define MT6360_REG_MT_ST3				(0x9D)
#define MT6360_REG_MT_ST4				(0x9E)
#define MT6360_REG_MT_ST5				(0x9F)

#define MT6360_REG_SWRESET				(0xA0)
#define MT6360_REG_DEBOUNCE_CTRL1			(0xA1)
#define MT6360_REG_DRP_CTRL1				(0xA2)
#define MT6360_REG_DRP_CTRL2				(0xA3)
#define MT6360_REG_DRP_CTRL3				(0xA4)
#define MT6360_REG_PD3_CTRL				(0xAF)

#define MT6360_REG_VBUS_DISC_CTRL			(0xB5)
#define MT6360_REG_CTD_CTRL1				(0xBD)
#define MT6360_REG_WATCHDOG_CTRL			(0xBE)
#define MT6360_REG_I2CRST_CTRL				(0xBF)

#define MT6360_REG_WD_DET_CTRL1				(0xC0)
#define MT6360_REG_WD_DET_CTRL2				(0xC1)
#define MT6360_REG_WD_DET_CTRL3				(0xC2)
#define MT6360_REG_WD_DET_CTRL4				(0xC3)
#define MT6360_REG_WD_DET_CTRL5				(0xC4)
#define MT6360_REG_WD_DET_CTRL6				(0xC5)
#define MT6360_REG_WD_DET_CTRL7				(0xC6)
#define MT6360_REG_WD_DET_CTRL8				(0xC7)

#define MT6360_REG_PHY_CTRL9				(0xC8)
#define MT6360_REG_PHY_CTRL10				(0xC9)
#define MT6360_REG_PHY_CTRL11				(0xCA)
#define MT6360_REG_PHY_CTRL12				(0xCB)
#define MT6360_REG_PHY_CTRL13				(0xCC)
#define MT6360_REG_PHY_CTRL14				(0xCD)
#define MT6360_REG_RX_CTRL1				(0xCE)
#define MT6360_REG_RX_CTRL2				(0xCF)

#define MT6360_REG_VBUS_CTRL2				(0xDA)
#define MT6360_REG_HILO_CTRL5				(0xDB)

#define MT6360_REG_VCONN_CTRL2				(0xE2)
#define MT6360_REG_VCONN_CTRL3				(0xE3)

#define MT6360_REG_DEBOUNCE_CTRL4			(0xE5)
#define MT6360_REG_CTD_CTRL2				(0xEC)

/*
 * Device ID
 */

#define MT6360_DID_A		0x3072

/*
 * MT6360_REG_PHY_CTRL1				(0x80)
 */

#define MT6360_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * MT6360_REG_CLK_CTRL1				(0x87)
 */

#define MT6360_CLK_DIV_600K_EN			BIT(7)
#define MT6360_CLK_BCLK2_EN			BIT(6)
#define MT6360_CLK_BCLK2_TG_EN			BIT(5)
#define MT6360_CLK_DIV_300K_EN			BIT(3)
#define MT6360_CLK_BCLK_EN			BIT(1)
#define MT6360_CLK_BCLK_TG_EN			BIT(0)

/*
 * MT6360_REG_CLK_CTRL2				(0x88)
 */

#define MT6360_CLK_OSCMUX_RG_EN			BIT(7)
#define MT6360_CLK_CK_24M_EN			BIT(6)
#define MT6360_CLK_OSC_RG_EN			BIT(5)
#define MT6360_CLK_DIV_2P4M_EN			BIT(4)
#define MT6360_CLK_CK_1P5M_EN			BIT(3)
#define MT6360_CLK_PCLK_EN			BIT(2)
#define MT6360_CLK_PCLK_RG_EN			BIT(1)
#define MT6360_CLK_PCLK_TG_EN			BIT(0)

/*
 * MT6360_REG_PHY_CTRL8
 */
#define MT6360_PRL_FSM_RSTB			BIT(1)

/*
 * MT6360_REG_VCONN_CTRL1			(0x8C)
 */
#define MT6360_VCONN_OCP_SEL			0xE0
#define MT6360_VCONN_CLIMIT_EN			BIT(0)

/*
 * MT6360_REG_MODE_CTRL2			(0x8F)
 */

#define MT6360_ENEXTMSG				BIT(6)
#define MT6360_SHIPPING_OFF			BIT(5)
#define MT6360_WAKEUP_EN			BIT(4)
#define MT6360_AUTOIDLE_EN			BIT(3)
#define MT6360_AUTOIDLE_TOUT			0x07

/* timeout = (tout*2+1) * 6.4ms */
#if CONFIG_USB_PD_REV30
#define MT6360_REG_MODE_CTRL2_SET(ship_dis, auto_idle, tout) \
	((ship_dis << 5) | (auto_idle << 3) | (tout & MT6360_AUTOIDLE_TOUT) | \
	 MT6360_WAKEUP_EN | MT6360_ENEXTMSG)
#else
#define MT6360_REG_MODE_CTRL2_SET(ship_dis, auto_idle, tout) \
	((ship_dis << 5) | (auto_idle << 3) | (tout & MT6360_AUTOIDLE_TOUT) | \
	 MT6360_WAKEUP_EN)
#endif /* CONFIG_USB_PD_REV30 */

/*
 * MT6360_REG_MODE_CTRL3			(0x90)
 */

#define MT6360_PD_BG_EN				BIT(7)
#define MT6360_BMCIO_IDLE_EN			BIT(6)
#define MT6360_VCONN_DISCHARGE_EN		BIT(5)
#define MT6360_LPWR_LDO_EN			BIT(4)
#define MT6360_LPWR_EN				BIT(3)
#define MT6360_PD_IREF_EN			BIT(2)
#define MT6360_VBUS_DET_EN			BIT(1)
#define MT6360_BMCIO_OSC_EN			BIT(0)

/*
 * MT6360_REG_MT_MASK1				(0x91)
 */

#define MT6360_M_VCONN_SHT_GND			BIT(3)
#define MT6360_M_VBUS_SAFE0V			BIT(1)
#define MT6360_M_WAKEUP				BIT(0)

/*
 * MT6360_REG_MT_MASK2				(0x92)
 */

#define MT6360_M_WD_ONESHOT_EVT			BIT(7)
#define MT6360_M_WD_EVT				BIT(6)
#define MT6360_M_VCONN_INVALID			BIT(4)
#define MT6360_M_VCONOCP_CLIMIT			BIT(3)
#define MT6360_M_VCONN_OCR			BIT(2)
#define MT6360_M_VCONN_OV_CC2			BIT(1)
#define MT6360_M_VCONN_OV_CC1			BIT(0)

/*
 * MT6360_REG_MT_MASK3				(0x93)
 */

#define MT6360_M_CTD				BIT(4)

/*
 * MT6360_REG_MT_MASK5				(0x95)
 */
#define MT6360_M_HIDET_CC2			BIT(5)
#define MT6360_M_HIDET_CC1			BIT(4)
#define MT6360_M_LODET_CC2			BIT(3)
#define MT6360_M_LODET_CC1			BIT(2)
#define MT6360_M_HIDET_CC \
	(MT6360_M_HIDET_CC2 | MT6360_M_HIDET_CC1)
#define MT6360_M_LODET_CC \
	(MT6360_M_LODET_CC2 | MT6360_M_LODET_CC1)

/*
 * MT6360_REG_MT_INT1				(0x96)
 */

#define MT6360_INT_VBUS_80			BIT(1)
#define MT6360_INT_WAKEUP			BIT(0)

/*
 * MT6360_REG_MT_INT2				(0x97)
 */

#define MT6360_INT_WATER_DET_DONE		BIT(7)
#define MT6360_INT_WATER_EVENT			BIT(6)

/*
 * MT6360_REG_MT_ST1				(0x9B)
 */

#define MT6360_ST_VCONN_SHT_GND			BIT(3)
#define MT6360_ST_VBUS_SAFE0V			BIT(1)

/*
 * MT6360_REG_MT_ST2				(0x9C)
 */

#define MT6360_ST_WATER_DET			0xC0
#define MT6360_ST_VCONN_INVALID			BIT(4)
#define MT6360_ST_VCONN_RV			BIT(2)
#define MT6360_ST_VCONN_OV_CC2			BIT(1)
#define MT6360_ST_VCONN_OV_CC1			BIT(0)
#define MT6360_ST_VCONN_FAULT \
	(MT6360_ST_VCONN_RV | MT6360_ST_VCONN_OV_CC2 | \
	 MT6360_ST_VCONN_OV_CC1 | MT6360_ST_VCONN_INVALID)

/*
 * MT6360_REG_MT_ST3				(0x9D)
 */
#define MT6360_ST_CABLE_TYPE			BIT(4)

/*
 * MT6360_REG_MT_ST5				(0x9F)
 */

#define MT6360_ST_HIDET_CC2			BIT(5)
#define MT6360_ST_HIDET_CC1			BIT(4)
#define MT6360_ST_LODET_CC2			BIT(3)
#define MT6360_ST_LODET_CC1			BIT(2)
#define MT6360_ST_HIDET_CC \
	(MT6360_ST_HIDET_CC2 | MT6360_ST_HIDET_CC1)
#define MT6360_ST_LODET_CC \
	(MT6360_ST_LODET_CC2 | MT6360_ST_LODET_CC1)

/*
 * MT6360_REG_WATCHDOG_CTRL			(0xBE)
 */

#define MT6360_WATCHDOG_SEL			0x0F

/* timeout = (tout+1) * 0.4sec */
#define MT6360_REG_WATCHDOG_CTRL_SET(tout) \
	(tout & MT6360_WATCHDOG_SEL)

/*
 * MT6360_REG_I2CRST_CTRL			(0xBF)
 */

#define MT6360_I2CRST_EN			BIT(7)
#define MT6360_I2CRST_SEL			0x0F

/* timeout = (tout+1) * 12.5ms */
#define MT6360_REG_I2CRST_SET(en, tout)	\
	((en << 7) | (tout & MT6360_I2CRST_SEL))

/*
 * MT6360_REG_WD_DET_CTRL1			(0xC0)
 */
#define MT6360_WD_DET_EN			BIT(7)
#define MT6360_WD_PROTECTION_EN			BIT(6)
#define MT6360_WD_ONESHOT_EN			BIT(5)
#define MT6360_WD_CC2_RUST_STS			BIT(3)
#define MT6360_WD_CC1_RUST_STS			BIT(2)
#define MT6360_WD_DM_RUST_STS			BIT(1)
#define MT6360_WD_DP_RUST_STS			BIT(0)
#define MT6360_WD_ALL_RUST_STS			0x0F

/*
 * MT6360_REG_WD_DET_CTRL3			(0xC2)
 */
#define MT6360_WD_DET_DPDM_PU			BIT(0)

/*
 * MT6360_REG_WD_DET_CTRL4			(0xC3)
 */
#define MT6360_WD_DET_CC_RPSEL			BIT(7)
#define MT6360_WD_DET_CC_ROLE_PRT		BIT(6)
#define MT6360_WD_DET_CC_ROLE_DET		BIT(5)

/*
 * MT6360_REG_WD_DET_CTRL5			(0xC4)
 */
#define MT6360_WD_SLEEP_TIME			0xFF

/* sleep time = (time + 1) * 102.4ms */
#define MT6360_REG_WD_DET_CTRL5_SET(time) \
	(time & MT6360_WD_SLEEP_TIME)

/*
 * MT6360_REG_RX_CTRL2				(0xCF)
 */

#define MT6360_OPEN400MS_EN			BIT(7)

/*
 * MT6360_REG_HILO_CTRL5			(0xDB)
 */
#define MT6360_CMPEN_HIDET_CC2			BIT(4)
#define MT6360_CMPEN_LODET_CC2			BIT(3)
#define MT6360_CMPEN_HIDET_CC1			BIT(1)
#define MT6360_CMPEN_LODET_CC1			BIT(0)
#define MT6360_CMPEN_HIDET_CC \
	(MT6360_CMPEN_HIDET_CC2 | MT6360_CMPEN_HIDET_CC1)
#define MT6360_CMPEN_LODET_CC \
	(MT6360_CMPEN_LODET_CC2 | MT6360_CMPEN_LODET_CC1)

/*
 * MT6360_REG_VCONN_CTRL2		(0xE2)
 */
#define MT6360_VCONN_OVP_CC1_EN		BIT(4)
#define MT6360_VCONN_OVP_CC2_EN		BIT(3)
#define MT6360_VCONN_OVP_CC_EN \
	(MT6360_VCONN_OVP_CC1_EN | MT6360_VCONN_OVP_CC2_EN)

/*
 * MT6360_REG_VCONN_CTRL3		(0xE3)
 */
#define MT6360_VCONN_RVP_EN		BIT(7)

/*
 * MT6360_REG_CTD_CTRL2			(0xEC)
 */
#define MT6360_DIS_RPDET			BIT(7)
#define MT6360_RPDET_ONESHOT			BIT(6)

#if ENABLE_MT6360_DBG
#define MT6360_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define MT6360_INFO(foramt, args...)
#endif /* ENABLE_MT6360_DBG */

#endif /* #ifndef __LINUX_MT6360_H */

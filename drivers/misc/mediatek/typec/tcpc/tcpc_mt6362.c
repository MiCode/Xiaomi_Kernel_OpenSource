// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/iio/consumer.h>
#include <uapi/linux/sched/types.h>
#include <dt-bindings/mfd/mt6362.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_core.h"
#include "inc/std_tcpci_v10.h"

#define MT6362_INFO_EN	1
#define MT6362_DBGINFO_EN	1
#define MT6362_WD1_EN	1
#define MT6362_WD2_EN	1

#define MT6362_INFO(fmt, ...) \
	do { \
		if (MT6362_INFO_EN) \
			pd_dbg_info("%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define MT6362_DBGINFO(fmt, ...) \
	do { \
		if (MT6362_DBGINFO_EN) \
			pd_dbg_info("%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define MT6362_VID	0x29CF
#define MT6362_PID	0x6362

#define MT6362_IRQ_WAKE_TIME	(500) /* ms */

#define MT6362_REG_BASEADDR	(0x400)
#define MT6362_REG_RT2BASEADDR	(0x500)
#define MT6362_REG_PHYCTRL1	(0x80)
#define MT6362_REG_PHYCTRL2	(0x81)
#define MT6362_REG_PHYCTRL3	(0x82)
#define MT6362_REG_PHYCTRL7	(0x86)
#define MT6362_REG_PHYCTRL8	(0x89)
#define MT6362_REG_VCONCTRL2	(0x8B)
#define MT6362_REG_VCONCTRL3	(0x8C)
#define MT6362_REG_SYSCTRL1	(0x8F)
#define MT6362_REG_SYSCTRL2	(0x90)
#define MT6362_REG_MTMASK1	(0x91)
#define MT6362_REG_MTMASK2	(0x92)
#define MT6362_REG_MTMASK3	(0x93)
#define MT6362_REG_MTMASK4	(0x94)
#define MT6362_REG_MTMASK5	(0x95)
#define MT6362_REG_MTMASK6	(0x96)
#define MT6362_REG_MTMASK7	(0x97)
#define MT6362_REG_MTINT1	(0x98)
#define MT6362_REG_MTINT2	(0x99)
#define MT6362_REG_MTINT3	(0x9A)
#define MT6362_REG_MTINT4	(0x9B)
#define MT6362_REG_MTINT5	(0x9C)
#define MT6362_REG_MTINT6	(0x9D)
#define MT6362_REG_MTINT7	(0x9E)
#define MT6362_REG_MTST1	(0x9F)
#define MT6362_REG_MTST2	(0xA0)
#define MT6362_REG_MTST3	(0xA1)
#define MT6362_REG_MTST4	(0xA2)
#define MT6362_REG_MTST5	(0xA3)
#define MT6362_REG_MTST6	(0xA4)
#define MT6362_REG_MTST7	(0xA5)
#define MT6362_REG_PHYCTRL9	(0xAC)
#define MT6362_REG_SYSCTRL3	(0xB0)
#define MT6362_REG_TCPCCTRL1	(0xB1)
#define MT6362_REG_TCPCCTRL2	(0xB2)
#define MT6362_REG_TCPCCTRL3	(0xB3)
#define MT6362_REG_LPWRCTRL3	(0xBB)
#define MT6362_REG_WATCHDOGCTRL	(0xBE)
#define MT6362_REG_HILOCTRL9	(0xC8)
#define MT6362_REG_HILOCTRL10	(0xC9)
#define MT6362_REG_SHIELDCTRL1	(0xCA)
#define MT6362_REG_TYPECOTPCTRL	(0xCD)
#define MT6362_REG_WD12MODECTRL	(0xD0)
#define MT6362_REG_WD1PATHEN	(0xD1)
#define MT6362_REG_WD1MISCCTRL	(0xD2)
#define MT6362_REG_WD2PATHEN	(0xD3)
#define MT6362_REG_WD2MISCCTRL	(0xD4)
#define MT6362_REG_WD1PULLST	(0xD5)
#define MT6362_REG_WD1DISCHGST	(0xD6)
#define MT6362_REG_WD2PULLST	(0xD7)
#define MT6362_REG_WD2DISCHGST	(0xD8)
#define MT6362_REG_WD0MODECTRL	(0xD9)
#define MT6362_REG_WD0SET	(0xDA)
#define MT6362_REG_WDSET	(0xDB)
#define MT6362_REG_WDSET1	(0xDC)

/* RT2 */
#define MT6362_REG_WDSET2	(0x20)
#define MT6362_REG_WDSET3	(0x21)
#define MT6362_REG_WD1MISCSET	(0x22)
#define MT6362_REG_WD1VOLCMP	(0x23)
#define MT6362_REG_WD2MISCSET	(0x28)
#define MT6362_REG_WD2VOLCMP	(0x29)

#define MT6362_MSK_OPEN40MS_EN	BIT(4)
#define MT6362_MSK_WAKEUP	BIT(0)
#define MT6362_MSK_VBUS80	BIT(1)
#define MT6362_MSK_OTDFLAG	BIT(2)
#define MT6362_MSK_VCON_OVCC1	BIT(0)
#define MT6362_MSK_VCON_OVCC2	BIT(1)
#define MT6362_MSK_VCON_RVP	BIT(2)
#define MT6362_MSK_VCON_UVP	BIT(4)
#define MT6362_MSK_VCON_SHTGND	BIT(5)
#define MT6362_MSK_VCON_FAULT \
	(MT6362_MSK_VCON_OVCC1 | MT6362_MSK_VCON_OVCC2 | MT6362_MSK_VCON_RVP | \
	 MT6362_MSK_VCON_UVP | MT6362_MSK_VCON_SHTGND)
#define MT6362_MSK_CTD		BIT(4)
#define MT6362_MSK_FOD_DONE	BIT(0)
#define MT6362_MSK_FOD_OV	BIT(1)
#define MT6362_MSK_FOD_DISCHGF	BIT(7)
#define MT6362_MSK_RPDET_AUTO	BIT(7)
#define MT6362_MSK_RPDET_MANUAL	BIT(6)
#define MT6362_MSK_CTD_EN	BIT(1)
#define MT6362_MSK_BMCIOOSC_EN	BIT(0)
#define MT6362_MSK_VBUSDET_EN	BIT(1)
#define MT6362_MSK_LPWR_EN	BIT(3)
#define MT6362_MSK_VCON_OVCC1EN	BIT(7)
#define MT6362_MSK_VCON_OVCC2EN	BIT(6)
#define MT6362_MSK_VCON_RVPEN	BIT(3)
#define MT6362_MSK_VCON_PROTEN	\
	(MT6362_MSK_VCON_OVCC1EN | MT6362_MSK_VCON_OVCC2EN | \
	 MT6362_MSK_VCON_RVPEN)
#define MT6362_MSK_PRLRSTB	BIT(1)
#define MT6362_MSK_TYPECOTP_FWEN	BIT(2)
#define MT6362_MSK_HIDET_CC1	BIT(4)
#define MT6362_MSK_HIDET_CC2	BIT(5)
#define MT6362_MSK_HIDET_CC	(MT6362_MSK_HIDET_CC1 | MT6362_MSK_HIDET_CC2)
#define MT6362_MSK_HIDET_CC1_CMPEN	BIT(1)
#define MT6362_MSK_HIDET_CC2_CMPEN	BIT(4)
#define MT6362_MSK_HIDET_CC_CMPEN \
	(MT6362_MSK_HIDET_CC1_CMPEN | MT6362_MSK_HIDET_CC2_CMPEN)
#define MT6362_MSK_FOD_DONE	BIT(0)
#define MT6362_MSK_FOD_OV	BIT(1)
#define MT6362_MSK_FOD_LR	BIT(5)
#define MT6362_MSK_FOD_HR	BIT(6)
#define MT6362_MSK_FOD_DISCHGF	BIT(7)
#define MT6362_MSK_FOD_ALL \
	(MT6362_MSK_FOD_DONE | MT6362_MSK_FOD_OV | MT6362_MSK_FOD_LR | \
	 MT6362_MSK_FOD_HR | MT6362_MSK_FOD_DISCHGF)
#define MT6362_MSK_CABLE_TYPEC	BIT(4)
#define MT6362_MSK_CABLE_TYPEA	BIT(5)
#define MT6362_MSK_SHIPPING_OFF	BIT(5)
#define MT6362_MSK_AUTOIDLE_EN	BIT(3)
#define MT6362_MSK_WD12_STFALL	BIT(0)
#define MT6362_MSK_WD12_STRISE	BIT(1)
#define MT6362_MSK_WD12_DONE	BIT(2)
#define MT6362_MSK_WDIPULL_SEL	(0x70)
#define MT6362_SFT_WDIPULL_SEL	(4)
#define MT6362_MSK_WDRPULL_SEL	(0x0E)
#define MT6362_SFT_WDRPULL_SEL	(1)
#define MT6362_MSK_WD12_VOLCOML	(0x0F)
#define MT6362_SFT_WD12_VOLCOML	(0)
#define MT6362_MSK_WDSBU1_EN	BIT(0)
#define MT6362_MSK_WDSBU2_EN	BIT(1)
#define MT6362_MSK_WDCC1_EN	BIT(2)
#define MT6362_MSK_WDCC2_EN	BIT(3)
#define MT6362_MSK_WDDP_EN	BIT(4)
#define MT6362_MSK_WDDM_EN	BIT(5)
#define MT6362_MSK_WD12PROT	BIT(6)
#define MT6362_MSK_WD12MODE_EN	BIT(4)
#define MT6362_MSK_WDIPULL_EN	BIT(3)
#define MT6362_MSK_WDRPULL_EN	BIT(2)
#define MT6362_MSK_WDDISCHG_EN	BIT(1)
#define MT6362_MSK_WDFWMODE_EN	BIT(0)
#define MT6362_MSK_WDLDO_SEL	(0xC0)
#define MT6362_SFT_WDLDO_SEL	(6)
#define MT6362_MSK_WD0MODE_EN	BIT(4)
#define MT6362_MSK_WD0PULL_STS	BIT(7)
#define MT6362_MASK_WD_TDET	(0x07)
#define MT6362_SHFT_WD_TDET	(0)

/* for Rust Protect DPDM */
#define MT6362_REG_DPDM_CTRL1	(0x53)
#define MT6362_MSK_MANUAL_MODE	BIT(7)
#define MT6362_MSK_DPDM_DET_EN	BIT(6)

#define MT6362_WD_TDET_10MS	(0x04)
#define MT6362_WD_TDET_1MS	(0x01)

#define MT6362_WD_VOL_CMPL_1_44V	(0x0A)
#define MT6362_WD_VOL_CMPL_1_54V	(0x0B)

enum mt6362_vend_int {
	MT6362_VEND_INT1 = 0,
	MT6362_VEND_INT2,
	MT6362_VEND_INT3,
	MT6362_VEND_INT4,
	MT6362_VEND_INT5,
	MT6362_VEND_INT6,
	MT6362_VEND_INT7,
	MT6362_VEND_INT_NUM,
};

enum mt6362_wd_chan {
	MT6362_WD_CHAN_WD1,
	MT6362_WD_CHAN_WD2,
	MT6362_WD_CHAN_NUM,
};

enum mt6362_wd_rpull {
	MT6362_WD_RPULL_500K,
	MT6362_WD_RPULL_200K,
	MT6362_WD_RPULL_75K,
	MT6362_WD_RPULL_40K,
	MT6362_WD_RPULL_20K,
	MT6362_WD_RPULL_10K,
	MT6362_WD_RPULL_5K,
	MT6362_WD_RPULL_1K,
};

enum mt6362_wd_ipull {
	MT6362_WD_IPULL_2UA,
	MT6362_WD_IPULL_5UA,
	MT6362_WD_IPULL_10UA,
	MT6362_WD_IPULL_20UA,
	MT6362_WD_IPULL_40UA,
	MT6362_WD_IPULL_80UA,
	MT6362_WD_IPULL_160UA,
	MT6362_WD_IPULL_240UA,
};

enum mt6362_wd_status {
	MT6362_WD_PULL,
	MT6362_WD_DISCHG,
	MT6362_WD_STATUS_NUM,
};

enum mt6362_wd_ldo {
	MT6362_WD_LDO_0_6V,
	MT6362_WD_LDO_1_8V,
	MT6362_WD_LDO_2_5V,
	MT6362_WD_LDO_3_0V,
};

static const u8 mt6362_vend_alert_clearall[MT6362_VEND_INT_NUM] = {
	0xFF, 0xFF, 0xF0, 0xE3, 0xFF, 0xF8, 0x3F,
};

static const u8 mt6362_vend_alert_maskall[MT6362_VEND_INT_NUM] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#ifdef CONFIG_WATER_DETECTION
/* reg0x20 ~ reg0x2D */
static const u8 mt6362_rt2_wd_init_setting[] = {
	0x50, 0x34, 0x44, 0xCA, 0x68, 0x02, 0x20, 0x03,
	0x44, 0xCA, 0x68, 0x02, 0x20, 0x03,
};

static const bool mt6362_wd_chan_en[MT6362_WD_CHAN_NUM] = {
	MT6362_WD1_EN,
	MT6362_WD2_EN,
};

static const u8 mt6362_wd_status_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1PULLST,
	MT6362_REG_WD2PULLST,
};

static const u8 mt6362_wd_miscctrl_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1MISCCTRL,
	MT6362_REG_WD2MISCCTRL,
};

static const u8 mt6362_wd_path_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1PATHEN,
	MT6362_REG_WD2PATHEN,
};

static const u8 mt6362_wd_rpull_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1MISCSET,
	MT6362_REG_WD2MISCSET
};

static const u8 __maybe_unused mt6362_wd_ipull_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1MISCSET,
	MT6362_REG_WD2MISCSET
};

static const u8 mt6362_wd_volcmp_reg[MT6362_WD_CHAN_NUM] = {
	MT6362_REG_WD1VOLCMP,
	MT6362_REG_WD2VOLCMP,
};

static const u8 mt6362_wd_polling_path[MT6362_WD_CHAN_NUM] = {
	MT6362_MSK_WDSBU1_EN,
	MT6362_MSK_WDSBU2_EN,
};

static const u8 mt6362_wd_protection_path[MT6362_WD_CHAN_NUM] = {
	MT6362_MSK_WDSBU1_EN | MT6362_MSK_WDSBU2_EN |
	MT6362_MSK_WDCC1_EN | MT6362_MSK_WDCC2_EN,
//	MT6362_MSK_WDDP_EN | MT6362_MSK_WDDM_EN,
	MT6362_MSK_WDSBU1_EN | MT6362_MSK_WDSBU2_EN |
	MT6362_MSK_WDCC1_EN | MT6362_MSK_WDCC2_EN,
//	MT6362_MSK_WDDP_EN | MT6362_MSK_WDDM_EN,
};
#endif /* CONFIG_WATER_DETECTION */

struct mt6362_tcpc_data {
	struct device *dev;
	struct regmap *regmap;
	struct tcpc_desc *desc;
	struct tcpc_device *tcpc;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct iio_channel *adc_iio;
	int irq;
	u16 did;

	atomic_t cpu_poll_count;
	struct delayed_work cpu_poll_dwork;

#ifdef CONFIG_WATER_DETECTION
	atomic_t wd_protect_rty;
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_WD_POLLING_ONLY
	struct delayed_work wd_poll_dwork;
#endif /* CONFIG_WD_POLLING_ONLY */

#ifdef CONFIG_CABLE_TYPE_DETECTION
	bool handle_init_ctd;
	enum tcpc_cable_type init_cable_type;
#endif /* CONFIG_CABLE_TYPE_DETECTION */

};

static inline int mt6362_write8(struct mt6362_tcpc_data *tdata, u32 reg,
				u8 data)
{
	return regmap_write(tdata->regmap, reg + MT6362_REG_BASEADDR, data);
}

static inline int mt6362_read8(struct mt6362_tcpc_data *tdata, u32 reg,
			       u8 *data)
{
	int ret;
	u32 _data;

	ret = regmap_read(tdata->regmap, reg + MT6362_REG_BASEADDR, &_data);
	if (ret < 0)
		return ret;
	*data = _data;
	return 0;
}

static inline int mt6362_write16(struct mt6362_tcpc_data *tdata, u32 reg,
				 u16 data)
{
	data = cpu_to_le16(data);
	return regmap_bulk_write(tdata->regmap, reg + MT6362_REG_BASEADDR,
				 &data, 2);
}

static inline int mt6362_read16(struct mt6362_tcpc_data *tdata, u32 reg,
				u16 *data)
{
	int ret;

	ret = regmap_bulk_read(tdata->regmap, reg + MT6362_REG_BASEADDR, data,
			       2);
	if (ret < 0)
		return ret;
	*data = le16_to_cpu(*data);
	return 0;
}

static inline int mt6362_bulk_write(struct mt6362_tcpc_data *tdata, u32 reg,
				    const void *data, size_t count)
{
	return regmap_bulk_write(tdata->regmap, reg + MT6362_REG_BASEADDR, data,
				 count);
}


static inline int mt6362_bulk_read(struct mt6362_tcpc_data *tdata, u32 reg,
				   void *data, size_t count)
{
	return regmap_bulk_read(tdata->regmap, reg + MT6362_REG_BASEADDR, data,
				count);
}

static inline int mt6362_update_bits(struct mt6362_tcpc_data *tdata, u32 reg,
				     u8 mask, u8 data)
{
	return regmap_update_bits(tdata->regmap, reg + MT6362_REG_BASEADDR,
				  mask, data);
}

static inline int mt6362_set_bits(struct mt6362_tcpc_data *tdata, u32 reg,
				  u8 mask)
{
	return mt6362_update_bits(tdata, reg, mask, mask);
}

static inline int mt6362_clr_bits(struct mt6362_tcpc_data *tdata, u32 reg,
				  u8 mask)
{
	return mt6362_update_bits(tdata, reg, mask, 0);
}

static inline int mt6362_write8_rt2(struct mt6362_tcpc_data *tdata, u32 reg,
				    u8 data)
{
	return regmap_write(tdata->regmap, reg + MT6362_REG_RT2BASEADDR, data);
}

static inline int mt6362_bulk_write_rt2(struct mt6362_tcpc_data *tdata, u32 reg,
					const void *data, size_t count)
{
	return regmap_bulk_write(tdata->regmap, reg + MT6362_REG_RT2BASEADDR,
				 data, count);
}

static inline int mt6362_update_bits_rt2(struct mt6362_tcpc_data *tdata,
					 u32 reg, u8 mask, u8 data)
{
	return regmap_update_bits(tdata->regmap, reg + MT6362_REG_RT2BASEADDR,
				  mask, data);
}

static int mt6362_sw_reset(struct mt6362_tcpc_data *tdata)
{
	int ret;

	ret = mt6362_write8(tdata, MT6362_REG_SYSCTRL3, 0x01);
	if (ret < 0)
		return ret;
	usleep_range(1000, 2000);

	/* disable ctd_en */
	return mt6362_clr_bits(tdata, MT6362_REG_SHIELDCTRL1,
			       MT6362_MSK_CTD_EN);
}

static int mt6362_init_power_status_mask(struct mt6362_tcpc_data *tdata)
{
	return mt6362_write8(tdata, TCPC_V10_REG_POWER_STATUS_MASK,
			     TCPC_V10_REG_POWER_STATUS_VBUS_PRES);
}

static int mt6362_init_fault_mask(struct mt6362_tcpc_data *tdata)
{
	return mt6362_write8(tdata, TCPC_V10_REG_FAULT_STATUS_MASK,
			     TCPC_V10_REG_FAULT_STATUS_VCONN_OC);
}

static int mt6362_init_ext_mask(struct mt6362_tcpc_data *tdata)
{
	return mt6362_write8(tdata, TCPC_V10_REG_EXT_STATUS_MASK, 0x00);
}

static int mt6362_init_vend_mask(struct mt6362_tcpc_data *tdata)
{
	u8 mask[MT6362_VEND_INT_NUM] = {0};

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask[MT6362_VEND_INT1] |= MT6362_MSK_VBUS80;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	if (tdata->tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		mask[MT6362_VEND_INT1] |= MT6362_MSK_WAKEUP;

	mask[MT6362_VEND_INT2] |= MT6362_MSK_VCON_OVCC1 |
				  MT6362_MSK_VCON_OVCC2 |
				  MT6362_MSK_VCON_RVP |
				  MT6362_MSK_VCON_UVP |
				  MT6362_MSK_VCON_SHTGND;

	if (tdata->tcpc->tcpc_flags & TCPC_FLAGS_CABLE_TYPE_DETECTION)
		mask[MT6362_VEND_INT3] |= MT6362_MSK_CTD;

	if (tdata->tcpc->tcpc_flags & TCPC_FLAGS_WATER_DETECTION)
		mask[MT6362_VEND_INT7] |= MT6362_MSK_WD12_STFALL |
					  MT6362_MSK_WD12_STRISE |
					  MT6362_MSK_WD12_DONE;

	return mt6362_bulk_write(tdata, MT6362_REG_MTMASK1, mask,
				 MT6362_VEND_INT_NUM);
}

static int mt6362_init_alert_mask(struct mt6362_tcpc_data *tdata)
{
	u16 mask = TCPC_V10_REG_ALERT_CC_STATUS |
		   TCPC_V10_REG_ALERT_POWER_STATUS |
		   TCPC_V10_REG_ALERT_VENDOR_DEFINED;

#ifdef CONFIG_USB_POWER_DELIVERY
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS |
		TCPC_V10_REG_ALERT_TX_DISCARDED |
		TCPC_V10_REG_ALERT_TX_FAILED |
		TCPC_V10_REG_ALERT_RX_HARD_RST |
		TCPC_V10_REG_ALERT_RX_STATUS |
		TCPC_V10_REG_RX_OVERFLOW;
#endif /* CONFIG_USB_POWER_DELIVERY */

	mask |= TCPC_REG_ALERT_FAULT;
	return mt6362_write16(tdata, TCPC_V10_REG_ALERT_MASK, mask);
}

static int __mt6362_set_cc(struct mt6362_tcpc_data *tdata, int rp_lvl, int pull)
{
	return mt6362_write8(tdata, TCPC_V10_REG_ROLE_CTRL,
			     TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull,
			     pull));
}

static int mt6362_enable_force_discharge(struct mt6362_tcpc_data *tdata,
					 bool en)
{
	return (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, TCPC_V10_REG_POWER_CTRL, TCPC_V10_REG_FORCE_DISC_EN);
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int mt6362_enable_vsafe0v_detect(struct mt6362_tcpc_data *tdata, bool en)
{
	return (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, MT6362_REG_MTMASK1, MT6362_MSK_VBUS80);
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static int __maybe_unused mt6362_enable_rpdet_auto(
					struct mt6362_tcpc_data *tdata, bool en)
{
	return (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, MT6362_REG_SHIELDCTRL1, MT6362_MSK_RPDET_AUTO);
}

static int __maybe_unused mt6362_enable_rpdet_manual(
					struct mt6362_tcpc_data *tdata, bool en)
{
	return (en ? mt6362_clr_bits : mt6362_set_bits)
		(tdata, MT6362_REG_SHIELDCTRL1, MT6362_MSK_RPDET_MANUAL);
}

static int mt6362_is_vconn_fault(struct mt6362_tcpc_data *tdata, bool *fault)
{
	int ret;
	u8 status;

	ret = mt6362_read8(tdata, MT6362_REG_MTST2, &status);
	if (ret < 0)
		return ret;
	*fault = (status & MT6362_MSK_VCON_FAULT) ? true : false;
	return 0;
}

static int mt6362_vend_alert_status_clear(struct mt6362_tcpc_data *tdata,
					  const u8 *mask)
{
	mt6362_bulk_write(tdata, MT6362_REG_MTINT1, mask, MT6362_VEND_INT_NUM);
	return mt6362_write16(tdata, TCPC_V10_REG_ALERT,
			      TCPC_V10_REG_ALERT_VENDOR_DEFINED);
}

#ifdef CONFIG_CABLE_TYPE_DETECTION
static int mt6362_get_cable_type(struct mt6362_tcpc_data *tdata,
				 enum tcpc_cable_type *type)
{
	int ret;
	u8 data;

	ret = mt6362_read8(tdata, MT6362_REG_MTST3, &data);
	if (ret < 0)
		return ret;
	if (data & MT6362_MSK_CABLE_TYPEC)
		*type = TCPC_CABLE_TYPE_C2C;
	else if (data & MT6362_MSK_CABLE_TYPEA)
		*type = TCPC_CABLE_TYPE_A2C;
	else
		*type = TCPC_CABLE_TYPE_NONE;
	return 0;
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

static int mt6362_init_fod_ctd(struct mt6362_tcpc_data *tdata)
{
	int ret = 0;
#ifdef CONFIG_CABLE_TYPE_DETECTION
	u8 ctd_evt;

	tdata->tcpc->typec_cable_type = TCPC_CABLE_TYPE_NONE;
	tdata->handle_init_ctd = true;
	ret = mt6362_read8(tdata, MT6362_REG_MTINT3, &ctd_evt);
	if (ret < 0)
		return ret;
	if (ctd_evt & MT6362_MSK_CTD)
		mt6362_get_cable_type(tdata, &tdata->init_cable_type);
#endif /* CONFIG_CABLE_TYPE_DETECTION */
	return ret;
}

#ifdef CONFIG_WATER_DETECTION
static int mt6362_set_wd_ldo(struct mt6362_tcpc_data *tdata,
			     enum mt6362_wd_ldo ldo)
{
	return mt6362_update_bits(tdata, MT6362_REG_WDSET, MT6362_MSK_WDLDO_SEL,
				  ldo << MT6362_SFT_WDLDO_SEL);
}
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_WATER_DETECTION
static int mt6362_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2);
static int mt6362_is_cc_toggling(struct mt6362_tcpc_data *tdata, bool *toggling)
{
	int ret;
	int cc1 = 0, cc2 = 0;

	ret = mt6362_get_cc(tdata->tcpc, &cc1, &cc2);
	if (ret < 0)
		return ret;
	*toggling = (cc1 == TYPEC_CC_DRP_TOGGLING &&
		     cc2 == TYPEC_CC_DRP_TOGGLING);
	return 0;
}

static int mt6362_init_wd(struct mt6362_tcpc_data *tdata)
{
	/*
	 * WD_LDO = 1.8V
	 * WD_CHPHYS_EN = 0
	 * WD_SWITCH_CNT = 100
	 * WD_POLL_SWITCH = 0
	 * WD12_TDET_ALWAYS = 0, depend on WD_TDET[2:0]
	 * WD0_TDET_ALWAYS = 0, depend on WD_TDET[2:0]
	 */
	mt6362_write8(tdata, MT6362_REG_WDSET, 0x50);

	/* WD_EXIT_CNT = 4times */
	mt6362_set_bits(tdata, MT6362_REG_WDSET1, 0x02);

	/* WD1_RPULL_EN = 1, WD1_DISCHG_EN = 1 */
	mt6362_write8(tdata, MT6362_REG_WD1MISCCTRL, 0x06);

	/* WD2_RPULL_EN = 1, WD2_DISCHG_EN = 1 */
	mt6362_write8(tdata, MT6362_REG_WD2MISCCTRL, 0x06);

	/* WD0_RPULL_EN = 1, WD0_DISCHG_EN = 1 */
	mt6362_write8(tdata, MT6362_REG_WD0SET, 0x06);

	mt6362_bulk_write_rt2(tdata, MT6362_REG_WDSET2,
			      mt6362_rt2_wd_init_setting,
			      ARRAY_SIZE(mt6362_rt2_wd_init_setting));
	return 0;
}

static int mt6362_set_wd_rpull(struct mt6362_tcpc_data *tdata,
			       enum mt6362_wd_chan chan,
			       enum mt6362_wd_rpull rpull)
{
	return mt6362_update_bits_rt2(tdata, mt6362_wd_rpull_reg[chan],
				      MT6362_MSK_WDRPULL_SEL,
				      rpull << MT6362_SFT_WDRPULL_SEL);
}

static int  __maybe_unused mt6362_set_wd_ipull(struct mt6362_tcpc_data *tdata,
					       enum mt6362_wd_chan chan,
					       enum mt6362_wd_ipull ipull)
{
	return mt6362_update_bits_rt2(tdata, mt6362_wd_ipull_reg[chan],
				      MT6362_MSK_WDIPULL_SEL,
				      ipull << MT6362_SFT_WDIPULL_SEL);
}

static int mt6362_set_wd_path(struct mt6362_tcpc_data *tdata,
			      enum mt6362_wd_chan chan, u8 path)
{
	return mt6362_write8(tdata, mt6362_wd_path_reg[chan], path);
}

static int mt6362_get_wd_path(struct mt6362_tcpc_data *tdata,
			      enum mt6362_wd_chan chan, u8 *path)
{
	return mt6362_read8(tdata, mt6362_wd_path_reg[chan], path);
}

static int mt6362_set_wd_polling_path(struct mt6362_tcpc_data *tdata,
				      enum mt6362_wd_chan chan)
{
	return mt6362_set_wd_path(tdata, chan, mt6362_wd_polling_path[chan]);
}

static int mt6362_set_wd_protection_path(struct mt6362_tcpc_data *tdata,
					 enum mt6362_wd_chan chan)
{
	return mt6362_set_wd_path(tdata, chan, mt6362_wd_protection_path[chan]);
}

static int mt6362_set_wd_polling_parameter(struct mt6362_tcpc_data *tdata,
					   enum mt6362_wd_chan chan)
{
	int ret;

	ret = mt6362_set_wd_rpull(tdata, chan, MT6362_WD_RPULL_75K);
	if (ret < 0)
		return ret;
	ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
			    MT6362_MSK_WDRPULL_EN | MT6362_MSK_WDDISCHG_EN);
	if (ret < 0)
		return ret;
	ret = mt6362_update_bits_rt2(tdata,
				     mt6362_wd_volcmp_reg[chan],
				     MT6362_MSK_WD12_VOLCOML,
				     MT6362_WD_VOL_CMPL_1_44V <<
						MT6362_SFT_WD12_VOLCOML);
	if (ret < 0)
		return ret;
	return mt6362_set_wd_polling_path(tdata, chan);
}

static int mt6362_set_wd_protection_parameter(struct mt6362_tcpc_data *tdata,
					      enum mt6362_wd_chan chan)
{
	int ret;

	ret = mt6362_set_wd_rpull(tdata, chan, MT6362_WD_RPULL_75K);
	if (ret < 0)
		return ret;
	ret = mt6362_update_bits_rt2(tdata,
				     mt6362_wd_volcmp_reg[chan],
				     MT6362_MSK_WD12_VOLCOML,
				     MT6362_WD_VOL_CMPL_1_54V <<
						MT6362_SFT_WD12_VOLCOML);
	if (ret < 0)
		return ret;
	ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
			    MT6362_MSK_WDRPULL_EN | MT6362_MSK_WDDISCHG_EN);
	if (ret < 0)
		return ret;
	return mt6362_set_wd_protection_path(tdata, chan);
}

static int mt6362_check_wd_status(struct mt6362_tcpc_data *tdata,
				  enum mt6362_wd_chan chan, bool *error)
{
	int i, ret;
	u8 path;
	u8 data[MT6362_WD_STATUS_NUM];

	ret = mt6362_bulk_read(tdata, mt6362_wd_status_reg[chan], data,
			       MT6362_WD_STATUS_NUM);
	if (ret < 0)
		return ret;
	ret = mt6362_get_wd_path(tdata, chan, &path);
	if (ret < 0)
		return ret;
	*error = false;
	for (i = 0; i < MT6362_WD_STATUS_NUM; i++) {
		if (path & data[i])
			*error = true;
		MT6362_DBGINFO("chan(path,stat)=%d(0x%02X,0x%02X)\n", chan,
			       path, data[i]);
	}
	return 0;
}

static int mt6362_enable_wd_dischg(struct mt6362_tcpc_data *tdata,
				   enum mt6362_wd_chan chan, bool en)
{
	int ret;

	if (en) {
		ret = mt6362_set_wd_polling_path(tdata, chan);
		if (ret < 0)
			return ret;
		ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
				    MT6362_MSK_WDDISCHG_EN |
				    MT6362_MSK_WDFWMODE_EN);
	} else {
		ret = mt6362_set_wd_path(tdata, chan, 0);
		if (ret < 0)
			return ret;
		ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
				    MT6362_MSK_WDRPULL_EN |
				    MT6362_MSK_WDDISCHG_EN);
	}
	return ret;
}

static int mt6362_enable_wd_pullup(struct mt6362_tcpc_data *tdata,
				   enum mt6362_wd_chan chan,
				   enum mt6362_wd_rpull rpull, bool en)
{
	int ret;

	if (en) {
		ret = mt6362_set_wd_polling_path(tdata, chan);
		if (ret < 0)
			return ret;
		ret = mt6362_set_wd_rpull(tdata, chan, rpull);
		if (ret < 0)
			return ret;
		ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
				    MT6362_MSK_WDRPULL_EN |
				    MT6362_MSK_WDFWMODE_EN);
	} else {
		ret = mt6362_set_wd_path(tdata, chan, 0);
		if (ret < 0)
			return ret;
		ret = mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
				    MT6362_MSK_WDRPULL_EN |
				    MT6362_MSK_WDDISCHG_EN);
	}
	return ret;
}

static int mt6362_get_wd_adc(struct mt6362_tcpc_data *tdata,
			     enum mt6362_wd_chan chan, int *val)
{
	int ret;

	ret = iio_read_channel_processed(&tdata->adc_iio[chan], val);
	if (ret < 0)
		return ret;
	*val /= 1000;
	return 0;
}

static bool mt6362_is_wd_audio_device(struct mt6362_tcpc_data *tdata,
				      enum mt6362_wd_chan chan, int wd_adc)
{
	struct tcpc_desc *desc = tdata->desc;
	int ret;

	if (wd_adc >= desc->wd_sbu_ph_auddev)
		return false;

	/* Pull high with 1K resistor */
	ret = mt6362_enable_wd_pullup(tdata, chan, MT6362_WD_RPULL_1K, true);
	if (ret < 0) {
		MT6362_DBGINFO("chan%d pull up 1k fail(%d)\n", chan, ret);
		goto not_auddev;
	}

	ret = mt6362_get_wd_adc(tdata, chan, &wd_adc);
	if (ret < 0) {
		MT6362_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
		goto not_auddev;
	}

	if (wd_adc >= desc->wd_sbu_aud_ubound)
		goto not_auddev;
	return true;

not_auddev:
	mt6362_enable_wd_pullup(tdata, chan, MT6362_WD_RPULL_500K, true);
	return false;
}

static int __mt6362_is_water_detected(struct mt6362_tcpc_data *tdata,
				      enum mt6362_wd_chan chan, bool *wd)
{
	int ret, wd_adc, i;
	struct tcpc_desc *desc = tdata->desc;
	u32 lb = desc->wd_sbu_ph_lbound;
	u32 ub = desc->wd_sbu_calib_init * 110 / 100;
#ifdef CONFIG_CABLE_TYPE_DETECTION
	enum tcpc_cable_type cable_type;
	u8 ctd_evt;
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	pm_stay_awake(tdata->dev);
	/* Check WD1/2 pulled low */
	for (i = 0; i < CONFIG_WD_SBU_PL_RETRY; i++) {
		ret = mt6362_enable_wd_dischg(tdata, chan, true);
		if (ret < 0) {
			MT6362_DBGINFO("en chan%d dischg fail(%d)\n", chan,
				       ret);
			goto out;
		}
		ret = mt6362_get_wd_adc(tdata, chan, &wd_adc);
		if (ret < 0) {
			MT6362_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
			goto out;
		}
		MT6362_DBGINFO("chan%d pull low %dmV\n", chan, wd_adc);
		ret = mt6362_enable_wd_dischg(tdata, chan, false);
		if (ret < 0) {
			MT6362_DBGINFO("disable chan%d dischg fail(%d)\n", chan,
				       ret);
			goto out;
		}
		if (wd_adc <= desc->wd_sbu_pl_bound ||
			(wd_adc >= desc->wd_sbu_pl_lbound_c2c &&
			wd_adc <= desc->wd_sbu_pl_ubound_c2c))
			break;
	}
	if (i == CONFIG_WD_SBU_PL_RETRY) {
		*wd = true;
		goto out;
	}

	ret = mt6362_enable_wd_pullup(tdata, chan, MT6362_WD_RPULL_500K, true);
	if (ret < 0) {
		MT6362_DBGINFO("chan%d pull up 500k fail(%d)\n", chan, ret);
		goto out;
	}

	for (i = 0; i < CONFIG_WD_SBU_PH_RETRY; i++) {
		ret = mt6362_get_wd_adc(tdata, chan, &wd_adc);
		if (ret < 0) {
			MT6362_DBGINFO("get chan%d adc fail(%d)\n", chan, ret);
			goto out;
		}
		MT6362_DBGINFO("chan%d pull high %dmV(lb %d, ub %d)\n", chan,
			       wd_adc, lb, ub);
		if (wd_adc >= lb && wd_adc <= ub) {
			*wd = false;
			goto out;
		}
		msleep(20);
	}

#ifdef CONFIG_CABLE_TYPE_DETECTION
	cable_type = tdata->tcpc->typec_cable_type;
	if (cable_type == TCPC_CABLE_TYPE_NONE) {
		ret = mt6362_read8(tdata, MT6362_REG_MTINT3, &ctd_evt);
		if (ret >= 0 && (ctd_evt & MT6362_MSK_CTD))
			ret = mt6362_get_cable_type(tdata, &cable_type);
	}
	if (cable_type == TCPC_CABLE_TYPE_C2C) {
		if (((wd_adc >= desc->wd_sbu_ph_lbound1_c2c) &&
		    (wd_adc <= desc->wd_sbu_ph_ubound1_c2c)) ||
		    (wd_adc > desc->wd_sbu_ph_ubound2_c2c)) {
			MT6362_DBGINFO("ignore water for C2C\n");
			*wd = false;
			goto out;
		}
	}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

	if (mt6362_is_wd_audio_device(tdata, chan, wd_adc)) {
		MT6362_DBGINFO("suspect audio device but not water\n");
		*wd = false;
		goto out;
	}
	*wd = true;
out:
	MT6362_DBGINFO("water %s\n", *wd ? "detected" : "not detected");
	mt6362_write8(tdata, mt6362_wd_miscctrl_reg[chan],
		      MT6362_MSK_WDRPULL_EN | MT6362_MSK_WDDISCHG_EN);
	pm_relax(tdata->dev);
	return ret;
}

static int mt6362_enable_wd_polling(struct mt6362_tcpc_data *tdata, bool en)
{
	int ret, i;

	if (en) {
		ret = mt6362_set_wd_ldo(tdata, MT6362_WD_LDO_1_8V);
		if (ret < 0)
			return ret;
		/* set wd detect time interval base 10ms */
		ret = mt6362_update_bits_rt2(tdata,
					     MT6362_REG_WDSET3,
					     MT6362_MASK_WD_TDET,
					     MT6362_WD_TDET_10MS <<
						MT6362_SHFT_WD_TDET);
		if (ret < 0)
			return ret;
		for (i = 0; i < MT6362_WD_CHAN_NUM; i++) {
			if (!mt6362_wd_chan_en[i])
				continue;
			ret = mt6362_set_wd_polling_parameter(tdata, i);
			if (ret < 0)
				return ret;
		}
	}
	return mt6362_write8(tdata, MT6362_REG_WD12MODECTRL,
			     en ? MT6362_MSK_WD12MODE_EN : 0);
}

static int mt6362_enable_wd_protection(struct mt6362_tcpc_data *tdata, bool en)
{
	int i, ret;

	MT6362_DBGINFO("%s: en = %d\n", __func__, en);
	if (en) {
		/* set wd detect time interval base 1ms */
		ret = mt6362_update_bits_rt2(tdata,
					     MT6362_REG_WDSET3,
					     MT6362_MASK_WD_TDET,
					     MT6362_WD_TDET_1MS <<
						MT6362_SHFT_WD_TDET);
		if (ret < 0)
			return ret;
		for (i = 0; i < MT6362_WD_CHAN_NUM; i++) {
			if (!mt6362_wd_chan_en[i])
				continue;
			mt6362_set_wd_protection_parameter(tdata, i);
		}
	}
	/* set DPDM manual mode and DPDM_DET_EN = 1 */
	ret = regmap_update_bits(tdata->regmap, MT6362_REG_DPDM_CTRL1,
		MT6362_MSK_MANUAL_MODE | MT6362_MSK_DPDM_DET_EN, en ? 0xff : 0);
	if (ret < 0)
		return ret;
	return mt6362_write8(tdata, MT6362_REG_WD12MODECTRL,
			     en ?
			     MT6362_MSK_WD12MODE_EN | MT6362_MSK_WD12PROT : 0);
}

static int mt6362_wd_polling_evt_process(struct mt6362_tcpc_data *tdata)
{
	int i, ret;
	bool toggling, polling = true, error = false;

	/* Only handle this event if CCs are still toggling */
	ret = mt6362_is_cc_toggling(tdata, &toggling);
	if (ret < 0)
		return ret;
	if (!toggling)
		return 0;

	mt6362_enable_wd_polling(tdata, false);
	for (i = 0; i < MT6362_WD_CHAN_NUM; i++) {
		if (!mt6362_wd_chan_en[i])
			continue;
		ret = mt6362_check_wd_status(tdata, i, &error);
		if (ret < 0 || !error)
			continue;
		ret = __mt6362_is_water_detected(tdata, i, &error);
		if (ret < 0 || !error)
			continue;
		polling = false;
		break;
	}
	if (polling)
		mt6362_enable_wd_polling(tdata, true);
	else
		tcpc_typec_handle_wd(tdata->tcpc, true);
	return 0;
}

static int mt6362_wd_protection_evt_process(struct mt6362_tcpc_data *tdata)
{
	int i, ret;
	bool error[2] = {false, false}, protection = false;

	for (i = 0; i < MT6362_WD_CHAN_NUM; i++) {
		if (!mt6362_wd_chan_en[i])
			continue;
		ret = mt6362_check_wd_status(tdata, i, &error[0]);
		if (ret < 0)
			goto out;
		ret = __mt6362_is_water_detected(tdata, i, &error[1]);
		if (ret < 0)
			goto out;
		MT6362_DBGINFO("%s: err1:%d, err2:%d\n",
			       __func__, error[0], error[1]);
		if (!error[0] && !error[1])
			continue;
out:
		protection = true;
		break;
	}
	MT6362_DBGINFO("%s: retry cnt = %d\n", __func__, tdata->wd_protect_rty);
	if (!protection && atomic_dec_and_test(&tdata->wd_protect_rty)) {
		tcpc_typec_handle_wd(tdata->tcpc, false);
		atomic_set(&tdata->wd_protect_rty,
			   CONFIG_WD_PROTECT_RETRY_COUNT);
	} else
		mt6362_enable_wd_protection(tdata, true);
	return 0;
}

#ifdef CONFIG_WD_POLLING_ONLY
static void mt6362_wd_poll_dwork_handler(struct work_struct *work)
{
	int ret;
	bool toggling;
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6362_tcpc_data *tdata = container_of(dwork,
						     struct mt6362_tcpc_data,
						     wd_poll_dwork);

	ret = mt6362_is_cc_toggling(tdata, &toggling);
	if (ret < 0)
		return;
	if (!toggling)
		return;
	mt6362_enable_wd_polling(tdata, true);
}
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WATER_DETECTION */

static int mt6362_set_cc_toggling(struct mt6362_tcpc_data *tdata, int pull)
{
	int ret, rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);
	u8 data = TCPC_V10_REG_ROLE_CTRL_RES_SET(1, rp_lvl, TYPEC_CC_RD,
						 TYPEC_CC_RD);

	ret = mt6362_write8(tdata, TCPC_V10_REG_ROLE_CTRL, data);
	if (ret < 0)
		return ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = mt6362_enable_vsafe0v_detect(tdata, false);
	if (ret < 0)
		return ret;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	/* Set LDO to 2V */
	ret = mt6362_write8(tdata, MT6362_REG_LPWRCTRL3, 0xD9);
	if (ret < 0)
		return ret;
#ifdef CONFIG_TCPC_LOW_POWER_MODE
	tcpci_set_low_power_mode(tdata->tcpc, true, pull);
#endif /* CONFIG_TCPC_LOW_POWER_MODE */
	udelay(30);
	ret = mt6362_write8(tdata, TCPC_V10_REG_COMMAND,
			    TCPM_CMD_LOOK_CONNECTION);
	if (ret < 0)
		return ret;
#ifdef CONFIG_WD_SBU_POLLING
#ifdef CONFIG_WD_POLLING_ONLY
	schedule_delayed_work(&tdata->wd_poll_dwork,
			msecs_to_jiffies(500));
#else
	mt6362_enable_wd_polling(tdata, true);
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WD_SBU_POLLING */
	return 0;
}

/*
 * ==================================================================
 * TCPC ops
 * ==================================================================
 */

static int mt6362_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	if (sw_reset) {
		ret = mt6362_sw_reset(tdata);
		if (ret < 0)
			return ret;
	}

	/* Select PD_IRQB from synchronous to 3M path */
	mt6362_set_bits(tdata, MT6362_REG_SYSCTRL1, 0x80);

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	mt6362_write8(tdata, TCPC_V10_REG_ROLE_CTRL,
		      TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	/* tTCPCFilter = 250us */
	mt6362_write8(tdata, MT6362_REG_TCPCCTRL1, 0x0A);

	/*
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC / 1024
	 */
	mt6362_write8(tdata, MT6362_REG_TCPCCTRL2, 4);
	mt6362_write16(tdata, MT6362_REG_TCPCCTRL3, TCPC_NORMAL_RP_DUTY);

	/*
	 * Transition toggle count = 7
	 * OSC_FREQ_CFG = 0x01
	 * RXFilter out 100ns glich = 0x00
	 */
	mt6362_write8(tdata, MT6362_REG_PHYCTRL1, 0x74);

	/* PHY_CDR threshold = 0x3A */
	mt6362_write8(tdata, MT6362_REG_PHYCTRL2, 0x3A);

	/* Transition window time = 43.29us */
	mt6362_write8(tdata, MT6362_REG_PHYCTRL3, 0x82);

	/* BMC decoder idle time = 17.982us */
	mt6362_write8(tdata, MT6362_REG_PHYCTRL7, 0x36);

	/* Retry period = 24.96us */
	mt6362_write8(tdata, MT6362_REG_PHYCTRL9, 0x3C);

	/* Enable PD Vconn current limit mode */
	mt6362_write8(tdata, MT6362_REG_VCONCTRL3, 0x41);

	/* Set HILOCCFILTER 250us */
	mt6362_write8(tdata, MT6362_REG_HILOCTRL9, 0x0A);

	/* Enable CC open 40ms when PMIC SYSUV */
	mt6362_set_bits(tdata, MT6362_REG_SHIELDCTRL1, MT6362_MSK_OPEN40MS_EN);

	/*
	 * Enable Alert.CCStatus assertion
	 * when CCStatus.Looking4Connection changes
	 */
	mt6362_set_bits(tdata, TCPC_V10_REG_TCPC_CTRL,
			TCPC_V10_REG_TCPC_CTRL_EN_LOOK4CONNECTION_ALERT);

#ifdef CONFIG_WATER_DETECTION
	mt6362_init_wd(tdata);
#endif /* CONFIG_WATER_DETECTION */

	tcpci_init_alert_mask(tcpc);

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN) {
		/* Set watchdog timer = 3.2s and enable */
		mt6362_write8(tdata, MT6362_REG_WATCHDOGCTRL, 0x07);
		tcpci_set_watchdog(tcpc, true);
	}

	/* enable ctd_en */
	ret = mt6362_set_bits(tdata, MT6362_REG_SHIELDCTRL1, MT6362_MSK_CTD_EN);

	/* SHIPPING off, AUTOIDLE on */
	mt6362_set_bits(tdata, MT6362_REG_SYSCTRL1,
			MT6362_MSK_SHIPPING_OFF | MT6362_MSK_AUTOIDLE_EN);
	return 0;
}

static int mt6362_init_mask(struct tcpc_device *tcpc)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	mt6362_init_alert_mask(tdata);
	mt6362_init_power_status_mask(tdata);
	mt6362_init_fault_mask(tdata);
	mt6362_init_ext_mask(tdata);
	mt6362_init_vend_mask(tdata);

#ifdef CONFIG_CABLE_TYPE_DETECTION
	/* Init cable type must be done after fod */
	if (tdata->handle_init_ctd) {
		/*
		 * wait 3ms for exit low power mode and
		 * TCPC filter debounce
		 */
		mdelay(3);
		tdata->handle_init_ctd = false;
		tcpc_typec_handle_ctd(tcpc, tdata->init_cable_type);
	}
#endif /* CONFIG_CABLE_TYPE_DETECTION */
	return 0;
}

static int mt6362_alert_status_clear(struct tcpc_device *tcpc, u32 mask)
{
	u16 std_mask = mask & 0xffff;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return std_mask ?
	       mt6362_write16(tdata, TCPC_V10_REG_ALERT, std_mask) : 0;
}

static int mt6362_fault_status_clear(struct tcpc_device *tcpc, u8 status)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	/*
	 * Not sure how to react after discharge fail
	 * follow previous H/W behavior, turn off force discharge
	 */
	if (status & TCPC_V10_REG_FAULT_STATUS_FORCE_DISC_FAIL)
		mt6362_enable_force_discharge(tdata, false);
	return mt6362_write8(tdata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6362_set_alert_mask(struct tcpc_device *tcpc, u32 mask)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	MT6362_DBGINFO("%s: mask = 0x%04x\n", __func__, mask);
	return mt6362_write16(tdata, TCPC_V10_REG_ALERT_MASK, mask);
}

static int mt6362_get_alert_mask(struct tcpc_device *tcpc, u32 *mask)
{
	int ret;
	u16 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read16(tdata, TCPC_V10_REG_ALERT_MASK, &data);
	if (ret < 0)
		return ret;
	*mask = data;
	return 0;
}

static int mt6362_get_alert_status(struct tcpc_device *tcpc, u32 *alert)
{
	int ret;
	u16 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read16(tdata, TCPC_V10_REG_ALERT, &data);
	if (ret < 0)
		return ret;
	*alert = data;
	return 0;
}

static int mt6362_get_power_status(struct tcpc_device *tcpc, u16 *status)
{
	int ret;
	u8 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read8(tdata, TCPC_V10_REG_POWER_STATUS, &data);
	if (ret < 0)
		return ret;

	*status = 0;
	if (data & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

	/*
	 * Vsafe0v only triggers when vbus falls under 0.8V,
	 * also update parameter if vbus present triggers
	 */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = tcpci_is_vsafe0v(tcpc);
	if (ret < 0)
		goto out;
	tcpc->vbus_safe0v = ret ? true : false;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
out:
	return 0;
}

static int mt6362_get_fault_status(struct tcpc_device *tcpc, u8 *status)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return mt6362_read8(tdata, TCPC_V10_REG_FAULT_STATUS, status);
}

static int mt6362_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int ret;
	bool act_as_sink, act_as_drp;
	u8 status, role_ctrl, cc_role;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read8(tdata, TCPC_V10_REG_CC_STATUS, &status);
	if (ret < 0)
		return ret;

	ret = mt6362_read8(tdata, TCPC_V10_REG_ROLE_CTRL, &role_ctrl);
	if (ret < 0)
		return ret;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp)
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	else {
		cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		act_as_sink = (cc_role == TYPEC_CC_RP) ? false : true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */
	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);
	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);
	return 0;
}

static int mt6362_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	MT6362_INFO("%s %d\n", __func__, pull);
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		ret = mt6362_set_cc_toggling(tdata, pull);
	} else {
#ifdef CONFIG_WD_POLLING_ONLY
		cancel_delayed_work_sync(&tdata->wd_poll_dwork);
		mt6362_enable_wd_polling(tdata, false);
#endif /* CONFIG_WD_POLLING_ONLY */
		ret = __mt6362_set_cc(tdata, rp_lvl, pull);
	}
	return ret;
}

static int mt6362_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return (polarity ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT);
}

static int mt6362_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);
	u16 duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return mt6362_write16(tdata, MT6362_REG_TCPCCTRL2, duty);
}

static int mt6362_set_vconn(struct tcpc_device *tcpc, int en)
{
	int ret;
	bool fault = false;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	/*
	 * Set Vconn OVP RVP
	 * Otherwise vconn present fail will be triggered
	 */
	if (en) {
		mt6362_set_bits(tdata, MT6362_REG_VCONCTRL2,
				MT6362_MSK_VCON_PROTEN);
		usleep_range(20, 50);
		ret = mt6362_is_vconn_fault(tdata, &fault);
		if (ret >= 0 && fault)
			return -EINVAL;
	}
	ret = (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, TCPC_V10_REG_POWER_CTRL, TCPC_V10_REG_POWER_CTRL_VCONN);
	if (!en)
		mt6362_clr_bits(tdata, MT6362_REG_VCONCTRL2,
				MT6362_MSK_VCON_PROTEN);
	return ret;
}

static int mt6362_tcpc_deinit(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	mt6362_set_cc(tcpc, TYPEC_CC_DRP);
	mt6362_set_cc(tcpc, TYPEC_CC_OPEN);
#else
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	mt6362_write8(tdata, MT6362_REG_SYSCTRL3, 0x01);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
	return 0;
}

static int mt6362_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, TCPC_V10_REG_TCPC_CTRL, TCPC_V10_REG_TCPC_CTRL_EN_WDT);
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int mt6362_is_vsafe0v(struct tcpc_device *tcpc)
{
	int ret;
	u8 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read8(tdata, MT6362_REG_MTST1, &data);
	if (ret < 0)
		return ret;
	return (data & MT6362_MSK_VBUS80) ? 1 : 0;
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int mt6362_is_low_power_mode(struct tcpc_device *tcpc)
{
	int ret;
	u8 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_read8(tdata, MT6362_REG_SYSCTRL2, &data);
	if (ret < 0)
		return ret;
	return (data & MT6362_MSK_LPWR_EN) != 0;
}

static int mt6362_set_low_power_mode(struct tcpc_device *tcpc, bool en,
				     int pull)
{
	u8 data;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	if (en) {
		data = MT6362_MSK_LPWR_EN;
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= MT6362_MSK_VBUSDET_EN;
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else {
		data = MT6362_MSK_VBUSDET_EN | MT6362_MSK_BMCIOOSC_EN;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
		mt6362_enable_vsafe0v_detect(tdata, true);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	}
	return mt6362_write8(tdata, MT6362_REG_SYSCTRL2, data);
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_USB_POWER_DELIVERY
static int mt6362_set_msg_header(struct tcpc_device *tcpc, u8 power_role,
				 u8 data_role)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);
	u8 msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(data_role, power_role);

	return mt6362_write8(tdata, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int mt6362_protocol_reset(struct tcpc_device *tcpc)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	mt6362_clr_bits(tdata, MT6362_REG_PHYCTRL8, MT6362_MSK_PRLRSTB);
	mdelay(1);
	mt6362_set_bits(tdata, MT6362_REG_PHYCTRL8, MT6362_MSK_PRLRSTB);
	return 0;
}

static int mt6362_set_rx_enable(struct tcpc_device *tcpc, u8 en)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return mt6362_write8(tdata, TCPC_V10_REG_RX_DETECT, en);
}

static int mt6362_get_message(struct tcpc_device *tcpc, u32 *payload,
			      u16 *msg_head,
			      enum tcpm_transmit_type *frame_type)
{
	int ret;
	u8 type, cnt = 0;
	u8 buf[4] = {0};
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_bulk_read(tdata, TCPC_V10_REG_RX_BYTE_CNT, buf, 4);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(u16 *)&buf[2];

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (ret >= 0 && cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		ret = mt6362_bulk_read(tdata, TCPC_V10_REG_RX_DATA,
				       (u8 *)payload, cnt);
	}
	*frame_type = (enum tcpm_transmit_type)type;

	/* Read complete, clear RX status alert bit */
	tcpci_alert_status_clear(tcpc, TCPC_V10_REG_ALERT_RX_STATUS |
				 TCPC_V10_REG_RX_OVERFLOW);
	return ret;
}

/* message header (2byte) + data object (7*4) */
#define MT6362_TRANSMIT_MAX_SIZE	(sizeof(u16) + sizeof(u32) * 7)

static int mt6362_transmit(struct tcpc_device *tcpc,
			   enum tcpm_transmit_type type, u16 header,
			   const u32 *data)
{
	int ret, data_cnt, packet_cnt;
	u8 temp[MT6362_TRANSMIT_MAX_SIZE];
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(u32) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(u16);

		temp[0] = packet_cnt;
		memcpy(temp + 1, (u8 *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp + 3, (u8 *)data, data_cnt);

		ret = mt6362_bulk_write(tdata, TCPC_V10_REG_TX_BYTE_CNT,
					(u8 *)temp, packet_cnt + 1);
		if (ret < 0)
			return ret;
	}

	return mt6362_write8(tdata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
			     type));
}

static int mt6362_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return (en ? mt6362_set_bits : mt6362_clr_bits)
		(tdata, TCPC_V10_REG_TCPC_CTRL,
		 TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE);
}

static int mt6362_set_bist_carrier_mode(struct tcpc_device *tcpc, u8 pattern)
{
	/* Not support this function */
	return 0;
}
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6362_retransmit(struct tcpc_device *tcpc)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return mt6362_write8(tdata, TCPC_V10_REG_TRANSMIT,
			     TCPC_V10_REG_TRANSMIT_SET(tcpc->pd_retry_count,
			     TCPC_TX_SOP));
}
#endif /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_WATER_DETECTION
static int mt6362_is_water_detected(struct tcpc_device *tcpc)
{
	int ret, i;
	bool error, wd = false;
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_set_wd_ldo(tdata, MT6362_WD_LDO_1_8V);
	if (ret < 0)
		return ret;
	for (i = 0; i < MT6362_WD_CHAN_NUM; i++) {
		if (!mt6362_wd_chan_en[i])
			continue;
		ret = __mt6362_is_water_detected(tdata, i, &error);
		if (ret < 0 || !error)
			continue;
		wd = true;
		break;
	}
	return wd ? 1 : 0;
}

static int mt6362_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	return mt6362_enable_wd_protection(tdata, en);
}

static int mt6362_set_wd_polling(struct tcpc_device *tcpc, bool en)
{
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

#ifdef CONFIG_WD_POLLING_ONLY
	if (!en)
		cancel_delayed_work_sync(&tdata->wd_poll_dwork);
#endif /* CONFIG_WD_POLLING_ONLY */
	return mt6362_enable_wd_polling(tdata, en);
}
#endif /* CONFIG_WATER_DETECTION */

/*
 * ==================================================================
 * TCPC vendor irq handlers
 * ==================================================================
 */

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
static int mt6362_vsafe0v_irq_handler(struct mt6362_tcpc_data *tdata)
{
	int ret;

	ret = tcpci_is_vsafe0v(tdata->tcpc);
	if (ret < 0)
		return ret;
	tdata->tcpc->vbus_safe0v = ret ? true : false;
	return 0;
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
static int mt6362_wd12_strise_irq_handler(struct mt6362_tcpc_data *tdata)
{
	/* Pull or discharge status from 0 to 1 in normal polling mode */
	return mt6362_wd_polling_evt_process(tdata);
}

static int mt6362_wd12_done_irq_handler(struct mt6362_tcpc_data *tdata)
{
	/* Oneshot or protect mode done */
	MT6362_DBGINFO("%s\n", __func__);
	return mt6362_wd_protection_evt_process(tdata);
}
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
static int mt6362_ctd_irq_handler(struct mt6362_tcpc_data *tdata)
{
	int ret;
	enum tcpc_cable_type cable_type;

	ret = mt6362_get_cable_type(tdata, &cable_type);
	if (ret < 0)
		return ret;

	tcpc_typec_handle_ctd(tdata->tcpc, cable_type);
	return 0;
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */


struct irq_mapping_tbl {
	u8 num;
	const char *name;
	int (*hdlr)(struct mt6362_tcpc_data *tdata);
};

#define MT6362_IRQ_MAPPING(_num, _name) \
	{ .num = _num, .name = #_name, .hdlr = mt6362_##_name##_irq_handler }

static struct irq_mapping_tbl mt6362_vend_irq_mapping_tbl[] = {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	MT6362_IRQ_MAPPING(1, vsafe0v),
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
	MT6362_IRQ_MAPPING(49, wd12_strise),
	MT6362_IRQ_MAPPING(50, wd12_done),
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
	MT6362_IRQ_MAPPING(20, ctd),
#endif /* CONFIG_CABLE_TYPE_DETECTION */
};

static int mt6362_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	int ret, i, irqnum, irqbit;
	u8 alert[MT6362_VEND_INT_NUM];
	u8 mask[MT6362_VEND_INT_NUM];
	struct mt6362_tcpc_data *tdata = tcpc_get_dev_data(tcpc);

	ret = mt6362_bulk_read(tdata, MT6362_REG_MTINT1, alert,
			       MT6362_VEND_INT_NUM);
	if (ret < 0)
		return ret;
	ret = mt6362_bulk_read(tdata, MT6362_REG_MTMASK1, mask,
			       MT6362_VEND_INT_NUM);
	if (ret < 0)
		return ret;

	for (i = 0; i < MT6362_VEND_INT_NUM; i++) {
		if (!alert[i])
			continue;
		MT6362_DBGINFO("vend_alert[%d]=alert,mask(0x%02X,0x%02X)\n",
			       i + 1, alert[i], mask[i]);
		alert[i] &= mask[i];
	}

	mt6362_vend_alert_status_clear(tdata, alert);

	for (i = 0; i < ARRAY_SIZE(mt6362_vend_irq_mapping_tbl); i++) {
		irqnum = mt6362_vend_irq_mapping_tbl[i].num / 8;
		if (irqnum >= MT6362_VEND_INT_NUM)
			continue;
		alert[irqnum] &= mask[irqnum];
		irqbit = mt6362_vend_irq_mapping_tbl[i].num % 8;
		if (alert[irqnum] & (1 << irqbit))
			mt6362_vend_irq_mapping_tbl[i].hdlr(tdata);
	}
	return 0;
}

static struct tcpc_ops mt6362_tcpc_ops = {
	.init = mt6362_tcpc_init,
	.init_alert_mask = mt6362_init_mask,
	.alert_status_clear = mt6362_alert_status_clear,
	.fault_status_clear = mt6362_fault_status_clear,
	.get_alert_mask = mt6362_get_alert_mask,
	.set_alert_mask = mt6362_set_alert_mask,
	.get_alert_status = mt6362_get_alert_status,
	.get_power_status = mt6362_get_power_status,
	.get_fault_status = mt6362_get_fault_status,
	.get_cc = mt6362_get_cc,
	.set_cc = mt6362_set_cc,
	.set_polarity = mt6362_set_polarity,
	.set_low_rp_duty = mt6362_set_low_rp_duty,
	.set_vconn = mt6362_set_vconn,
	.deinit = mt6362_tcpc_deinit,
	.set_watchdog = mt6362_set_watchdog,
	.alert_vendor_defined_handler = mt6362_alert_vendor_defined_handler,

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	.is_vsafe0v = mt6362_is_vsafe0v,
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = mt6362_is_low_power_mode,
	.set_low_power_mode = mt6362_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = mt6362_set_msg_header,
	.set_rx_enable = mt6362_set_rx_enable,
	.protocol_reset = mt6362_protocol_reset,
	.get_message = mt6362_get_message,
	.transmit = mt6362_transmit,
	.set_bist_test_mode = mt6362_set_bist_test_mode,
	.set_bist_carrier_mode = mt6362_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6362_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_WATER_DETECTION
	.is_water_detected = mt6362_is_water_detected,
	.set_water_protection = mt6362_set_water_protection,
	.set_usbid_polling = mt6362_set_wd_polling,
#endif /* CONFIG_WATER_DETECTION */
};

static void mt6362_cpu_poll_ctrl(struct mt6362_tcpc_data *tdata)
{
	cancel_delayed_work_sync(&tdata->cpu_poll_dwork);

	if (atomic_read(&tdata->cpu_poll_count) == 0) {
		atomic_inc(&tdata->cpu_poll_count);
		cpu_idle_poll_ctrl(true);
	}

	schedule_delayed_work(&tdata->cpu_poll_dwork, msecs_to_jiffies(40));
}

static void mt6362_cpu_poll_dwork_handler(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6362_tcpc_data *tdata = container_of(dwork,
						      struct mt6362_tcpc_data,
						      cpu_poll_dwork);

	if (atomic_dec_and_test(&tdata->cpu_poll_count))
		cpu_idle_poll_ctrl(false);
}

static void mt6362_irq_work_handler(struct kthread_work *work)
{
	struct mt6362_tcpc_data *tdata = container_of(work,
						      struct mt6362_tcpc_data,
						      irq_work);

	MT6362_DBGINFO("++\n");
	mt6362_cpu_poll_ctrl(tdata);
	tcpci_lock_typec(tdata->tcpc);
	tcpci_alert(tdata->tcpc);
	tcpci_unlock_typec(tdata->tcpc);
}

static irqreturn_t mt6362_irq_handler(int irq, void *data)
{
	struct mt6362_tcpc_data *tdata = data;

	MT6362_DBGINFO("++\n");
	pm_wakeup_event(tdata->dev, MT6362_IRQ_WAKE_TIME);
	kthread_queue_work(&tdata->irq_worker, &tdata->irq_work);
	return IRQ_HANDLED;
}

static int mt6362_init_irq(struct mt6362_tcpc_data *tdata,
			   struct platform_device *pdev)
{
	int ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	/* Mask all alerts & clear them */
	mt6362_bulk_write(tdata, MT6362_REG_MTMASK1, mt6362_vend_alert_maskall,
			  ARRAY_SIZE(mt6362_vend_alert_maskall));
	mt6362_bulk_write(tdata, MT6362_REG_MTINT1, mt6362_vend_alert_clearall,
			  ARRAY_SIZE(mt6362_vend_alert_clearall));
	mt6362_write16(tdata, TCPC_V10_REG_ALERT_MASK, 0);
	mt6362_write16(tdata, TCPC_V10_REG_ALERT, 0xFFFF);

	kthread_init_worker(&tdata->irq_worker);
	tdata->irq_worker_task = kthread_run(kthread_worker_fn,
					     &tdata->irq_worker, "%s",
					     tdata->desc->name);
	if (IS_ERR(tdata->irq_worker_task)) {
		dev_err(tdata->dev, "%s create tcpc task fail\n", __func__);
		return -EINVAL;
	}
	sched_setscheduler(tdata->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&tdata->irq_work, mt6362_irq_work_handler);

	tdata->irq = platform_get_irq_byname(pdev, "pd_evt");
	if (tdata->irq <= 0) {
		dev_err(tdata->dev, "%s get irq number fail(%d)\n", __func__,
			tdata->irq);
		return -EINVAL;
	}
	ret = devm_request_threaded_irq(tdata->dev, tdata->irq, NULL,
					mt6362_irq_handler, IRQF_TRIGGER_NONE,
					NULL, tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s fail to request irq%d(%d)\n", __func__,
			tdata->irq, ret);
		return -EINVAL;
	}
	device_init_wakeup(tdata->dev, true);
	return 0;
}

static int mt6362_register_tcpcdev(struct mt6362_tcpc_data *tdata)
{
	tdata->tcpc = tcpc_device_register(tdata->dev, tdata->desc,
					  &mt6362_tcpc_ops, tdata);
	if (IS_ERR(tdata->tcpc))
		return -EINVAL;

	/* Init tcpc_flags */
#ifdef CONFIG_CABLE_TYPE_DETECTION
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_CABLE_TYPE_DETECTION;
#endif /* CONFIG_CABLE_TYPE_DETECTION */
#ifdef CONFIG_WATER_DETECTION
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_WATER_DETECTION;
#endif /* CONFIG_WATER_DETECTION */
#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_LPM_WAKEUP_WATCHDOG;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */
#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#ifdef CONFIG_USB_PD_REV30
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;
#endif	/* CONFIG_USB_PD_REV30 */
	if (tdata->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(tdata->dev, "%s PD REV30\n", __func__);
	else
		dev_info(tdata->dev, "%s PD REV20\n", __func__);
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_DISABLE_LEGACY;
	tdata->tcpc->tcpc_flags |= TCPC_FLAGS_WATCHDOG_EN;
	return 0;
}

static int mt6362_parse_dt(struct mt6362_tcpc_data *tdata)
{
	u32 val;
	struct tcpc_desc *desc;
	struct device_node *np = tdata->dev->of_node;

	desc = devm_kzalloc(tdata->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	/* default setting */
	desc->role_def = TYPEC_ROLE_DRP;
	desc->notifier_supply_num = 0;
	desc->rp_lvl = TYPEC_CC_RP_DFT;
	desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;

	if (of_property_read_u32(np, "tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	}

	if (of_property_read_u32(np, "tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	}

	if (of_property_read_u32(np, "tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case 0: /* RP Default */
			desc->rp_lvl = TYPEC_CC_RP_DFT;
			break;
		case 1: /* RP 1.5V */
			desc->rp_lvl = TYPEC_CC_RP_1_5;
			break;
		case 2: /* RP 3.0V */
			desc->rp_lvl = TYPEC_CC_RP_3_0;
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_WATER_DETECTION
	if (of_property_read_u32(np, "wd,sbu_calib_init", &val) < 0)
		desc->wd_sbu_calib_init = CONFIG_WD_SBU_CALIB_INIT;
	else
		desc->wd_sbu_calib_init = val;
	if (of_property_read_u32(np, "wd,sbu_pl_bound", &val) < 0)
		desc->wd_sbu_pl_bound = CONFIG_WD_SBU_PL_BOUND;
	else
		desc->wd_sbu_pl_bound = val;
	if (of_property_read_u32(np, "wd,sbu_pl_lbound_c2c", &val) < 0)
		desc->wd_sbu_pl_lbound_c2c = CONFIG_WD_SBU_PL_LBOUND_C2C;
	else
		desc->wd_sbu_pl_lbound_c2c = val;
	if (of_property_read_u32(np, "wd,sbu_pl_ubound_c2c", &val) < 0)
		desc->wd_sbu_pl_ubound_c2c = CONFIG_WD_SBU_PL_UBOUND_C2C;
	else
		desc->wd_sbu_pl_ubound_c2c = val;
	if (of_property_read_u32(np, "wd,sbu_ph_auddev", &val) < 0)
		desc->wd_sbu_ph_auddev = CONFIG_WD_SBU_PH_AUDDEV;
	else
		desc->wd_sbu_ph_auddev = val;
	if (of_property_read_u32(np, "wd,sbu_ph_lbound", &val) < 0)
		desc->wd_sbu_ph_lbound = CONFIG_WD_SBU_PH_LBOUND;
	else
		desc->wd_sbu_ph_lbound = val;
	if (of_property_read_u32(np, "wd,sbu_ph_lbound1_c2c", &val) < 0)
		desc->wd_sbu_ph_lbound1_c2c = CONFIG_WD_SBU_PH_LBOUND1_C2C;
	else
		desc->wd_sbu_ph_lbound1_c2c = val;
	if (of_property_read_u32(np, "wd,sbu_ph_ubound1_c2c", &val) < 0)
		desc->wd_sbu_ph_ubound1_c2c = CONFIG_WD_SBU_PH_UBOUND1_C2C;
	else
		desc->wd_sbu_ph_ubound1_c2c = val;
	if (of_property_read_u32(np, "wd,sbu_ph_ubound2_c2c", &val) < 0)
		desc->wd_sbu_ph_ubound2_c2c = CONFIG_WD_SBU_PH_UBOUND2_C2C;
	else
		desc->wd_sbu_ph_ubound2_c2c = val;
	if (of_property_read_u32(np, "wd,sbu_aud_ubound", &val) < 0)
		desc->wd_sbu_aud_ubound = CONFIG_WD_SBU_AUD_UBOUND;
	else
		desc->wd_sbu_aud_ubound = val;
#endif /* CONFIG_WATER_DETECTION */

	of_property_read_string(np, "tcpc,name", (const char **)&desc->name);
	tdata->desc = desc;
	return 0;
}

static int mt6362_check_revision(struct mt6362_tcpc_data *tdata)
{
	int ret;
	u16 id;

	ret = mt6362_read16(tdata, TCPC_V10_REG_VID, &id);
	if (ret < 0) {
		dev_err(tdata->dev, "%s read vid fail(%d)\n", __func__, ret);
		return ret;
	}
	if (id != MT6362_VID) {
		dev_err(tdata->dev, "%s vid incorrect(0x%04X)\n", __func__, id);
		return -ENODEV;
	}

	ret = mt6362_read16(tdata, TCPC_V10_REG_PID, &id);
	if (ret < 0) {
		dev_err(tdata->dev, "%s read pid fail(%d)\n", __func__, ret);
		return ret;
	}
	if (id != MT6362_PID) {
		dev_err(tdata->dev, "%s pid incorrect(0x%04X)\n", __func__, id);
		return -ENODEV;
	}

	ret = mt6362_read16(tdata, TCPC_V10_REG_DID, &id);
	if (ret < 0) {
		dev_err(tdata->dev, "%s read did fail(%d)\n", __func__, ret);
		return ret;
	}
	dev_info(tdata->dev, "%s did = 0x%04X\n", __func__, id);
	tdata->did = id;
	return 0;
}

/*
 * In some platform pr_info may spend too much time on printing debug message.
 * So we use this function to test the printk performance.
 * If your platform cannot not pass this check function, please config
 * PD_DBG_INFO, this will provide the threaded debug message for you.
 */
#if TCPC_ENABLE_ANYMSG
static void check_printk_performance(void)
{
	int i;
	u64 t1, t2;
	u32 nsrem;

#ifdef CONFIG_PD_DBG_INFO
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pd_dbg_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pd_dbg_info("pd_dbg_info : t2-t1 = %lu\n",
			    (unsigned long)nsrem / 1000);
	}
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("pr_info : t2-t1 = %lu\n", (unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("t2-t1 = %lu\n", (unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int mt6362_tcpc_probe(struct platform_device *pdev)
{
	int ret;
	bool use_dt = pdev->dev.of_node;
	struct mt6362_tcpc_data *tdata;

	if (!use_dt) {
		dev_err(&pdev->dev, "%s no dts node\n", __func__);
		return -ENODEV;
	}

	tdata = devm_kzalloc(&pdev->dev, sizeof(*tdata), GFP_KERNEL);
	if (!tdata)
		return -ENOMEM;
	tdata->dev = &pdev->dev;
	tdata->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!tdata->regmap) {
		dev_err(tdata->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}
	platform_set_drvdata(pdev, tdata);

	ret = mt6362_check_revision(tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s check revision fail(%d)\n", __func__,
			ret);
		return ret;
	}
#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */
	INIT_DELAYED_WORK(&tdata->cpu_poll_dwork,
			  mt6362_cpu_poll_dwork_handler);

#ifdef CONFIG_WATER_DETECTION
	atomic_set(&tdata->wd_protect_rty, CONFIG_WD_PROTECT_RETRY_COUNT);
#ifdef CONFIG_WD_POLLING_ONLY
	INIT_DELAYED_WORK(&tdata->wd_poll_dwork, mt6362_wd_poll_dwork_handler);
#endif /* CONFIG_WD_POLLING_ONLY */
#endif /* CONFIG_WATER_DETECTION */

	ret = mt6362_parse_dt(tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s parse dt fail(%d)\n", __func__, ret);
		return ret;
	}

	tdata->adc_iio = devm_iio_channel_get_all(tdata->dev);
	if (IS_ERR(tdata->adc_iio)) {
		ret = PTR_ERR(tdata->adc_iio);
		dev_err(tdata->dev, "%s get adc iio fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = mt6362_register_tcpcdev(tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s register tcpcdev fail(%d)\n", __func__,
			ret);
		return ret;
	}

	/* Must init before sw reset */
	ret = mt6362_init_fod_ctd(tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s init fod ctd fail(%d)\n", __func__,
			ret);
		goto err;
	}

	ret = mt6362_sw_reset(tdata);
	if (ret < 0) {
		dev_err(tdata->dev, "%s sw reset fail(%d)\n", __func__, ret);
		goto err;
	}

	ret = mt6362_init_irq(tdata, pdev);
	if (ret < 0) {
		dev_err(tdata->dev, "%s init alert fail\n", __func__);
		goto err;
	}

	tcpc_schedule_init_work(tdata->tcpc);
	dev_info(tdata->dev, "%s successfully!\n", __func__);
	return 0;
err:
	tcpc_device_unregister(tdata->dev, tdata->tcpc);
	return ret;
}

static int mt6362_tcpc_remove(struct platform_device *pdev)
{
	struct mt6362_tcpc_data *tdata = platform_get_drvdata(pdev);

	if (!tdata)
		return 0;
#ifdef CONFIG_WD_POLLING_ONLY
	cancel_delayed_work_sync(&tdata->wd_poll_dwork);
#endif /* CONFIG_WD_POLLING_ONLY */
	cancel_delayed_work_sync(&tdata->cpu_poll_dwork);
	if (tdata->tcpc)
		tcpc_device_unregister(tdata->dev, tdata->tcpc);
	return 0;
}

static void mt6362_shutdown(struct platform_device *pdev)
{
	struct mt6362_tcpc_data *tdata = platform_get_drvdata(pdev);

	/* Please reset IC here */
	if (!tdata)
		return;
	if (tdata->irq)
		disable_irq(tdata->irq);
	tcpm_shutdown(tdata->tcpc);
}

static const struct of_device_id __maybe_unused mt6362_tcpc_ofid_tbls[] = {
	{ .compatible = "mediatek,mt6362-tcpc", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_tcpc_ofid_tbls);

static struct platform_driver mt6362_tcpc_driver = {
	.driver = {
		.name = "mt6362-tcpc",
		.of_match_table = of_match_ptr(mt6362_tcpc_ofid_tbls),
	},
	.probe = mt6362_tcpc_probe,
	.remove = mt6362_tcpc_remove,
	.shutdown = mt6362_shutdown,
};
module_platform_driver(mt6362_tcpc_driver);

MODULE_AUTHOR("ShuFan Lee<shufan_lee@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI TCPC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

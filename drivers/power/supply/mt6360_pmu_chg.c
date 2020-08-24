// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/regulator/driver.h>
#include <linux/phy/phy.h>
#include <linux/kconfig.h>
#include <linux/reboot.h>
#include <linux/i2c.h>

#include <linux/mfd/mt6360.h>
#include <linux/mfd/mt6360-private.h>
#if IS_ENABLED(CONFIG_MTK_CHARGER)
#include "charger_class.h"
#include "mtk_charger.h"
#endif
#if FIXME /* TODO: implement not yet */
#include <mt-plat/mtk_boot.h>
#endif
#include <tcpm.h>

#include "mt6360_pmu_chg.h"

#define MT6360_PMU_CHG_DRV_VERSION	"1.0.7_MTK"

#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

bool dbg_log_en = true; /* module param to enable/disable debug log */
module_param(dbg_log_en, bool, 0644);
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_dbg(dev, fmt, ##__VA_ARGS__); \
	} while (0)

void __attribute__ ((weak)) Charger_Detect_Init(void)
{
}

void __attribute__ ((weak)) Charger_Detect_Release(void)
{
}

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

enum mt6360_adc_channel {
	MT6360_ADC_VBUSDIV5,
	MT6360_ADC_VSYS,
	MT6360_ADC_VBAT,
	MT6360_ADC_IBUS,
	MT6360_ADC_IBAT,
	MT6360_ADC_NODUMP,
	MT6360_ADC_TEMP_JC = MT6360_ADC_NODUMP,
	MT6360_ADC_USBID,
	MT6360_ADC_TS,
	MT6360_ADC_MAX,
};

static const char * const mt6360_adc_chan_list[] = {
	"VBUSDIV5", "VSYS", "VBAT", "IBUS", "IBAT", "TEMP_JC", "USBID", "TS",
};

struct mt6360_chg_info {
	struct device *dev;
	struct mt6360_pmu_data *mpd;
	struct regmap *regmap;
	struct iio_channel *channels[MT6360_ADC_MAX];
	struct power_supply *psy;
	struct charger_device *chg_dev;
	int hidden_mode_cnt;
	struct mutex hidden_mode_lock;
	struct mutex pe_lock;
	struct mutex aicr_lock;
	struct mutex tchg_lock;
	struct mutex ichg_lock;
	int tchg;
	u32 zcv;
	u32 ichg;
	u32 ichg_dis_chg;

	/* boot_mode */
	u32 bootmode;
	u32 boottype;

	/* Charger type detection */
	struct mutex chgdet_lock;
	bool attach;
	bool pwr_rdy;
	bool bc12_en;
	int psy_usb_type;
	bool tcpc_attach;

	/* dts: charger */
	struct power_supply *chg_psy;

	/* type_c_port0 */
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	/* chg_det */
	wait_queue_head_t attach_wq;
	atomic_t chrdet_start;
	struct task_struct *attach_task;
	struct mutex attach_lock;
	bool typec_attach;
	bool tcpc_kpoc;
	struct work_struct chgdet_work;
	/* power supply */
	struct power_supply_desc psy_desc;

	struct completion aicc_done;
	struct completion pumpx_done;
	atomic_t pe_complete;
	/* mivr */
	atomic_t mivr_cnt;
	wait_queue_head_t mivr_wq;
	struct task_struct *mivr_task;
	/* unfinish pe pattern */
	struct workqueue_struct *pe_wq;
	struct work_struct pe_work;
	u8 ctd_dischg_status;
	/* otg_vbus */
	struct regulator_dev *otg_rdev;	};

/* for recive bat oc notify */
struct mt6360_chg_info *g_mci;

static const u32 mt6360_otg_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

enum mt6360_iinlmtsel {
	MT6360_IINLMTSEL_AICR_3250 = 0,
	MT6360_IINLMTSEL_CHG_TYPE,
	MT6360_IINLMTSEL_AICR,
	MT6360_IINLMTSEL_LOWER_LEVEL,
};

enum mt6360_charging_status {
	MT6360_CHG_STATUS_READY = 0,
	MT6360_CHG_STATUS_PROGRESS,
	MT6360_CHG_STATUS_DONE,
	MT6360_CHG_STATUS_FAULT,
	MT6360_CHG_STATUS_MAX,
};

enum mt6360_usbsw_state {
	MT6360_USBSW_CHG = 0,
	MT6360_USBSW_USB,
};

enum mt6360_pmu_chg_type {
	MT6360_CHG_TYPE_NOVBUS = 0,
	MT6360_CHG_TYPE_UNDER_GOING,
	MT6360_CHG_TYPE_SDP,
	MT6360_CHG_TYPE_SDPNSTD,
	MT6360_CHG_TYPE_DCP,
	MT6360_CHG_TYPE_CDP,
	MT6360_CHG_TYPE_MAX,
};

/* power supply enum */
static enum power_supply_usb_type mt6360_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const char __maybe_unused *mt6360_chg_status_name[] = {
	"ready", "progress", "done", "fault",
};

static const struct mt6360_chg_platform_data def_platform_data = {
	.ichg = 2000000,		/* uA */
	.aicr = 500000,			/* uA */
	.mivr = 4400000,		/* uV */
	.cv = 4350000,			/* uA */
	.ieoc = 250000,			/* uA */
	.safety_timer = 12,		/* hour */
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif
	.en_te = true,
	.en_wdt = true,
	.aicc_once = true,
	.post_aicc = true,
	.batoc_notify = false,
	.bc12_sel = 0,
	.chg_name = "primary_chg",
};

/* ================== */
/* Internal Functions */
/* ================== */
static inline u32 mt6360_trans_sel(u32 target, u32 min_val, u32 step,
				   u32 max_sel)
{
	u32 data = 0;

	if (target >= min_val)
		data = (target - min_val) / step;
	if (data > max_sel)
		data = max_sel;
	return data;
}

static u32 mt6360_trans_ichg_sel(u32 uA)
{
	return mt6360_trans_sel(uA, 100000, 100000, 0x31);
}

static u32 mt6360_trans_aicr_sel(u32 uA)
{
	return mt6360_trans_sel(uA, 100000, 50000, 0x3F);
}

static u32 mt6360_trans_mivr_sel(u32 uV)
{
	return mt6360_trans_sel(uV, 3900000, 100000, 0x5F);
}

static u32 mt6360_trans_cv_sel(u32 uV)
{
	return mt6360_trans_sel(uV, 3900000, 10000, 0x51);
}

static u32 mt6360_trans_ieoc_sel(u32 uA)
{
	return mt6360_trans_sel(uA, 100000, 50000, 0x0F);
}

static u32 mt6360_trans_safety_timer_sel(u32 hr)
{
	u32 mt6360_st_tbl[] = {4, 6, 8, 10, 12, 14, 16, 20};
	u32 cur_val, next_val;
	int i;

	if (hr < mt6360_st_tbl[0])
		return 0;
	for (i = 0; i < ARRAY_SIZE(mt6360_st_tbl) - 1; i++) {
		cur_val = mt6360_st_tbl[i];
		next_val = mt6360_st_tbl[i+1];
		if (hr >= cur_val && hr < next_val)
			return i;
	}
	return ARRAY_SIZE(mt6360_st_tbl) - 1;
}

static u32 mt6360_trans_ircmp_r_sel(u32 uohm)
{
	return mt6360_trans_sel(uohm, 0, 25000, 0x07);
}

static u32 mt6360_trans_ircmp_vclamp_sel(u32 uV)
{
	return mt6360_trans_sel(uV, 0, 32000, 0x07);
}

static const u32 mt6360_usbid_rup[] = {
	500000, 75000, 5000, 1000
};

static inline u32 mt6360_trans_usbid_rup(u32 rup)
{
	int i;
	int maxidx = ARRAY_SIZE(mt6360_usbid_rup) - 1;

	if (rup >= mt6360_usbid_rup[0])
		return 0;
	if (rup <= mt6360_usbid_rup[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (rup == mt6360_usbid_rup[i])
			return i;
		if (rup < mt6360_usbid_rup[i] &&
		    rup > mt6360_usbid_rup[i + 1]) {
			if ((mt6360_usbid_rup[i] - rup) <=
			    (rup - mt6360_usbid_rup[i + 1]))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static const u32 mt6360_usbid_src_ton[] = {
	400, 1000, 4000, 10000, 40000, 100000, 400000,
};

static inline u32 mt6360_trans_usbid_src_ton(u32 src_ton)
{
	int i;
	int maxidx = ARRAY_SIZE(mt6360_usbid_src_ton) - 1;

	/* There is actually an option, always on, after 400000 */
	if (src_ton == 0)
		return maxidx + 1;
	if (src_ton < mt6360_usbid_src_ton[0])
		return 0;
	if (src_ton > mt6360_usbid_src_ton[maxidx])
		return maxidx;

	for (i = 0; i < maxidx; i++) {
		if (src_ton == mt6360_usbid_src_ton[i])
			return i;
		if (src_ton > mt6360_usbid_src_ton[i] &&
		    src_ton < mt6360_usbid_src_ton[i + 1]) {
			if ((src_ton - mt6360_usbid_src_ton[i]) <=
			    (mt6360_usbid_src_ton[i + 1] - src_ton))
				return i;
			else
				return i + 1;
		}
	}
	return maxidx;
}

static inline int mt6360_get_ieoc(struct mt6360_chg_info *mci, u32 *uA)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL9, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & MT6360_MASK_IEOC) >> MT6360_SHFT_IEOC;
	*uA = 100000 + (ret * 50000);
	return ret;
}

static inline int mt6360_get_charging_status(
					struct mt6360_chg_info *mci,
					enum mt6360_charging_status *chg_stat)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT, &regval);
	if (ret < 0)
		return ret;
	*chg_stat = (regval & MT6360_MASK_CHG_STAT) >> MT6360_SHFT_CHG_STAT;
	return 0;
}

static inline int mt6360_is_charger_enabled(struct mt6360_chg_info *mci,
					    bool *en)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL2, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_MASK_CHG_EN) ? true : false;
	return 0;
}

static inline int mt6360_select_input_current_limit(
		struct mt6360_chg_info *mci, enum mt6360_iinlmtsel sel)
{
	dev_dbg(mci->dev,
		"%s: select input current limit = %d\n", __func__, sel);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL2,
				  MT6360_MASK_IINLMTSEL,
				  sel << MT6360_SHFT_IINLMTSEL);
}

static int mt6360_enable_wdt(struct mt6360_chg_info *mci, bool en)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_dbg(mci->dev, "%s enable wdt, en = %d\n", __func__, en);
	if (!pdata->en_wdt)
		return 0;
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL13,
				  MT6360_MASK_CHG_WDT_EN,
				  en ? 0xff : 0);
}

static inline int mt6360_get_chrdet_ext_stat(struct mt6360_chg_info *mci,
					  bool *pwr_rdy)
{
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_FOD_STAT, &regval);
	if (ret < 0)
		return ret;
	*pwr_rdy = (regval & BIT(4)) ? true : false;
	return 0;
}

static int DPDM_Switch_TO_CHG_upstream(struct mt6360_chg_info *mci,
						bool switch_to_chg)
{
	struct phy *phy;
	int mode = 0;
	int ret;

	mode = switch_to_chg ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;
	phy = phy_get(mci->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_info(mci->dev, "phy_get fail\n");
		return -EINVAL;
	}

	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_info(mci->dev, "phy_set_mode_ext fail\n");

	phy_put(mci->dev, phy);

	return 0;
}

static int mt6360_set_usbsw_state(struct mt6360_chg_info *mci, int state)
{
	dev_info(mci->dev, "%s: state = %d\n", __func__, state);

	/* Switch D+D- to AP/MT6360 */
	if (state == MT6360_USBSW_CHG)
		DPDM_Switch_TO_CHG_upstream(mci, true);
	else
		DPDM_Switch_TO_CHG_upstream(mci, false);

	return 0;
}

#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
static int __maybe_unused mt6360_enable_dcd_tout(
				      struct mt6360_chg_info *mci, bool en)
{
	dev_info(mci->dev, "%s en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
				  MT6360_MASK_DCDTOUTEN, en ? 0xff : 0);
}

static int __maybe_unused mt6360_is_dcd_tout_enable(
				     struct mt6360_chg_info *mci, bool *en)
{
	int ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_DEVICE_TYPE, &regval);
	if (ret < 0) {
		*en = false;
		return ret;
	}
	*en = (regval & MT6360_MASK_DCDTOUTEN ? true : false);
	return 0;
}
#endif

static bool is_usb_rdy(struct device *dev)
{
	struct device_node *node;
	bool ready = true;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = of_property_read_bool(node, "gadget-ready");
		dev_info(dev, "gadget-ready=%d\n", ready);
	} else
		dev_info(dev, "usb node missing or invalid\n");

	return ready;
}

static int __mt6360_enable_usbchgen(struct mt6360_chg_info *mci, bool en)
{
	int i, ret = 0;
	const int max_wait_cnt = 200;
	bool pwr_rdy = false;
	enum mt6360_usbsw_state usbsw =
				       en ? MT6360_USBSW_CHG : MT6360_USBSW_USB;
#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
	bool dcd_en = false;
#endif /* CONFIG_MT6360_DCDTOUT_SUPPORT */

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	if (en) {
#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
		ret = mt6360_is_dcd_tout_enable(mci, &dcd_en);
		if (!dcd_en)
			msleep(180);
#endif /* CONFIG_MT6360_DCDTOUT_SUPPORT */

		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy(mci->dev)) {
				dev_info(mci->dev, "%s: USB ready\n",
					__func__);
				break;
			}
			dev_info(mci->dev, "%s: CDP block\n", __func__);
			if (IS_ENABLED(CONFIG_TCPC_CLASS)) {
				if (!(mci->tcpc_attach)) {
					dev_info(mci->dev,
						 "%s: plug out\n", __func__);
					return 0;
				}
			} else {
				/* Check vbus */
				ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
				if (ret < 0) {
					dev_err(mci->dev,
						"%s: fail, ret = %d\n",
						 __func__, ret);
					return ret;
				}
				dev_info(mci->dev,
					 "%s: pwr_rdy = %d\n", __func__,
					 pwr_rdy);
				if (!pwr_rdy) {
					dev_info(mci->dev,
						 "%s: plug out\n", __func__);
					return ret;
				}
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			dev_err(mci->dev, "%s: CDP timeout\n", __func__);
		else
			dev_info(mci->dev, "%s: CDP free\n", __func__);
	}
	mt6360_set_usbsw_state(mci, usbsw);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_DEVICE_TYPE,
				 MT6360_MASK_USBCHGEN, en ? 0xff : 0);
	if (ret >= 0)
		mci->bc12_en = en;
	return ret;
}

static int mt6360_enable_usbchgen(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;

	mutex_lock(&mci->chgdet_lock);
	ret = __mt6360_enable_usbchgen(mci, en);
	mutex_unlock(&mci->chgdet_lock);
	return ret;
}

static int mt6360_chgdet_pre_process(struct mt6360_chg_info *mci)
{
	bool attach = false;

	if (IS_ENABLED(CONFIG_TCPC_CLASS))
		attach = mci->tcpc_attach;
	else
		attach = mci->pwr_rdy;
	if (attach && (mci->bootmode == 5)) {
		/* Skip charger type detection to speed up meta boot.*/
		dev_notice(mci->dev, "%s: force Standard USB Host in meta\n",
			   __func__);
		mci->attach = attach;
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		power_supply_changed(mci->psy);
		return 0;
	}
	return __mt6360_enable_usbchgen(mci, attach);
}

static int mt6360_chgdet_post_process(struct mt6360_chg_info *mci)
{
	int ret = 0;
	bool attach = false, inform_psy = true;
	u8 usb_status = MT6360_CHG_TYPE_NOVBUS;
	unsigned int regval;

	if (IS_ENABLED(CONFIG_TCPC_CLASS))
		attach = mci->tcpc_attach;
	else
		attach = mci->pwr_rdy;
	if (mci->attach == attach) {
		dev_info(mci->dev, "%s: attach(%d) is the same\n",
				    __func__, attach);
		inform_psy = !attach;
		goto out;
	}
	mci->attach = attach;
	dev_info(mci->dev, "%s: attach = %d\n", __func__, attach);
	/* Plug out during BC12 */
	if (!attach) {
		dev_info(mci->dev, "%s: Charger Type: UNKONWN\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out;
	}
	/* Plug in */
	ret = regmap_read(mci->regmap, MT6360_PMU_USB_STATUS1, &regval);
	if (ret < 0)
		goto out;
	usb_status = (regval & MT6360_MASK_USB_STATUS) >> MT6360_SHFT_USB_STATUS;
	switch (usb_status) {
	case MT6360_CHG_TYPE_UNDER_GOING:
		dev_info(mci->dev, "%s: under going...\n", __func__);
		return ret;
	case MT6360_CHG_TYPE_SDP:
		dev_info(mci->dev,
			  "%s: Charger Type: STANDARD_HOST\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case MT6360_CHG_TYPE_SDPNSTD:
		dev_info(mci->dev,
			  "%s: Charger Type: NONSTANDARD_CHARGER\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case MT6360_CHG_TYPE_CDP:
		dev_info(mci->dev,
			  "%s: Charger Type: CHARGING_HOST\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case MT6360_CHG_TYPE_DCP:
		dev_info(mci->dev,
			  "%s: Charger Type: STANDARD_CHARGER\n", __func__);
		mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	}
out:
	if (!attach) {
		ret = __mt6360_enable_usbchgen(mci, false);
		if (ret < 0)
			dev_notice(mci->dev, "%s: disable chgdet fail\n",
				   __func__);
	} else if (mci->psy_usb_type != POWER_SUPPLY_USB_TYPE_DCP)
		mt6360_set_usbsw_state(mci, MT6360_USBSW_USB);
	if (!inform_psy)
		return ret;
	power_supply_changed(mci->psy);
	return ret;
}

static const u32 mt6360_vinovp_list[] = {
	5500000, 6500000, 10500000, 14500000,
};

static int mt6360_select_vinovp(struct mt6360_chg_info *mci, u32 uV)
{
	int i;

	if (uV < mt6360_vinovp_list[0])
		return -EINVAL;
	for (i = 1; i < ARRAY_SIZE(mt6360_vinovp_list); i++) {
		if (uV < mt6360_vinovp_list[i])
			break;
	}
	i--;
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL19,
				  MT6360_MASK_CHG_VIN_OVP_VTHSEL,
				  i << MT6360_SHFT_CHG_VIN_OVP_VTHSEL);
}

static inline int mt6360_read_zcv(struct mt6360_chg_info *mci)
{
	int ret = 0;
	u8 zcv_data[2] = {0};

	dev_dbg(mci->dev, "%s\n", __func__);
	/* Read ZCV data */
	ret = regmap_bulk_read(mci->regmap, MT6360_PMU_ADC_BAT_DATA_H,
			       zcv_data, 2);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read zcv data fail\n", __func__);
		return ret;
	}
	mci->zcv = 1250 * (zcv_data[0] * 256 + zcv_data[1]);
	dev_info(mci->dev, "%s: zcv = (0x%02X, 0x%02X, %dmV)\n",
		 __func__, zcv_data[0], zcv_data[1], mci->zcv/1000);
	/* Disable ZCV */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_ADC_CONFIG,
				 MT6360_MASK_ZCV_EN, 0);
	if (ret < 0)
		dev_err(mci->dev, "%s: disable zcv fail\n", __func__);
	return ret;
}

static int mt6360_toggle_aicc(struct mt6360_chg_info *mci)
{
	struct mt6360_pmu_data *mpd = mci->mpd;
	int ret = 0;
	u8 data = 0;

	mutex_lock(&mpd->io_lock);
	/* read aicc */
	ret = i2c_smbus_read_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
					       MT6360_PMU_CHG_CTRL14, 1, &data);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read aicc fail\n", __func__);
		goto out;
	}
	/* aicc off */
	data &= ~MT6360_MASK_RG_EN_AICC;
	ret = i2c_smbus_read_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
					       MT6360_PMU_CHG_CTRL14, 1, &data);
	if (ret < 0) {
		dev_err(mci->dev, "%s: aicc off fail\n", __func__);
		goto out;
	}
	/* aicc on */
	data |= MT6360_MASK_RG_EN_AICC;
	ret = i2c_smbus_read_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
					       MT6360_PMU_CHG_CTRL14, 1, &data);
	if (ret < 0)
		dev_err(mci->dev, "%s: aicc on fail\n", __func__);
out:
	mutex_unlock(&mpd->io_lock);
	return ret;
}

static int __mt6360_set_aicr(struct mt6360_chg_info *mci, u32 uA)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	u8 data = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	/* Toggle aicc for auto aicc mode */
	if (!pdata->aicc_once) {
		ret = mt6360_toggle_aicc(mci);
		if (ret < 0) {
			dev_err(mci->dev, "%s: toggle aicc fail\n", __func__);
			return ret;
		}
	}
	/* Disable sys drop improvement for download mode */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL20,
				 MT6360_MASK_EN_SDI, 0);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable en_sdi fail\n", __func__);
		return ret;
	}
	data = mt6360_trans_aicr_sel(uA);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL3,
				  MT6360_MASK_AICR,
				  data << MT6360_SHFT_AICR);
}

static int __mt6360_set_ichg(struct mt6360_chg_info *mci, u32 uA)
{
	int ret = 0;
	u32 data = 0;

	mt_dbg(mci->dev, "%s\n", __func__);
	data = mt6360_trans_ichg_sel(uA);
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL7,
				 MT6360_MASK_ICHG,
				 data << MT6360_SHFT_ICHG);
	if (ret < 0)
		dev_notice(mci->dev, "%s: fail\n", __func__);
	else
		mci->ichg = uA;
	return ret;
}

static int __mt6360_enable(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;
	u32 ichg_ramp_t = 0;

	mt_dbg(mci->dev, "%s: en = %d\n", __func__, en);

	/* Workaround for vsys overshoot */
	mutex_lock(&mci->ichg_lock);
	if (mci->ichg < 500000) {
		dev_info(mci->dev,
			 "%s: ichg < 500mA, bypass vsys wkard\n", __func__);
		goto out;
	}
	if (!en) {
		mci->ichg_dis_chg = mci->ichg;
		ichg_ramp_t = (mci->ichg - 500000) / 50000 * 2;
		/* Set ichg to 500mA */
		ret = regmap_update_bits(mci->regmap,
					 MT6360_PMU_CHG_CTRL7,
					 MT6360_MASK_ICHG,
					 0x04 << MT6360_SHFT_ICHG);
		if (ret < 0) {
			dev_notice(mci->dev,
				   "%s: set ichg fail\n", __func__);
			goto vsys_wkard_fail;
		}
		mdelay(ichg_ramp_t);
	} else {
		if (mci->ichg == mci->ichg_dis_chg) {
			ret = __mt6360_set_ichg(mci, mci->ichg);
			if (ret < 0) {
				dev_notice(mci->dev,
					   "%s: set ichg fail\n", __func__);
				goto out;
			}
		}
	}

out:
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL2,
				 MT6360_MASK_CHG_EN, en ? 0xff : 0);
	if (ret < 0)
		dev_notice(mci->dev, "%s: fail, en = %d\n", __func__, en);
vsys_wkard_fail:
	mutex_unlock(&mci->ichg_lock);
	return ret;
}

static int mt6360_enable_pump_express(struct mt6360_chg_info *mci,
				      bool pe20)
{
	long timeout, pe_timeout = pe20 ? 1400 : 2800;
	int ret = 0;

	dev_info(mci->dev, "%s\n", __func__);
	ret = __mt6360_set_aicr(mci, 800000);
	if (ret < 0)
		return ret;
	mutex_lock(&mci->ichg_lock);
	ret = __mt6360_set_ichg(mci, 2000000);
	mutex_unlock(&mci->ichg_lock);
	if (ret < 0)
		return ret;
	ret = __mt6360_enable(mci, true);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_EN_PUMPX, 0);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_EN_PUMPX, 0xff);
	if (ret < 0)
		return ret;
	reinit_completion(&mci->pumpx_done);
	atomic_set(&mci->pe_complete, 1);
	timeout = wait_for_completion_interruptible_timeout(
			       &mci->pumpx_done, msecs_to_jiffies(pe_timeout));
	if (timeout == 0)
		ret = -ETIMEDOUT;
	else if (timeout < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0)
		dev_err(mci->dev,
			"%s: wait pumpx timeout, ret = %d\n", __func__, ret);
	return ret;
}

static int __mt6360_set_pep20_current_pattern(struct mt6360_chg_info *mci,
					    u32 uV)
{
	int ret = 0;
	u8 data = 0;

	dev_dbg(mci->dev, "%s: pep2.0 = %d\n", __func__, uV);
	mutex_lock(&mci->pe_lock);
	if (uV >= 5500000)
		data = (uV - 5500000) / 500000;
	if (data > MT6360_PUMPX_20_MAXVAL)
		data = MT6360_PUMPX_20_MAXVAL;
	/* Set to PE2.0 */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_20_10, 0xff);
	if (ret < 0) {
		dev_err(mci->dev, "%s: enable pumpx 20 fail\n", __func__);
		goto out;
	}
	/* Set Voltage */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_DEC,
				 data << MT6360_SHFT_PUMPX_DEC);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set pumpx voltage fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, true);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int __mt6360_get_adc(struct mt6360_chg_info *mci,
			    enum mt6360_adc_channel channel, int *min, int *max)
{
	int ret = 0;

	ret = iio_read_channel_processed(mci->channels[channel], min);
	if (ret < 0) {
		dev_info(mci->dev, "%s: fail(%d)\n", __func__, ret);
		return ret;
	}
	*max = *min;
	return 0;
}

static int __mt6360_enable_chg_type_det(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);

	if (!IS_ENABLED(CONFIG_TCPC_CLASS) || pdata->bc12_sel != 0)
		return ret;

	mutex_lock(&mci->chgdet_lock);
	if (mci->tcpc_attach == en) {
		dev_info(mci->dev, "%s attach(%d) is the same\n",
			 __func__, mci->tcpc_attach);
		goto out;
	}
	mci->tcpc_attach = en;
	ret = (en ? mt6360_chgdet_pre_process :
		    mt6360_chgdet_post_process)(mci);
out:
	mutex_unlock(&mci->chgdet_lock);
	return ret;
}

static int __mt6360_enable_otg(struct mt6360_chg_info *mci, bool en)
{
	int ret = 0;

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	ret = mt6360_enable_wdt(mci, en ? true : false);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set wdt fail, en = %d\n", __func__, en);
		return ret;
	}
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_MASK_OPA_MODE, en ? 0xff : 0);
}

/* ================== */
/* External Functions */
/* ================== */
#if IS_ENABLED(CONFIG_MTK_CHARGER)
static int mt6360_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	mutex_lock(&mci->ichg_lock);
	ret = __mt6360_set_ichg(mci, uA);
	mutex_unlock(&mci->ichg_lock);
	return ret;
}

static int mt6360_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL7, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & MT6360_MASK_ICHG) >> MT6360_SHFT_ICHG;
	*uA = 100000 + (ret * 100000);
	return 0;
}

static int mt6360_enable_hidden_mode(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	static const u8 pascode[] = { 0x69, 0x96, 0x63, 0x72, };
	int ret = 0;

	mutex_lock(&mci->hidden_mode_lock);
	if (en) {
		if (mci->hidden_mode_cnt == 0) {
			ret = regmap_bulk_write(mci->regmap,
					   MT6360_PMU_TM_PAS_CODE1, pascode, 4);
			if (ret < 0)
				goto err;
		}
		mci->hidden_mode_cnt++;
	} else {
		if (mci->hidden_mode_cnt == 1)
			ret = regmap_write(mci->regmap,
					   MT6360_PMU_TM_PAS_CODE1, 0x00);
		mci->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	mt_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	goto out;
err:
	dev_err(mci->dev, "%s failed, en = %d\n", __func__, en);
out:
	mutex_unlock(&mci->hidden_mode_lock);
	return ret;
}

static int mt6360_enable(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_enable(mci, en);
}

static int mt6360_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 300000;
	return 0;
}

static int mt6360_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u8 data = 0;

	dev_dbg(mci->dev, "%s: cv = %d\n", __func__, uV);
	data = mt6360_trans_cv_sel(uV);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL4,
				  MT6360_MASK_VOREG,
				  data << MT6360_SHFT_VOREG);
}

static int mt6360_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL4, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & MT6360_MASK_VOREG) >> MT6360_SHFT_VOREG;
	*uV = 3900000 + (ret * 10000);
	return 0;
}

static int mt6360_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_set_aicr(mci, uA);
}

static int mt6360_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL3, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & MT6360_MASK_AICR) >> MT6360_SHFT_AICR;
	*uA = 100000 + (ret * 50000);
	return 0;
}

static int mt6360_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int mt6360_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u8 data = 0;

	dev_dbg(mci->dev, "%s: ieoc = %d\n", __func__, uA);
	data = mt6360_trans_ieoc_sel(uA);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL9,
				  MT6360_MASK_IEOC,
				  data << MT6360_SHFT_IEOC);
}

static int mt6360_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 aicc_vth = 0, data = 0;
	u8 aicc_vth_sel = 0;
	int ret = 0;

	mt_dbg(mci->dev, "%s: mivr = %d\n", __func__, uV);
	if (uV < 3900000 || uV > 13400000) {
		dev_err(mci->dev,
			"%s: unsuitable mivr val(%d)\n", __func__, uV);
		return -EINVAL;
	}
	/* Check if there's a suitable AICC_VTH */
	aicc_vth = uV + 200000;
	aicc_vth_sel = (aicc_vth - 3900000) / 100000;
	if (aicc_vth_sel > MT6360_AICC_VTH_MAXVAL) {
		dev_err(mci->dev, "%s: can't match, aicc_vth_sel = %d\n",
			__func__, aicc_vth_sel);
		return -EINVAL;
	}
	/* Set AICC_VTH threshold */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL16,
				 MT6360_MASK_AICC_VTH,
				 aicc_vth_sel << MT6360_SHFT_AICC_VTH);
	if (ret < 0)
		return ret;
	/* Set MIVR */
	data = mt6360_trans_mivr_sel(uV);
	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL6,
				  MT6360_MASK_MIVR,
				  data << MT6360_SHFT_MIVR);
}

static inline int mt6360_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL6, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & MT6360_MASK_MIVR) >> MT6360_SHFT_MIVR;
	*uV = 3900000 + (ret * 100000);
	return 0;
}

static int mt6360_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	*in_loop = (regval & MT6360_MASK_MIVR_EVT) >> MT6360_SHFT_MIVR_EVT;
	return 0;
}

static int mt6360_enable_te(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	if (!pdata->en_te)
		return 0;
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
				  MT6360_MASK_TE_EN, en ? 0xff : 0);
}

static int mt6360_set_pep_current_pattern(struct charger_device *chg_dev,
					  bool is_inc)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s: pe1.0 pump up = %d\n", __func__, is_inc);

	mutex_lock(&mci->pe_lock);
	/* Set to PE1.0 */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_20_10, 0);
	if (ret < 0) {
		dev_err(mci->dev, "%s: enable pumpx 10 fail\n", __func__);
		goto out;
	}

	/* Set Pump Up/Down */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_UP_DN,
				 is_inc ? 0xff : 0);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set pumpx up/down fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, false);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int mt6360_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
#if FIXME /* TODO: without charger manager */
	struct charger_manager *chg_mgr = NULL;

	chg_mgr = charger_dev_get_drvdata(chg_dev);
	if (!chg_mgr)
		return -EINVAL;

	chg_mgr->pe2.profile[0].vchr = 8000000;
	chg_mgr->pe2.profile[1].vchr = 8000000;
	chg_mgr->pe2.profile[2].vchr = 8000000;
	chg_mgr->pe2.profile[3].vchr = 8500000;
	chg_mgr->pe2.profile[4].vchr = 8500000;
	chg_mgr->pe2.profile[5].vchr = 8500000;
	chg_mgr->pe2.profile[6].vchr = 9000000;
	chg_mgr->pe2.profile[7].vchr = 9000000;
	chg_mgr->pe2.profile[8].vchr = 9500000;
	chg_mgr->pe2.profile[9].vchr = 9500000;
#endif
	return 0;
}

static int mt6360_set_pep20_current_pattern(struct charger_device *chg_dev,
					    u32 uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_set_pep20_current_pattern(mci, uV);
}

static int mt6360_reset_ta(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_set_mivr(chg_dev, 4600000);
	if (ret < 0)
		return ret;
	ret = mt6360_select_input_current_limit(mci, MT6360_IINLMTSEL_AICR);
	if (ret < 0)
		return ret;
	ret = mt6360_set_aicr(chg_dev, 100000);
	if (ret < 0)
		return ret;
	msleep(250);
	return mt6360_set_aicr(chg_dev, 500000);
}

static int mt6360_enable_cable_drop_comp(struct charger_device *chg_dev,
					 bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);
	if (en)
		return ret;

	/* Set to PE2.0 */
	mutex_lock(&mci->pe_lock);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_20_10, 0xff);
	if (ret < 0) {
		dev_err(mci->dev, "%s: enable pumpx 20 fail\n", __func__);
		goto out;
	}
	/* Disable cable drop compensation */
	ret = regmap_update_bits(mci->regmap,
				 MT6360_PMU_CHG_CTRL17,
				 MT6360_MASK_PUMPX_DEC,
				 0x1F << MT6360_SHFT_PUMPX_DEC);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set pumpx voltage fail\n", __func__);
		goto out;
	}
	ret = mt6360_enable_pump_express(mci, true);
out:
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static inline int mt6360_get_aicc(struct mt6360_chg_info *mci,
				  u32 *aicc_val)
{
	u8 aicc_sel = 0;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_AICC_RESULT, &regval);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read aicc result fail\n", __func__);
		return ret;
	}
	aicc_sel = (regval & MT6360_MASK_RG_AICC_RESULT) >>
						     MT6360_SHFT_RG_AICC_RESULT;
	*aicc_val = (aicc_sel * 50000) + 100000;
	return 0;
}

static inline int mt6360_post_aicc_measure(struct charger_device *chg_dev,
					   u32 start, u32 stop, u32 step,
					   u32 *measure)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int cur, ret;
	unsigned int regval;

	mt_dbg(mci->dev,
		"%s: post_aicc = (%d, %d, %d)\n", __func__, start, stop, step);
	for (cur = start; cur < (stop + step); cur += step) {
		/* set_aicr to cur */
		ret = mt6360_set_aicr(chg_dev, cur + step);
		if (ret < 0)
			return ret;
		usleep_range(150, 200);
		ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
		if (ret < 0)
			return ret;
		/* read mivr stat */
		if (regval & MT6360_MASK_MIVR_EVT)
			break;
	}
	*measure = cur;
	return 0;
}

static int mt6360_run_aicc(struct charger_device *chg_dev, u32 *uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	u32 aicc_val = 0, aicr_val;
	long timeout;
	bool mivr_stat = false;
	unsigned int regval;

	mt_dbg(mci->dev, "%s: aicc_once = %d\n", __func__, pdata->aicc_once);
	/* check MIVR stat is act */
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read mivr stat fail\n", __func__);
		return ret;
	}
	mivr_stat = (regval & MT6360_MASK_MIVR_EVT) ? true : false;
	if (!mivr_stat) {
		dev_err(mci->dev, "%s: mivr stat not act\n", __func__);
		return ret;
	}

	/* Auto run aicc */
	if (!pdata->aicc_once) {
		if (!try_wait_for_completion(&mci->aicc_done)) {
			dev_info(mci->dev, "%s: aicc is not act\n", __func__);
			return 0;
		}

		/* get aicc result */
		ret = mt6360_get_aicc(mci, &aicc_val);
		if (ret < 0) {
			dev_err(mci->dev,
				"%s: get aicc fail\n", __func__);
			return ret;
		}
		*uA = aicc_val;
		reinit_completion(&mci->aicc_done);
		return ret;
	}

	/* Use aicc once method */
	/* Run AICC measure */
	mutex_lock(&mci->pe_lock);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_MASK_RG_EN_AICC, 0xff);
	if (ret < 0)
		goto out;
	/* Clear AICC measurement IRQ */
	reinit_completion(&mci->aicc_done);
	timeout = wait_for_completion_interruptible_timeout(
				   &mci->aicc_done, msecs_to_jiffies(3000));
	if (timeout == 0)
		ret = -ETIMEDOUT;
	else if (timeout < 0)
		ret = -EINTR;
	else
		ret = 0;
	if (ret < 0) {
		dev_err(mci->dev,
			"%s: wait AICC time out, ret = %d\n", __func__, ret);
		goto out;
	}
	/* get aicc_result */
	ret = mt6360_get_aicc(mci, &aicc_val);
	if (ret < 0) {
		dev_err(mci->dev, "%s: get aicc result fail\n", __func__);
		goto out;
	}

	if (!pdata->post_aicc)
		goto skip_post_aicc;

	dev_info(mci->dev, "%s: aicc pre val = %d\n", __func__, aicc_val);
	ret = mt6360_get_aicr(chg_dev, &aicr_val);
	if (ret < 0) {
		dev_err(mci->dev, "%s: get aicr fail\n", __func__);
		goto out;
	}
	ret = mt6360_set_aicr(chg_dev, aicc_val);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set aicr fail\n", __func__);
		goto out;
	}
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_MASK_RG_EN_AICC, 0);
	if (ret < 0)
		goto out;
	/* always start/end aicc_val/aicc_val+200mA */
	ret = mt6360_post_aicc_measure(chg_dev, aicc_val,
				       aicc_val + 200000, 50000, &aicc_val);
	if (ret < 0)
		goto out;
	ret = mt6360_set_aicr(chg_dev, aicr_val);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set aicr fail\n", __func__);
		goto out;
	}
	dev_info(mci->dev, "%s: aicc post val = %d\n", __func__, aicc_val);
skip_post_aicc:
	*uA = aicc_val;
out:
	/* Clear EN_AICC */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
				 MT6360_MASK_RG_EN_AICC, 0);
	mutex_unlock(&mci->pe_lock);
	return ret;
}

static int mt6360_enable_power_path(struct charger_device *chg_dev,
					    bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_MASK_FORCE_SLEEP, en ? 0 : 0xff);
}

static int mt6360_is_power_path_enabled(struct charger_device *chg_dev,
						bool *en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_MASK_FORCE_SLEEP) ? false : true;
	return 0;
}

static int mt6360_enable_safety_timer(struct charger_device *chg_dev,
					      bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL12,
				  MT6360_MASK_TMR_EN, en ? 0xff : 0);
}

static int mt6360_is_safety_timer_enabled(
				struct charger_device *chg_dev, bool *en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL12, &regval);
	if (ret < 0)
		return ret;
	*en = (regval & MT6360_MASK_TMR_EN) ? true : false;
	return 0;
}

static int mt6360_enable_hz(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: en = %d\n", __func__, en);

	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL1,
				  MT6360_MASK_HZ_EN, en ? 0xff : 0);
}

static const u32 otg_oc_table[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000
};

static int mt6360_set_otg_current_limit(struct charger_device *chg_dev,
						u32 uA)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i;

	/* Set higher OC threshold protect */
	for (i = 0; i < ARRAY_SIZE(otg_oc_table); i++) {
		if (uA <= otg_oc_table[i])
			break;
	}
	if (i == ARRAY_SIZE(otg_oc_table))
		i = MT6360_OTG_OC_MAXVAL;
	dev_dbg(mci->dev,
		"%s: select oc threshold = %d\n", __func__, otg_oc_table[i]);

	return regmap_update_bits(mci->regmap,
				  MT6360_PMU_CHG_CTRL10,
				  MT6360_MASK_OTG_OC,
				  i << MT6360_SHFT_OTG_OC);
}

static int mt6360_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_enable_otg(mci, en);
}

static int mt6360_enable_discharge(struct charger_device *chg_dev,
					   bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i, ret = 0;
	const int dischg_retry_cnt = 3;
	bool is_dischg;
	unsigned int regval;

	dev_dbg(mci->dev, "%s: en = %d\n", __func__, en);
	ret = mt6360_enable_hidden_mode(mci->chg_dev, true);
	if (ret < 0)
		return ret;
	/* Set bit2 of reg[0x31] to 1/0 to enable/disable discharging */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL2,
				 MT6360_MASK_DISCHG, en ? 0xff : 0);
	if (ret < 0) {
		dev_err(mci->dev, "%s: fail, en = %d\n", __func__, en);
		goto out;
	}

	if (!en) {
		for (i = 0; i < dischg_retry_cnt; i++) {
			ret = regmap_read(mci->regmap,
					  MT6360_PMU_CHG_HIDDEN_CTRL2, &regval);
			is_dischg = (regval & MT6360_MASK_DISCHG) ?
								   true : false;
			if (!is_dischg)
				break;
			ret = regmap_update_bits(mci->regmap,
						MT6360_PMU_CHG_HIDDEN_CTRL2,
						MT6360_MASK_DISCHG, 0);
			if (ret < 0) {
				dev_err(mci->dev,
					"%s: disable dischg failed\n",
					__func__);
				goto out;
			}
		}
		if (i == dischg_retry_cnt) {
			dev_err(mci->dev, "%s: dischg failed\n", __func__);
			ret = -EINVAL;
		}
	}
out:
	mt6360_enable_hidden_mode(mci->chg_dev, false);
	return ret;
}

static int mt6360_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return __mt6360_enable_chg_type_det(mci, en);
}

static int mt6360_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	enum mt6360_adc_channel channel;

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		channel = MT6360_ADC_VBUSDIV5;
		break;
	case ADC_CHANNEL_VSYS:
		channel = MT6360_ADC_VSYS;
		break;
	case ADC_CHANNEL_VBAT:
		channel = MT6360_ADC_VBAT;
		break;
	case ADC_CHANNEL_IBUS:
		channel = MT6360_ADC_IBUS;
		break;
	case ADC_CHANNEL_IBAT:
		channel = MT6360_ADC_IBAT;
		break;
	case ADC_CHANNEL_TEMP_JC:
		channel = MT6360_ADC_TEMP_JC;
		break;
	case ADC_CHANNEL_USBID:
		channel = MT6360_ADC_USBID;
		break;
	case ADC_CHANNEL_TS:
		channel = MT6360_ADC_TS;
		break;
	default:
		return -ENOTSUPP;
	}
	return __mt6360_get_adc(mci, channel, min, max);
}

static int mt6360_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_VBUS, vbus, vbus);
}

static int mt6360_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_IBUS, ibus, ibus);
}

static int mt6360_get_ibat(struct charger_device *chg_dev, u32 *ibat)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);
	return mt6360_get_adc(chg_dev, ADC_CHANNEL_IBAT, ibat, ibat);
}

static int mt6360_get_tchg(struct charger_device *chg_dev,
				   int *tchg_min, int *tchg_max)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int temp_jc = 0, ret = 0, retry_cnt = 3;

	mt_dbg(mci->dev, "%s\n", __func__);
	/* temp abnormal Workaround */
	do {
		ret = mt6360_get_adc(chg_dev, ADC_CHANNEL_TEMP_JC,
				     &temp_jc, &temp_jc);
		if (ret < 0) {
			dev_err(mci->dev,
				"%s: failed, ret = %d\n", __func__, ret);
			return ret;
		}
	} while (temp_jc >= 120 && (retry_cnt--) > 0);
	mutex_lock(&mci->tchg_lock);
	if (temp_jc >= 120)
		temp_jc = mci->tchg;
	else
		mci->tchg = temp_jc;
	mutex_unlock(&mci->tchg_lock);
	*tchg_min = *tchg_max = temp_jc;
	dev_info(mci->dev, "%s: tchg = %d\n", __func__, temp_jc);
	return 0;
}

static int mt6360_kick_wdt(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_dbg(mci->dev, "%s\n", __func__);
	return regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, NULL);
}

static int mt6360_safety_check(struct charger_device *chg_dev, u32 polling_ieoc)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret, ibat = 0;
	static int eoc_cnt;

	mt_dbg(mci->dev, "%s\n", __func__);
	ret = iio_read_channel_processed(mci->channels[MT6360_ADC_IBAT],
					 &ibat);
	if (ret < 0)
		dev_err(mci->dev, "%s: failed, ret = %d\n", __func__, ret);

	if (ibat <= polling_ieoc)
		eoc_cnt++;
	else
		eoc_cnt = 0;
	/* If ibat is less than polling_ieoc for 3 times, trigger EOC event */
	if (eoc_cnt == 3) {
		dev_info(mci->dev, "%s: polling_ieoc = %d, ibat = %d\n",
			 __func__, polling_ieoc, ibat);
		charger_dev_notify(mci->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		eoc_cnt = 0;
	}
	return ret;
}

static int mt6360_reset_eoc_state(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);

	ret = mt6360_enable_hidden_mode(mci->chg_dev, true);
	if (ret < 0)
		return ret;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL1,
				 MT6360_MASK_EOC_RST, 0xff);
	if (ret < 0) {
		dev_err(mci->dev, "%s: set failed, ret = %d\n", __func__, ret);
		goto out;
	}
	udelay(100);
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_HIDDEN_CTRL1,
				 MT6360_MASK_EOC_RST, 0);
	if (ret < 0) {
		dev_err(mci->dev,
			"%s: clear failed, ret = %d\n", __func__, ret);
		goto out;
	}
out:
	mt6360_enable_hidden_mode(mci->chg_dev, false);
	return ret;
}

static int mt6360_is_charging_done(struct charger_device *chg_dev,
					   bool *done)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	enum mt6360_charging_status chg_stat;
	int ret = 0;

	mt_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_get_charging_status(mci, &chg_stat);
	if (ret < 0)
		return ret;
	*done = (chg_stat == MT6360_CHG_STATUS_DONE) ? true : false;
	return 0;
}

static int mt6360_get_zcv(struct charger_device *chg_dev, u32 *uV)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	dev_info(mci->dev, "%s: zcv = %dmV\n", __func__, mci->zcv / 1000);
	*uV = mci->zcv;
	return 0;
}

static int mt6360_dump_registers(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int i, ret = 0;
	int adc_vals[MT6360_ADC_MAX];
	u32 ichg = 0, aicr = 0, mivr = 0, cv = 0, ieoc = 0;
	enum mt6360_charging_status chg_stat = MT6360_CHG_STATUS_READY;
	bool chg_en = false;
	u8 chg_stat1 = 0, chg_ctrl[2] = {0};
	unsigned int regval;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_get_ichg(chg_dev, &ichg);
	ret |= mt6360_get_aicr(chg_dev, &aicr);
	ret |= mt6360_get_mivr(chg_dev, &mivr);
	ret |= mt6360_get_cv(chg_dev, &cv);
	ret |= mt6360_get_ieoc(mci, &ieoc);
	ret |= mt6360_get_charging_status(mci, &chg_stat);
	ret |= mt6360_is_charger_enabled(mci, &chg_en);
	if (ret < 0) {
		dev_notice(mci->dev, "%s: parse chg setting fail\n", __func__);
		return ret;
	}
	for (i = 0; i < MT6360_ADC_MAX; i++) {
		/* Skip unnecessary channel */
		if (i >= MT6360_ADC_NODUMP)
			break;
		ret = iio_read_channel_processed(mci->channels[i],
						 &adc_vals[i]);
		if (ret < 0) {
			dev_err(mci->dev,
				"%s: read [%s] adc fail(%d)\n",
				__func__, mt6360_adc_chan_list[i], ret);
			return ret;
		}
	}
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	chg_stat1 = regval;

	ret = regmap_bulk_read(mci->regmap, MT6360_PMU_CHG_CTRL1, chg_ctrl, 2);
	if (ret < 0)
		return ret;
	dev_info(mci->dev,
		 "%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA, CV = %dmV\n",
		 __func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000,
		 cv / 1000);
	dev_info(mci->dev,
		 "%s: VBUS = %dmV, IBUS = %dmA, VSYS = %dmV, VBAT = %dmV, IBAT = %dmA\n",
		 __func__,
		 adc_vals[MT6360_ADC_VBUSDIV5] / 1000,
		 adc_vals[MT6360_ADC_IBUS] / 1000,
		 adc_vals[MT6360_ADC_VSYS] / 1000,
		 adc_vals[MT6360_ADC_VBAT] / 1000,
		 adc_vals[MT6360_ADC_IBAT] / 1000);
	dev_info(mci->dev, "%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT1 = 0x%02X\n",
		 __func__, chg_en, mt6360_chg_status_name[chg_stat], chg_stat1);
	dev_info(mci->dev, "%s: CHG_CTRL1 = 0x%02X, CHG_CTRL2 = 0x%02X\n",
		 __func__, chg_ctrl[0], chg_ctrl[1]);
	return 0;
}

static int mt6360_do_event(struct charger_device *chg_dev, u32 event,
				   u32 args)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	mt_dbg(mci->dev, "%s\n", __func__);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(mci->psy);
		break;
	default:
		break;
	}

	return 0;
}

static int mt6360_plug_in(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	union power_supply_propval propval;
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);

	ret = mt6360_enable_wdt(mci, true);
	if (ret < 0) {
		dev_err(mci->dev, "%s: en wdt failed\n", __func__);
		return ret;
	}
	/* Replace CHG_EN by TE for avoid CV level too low trigger ieoc */
	/* TODO: First select cv, then chg_en, no need ? */
	ret = mt6360_enable_te(chg_dev, true);
	if (ret < 0) {
		dev_err(mci->dev, "%s: en te failed\n", __func__);
		return ret;
	}

	/* Workaround for ibus stuck in pe/pe20 pattern */
	ret = power_supply_get_property(mci->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);
	if (ret < 0) {
		dev_err(mci->dev, "%s: get chg_type fail\n", __func__);
		return ret;
	}
	return ret;
}

static int mt6360_plug_out(struct charger_device *chg_dev)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	int ret = 0;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = mt6360_enable_wdt(mci, false);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable wdt failed\n", __func__);
		return ret;
	}
	ret = mt6360_enable_te(chg_dev, false);
	if (ret < 0)
		dev_err(mci->dev, "%s: disable te failed\n", __func__);
	return ret;
}

static int mt6360_enable_usbid(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_MASK_USBID_EN, en ? 0xff : 0);
}

static int mt6360_set_usbid_rup(struct charger_device *chg_dev, u32 rup)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = mt6360_trans_usbid_rup(rup);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_MASK_ID_RPULSEL,
				  data << MT6360_SHFT_ID_RPULSEL);
}

static int mt6360_set_usbid_src_ton(struct charger_device *chg_dev, u32 src_ton)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);
	u32 data = mt6360_trans_usbid_src_ton(src_ton);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL1,
				  MT6360_MASK_ISTDET,
				  data << MT6360_SHFT_ISTDET);
}

static int mt6360_enable_usbid_floating(struct charger_device *chg_dev, bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL2,
				  MT6360_MASK_USBID_FLOAT, en ? 0xff : 0);
}

static int mt6360_enable_force_typec_otp(struct charger_device *chg_dev,
					 bool en)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	return regmap_update_bits(mci->regmap, MT6360_PMU_TYPEC_OTP_CTRL,
				  MT6360_MASK_TYPEC_OTP_SWEN, en ? 0xff : 0);
}

static int mt6360_get_ctd_dischg_status(struct charger_device *chg_dev,
					u8 *status)
{
	struct mt6360_chg_info *mci = charger_get_data(chg_dev);

	*status = mci->ctd_dischg_status;
	return 0;
}

static const struct charger_ops mt6360_chg_ops = {
	/* cable plug in/out */
	.plug_in = mt6360_plug_in,
	.plug_out = mt6360_plug_out,
	/* enable */
	.enable = mt6360_enable,
	/* charging current */
	.set_charging_current = mt6360_set_ichg,
	.get_charging_current = mt6360_get_ichg,
	.get_min_charging_current = mt6360_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = mt6360_set_cv,
	.get_constant_voltage = mt6360_get_cv,
	/* charging input current */
	.set_input_current = mt6360_set_aicr,
	.get_input_current = mt6360_get_aicr,
	.get_min_input_current = mt6360_get_min_aicr,
	/* set termination current */
	.set_eoc_current = mt6360_set_ieoc,
	/* charging mivr */
	.set_mivr = mt6360_set_mivr,
	.get_mivr = mt6360_get_mivr,
	.get_mivr_state = mt6360_get_mivr_state,
	/* charing termination */
	.enable_termination = mt6360_enable_te,
	/* PE+/PE+20 */
	.send_ta_current_pattern = mt6360_set_pep_current_pattern,
	.set_pe20_efficiency_table = mt6360_set_pep20_efficiency_table,
	.send_ta20_current_pattern = mt6360_set_pep20_current_pattern,
	.reset_ta = mt6360_reset_ta,
	.enable_cable_drop_comp = mt6360_enable_cable_drop_comp,
	.run_aicl = mt6360_run_aicc,
	/* Power path */
	.enable_powerpath = mt6360_enable_power_path,
	.is_powerpath_enabled = mt6360_is_power_path_enabled,
	/* safety timer */
	.enable_safety_timer = mt6360_enable_safety_timer,
	.is_safety_timer_enabled = mt6360_is_safety_timer_enabled,
	/* OTG */
	.enable_otg = mt6360_enable_otg,
	.set_boost_current_limit = mt6360_set_otg_current_limit,
	.enable_discharge = mt6360_enable_discharge,
	/* Charger type detection */
	.enable_chg_type_det = mt6360_enable_chg_type_det,
	/* ADC */
	.get_adc = mt6360_get_adc,
	.get_vbus_adc = mt6360_get_vbus,
	.get_ibus_adc = mt6360_get_ibus,
	.get_ibat_adc = mt6360_get_ibat,
	.get_tchg_adc = mt6360_get_tchg,
	/* kick wdt */
	.kick_wdt = mt6360_kick_wdt,
	/* misc */
	.safety_check = mt6360_safety_check,
	.reset_eoc_state = mt6360_reset_eoc_state,
	.is_charging_done = mt6360_is_charging_done,
	.get_zcv = mt6360_get_zcv,
	.dump_registers = mt6360_dump_registers,
	.enable_hz = mt6360_enable_hz,
	/* event */
	.event = mt6360_do_event,
	/* TypeC */
	.enable_usbid = mt6360_enable_usbid,
	.set_usbid_rup = mt6360_set_usbid_rup,
	.set_usbid_src_ton = mt6360_set_usbid_src_ton,
	.enable_usbid_floating = mt6360_enable_usbid_floating,
	.enable_force_typec_otp = mt6360_enable_force_typec_otp,
	.get_ctd_dischg_status = mt6360_get_ctd_dischg_status,
	.enable_hidden_mode = mt6360_enable_hidden_mode,
};

static const struct charger_properties mt6360_chg_props = {
	.alias_name = "mt6360_chg",
};
#endif /* CONFIG_MTK_CHARGER */

static irqreturn_t mt6360_pmu_chg_treg_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret = 0;
	unsigned int regval;

	dev_err(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	if ((regval & MT6360_MASK_CHG_TREG) >> MT6360_SHFT_CHG_TREG)
		dev_err(mci->dev,
			"%s: thermal regulation loop is active\n", __func__);
	return IRQ_HANDLED;
}

static void mt6360_pmu_chg_irq_enable(const char *name, int en);
static irqreturn_t mt6360_pmu_chg_mivr_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	mt_dbg(mci->dev, "%s\n", __func__);
	mt6360_pmu_chg_irq_enable("chg_mivr_evt", 0);
	atomic_inc(&mci->mivr_cnt);
	wake_up(&mci->mivr_wq);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_pwr_rdy_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	bool pwr_rdy = false;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
	if (ret < 0)
		return ret;
	pwr_rdy = (regval & MT6360_MASK_PWR_RDY_EVT);
	dev_info(mci->dev, "%s: pwr_rdy = %d\n", __func__, pwr_rdy);

	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_batsysuv_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vsysuv_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vsysov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vbatov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_warn(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_vbusov_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	struct chgdev_notify *noti = &(mci->chg_dev->noti);
	bool vbusov_stat = false;
	int ret = 0;
	unsigned int regval;

	dev_warn(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT2, &regval);
	if (ret < 0)
		goto out;
	vbusov_stat = (regval & BIT(7));
	noti->vbusov_stat = vbusov_stat;
	dev_info(mci->dev, "%s: stat = %d\n", __func__, vbusov_stat);
out:
#else
	dev_info(mci->dev, "%s\n", __func__);
#endif /* CONFIG_MTK_CHARGER */
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wd_pmu_det_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wd_pmu_done_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_tmri_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret = 0;
	unsigned int regval;

	dev_notice(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		return IRQ_HANDLED;
	dev_info(mci->dev, "%s: chg_stat4 = 0x%02x\n", __func__, ret);
	if (!(regval & MT6360_MASK_CHG_TMRI))
		return IRQ_HANDLED;

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_dev_notify(mci->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
#endif /* CONFIG_MTK_CHARGER */
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_aiccmeasl_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	complete(&mci->aicc_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_wdtmri_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	int ret;

	dev_warn(mci->dev, "%s\n", __func__);
	/* Any I2C R/W can kick watchdog timer */
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_CTRL1, NULL);
	if (ret < 0)
		dev_err(mci->dev, "%s: kick wdt failed\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_rechgi_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	power_supply_changed(mci->psy);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_termi_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chg_ieoci_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	bool ieoc_stat = false;
	int ret = 0;
	unsigned int regval;

	dev_dbg(mci->dev, "%s\n", __func__);
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT4, &regval);
	if (ret < 0)
		goto out;
	ieoc_stat = (regval & BIT(7));
	if (!ieoc_stat)
		goto out;
	power_supply_changed(mci->psy);
out:
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_pumpx_donei_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_info(mci->dev, "%s\n", __func__);
	atomic_set(&mci->pe_complete, 0);
	complete(&mci->pumpx_done);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_attach_i_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);

	dev_dbg(mci->dev, "%s\n", __func__);

	if (pdata->bc12_sel != 0)
		return IRQ_HANDLED;

	mutex_lock(&mci->chgdet_lock);
	if (!mci->bc12_en) {
		dev_err(mci->dev, "%s: bc12 disabled, ignore irq\n",
			__func__);
		goto out;
	}
	mt6360_chgdet_post_process(mci);
out:
	mutex_unlock(&mci->chgdet_lock);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_hvdcp_det_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_dcdti_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;

	dev_dbg(mci->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_chrdet_ext_evt_handler(int irq, void *data)
{
	struct mt6360_chg_info *mci = data;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	bool pwr_rdy = false;

	ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
	dev_info(mci->dev, "%s: pwr_rdy = %d\n", __func__, pwr_rdy);
	if (ret < 0)
		goto out;
	if (mci->pwr_rdy == pwr_rdy)
		goto out;
	mci->pwr_rdy = pwr_rdy;
	if (!IS_ENABLED(CONFIG_TCPC_CLASS) || pdata->bc12_sel != 0) {
		mutex_lock(&mci->chgdet_lock);
		(pwr_rdy ? mt6360_chgdet_pre_process :
			   mt6360_chgdet_post_process)(mci);
		mutex_unlock(&mci->chgdet_lock);
	}
	if (atomic_read(&mci->pe_complete) && pwr_rdy == true &&
	    mci->mpd->chip_rev <= 0x02) {
		dev_info(mci->dev, "%s: re-trigger pe20 pattern\n", __func__);
		queue_work(mci->pe_wq, &mci->pe_work);
	}
out:
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_chg_irq_desc[] = {
	{ "chg_treg_evt", mt6360_pmu_chg_treg_evt_handler },
	{ "chg_mivr_evt", mt6360_pmu_chg_mivr_evt_handler },
	{ "pwr_rdy_evt", mt6360_pmu_pwr_rdy_evt_handler },
	{ "chg_batsysuv_evt", mt6360_pmu_chg_batsysuv_evt_handler },
	{ "chg_vsysuv_evt", mt6360_pmu_chg_vsysuv_evt_handler },
	{ "chg_vsysov_evt", mt6360_pmu_chg_vsysov_evt_handler },
	{ "chg_vbatov_evt", mt6360_pmu_chg_vbatov_evt_handler },
	{ "chg_vbusov_evt", mt6360_pmu_chg_vbusov_evt_handler },
	{ "wd_pmu_det", mt6360_pmu_wd_pmu_det_handler },
	{ "wd_pmu_done", mt6360_pmu_wd_pmu_done_handler },
	{ "chg_tmri", mt6360_pmu_chg_tmri_handler },
	{ "chg_aiccmeasl", mt6360_pmu_chg_aiccmeasl_handler },
	{ "wdtmri", mt6360_pmu_wdtmri_handler },
	{ "chg_rechgi", mt6360_pmu_chg_rechgi_handler },
	{ "chg_termi", mt6360_pmu_chg_termi_handler },
	{ "chg_ieoci", mt6360_pmu_chg_ieoci_handler },
	{ "pumpx_donei", mt6360_pmu_pumpx_donei_handler },
	{ "attach_i", mt6360_pmu_attach_i_handler },
	{ "hvdcp_det", mt6360_pmu_hvdcp_det_handler },
	{ "dcdti", mt6360_pmu_dcdti_handler },
	{ "chrdet_ext_evt", mt6360_pmu_chrdet_ext_evt_handler },
};

static void mt6360_pmu_chg_irq_enable(const char *name, int en)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i = 0;

	if (unlikely(!name))
		return;
	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_chg_irq_desc); i++) {
		irq_desc = mt6360_pmu_chg_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		if (!strcmp(irq_desc->name, name)) {
			if (en)
				enable_irq(irq_desc->irq);
			else
				disable_irq_nosync(irq_desc->irq);
			break;
		}
	}
}

static void mt6360_pmu_chg_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_chg_irq_desc); i++) {
		irq_desc = mt6360_pmu_chg_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static int mt6360_toggle_cfo(struct mt6360_chg_info *mci)
{
	struct mt6360_pmu_data *mpd = mci->mpd;
	int ret = 0;
	u8 data = 0;

	mutex_lock(&mpd->io_lock);
	/* check if strobe mode */
	ret = i2c_smbus_read_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
						  MT6360_PMU_FLED_EN, 1, &data);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read cfo fail\n", __func__);
		goto out;
	}
	if (data & MT6360_MASK_STROBE_EN) {
		dev_err(mci->dev, "%s: fled in strobe mode\n", __func__);
		goto out;
	}
	/* read data */
	ret = i2c_smbus_read_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
						MT6360_PMU_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read cfo fail\n", __func__);
		goto out;
	}
	/* cfo off */
	data &= ~MT6360_MASK_CFO_EN;
	ret = i2c_smbus_write_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
						MT6360_PMU_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		dev_err(mci->dev, "%s: clear cfo fail\n", __func__);
		goto out;
	}
	/* cfo on */
	data |= MT6360_MASK_CFO_EN;
	ret = i2c_smbus_write_i2c_block_data(mpd->i2c[MT6360_SLAVE_PMU],
						MT6360_PMU_CHG_CTRL2, 1, &data);
	if (ret < 0)
		dev_err(mci->dev, "%s: set cfo fail\n", __func__);
out:
	mutex_unlock(&mpd->io_lock);
	return ret;
}

static int mt6360_chg_mivr_task_threadfn(void *data)
{
	struct mt6360_chg_info *mci = data;
	u32 ibus;
	int ret;
	unsigned int regval;

	dev_info(mci->dev, "%s ++\n", __func__);
	while (!kthread_should_stop()) {
		wait_event(mci->mivr_wq, atomic_read(&mci->mivr_cnt) > 0 ||
							 kthread_should_stop());
		mt_dbg(mci->dev, "%s: enter mivr thread\n", __func__);
		if (kthread_should_stop())
			break;
		pm_stay_awake(mci->dev);
		/* check real mivr stat or not */
		ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT1, &regval);
		if (ret < 0)
			goto loop_cont;
		if (!(regval & MT6360_MASK_MIVR_EVT)) {
			mt_dbg(mci->dev, "%s: mivr stat not act\n", __func__);
			goto loop_cont;
		}
		/* read ibus adc */
		ret = __mt6360_get_adc(mci, MT6360_ADC_IBUS, &ibus, &ibus);
		if (ret < 0) {
			dev_err(mci->dev, "%s: get ibus adc fail\n", __func__);
			goto loop_cont;
		}
		/* if ibus adc value < 100mA), toggle cfo */
		if (ibus < 100000) {
			dev_dbg(mci->dev, "%s: enter toggle cfo\n", __func__);
			ret = mt6360_toggle_cfo(mci);
			if (ret < 0)
				dev_err(mci->dev,
					"%s: toggle cfo fail\n", __func__);
		}
loop_cont:
		pm_relax(mci->dev);
		atomic_set(&mci->mivr_cnt, 0);
		mt6360_pmu_chg_irq_enable("chg_mivr_evt", 1);
		msleep(200);
	}
	dev_info(mci->dev, "%s --\n", __func__);
	return 0;
}

static void mt6360_trigger_pep_work_handler(struct work_struct *work)
{
	struct mt6360_chg_info *mci =
		(struct mt6360_chg_info *)container_of(work,
		struct mt6360_chg_info, pe_work);
	int ret = 0;

	ret = __mt6360_set_pep20_current_pattern(mci, 5000000);
	if (ret < 0)
		dev_err(mci->dev, "%s: trigger pe20 pattern fail\n",
			__func__);
}

static void mt6360_chgdet_work_handler(struct work_struct *work)
{
	int ret = 0;
	bool pwr_rdy = false;
	struct mt6360_chg_info *mci =
		(struct mt6360_chg_info *)container_of(work,
		struct mt6360_chg_info, chgdet_work);

	mutex_lock(&mci->chgdet_lock);
	/* Check PWR_RDY_STAT */
	ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
	if (ret < 0)
		goto out;
	/* power not good */
	if (!pwr_rdy)
		goto out;
	/* power good */
	mci->pwr_rdy = pwr_rdy;
	/* Turn on USB charger detection */
	ret = __mt6360_enable_usbchgen(mci, true);
	if (ret < 0)
		dev_err(mci->dev, "%s: en bc12 fail\n", __func__);
out:
	mutex_unlock(&mci->chgdet_lock);
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(ichg, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL7, 2, 0xFC,
			     mt6360_trans_ichg_sel, 0),
	MT6360_PDATA_VALPROP(aicr, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL3, 2, 0xFC,
			     mt6360_trans_aicr_sel, 0),
	MT6360_PDATA_VALPROP(mivr, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL6, 1, 0xFE,
			     mt6360_trans_mivr_sel, 0),
	MT6360_PDATA_VALPROP(cv, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL4, 1, 0xFE,
			     mt6360_trans_cv_sel, 0),
	MT6360_PDATA_VALPROP(ieoc, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL9, 4, 0xF0,
			     mt6360_trans_ieoc_sel, 0),
	MT6360_PDATA_VALPROP(safety_timer, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL12, 5, 0xE0,
			     mt6360_trans_safety_timer_sel, 0),
	MT6360_PDATA_VALPROP(ircmp_resistor, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL18, 3, 0x38,
			     mt6360_trans_ircmp_r_sel, 0),
	MT6360_PDATA_VALPROP(ircmp_vclamp, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL18, 0, 0x07,
			     mt6360_trans_ircmp_vclamp_sel, 0),
	MT6360_PDATA_VALPROP(aicc_once, struct mt6360_chg_platform_data,
			     MT6360_PMU_CHG_CTRL14, 0, 0x04, NULL, 0),
};

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(ichg, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(aicr, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(mivr, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(cv, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(ieoc, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(safety_timer, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(ircmp_resistor, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(ircmp_vclamp, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(en_te, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(en_wdt, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(aicc_once, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(post_aicc, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(batoc_notify, struct mt6360_chg_platform_data),
	MT6360_DT_VALPROP(bc12_sel, struct mt6360_chg_platform_data),
};

static int mt6360_enable_ilim(struct mt6360_chg_info *mci, bool en)
{
	return regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL3,
				  MT6360_MASK_ILIM_EN, en ? 0xff : 0);
}

static void mt6360_get_boot_mode(struct mt6360_chg_info *mci)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(mci->dev->of_node, "bootmode", 0);
	if (!boot_node)
		dev_info(mci->dev, "%s: failed to get boot mode phandle\n",
			 __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			dev_info(mci->dev, "%s: failed to get atag,boot\n",
				 __func__);
		else {
			dev_info(mci->dev,
			"%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			mci->bootmode = tag->bootmode;
			mci->boottype = tag->boottype;
		}
	}
}


static int mt6360_chg_init_setting(struct mt6360_chg_info *mci)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	unsigned int regval;

	dev_info(mci->dev, "%s\n", __func__);
	/*get boot mode*/
	mt6360_get_boot_mode(mci);
	ret = regmap_read(mci->regmap, MT6360_PMU_FOD_STAT, &regval);
	if (ret >= 0)
		mci->ctd_dischg_status = regval & 0xE3;
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_FOD_CTRL, 0x40, 0);
	if (ret < 0)
		dev_err(mci->dev, "%s: disable fod ctrl fail\n", __func__);
	ret = mt6360_select_input_current_limit(mci, MT6360_IINLMTSEL_AICR);
	if (ret < 0) {
		dev_err(mci->dev, "%s: select iinlmtsel by aicr fail\n",
			__func__);
		return ret;
	}
	usleep_range(5000, 6000);
	ret = mt6360_enable_ilim(mci, false);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable ilim fail\n", __func__);
		return ret;
	}
	if (mci->bootmode == 1 || mci->bootmode == 5) {
		/*1:META_BOOT 5:ADVMETA_BOOT*/
		ret = regmap_update_bits(mci->regmap,
					 MT6360_PMU_CHG_CTRL3,
					 MT6360_MASK_AICR,
					 0x02 << MT6360_SHFT_AICR);
		dev_info(mci->dev, "%s: set aicr to 200mA in meta mode\n",
			__func__);
	}

	/* disable wdt reduce 1mA power consumption */
	ret = mt6360_enable_wdt(mci, false);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable wdt fail\n", __func__);
		return ret;
	}
	/* Disable USB charger type detect, no matter use it or not */
	ret = mt6360_enable_usbchgen(mci, false);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable chg type detect fail\n",
			__func__);
		return ret;
	}
	/* unlock ovp limit for pump express, can be replaced by option */
	ret = mt6360_select_vinovp(mci, 14500000);
	if (ret < 0) {
		dev_err(mci->dev, "%s: unlimit vin for pump express\n",
			__func__);
		return ret;
	}
	/* Disable TE, set TE when plug in/out */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL2,
				 MT6360_MASK_TE_EN, 0);
	if (ret < 0) {
		dev_err(mci->dev, "%s: disable te fail\n", __func__);
		return ret;
	}
	/* Read ZCV */
	ret = mt6360_read_zcv(mci);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read zcv fail\n", __func__);
		return ret;
	}
	/* enable AICC_EN if aicc_once = 0 */
	if (!pdata->aicc_once) {
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_CTRL14,
					 MT6360_MASK_RG_EN_AICC, 0xff);
		if (ret < 0) {
			dev_err(mci->dev,
				"%s: enable en_aicc fail\n", __func__);
			return ret;
		}
	}
#ifndef CONFIG_MT6360_DCDTOUT_SUPPORT
	/* Disable DCD */
	ret = mt6360_enable_dcd_tout(mci, false);
	if (ret < 0)
		dev_notice(mci->dev, "%s disable dcd fail\n", __func__);
#endif
	/* Check BATSYSUV occurred last time boot-on */
	ret = regmap_read(mci->regmap, MT6360_PMU_CHG_STAT, &regval);
	if (ret < 0) {
		dev_err(mci->dev, "%s: read BATSYSUV fail\n", __func__);
		return ret;
	}
	if (!(ret & MT6360_MASK_CHG_BATSYSUV)) {
		dev_warn(mci->dev, "%s: BATSYSUV occurred\n", __func__);
		ret = regmap_update_bits(mci->regmap, MT6360_PMU_CHG_STAT,
					 MT6360_MASK_CHG_BATSYSUV, 0xff);
		if (ret < 0)
			dev_err(mci->dev,
				"%s: clear BATSYSUV fail\n", __func__);
	}

	/* USBID ID_TD = 32T */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_USBID_CTRL2,
				 MT6360_MASK_IDTD | MT6360_MASK_USBID_FLOAT,
				 0x62);
	/* Disable TypeC OTP for check EVB version by TS pin */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_TYPEC_OTP_CTRL,
				 MT6360_MASK_TYPEC_OTP_EN, 0);
	return ret;
}

static int mt6360_set_shipping_mode(struct mt6360_chg_info *mci)
{
	int ret;

	dev_info(mci->dev, "%s\n", __func__);
	/* disable shipping mode rst */
	ret = regmap_update_bits(mci->regmap, MT6360_PMU_CORE_CTRL2,
				 MT6360_MASK_SHIP_RST_DIS, 0xff);
	if (ret < 0)
		dev_err(mci->dev,
			"%s: fail to disable ship reset\n", __func__);
		goto out;
	/* enter shipping mode and disable cfo_en/chg_en */
	ret = regmap_write(mci->regmap, MT6360_PMU_CHG_CTRL2, 0x80);
	if (ret < 0)
		dev_err(mci->dev,
			"%s: fail to enter shipping mode\n", __func__);
out:
	return ret;
}

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct mt6360_chg_info *mci = dev_get_drvdata(dev);
	int32_t tmp = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &tmp) < 0) {
		dev_notice(dev, "parsing number fail\n");
		return -EINVAL;
	}
	if (tmp != 5526789)
		return -EINVAL;
	ret = mt6360_set_shipping_mode(mci);
	if (ret < 0)
		return ret;
	return count;
}
static const DEVICE_ATTR_WO(shipping_mode);

#if FIXME /* TODO: wait mtk_charger_intf.h */
void mt6360_recv_batoc_callback(BATTERY_OC_LEVEL tag)
{
	int ret, cnt = 0;

	if (tag != BATTERY_OC_LEVEL_1)
		return;
	while (!pmic_get_register_value(PMIC_RG_INT_STATUS_FG_CUR_H)) {
		if (cnt >= 1) {
			ret = mt6360_set_shipping_mode(g_mci);
			if (ret < 0)
				dev_err(g_mci->dev,
					"%s: set shipping mode fail\n",
					__func__);
			else
				dev_info(g_mci->dev,
					 "%s: set shipping mode done\n",
					 __func__);
		}
		mdelay(8);
		cnt++;
	}
	dev_info(g_mci->dev, "%s exit, cnt = %d, FG_CUR_H = %d\n",
		 __func__, cnt,

}
#endif
/* ======================= */
/* MT6360 Power Supply Ops */
/* ======================= */
static int mt6360_charger_get_online(struct mt6360_chg_info *mci,
				     union power_supply_propval *val)
{
	int ret;
	bool pwr_rdy;

	if (IS_ENABLED(CONFIG_TCPC_CLASS)) {
		mutex_lock(&mci->attach_lock);
		pwr_rdy = mci->attach;
		mutex_unlock(&mci->attach_lock);
	} else {
		/*uvp_d_stat=true => vbus_on=1*/
		ret = mt6360_get_chrdet_ext_stat(mci, &pwr_rdy);
		if (ret < 0) {
			dev_notice(mci->dev,
				"%s: read uvp_d_stat fail\n", __func__);
			return ret;
		}
	}
	dev_info(mci->dev, "%s: online = %d\n", __func__, pwr_rdy);
	val->intval = pwr_rdy;
	return 0;
}

static int mt6360_charger_set_online(struct mt6360_chg_info *mci,
				     const union power_supply_propval *val)
{
	return __mt6360_enable_chg_type_det(mci, val->intval);
}

static int mt6360_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct mt6360_chg_info *mci = power_supply_get_drvdata(psy);
	enum mt6360_charging_status chg_stat = MT6360_CHG_STATUS_MAX;
	int ret = 0;

	dev_dbg(mci->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = mt6360_charger_get_online(mci, val);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = mci->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = mt6360_get_charging_status(mci, &chg_stat);
		if (ret < 0)
			dev_info(mci->dev,
				"%s: get mt6360 chg_status failed\n", __func__);
		switch (chg_stat) {
		case MT6360_CHG_STATUS_READY:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case MT6360_CHG_STATUS_PROGRESS:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case MT6360_CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		case MT6360_CHG_STATUS_FAULT:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int mt6360_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct mt6360_chg_info *mci = power_supply_get_drvdata(psy);
	int ret;

	dev_dbg(mci->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = mt6360_charger_set_online(mci, val);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int mt6360_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property mt6360_charger_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc mt6360_charger_desc = {
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= mt6360_charger_properties,
	.num_properties		= ARRAY_SIZE(mt6360_charger_properties),
	.get_property		= mt6360_charger_get_property,
	.set_property		= mt6360_charger_set_property,
	.property_is_writeable	= mt6360_charger_property_is_writeable,
	.usb_types		= mt6360_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(mt6360_charger_usb_types),
};

static char *mt6360_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};
/*otg_vbus*/
static int mt6360_boost_enable(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);

	return __mt6360_enable_otg(mci, true);
}

static int mt6360_boost_disable(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);

	return __mt6360_enable_otg(mci, false);
}

static int mt6360_boost_is_enabled(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int ret = 0;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->enable_reg, &regval);

	if (ret < 0)
		return ret;
	return regval & desc->enable_mask ? true : false;
}

static int mt6360_boost_set_voltage_sel(struct regulator_dev *rdev,
					unsigned int sel)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->vsel_mask) - 1;

	return regmap_update_bits(mci->regmap, desc->enable_reg,
				  desc->enable_mask, sel << shift);
}

static int mt6360_boost_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->vsel_mask) - 1, ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->vsel_reg, &regval);
	if (ret < 0)
		return ret;
	return (regval & desc->vsel_mask) >> shift;
}

static int mt6360_boost_set_current_limit(struct regulator_dev *rdev,
					  int min_uA, int max_uA)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int i, shift = ffs(desc->csel_mask) - 1;

	for (i = 0; i < ARRAY_SIZE(mt6360_otg_oc_threshold); i++) {
		if (min_uA <= mt6360_otg_oc_threshold[i])
			break;
	}
	if (i == ARRAY_SIZE(mt6360_otg_oc_threshold) ||
		mt6360_otg_oc_threshold[i] > max_uA) {
		dev_notice(mci->dev,
			"%s: out of current range\n", __func__);
		return -EINVAL;
	}
	dev_info(mci->dev, "%s: select otg_oc = %d\n",
		 __func__, mt6360_otg_oc_threshold[i]);
	return regmap_update_bits(mci->regmap, desc->csel_reg,
				  desc->csel_mask, i << shift);
}

static int mt6360_boost_get_current_limit(struct regulator_dev *rdev)
{
	struct mt6360_chg_info *mci = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int shift = ffs(desc->csel_mask) - 1, ret;
	unsigned int regval;

	ret = regmap_read(mci->regmap, desc->csel_reg, &regval);
	if (ret < 0)
		return ret;
	ret = (regval & desc->csel_mask) >> shift;
	if (ret > ARRAY_SIZE(mt6360_otg_oc_threshold))
		return -EINVAL;
	return mt6360_otg_oc_threshold[ret];
}

static const struct regulator_ops mt6360_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = mt6360_boost_enable,
	.disable = mt6360_boost_disable,
	.is_enabled = mt6360_boost_is_enabled,
	.set_voltage_sel = mt6360_boost_set_voltage_sel,
	.get_voltage_sel = mt6360_boost_get_voltage_sel,
	.set_current_limit = mt6360_boost_set_current_limit,
	.get_current_limit = mt6360_boost_get_current_limit,
};

static const struct regulator_desc mt6360_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &mt6360_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.min_uV = 4425000,
	.uV_step = 25000, /* 25mV per step */
	.n_voltages = 57, /* 4425mV to 5825mV */
	.vsel_reg = MT6360_PMU_CHG_CTRL5,
	.vsel_mask = MT6360_MASK_BOOST_VOREG,
	.enable_reg = MT6360_PMU_CHG_CTRL1,
	.enable_mask = MT6360_MASK_OPA_MODE,
	.csel_reg = MT6360_PMU_CHG_CTRL10,
	.csel_mask = MT6360_MASK_OTG_OC,
};

static int mt6360_get_charger_type(struct mt6360_chg_info *mci,
	bool attach)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	union power_supply_propval prop, prop2;
	static struct power_supply *chg_psy;
	int ret = 0;

	if (chg_psy == NULL) {
		if (pdata->bc12_sel == 1)
			chg_psy = power_supply_get_by_name("mtk_charger_type");
		else if (pdata->bc12_sel == 2)
			chg_psy = power_supply_get_by_name("ext_charger_type");
	}

	if (IS_ERR_OR_NULL(chg_psy))
		pr_notice("%s Couldn't get chg_psy\n", __func__);
	else {
		prop.intval = attach;
		if (attach) {
			ret = power_supply_set_property(chg_psy,
					POWER_SUPPLY_PROP_ONLINE, &prop);
			ret = power_supply_get_property(chg_psy,
					POWER_SUPPLY_PROP_USB_TYPE, &prop2);
		} else
			prop2.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;

		pr_notice("%s type:%d\n", __func__, prop2.intval);

		switch (prop2.intval) {
		case POWER_SUPPLY_USB_TYPE_UNKNOWN:
			mci->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			break;
		case POWER_SUPPLY_USB_TYPE_SDP:
			mci->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			break;
		case POWER_SUPPLY_USB_TYPE_CDP:
			mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			break;
		case POWER_SUPPLY_USB_TYPE_DCP:
			mci->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			mci->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		}
		power_supply_changed(mci->psy);
	}
	return prop2.intval;
}

static int typec_attach_thread(void *data)
{
	struct mt6360_chg_info *mci = data;
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(mci->dev);
	int ret = 0;
	bool attach;
	union power_supply_propval val;

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		wait_event(mci->attach_wq,
			   atomic_read(&mci->chrdet_start) > 0 ||
							 kthread_should_stop());
		if (kthread_should_stop())
			break;
		mutex_lock(&mci->attach_lock);
		attach = mci->typec_attach;
		mutex_unlock(&mci->attach_lock);
		val.intval = attach;
		pr_notice("%s bc12_sel:%d attach:%d\n", __func__,
				pdata->bc12_sel, attach);
		if (pdata->bc12_sel == 0) {
			ret = power_supply_set_property(mci->chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &val);
			if (ret < 0)
				dev_info(mci->dev, "%s: set online fail(%d)\n",
					__func__, ret);
		} else
			mt6360_get_charger_type(mci, attach);
		atomic_set(&mci->chrdet_start, 0);
	}
	return ret;
}

static void handle_typec_attach(struct mt6360_chg_info *mci,
				bool en)
{
	mutex_lock(&mci->attach_lock);
	mci->typec_attach = en;
	atomic_inc(&mci->chrdet_start);
	wake_up(&mci->attach_wq);
	mutex_unlock(&mci->attach_lock);
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mt6360_chg_info *chg_data =
		(struct mt6360_chg_info *)container_of(nb,
		struct mt6360_chg_info, pd_nb);

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			handle_typec_attach(chg_data, true);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("%s USB Plug out\n", __func__);
			if (chg_data->tcpc_kpoc) {
				pr_info("%s: typec unattached, power off\n",
					__func__);
				kernel_power_off();
			}
			handle_typec_attach(chg_data, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			handle_typec_attach(chg_data, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			handle_typec_attach(chg_data, false);
		}
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

static int mt6360_pmu_chg_probe(struct platform_device *pdev)
{
	struct mt6360_chg_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_chg_info *mci;
	struct iio_channel *channel;
	struct power_supply_config charger_cfg = {};
	struct regulator_config config = { };
	struct device_node *np = pdev->dev.of_node;
	int i, ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	if (np) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &def_platform_data, sizeof(*pdata));
		mt6360_dt_parser_helper(np, (void *)pdata,
					mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mci = devm_kzalloc(&pdev->dev, sizeof(*mci), GFP_KERNEL);
	if (!mci)
		return -ENOMEM;
	mci->dev = &pdev->dev;
	mci->mpd = dev_get_drvdata(pdev->dev.parent);
	mci->hidden_mode_cnt = 0;
	mutex_init(&mci->hidden_mode_lock);
	mutex_init(&mci->pe_lock);
	mutex_init(&mci->aicr_lock);
	mutex_init(&mci->chgdet_lock);
	mutex_init(&mci->tchg_lock);
	mutex_init(&mci->ichg_lock);
	mci->tchg = 0;
	mci->ichg = 2000000;
	mci->ichg_dis_chg = 2000000;
	mci->attach = false;
	g_mci = mci;
	init_completion(&mci->aicc_done);
	init_completion(&mci->pumpx_done);
	atomic_set(&mci->pe_complete, 0);
	atomic_set(&mci->mivr_cnt, 0);
	init_waitqueue_head(&mci->mivr_wq);
	if (IS_ENABLED(CONFIG_TCPC_CLASS)) {
		init_waitqueue_head(&mci->attach_wq);
		atomic_set(&mci->chrdet_start, 0);
		mutex_init(&mci->attach_lock);
	} else if (pdata->bc12_sel == 0)
		INIT_WORK(&mci->chgdet_work, mt6360_chgdet_work_handler);

	platform_set_drvdata(pdev, mci);

	/* get parent regmap */
	mci->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!mci->regmap) {
		dev_err(&pdev->dev, "Fail to get parent regmap\n");
		ret = -ENODEV;
		goto err_mutex_init;
	}
	/* apply platform data */
	ret = mt6360_pdata_apply_helper(mci->regmap, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0) {
		dev_err(&pdev->dev, "apply pdata fail\n");
		goto err_mutex_init;
	}
	/* Initial Setting */
	ret = mt6360_chg_init_setting(mci);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: init setting fail\n", __func__);
		goto err_mutex_init;
	}

	/* Get ADC iio channels */
	for (i = 0; i < MT6360_ADC_MAX; i++) {
		channel = devm_iio_channel_get(&pdev->dev,
					       mt6360_adc_chan_list[i]);
		if (IS_ERR(channel)) {
			dev_err(&pdev->dev, "%s: iio channel get fail\n", __func__);
			ret = PTR_ERR(channel);
			goto err_mutex_init;
		}
		mci->channels[i] = channel;
	}

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	/* charger class register */
	mci->chg_dev = charger_device_register(pdata->chg_name, mci->dev,
						mci, &mt6360_chg_ops,
						&mt6360_chg_props);
	if (IS_ERR(mci->chg_dev)) {
		dev_err(mci->dev, "charger device register fail\n");
		ret = PTR_ERR(mci->chg_dev);
		goto err_mutex_init;
	}
#endif /* CONFIG_MTK_CHARGER */

	/* irq register */
	mt6360_pmu_chg_irq_register(pdev);
	device_init_wakeup(&pdev->dev, true);
	/* mivr task */
	mci->mivr_task = kthread_run(mt6360_chg_mivr_task_threadfn, mci,
				      devm_kasprintf(mci->dev, GFP_KERNEL,
				      "mivr_thread.%s", dev_name(mci->dev)));
	ret = PTR_ERR_OR_ZERO(mci->mivr_task);
	if (ret < 0) {
		dev_err(mci->dev, "create mivr handling thread fail\n");
		goto err_register_chg_dev;
	}
	ret = device_create_file(mci->dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_notice(&pdev->dev, "create shipping attr fail\n");
		goto err_create_mivr_thread_run;
	}
	/* for trigger unfinish pe pattern */
	mci->pe_wq = create_singlethread_workqueue("pe_pattern");
	if (!mci->pe_wq) {
		dev_err(mci->dev, "%s: create pe_pattern work queue fail\n",
			__func__);
		goto err_shipping_mode_attr;
	}
	INIT_WORK(&mci->pe_work, mt6360_trigger_pep_work_handler);

	/* register fg bat oc notify */
#if FIXME /* without mtk_charger_intf  BATTERY_OC_LEVEL */
	if (pdata->batoc_notify)
		register_battery_oc_notify(&mt6360_recv_batoc_callback,
					   BATTERY_OC_PRIO_CHARGER);
#endif

	/* otg regulator */
	config.dev = &pdev->dev;
	config.driver_data = mci;
	mci->otg_rdev = devm_regulator_register(&pdev->dev,
						&mt6360_otg_rdesc, &config);
	if (IS_ERR(mci->otg_rdev)) {
		ret = PTR_ERR(mci->otg_rdev);
		goto err_register_otg;
	}


	/* power supply register */
	memcpy(&mci->psy_desc,
		&mt6360_charger_desc, sizeof(mci->psy_desc));
	mci->psy_desc.name = dev_name(&pdev->dev);

	charger_cfg.drv_data = mci;
	charger_cfg.of_node = pdev->dev.of_node;
	charger_cfg.supplied_to = mt6360_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(mt6360_charger_supplied_to);
	mci->psy = devm_power_supply_register(&pdev->dev,
					      &mci->psy_desc, &charger_cfg);
	if (IS_ERR(mci->psy)) {
		dev_notice(&pdev->dev, "Fail to register power supply dev\n");
		ret = PTR_ERR(mci->psy);
		goto err_register_psy;
	}

	/* get bc1.2 power supply: chg_psy */
	mci->chg_psy = devm_power_supply_get_by_phandle(&pdev->dev, "charger");
	if (IS_ERR(mci->chg_psy)) {
		dev_notice(&pdev->dev, "Failed to get charger psy\n");
		ret = PTR_ERR(mci->chg_psy);
		goto err_psy_get_phandle;
	}

	if (!IS_ENABLED(CONFIG_TCPC_CLASS))
		goto bypass_tcpc_init;

	mci->attach_task = kthread_run(typec_attach_thread, mci,
					"attach_thread");
	if (IS_ERR(mci->attach_task)) {
		ret = PTR_ERR(mci->attach_task);
		goto err_attach_task;
	}

	mci->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!mci->tcpc_dev) {
		pr_notice("%s get tcpc device type_c_port0 fail\n", __func__);
		ret = -ENODEV;
		goto err_get_tcpcdev;
	}

	mci->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(mci->tcpc_dev, &mci->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_register_tcp_notifier;
	}

bypass_tcpc_init:
	/* Schedule work for microB's BC1.2 */
	if (!IS_ENABLED(CONFIG_TCPC_CLASS) && pdata->bc12_sel == 0)
		schedule_work(&mci->chgdet_work);
	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;
err_register_tcp_notifier:
err_get_tcpcdev:
	if (mci->attach_task)
		kthread_stop(mci->attach_task);
err_attach_task:
err_psy_get_phandle:
err_register_psy:
err_register_otg:
	destroy_workqueue(mci->pe_wq);
err_shipping_mode_attr:
	device_remove_file(mci->dev, &dev_attr_shipping_mode);
err_create_mivr_thread_run:
	if (mci->mivr_task)
		kthread_stop(mci->mivr_task);
err_register_chg_dev:
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_device_unregister(mci->chg_dev);
#endif /* CONFIG_MTK_CHARGER */
err_mutex_init:
	mutex_destroy(&mci->tchg_lock);
	mutex_destroy(&mci->chgdet_lock);
	mutex_destroy(&mci->aicr_lock);
	mutex_destroy(&mci->pe_lock);
	mutex_destroy(&mci->hidden_mode_lock);
	mutex_destroy(&mci->attach_lock);
	return -EPROBE_DEFER;
}

static int mt6360_pmu_chg_remove(struct platform_device *pdev)
{
	struct mt6360_chg_info *mci = platform_get_drvdata(pdev);

	dev_dbg(mci->dev, "%s\n", __func__);
	flush_workqueue(mci->pe_wq);
	destroy_workqueue(mci->pe_wq);
	if (mci->mivr_task) {
		atomic_inc(&mci->mivr_cnt);
		wake_up(&mci->mivr_wq);
		kthread_stop(mci->mivr_task);
	}
	if (IS_ENABLED(CONFIG_TCPC_CLASS) && mci->attach_task)
		kthread_stop(mci->attach_task);
	mutex_destroy(&mci->attach_lock);
	device_remove_file(mci->dev, &dev_attr_shipping_mode);
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	charger_device_unregister(mci->chg_dev);
#endif /* CONFIG_MTK_CHARGER */
	mutex_destroy(&mci->tchg_lock);
	mutex_destroy(&mci->chgdet_lock);
	mutex_destroy(&mci->aicr_lock);
	mutex_destroy(&mci->pe_lock);
	mutex_destroy(&mci->hidden_mode_lock);
	return 0;
}

static int __maybe_unused mt6360_pmu_chg_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_chg_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_chg_pm_ops,
			 mt6360_pmu_chg_suspend, mt6360_pmu_chg_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_chg_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_chg", },
	{ .compatible = "mediatek,mt6360_chg", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_chg_of_id);

static const struct platform_device_id mt6360_pmu_chg_id[] = {
	{ "mt6360_pmu_chg", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_chg_id);

static struct platform_driver mt6360_pmu_chg_driver = {
	.driver = {
		.name = "mt6360_pmu_chg",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_chg_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_chg_of_id),
	},
	.probe = mt6360_pmu_chg_probe,
	.remove = mt6360_pmu_chg_remove,
	.id_table = mt6360_pmu_chg_id,
};

static int __init mt6360_pmu_chg_init(void)
{
	return platform_driver_register(&mt6360_pmu_chg_driver);
}
device_initcall_sync(mt6360_pmu_chg_init);

static void __exit mt6360_pmu_chg_exit(void)
{
	platform_driver_unregister(&mt6360_pmu_chg_driver);
}
module_exit(mt6360_pmu_chg_exit);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU CHG Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MT6360_PMU_CHG_DRV_VERSION);

/*
 * Version Note
 * 1.0.7_MTK
 * (1) Fix Unbalanced enable for MIVR IRQ
 * (2) Sleep 200ms before do another iteration in mt6360_chg_mivr_task_threadfn
 *
 * 1.0.6_MTK
 * (1) Fix the usages of charger power supply
 *
 * 1.0.5_MTK
 * (1) Prevent charger type infromed repeatedly
 *
 * 1.0.4_MTK
 * (1) Mask mivr irq until mivr task has run an iteration
 *
 * 1.0.3_MTK
 * (1) fix zcv adc from 5mV to 1.25mV per step
 * (2) add BC12 initial setting dcd timeout disable when unuse dcd
 *
 * 1.0.2_MTK
 * (1) remove eoc, rechg, te irq for evb with phone load
 * (2) report power supply online with chg type detect done
 * (3) remove unused irq event and status
 * (4) add chg termination irq notifier when safety timer timeout
 *
 * 1.0.1_MTK
 * (1) fix dtsi parse attribute about en_te, en_wdt, aicc_once
 * (2) add charger class get vbus adc interface
 * (3) add initial setting about disable en_sdi, and check batsysuv.
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */

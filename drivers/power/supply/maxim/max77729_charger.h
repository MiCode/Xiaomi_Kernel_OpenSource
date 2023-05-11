/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MAX77729_CHARGER_H
#define __MAX77729_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/mfd/max77729.h>
#include <linux/mfd/max77729-private.h>
#include <linux/regulator/machine.h>
#include <linux/pm_wakeup.h>
#include "linux/mfd/max77729_common.h"
#include <linux/pmic-voter.h>

enum sec_otg_attrs {
	OTG_SEC_TYPE = 0,
};

ssize_t sec_otg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

#define SEC_OTG_ATTR(_name)					\
{									\
	.attr = {.name = #_name, .mode = 0444},	\
	.show = sec_otg_show_attrs,				\
	.store = NULL,				\
}

enum {
	CHIP_ID = 0,
	DATA,
};

enum {
	SHIP_MODE_DISABLE = 0,
	SHIP_MODE_EN_OP,
	SHIP_MODE_EN,
};

ssize_t max77729_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max77729_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define MAX77729_CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = max77729_chg_show_attrs,			\
	.store = max77729_chg_store_attrs,			\
}

#define MAX77729_CHG_SAFEOUT2                0x80

/* MAX77729_CHG_REG_CHG_INT */
#define MAX77729_BYP_I                  (1 << 0)
#define MAX77729_INP_LIMIT_I		(1 << 1)
#define MAX77729_BATP_I                 (1 << 2)
#define MAX77729_BAT_I                  (1 << 3)
#define MAX77729_CHG_I                  (1 << 4)
#define MAX77729_WCIN_I                 (1 << 5)
#define MAX77729_CHGIN_I                (1 << 6)
#define MAX77729_AICL_I                 (1 << 7)

/* MAX77729_CHG_REG_CHG_INT_MASK */
#define MAX77729_BYP_IM                 (1 << 0)
#define MAX77729_INP_LIMIT_IM		(1 << 1)
#define MAX77729_BATP_IM                (1 << 2)
#define MAX77729_BAT_IM                 (1 << 3)
#define MAX77729_CHG_IM                 (1 << 4)
#define MAX77729_WCIN_IM                (1 << 5)
#define MAX77729_CHGIN_IM               (1 << 6)
#define MAX77729_AICL_IM                (1 << 7)

/* MAX77729_CHG_REG_CHG_INT_OK */
#define MAX77729_BYP_OK                 0x01
#define MAX77729_BYP_OK_SHIFT           0
#define MAX77729_DISQBAT_OK		0x02
#define MAX77729_DISQBAT_OK_SHIFT	1
#define MAX77729_BATP_OK		0x04
#define MAX77729_BATP_OK_SHIFT		2
#define MAX77729_BAT_OK                 0x08
#define MAX77729_BAT_OK_SHIFT           3
#define MAX77729_CHG_OK                 0x10
#define MAX77729_CHG_OK_SHIFT           4
#define MAX77729_WCIN_OK		0x20
#define MAX77729_WCIN_OK_SHIFT		5
#define MAX77729_CHGIN_OK               0x40
#define MAX77729_CHGIN_OK_SHIFT         6
#define MAX77729_AICL_OK                0x80
#define MAX77729_AICL_OK_SHIFT          7
#define MAX77729_DETBAT                 0x04
#define MAX77729_DETBAT_SHIFT           2

/* MAX77729_CHG_REG_CHG_DTLS_00 */
#define MAX77729_BATP_DTLS		0x01
#define MAX77729_BATP_DTLS_SHIFT	0
#define MAX77729_WCIN_DTLS		0x18
#define MAX77729_WCIN_DTLS_SHIFT	3
#define MAX77729_CHGIN_DTLS             0x60
#define MAX77729_CHGIN_DTLS_SHIFT       5

/* MAX77729_CHG_REG_CHG_DTLS_01 */
#define MAX77729_CHG_DTLS               0x0F
#define MAX77729_CHG_DTLS_SHIFT         0
#define MAX77729_BAT_DTLS               0x70
#define MAX77729_BAT_DTLS_SHIFT         4

/* MAX77729_CHG_REG_CHG_DTLS_02 */
#define MAX77729_BYP_DTLS               0x0F
#define MAX77729_BYP_DTLS_SHIFT         0
#define MAX77729_BYP_DTLS0      0x1
#define MAX77729_BYP_DTLS1      0x2
#define MAX77729_BYP_DTLS2      0x4
#define MAX77729_BYP_DTLS3      0x8

#if 1
/* MAX77729_CHG_REG_CHG_CNFG_00 */
#define CHG_CNFG_00_MODE_SHIFT		        0
#define CHG_CNFG_00_CHG_SHIFT		        0
#define CHG_CNFG_00_UNO_SHIFT		        1
#define CHG_CNFG_00_OTG_SHIFT		        1
#define CHG_CNFG_00_BUCK_SHIFT		        2
#define CHG_CNFG_00_BOOST_SHIFT		        3
#define CHG_CNFG_00_WDTEN_SHIFT		        4
#define CHG_CNFG_00_MODE_MASK		        (0x0F << CHG_CNFG_00_MODE_SHIFT)
#define CHG_CNFG_00_CHG_MASK		        (1 << CHG_CNFG_00_CHG_SHIFT)
#define CHG_CNFG_00_UNO_MASK		        (1 << CHG_CNFG_00_UNO_SHIFT)
#define CHG_CNFG_00_OTG_MASK		        (1 << CHG_CNFG_00_OTG_SHIFT)
#define CHG_CNFG_00_BUCK_MASK		        (1 << CHG_CNFG_00_BUCK_SHIFT)
#define CHG_CNFG_00_BOOST_MASK		        (1 << CHG_CNFG_00_BOOST_SHIFT)
#define CHG_CNFG_00_WDTEN_MASK		        (1 << CHG_CNFG_00_WDTEN_SHIFT)
#define CHG_CNFG_00_UNO_CTRL			(CHG_CNFG_00_UNO_MASK | CHG_CNFG_00_BOOST_MASK)
#define CHG_CNFG_00_OTG_CTRL			(CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK)
#define MAX77729_MODE_DEFAULT			0x04
#define MAX77729_MODE_CHGR			0x01
#define MAX77729_MODE_UNO			0x01
#define MAX77729_MODE_OTG			0x02
#define MAX77729_MODE_BUCK			0x04
#define MAX77729_MODE_BOOST			0x08
#endif
#define CHG_CNFG_00_MODE_SHIFT		        0
#define CHG_CNFG_00_MODE_MASK		        (0x0F << CHG_CNFG_00_MODE_SHIFT)
#define CHG_CNFG_00_WDTEN_SHIFT		        4
#define CHG_CNFG_00_WDTEN_MASK		        (1 << CHG_CNFG_00_WDTEN_SHIFT)

/* MAX77729_CHG_REG_CHG_CNFG_00 MODE[3:0] */
#define MAX77729_MODE_0_ALL_OFF						0x0
#define MAX77729_MODE_1_ALL_OFF						0x1
#define MAX77729_MODE_2_ALL_OFF						0x2
#define MAX77729_MODE_3_ALL_OFF						0x3
#define MAX77729_MODE_4_BUCK_ON						0x4
#define MAX77729_MODE_5_BUCK_CHG_ON					0x5
#define MAX77729_MODE_6_BUCK_CHG_ON					0x6
#define MAX77729_MODE_7_BUCK_CHG_ON					0x7
#define MAX77729_MODE_8_BOOST_UNO_ON				0x8
#define MAX77729_MODE_9_BOOST_ON					0x9
#define MAX77729_MODE_A_BOOST_OTG_ON				0xA
#define MAX77729_MODE_B_RESERVED					0xB
#define MAX77729_MODE_C_BUCK_BOOST_UNO_ON				0xC
#define MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON			0xD
#define MAX77729_MODE_E_BUCK_BOOST_OTG_ON				0xE
#define MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON			0xF

/* MAX77729_CHG_REG_CHG_CNFG_01 */
#define CHG_CNFG_01_FCHGTIME_SHIFT			0
#define CHG_CNFG_01_FCHGTIME_MASK			(0x7 << CHG_CNFG_01_FCHGTIME_SHIFT)
#define MAX77729_FCHGTIME_DISABLE			0x0

#define CHG_CNFG_01_RECYCLE_EN_SHIFT	3
#define CHG_CNFG_01_RECYCLE_EN_MASK	(0x1 << CHG_CNFG_01_RECYCLE_EN_SHIFT)
#define MAX77729_RECYCLE_EN_ENABLE	0x1

#define CHG_CNFG_01_CHG_RSTRT_SHIFT	4
#define CHG_CNFG_01_CHG_RSTRT_MASK	(0x3 << CHG_CNFG_01_CHG_RSTRT_SHIFT)
#define MAX77729_CHG_RSTRT_DISABLE	0x3

#define CHG_CNFG_01_PQEN_SHIFT			7
#define CHG_CNFG_01_PQEN_MASK			(0x1 << CHG_CNFG_01_PQEN_SHIFT)
#define MAX77729_CHG_PQEN_DISABLE		0x0
#define MAX77729_CHG_PQEN_ENABLE		0x1

/* MAX77729_CHG_REG_CHG_CNFG_02 */
#define CHG_CNFG_02_OTG_ILIM_SHIFT		6
#define CHG_CNFG_02_OTG_ILIM_MASK		(0x3 << CHG_CNFG_02_OTG_ILIM_SHIFT)
#define MAX77729_OTG_ILIM_500		0x0
#define MAX77729_OTG_ILIM_900		0x1
#define MAX77729_OTG_ILIM_1200		0x2
#define MAX77729_OTG_ILIM_1500		0x3
#define MAX77729_CHG_CC                         0x3F

/* MAX77729_CHG_REG_CHG_CNFG_03 */
#define CHG_CNFG_03_TO_ITH_SHIFT		0
#define CHG_CNFG_03_TO_ITH_MASK			(0x7 << CHG_CNFG_03_TO_ITH_SHIFT)
#define MAX77729_TO_ITH_150MA			0x0

#define CHG_CNFG_03_TO_TIME_SHIFT		3
#define CHG_CNFG_03_TO_TIME_MASK			(0x7 << CHG_CNFG_03_TO_TIME_SHIFT)
#define MAX77729_TO_TIME_30M			0x3
#define MAX77729_TO_TIME_70M			0x7

#define CHG_CNFG_03_REG_AUTO_SHIPMODE_SHIFT		6
#define CHG_CNFG_03_REG_AUTO_SHIPMODE_MASK		(0x1 << CHG_CNFG_03_REG_AUTO_SHIPMODE_SHIFT)

#define CHG_CNFG_03_SYS_TRACK_DIS_SHIFT		7
#define CHG_CNFG_03_SYS_TRACK_DIS_MASK		(0x1 << CHG_CNFG_03_SYS_TRACK_DIS_SHIFT)
#define MAX77729_SYS_TRACK_ENABLE	        0x0
#define MAX77729_SYS_TRACK_DISABLE	        0x1

/* MAX77729_CHG_REG_CHG_CNFG_04 */
#define MAX77729_CHG_MINVSYS_MASK               0xC0
#define MAX77729_CHG_MINVSYS_SHIFT		6
#define MAX77729_CHG_PRM_MASK                   0x1F
#define MAX77729_CHG_PRM_SHIFT                  0

#define CHG_CNFG_04_CHG_CV_PRM_SHIFT            0
#define CHG_CNFG_04_CHG_CV_PRM_MASK             (0x3F << CHG_CNFG_04_CHG_CV_PRM_SHIFT)

/* MAX77729_CHG_REG_CHG_CNFG_05 */
#define CHG_CNFG_05_REG_B2SOVRC_SHIFT	0
#define CHG_CNFG_05_REG_B2SOVRC_MASK	(0xF << CHG_CNFG_05_REG_B2SOVRC_SHIFT)
#define MAX77729_B2SOVRC_DISABLE	0x0
#define MAX77729_B2SOVRC_4_6A		0x7
#define MAX77729_B2SOVRC_4_8A		0x8
#define MAX77729_B2SOVRC_5_0A		0x9
#define MAX77729_B2SOVRC_5_2A		0xA
#define MAX77729_B2SOVRC_5_4A		0xB
#define MAX77729_B2SOVRC_5_6A		0xC
#define MAX77729_B2SOVRC_5_8A		0xD
#define MAX77729_B2SOVRC_6_0A		0xE
#define MAX77729_B2SOVRC_6_2A		0xF

#define CHG_CNFG_05_REG_UNOILIM_SHIFT	4
#define CHG_CNFG_05_REG_UNOILIM_MASK	(0x7 << CHG_CNFG_05_REG_UNOILIM_SHIFT)
#define MAX77729_UNOILIM_200		0x1
#define MAX77729_UNOILIM_300		0x2
#define MAX77729_UNOILIM_400		0x3
#define MAX77729_UNOILIM_600		0x4
#define MAX77729_UNOILIM_800		0x5
#define MAX77729_UNOILIM_1000		0x6
#define MAX77729_UNOILIM_1500		0x7

/* MAX77729_CHG_CNFG_06 */
#define CHG_CNFG_06_WDTCLR_SHIFT		0
#define CHG_CNFG_06_WDTCLR_MASK			(0x3 << CHG_CNFG_06_WDTCLR_SHIFT)
#define MAX77729_WDTCLR				0x01
#define CHG_CNFG_06_DIS_AICL_SHIFT		4
#define CHG_CNFG_06_DIS_AICL_MASK		(0x1 << CHG_CNFG_06_DIS_AICL_SHIFT)
#define MAX77729_DIS_AICL			0x0
#define CHG_CNFG_06_B2SOVRC_DTC_SHIFT	7
#define CHG_CNFG_06_B2SOVRC_DTC_MASK	(0x1 << CHG_CNFG_06_B2SOVRC_DTC_SHIFT)
#define MAX77729_B2SOVRC_DTC_100MS		0x1

/* MAX77729_CHG_REG_CHG_CNFG_07 */
#define MAX77729_CHG_FMBST			0x04
#define CHG_CNFG_07_REG_FMBST_SHIFT		2
#define CHG_CNFG_07_REG_FMBST_MASK		(0x1 << CHG_CNFG_07_REG_FMBST_SHIFT)
#define CHG_CNFG_07_REG_FGSRC_SHIFT		1
#define CHG_CNFG_07_REG_FGSRC_MASK		(0x1 << CHG_CNFG_07_REG_FGSRC_SHIFT)
#define CHG_CNFG_07_REG_SHIPMODE_SHIFT		0
#define CHG_CNFG_07_REG_SHIPMODE_MASK		(0x1 << CHG_CNFG_07_REG_SHIPMODE_SHIFT)

/* MAX77729_CHG_REG_CHG_CNFG_08 */
#define CHG_CNFG_08_REG_FSW_SHIFT	0
#define CHG_CNFG_08_REG_FSW_MASK	(0x3 << CHG_CNFG_08_REG_FSW_SHIFT)
#define MAX77729_CHG_FSW_3MHz		0x00
#define MAX77729_CHG_FSW_2MHz		0x01
#define MAX77729_CHG_FSW_1_5MHz		0x02

/* MAX77729_CHG_REG_CHG_CNFG_09 */
#define MAX77729_CHG_CHGIN_LIM                  0x7F
#define MAX77729_CHG_EN                         0x80

/* MAX77729_CHG_REG_CHG_CNFG_10 */
#define MAX77729_CHG_WCIN_LIM                   0x3F

/* MAX77729_CHG_REG_CHG_CNFG_11 */
#define CHG_CNFG_11_VBYPSET_SHIFT		0
#define CHG_CNFG_11_VBYPSET_MASK		(0x7F << CHG_CNFG_11_VBYPSET_SHIFT)

/* MAX77729_CHG_REG_CHG_CNFG_12 */
#define MAX77729_CHG_WCINSEL			0x40
#define CHG_CNFG_12_CHGINSEL_SHIFT		5
#define CHG_CNFG_12_CHGINSEL_MASK		(0x1 << CHG_CNFG_12_CHGINSEL_SHIFT)
#define CHG_CNFG_12_WCINSEL_SHIFT		6
#define CHG_CNFG_12_WCINSEL_MASK		(0x1 << CHG_CNFG_12_WCINSEL_SHIFT)
#define CHG_CNFG_12_VCHGIN_REG_MASK		(0x3 << 3)
#define CHG_CNFG_12_WCIN_REG_MASK		(0x3 << 1)
#define CHG_CNFG_12_REG_DISKIP_SHIFT		0
#define CHG_CNFG_12_REG_DISKIP_MASK		(0x1 << CHG_CNFG_12_REG_DISKIP_SHIFT)
#define MAX77729_DISABLE_SKIP			0x1
#define MAX77729_AUTO_SKIP			0x0

/* MAX77729_CHG_REG_CHG_SWI_INT */
#define MAX77729_CLIENT_TREG_I			(1 << 0)
#define MAX77729_CV_I				(1 << 1)
#define MAX77729_CLIENT_FAULT_I			(1 << 2)

/* MAX77729_CHG_REG_CHG_SWI_INT_MASK */
#define MAX77729_CLIENT_TREG_IM			(1 << 0)
#define MAX77729_CV_IM				(1 << 1)
#define MAX77729_CLIENT_FAULT_IM		(1 << 2)

/* MAX77729_CHG_REG_CHG_SWI_STATUS */
#define MAX77729_CLIENT_TREG_S			0x00
#define MAX77729_CV_S				0x01

/* MAX77729_CHG_REG_CHG_SWI_STATUS */
#define MAX77729_DIS_MIN_SELECTOR		0x80

/* MAX77729_CHG_REG_CHG_CLIENT_READBACK */
#define MAX77729_SWI_READBACK			0x3F

/* MAX77729_CHG_REG_CHG_CLIENT_CNTL */
#define MAX77729_BOVE				0x03

#define REDUCE_CURRENT_STEP						100
#define MINIMUM_INPUT_CURRENT					300
#define SLOW_CHARGING_CURRENT_STANDARD          400

#define WC_CURRENT_STEP		100
#define WC_CURRENT_START	480

#define MAX77729_MAX_ICL			3000
#define MAX77729_MAX_FCC			6000

typedef struct max77729_charger_platform_data {
	/* wirelss charger */
	char *wireless_charger_name;
	int wireless_cc_cv;

	/* float voltage (mV) */
	int chg_float_voltage;
	int chg_irq;
	unsigned int chg_ocp_current;
	unsigned int chg_ocp_dtc;
	unsigned int topoff_time;
	int fac_vsys;
	bool enable_noise_wa;
	bool enable_sysovlo_irq;
	int fsw;

	/* OVP/UVLO check */
	int ovp_uvlo_check_type;
	/* 1st full check */
	int full_check_type;
	/* 2nd full check */
	int full_check_type_2nd;

} max77729_charger_platform_data_t;

struct max77729_charger_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct i2c_client       *pmic_i2c;
	struct i2c_client       *fg_i2c;
	struct i2c_client       *muic;

	struct max77729_platform_data *max77729_pdata;

	/*add by xiaomi start*/
	struct delayed_work period_work;
	struct delayed_work adapter_change_work;
	struct power_supply	*psy_usb;
	struct power_supply	*psy_batt;
	struct power_supply	*psy_bms;
	int pd_active;
	int usb_online;
	enum power_supply_type real_type;
	int charging_enable;
	struct votable		*usb_icl_votable;
	struct votable		*fcc_votable;
	struct votable		*mainfcc_votable;
	struct votable		*fv_votable;
	struct votable      *chgctrl_votable;
	/*add by xiaomi end*/

	struct power_supply	*psy_chg;
	struct power_supply	*psy_otg;

	struct workqueue_struct *wqueue;
	struct delayed_work	redet_work;
	struct delayed_work	chgin_work;
	struct delayed_work	aicl_work;
	struct delayed_work	isr_work;
	struct delayed_work notify_work;
	struct delayed_work batt_notify_work;

	/* mutex */
	struct mutex            charger_mutex;
	struct mutex            mode_mutex;

	/* wakelock */
	struct wakeup_source *chgin_ws;
	struct wakeup_source *wc_current_ws;
	struct wakeup_source *aicl_ws;
	struct wakeup_source *otg_ws;
	struct wakeup_source *sysovlo_ws;

	unsigned int	is_charging;
	unsigned int	cable_type;
	unsigned int	input_current;
	unsigned int	charging_current;
	unsigned int	vbus_state;
	int		aicl_curr;
	bool	slow_charging;
	int		status;
	int		charge_mode;
	u8		cnfg00_mode;
	int		fsw_now;

	int		irq_bypass;
	int		irq_batp;
#if defined(CONFIG_MAX77729_CHECK_B2SOVRC)
	int		irq_bat;
#endif
	int		irq_chgin;
	int		irq_aicl;
	int		irq_aicl_enabled;

	int		wc_current;
	int		wc_pre_current;

	bool	jig_low_active;
	int		jig_gpio;

	int irq_sysovlo;

	bool otg_on;
	bool uno_on;
	bool shutdown_delay;
	bool in_suspend;

	int pmic_ver;
	int float_voltage;

	int misalign_cnt;
	long tmr_chgoff;
	int batt_notify_count;

	max77729_charger_platform_data_t *pdata;
};


int max77729_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val);
int max77729_usb_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val);
int usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp);
int max77729_batt_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval);
int max77729_batt_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val);
int batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp);

#endif /* __MAX77729_CHARGER_H */

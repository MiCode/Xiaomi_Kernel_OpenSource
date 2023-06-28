/*
 * Copyright (c) 2022, Hynetek Semiconductor Inc. All rights reserved.
 *
 * husb320 USB TYPE-C Configuration Controller driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "inc/tcpci.h"
#include <linux/power_supply.h>
//#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci_timer.h"

#ifdef HAVE_DR
#include <linux/usb/class-dual-role.h>
#endif /* HAVE_DR */
#undef  __CONST_FFS
#define __CONST_FFS(_x) \
	((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
		((_x) & 0x04 ? 2 : 3)) :\
	 ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
	  ((_x) & 0x40 ? 6 : 7)))
#undef  FFS
#define FFS(_x) \
	((_x) ? __CONST_FFS(_x) : 0)
#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))
#undef  __BITS_GET
#define __BITS_GET(_byte, _mask, _shift) \
	(((_byte) & (_mask)) >> (_shift))
#undef  BITS_GET
#define BITS_GET(_byte, _bit) \
	__BITS_GET(_byte, _bit, FFS(_bit))
#undef  __BITS_SET
#define __BITS_SET(_byte, _mask, _shift, _val) \
	(((_byte) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))
#undef  BITS_SET
#define BITS_SET(_byte, _bit, _val) \
	__BITS_SET(_byte, _bit, FFS(_bit), _val)
#undef  BITS_MATCH
#define BITS_MATCH(_byte, _bit) \
	(((_byte) & (_bit)) == (_bit))
/* Register Map */
#define HUSB320_REG_DEVICEID            0x01
#define HUSB320_REG_DEVICETYPE          0x02
#define HUSB320_REG_PORTROLE            0x03
#define HUSB320_REG_CONTROL             0x04
#define HUSB320_REG_CONTROL1            0x05
#define HUSB320_REG_MANUAL              0x09
#define HUSB320_REG_RESET               0x0A
#define HUSB320_REG_MASK                0x0E
#define HUSB320_REG_MASK1               0x0F
#define HUSB320_REG_STATUS              0x11
#define HUSB320_REG_STATUS1             0x12
#define HUSB320_REG_TYPE                0x13
#define HUSB320_REG_INTERRUPT           0x14
#define HUSB320_REG_INTERRUPT1          0x15
#define HUSB320_REG_USER_CFG            0x16
/* Register Values */
#define HUSB320_REV                     0x10
#define HUSB320_REVTYPE                 0x01
/*  PORTROLE (03h)  */
#define HUSB320_ORIENTDEB               BIT(6)
#define HUSB320_TRY_NORMAL              0
#define HUSB320_TRY_SNK                 1
#define HUSB320_TRY_SRC                 2
#define HUSB320_TRY_DISABLE             3
#define HUSB320_AUD_ACC                 BIT(3)
#define HUSB320_DRP                     BIT(2)
#define HUSB320_SNK                     BIT(1)
#define HUSB320_SRC                     BIT(0)
#define HUSB320_DRP_ACC                (HUSB320_DRP|\
		HUSB320_AUD_ACC)
#define HUSB320_SNK_ACC                (HUSB320_SNK|\
		HUSB320_AUD_ACC)
#define HUSB320_SRC_ACC                (HUSB320_SRC|\
		HUSB320_AUD_ACC)
/*  CONTROL (04h)  */
#define HUSB320_TDRP_60MS               0
#define HUSB320_TDRP_70MS               1
#define HUSB320_TDRP_80MS               2
#define HUSB320_TDRP_90MS               3
#define HUSB320_TGL_60PCT               0
#define HUSB320_TGL_50PCT               1
#define HUSB320_TGL_40PCT               2
#define HUSB320_TGL_30PCT               3
#define HUSB320_DCABLE_EN               BIT(3)
#define HUSB320_HOST_0MA                0
#define HUSB320_HOST_DEFAULT            1
#define HUSB320_HOST_1500MA             2
#define HUSB320_HOST_3000MA             3
#define HUSB320_INT_ENABLE              0x00
#define HUSB320_INT_DISABLE             BIT(0)
/*  CONTROL1 (05h)  */
#define HUSB320_REMEDY_EN               BIT(7)
#define HUSB320_AUTO_SNK_TH_3P0V        0
#define HUSB320_AUTO_SNK_TH_3P1V        1
#define HUSB320_AUTO_SNK_TH_3P2V        2
#define HUSB320_AUTO_SNK_TH_3P3V        3
#define HUSB320_AUTO_SNK_EN             BIT(4)
#define HUSB320_ENABLE                  1
#define HUSB320_DISABLE                 0
#define HUSB320_TCCDEB_120MS            0
#define HUSB320_TCCDEB_130MS            1
#define HUSB320_TCCDEB_140MS            2
#define HUSB320_TCCDEB_150MS            3
#define HUSB320_TCCDEB_160MS            4
#define HUSB320_TCCDEB_170MS            5
#define HUSB320_TCCDEB_180MS            6
/*  MANUAL (09h)  */
#define HUSB320_FORCE_SRC               BIT(5)
#define HUSB320_FORCE_SNK               BIT(4)
#define HUSB320_UNATT_SNK               BIT(3)
#define HUSB320_UNATT_SRC               BIT(2)
#define HUSB320_DISABLED                BIT(1)
#define HUSB320_ERR_REC                 BIT(0)
/*  RESET (0Ah)  */
#define HUSB320_DISABLED_CLEAR          0x00
#define HUSB320_SW_RESET                BIT(0)
/*  MASK (0Eh)  */
#define HUSB320_M_ORIENT                BIT(6)
#define HUSB320_M_FAULT                 BIT(5)
#define HUSB320_M_VBUS_CHG              BIT(4)
#define HUSB320_M_AUTOSNK               BIT(3)
#define HUSB320_M_BC_LVL                BIT(2)
#define HUSB320_M_DETACH                BIT(1)
#define HUSB320_M_ATTACH                BIT(0)
/*  MASK1 (0Fh)  */
#define HUSB320_M_REM_VBOFF             BIT(6)
#define HUSB320_M_REM_VBON              BIT(5)
#define HUSB320_M_REM_FAIL              BIT(3)
#define HUSB320_M_FRC_FAIL              BIT(2)
#define HUSB320_M_FRC_SUCC              BIT(1)
#define HUSB320_M_REMEDY                BIT(0)
/*  STATUS (11h)  */
#define HUSB320_AUTOSNK                 BIT(7)
#define HUSB320_VSAFE0V                 BIT(6)
#define HUSB320_ORIENT_NONE             0
#define HUSB320_ORIENT_CC1              1
#define HUSB320_ORIENT_CC2              2
#define HUSB320_ORIENT_FAULT            3
#define HUSB320_VBUSOK                  BIT(3)
#define HUSB320_SNK_0MA                 0x00
#define HUSB320_SNK_DEFAULT             0x02
#define HUSB320_SNK_1500MA              0x04
#define HUSB320_SNK_3000MA              0x06
#define HUSB320_ATTACH                  BIT(0)
/*  STATUS1 (12h)  */
#define HUSB320_FAULT                   BIT(1)
#define HUSB320_REMEDY                  BIT(0)
/*  TYPE (13h)  */
#define HUSB320_TYPE_DBG_ACC_SRC        BIT(6)
#define HUSB320_TYPE_DBG_ACC_SNK        BIT(5)
#define HUSB320_TYPE_SNK                BIT(4)
#define HUSB320_TYPE_SRC                BIT(3)
#define HUSB320_TYPE_ACTV_CABLE         BIT(2)
#define HUSB320_TYPE_PWR_AUD_ACC        BIT(1)
#define HUSB320_TYPE_AUD_ACC            BIT(0)
#define HUSB320_TYPE_INVALID            0x00
#define HUSB320_TYPE_SRC_ACC           (HUSB320_TYPE_SRC|\
		HUSB320_TYPE_ACTV_CABLE)
/*  INTERRUPT (14h)  */
#define HUSB320_I_ORIENT                BIT(6)
#define HUSB320_I_FAULT                 BIT(5)
#define HUSB320_I_VBUS_CHG              BIT(4)
#define HUSB320_I_AUTOSNK               BIT(3)
#define HUSB320_I_BC_LVL                BIT(2)
#define HUSB320_I_DETACH                BIT(1)
#define HUSB320_I_ATTACH                BIT(0)
/*  INTERRUPT1 (15h)  */
#define HUSB320_I_REM_VBOFF             BIT(6)
#define HUSB320_I_REM_VBON              BIT(5)
#define HUSB320_I_REM_FAIL              BIT(3)
#define HUSB320_I_FRC_FAIL              BIT(2)
#define HUSB320_I_FRC_SUCC              BIT(1)
#define HUSB320_I_REMEDY                BIT(0)
/* USER CFG */
#define HUSB320_CC_DSCNTEN				BIT(5)
#define HUSB320_TBC_LEVEL_0_5MS		    0x00
#define HUSB320_TBC_LEVEL_12MS			0x1
#define HUSB320_TBC_LEVEL_15MS			0x2
#define HUSB320_TBC_LEVEL_18MS			0x3
/* Mask */
#define HUSB320_ORIEN_DBG_ACC_MASK      0x40
#define HUSB320_PORTROLE_MASK           0x3F
#define HUSB320_TRY_MASK                0x30

#define HUSB320_TDRP_MASK               0xC0
#define HUSB320_TGL_MASK                0x30
#define HUSB320_DCBL_EN_MASK            0x08
#define HUSB320_HOST_CUR_MASK           0x06
#define HUSB320_INT_MASK                0x01

#define HUSB320_REMDY_EN_MASK           0x80
#define HUSB320_AUTO_SNK_TH_MASK        0x60
#define HUSB320_AUTO_SNK_EN_MASK        0x10
#define HUSB320_ENABLE_MASK             0x08
#define HUSB320_TCCDEB_MASK             0x07

#define HUSB320_ORIENT_MASK             0x30
#define HUSB320_BCLVL_MASK              0x06
#define HUSB320_TYPE_MASK               0x7F
#define HUSB320_TBC_LVL_MASK			0x03

#define HUSB320_INT_STS_MASK            0x7F
#define HUSB320_INT1_STS_MASK           0x6F
#define HUSB320_MASK_INT_MASK           0x7F
#define HUSB320_MASK_INT1_MASK          0x6F
/* TYPEC STATES */
#define TYPEC_STATE_DISABLED             0x00
#define TYPEC_STATE_ERROR_RECOVERY       0x01
#define TYPEC_STATE_UNATTACHED_SNK       0x02
#define TYPEC_STATE_UNATTACHED_SRC       0x03
#define TYPEC_STATE_ATTACHWAIT_SNK       0x04
#define TYPEC_STATE_ATTACHWAIT_SRC       0x05
#define TYPEC_STATE_ATTACHED_SNK         0x06
#define TYPEC_STATE_ATTACHED_SRC         0x07
#define TYPEC_STATE_AUDIO_ACCESSORY      0x08
#define TYPEC_STATE_DEBUG_ACCESSORY      0x09
#define TYPEC_STATE_TRY_SNK              0x0A
#define TYPEC_STATE_TRYWAIT_SRC          0x0B
#define TYPEC_STATE_TRY_SRC              0x0C
#define TYPEC_STATE_TRYWAIT_SNK          0x0D
#define TYPEC_STATE_FORCE_SRC            0x0E
#define TYPEC_STATE_FORCE_SNK            0x0F

#define REVERSE_CHG_SOURCE				0X01
#define REVERSE_CHG_SINK				0X02
#define REVERSE_CHG_DRP					0X03
#define REVERSE_CHG_TEST				0X04

extern uint8_t     typec_cc_orientation;
bool first_check = true;
struct husb320_chip *chip_chg;
struct i2c_client *g_client;
static bool husb320_is_vbus_on(struct husb320_chip *chip);
struct husb320_data {
	int int_gpio;
	u32 init_mode;
	u32 dfp_power;
	u32 tdrptime;
	u32 dttime;
	u32 autosnk_thres;
	u32 ccdebtime;
};
struct husb320_chip {
	struct i2c_client *client;
	struct husb320_data *pdata;
	struct workqueue_struct  *cc_wq;
	struct tcpc_device *tcpc;
	struct tcpc_desc *tcpc_desc;
	int irq_gpio;
	int irq;
	int ufp_power;
	u32 mode;
	u32 dev_id;
	u32 type;
	u32 state;
	u32 bc_lvl;
	u32 dfp_power;
	u32 tdrptime;
	u32 orientation;
	u32 dttime;
	u32 autosnk_thres;
	u32 ccdebtime;
	struct work_struct dwork;
	struct delayed_work first_check_typec_work;
	struct mutex mlock;
	struct semaphore suspend_lock;
	//	struct power_supply *usb_psy;
#ifdef HAVE_DR
	struct dual_role_phy_instance *dual_role;
#endif /* HAVE_DR */
	bool role_switch;
#ifdef HAVE_DR
	struct dual_role_phy_desc *desc;
#endif /* HAVE_DR */
};
#define typec_update_state(chip, st) \
	do { \
	if (chip && st < TYPEC_STATE_FORCE_SNK) { \
		chip->state = st; \
		dev_info(&chip->client->dev, "%s: %s\n", __func__, #st); \
		wake_up_interruptible(&mode_switch); \
	} \
	} while (0)
#define STR(s)    #s
#define STRV(s)   STR(s)
static void husb320_detach(struct husb320_chip *chip);
DECLARE_WAIT_QUEUE_HEAD(mode_switch);
static int husb320_write_masked_byte(struct i2c_client *client,
		u8 addr, u8 mask, u8 val)
{
	int rc;
	if (!mask) {
		/* no actual access */
		rc = -EINVAL;
		goto out;
	}
	rc = i2c_smbus_read_byte_data(client, addr);
	if (!(rc < 0)) {
		rc = i2c_smbus_write_byte_data(client,
				addr, BITS_SET((u8)rc, mask, val));
	}
out:
	return rc;
}
static int husb320_read_device_id(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_USER_CFG,
			HUSB320_TBC_LVL_MASK,
			HUSB320_TBC_LEVEL_18MS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write user_cfg, errno=%d\n",
				__func__, rc);
		return rc;
	}

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_USER_CFG);
	if (rc < 0)
		return rc;
	dev_info(cdev, "%s: user cfg=0x%02x\n", __func__, rc);

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_DEVICEID);
	if (rc < 0)
		return rc;
	chip->dev_id = rc;
	dev_info(cdev, "%s: device id=0x%02x\n", __func__, rc);
	return rc;
}
static int husb320_update_status(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u16 control_now;
	u16 control1_now;

	/* read mode & control register */
	rc = i2c_smbus_read_word_data(chip->client, HUSB320_REG_PORTROLE);
	if (rc < 0) {
		dev_err(cdev, "%s: fail to read mode\n", __func__);
		return rc;
	}
	chip->mode = rc & HUSB320_PORTROLE_MASK;
	control_now = (rc >> 8) & 0xFF;
	chip->dfp_power = BITS_GET(control_now, HUSB320_HOST_CUR_MASK);
	chip->tdrptime = BITS_GET(control_now, HUSB320_TDRP_MASK);
	chip->dttime = BITS_GET(control_now, HUSB320_TGL_MASK);

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_CONTROL1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read control1 reg\n", __func__);
		return rc;
	}
	control1_now = rc ;
	chip->autosnk_thres = BITS_GET(control1_now, HUSB320_AUTO_SNK_TH_MASK);
	chip->ccdebtime = BITS_GET(control1_now, HUSB320_TCCDEB_MASK);
	return 0;
}
static int husb320_set_manual_reg(struct husb320_chip *chip, u8 state)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (state > HUSB320_FORCE_SRC)
		return -EINVAL;
	if (state & HUSB320_DISABLED) {
		dev_err(cdev,
				"%s: return err if sw try to disable device state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	if ((state & HUSB320_FORCE_SRC) && (chip->type == HUSB320_TYPE_SRC)) {
		dev_err(cdev,
				"%s: return err if chip already in src, state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	if ((state & HUSB320_FORCE_SNK) && (chip->type == HUSB320_TYPE_SNK)) {
		dev_err(cdev,
				"%s: return err if chip already in snk, state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	rc = i2c_smbus_write_byte_data(chip->client,
			HUSB320_REG_MANUAL,
			state);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write manual, errno=%d\n",
				__func__, rc);
		return rc;
	}
	dev_info(cdev, "%s: state=%d\n", __func__, state);
	return rc;
}
static int husb320_set_chip_state(struct husb320_chip *chip, u8 state)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	dev_info(cdev, "%s: update manual reg=%d\n", __func__, state);
	rc = husb320_set_manual_reg(chip, state);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write manual reg\n", __func__);
		return rc;
	}
	chip->state = state;
	return rc;
}
static int husb320_set_mode(struct husb320_chip *chip, u8 mode)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (mode != chip->mode) {
		rc = i2c_smbus_write_byte_data(chip->client,
				HUSB320_REG_PORTROLE, mode);
		if (rc < 0) {
			dev_err(cdev, "%s: failed to write mode\n", __func__);
			return rc;
		}
		chip->mode = mode;
	}
	dev_info(cdev, "%s: chip->mode=%d\n", __func__, chip->mode);
	return rc;
}
static int husb320_set_dfp_power(struct husb320_chip *chip, u8 hcurrent)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (hcurrent > HUSB320_HOST_3000MA) {
		dev_err(cdev, "%s: hcurrent=%d is unavailable\n",
				__func__, hcurrent);
		return -EINVAL;
	}
	if (hcurrent == chip->dfp_power) {
		dev_err(cdev, "%s: hcurrent=%d, value is not updated\n",
				__func__, hcurrent);
		return rc;
	}
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_HOST_CUR_MASK,
			hcurrent);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write current, errno=%d\n",
				__func__, rc);
		return rc;
	}
	chip->dfp_power = hcurrent;
	dev_info(cdev, "%s: host current(%d)\n", __func__, chip->dfp_power);
	return rc;
}

static int husb320_init_force_dfp_power(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_HOST_CUR_MASK,
			HUSB320_HOST_1500MA);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write current\n", __func__);
		return rc;
	}
	chip->dfp_power = HUSB320_HOST_1500MA;
	dev_info(cdev, "%s: host current=%d\n", __func__, chip->dfp_power);
	return rc;
}
static int husb320_set_drp_cycle_time(struct husb320_chip *chip,
		u8 drp_cycle_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (drp_cycle_time > HUSB320_TDRP_90MS) {
		dev_err(cdev, "%s: drp cycle time=%d is unavailable\n",
				__func__, drp_cycle_time);
		return -EINVAL;
	}
	if (drp_cycle_time == chip->tdrptime) {
		dev_err(cdev, "%s: drp cycle time=%d, value is not updated \n",
				__func__, drp_cycle_time);
		return rc;
	}
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_TDRP_MASK,
			drp_cycle_time);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write drp cycle time, errno=%d\n",
				__func__, rc);
		return rc;
	}

	chip->tdrptime = drp_cycle_time;
	dev_info(cdev, "%s: drp cycle time=%d\n", __func__, chip->tdrptime);
	return rc;
}

static int husb320_set_toggle_time(struct husb320_chip *chip,
		u8 toggle_dutycycle_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (toggle_dutycycle_time > HUSB320_TGL_30PCT) {
		dev_err(cdev, "%s: toggle dutycycle time=%d is unavailable\n",
				__func__, toggle_dutycycle_time);
		return -EINVAL;
	}
	if (toggle_dutycycle_time == chip->dttime) {
		dev_err(cdev,
				"%s: toggle dutycycle time=%d, value is not updated \n",
				__func__, toggle_dutycycle_time);
		return rc;
	}
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_TGL_MASK,
			toggle_dutycycle_time);
	if (rc < 0) {
		dev_err(cdev,
				"%s: failed to write toggle dutycycle time, errno=%d\n",
				__func__, rc);
		return rc;
	}

	chip->dttime = toggle_dutycycle_time;
	dev_info(cdev, "%s: toggle dutycycle time=%d\n",
			__func__, chip->dttime);
	return rc;
}
static int husb320_set_autosink_threshold(struct husb320_chip *chip,
		u8 autosink_threshold)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (autosink_threshold > HUSB320_AUTO_SNK_TH_3P3V) {
		dev_err(cdev, "%s: auto sink threshold=%d is unavailable\n",
				__func__, autosink_threshold);
		return -EINVAL;
	}
	if (autosink_threshold == chip->autosnk_thres) {
		dev_err(cdev,
				"%s: auto sink threshold=%d, value is not updated \n",
				__func__, autosink_threshold);
		return rc;
	}
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL1,
			HUSB320_AUTO_SNK_TH_MASK,
			autosink_threshold);
	if (rc < 0) {
		dev_err(cdev,
				"%s:failed to write auto sink threshold，errno=%d\n",
				__func__, rc);
		return rc;
	}

	chip->autosnk_thres = autosink_threshold;
	dev_info(cdev, "%s: auto sink threshold=%d\n",
			__func__, chip->autosnk_thres);
	return rc;
}
static int husb320_set_tccdebounce_time(struct husb320_chip *chip,
		u8 tccdebounce_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (tccdebounce_time > HUSB320_TCCDEB_180MS) {
		dev_err(cdev, "%s: CC debounce time=%d is unavailable\n",
				__func__, tccdebounce_time);
		return -EINVAL;
	}
	if (tccdebounce_time == chip->ccdebtime) {
		dev_err(cdev,
				"%s: CC debounce time=%d, value is not updated \n",
				__func__, tccdebounce_time);
		return rc;
	}
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL1,
			HUSB320_TCCDEB_MASK,
			tccdebounce_time);
	if (rc < 0) {
		dev_err(cdev,
				"%s: failed to write CC debounce time, errno=%d\n",
				__func__, rc);
		return rc;
	}

	chip->ccdebtime = tccdebounce_time;
	dev_info(cdev, "%s: CC debounce time=%d\n", __func__, chip->ccdebtime);
	return rc;
}
static int husb320_set_int_mask_state(struct husb320_chip *chip, u8 msk_val)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_MASK,
			HUSB320_MASK_INT_MASK,
			msk_val);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write mask reg, errno=%d\n",
				__func__, rc);
		return rc;
	}

	dev_info(cdev, "%s: mask=0x%x\n", __func__, msk_val);
	return rc;
}
static int husb320_set_int1_mask_state(struct husb320_chip *chip, u8 mask1_val)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_MASK1,
			HUSB320_MASK_INT1_MASK,
			mask1_val);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write mask1 reg, errno=%d\n",
				__func__, rc);
		return rc;
	}

	dev_info(cdev, "%s: mask1=0x%x\n", __func__, mask1_val);
	return rc;
}

static int husb320_dangling_cbl_en(struct husb320_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_DCBL_EN_MASK,
			enable);
	if (rc < 0) {
		dev_err(cdev,
				"%s: failed to switch dangling cable func, errno=%d\n",
				__func__, rc);
		return rc;
	}
	dev_info(cdev, "%s: dangling cable enabled=%d\n", __func__, enable);
	return rc;
}
static int husb320_remedy_en(struct husb320_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL1,
			HUSB320_REMDY_EN_MASK,
			enable);
	if (rc < 0) {
		dev_err(cdev,
				"%s: failed to switch remedy func, errno=%d\n",
				__func__, rc);
		return rc;
	}
	dev_info(cdev, "%s: remedy func enabled=%d\n", __func__, enable);
	return rc;
}
static int husb320_auto_snk_en(struct husb320_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL1,
			HUSB320_AUTO_SNK_EN_MASK,
			enable);
	if (rc < 0) {
		dev_err(cdev,
				"%s: failed to switch auto snk func, errno=%d\n",
				__func__, rc);
		return rc;
	}
	dev_info(cdev, "%s: auto snk func enabled=%d\n", __func__, enable);
	return rc;
}

bool platform_get_device_irq_state(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	dev_info(cdev, "%s enter\n", __func__);

	if (!chip) {
		pr_err("%s Error: Chip structure is NULL!\n", __func__);
		return false;
	} else {
		if (gpio_cansleep(chip->pdata->int_gpio)) {
			rc = !gpio_get_value_cansleep(chip->pdata->int_gpio);
		} else {
			rc = !gpio_get_value(chip->pdata->int_gpio);
		}
		dev_info(cdev, "%s finish, state=%d\n", __func__, rc);
		return (rc != 0);
	}
}

static int husb320_enable(struct husb320_chip *chip, bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	u8  count = 5;
	u8 data[5] = {0x2B, 0x00, 0x00, 0x00, 0x08};

	dev_info(cdev, "%s: state=%d\n", __func__, enable);
	if (chip->ccdebtime != HUSB320_TCCDEB_150MS) {
		data[0] = data[0] & (~HUSB320_TCCDEB_MASK);
		data[0] = data[0] | chip->ccdebtime;
		dev_err(cdev, "%s: Control1 reg=0x%02x\n", __func__, data[0]);
	}
	if (chip->autosnk_thres != HUSB320_AUTO_SNK_TH_3P1V) {
		data[0] = data[0] & (~HUSB320_AUTO_SNK_TH_MASK);
		data[0] = data[0] | chip->autosnk_thres;
		dev_err(cdev, "%s: Control1 reg=0x%02x\n", __func__, data[0]);
	}

	if (enable == true) {
		while (count) {
			rc = i2c_smbus_write_i2c_block_data(chip->client,
					HUSB320_REG_CONTROL1,
					sizeof(data), data);
			if (rc < 0) {
				dev_err(cdev, "%s: Unable to write registers\n",
						__func__);
				count--;
			} else {
				return rc;
			}
			udelay(100);
		}
	} else {
		rc = husb320_write_masked_byte(chip->client,
				HUSB320_REG_CONTROL1,
				HUSB320_ENABLE_MASK,
				HUSB320_DISABLE);
		if (rc < 0) {
			dev_err(cdev, "%s: failed to disable husb320\n",
					__func__);
			return rc;
		}
	}
	return rc;
}
static int husb320_init_reg(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	/* change current */
	rc = husb320_init_force_dfp_power(chip);
	if (rc < 0)
		dev_err(cdev, "%s: failed to force dfp power\n", __func__);
	/* change tdrp time */
	rc = husb320_set_drp_cycle_time(chip, chip->pdata->tdrptime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set drp cycle time\n", __func__);
	/* change toggle time */
	rc = husb320_set_toggle_time(chip, chip->pdata->dttime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set toggle dutycycle time\n",
				__func__);
	/* change auto sink threshold */
	rc = husb320_set_autosink_threshold(chip, chip->pdata->autosnk_thres);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set auto sink threshold\n",
				__func__);
	/* change CC debounce time */
	rc = husb320_set_tccdebounce_time(chip, chip->pdata->ccdebtime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set CC debounce time\n",
				__func__);
	/* change mode */
	rc = husb320_set_mode(chip, chip->pdata->init_mode);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set mode\n", __func__);
	/* enable detection */
	rc = husb320_enable(chip, true);
	if (rc < 0)
		dev_err(cdev, "%s: failed to enable detection\n", __func__);
	dev_info(cdev, "%s: end\n", __func__);
	return rc;
}
static int husb320_reset_device(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = i2c_smbus_write_byte_data(chip->client,
			HUSB320_REG_RESET,
			HUSB320_SW_RESET);
	if (rc < 0) {
		dev_err(cdev, "%s: reset fails\n", __func__);
		return rc;
	}
	msleep(100);
	dev_err(cdev, "%s: mode=0x%02x, host_cur=0x%02x\n",
			__func__, chip->mode, chip->dfp_power);
	dev_err(cdev, "%s: tdrptime=0x%02x, dttime=0x%02x\n",
			__func__, chip->tdrptime, chip->dttime);
	dev_err(cdev, "%s: autosnk_thres=0x%02x, ccdebtime=0x%02x\n",
			__func__, chip->autosnk_thres, chip->ccdebtime);
	rc = husb320_update_status(chip);
	if (rc < 0)
		dev_err(cdev, "%s: fail to read status\n", __func__);
	rc = husb320_init_reg(chip);
	if (rc < 0)
		dev_err(cdev, "%s: fail to init reg\n", __func__);
	rc = husb320_dangling_cbl_en(chip, false);
	if (rc < 0)
		dev_err(cdev,
				"%s: fail to disable dangling cbl func\n", __func__);
	rc = husb320_remedy_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable remedy func\n", __func__);
	rc = husb320_auto_snk_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable auto snk func\n", __func__);
	rc = husb320_set_int_mask_state(chip, HUSB320_M_FAULT | HUSB320_M_AUTOSNK);
	if (rc < 0)
		dev_err(cdev, "%s: fail to set mask reg\n", __func__);
	rc = husb320_set_int1_mask_state(chip, 0xFF);
	if (rc < 0)
		dev_err(cdev, "%s: fail to set mask1 reg\n", __func__);

	/* clear global interrupt mask */
	rc = husb320_write_masked_byte(chip->client,
			HUSB320_REG_CONTROL,
			HUSB320_INT_MASK,
			HUSB320_INT_ENABLE);
	if (rc < 0) {
		dev_err(cdev, "%s: fail to init\n", __func__);
		return rc;
	}
	dev_err(cdev, "%s: mode=0x%02x, host_cur=0x%02x\n",
			__func__, chip->mode, chip->dfp_power);
	dev_err(cdev, "%s: tdrptime=0x%02x, dttime=0x%02x\n",
			__func__, chip->tdrptime, chip->dttime);
	dev_err(cdev, "%s: autosnk_thres=0x%02x, ccdebtime=0x%02x\n",
			__func__, chip->autosnk_thres, chip->ccdebtime);
	return rc;
}
static ssize_t fregdump_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	u8 start_reg[] = {0x01, 0x02,
		0x03, 0x04,
		0x05, 0x09,
		0x0A, 0x0E,
		0x0F, 0x11,
		0x12, 0x13,
		0x14, 0x15};
	int i, rc, ret = 0;

	mutex_lock(&chip->mlock);
	for (i = 0 ; i < 14; i++) {
		rc = i2c_smbus_read_byte_data(chip->client, start_reg[i]);
		if (rc < 0) {
			pr_err("%s: cannot read 0x%02x\n",
					__func__, start_reg[i]);
			rc = 0;
		}
		pr_err("%s: reg[%d]=0x%02x\n", __func__, start_reg[i], rc);
		ret += snprintf(buf + ret, 1024,
				"from 0x%02x read 0x%02x\n",
				start_reg[i],
				(rc & 0xFF));
	}
	mutex_unlock(&chip->mlock);
	return ret;
}
DEVICE_ATTR(fregdump, S_IRUGO, fregdump_show, NULL);
static ssize_t ftype_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	switch (chip->type) {
	case HUSB320_TYPE_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SINK(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SOURCE(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_DBG_ACC_SRC:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACCSRC(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_DBG_ACC_SNK:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACCSNK(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "AUDIOACC(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_PWR_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "POWEREDAUDIOACC(%d)\n",
				chip->type);
		break;
	case HUSB320_TYPE_ACTV_CABLE:
		ret = snprintf(buf, PAGE_SIZE, "ACTIVECABLE(%d)\n", chip->type);
		break;
	case HUSB320_TYPE_SRC_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SOURCE + ACTIVECABLE(%d)\n",
				chip->type);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "NOTYPE(%d)\n", chip->type);
		break;
	}
	mutex_unlock(&chip->mlock);
	return ret;
}
DEVICE_ATTR(ftype, S_IRUGO, ftype_show, NULL);
static ssize_t fchip_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	switch (chip->state) {
	case TYPEC_STATE_DISABLED:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_DISABLED(%d)\n", chip->state);
		break;
	case TYPEC_STATE_ERROR_RECOVERY:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_ERROR_RECOVERY(%d)\n", chip->state);
		break;
	case TYPEC_STATE_FORCE_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_FORCE_SRC(%d)\n", chip->state);
		break;
	case TYPEC_STATE_FORCE_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_FORCE_SNK(%d)\n", chip->state);
		break;
	case TYPEC_STATE_UNATTACHED_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_UNATTACHED_SNK(%d)\n", chip->state);
		break;
	case TYPEC_STATE_UNATTACHED_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_UNATTACHED_SRC(%d)\n", chip->state);
		break;
	case TYPEC_STATE_ATTACHED_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_ATTACHED_SNK(%d)\n", chip->state);
		break;
	case TYPEC_STATE_ATTACHED_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"TYPEC_STATE_ATTACHED_SRC(%d)\n", chip->state);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE,
				"UNKNOWN(%d)\n", chip->state);
		break;
	}
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fchip_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		if (((state == TYPEC_STATE_UNATTACHED_SNK) &&
					(chip->mode & (HUSB320_SRC | HUSB320_SRC_ACC))) ||
				((state == TYPEC_STATE_UNATTACHED_SRC) &&
				 (chip->mode & (HUSB320_SNK | HUSB320_SNK_ACC)))) {
			mutex_unlock(&chip->mlock);
			return -EINVAL;
		}
		rc = husb320_set_chip_state(chip, (u8)state);
		if (rc < 0) {
			mutex_unlock(&chip->mlock);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fchip_state, S_IRUGO|S_IWUSR, fchip_state_show,
		fchip_state_store);
static ssize_t fmode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	switch (chip->mode) {   // make sure all MODEs are here
	case HUSB320_DRP_ACC:
		ret = snprintf(buf, PAGE_SIZE, "DRP+ACC(%d)\n", chip->mode);
		break;
	case HUSB320_DRP:
		ret = snprintf(buf, PAGE_SIZE, "DRP(%d)\n", chip->mode);
		break;
	case HUSB320_SNK_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SNK+ACC(%d)\n", chip->mode);
		break;
	case HUSB320_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SNK(%d)\n", chip->mode);
		break;
	case HUSB320_SRC_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SRC+ACC(%d)\n", chip->mode);
		break;
	case HUSB320_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SRC(%d)\n", chip->mode);
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "UNKNOWN(%d)\n", chip->mode);
		break;
	}
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fmode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int mode = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &mode) == 1) {
		mutex_lock(&chip->mlock);
		/*
		 * since device trigger to usb happens independent
		 * from charger based on vbus, setting SRC modes
		 * doesn't prevent usb enumeration as device
		 * KNOWN LIMITATION
		 */
		rc = husb320_set_mode(chip, (u8)mode);
		if (rc < 0) {
			mutex_unlock(&chip->mlock);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fmode, S_IRUGO | S_IWUSR, fmode_show, fmode_store);
static ssize_t ftdrptime_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->tdrptime);
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t ftdrptime_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ftdrptime = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &ftdrptime) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_set_drp_cycle_time(chip, (u8)ftdrptime);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(ftdrptime, S_IRUGO | S_IWUSR, ftdrptime_show, ftdrptime_store);
static ssize_t fdttime_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->dttime);
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fdttime_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int dttime = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &dttime) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_set_toggle_time(chip, (u8)dttime);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fdttime, S_IRUGO | S_IWUSR, fdttime_show, fdttime_store);
static ssize_t fhostcur_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->dfp_power);
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fhostcur_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_set_dfp_power(chip, (u8)buf);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fhostcur, S_IRUGO | S_IWUSR, fhostcur_show, fhostcur_store);
static ssize_t fclientcur_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->ufp_power);
	mutex_unlock(&chip->mlock);
	return ret;
}
DEVICE_ATTR(fclientcur, S_IRUGO, fclientcur_show, NULL);
static ssize_t freset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	u32 reset = 0;
	int rc = 0;
	if (sscanf(buff, "%u", &reset) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_reset_device(chip);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(freset, S_IWUSR, NULL, freset_store);
static ssize_t fauto_snk_th_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->autosnk_thres);
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fauto_snk_th_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_set_autosink_threshold(chip, (u8)buf);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fauto_snk_th, S_IRUGO | S_IWUSR, fauto_snk_th_show,
		fauto_snk_th_store);
static ssize_t fccdebounce_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", chip->ccdebtime);
	mutex_unlock(&chip->mlock);
	return ret;
}
static ssize_t fccdebounce_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_set_tccdebounce_time(chip, (u8)buf);
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fccdebounce, S_IRUGO | S_IWUSR, fccdebounce_show,
		fccdebounce_store);
static ssize_t fdcable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_CONTROL);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & HUSB320_DCBL_EN_MASK) >> 3;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fdcable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_write_masked_byte(chip->client,
				HUSB320_REG_CONTROL,
				HUSB320_DCBL_EN_MASK,
				state);
		if (rc < 0) {
			dev_err(dev, "%s: failed to write 04h reg\n",
					__func__);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fdcable, S_IRUGO | S_IWUSR, fdcable_show, fdcable_store);
static ssize_t fremedy_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_CONTROL1);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & HUSB320_REMDY_EN_MASK) >> 7;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fremedy_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_write_masked_byte(chip->client,
				HUSB320_REG_CONTROL1,
				HUSB320_REMDY_EN_MASK,
				state);
		if (rc < 0) {
			dev_err(dev, "%s: failed to write 04h reg\n",
					__func__);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fremedy, S_IRUGO | S_IWUSR, fremedy_show, fremedy_store);
static ssize_t fauto_snk_en_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_CONTROL1);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & HUSB320_AUTO_SNK_EN_MASK) >> 4;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fauto_snk_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = husb320_write_masked_byte(chip->client,
				HUSB320_REG_CONTROL1,
				HUSB320_AUTO_SNK_EN_MASK,
				state);
		if (rc < 0) {
			dev_err(dev, "%s: failed to write 04h reg\n",
					__func__);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		if (rc < 0)
			return rc;
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fauto_snk_en, S_IRUGO | S_IWUSR,
		fauto_snk_en_show, fauto_snk_en_store);
static int husb320_create_devices(struct device *cdev)
{
	int ret = 0;
	ret = device_create_file(cdev, &dev_attr_fchip_state);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fchip_state\n");
		ret = -ENODEV;
		goto err0;
	}
	ret = device_create_file(cdev, &dev_attr_ftype);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_ftype\n");
		ret = -ENODEV;
		goto err1;
	}
	ret = device_create_file(cdev, &dev_attr_fmode);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fmode\n");
		ret = -ENODEV;
		goto err2;
	}
	ret = device_create_file(cdev, &dev_attr_freset);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_freset\n");
		ret = -ENODEV;
		goto err3;
	}
	ret = device_create_file(cdev, &dev_attr_ftdrptime);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_ftdrptime\n");
		ret = -ENODEV;
		goto err4;
	}
	ret = device_create_file(cdev, &dev_attr_fdttime);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fdttime\n");
		ret = -ENODEV;
		goto err5;
	}
	ret = device_create_file(cdev, &dev_attr_fhostcur);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fhostcur\n");
		ret = -ENODEV;
		goto err6;
	}
	ret = device_create_file(cdev, &dev_attr_fclientcur);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fufpcur\n");
		ret = -ENODEV;
		goto err7;
	}
	ret = device_create_file(cdev, &dev_attr_fregdump);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fregdump\n");
		ret = -ENODEV;
		goto err8;
	}
	ret = device_create_file(cdev, &dev_attr_fauto_snk_th);
	if (ret < 0) {
		dev_err(cdev, "failed to create dev_attr_fsw_trysnk\n");
		ret = -ENODEV;
		goto err9;
	}
	ret = device_create_file(cdev, &dev_attr_fccdebounce);
	if (ret < 0) {
		dev_err(cdev,
				"failed to create dev_attr_fccdebounce\n");
		ret = -ENODEV;
		goto err10;
	}
	ret = device_create_file(cdev, &dev_attr_fdcable);
	if (ret < 0) {
		dev_err(cdev,
				"failed to create dev_attr_fdcable\n");
		ret = -ENODEV;
		goto err11;
	}
	ret = device_create_file(cdev, &dev_attr_fremedy);
	if (ret < 0) {
		dev_err(cdev,
				"failed to create dev_attr_fremedy\n");
		ret = -ENODEV;
		goto err12;
	}
	ret = device_create_file(cdev, &dev_attr_fauto_snk_en);
	if (ret < 0) {
		dev_err(cdev,
				"failed to create dev_attr_fauto_snk_en\n");
		ret = -ENODEV;
		goto err13;
	}
	return ret;
err13:
	device_remove_file(cdev, &dev_attr_fremedy);
err12:
	device_remove_file(cdev, &dev_attr_fdcable);
err11:
	device_remove_file(cdev, &dev_attr_fccdebounce);
err10:
	device_remove_file(cdev, &dev_attr_fauto_snk_th);
err9:
	device_remove_file(cdev, &dev_attr_fregdump);
err8:
	device_remove_file(cdev, &dev_attr_fclientcur);
err7:
	device_remove_file(cdev, &dev_attr_fhostcur);
err6:
	device_remove_file(cdev, &dev_attr_fdttime);
err5:
	device_remove_file(cdev, &dev_attr_ftdrptime);
err4:
	device_remove_file(cdev, &dev_attr_freset);
err3:
	device_remove_file(cdev, &dev_attr_fmode);
err2:
	device_remove_file(cdev, &dev_attr_ftype);
err1:
	device_remove_file(cdev, &dev_attr_fchip_state);
err0:
	return ret;
}
static void husb320_destory_device(struct device *cdev)
{
	device_remove_file(cdev, &dev_attr_ftype);
	device_remove_file(cdev, &dev_attr_fmode);
	device_remove_file(cdev, &dev_attr_freset);
	device_remove_file(cdev, &dev_attr_ftdrptime);
	device_remove_file(cdev, &dev_attr_fdttime);
	device_remove_file(cdev, &dev_attr_fhostcur);
	device_remove_file(cdev, &dev_attr_fclientcur);
	device_remove_file(cdev, &dev_attr_fregdump);
	device_remove_file(cdev, &dev_attr_fauto_snk_th);
	device_remove_file(cdev, &dev_attr_fccdebounce);
	device_remove_file(cdev, &dev_attr_fdcable);
	device_remove_file(cdev, &dev_attr_fremedy);
}
static int husb320_power_set_icurrent_max(struct husb320_chip *chip,
		int icurrent)
{
#if 0
	const union power_supply_propval ret = {icurrent,};
	chip->ufp_power = icurrent;
	if (chip->usb_psy->set_property)
		return chip->usb_psy->set_property(chip->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);
	return -ENXIO;
#endif
	return 0;
}
static void husb320_bclvl_changed(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc, limit;
	u8 status, type;

	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		rc = husb320_reset_device(chip);
		if (rc < 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	status = rc & 0xFF;
	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}
	type = (status & HUSB320_ATTACH) ?
		(rc & HUSB320_TYPE_MASK) : HUSB320_TYPE_INVALID;
	dev_info(cdev, "%s: status=0x%02x, type=0x%02x\n",
			__func__, status, type);

	// make sure all TYPEs are correct here
	if (type == HUSB320_TYPE_SNK ||
			type == HUSB320_TYPE_PWR_AUD_ACC ||
			type == HUSB320_TYPE_DBG_ACC_SNK) {
		chip->bc_lvl = status & 0x06;
		chip->ufp_power = status & 0x06 >> 1;
		limit = (chip->bc_lvl == HUSB320_SNK_3000MA ? 3000 :
				(chip->bc_lvl == HUSB320_SNK_1500MA ? 1500 : 0));
		husb320_power_set_icurrent_max(chip, limit);
	}
	dev_info(cdev, "%s: bc_lvl=%d\n", __func__, chip->bc_lvl);

	if (!chip->bc_lvl && type == HUSB320_TYPE_SNK) {
		husb320_detach(chip);
	}
}
static void husb320_autosnk_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement autosnk changed work */
	struct device *cdev = &chip->client->dev;
	int rc;

	pr_info("%s: enter \n", __func__);

	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		rc = husb320_reset_device(chip);
		if (rc < 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}

	dev_info(cdev, "%s: status_reg=%d\n", __func__, rc);

	return;
}
static void husb320_vbus_changed(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status, type;

	/* get status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}
	status = rc & 0xFF;

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}
	type = (status & HUSB320_ATTACH) ?
		(rc & HUSB320_TYPE_MASK) : HUSB320_TYPE_INVALID;
	dev_err(cdev,
			"%s status=0x%02x, type=0x%02x\n", __func__, status, type);

	if (type == HUSB320_TYPE_SRC || type == HUSB320_TYPE_SRC_ACC)
		return;

	if (husb320_is_vbus_on(chip)) {
		dev_err(cdev, "%s: vbus voltage was high\n", __func__);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
	} else {
		husb320_detach(chip);
	}
	return;
}
static void husb320_fault_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement fault changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_orient_changed(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status;

	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		if (husb320_reset_device(chip) != 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	status = rc & HUSB320_ORIENT_MASK;

	if (rc & HUSB320_ATTACH) {
		if (status == 0x10)
			chip->orientation = HUSB320_ORIENT_CC1;
		else if (status == 0x20)
			chip->orientation = HUSB320_ORIENT_CC2;
		else
			chip->orientation = HUSB320_ORIENT_NONE;
	} else {
		chip->orientation = HUSB320_ORIENT_NONE;
	}
	typec_cc_orientation = chip->orientation;
	dev_info(cdev, "%s: orientation=0x%02x\n", __func__, chip->orientation);
	return;
}
static void husb320_remedy_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement remedy changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_frc_succ_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement frc succ changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_frc_fail_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement frc fail changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_rem_fail_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement rem fail changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_rem_vbon_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement rem vbon changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_rem_vboff_changed(struct husb320_chip *chip)
{
	/* TODO */
	/* implement rem vboff changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void husb320_attached_src(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (HUSB320_SNK | HUSB320_SNK_ACC)) {
		dev_err(cdev, "%s: donot support source mode\n", __func__);
	}

	typec_update_state(chip, TYPEC_STATE_ATTACHED_SRC);
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
	chip->type = HUSB320_TYPE_SRC;
	dev_info(cdev, "%s: chip->type=0x%02x\n", __func__, chip->type);
}
static void husb320_attached_snk(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	if (chip->mode & (HUSB320_SRC | HUSB320_SRC_ACC)) {
		dev_err(cdev, "%s: donot support sink mode\n", __func__);
	}

	typec_update_state(chip, TYPEC_STATE_ATTACHED_SNK);
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
	chip->type = HUSB320_TYPE_SNK;
	dev_info(cdev, "%s: chip->type=0x%02x\n", __func__, chip->type);
}
static void husb320_attached_dbg_acc(struct husb320_chip *chip)
{
	/*
	 * TODO
	 * need to implement
	 */
	pr_info("%s: enter \n", __func__);
	typec_update_state(chip, TYPEC_STATE_DEBUG_ACCESSORY);
}
static void husb320_attached_aud_acc(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	if (!(chip->mode & HUSB320_AUD_ACC)) {
		dev_err(cdev, "%s: not support accessory mode\n", __func__);
		if (husb320_reset_device(chip) < 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	/*
	 * TODO
	 * need to implement
	 */
	pr_info("%s: enter \n", __func__);
	typec_update_state(chip, TYPEC_STATE_AUDIO_ACCESSORY);
	//husb320_detach(chip);
}
static void husb320_detach(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_err(cdev, "%s: chip->type=0x%02x, chip->state=0x%02x\n",
			__func__, chip->type, chip->state);

	chip->type = HUSB320_TYPE_INVALID;
	chip->bc_lvl = HUSB320_SNK_0MA;
	chip->ufp_power = 0;

	typec_cc_orientation = 0x0;
	dev_err(cdev, "%s: typec_attach_old= %d\n",
			__func__, chip->tcpc->typec_attach_old);
	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;
	tcpci_notify_typec_state(chip->tcpc);
	if (chip->tcpc->typec_attach_old == TYPEC_ATTACHED_SRC) {
		tcpci_source_vbus(chip->tcpc,
				TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_0V, 0);
	}
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
}
static bool husb320_is_vbus_on(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client, HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return false;
	}

	return !!(rc & HUSB320_VBUSOK);
}
static void husb320_attach(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status, status1, type;

	/* get status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			HUSB320_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}

	status = rc & 0xFF;
	status1 = (rc >> 8) & 0xFF;

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}

	type = (status & HUSB320_ATTACH) ?
		(rc & HUSB320_TYPE_MASK) : HUSB320_TYPE_INVALID;
	dev_info(cdev, "%s: status=0x%02x, status1=0x%02x, type=0x%02x\n",
			__func__, status, status1, type);

	switch (type) {
	case HUSB320_TYPE_SRC:
	case HUSB320_TYPE_SRC_ACC:
		husb320_attached_src(chip);
		if (husb320_is_vbus_on(chip)) {
			dev_err(cdev, "%s: vbus voltage was high\n", __func__);
			break;
		}
		chip->tcpc_desc->rp_lvl = TYPEC_CC_RP_3_0;
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SRC) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SRC;
			tcpci_source_vbus(chip->tcpc,
					TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SOURCE_5V, 3000);
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SRC;
		}
		break;
	case HUSB320_TYPE_SNK:
		husb320_attached_snk(chip);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
		break;
	case HUSB320_TYPE_ACTV_CABLE:
		chip->type = type;
		break;
	case HUSB320_TYPE_DBG_ACC_SRC:
		break;
	case HUSB320_TYPE_DBG_ACC_SNK:
		husb320_attached_dbg_acc(chip);
		chip->type = type;
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
		break;
	case HUSB320_TYPE_AUD_ACC:
	case HUSB320_TYPE_PWR_AUD_ACC:
		husb320_attached_aud_acc(chip);
		chip->type = type;
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_AUDIO) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_AUDIO;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_AUDIO;
		}
		break;
	case HUSB320_TYPE_INVALID:
		husb320_detach(chip);
		dev_err(cdev, "%s: Invaild type[0x%02x]\n", __func__, type);
		break;
	default:
		husb320_detach(chip);
		dev_err(cdev, "%s: Unknwon type=0x%02x\n", __func__, type);
		break;
	}
}
static void husb320_work_handler(struct work_struct *work)
{
	struct husb320_chip *chip =
		container_of(work, struct husb320_chip, dwork);
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 int_sts = 0;
	u8 int_sts1 = 0;

	if (first_check == true) {
		return;
	}
	do {
	mutex_lock(&chip->mlock);
	/* get interrupt */
	rc = i2c_smbus_read_byte_data(chip->client, HUSB320_REG_INTERRUPT);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}

	int_sts = rc & HUSB320_INT_STS_MASK;
	dev_info(cdev, "%s: interrupt=0x%02x\n", __func__, int_sts);

	if (int_sts & HUSB320_I_DETACH) {
		husb320_detach(chip);
	} else {
		if (int_sts & HUSB320_I_ATTACH) {
			husb320_attach(chip);
		}
		if (int_sts & HUSB320_I_BC_LVL) {
			husb320_bclvl_changed(chip);
		}
		if (int_sts & HUSB320_I_AUTOSNK) {
			husb320_autosnk_changed(chip);
		}
		if (int_sts & HUSB320_I_VBUS_CHG) {
			husb320_vbus_changed(chip);
		}
		if (int_sts & HUSB320_I_FAULT) {
			husb320_fault_changed(chip);
		}
		if (int_sts & HUSB320_I_ORIENT) {
			husb320_orient_changed(chip);
		}
	}

	rc = i2c_smbus_read_byte_data(chip->client, HUSB320_REG_INTERRUPT1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt1\n", __func__);
		goto work_unlock;
	}

	int_sts1 = rc & HUSB320_INT1_STS_MASK;
	dev_info(cdev, "%s: interrupt_1=0x%02x\n", __func__, int_sts1);

	if (int_sts1 & HUSB320_I_REMEDY) {
		husb320_remedy_changed(chip);
	}
	if (int_sts1 & HUSB320_I_FRC_SUCC) {
		husb320_frc_succ_changed(chip);
	}
	if (int_sts1 & HUSB320_I_FRC_FAIL) {
		husb320_frc_fail_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_FAIL) {
		husb320_rem_fail_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_VBON) {
		husb320_rem_vbon_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_VBOFF) {
		husb320_rem_vboff_changed(chip);
	}

	i2c_smbus_write_byte_data(chip->client,
				HUSB320_REG_INTERRUPT, int_sts);
	i2c_smbus_write_byte_data(chip->client,
				HUSB320_REG_INTERRUPT1, int_sts1);
work_unlock:
	mutex_unlock(&chip->mlock);
	} while (platform_get_device_irq_state(chip));
}
static irqreturn_t husb320_interrupt(int irq, void *data)
{
	struct husb320_chip *chip = (struct husb320_chip *)data;
	if (!chip) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}
	queue_work(chip->cc_wq, &chip->dwork);
	return IRQ_HANDLED;
}
static int husb320_init_gpio(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int ret = 0;

	chip->pdata->int_gpio = of_get_named_gpio(cdev->of_node,
			"husb320,int-gpio", 0);
	dev_info(cdev, "%s: int_gpio: %d\n",
			__func__, chip->pdata->int_gpio);

	ret = devm_gpio_request(&chip->client->dev,
			chip->pdata->int_gpio, "type_c_port0");
	if (ret < 0) {
		dev_err(cdev, "Error: failed to request GPIO %d (ret = %d)\n",
				chip->pdata->int_gpio, ret);
	}

	ret = gpio_direction_input(chip->pdata->int_gpio);
	if (ret < 0) {
		dev_err(cdev,
				"Error: failed to set GPIO %d as input pin(ret = %d)\n",
				chip->pdata->int_gpio, ret);
	}

	chip->irq_gpio = gpio_to_irq(chip->pdata->int_gpio);
	if (chip->irq_gpio  <= 0) {
		dev_err(cdev, "gpio to irq fail, chip->irq(%d)\n",
				chip->irq_gpio);
	}

	ret = request_irq(chip->irq_gpio, husb320_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"husb320_int_gpio", chip);
	if (ret)
		dev_err(cdev, "unable to request int_gpio %d\n",
				chip->pdata->int_gpio);

	dev_info(cdev, "%s: name=%s, gpio=%d, IRQ number=%d\n",
			__func__, chip->tcpc_desc->name,
			chip->pdata->int_gpio, chip->irq_gpio);
	return ret;
}
static void husb320_free_gpio(struct husb320_chip *chip)
{
	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}
static int husb320_parse_dt(struct husb320_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct device_node *dev_node = cdev->of_node;
	struct husb320_data *data = chip->pdata;
	int rc = 0;

	rc = of_property_read_u32(dev_node,
			"husb320,init-mode", &data->init_mode);
	if (rc) {
		dev_err(cdev, "init mode is not available and set default\n");
		data->init_mode = HUSB320_DRP;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"husb320,host-current", &data->dfp_power);
	if (rc || (data->dfp_power > HUSB320_HOST_3000MA)) {
		dev_err(cdev,
				"host current is not available and set default\n");
		data->dfp_power = HUSB320_HOST_1500MA;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"husb320,drp-toggle-time", &data->tdrptime);
	if (rc || (data->tdrptime > HUSB320_TDRP_90MS)) {
		dev_err(cdev, "drp time is not available and set default\n");
		data->tdrptime = HUSB320_TDRP_70MS;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"husb320,drp-duty-time", &data->dttime);
	if (rc || (data->dttime > HUSB320_TGL_30PCT)) {
		dev_err(cdev,
				"drp dutycycle time not available and set default\n");
		data->dttime = HUSB320_TGL_60PCT;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"husb320,autosink-threshold",
			&data->autosnk_thres);
	if (rc || (data->autosnk_thres > HUSB320_AUTO_SNK_TH_3P3V)) {
		dev_err(cdev,
				"auto sink threshold not available and set default\n");
		data->autosnk_thres = HUSB320_AUTO_SNK_TH_3P1V;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"husb320,cc-debounce-time", &data->ccdebtime);
	if (rc || (data->ccdebtime > HUSB320_TCCDEB_180MS)) {
		dev_err(cdev,
				"CC debounce time not available and set default\n");
		data->ccdebtime = HUSB320_TCCDEB_150MS;
		rc = 0;
	}
	dev_err(cdev,
			"%s: init_mode:%d dfp_power:%d tdrp_time:%d dttime:%d\n",
			__func__, data->init_mode, data->dfp_power,
			data->tdrptime, data->dttime);
	dev_err(cdev, "%s: autosnk_thres:%d ccdebtime:%d\n",
			__func__, data->autosnk_thres, data->ccdebtime);
	return rc;
}
#ifdef HAVE_DR
static enum dual_role_property typec_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};
/* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
static int dual_role_get_local_prop(
		struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		unsigned int *val)
{
	struct husb320_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	int ret = 0;
	if (!client)
		return -EINVAL;
	chip = i2c_get_clientdata(client);
	mutex_lock(&chip->mlock);
	if (chip->state == TYPEC_STATE_ATTACHED_SRC) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SRC;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			ret = -EINVAL;
	} else if (chip->state == TYPEC_STATE_ATTACHED_SNK) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SNK;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			ret = -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			ret = -EINVAL;
	}
	mutex_unlock(&chip->mlock);
	return ret;
}
/* Decides whether userspace can change a specific property */
static int dual_role_is_writeable(struct dual_role_phy_instance *drp,
		enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}
/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switched to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
static int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop,
		const unsigned int *val)
{
	struct husb320_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	u8 mode, target_state, fallback_mode, fallback_state;
	int rc;
	struct device *cdev;

	if (!client)
		return -EIO;
	chip = i2c_get_clientdata(client);
	cdev = &client->dev;
	if (prop == DUAL_ROLE_PROP_MODE) {
		if (*val == DUAL_ROLE_PROP_MODE_DFP) {
			dev_dbg(cdev, "%s: Setting SRC mode\n", __func__);
			mode = HUSB320_SRC;
			fallback_mode = HUSB320_SNK;
			target_state = TYPEC_STATE_ATTACHED_SRC;
			fallback_state = TYPEC_STATE_ATTACHED_SNK;
		} else if (*val == DUAL_ROLE_PROP_MODE_UFP) {
			dev_dbg(cdev, "%s: Setting SNK mode\n", __func__);
			mode = HUSB320_SNK;
			fallback_mode = HUSB320_SRC;
			target_state = TYPEC_STATE_ATTACHED_SNK;
			fallback_state = TYPEC_STATE_ATTACHED_SRC;
		} else {
			dev_err(cdev, "%s: Trying to set invalid mode\n",
					__func__);
			return -EINVAL;
		}
	} else {
		dev_err(cdev, "%s: Property cannot be set\n", __func__);
		return -EINVAL;
	}
	if (chip->state == target_state)
		return 0;
	mutex_lock(&chip->mlock);
	if ((mode == HUSB320_SRC) &&
			(chip->state == TYPEC_STATE_ATTACHED_SNK)) {
		husb320_set_manual_reg(chip, HUSB320_FORCE_SRC);
	} else if (mode == HUSB320_SNK &&
			chip->state == TYPEC_STATE_ATTACHED_SRC) {
		husb320_set_manual_reg(chip, HUSB320_FORCE_SNK);
	}
	mutex_unlock(&chip->mlock);
	return 0;
}
#endif /* HAVE_DR */
int husb320_alert_status_clear(struct tcpc_device *tcpc,
		uint32_t mask)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
int husb320_fault_status_clear(struct tcpc_device *tcpc,
		uint8_t status)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
int husb320_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
int husb320_get_alert_status(struct tcpc_device *tcpc,
		uint32_t *alert)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_get_power_status(struct tcpc_device *tcpc,
		uint16_t *pwr_status)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
int husb320_get_fault_status(struct tcpc_device *tcpc,
		uint8_t *status)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_set_cc(struct tcpc_device *tcpc, int pull)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_set_low_rp_duty(struct tcpc_device *tcpc,
		bool low_rp)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_set_vconn(struct tcpc_device *tcpc, int enable)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}
static int husb320_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
	pr_info("%s: enter \n", __func__);
	return 0;
}

int husb320_get_mode(struct tcpc_device *tcpc, int *typec_mode)
{
	struct device *cdev = &g_client->dev;
	int rc;
	u8 type;


	rc = i2c_smbus_read_byte_data(g_client,
			HUSB320_REG_TYPE);
	if (rc < 0) {
		*typec_mode = 0;
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return 0;
	}

	type = rc & HUSB320_TYPE_MASK;

	switch (type) {
	case HUSB320_TYPE_SRC:
	case HUSB320_TYPE_SRC_ACC:
	case HUSB320_TYPE_DBG_ACC_SRC:
		*typec_mode = POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
		break;
	case HUSB320_TYPE_SNK:
		*typec_mode = POWER_SUPPLY_TYPEC_SINK;
		break;
	default:
		*typec_mode = 0;
		dev_err(cdev, "%s: Invaild type[0x%02x]\n", __func__, type);
		break;
	}
	pr_err("dhx---husb320 get typec mode type:%d, reg:%x\n", *typec_mode, type);
	return 0;

}

int husb320_set_role(struct tcpc_device *tcpc, int state)
{
	int rc = 0;
	u8 reg;

	pr_err("dhx--set role %d\n", state);

	if (state == REVERSE_CHG_SOURCE)
		reg = HUSB320_FORCE_SRC;
	else
		return 0;

	rc = i2c_smbus_write_byte_data(g_client, HUSB320_REG_MANUAL, reg);

	if (rc < 0) {
		pr_err("%s: failed to write manual(%d)\n", __func__, rc);
		return rc;
	}
	return rc;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int husb320_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_protocol_reset(struct tcpc_device *tcpc_dev)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
static int husb320_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
		pr_info("%s enter \n", __func__);
	return 0;
}
#endif // CONFIG_USB_POWER_DELIVERY

static struct tcpc_ops husb320_tcpc_ops = {
	.init = husb320_tcpc_init,
	.alert_status_clear = husb320_alert_status_clear,
	.fault_status_clear = husb320_fault_status_clear,
	.get_alert_mask = husb320_get_alert_mask,
	.get_alert_status = husb320_get_alert_status,
	.get_power_status = husb320_get_power_status,
	.get_fault_status = husb320_get_fault_status,
	.get_cc = husb320_get_cc,
	.set_cc = husb320_set_cc,
	.set_polarity = husb320_set_polarity,
	.set_low_rp_duty = husb320_set_low_rp_duty,
	.set_vconn = husb320_set_vconn,
	//.set_role = husb320_set_role,
	.get_mode = husb320_get_mode,
	.deinit = husb320_tcpc_deinit,
#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = husb320_set_msg_header,
	.set_rx_enable = husb320_set_rx_enable,
	.protocol_reset = husb320_protocol_reset,
	.get_message = husb320_get_message,
	.transmit = husb320_transmit,
	.set_bist_test_mode = husb320_set_bist_test_mode,
	.set_bist_carrier_mode = husb320_set_bist_carrier_mode,
#endif
};
static void husb320_first_check_typec_work(struct work_struct *work)
{
	struct husb320_chip *chip = container_of(work,
			struct husb320_chip, first_check_typec_work.work);
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 int_sts = 0;
	u8 int_sts1 = 0;

	do {
	mutex_lock(&chip->mlock);
	/* get interrupt */
	rc = i2c_smbus_read_byte_data(chip->client, HUSB320_REG_INTERRUPT);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}

	first_check = false;
	int_sts = rc & HUSB320_INT_STS_MASK;
	dev_info(cdev, "%s: interrupt=0x%02x\n", __func__, int_sts);
	if (int_sts & HUSB320_I_DETACH) {
		husb320_detach(chip);
	} else {
		if (int_sts & HUSB320_I_ATTACH) {
			husb320_attach(chip);
		}
		if (int_sts & HUSB320_I_BC_LVL) {
			husb320_bclvl_changed(chip);
		}
		if (int_sts & HUSB320_I_AUTOSNK) {
			husb320_autosnk_changed(chip);
		}
		if (int_sts & HUSB320_I_VBUS_CHG) {
			husb320_vbus_changed(chip);
		}
		if (int_sts & HUSB320_I_FAULT) {
			husb320_fault_changed(chip);
		}
		if (int_sts & HUSB320_I_ORIENT) {
			husb320_orient_changed(chip);
		}
	}

	/* get interrupt1 */
	rc = i2c_smbus_read_byte_data(chip->client, HUSB320_REG_INTERRUPT1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt1\n", __func__);
		goto work_unlock;
	}
	int_sts1 = rc & HUSB320_INT1_STS_MASK;
	dev_info(cdev, "%s: interrupt_1=0x%02x\n", __func__, int_sts1);
	if (int_sts1 & HUSB320_I_REMEDY) {
		husb320_remedy_changed(chip);
	}
	if (int_sts1 & HUSB320_I_FRC_SUCC) {
		husb320_frc_succ_changed(chip);
	}
	if (int_sts1 & HUSB320_I_FRC_FAIL) {
		husb320_frc_fail_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_FAIL) {
		husb320_rem_fail_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_VBON) {
		husb320_rem_vbon_changed(chip);
	}
	if (int_sts1 & HUSB320_I_REM_VBOFF) {
		husb320_rem_vboff_changed(chip);
	}

	i2c_smbus_write_byte_data(chip->client,
				HUSB320_REG_INTERRUPT, int_sts);
	i2c_smbus_write_byte_data(chip->client,
				HUSB320_REG_INTERRUPT1, int_sts1);
work_unlock:
	mutex_unlock(&chip->mlock);
	} while (platform_get_device_irq_state(chip));
}
static int husb320_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct husb320_chip *chip;
	struct device *cdev = &client->dev;
	//	struct power_supply *usb_psy;
	struct husb320_data *data ;
	struct tcpc_desc *desc;
#ifdef HAVE_DR
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
#endif /* HAVE_DR */
	int ret = 0;
	first_check = true;
	//	usb_psy = power_supply_get_by_name("usb");
	//	if (!usb_psy) {
	//		dev_err(cdev, "USB supply not found, deferring probe\n");
	//		return -EPROBE_DEFER;
	//	}
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(cdev, "%s: smbus data not supported!\n", __func__);
		return -EIO;
	}
	chip = devm_kzalloc(cdev, sizeof(struct husb320_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(cdev, "%s: can't alloc husb320_chip\n", __func__);
		return -ENOMEM;
	}
	chip->client = client;
	i2c_set_clientdata(client, chip);
	g_client = chip->client;
	ret = husb320_read_device_id(chip);
	if (ret != HUSB320_REV) {
		dev_err(cdev, "%s: HUSB320 not support\n", __func__);
		goto err1;
	}
	data = devm_kzalloc(cdev,
			sizeof(struct husb320_data), GFP_KERNEL);
	if (!data) {
		dev_err(cdev, "%s: can't alloc husb320_data\n", __func__);
		ret = -ENOMEM;
		goto err1;
	}
	chip->pdata = data;
	ret = husb320_parse_dt(chip);
	if (ret) {
		dev_err(cdev, "%s: can't parse dt\n", __func__);
		goto err2;
	}

	chip->type = HUSB320_TYPE_INVALID;
	chip->state = TYPEC_STATE_DISABLED;
	chip->bc_lvl = HUSB320_SNK_0MA;
	chip->ufp_power = 0;
	//	chip->usb_psy = usb_psy;

	chip->cc_wq = alloc_ordered_workqueue("husb320-wq", WQ_HIGHPRI);
	if (!chip->cc_wq) {
		dev_err(cdev, "%s: unable to create workqueue husb320-wq\n",
				__func__);
		goto err2;
	}
	INIT_WORK(&chip->dwork, husb320_work_handler);
	sema_init(&chip->suspend_lock, 1);
	INIT_DELAYED_WORK(&chip->first_check_typec_work,
			husb320_first_check_typec_work);
	mutex_init(&chip->mlock);
	ret = husb320_create_devices(cdev);
	if (ret < 0) {
		dev_err(cdev, "%s: could not create devices\n", __func__);
		goto err3;
	}

	desc = devm_kzalloc(cdev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = kzalloc(13, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strcpy((char *)desc->name, "type_c_port0");
	desc->role_def = TYPEC_ROLE_TRY_SRC;
	desc->rp_lvl = TYPEC_CC_RP_1_5;
	desc->notifier_supply_num = 3;
	chip->tcpc_desc = desc;
	dev_info(cdev, "%s: type_c_port0, role=%d\n",
			__func__, desc->role_def);

	chip->tcpc = tcpc_device_register(cdev,
			desc, &husb320_tcpc_ops, chip);
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;

	ret = husb320_init_gpio(chip);
	if (ret) {
		dev_err(cdev, "%s: fail to init gpio\n", __func__);
		goto err4;
	}
#ifdef HAVE_DR
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		desc = devm_kzalloc(cdev, sizeof(struct dual_role_phy_desc),
				GFP_KERNEL);
		if (!desc) {
			dev_err(cdev,
					"%s: unable to allocate dual role descriptor\n",
					__func__);
			goto err4;
		}
		desc->name = "otg_default";
		desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		desc->get_property = dual_role_get_local_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = typec_drp_properties;
		desc->num_properties = ARRAY_SIZE(typec_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(cdev, desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->desc = desc;
	}
#endif /* HAVE_DR */
	schedule_delayed_work(&chip->first_check_typec_work,
			msecs_to_jiffies(3000));
	ret = husb320_reset_device(chip);
	if (ret) {
		dev_err(cdev, "%s: failed to initialize\n", __func__);
		goto err5;
	}

	enable_irq_wake(chip->irq_gpio);
	dev_err(cdev, "%s: device probe successfully\n", __func__);
	return 0;
err5:
#ifdef HAVE_DR
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF))
		devm_kfree(cdev, chip->desc);
#endif /* HAVE_DR */
err4:
	cancel_delayed_work_sync(&chip->first_check_typec_work);
	husb320_destory_device(cdev);
err3:
	tcpc_device_unregister(cdev, chip->tcpc);
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	husb320_free_gpio(chip);
err2:
	devm_kfree(cdev, chip->pdata);
err1:
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);
	return ret;
}
static int husb320_remove(struct i2c_client *client)
{
	struct husb320_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	dev_err(cdev, "%s: enter\n", __func__);

	if (!chip) {
		dev_err(cdev, "%s: chip is null\n", __func__);
		return -ENODEV;
	}
	if (chip->irq_gpio > 0)
		devm_free_irq(cdev, chip->irq_gpio, chip);
#ifdef HAVE_DR
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(cdev, chip->dual_role);
		devm_kfree(cdev, chip->desc);
	}
#endif /* HAVE_DR */

	cancel_delayed_work_sync(&chip->first_check_typec_work);
	tcpc_device_unregister(cdev, chip->tcpc);
	husb320_destory_device(cdev);

	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	husb320_free_gpio(chip);
	devm_kfree(cdev, chip->pdata);
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);
	return 0;
}
static void husb320_shutdown(struct i2c_client *client)
{
	struct husb320_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;
	int rc = 0;

	dev_err(cdev, "%s: enter\n", __func__);
	if (!chip) {
        	dev_err(cdev, "%s: chip is null\n", __func__);
        	return;
        }
	rc = i2c_smbus_write_byte_data(chip->client,
			HUSB320_REG_RESET,
			HUSB320_SW_RESET);
	if (rc < 0) {
		dev_err(cdev, "%s: reset fails\n", __func__);
	}
	if (husb320_set_mode(chip, HUSB320_SNK) != 0)
		dev_err(cdev, "%s: failed to set sink mode\n", __func__);
	rc = husb320_dangling_cbl_en(chip, false);
	if (rc < 0)
		dev_err(cdev,
				"%s: fail to disable dangling cbl func\n", __func__);
	rc = husb320_remedy_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable remedy func\n", __func__);
	rc = husb320_auto_snk_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable auto snk func\n", __func__);
}
#ifdef CONFIG_PM
static int husb320_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	dev_err(cdev, "%s: enter\n", __func__);

	if (!chip) {
		dev_err(cdev, "%s: No device is available!\n", __func__);
		return -EINVAL;
	}

	down(&chip->suspend_lock);
	return 0;
}
static int husb320_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct husb320_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;
	int rc = 0;

	dev_err(cdev, "%s: enter\n", __func__);

	rc = i2c_smbus_read_byte_data(chip->client,
			HUSB320_REG_CONTROL1);

	if (rc < 0) {
		dev_err(cdev, "%s: failed to read control1\n", __func__);
	}
	if (!(rc & 0x08)) {
		rc = husb320_reset_device(chip);
		if (rc) {
			dev_err(cdev, "%s: failed to initialize chip\n", __func__);
		}
	}

	if (!chip) {
		dev_err(cdev, "%s: No device is available!\n", __func__);
		return -EINVAL;
	}

	up(&chip->suspend_lock);
	return 0;
}
static const struct dev_pm_ops husb320_dev_pm_ops = {
	.suspend = husb320_suspend,
	.resume  = husb320_resume,
};
#endif
static const struct i2c_device_id husb320_id_table[] = {
	{"husb320", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, husb320_id_table);
#ifdef CONFIG_OF
static struct of_device_id husb320_match_table[] = {
	{.compatible = "hynetek,husb320",},
	{},
};
#else
#define husb320_match_table NULL
#endif
static struct i2c_driver husb320_i2c_driver = {
	.driver = {
		.name = "husb320",
		.owner = THIS_MODULE,
		.of_match_table = husb320_match_table,
#ifdef CONFIG_PM
		.pm = &husb320_dev_pm_ops,
#endif
	},
	.probe = husb320_probe,
	.remove = husb320_remove,
	.shutdown = husb320_shutdown,
	.id_table = husb320_id_table,
};
static __init int husb320_i2c_init(void)
{
	return i2c_add_driver(&husb320_i2c_driver);
}
static __exit void husb320_i2c_exit(void)
{
	i2c_del_driver(&husb320_i2c_driver);
}
subsys_initcall(husb320_i2c_init);
module_exit(husb320_i2c_exit);
MODULE_AUTHOR("mike.gao@hynetek.com");
MODULE_DESCRIPTION("I2C bus driver for husb320 USB Type-C");
MODULE_LICENSE("GPL v2");
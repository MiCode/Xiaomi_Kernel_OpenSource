/*
 * Copyright (c) 2018, ON Semiconductor Inc. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * fusb303 USB TYPE-C Configuration Controller driver
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
#define FUSB303_REG_DEVICEID            0x01
#define FUSB303_REG_DEVICETYPE          0x02
#define FUSB303_REG_PORTROLE            0x03
#define FUSB303_REG_CONTROL             0x04
#define FUSB303_REG_CONTROL1            0x05
#define FUSB303_REG_MANUAL              0x09
#define FUSB303_REG_RESET               0x0A
#define FUSB303_REG_MASK                0x0E
#define FUSB303_REG_MASK1               0x0F
#define FUSB303_REG_STATUS              0x11
#define FUSB303_REG_STATUS1             0x12
#define FUSB303_REG_TYPE                0x13
#define FUSB303_REG_INTERRUPT           0x14
#define FUSB303_REG_INTERRUPT1          0x15
/* Register Values */
#define FUSB303_REV                     0x10
#define FUSB303_REVTYPE                 0x01
/*  PORTROLE (03h)  */
#define FUSB303_ORIENTDEB               BIT(6)
#define FUSB303_TRY_NORMAL              0
#define FUSB303_TRY_SNK                 1
#define FUSB303_TRY_SRC                 2
#define FUSB303_TRY_DISABLE             3
#define FUSB303_AUD_ACC                 BIT(3)
#define FUSB303_DRP                     BIT(2)
#define FUSB303_SNK                     BIT(1)
#define FUSB303_SRC                     BIT(0)
#define FUSB303_DRP_ACC                (FUSB303_DRP|\
		FUSB303_AUD_ACC)
#define FUSB303_SNK_ACC                (FUSB303_SNK|\
		FUSB303_AUD_ACC)
#define FUSB303_SRC_ACC                (FUSB303_SRC|\
		FUSB303_AUD_ACC)
/*  CONTROL (04h)  */
#define FUSB303_TDRP_60MS               0
#define FUSB303_TDRP_70MS               1
#define FUSB303_TDRP_80MS               2
#define FUSB303_TDRP_90MS               3
#define FUSB303_TGL_60PCT               0
#define FUSB303_TGL_50PCT               1
#define FUSB303_TGL_40PCT               2
#define FUSB303_TGL_30PCT               3
#define FUSB303_DCABLE_EN               BIT(3)
#define FUSB303_HOST_0MA                0
#define FUSB303_HOST_DEFAULT            1
#define FUSB303_HOST_1500MA             2
#define FUSB303_HOST_3000MA             3
#define FUSB303_INT_ENABLE              0x00
#define FUSB303_INT_DISABLE             BIT(0)
/*  CONTROL1 (05h)  */
#define FUSB303_REMEDY_EN               BIT(7)
#define FUSB303_AUTO_SNK_TH_3P0V        0
#define FUSB303_AUTO_SNK_TH_3P1V        1
#define FUSB303_AUTO_SNK_TH_3P2V        2
#define FUSB303_AUTO_SNK_TH_3P3V        3
#define FUSB303_AUTO_SNK_EN             BIT(4)
#define FUSB303_ENABLE                  1
#define FUSB303_DISABLE                 0
#define FUSB303_TCCDEB_120MS            0
#define FUSB303_TCCDEB_130MS            1
#define FUSB303_TCCDEB_140MS            2
#define FUSB303_TCCDEB_150MS            3
#define FUSB303_TCCDEB_160MS            4
#define FUSB303_TCCDEB_170MS            5
#define FUSB303_TCCDEB_180MS            6
/*  MANUAL (09h)  */
#define FUSB303_FORCE_SRC               BIT(5)
#define FUSB303_FORCE_SNK               BIT(4)
#define FUSB303_UNATT_SNK               BIT(3)
#define FUSB303_UNATT_SRC               BIT(2)
#define FUSB303_DISABLED                BIT(1)
#define FUSB303_ERR_REC                 BIT(0)
/*  RESET (0Ah)  */
#define FUSB303_DISABLED_CLEAR          0x00
#define FUSB303_SW_RESET                BIT(0)
/*  MASK (0Eh)  */
#define FUSB303_M_ORIENT                BIT(6)
#define FUSB303_M_FAULT                 BIT(5)
#define FUSB303_M_VBUS_CHG              BIT(4)
#define FUSB303_M_AUTOSNK               BIT(3)
#define FUSB303_M_BC_LVL                BIT(2)
#define FUSB303_M_DETACH                BIT(1)
#define FUSB303_M_ATTACH                BIT(0)
/*  MASK1 (0Fh)  */
#define FUSB303_M_REM_VBOFF             BIT(6)
#define FUSB303_M_REM_VBON              BIT(5)
#define FUSB303_M_REM_FAIL              BIT(3)
#define FUSB303_M_FRC_FAIL              BIT(2)
#define FUSB303_M_FRC_SUCC              BIT(1)
#define FUSB303_M_REMEDY                BIT(0)
/*  STATUS (11h)  */
#define FUSB303_AUTOSNK                 BIT(7)
#define FUSB303_VSAFE0V                 BIT(6)
#define FUSB303_ORIENT_NONE             0
#define FUSB303_ORIENT_CC1              1
#define FUSB303_ORIENT_CC2              2
#define FUSB303_ORIENT_FAULT            3
#define FUSB303_VBUSOK                  BIT(3)
#define FUSB303_SNK_0MA                 0x00
#define FUSB303_SNK_DEFAULT             0x02
#define FUSB303_SNK_1500MA              0x04
#define FUSB303_SNK_3000MA              0x06
#define FUSB303_ATTACH                  BIT(0)
/*  STATUS1 (12h)  */
#define FUSB303_FAULT                   BIT(1)
#define FUSB303_REMEDY                  BIT(0)
/*  TYPE (13h)  */
#define FUSB303_TYPE_DBG_ACC_SRC        BIT(6)
#define FUSB303_TYPE_DBG_ACC_SNK        BIT(5)
#define FUSB303_TYPE_SNK                BIT(4)
#define FUSB303_TYPE_SRC                BIT(3)
#define FUSB303_TYPE_ACTV_CABLE         BIT(2)
#define FUSB303_TYPE_PWR_AUD_ACC        BIT(1)
#define FUSB303_TYPE_AUD_ACC            BIT(0)
#define FUSB303_TYPE_INVALID            0x00
#define FUSB303_TYPE_SRC_ACC           (FUSB303_TYPE_SRC|\
		FUSB303_TYPE_ACTV_CABLE)
/*  INTERRUPT (14h)  */
#define FUSB303_I_ORIENT                BIT(6)
#define FUSB303_I_FAULT                 BIT(5)
#define FUSB303_I_VBUS_CHG              BIT(4)
#define FUSB303_I_AUTOSNK               BIT(3)
#define FUSB303_I_BC_LVL                BIT(2)
#define FUSB303_I_DETACH                BIT(1)
#define FUSB303_I_ATTACH                BIT(0)
/*  INTERRUPT1 (15h)  */
#define FUSB303_I_REM_VBOFF             BIT(6)
#define FUSB303_I_REM_VBON              BIT(5)
#define FUSB303_I_REM_FAIL              BIT(3)
#define FUSB303_I_FRC_FAIL              BIT(2)
#define FUSB303_I_FRC_SUCC              BIT(1)
#define FUSB303_I_REMEDY                BIT(0)
/* Mask */
#define FUSB303_ORIEN_DBG_ACC_MASK      0x40
#define FUSB303_PORTROLE_MASK           0x3F
#define FUSB303_TRY_MASK                0x30

#define FUSB303_TDRP_MASK               0xC0
#define FUSB303_TGL_MASK                0x30
#define FUSB303_DCBL_EN_MASK            0x08
#define FUSB303_HOST_CUR_MASK           0x06
#define FUSB303_INT_MASK                0x01

#define FUSB303_REMDY_EN_MASK           0x80
#define FUSB303_AUTO_SNK_TH_MASK        0x60
#define FUSB303_AUTO_SNK_EN_MASK        0x10
#define FUSB303_ENABLE_MASK             0x08
#define FUSB303_TCCDEB_MASK             0x07

#define FUSB303_ORIENT_MASK             0x30
#define FUSB303_BCLVL_MASK              0x06
#define FUSB303_TYPE_MASK               0x7F

#define FUSB303_INT_STS_MASK            0x7F
#define FUSB303_INT1_STS_MASK           0x6F
/* FUSB STATES */
#define FUSB_STATE_DISABLED             0x00
#define FUSB_STATE_ERROR_RECOVERY       0x01
#define FUSB_STATE_UNATTACHED_SNK       0x02
#define FUSB_STATE_UNATTACHED_SRC       0x03
#define FUSB_STATE_ATTACHWAIT_SNK       0x04
#define FUSB_STATE_ATTACHWAIT_SRC       0x05
#define FUSB_STATE_ATTACHED_SNK         0x06
#define FUSB_STATE_ATTACHED_SRC         0x07
#define FUSB_STATE_AUDIO_ACCESSORY      0x08
#define FUSB_STATE_DEBUG_ACCESSORY      0x09
#define FUSB_STATE_TRY_SNK              0x0A
#define FUSB_STATE_TRYWAIT_SRC          0x0B
#define FUSB_STATE_TRY_SRC              0x0C
#define FUSB_STATE_TRYWAIT_SNK          0x0D
#define FUSB_STATE_FORCE_SRC            0x0E
#define FUSB_STATE_FORCE_SNK            0x0F
/* wake lock timeout in ms */
#define FUSB303_WAKE_LOCK_TIMEOUT       1000
#define ROLE_SWITCH_TIMEOUT             1500
#define REVERSE_CHG_SOURCE				0X01
#define REVERSE_CHG_SINK				0X02
#define REVERSE_CHG_DRP					0X03
#define REVERSE_CHG_TEST				0X04

extern uint8_t     typec_cc_orientation;
extern int tcpci_report_usb_port_attached(struct tcpc_device *tcpc);
extern int tcpci_report_usb_port_detached(struct tcpc_device *tcpc);
bool first_check = true;
struct fusb303_chip *chip_chg;
struct i2c_client *g_client;
static bool fusb303_is_vbus_on(struct fusb303_chip *chip);
struct fusb303_data {
	int int_gpio;
	u32 init_mode;
	u32 dfp_power;
	u32 tdrptime;
	u32 dttime;
	u32 autosnk_thres;
	u32 ccdebtime;
	bool try_snk_emulation;
	u32 ttry_timeout;
	u32 ccdebounce_timeout;
};
struct fusb303_chip {
	struct i2c_client *client;
	struct fusb303_data *pdata;
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
	bool triedsnk;
	int try_attcnt;
	struct work_struct dwork;
	struct delayed_work twork;
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
#define fusb_update_state(chip, st) \
	do { \
	if (chip && st < FUSB_STATE_FORCE_SNK) { \
		chip->state = st; \
		dev_info(&chip->client->dev, "%s: %s\n", __func__, #st); \
		wake_up_interruptible(&mode_switch); \
	} \
	} while (0)
#define STR(s)    #s
#define STRV(s)   STR(s)
static void fusb303_detach(struct fusb303_chip *chip);
DECLARE_WAIT_QUEUE_HEAD(mode_switch);
static int fusb303_write_masked_byte(struct i2c_client *client,
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
static int fusb303_read_device_id(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	rc = i2c_smbus_read_byte_data(chip->client,
				FUSB303_REG_DEVICEID);
	if (rc < 0)
		return rc;
	chip->dev_id = rc;
	dev_info(cdev, "%s: device id: 0x%02x\n", __func__, rc);
	return rc;
}
static int fusb303_update_status(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u16 control_now;
	u16 control1_now;

	/* read mode & control register */
	rc = i2c_smbus_read_word_data(chip->client, FUSB303_REG_PORTROLE);
	if (rc < 0) {
		dev_err(cdev, "%s: fail to read mode\n", __func__);
		return rc;
	}
	chip->mode = rc & FUSB303_PORTROLE_MASK;
	control_now = (rc >> 8) & 0xFF;
	chip->dfp_power = BITS_GET(control_now, FUSB303_HOST_CUR_MASK);
	chip->tdrptime = BITS_GET(control_now, FUSB303_TDRP_MASK);
	chip->dttime = BITS_GET(control_now, FUSB303_TGL_MASK);

	rc = i2c_smbus_read_byte_data(chip->client,
			FUSB303_REG_CONTROL1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read control1 reg\n", __func__);
		return rc;
	}
	control1_now = rc ;
	chip->autosnk_thres = BITS_GET(control1_now, FUSB303_AUTO_SNK_TH_MASK);
	chip->ccdebtime = BITS_GET(control1_now, FUSB303_TCCDEB_MASK);
	return 0;
}
static int fusb303_set_manual_reg(struct fusb303_chip *chip, u8 state)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	if (state > FUSB303_FORCE_SRC)
		return -EINVAL;
	if (state & FUSB303_DISABLED) {
		dev_err(cdev,
				"%s: return err if sw try to disable device state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	if ((state & FUSB303_FORCE_SRC) && (chip->type == FUSB303_TYPE_SRC)) {
		dev_err(cdev,
				"%s: return err if chip already in src, state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	if ((state & FUSB303_FORCE_SNK) && (chip->type == FUSB303_TYPE_SNK)) {
		dev_err(cdev,
				"%s: return err if chip already in snk, state=%d\n",
				__func__, state);
		return -EINVAL;
	}
	rc = i2c_smbus_write_byte_data(chip->client,
			FUSB303_REG_MANUAL,
			state);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write manual, errno=%d\n",
				__func__, rc);
		return rc;
	}
	dev_info(cdev, "%s: state=%d\n", __func__, state);
	return rc;
}
static int fusb303_set_chip_state(struct fusb303_chip *chip, u8 state)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	dev_info(cdev, "%s: update manual reg=%d\n", __func__, state);
	rc = fusb303_set_manual_reg(chip, state);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to write manual reg\n", __func__);
		return rc;
	}
	chip->state = state;
	return rc;
}
static int fusb303_set_mode(struct fusb303_chip *chip, u8 mode)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (mode != chip->mode) {
		rc = i2c_smbus_write_byte_data(chip->client,
				FUSB303_REG_PORTROLE, mode);
		if (rc < 0) {
			dev_err(cdev, "%s: failed to write mode\n", __func__);
			return rc;
		}
		chip->mode = mode;
	}
	dev_dbg(cdev, "%s: mode (%d)(%d)\n", __func__, chip->mode, mode);
	return rc;
}
static int fusb303_set_dfp_power(struct fusb303_chip *chip, u8 hcurrent)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (hcurrent > FUSB303_HOST_3000MA) {
		dev_err(cdev, "hcurrent(%d) is unavailable\n", hcurrent);
		return -EINVAL;
	}
	if (hcurrent == chip->dfp_power) {
		dev_dbg(cdev, "value is not updated(%d)\n", hcurrent);
		return rc;
	}
	rc = fusb303_write_masked_byte(chip->client,
					FUSB303_REG_CONTROL,
					FUSB303_HOST_CUR_MASK,
					hcurrent);
	if (rc < 0) {
		dev_err(cdev, "failed to write current(%d)\n", rc);
		return rc;
	}
	chip->dfp_power = hcurrent;
	dev_dbg(cdev, "%s: host current(%d)\n", __func__, hcurrent);
	return rc;
}
/*
 * When 3A capable DRP device is connected without VBUS,
 * DRP always detect it as SINK device erroneously.
 * Since USB Type-C specification 1.0 and 1.1 doesn't
 * consider this corner case, apply workaround for this case.
 * Set host mode current to 1.5A initially, and then change
 * it to default USB current right after detection SINK port.
 */
static int fusb303_init_force_dfp_power(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	rc = fusb303_write_masked_byte(chip->client,
					FUSB303_REG_CONTROL,
					FUSB303_HOST_CUR_MASK,
					FUSB303_HOST_1500MA);
	if (rc < 0) {
		dev_err(cdev, "failed to write current\n");
		return rc;
	}
	chip->dfp_power = FUSB303_HOST_1500MA;
	dev_dbg(cdev, "%s: host current (%d)\n", __func__, rc);
	return rc;
}
static int fusb303_set_drp_cycle_time(struct fusb303_chip *chip,
		u8 drp_cycle_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (drp_cycle_time > FUSB303_TDRP_90MS) {
		dev_err(cdev, "drp cycle time(%d) is unavailable\n",
				drp_cycle_time);
		return -EINVAL;
	}
	if (drp_cycle_time == chip->tdrptime) {
		dev_dbg(cdev, "value is not updated drp cycle time:(%d)\n",
				drp_cycle_time);
		return rc;
	} else if (drp_cycle_time != chip->tdrptime) {
		rc = fusb303_write_masked_byte(chip->client,
						FUSB303_REG_CONTROL,
						FUSB303_TDRP_MASK,
						drp_cycle_time);
		if (rc < 0) {
			dev_err(cdev, "failed to write drp cycle time\n");
			return rc;
		}
	}
	chip->tdrptime = drp_cycle_time;
	dev_dbg(cdev, "%s: drp cycle time: (%d)\n", __func__, chip->tdrptime);
	return rc;
}

static int fusb303_set_toggle_time(struct fusb303_chip *chip,
		u8 toggle_dutycycle_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (toggle_dutycycle_time > FUSB303_TGL_30PCT) {
		dev_err(cdev, "toggle dutycycle time(%d) is unavailable\n",
				toggle_dutycycle_time);
		return -EINVAL;
	}
	if (toggle_dutycycle_time == chip->dttime) {
		dev_dbg(cdev,
			"value is not updated toggle dutycycle time:(%d)\n",
			toggle_dutycycle_time);
		return rc;
	} else if (toggle_dutycycle_time != chip->dttime) {
		rc = fusb303_write_masked_byte(chip->client,
						FUSB303_REG_CONTROL,
						FUSB303_TGL_MASK,
						toggle_dutycycle_time);
		if (rc < 0) {
			dev_err(cdev,
				"failed to write toggle dutycycle time\n");
			return rc;
		}

	}
	chip->dttime = toggle_dutycycle_time;
	dev_dbg(cdev, "%s: toggle dutycycle time: (%d)\n", __func__,
			chip->dttime);
	return rc;
}
static int fusb303_set_autosink_threshold(struct fusb303_chip *chip,
		u8 autosink_threshold)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (autosink_threshold > FUSB303_AUTO_SNK_TH_3P3V) {
		dev_err(cdev, "auto sink threshold(%d) is unavailable\n",
				autosink_threshold);
		return -EINVAL;
	}
	if (autosink_threshold == chip->autosnk_thres) {
		dev_dbg(cdev, "value is not updated auto sink threshold:(%d)\n",
				autosink_threshold);
		return rc;
	} else if (autosink_threshold != chip->autosnk_thres) {
		rc = fusb303_write_masked_byte(chip->client,
						FUSB303_REG_CONTROL1,
						FUSB303_AUTO_SNK_TH_MASK,
						autosink_threshold);
		if (rc < 0) {
			dev_err(cdev, "failed to write auto sink threshold\n");
			return rc;
		}

	}
	chip->autosnk_thres = autosink_threshold;
	dev_dbg(cdev, "%s: auto sink threshold: (%d)\n", __func__,
			chip->autosnk_thres);
	return rc;
}
static int fusb303_set_tccdebounce_time(struct fusb303_chip *chip,
		u8 tccdebounce_time)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	if (tccdebounce_time > FUSB303_TCCDEB_180MS) {
		dev_err(cdev, "CC debounce time(%d) is unavailable\n",
				tccdebounce_time);
		return -EINVAL;
	}
	if (tccdebounce_time == chip->ccdebtime) {
		dev_dbg(cdev, "value is not updated CC debounce time:(%d)\n",
				tccdebounce_time);
		return rc;
	} else if (tccdebounce_time != chip->ccdebtime) {
		rc = fusb303_write_masked_byte(chip->client,
						FUSB303_REG_CONTROL1,
						FUSB303_TCCDEB_MASK,
						tccdebounce_time);
		if (rc < 0) {
			dev_err(cdev, "failed to write CC debounce time\n");
			return rc;
		}

	}
	chip->ccdebtime = tccdebounce_time;
	dev_dbg(cdev, "%s: CC debounce time: (%d)\n", __func__,
			chip->ccdebtime);
	return rc;
}
static int fusb303_dangling_cbl_en(struct fusb303_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = fusb303_write_masked_byte(chip->client,
			FUSB303_REG_CONTROL,
			FUSB303_DCBL_EN_MASK,
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
static int fusb303_remedy_en(struct fusb303_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = fusb303_write_masked_byte(chip->client,
			FUSB303_REG_CONTROL1,
			FUSB303_REMDY_EN_MASK,
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
static int fusb303_auto_snk_en(struct fusb303_chip *chip,
		bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;

	rc = fusb303_write_masked_byte(chip->client,
			FUSB303_REG_CONTROL1,
			FUSB303_AUTO_SNK_EN_MASK,
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

bool platform_get_device_irq_state(struct fusb303_chip *chip)
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

static int fusb303_enable(struct fusb303_chip *chip, bool enable)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	u8  count = 5;
	u8 data[5] = {0xAB, 0x00, 0x00, 0x00, 0x08};

	dev_info(cdev, "%s: state=%d\n", __func__, enable);
	if (chip->ccdebtime != FUSB303_TCCDEB_150MS) {
		data[0] = data[0] & (~FUSB303_TCCDEB_MASK);
		data[0] = data[0] | chip->ccdebtime;
		dev_err(cdev, "%s: Control1 reg=0x%02x\n", __func__, data[0]);
	}
	if (chip->autosnk_thres != FUSB303_AUTO_SNK_TH_3P1V) {
		data[0] = data[0] & (~FUSB303_AUTO_SNK_TH_MASK);
		data[0] = data[0] | chip->autosnk_thres;
		dev_err(cdev, "%s: Control1 reg=0x%02x\n", __func__, data[0]);
	}

	if (enable == true) {
		while (count) {
			rc = i2c_smbus_write_i2c_block_data(chip->client,
					FUSB303_REG_CONTROL1,
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
		rc = fusb303_write_masked_byte(chip->client,
				FUSB303_REG_CONTROL1,
				FUSB303_ENABLE_MASK,
				FUSB303_DISABLE);
		if (rc < 0) {
			dev_err(cdev, "%s: failed to disable fusb303\n",
					__func__);
			return rc;
		}
	}
	return rc;
}
static int fusb303_init_reg(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	/* change current */
	rc = fusb303_init_force_dfp_power(chip);
	if (rc < 0)
		dev_err(cdev, "%s: failed to force dfp power\n",
				__func__);
	/* change tdrp time */
	rc = fusb303_set_drp_cycle_time(chip, chip->pdata->tdrptime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set drp cycle time\n",
				__func__);
	/* change toggle time */
	rc = fusb303_set_toggle_time(chip, chip->pdata->dttime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set toggle dutycycle time\n",
				__func__);
	/* change auto sink threshold */
	rc = fusb303_set_autosink_threshold(chip, chip->pdata->autosnk_thres);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set auto sink threshold\n",
				__func__);
	/* change CC debounce time */
	rc = fusb303_set_tccdebounce_time(chip, chip->pdata->ccdebtime);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set CC debounce time\n",
				__func__);
	/* change mode */
	rc = fusb303_set_mode(chip, chip->pdata->init_mode);
	if (rc < 0)
		dev_err(cdev, "%s: failed to set mode\n", __func__);
	/* enable detection */
	rc = fusb303_enable(chip, true);
	if (rc < 0)
		dev_err(cdev, "%s: failed to enable detection\n",
				__func__);
	return rc;
}

static int fusb303_reset_device(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc = 0;
	rc = i2c_smbus_write_byte_data(chip->client,
					FUSB303_REG_RESET,
					FUSB303_SW_RESET);
	if (rc < 0) {
		dev_err(cdev, "reset fails\n");
		return rc;
	}

	msleep(100);
	pr_err(
		"%s, mode[0x%02x], host_cur[0x%02x], tdrptime[0x%02x], dttime[0x%02x]\n",
			__func__, chip->mode, chip->dfp_power, chip->tdrptime, chip->dttime);
	pr_err(
		"%s, autosnk_thres[0x%02x], ccdebtime[0x%02x]\n",
			__func__, chip->autosnk_thres, chip->ccdebtime);
	rc = fusb303_update_status(chip);
	if (rc < 0)
		dev_err(cdev, "fail to read status\n");
	rc = fusb303_init_reg(chip);
	if (rc < 0)
		dev_err(cdev, "fail to init reg\n");
	rc = fusb303_dangling_cbl_en(chip, false);
	if (rc < 0)
		dev_err(cdev,
				"%s: fail to disable dangling cbl func\n", __func__);
	rc = fusb303_remedy_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable remedy func\n", __func__);
	rc = fusb303_auto_snk_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable auto snk func\n", __func__);
	/* clear global interrupt mask */
	rc = fusb303_write_masked_byte(chip->client,
				FUSB303_REG_CONTROL,
				FUSB303_INT_MASK,
				FUSB303_INT_ENABLE);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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

		rc = i2c_smbus_read_byte_data(chip->client, start_reg[(i)]);
		if (rc < 0) {
			pr_err("cannot read 0x%02x\n", start_reg[(i)]);
			rc = 0;
		}
		pr_err("%s, reg[%d]=0x%02x\n", __func__, i, rc);
		ret += snprintf(buf + ret, 1024,
						"from 0x%02x read 0x%02x\n",
							start_reg[(i)],
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	switch (chip->type) {
	case FUSB303_TYPE_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SINK(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_SRC:
		ret = snprintf(buf, PAGE_SIZE, "SOURCE(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_DBG_ACC_SRC:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACCSRC(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_DBG_ACC_SNK:
		ret = snprintf(buf, PAGE_SIZE, "DEBUGACCSNK(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "AUDIOACC(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_PWR_AUD_ACC:
		ret = snprintf(buf, PAGE_SIZE, "POWEREDAUDIOACC(%d)\n",
				chip->type);
		break;
	case FUSB303_TYPE_ACTV_CABLE:
		ret = snprintf(buf, PAGE_SIZE, "ACTIVECABLE(%d)\n", chip->type);
		break;
	case FUSB303_TYPE_SRC_ACC:
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mlock);
	switch (chip->state) {
	case FUSB_STATE_DISABLED:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_DISABLED(%d)\n", chip->state);
		break;
	case FUSB_STATE_ERROR_RECOVERY:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_ERROR_RECOVERY(%d)\n", chip->state);
		break;
	case FUSB_STATE_FORCE_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_FORCE_SRC(%d)\n", chip->state);
		break;
	case FUSB_STATE_FORCE_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_FORCE_SNK(%d)\n", chip->state);
		break;
	case FUSB_STATE_UNATTACHED_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_UNATTACHED_SNK(%d)\n", chip->state);
		break;
	case FUSB_STATE_UNATTACHED_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_UNATTACHED_SRC(%d)\n", chip->state);
		break;
	case FUSB_STATE_ATTACHED_SNK:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_ATTACHED_SNK(%d)\n", chip->state);
		break;
	case FUSB_STATE_ATTACHED_SRC:
		ret = snprintf(buf, PAGE_SIZE,
				"FUSB_STATE_ATTACHED_SRC(%d)\n", chip->state);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		if (((state == FUSB_STATE_UNATTACHED_SNK) &&
			(chip->mode & (FUSB303_SRC | FUSB303_SRC_ACC))) ||
			((state == FUSB_STATE_UNATTACHED_SRC) &&
			(chip->mode & (FUSB303_SNK | FUSB303_SNK_ACC)))) {
			mutex_unlock(&chip->mlock);
			return -EINVAL;
		}
		rc = fusb303_set_chip_state(chip, (u8)state);
		if (rc < 0) {
			mutex_unlock(&chip->mlock);
			return rc;
		}
		mutex_unlock(&chip->mlock);
		return size;
	}
	return -EINVAL;
}
DEVICE_ATTR(fchip_state, S_IRUGO|S_IWUSR, fchip_state_show, fchip_state_store);
static ssize_t fmode_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&chip->mlock);
	switch (chip->mode) {   // make sure all MODEs are here
	case FUSB303_DRP_ACC:
		ret = snprintf(buf, PAGE_SIZE, "DRP+ACC(%d)\n", chip->mode);
		break;
	case FUSB303_DRP:
		ret = snprintf(buf, PAGE_SIZE, "DRP(%d)\n", chip->mode);
		break;
	case FUSB303_SNK_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SNK+ACC(%d)\n", chip->mode);
		break;
	case FUSB303_SNK:
		ret = snprintf(buf, PAGE_SIZE, "SNK(%d)\n", chip->mode);
		break;
	case FUSB303_SRC_ACC:
		ret = snprintf(buf, PAGE_SIZE, "SRC+ACC(%d)\n", chip->mode);
		break;
	case FUSB303_SRC:
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
		rc = fusb303_set_mode(chip, (u8)mode);
		if (rc < 0) {
			mutex_unlock(&chip->mlock);
			return rc;
		}
		rc = fusb303_set_chip_state(chip,
					0x0);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int ftdrptime = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &ftdrptime) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_set_drp_cycle_time(chip, (u8)ftdrptime);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int dttime = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &dttime) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_set_toggle_time(chip, (u8)dttime);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_set_dfp_power(chip, (u8)buf);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	u32 reset = 0;
	int rc = 0;
	if (sscanf(buff, "%u", &reset) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_reset_device(chip);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_set_autosink_threshold(chip, (u8)buf);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int buf = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &buf) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_set_tccdebounce_time(chip, (u8)buf);
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB303_REG_CONTROL);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & FUSB303_DCBL_EN_MASK) >> 3;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fdcable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_write_masked_byte(chip->client,
				FUSB303_REG_CONTROL,
				FUSB303_DCBL_EN_MASK,
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB303_REG_CONTROL1);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & FUSB303_REMDY_EN_MASK) >> 7;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fremedy_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_write_masked_byte(chip->client,
				FUSB303_REG_CONTROL1,
				FUSB303_REMDY_EN_MASK,
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
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int rc = 0;
	int state = 0;

	mutex_lock(&chip->mlock);
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB303_REG_CONTROL1);
	if (rc < 0) {
		dev_err(dev, "%s: failed to read status\n", __func__);
		return rc;
	}
	state = (rc & FUSB303_AUTO_SNK_EN_MASK) >> 4;
	rc = snprintf(buf, PAGE_SIZE, "%u\n", state);
	mutex_unlock(&chip->mlock);
	return rc;
}
static ssize_t fauto_snk_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	int state = 0;
	int rc = 0;
	if (sscanf(buff, "%d", &state) == 1) {
		mutex_lock(&chip->mlock);
		rc = fusb303_write_masked_byte(chip->client,
				FUSB303_REG_CONTROL1,
				FUSB303_AUTO_SNK_EN_MASK,
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
static int fusb303_create_devices(struct device *cdev)
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
static void fusb303_destory_device(struct device *cdev)
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
static int fusb303_power_set_icurrent_max(struct fusb303_chip *chip,
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
static void fusb303_bclvl_changed(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc, limit;
	u8 status, type;
	rc = i2c_smbus_read_word_data(chip->client,
				FUSB303_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		rc = fusb303_reset_device(chip);
		if (rc < 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	status = rc & 0xFF;
	rc = i2c_smbus_read_byte_data(chip->client,
			FUSB303_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}
	type = (status & FUSB303_ATTACH) ?
			(rc & FUSB303_TYPE_MASK) : FUSB303_TYPE_INVALID;
	dev_info(cdev, "sts[0x%02x], type[0x%02x]\n", status, type);

	// make sure all TYPEs are correct here
	if (type == FUSB303_TYPE_SNK ||
			type == FUSB303_TYPE_PWR_AUD_ACC ||
			type == FUSB303_TYPE_DBG_ACC_SNK) {
		chip->bc_lvl = status & 0x06;
		chip->ufp_power = status & 0x06 >> 1;
		limit = (chip->bc_lvl == FUSB303_SNK_3000MA ? 3000 :
				(chip->bc_lvl == FUSB303_SNK_1500MA ? 1500 : 0));
		fusb303_power_set_icurrent_max(chip, limit);
	}
	dev_info(cdev, "%s: bc_lvl=%d\n", __func__, chip->bc_lvl);

	if (!chip->bc_lvl && type == FUSB303_TYPE_SNK) {
		fusb303_detach(chip);
	}
}
static void fusb303_autosnk_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement autosnk changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_vbus_changed(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status, type;

	/* get status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB303_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}
	status = rc & 0xFF;

	rc = i2c_smbus_read_byte_data(chip->client,
			FUSB303_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}
	type = (status & FUSB303_ATTACH) ?
		(rc & FUSB303_TYPE_MASK) : FUSB303_TYPE_INVALID;
	dev_err(cdev,
			"%s status=0x%02x, type=0x%02x\n", __func__, status, type);

	if (type == FUSB303_TYPE_SRC || type == FUSB303_TYPE_SRC_ACC)
		return;

	if (fusb303_is_vbus_on(chip)) {
		dev_err(cdev, "%s: vbus voltage was high\n", __func__);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
	} else {
		fusb303_detach(chip);
	}
	return;
}
static void fusb303_fault_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement fault changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_orient_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement orient changed work */
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status;
	rc = i2c_smbus_read_word_data(chip->client,
				FUSB303_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read\n", __func__);
		if (fusb303_reset_device(chip) != 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	status = rc & FUSB303_ORIENT_MASK;

	if (rc & FUSB303_ATTACH) {
		if (status == 0x10)
			chip->orientation = FUSB303_ORIENT_CC1;
		else if (status == 0x20)
			chip->orientation = FUSB303_ORIENT_CC2;
		else
			chip->orientation = FUSB303_ORIENT_NONE;
	} else {
		chip->orientation = FUSB303_ORIENT_NONE;
	}
	typec_cc_orientation = chip->orientation;
	dev_info(cdev, "orientation[0x%02x]\n", chip->orientation);
	return;
}
static void fusb303_remedy_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement remedy changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_frc_succ_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement frc succ changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_frc_fail_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement frc fail changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_rem_fail_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement rem fail changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_rem_vbon_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement rem vbon changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_rem_vboff_changed(struct fusb303_chip *chip)
{
	/* TODO */
	/* implement rem vboff changed work */
	pr_info("%s: enter \n", __func__);
	return;
}
static void fusb303_attached_src(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	if (chip->mode & (FUSB303_SNK | FUSB303_SNK_ACC)) {
		dev_err(cdev, "%s: donot support source mode\n", __func__);
	}

	fusb_update_state(chip, FUSB_STATE_ATTACHED_SRC);
	tcpci_report_usb_port_attached(chip->tcpc);
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
	chip->type = FUSB303_TYPE_SRC;
	dev_info(cdev, "%s: chip->type=0x%02x\n", __func__, chip->type);
}
static void fusb303_attached_snk(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	if (chip->mode & (FUSB303_SRC | FUSB303_SRC_ACC)) {
		dev_err(cdev, "%s: donot support sink mode\n", __func__);
	}

	fusb_update_state(chip, FUSB_STATE_ATTACHED_SNK);
	tcpci_report_usb_port_attached(chip->tcpc);
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
	chip->type = FUSB303_TYPE_SNK;
	dev_info(cdev, "%s: chip->type=0x%02x\n", __func__, chip->type);
}
static void fusb303_attached_dbg_acc(struct fusb303_chip *chip)
{
	/*
	 * TODO
	 * need to implement
	 */
	pr_info("%s: enter \n", __func__);
	fusb_update_state(chip, FUSB_STATE_DEBUG_ACCESSORY);
}
static void fusb303_attached_aud_acc(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	if (!(chip->mode & FUSB303_AUD_ACC)) {
		dev_err(cdev, "%s: not support accessory mode\n", __func__);
		if (fusb303_reset_device(chip) < 0)
			dev_err(cdev, "%s: failed to reset\n", __func__);
		return;
	}
	/*
	 * TODO
	 * need to implement
	 */
	pr_info("%s: enter \n", __func__);
	fusb_update_state(chip, FUSB_STATE_AUDIO_ACCESSORY);
	fusb303_detach(chip);
}
static void fusb303_detach(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;

	dev_err(cdev, "%s: chip->type=0x%02x, chip->state=0x%02x\n",
			__func__, chip->type, chip->state);

	chip->type = FUSB303_TYPE_INVALID;
	chip->bc_lvl = FUSB303_SNK_0MA;
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
	tcpci_report_usb_port_detached(chip->tcpc);
#ifdef HAVE_DR
	dual_role_instance_changed(chip->dual_role);
#endif /* HAVE_DR */
}
static bool fusb303_is_vbus_on(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;

	rc = i2c_smbus_read_byte_data(chip->client, FUSB303_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return false;
	}

	return !!(rc & FUSB303_VBUSOK);
}
static void fusb303_attach(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 status, status1, type;

	/* get status and type */
	rc = i2c_smbus_read_word_data(chip->client,
			FUSB303_REG_STATUS);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read status\n", __func__);
		return;
	}

	status = rc & 0xFF;
	status1 = (rc >> 8) & 0xFF;

	rc = i2c_smbus_read_byte_data(chip->client,
			FUSB303_REG_TYPE);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return;
	}

	type = (status & FUSB303_ATTACH) ?
		(rc & FUSB303_TYPE_MASK) : FUSB303_TYPE_INVALID;
	dev_info(cdev, "%s: status=0x%02x, status1=0x%02x, type=0x%02x\n",
			__func__, status, status1, type);

	switch (type) {
	case FUSB303_TYPE_SRC:
	case FUSB303_TYPE_SRC_ACC:
		fusb303_attached_src(chip);
		if (fusb303_is_vbus_on(chip)) {
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
	case FUSB303_TYPE_SNK:
		fusb303_attached_snk(chip);
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
				chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
				tcpci_notify_typec_state(chip->tcpc);
				chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
		break;
	case FUSB303_TYPE_ACTV_CABLE:
		chip->type = type;
		break;
	case FUSB303_TYPE_DBG_ACC_SRC:
		break;
	case FUSB303_TYPE_DBG_ACC_SNK:
		fusb303_attached_dbg_acc(chip);
		chip->type = type;
		if (chip->tcpc->typec_attach_new != TYPEC_ATTACHED_SNK) {
			chip->tcpc->typec_attach_new = TYPEC_ATTACHED_SNK;
			tcpci_notify_typec_state(chip->tcpc);
			chip->tcpc->typec_attach_old = TYPEC_ATTACHED_SNK;
		}
		break;
	case FUSB303_TYPE_AUD_ACC:
	case FUSB303_TYPE_PWR_AUD_ACC:
		fusb303_attached_aud_acc(chip);
		chip->type = type;
		break;
	case FUSB303_TYPE_INVALID:
		fusb303_detach(chip);
		dev_err(cdev, "%s: Invaild type[0x%02x]\n", __func__, type);
		break;
	default:
		rc = fusb303_set_chip_state(chip,
				0x0);
		if (rc < 0)
			dev_err(cdev, "%s: failed to set error recovery\n",
					__func__);
		fusb303_detach(chip);
		dev_err(cdev, "%s: Unknwon type[0x%02x]\n", __func__, type);
		break;
	}
}
static void fusb303_work_handler(struct work_struct *work)
{
	struct fusb303_chip *chip =
			container_of(work, struct fusb303_chip, dwork);
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
	rc = i2c_smbus_read_byte_data(chip->client, FUSB303_REG_INTERRUPT);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}
	int_sts = rc & FUSB303_INT_STS_MASK;
	dev_info(cdev, "%s: int_sts[0x%02x]\n", __func__, int_sts);
	if (int_sts & FUSB303_I_DETACH) {
		fusb303_detach(chip);
	} else {
		if (int_sts & FUSB303_I_ATTACH) {
			fusb303_attach(chip);
		}
		if (int_sts & FUSB303_I_BC_LVL) {
			fusb303_bclvl_changed(chip);
		}
		if (int_sts & FUSB303_I_AUTOSNK) {
			fusb303_autosnk_changed(chip);
		}
		if (int_sts & FUSB303_I_VBUS_CHG) {
			fusb303_vbus_changed(chip);
		}
		if (int_sts & FUSB303_I_FAULT) {
			fusb303_fault_changed(chip);
		}
		if (int_sts & FUSB303_I_ORIENT) {
			fusb303_orient_changed(chip);
		}
	}

	rc = i2c_smbus_read_byte_data(chip->client, FUSB303_REG_INTERRUPT1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt1\n", __func__);
		goto work_unlock;
	}

	int_sts1 = rc & FUSB303_INT1_STS_MASK;
	dev_info(cdev, "%s: interrupt_1=0x%02x\n", __func__, int_sts1);

	if (int_sts1 & FUSB303_I_REMEDY) {
		fusb303_remedy_changed(chip);
	}
	if (int_sts1 & FUSB303_I_FRC_SUCC) {
		fusb303_frc_succ_changed(chip);
	}
	if (int_sts1 & FUSB303_I_FRC_FAIL) {
		fusb303_frc_fail_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_FAIL) {
		fusb303_rem_fail_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_VBON) {
		fusb303_rem_vbon_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_VBOFF) {
		fusb303_rem_vboff_changed(chip);
	}

	i2c_smbus_write_byte_data(chip->client,
				FUSB303_REG_INTERRUPT, int_sts);
	i2c_smbus_write_byte_data(chip->client,
				FUSB303_REG_INTERRUPT1, int_sts1);
work_unlock:
	mutex_unlock(&chip->mlock);
	} while (platform_get_device_irq_state(chip));
}
static irqreturn_t fusb303_interrupt(int irq, void *data)
{
	struct fusb303_chip *chip = (struct fusb303_chip *)data;
	if (!chip) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}
	/*
	 * wake_lock_timeout, prevents multiple suspend entries
	 * before charger gets chance to trigger usb core for device
	 */

	if (!queue_work(chip->cc_wq, &chip->dwork))
		dev_err(&chip->client->dev, "%s: can't alloc work\n", __func__);
	return IRQ_HANDLED;
}
static int fusb303_init_gpio(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	int ret = 0;

	chip->pdata->int_gpio = of_get_named_gpio(cdev->of_node,
			"fusb303,int-gpio", 0);
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

	ret = request_irq(chip->irq_gpio, fusb303_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"fusb303_int_gpio", chip);
	if (ret)
		dev_err(cdev, "unable to request int_gpio %d\n",
				chip->pdata->int_gpio);

	dev_info(cdev, "%s: name=%s, gpio=%d, IRQ number=%d\n",
			__func__, chip->tcpc_desc->name,
			chip->pdata->int_gpio, chip->irq_gpio);
	return ret;
}
static void fusb303_free_gpio(struct fusb303_chip *chip)
{
	if (gpio_is_valid(chip->pdata->int_gpio))
		gpio_free(chip->pdata->int_gpio);
}
static int fusb303_parse_dt(struct fusb303_chip *chip)
{
	struct device *cdev = &chip->client->dev;
	struct device_node *dev_node = cdev->of_node;
	struct fusb303_data *data = chip->pdata;
	int rc = 0;

	rc = of_property_read_u32(dev_node,
				"fusb303,init-mode", &data->init_mode);
	if (rc) {
		dev_err(cdev, "init mode is not available and set default\n");
		data->init_mode = FUSB303_DRP;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
				"fusb303,host-current", &data->dfp_power);
	if (rc || (data->dfp_power > FUSB303_HOST_3000MA)) {
		dev_err(cdev,
			"host current is not available and set default\n");
		data->dfp_power = FUSB303_HOST_1500MA;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"fusb303,drp-toggle-time", &data->tdrptime);
	if (rc || (data->tdrptime > FUSB303_TDRP_90MS)) {
		dev_err(cdev, "drp time is not available and set default\n");
		data->tdrptime = FUSB303_TDRP_70MS;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
			"fusb303,drp-duty-time", &data->dttime);
	if (rc || (data->dttime > FUSB303_TGL_30PCT)) {
		dev_err(cdev,
			"drp dutycycle time not available and set default\n");
		data->dttime = FUSB303_TGL_60PCT;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
				"fusb303,autosink-threshold",
				&data->autosnk_thres);
	if (rc || (data->autosnk_thres > FUSB303_AUTO_SNK_TH_3P3V)) {
		dev_err(cdev,
			"auto sink threshold not available and set default\n");
		data->autosnk_thres = FUSB303_AUTO_SNK_TH_3P1V;
		rc = 0;
	}
	rc = of_property_read_u32(dev_node,
				"fusb303,cc-debounce-time", &data->ccdebtime);
	if (rc || (data->ccdebtime > FUSB303_TCCDEB_180MS)) {
		dev_err(cdev,
			"CC debounce time not available and set default\n");
		data->ccdebtime = FUSB303_TCCDEB_150MS;
		rc = 0;
	}
	dev_err(cdev,
		"%s init_mode:%d dfp_power:%d tdrp_time:%d toggle_dutycycle_time:%d\n",
			__func__, data->init_mode, data->dfp_power,
			data->tdrptime, data->dttime);
	dev_err(cdev, "%s autosnk_thres:%d ccdebtime:%d\n",
			__func__, data->autosnk_thres, data->ccdebtime);
	return rc;
}

#ifdef HAVE_DR
static enum dual_role_property fusb_drp_properties[] = {
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
	struct fusb303_chip *chip;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	int ret = 0;
	if (!client)
		return -EINVAL;
	chip = i2c_get_clientdata(client);
	mutex_lock(&chip->mlock);
	if (chip->state == FUSB_STATE_ATTACHED_SRC) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SRC;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			ret = -EINVAL;
	} else if (chip->state == FUSB_STATE_ATTACHED_SNK) {
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
	struct fusb303_chip *chip;
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
			mode = FUSB303_SRC;
			fallback_mode = FUSB303_SNK;
			target_state = FUSB_STATE_ATTACHED_SRC;
			fallback_state = FUSB_STATE_ATTACHED_SNK;
		} else if (*val == DUAL_ROLE_PROP_MODE_UFP) {
			dev_dbg(cdev, "%s: Setting SNK mode\n", __func__);
			mode = FUSB303_SNK;
			fallback_mode = FUSB303_SRC;
			target_state = FUSB_STATE_ATTACHED_SNK;
			fallback_state = FUSB_STATE_ATTACHED_SRC;
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
	if ((mode == FUSB303_SRC) &&
			(chip->state == FUSB_STATE_ATTACHED_SNK)) {
		fusb303_set_manual_reg(chip, FUSB303_FORCE_SRC);
	} else if (mode == FUSB303_SNK &&
			chip->state == FUSB_STATE_ATTACHED_SRC) {
		fusb303_set_manual_reg(chip, FUSB303_FORCE_SNK);
	}
	mutex_unlock(&chip->mlock);
	return 0;
}
#endif /* HAVE_DR */
int fusb303_alert_status_clear(struct tcpc_device *tcpc,
		uint32_t mask)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

int fusb303_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

int fusb303_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

int fusb303_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

int fusb303_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{

		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_set_cc(struct tcpc_device *tcpc, int pull)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_set_polarity(struct tcpc_device *tcpc, int polarity)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_set_vconn(struct tcpc_device *tcpc, int enable)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

static int fusb303_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
		pr_info("%s enter \n", __func__);
	return 0;
}

int fusb303_get_mode(struct tcpc_device *tcpc, int *typec_mode)
{
	struct device *cdev = &g_client->dev;
	int rc;
	u8 type;


	rc = i2c_smbus_read_byte_data(g_client,
			FUSB303_REG_TYPE);
	if (rc < 0) {
		*typec_mode = 0;
		dev_err(cdev, "%s: failed to read type\n", __func__);
		return 0;
	}

	type = rc & FUSB303_TYPE_MASK;

	switch (type) {
	case FUSB303_TYPE_SRC:
	case FUSB303_TYPE_SRC_ACC:
	case FUSB303_TYPE_DBG_ACC_SRC:
		*typec_mode = 2;
		break;
	case FUSB303_TYPE_SNK:
	case FUSB303_TYPE_DBG_ACC_SNK:
		*typec_mode = 1;
		break;
	default:
		*typec_mode = 0;
		dev_err(cdev, "%s: Invaild type[0x%02x]\n", __func__, type);
		break;
	}
	pr_err("dhx---fusb303 get typec mode type:%d, reg:%x\n", *typec_mode, type);
	return 0;

}
int fusb303_set_role(struct tcpc_device *tcpc, int state)
{
	int rc = 0;
	u8 reg;

	pr_err("dhx--set role %d\n", state);

	if (state == REVERSE_CHG_SOURCE)
		reg = FUSB303_FORCE_SRC;
	else
		return 0;

	rc = i2c_smbus_write_byte_data(g_client, FUSB303_REG_MANUAL, reg);

	if (rc < 0) {
		pr_err("%s: failed to write manual(%d)\n", __func__, rc);
		return rc;
	}
	return rc;
}
static struct tcpc_ops fusb303_tcpc_ops = {
	.init = fusb303_tcpc_init,
	.alert_status_clear = fusb303_alert_status_clear,
	.fault_status_clear = fusb303_fault_status_clear,
	.get_alert_mask = fusb303_get_alert_mask,
	.get_alert_status = fusb303_get_alert_status,
	.get_power_status = fusb303_get_power_status,
	.get_fault_status = fusb303_get_fault_status,
	.get_cc = fusb303_get_cc,
	.set_cc = fusb303_set_cc,
	.set_polarity = fusb303_set_polarity,
	.set_low_rp_duty = fusb303_set_low_rp_duty,
	.set_vconn = fusb303_set_vconn,
	.set_role = fusb303_set_role,
	.get_mode = fusb303_get_mode,
	.deinit = fusb303_tcpc_deinit,
};
static void fusb303_first_check_typec_work(struct work_struct *work)
{
	struct fusb303_chip *chip = container_of(work,
			struct fusb303_chip, first_check_typec_work.work);
	struct device *cdev = &chip->client->dev;
	int rc;
	u8 int_sts = 0;
	u8 int_sts1 = 0;

	do {
	mutex_lock(&chip->mlock);
	/* get interrupt */
	rc = i2c_smbus_read_byte_data(chip->client, FUSB303_REG_INTERRUPT);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt\n", __func__);
		goto work_unlock;
	}

	first_check = false;
	int_sts = rc & FUSB303_INT_STS_MASK;
	dev_info(cdev, "%s: interrupt=0x%02x\n", __func__, int_sts);
	if (int_sts & FUSB303_I_DETACH) {
		fusb303_detach(chip);
	} else {
		if (int_sts & FUSB303_I_ATTACH) {
			fusb303_attach(chip);
		}
		if (int_sts & FUSB303_I_BC_LVL) {
			fusb303_bclvl_changed(chip);
		}
		if (int_sts & FUSB303_I_AUTOSNK) {
			fusb303_autosnk_changed(chip);
		}
		if (int_sts & FUSB303_I_VBUS_CHG) {
			fusb303_vbus_changed(chip);
		}
		if (int_sts & FUSB303_I_FAULT) {
			fusb303_fault_changed(chip);
		}
		if (int_sts & FUSB303_I_ORIENT) {
			fusb303_orient_changed(chip);
		}
	}

	/* get interrupt1 */
	rc = i2c_smbus_read_byte_data(chip->client, FUSB303_REG_INTERRUPT1);
	if (rc < 0) {
		dev_err(cdev, "%s: failed to read interrupt1\n", __func__);
		goto work_unlock;
	}
	int_sts1 = rc & FUSB303_INT1_STS_MASK;
	dev_info(cdev, "%s: interrupt_1=0x%02x\n", __func__, int_sts1);
	if (int_sts1 & FUSB303_I_REMEDY) {
		fusb303_remedy_changed(chip);
	}
	if (int_sts1 & FUSB303_I_FRC_SUCC) {
		fusb303_frc_succ_changed(chip);
	}
	if (int_sts1 & FUSB303_I_FRC_FAIL) {
		fusb303_frc_fail_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_FAIL) {
		fusb303_rem_fail_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_VBON) {
		fusb303_rem_vbon_changed(chip);
	}
	if (int_sts1 & FUSB303_I_REM_VBOFF) {
		fusb303_rem_vboff_changed(chip);
	}

	i2c_smbus_write_byte_data(chip->client,
				FUSB303_REG_INTERRUPT, int_sts);
	i2c_smbus_write_byte_data(chip->client,
				FUSB303_REG_INTERRUPT1, int_sts1);
work_unlock:
	mutex_unlock(&chip->mlock);
	} while (platform_get_device_irq_state(chip));
}
static int fusb303_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct fusb303_chip *chip;
	struct device *cdev = &client->dev;
//	struct power_supply *usb_psy;
	struct fusb303_data *data ;
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
		dev_err(cdev, "smbus data not supported!\n");
		return -EIO;
	}
	chip = devm_kzalloc(cdev, sizeof(struct fusb303_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(cdev, "can't alloc fusb303_chip\n");
		return -ENOMEM;
	}
	chip->client = client;
	i2c_set_clientdata(client, chip);
	g_client = chip->client;
	ret = fusb303_read_device_id(chip);
	if (ret != FUSB303_REV) {
		dev_err(cdev, "fusb303 not support\n");
		goto err1;
	}
	data = devm_kzalloc(cdev,
				sizeof(struct fusb303_data), GFP_KERNEL);
	if (!data) {
		dev_err(cdev, "can't alloc fusb303_data\n");
		ret = -ENOMEM;
		goto err1;
	}
	chip->pdata = data;
	ret = fusb303_parse_dt(chip);
	if (ret) {
		dev_err(cdev, "can't parse dt\n");
		goto err2;
	}

	chip->type = FUSB303_TYPE_INVALID;
	chip->state = 0x0;
	chip->bc_lvl = FUSB303_SNK_0MA;
	chip->ufp_power = 0;
	//	chip->usb_psy = usb_psy;

	chip->cc_wq = alloc_ordered_workqueue("fusb303-wq", WQ_HIGHPRI);
	if (!chip->cc_wq) {
		dev_err(cdev, "unable to create workqueue fusb303-wq\n");
		goto err2;
	}
	INIT_WORK(&chip->dwork, fusb303_work_handler);
	sema_init(&chip->suspend_lock, 1);
	INIT_DELAYED_WORK(&chip->first_check_typec_work,
			fusb303_first_check_typec_work);
	mutex_init(&chip->mlock);
	ret = fusb303_create_devices(cdev);
	if (ret < 0) {
		dev_err(cdev, "could not create devices\n");
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
	chip->tcpc_desc = desc;
	pr_err("fusb303 role = %d\n", desc->role_def);
	chip->tcpc = tcpc_device_register(cdev,
			desc, &fusb303_tcpc_ops, chip);
	chip->tcpc->typec_attach_old = TYPEC_UNATTACHED;
	chip->tcpc->typec_attach_new = TYPEC_UNATTACHED;
	ret = fusb303_init_gpio(chip);
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
				"unable to allocate dual role descriptor\n");
			goto err4;
		}
		desc->name = "otg_default";
		desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		desc->get_property = dual_role_get_local_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = fusb_drp_properties;
		desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(cdev, desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->desc = desc;
	}
#endif /* HAVE_DR */
	schedule_delayed_work(&chip->first_check_typec_work,
			msecs_to_jiffies(3000));
	ret = fusb303_reset_device(chip);
	if (ret) {
		dev_err(cdev, "failed to initialize\n");
		goto err5;
	}
	enable_irq_wake(chip->irq_gpio);
	return 0;
err5:
#ifdef HAVE_DR
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF))
		devm_kfree(cdev, chip->desc);
#endif /* HAVE_DR */
err4:
	fusb303_destory_device(cdev);

err3:
	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	fusb303_free_gpio(chip);
err2:
	devm_kfree(cdev, chip->pdata);
err1:
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);
	return ret;
}
static int fusb303_remove(struct i2c_client *client)
{
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;
	if (!chip) {
		pr_err("%s : chip is null\n", __func__);
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

	fusb303_destory_device(cdev);

	destroy_workqueue(chip->cc_wq);
	mutex_destroy(&chip->mlock);
	fusb303_free_gpio(chip);
	devm_kfree(cdev, chip->pdata);
	i2c_set_clientdata(client, NULL);
	devm_kfree(cdev, chip);
	return 0;
}
static void fusb303_shutdown(struct i2c_client *client)
{
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;
	int rc = 0;

	dev_err(cdev, "%s: enter\n", __func__);
	rc = i2c_smbus_write_byte_data(chip->client,
			FUSB303_REG_RESET,
			FUSB303_SW_RESET);
	if (rc < 0) {
		dev_err(cdev, "%s: reset fails\n", __func__);
	}
	if (fusb303_set_mode(chip, FUSB303_SNK) != 0)
		dev_err(cdev, "%s: failed to set sink mode\n", __func__);
	rc = fusb303_dangling_cbl_en(chip, false);
	if (rc < 0)
		dev_err(cdev,
				"%s: fail to disable dangling cbl func\n", __func__);
	rc = fusb303_remedy_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable remedy func\n", __func__);
	rc = fusb303_auto_snk_en(chip, false);
	if (rc < 0)
		dev_err(cdev, "%s: fail to disable auto snk func\n", __func__);
}
#ifdef CONFIG_PM
static int fusb303_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	dev_err(cdev, "%s: enter\n", __func__);

	if (!chip) {
		dev_err(cdev, "%s: No device is available!\n", __func__);
		return -EINVAL;
	}

	down(&chip->suspend_lock);
	return 0;
}
static int fusb303_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fusb303_chip *chip = i2c_get_clientdata(client);
	struct device *cdev = &client->dev;

	dev_err(cdev, "%s: enter\n", __func__);

	if (!chip) {
		dev_err(cdev, "%s: No device is available!\n", __func__);
		return -EINVAL;
	}

	up(&chip->suspend_lock);
	return 0;
}
static const struct dev_pm_ops fusb303_dev_pm_ops = {
	.suspend = fusb303_suspend,
	.resume  = fusb303_resume,
};
#endif
static const struct i2c_device_id fusb303_id_table[] = {
	{"fusb303", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fusb303_id_table);
#ifdef CONFIG_OF
static struct of_device_id fusb303_match_table[] = {
	{.compatible = "on,fusb303",},
	{},
};
#else
#define fusb303_match_table NULL
#endif
static struct i2c_driver fusb303_i2c_driver = {
	.driver = {
		.name = "fusb303",
		.owner = THIS_MODULE,
		.of_match_table = fusb303_match_table,
#ifdef CONFIG_PM
		.pm = &fusb303_dev_pm_ops,
#endif
	},
	.probe = fusb303_probe,
	.remove = fusb303_remove,
	.shutdown = fusb303_shutdown,
	.id_table = fusb303_id_table,
};
static __init int fusb303_i2c_init(void)
{
	return i2c_add_driver(&fusb303_i2c_driver);
}
static __exit void fusb303_i2c_exit(void)
{
	i2c_del_driver(&fusb303_i2c_driver);
}
module_init(fusb303_i2c_init);
module_exit(fusb303_i2c_exit);
MODULE_AUTHOR("");
MODULE_DESCRIPTION("I2C bus driver for fusb303 USB Type-C");
MODULE_LICENSE("GPL v2");

/* SPDX-License-Identifier: GPL-2.0
 *
 * LN8000 MTK charger class device driver
 *
 * Copyright © 2022 Cirrus Logic Incorporated - https://cirrus.com
 */

#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>

#include "charger_class.h"
#include "ln8000.h"

enum ln8000_opmode_index {
	LN8000_OPMODE_2TO1 = 0,
	LN8000_OPMODE_BYPASS,
};

enum ln8000_adc_channel_index {
	LN8000_ADC_CH_VOUT = 1,
	LN8000_ADC_CH_VIN,
	LN8000_ADC_CH_VBAT,
	LN8000_ADC_CH_VAC,
	LN8000_ADC_CH_IIN,
	LN8000_ADC_CH_DIETEMP,
	LN8000_ADC_CH_TSBAT,
	LN8000_ADC_CH_TSBUS,
	LN8000_ADC_CH_ALL
};

struct ln8000_platform_data {
	u32 vbat_ovp;
	u32 ibat_ocp;
	u32 vbus_ovp;
	u32 ibus_ocp;
	u32 vbat_alarm;
	u32 vbus_alarm;

	bool dualsync_func;
};

/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 start*/
enum xmc_cp_div_mode {
	XMC_CP_2T1,
	XMC_CP_1T1,
};
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 end*/

typedef struct ln8000_charger {
	struct i2c_client *client;
	struct device *dev;
	const char *dev_name;
	int dev_id;
	struct ln8000_platform_data *pdata;
	struct regmap *map;
	struct mutex irq_lock;
	struct mutex notify_lock;
	struct delayed_work notify_work;
	struct delayed_work alarm_work;
	/* for RCP patch */
	struct delayed_work rcp_work;
	/* for charger_class */
	struct charger_device *chg_dev;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
	struct power_supply *psy;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
	u32 bus_ovp_alarm_th;
	u32 bat_ovp_alarm_th;
	u32 bus_ovp;
	u32 bus_ocp;
	u32 bat_ovp;
	u32 bat_ocp;
	u8 op_mode;
	bool chg_en;
	bool dualsync;

	/* for detect IC RESET */
	u8 regulation_ctrl;
	u8 adc_ctrl;
	u8 v_float_ctrl;
	u8 charge_ctrl;
} ln8000_charger_t;

/* ---------------------------------------------------------------------- */
/* I2C transfer lapping functions                                         */
/* ---------------------------------------------------------------------- */
static int ln8000_read_reg(ln8000_charger_t * chip, u8 addr, u8 * data)
{
	int i, ret;
	unsigned int val;

	for (i = 0; i < 3; ++i) {
		ret = regmap_read(chip->map, addr, &val);
		if (ret < 0) {
			dev_err(chip->dev,
				"failed-read, reg(0x%02X), ret(%d)\n",
				addr, ret);
		} else {
			*data = (u8) (val & 0xFF);
			break;
		}
	}

	if (ret < 0)
		*data = (u8) (0xff);

	return ret;
}

static int ln8000_bulk_read_reg(ln8000_charger_t * chip, u8 addr,
				void *data, int count)
{
	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_bulk_read(chip->map, addr, data, count);
		if (ret < 0) {
			dev_err(chip->dev,
				"failed-bulkread, reg(0x%02X), ret(%d)\n",
				addr, ret);
		} else {
			break;
		}
	}

	return ret;
}

static int ln8000_write_reg(ln8000_charger_t * chip, u8 addr, u8 data)
{
	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_write(chip->map, addr, data);
		if (ret < 0) {
			dev_err(chip->dev,
				"failed-write, reg(0x%02X), ret(%d)\n",
				addr, ret);
		} else {
			break;
		}
	}

	return ret;
}

static int ln8000_update_reg(ln8000_charger_t * chip, u8 addr, u8 mask,
			     u8 data)
{
	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_update_bits(chip->map, addr, mask, data);
		if (ret < 0) {
			dev_err(chip->dev,
				"fail-update, reg(0x%02X), ret(%d)\n",
				addr, ret);
		} else {
			break;
		}
	}

	return ret;
}

/* ---------------------------------------------------------------------- */
/* IC control functions                                                   */
/* ---------------------------------------------------------------------- */
static void ln8000_convert_adc_code(ln8000_charger_t * chip,
				    unsigned int ch, u8 * sts, int *result)
{
	u16 adc_raw;
	int adc_final;

	switch (ch) {
	case LN8000_ADC_CH_VOUT:
		adc_raw = ((sts[1] & 0xFF) << 2) | ((sts[0] & 0xC0) >> 6);
		adc_final = adc_raw * LN8000_ADC_VOUT_STEP_uV;
		break;
	case LN8000_ADC_CH_VIN:
		adc_raw = ((sts[1] & 0x3F) << 4) | ((sts[0] & 0xF0) >> 4);
		adc_final = adc_raw * LN8000_ADC_VIN_STEP_uV;
		break;
	case LN8000_ADC_CH_VBAT:
		adc_raw = ((sts[1] & 0x03) << 8) | (sts[0] & 0xFF);
		adc_final = adc_raw * LN8000_ADC_VBAT_STEP_uV;
		break;
	case LN8000_ADC_CH_VAC:
		adc_raw =
		    (((sts[1] & 0x0F) << 6) | ((sts[0] & 0xFC) >> 2)) +
		    LN8000_ADC_VAC_OS;
		adc_final = adc_raw * LN8000_ADC_VAC_STEP_uV;
		break;
	case LN8000_ADC_CH_IIN:
		adc_raw = ((sts[1] & 0x03) << 8) | (sts[0] & 0xFF);
		adc_final = adc_raw * LN8000_ADC_IIN_STEP_uA;
		break;
	case LN8000_ADC_CH_DIETEMP:
		adc_raw = ((sts[1] & 0x0F) << 6) | ((sts[0] & 0xFC) >> 2);
		/* Die Temp = (935-ADC code)/2.3 */
		adc_final = (935 - adc_raw) * 100 / 23;
		adc_final = clamp(adc_final, (int) LN8000_ADC_DIETEMP_MIN,
				  (int) LN8000_ADC_DIETEMP_MAX);
		break;
	case LN8000_ADC_CH_TSBAT:
		adc_raw = ((sts[1] & 0x3F) << 4) | ((sts[0] & 0xF0) >> 4);
		adc_final = adc_raw * LN8000_ADC_NTCV_STEP;	//(NTC) uV
		break;
	case LN8000_ADC_CH_TSBUS:
		adc_raw = ((sts[1] & 0xFF) << 2) | ((sts[0] & 0xC0) >> 6);
		adc_final = adc_raw * LN8000_ADC_NTCV_STEP;	//(NTC) uV
		break;
	default:
		adc_raw = -EINVAL;
		adc_final = -EINVAL;
		break;
	}

	*result = adc_final;
}

static bool ln8000_is_valid_vbus(ln8000_charger_t * chip)
{
	u8 fault1_sts;
	int ret;

	ret = ln8000_read_reg(chip, LN8000_REG_FAULT1_STS, &fault1_sts);
	if (ret)
		return FALSE;

	if (fault1_sts & BIT(LN8000_V_SHORT_STS)
	    || fault1_sts & BIT(LN8000_VAC_UNPLUG_STS))
		return FALSE;
	else
		return TRUE;
}

static int ln8000_get_adc(ln8000_charger_t * chip, unsigned int ch,
			  int *result)
{
	u8 sts[2];
	int ret;

	/* pause adc update */
	ret =
	    ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			      BIT(LN8000_PAUSE_ADC_UPDATES),
			      0x1 << LN8000_PAUSE_ADC_UPDATES);
	if (ret < 0) {
		dev_err(chip->dev,
			"fail to update bit PAUSE_ADC_UPDATE:1 (ret=%d)\n",
			ret);
		return ret;
	}

	switch (ch) {
	case LN8000_ADC_CH_VOUT:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC04_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_VIN:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC03_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_VBAT:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC06_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_VAC:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC02_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_IIN:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC01_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_DIETEMP:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC07_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_TSBAT:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC08_STS, sts,
					 2);
		break;
	case LN8000_ADC_CH_TSBUS:
		ret =
		    ln8000_bulk_read_reg(chip, LN8000_REG_ADC09_STS, sts,
					 2);
		break;
	default:
		dev_err(chip->dev, "invalid channel(%d)\n", ch);
		ret = -EINVAL;
		break;
	}

	/* resume adc update */
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_PAUSE_ADC_UPDATES),
			  0x0 << LN8000_PAUSE_ADC_UPDATES);

	if (!ret) {
		ln8000_convert_adc_code(chip, ch, sts, result);
	}

	return ret;
}

static const u8 ln8000_print_regs[] = {
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x1b, 0x1c, 0x1d, 0x1e,
	    0x1f, 0x20, 0x21,
	0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x31, 0x41, 0x49,
	    0x4a, 0x4b,
};

static void ln8000_print_regmap(ln8000_charger_t * chip)
{
	int i, array_num = ARRAY_SIZE(ln8000_print_regs);
	u32 vac, vin, iin, vout, vbat, tdie;
	u8 data;

	for (i = 0; i < array_num; ++i) {
		ln8000_read_reg(chip, ln8000_print_regs[i], &data);
		dev_err(chip->dev, "DEV[%d] REG[0x%02X] = 0x%02X\n",
			chip->dev_id, ln8000_print_regs[i], data);
	}

	ln8000_get_adc(chip, LN8000_ADC_CH_VAC, &vac);
	ln8000_get_adc(chip, LN8000_ADC_CH_VIN, &vin);
	ln8000_get_adc(chip, LN8000_ADC_CH_IIN, &iin);
	ln8000_get_adc(chip, LN8000_ADC_CH_VOUT, &vout);
	ln8000_get_adc(chip, LN8000_ADC_CH_VBAT, &vbat);
	ln8000_get_adc(chip, LN8000_ADC_CH_DIETEMP, &tdie);
	dev_err(chip->dev, "VAC=%dmV:VIN=%dmV:IIN=%dmV\n", vac / 1000,
		vin / 1000, iin / 1000);
	dev_err(chip->dev, "VOUT=%dmV:VBAT=%dmV:TDIE=%d", vout / 1000,
		vbat / 1000, tdie);
}

static void ln8000_soft_reset(ln8000_charger_t * chip)
{
	ln8000_write_reg(chip, LN8000_REG_LION_CTRL, 0xC6);

	disable_irq(chip->client->irq);

	ln8000_update_reg(chip, LN8000_REG_BC_OP_2,
			  BIT(LN8000_SOFT_RESET_REQ),
			  0x1 << LN8000_SOFT_RESET_REQ);
	msleep(60);
	dev_err(chip->dev, "Do soft-reset\n");

	enable_irq(chip->client->irq);
}

static int ln8000_set_vbat_ovp(ln8000_charger_t * chip, u32 ovp_th)
{
	/* ln8000 VBAT_OVP = VFLOAT x 1.02 */
	u32 vbat_ovp =
	    clamp(ovp_th, (u32) LN8000_BAT_OVP_MIN_uV,
		  (u32) LN8000_BAT_OVP_MAX_uV);
	u32 vbat_float =
	    ((vbat_ovp / 1020) * 1000) - LN8000_VFLOAT_BG_OFFSET_uV;
	u8 vfloat =
	    (vbat_float - LN8000_VFLOAT_MIN_uV) / LN8000_VFLOAT_LSB_uV;

	chip->v_float_ctrl = vfloat;
	return ln8000_write_reg(chip, LN8000_REG_V_FLOAT_CTRL, vfloat);
}

static int ln8000_set_vac_ovp(ln8000_charger_t * chip, u32 ovp_th)
{
	u8 vac_ov_cfg;

	if (ovp_th <= 6500000)
		vac_ov_cfg = LN8000_VAC_OVP_6P5V;
	else if (ovp_th <= 11000000)
		vac_ov_cfg = LN8000_VAC_OVP_11V;
	else if (ovp_th <= 12000000)
		vac_ov_cfg = LN8000_VAC_OVP_12V;
	else
		vac_ov_cfg = LN8000_VAC_OVP_13V;

	dev_err(chip->dev, "%s:ovp_th=%d vac_ov_cfg=0x%x\n", __func__,
		ovp_th, vac_ov_cfg);
	return ln8000_update_reg(chip, LN8000_REG_GLITCH_CTRL,
				 LN8000_VAC_OV_CFG_MASK,
				 vac_ov_cfg << LN8000_VAC_OV_CFG_SHIFT);
}

static int ln8000_set_ibus_ocp(ln8000_charger_t * chip, u32 ocp_th)
{
	/* ln8000 IBUS_OCP = IIN_CFG_PROG + 700mA */
	u32 ibus_ocp =
	    clamp(ocp_th, (u32) LN8000_BUS_OCP_MIN_uA,
		  (u32) LN8000_BUS_OCP_MAX_uA);
	u8 iin_cfg =
	    (ibus_ocp - LN8000_OCP_OFFSET_uA) / LN8000_IIN_CFG_LSB_uA;

	return ln8000_update_reg(chip, LN8000_REG_IIN_CTRL,
				 LN8000_IIN_CFG_PROG_MASK, iin_cfg);
}


static int ln8000_enable_rcp(ln8000_charger_t * chip, bool enable)
{
	int ret;

	ret =
	    ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
			      BIT(LN8000_REV_IIN_DET),
			      enable << LN8000_REV_IIN_DET);

	return ret;
}

static int ln8000_enable_switching(ln8000_charger_t * chip, bool enable)
{
	int ret;

	if (enable) {
		/* RCP W/A patch */
		ln8000_enable_rcp(chip, FALSE);
		schedule_delayed_work(&chip->rcp_work,
				      msecs_to_jiffies(200));
		/* STANDBY_EN = 0 */
		ret =
		    ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
				      BIT(LN8000_STANDBY_EN),
				      0x0 << LN8000_STANDBY_EN);
		chip->chg_en = 1;
		if (chip->bat_ovp_alarm_th || chip->bus_ovp_alarm_th)
			schedule_delayed_work(&chip->alarm_work,
					      msecs_to_jiffies(1000));
		mdelay(5);
		ln8000_print_regmap(chip);
	} else {
		ln8000_enable_rcp(chip, TRUE);
		/* STANDBY_EN = 1 */
		ret =
		    ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
				      BIT(LN8000_STANDBY_EN),
				      0x1 << LN8000_STANDBY_EN);
		chip->chg_en = 0;
	}

	return ret;
}

static bool ln8000_is_switching_enabled(ln8000_charger_t * chip)
{
	u8 sys_sts, is_switching;

	ln8000_read_reg(chip, LN8000_REG_SYS_STS, &sys_sts);
	if (sys_sts == 0x0) {
		/* waiting for internal state transition time */
		mdelay(5);
		ln8000_read_reg(chip, LN8000_REG_SYS_STS, &sys_sts);
	}
	is_switching = (sys_sts & 0x3) ? FALSE : TRUE;

	if (chip->chg_en == TRUE && is_switching == FALSE) {
		dev_err(chip->dev,
			"forced cut-off switching (sys_sts=0x%x)\n",
			sys_sts);
		ln8000_enable_switching(chip, FALSE);
		ln8000_print_regmap(chip);
	}

	return is_switching;
}

static int ln8000_set_dual_function(ln8000_charger_t * chip)
{
	int ret = 0;

	if (chip->dualsync) {
		ln8000_write_reg(chip, LN8000_REG_LION_CTRL, 0xAA);
		/* if uses dual-sync mode */
		if (chip->dev_id == 0) {
			/* for Main(Host) */
			ret = ln8000_update_reg(chip, LN8000_REG_BC_OP_1,
						LN8000_DUAL_FUNCTION_MASK,
						BIT
						(LN8000_DUAL_FUNCTION_EN));
		} else {
			/* for Second(Device) */
			ret = ln8000_update_reg(chip, LN8000_REG_BC_OP_1,
						LN8000_DUAL_FUNCTION_MASK,
						BIT
						(LN8000_DUAL_FUNCTION_EN) |
						BIT(LN8000_DUAL_CFG) |
						BIT
						(LN8000_DUAL_LOCKOUT_EN));
		}
		ln8000_write_reg(chip, LN8000_REG_LION_CTRL, 0x00);
	}

	return ret;
}

static int ln8000_set_opmode(ln8000_charger_t * chip,
			     enum ln8000_opmode_index mode)
{
	int ret;

	switch (mode) {
	case LN8000_OPMODE_2TO1:
		ret = ln8000_update_reg(chip, LN8000_REG_FAULT_CTRL,
					BIT(LN8000_DISABLE_VIN_UV_TRACK),
					0x1 <<
					LN8000_DISABLE_VIN_UV_TRACK);
		ret =
		    ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
				      BIT(LN8000_EN_1TO1), 0x0);
		break;
	case LN8000_OPMODE_BYPASS:
		ret = ln8000_update_reg(chip, LN8000_REG_FAULT_CTRL,
					BIT(LN8000_DISABLE_VIN_UV_TRACK),
					0x0 <<
					LN8000_DISABLE_VIN_UV_TRACK);
		ret =
		    ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
				      BIT(LN8000_EN_1TO1), 0x1);
		break;
	default:
		dev_err(chip->dev, "invalid mode=%d\n", mode);
		return -EINVAL;
	}
	chip->op_mode = mode;

	return ret;
}

static int ln8000_set_watchdog(ln8000_charger_t * chip, bool enable,
			       u8 timeout)
{
	int ret;

	ret =
	    ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			      BIT(LN8000_WATCHDOG_EN),
			      enable << LN8000_WATCHDOG_EN);
	if (ret)
		return ret;

	ret =
	    ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			      LN8000_WATCHDOG_CFG_MASK,
			      timeout << LN8000_WATCHDOG_CFG_SHIFT);
	return ret;
}

static int ln8000_set_adc_mode(ln8000_charger_t * chip, u8 mode,
			       u8 hib_delay)
{
	int ret;

	/* enable ADC channel (disable VOUT/TSBAT/TSBUS) */
	ret = ln8000_write_reg(chip, LN8000_REG_ADC_CFG, 0x3F);
	if (ret)
		return ret;

	ret =
	    ln8000_update_reg(chip, LN8000_REG_ADC_CTRL,
			      LN8000_ADC_MODE_MASK,
			      mode << LN8000_ADC_MODE_SHIFT);
	if (ret)
		return ret;

	ret =
	    ln8000_update_reg(chip, LN8000_REG_ADC_CTRL,
			      LN8000_ADC_HIBERNATE_DELAY_MASK,
			      hib_delay <<
			      LN8000_ADC_HIBERNATE_DELAY_SHIFT);
	return ret;
}

static int ln8000_read_int_value(ln8000_charger_t * chip, u8 * int_value)
{
	u8 int1, int1_msk;
	int ret;

	/* pause INT updates */
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_PAUSE_INT_UPDATES),
			  0x1 << LN8000_PAUSE_INT_UPDATES);
	mdelay(1);
	ret = ln8000_read_reg(chip, LN8000_REG_INT1, &int1);
	/* resume INT updates */
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_PAUSE_INT_UPDATES),
			  0x0 << LN8000_PAUSE_INT_UPDATES);
	ln8000_read_reg(chip, LN8000_REG_INT1_MSK, &int1_msk);

	if (int_value != NULL)
		*int_value = int1 & ~int1_msk;

	return 0;
}

static int ln8000_init_hw(ln8000_charger_t * chip, bool req_SW_RESET)
{
	u8 int_mask;

	if (req_SW_RESET)
		ln8000_soft_reset(chip);

	ln8000_set_vbat_ovp(chip, chip->bat_ovp);
	ln8000_set_vac_ovp(chip, chip->bus_ovp);
	ln8000_set_ibus_ocp(chip, chip->bus_ocp);
	ln8000_set_watchdog(chip, FALSE, LN8000_WATCHDOG_5SEC);
	ln8000_set_adc_mode(chip, LN8000_ADC_AUTO_SHD_MODE,
			    LN8000_ADC_HIBERNATE_4S);
	ln8000_set_opmode(chip, LN8000_OPMODE_2TO1);
	ln8000_set_dual_function(chip);
	ln8000_enable_switching(chip, FALSE);

	/* fixed settings */
	/* disable regulation functions */
	ln8000_write_reg(chip, LN8000_REG_REGULATION_CTRL, 0x34);
	ln8000_update_reg(chip, LN8000_REG_SYS_CTRL,
			  BIT(LN8000_SOFT_START_EN), 0x0);
	/* disable auto recovery functions */
	ln8000_update_reg(chip, LN8000_REG_RECOVERY_CTRL, 0xf << 4, 0x0);
	ln8000_write_reg(chip, LN8000_REG_FAULT_CTRL, 0x80);
	/* mark sw initialized (used CHARGE_CTRL bit:7) */
	ln8000_update_reg(chip, LN8000_REG_CHARGE_CTRL, 0x1 << 7,
			  0x1 << 7);
	ln8000_write_reg(chip, LN8000_REG_THRESHOLD_CTRL, 0x0E);
	/* set auto-clear latched status */
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_AUTO_CLEAR_LATCHED_STS),
			  0x1 << LN8000_AUTO_CLEAR_LATCHED_STS);
	/* interrupt mask */
	int_mask =
	    (u8) ~ (BIT(LN8000_FAULT_INT) | BIT(LN8000_MODE_INT) |
		    BIT(LN8000_REV_CURR_INT));
	ln8000_write_reg(chip, LN8000_REG_INT1_MSK, int_mask);
	ln8000_read_int_value(chip, NULL);

	/* clear latched status */
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_CLEAR_LATCHED_STS),
			  0x1 << LN8000_CLEAR_LATCHED_STS);
	ln8000_update_reg(chip, LN8000_REG_TIMER_CTRL,
			  BIT(LN8000_CLEAR_LATCHED_STS),
			  0x0 << LN8000_CLEAR_LATCHED_STS);

	/* record fixed setting values for detect IC RESET */
	ln8000_read_reg(chip, LN8000_REG_REGULATION_CTRL,
			&chip->regulation_ctrl);
	ln8000_read_reg(chip, LN8000_REG_ADC_CTRL, &chip->adc_ctrl);
	//ln8000_read_reg(chip, LN8000_REG_V_FLOAT_CTRL, &chip->v_float_ctrl);
	ln8000_read_reg(chip, LN8000_REG_CHARGE_CTRL, &chip->charge_ctrl);

	if (chip->dev_id == 1) {
		/* disable ovpgate for secondary device */
		ln8000_write_reg(chip, LN8000_REG_LION_CTRL, 0xAA);
		ln8000_update_reg(chip, LN8000_REG_PRODUCT_ID,
				  BIT(LN8000_OVPFETDR_HIGH_IMP),
				  0x1 << LN8000_OVPFETDR_HIGH_IMP);
		ln8000_write_reg(chip, LN8000_REG_LION_CTRL, 0x00);
	}

	ln8000_print_regmap(chip);

	return 0;
}

static int ln8000_check_regmap_data(ln8000_charger_t * chip)
{
	u8 regulation_ctrl, adc_ctrl, v_float_ctrl, charge_ctrl;

	ln8000_read_reg(chip, LN8000_REG_REGULATION_CTRL,
			&regulation_ctrl);
	ln8000_read_reg(chip, LN8000_REG_ADC_CTRL, &adc_ctrl);
	ln8000_read_reg(chip, LN8000_REG_V_FLOAT_CTRL, &v_float_ctrl);
	ln8000_read_reg(chip, LN8000_REG_CHARGE_CTRL, &charge_ctrl);

	if ((chip->regulation_ctrl != regulation_ctrl) ||
	    (chip->adc_ctrl != adc_ctrl) ||
	    (chip->charge_ctrl != charge_ctrl) ||
	    (chip->v_float_ctrl != v_float_ctrl)) {
		/* Decide register map was reset. caused by EOS */
		dev_err(chip->dev,
			"decided register map RESET, re-initialize device\n");
		dev_err(chip->dev, "regulation_ctrl = 0x%x:0x%x\n",
			chip->regulation_ctrl, regulation_ctrl);
		dev_err(chip->dev, "adc_ctrl        = 0x%x:0x%x\n",
			chip->adc_ctrl, adc_ctrl);
		dev_err(chip->dev, "charge_ctrl     = 0x%x:0x%x\n",
			chip->charge_ctrl, charge_ctrl);
		dev_err(chip->dev, "vbat_float      = 0x%x:0x%x\n",
			chip->v_float_ctrl, v_float_ctrl);
		ln8000_init_hw(chip, FALSE);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */
/* Support MTK Charger Class device                                       */
/* ---------------------------------------------------------------------- */
static int ln8000_enable_charge(struct charger_device *chg_dev,
				bool enable)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret;
	dev_err(chip->dev, "%s enable=%d\n", __func__, enable);
	ln8000_check_regmap_data(chip);
	ret = ln8000_enable_switching(chip, enable);

	return ret;
}

static int ln8000_check_charge_enabled(struct charger_device *chg_dev,
				       bool * enabled)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);

	*enabled = ln8000_is_switching_enabled(chip);

	return 0;
}

/*
static int ln8000_set_chg_mode(struct charger_device *chg_dev, int mode)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	u8 op_mode = LN8000_OPMODE_2TO1;

	dev_err(chip->dev, "%s mode=%d\n", __func__, mode);

	if (mode == CHARGE_MODE_BYPASS)
		op_mode = LN8000_OPMODE_BYPASS;

	ln8000_set_opmode(chip, op_mode);

	return 0;
}
*/
static int ln8000_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret;

	ret = ln8000_set_ibus_ocp(chip, uA);
	if (ret)
		return ret;
	chip->bus_ocp = uA;

	return 0;
}

static int ln8000_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret;

	ret = ln8000_set_vac_ovp(chip, uV);
	if (ret)
		return ret;
	chip->bus_ovp = uV;

	return 0;
}

static int ln8000_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
	/* ln8000 not support IBATOCP function */

	return 0;
}

static int ln8000_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret;

	ret = ln8000_set_vbat_ovp(chip, uV);
	if (ret)
		return ret;
	chip->bat_ovp = uV;

	return 0;
}

static int ln8000_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);

	chip->bat_ovp_alarm_th = uV;

	return 0;
}

static int ln8000_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
	/* we will notify alarm status used adc value. */
	/* therefore we don't need reset function. */
	return 0;
}

static int ln8000_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);

	chip->bus_ovp_alarm_th = uV;

	return 0;
}

static int ln8000_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
	/* we will notify alarm status used adc value. */
	/* therefore we don't need reset function. */
	return 0;
}

static int ln8000_is_vbuslowerr(struct charger_device *chg_dev, bool * err)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);

	*err = (ln8000_is_valid_vbus(chip)) ? FALSE : TRUE;

	return 0;
}

static int ln8000_init_chip(struct charger_device *chg_dev)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret;
	dev_err(chip->dev, "call %s\n", __func__);
	//ret = ln8000_init_hw(chip, TRUE);
	ret = ln8000_init_hw(chip, FALSE);

	return ret;
}

static int ln8000_get_adc_value(struct charger_device *chg_dev,
				enum adc_channel chan, int *min, int *max)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	u32 adc_val = 0;
	int ret = 0;
	int tmp_ret = 0;
	struct power_supply *bat_psy = NULL;
	union power_supply_propval val_new;

	dev_err(chip->dev, "ln8000_get_adc_value_chan:%d\n", chan);

	switch (chan) {
	case ADC_CHANNEL_VBUS:
		ret = ln8000_get_adc(chip, LN8000_ADC_CH_VAC, &adc_val);
		break;
	case ADC_CHANNEL_VBAT:
		ln8000_check_regmap_data(chip);
		ret = ln8000_get_adc(chip, LN8000_ADC_CH_VBAT, &adc_val);
		break;
	case ADC_CHANNEL_IBUS:
		ret = ln8000_get_adc(chip, LN8000_ADC_CH_IIN, &adc_val);
		break;
	case ADC_CHANNEL_IBAT:
/*		ret = ln8000_get_adc(chip, LN8000_ADC_CH_IIN, &adc_ibus);
		if (ret)
				adc_ibus = 0;
		adc_val = adc_ibus * ((chip->op_mode == LN8000_OPMODE_2TO1) ? 2 : 1);
		dev_err(chip->dev, "IBAT_ADC=%d(IBUS=%d x 2)\n",adc_val,adc_ibus);
*/
		bat_psy = power_supply_get_by_name("battery");
		if (IS_ERR_OR_NULL(bat_psy)) {
			dev_err(chip->dev,
				"%s: failed to get battery psy\n",
				__func__);
			return 0;
		} else {
			tmp_ret = power_supply_get_property(bat_psy,
							    POWER_SUPPLY_PROP_CURRENT_NOW,
							    &val_new);
			adc_val = val_new.intval;
		}

		dev_err(chip->dev, "IBAT_ADC=%d\n", adc_val);
		break;

	case ADC_CHANNEL_TEMP_JC:
		ret =
		    ln8000_get_adc(chip, LN8000_ADC_CH_DIETEMP, &adc_val);
		adc_val = adc_val / 10;
		break;
	case ADC_CHANNEL_TBAT:
		ret = ln8000_get_adc(chip, LN8000_ADC_CH_TSBAT, &adc_val);
		break;
	case ADC_CHANNEL_VOUT:
		ret = ln8000_get_adc(chip, LN8000_ADC_CH_VOUT, &adc_val);
		break;
		/* Not support */
	case ADC_CHANNEL_VSYS:
	case ADC_CHANNEL_TS:
	case ADC_CHANNEL_USBID:
	default:
		adc_val = 0;
		ret = 0;
		break;
	}

	*min = adc_val;
	*max = *min;
	dev_err(chip->dev, "ln8000_get_adc_value_*max:%d\n", *max);
	return ret;
}

/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
static int ln8000_get_adc_accuracy(struct charger_device *chg_dev,
				   enum adc_channel chan, int *min,
				   int *max)
{
	switch (chan) {
	case ADC_CHANNEL_VOUT:
		*min = *max = 80000;	/* +- 2% at 4.0V */
		break;
	case ADC_CHANNEL_VBUS:
		*min = *max = 160000;	/* +- 2% at 8.0V */
		break;
	case ADC_CHANNEL_VBAT:
		*min = *max = 20000;	/* +- 0.5% at 4.0V */
		break;
	case ADC_CHANNEL_IBUS:
		*min = *max = 200000;	/* +- 5% at 4.0A */
		break;
	case ADC_CHANNEL_IBAT:
		*min = *max = 400000;	/* +- x2 from IBUS accuracy */
		break;
	case ADC_CHANNEL_TEMP_JC:
		*min = *max = 10;	/* +- 10 dgress for TDIE */
		break;
	case ADC_CHANNEL_TBAT:
	case ADC_CHANNEL_VSYS:
	case ADC_CHANNEL_TS:
	case ADC_CHANNEL_USBID:
	default:
		*min = *max = 0;
		break;
	}

	return 0;
}

/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 start*/
static int ln8000_get_work_mode(ln8000_charger_t *chip, int *mode)
{
	int ret = 0;
	u8 val = 0;
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 start*/
	int result;
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 end*/
	ret = ln8000_read_reg(chip, LN8000_REG_SYS_STS, &val);
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 start*/
	result = (int)(val & LN8000_REG_SYS_MASK);
	dev_err(chip->dev, "%s N17 get work mode reg value  =%d\n", __func__, result);

	if (result == LN8000_FORWARD_2_1_CHARGER_MODE)
		*mode = XMC_CP_2T1;
	else if (result == LN8000_FORWARD_1_1_CHARGER_MODE)
		*mode = XMC_CP_1T1;
	else
		dev_err(chip->dev, "%s N17 get work mode reg value is error\n", __func__);
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 end*/
	*mode = (int)(val & LN8000_REG_SYS_MASK);
	return ret;
}

/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 start*/
#if 0
static int ln8000_set_work_mode(ln8000_charger_t *chip, int mode)
{
   u8 val = (u8)mode;
   int ret = 0;
   ret = ln8000_write_reg(chip, LN8000_REG_SYS_CTRL, val);
   return ret;
}
#endif
/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 end*/

static int ln8000_ops_set_chg_work_mode(struct charger_device *chg_dev, int mode)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int ret = 0;
	dev_err(chip->dev, "N17:set work mode is %d\n", mode);
   switch (mode) {
   case XMC_CP_1T1:
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 start*/
/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 start*/
	    ret = ln8000_set_opmode(chip, LN8000_OPMODE_BYPASS);
/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 end*/
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 end*/
	   break;
   case XMC_CP_2T1:
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 start*/
/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 start*/
	   ret = ln8000_set_opmode(chip, LN8000_OPMODE_2TO1);
/*N17 code for HQ-308575 by xm tianye9 at 2023/07/24 end*/
/*N17 code for HQ-307853 by xm tianye9 at 2023/07/17 end*/
	   break;
   default:
	   dev_err(chip->dev, "%s N17 set work mode is unsupported\n", __func__);
   }

   return ret;
}

/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 start*/
static int ln8000_get_cp_device_id(void)
{
	return LN8000_CP;
}
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 end*/

static int ln8000_ops_get_chg_work_mode(struct charger_device *chg_dev, int *mode)
{
	ln8000_charger_t *chip = charger_get_data(chg_dev);
	int value = 0, ret = 0;

	ret = ln8000_get_work_mode(chip, &value);
	if (ret)
		dev_err(chip->dev, "%s N17 failed to get work_mode\n");

	dev_err(chip->dev, "N17:get work mode is %d\n", value);
	switch (value) {
	case LN8000_FORWARD_1_1_CHARGER_MODE:
		*mode = XMC_CP_1T1;
		break;
	case LN8000_FORWARD_2_1_CHARGER_MODE:
		*mode = XMC_CP_2T1;
		break;
	default:
		dev_err(chip->dev, "%s N17 get work mode is unsupported\n", __func__);
	}

	return ret;
}
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 end*/

/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
static void rcp_control_work(struct work_struct *work)
{
	ln8000_charger_t *chip =
	    container_of(work, ln8000_charger_t, rcp_work.work);
	u32 vbus_uV, iin_uA, vbat_uV, v_offset;
	bool is_chg;

	is_chg = ln8000_is_switching_enabled(chip);
	ln8000_get_adc(chip, LN8000_ADC_CH_VIN, &vbus_uV);
	ln8000_get_adc(chip, LN8000_ADC_CH_IIN, &iin_uA);
	ln8000_get_adc(chip, LN8000_ADC_CH_VBAT, &vbat_uV);
	if (vbus_uV < (vbat_uV * 2)) {
		v_offset = 0;
	} else {
		v_offset = vbus_uV - (vbat_uV * 2);
	}

	if (!is_chg) {
		/* if already disabled, we don't need this work */
		return;
	}

	dev_err(chip->dev,
		"vbus:%d iin:%d, vbat:%d, v_offset=%d is_chg=%d\n",
		vbus_uV / 1000, iin_uA / 1000, vbat_uV / 1000,
		v_offset / 1000, is_chg);

	/* When the input current rises above 400mA, we can activate RCP. */
	if (iin_uA > 400000) {
		ln8000_enable_rcp(chip, 1);
		dev_err(chip->dev, "enabled rcp\n");
		return;
	}

	/* If an unplug event occurred during disabled REV_IIN_DET, */
	/* we need to turn-off switching */
	/* unplug event cond. : IBUS lower then 70mA and v_offset(100mV) */
	if (iin_uA < 70000 && v_offset < 100000) {
		ln8000_enable_switching(chip, FALSE);
		dev_err(chip->dev,
			"Occurs plug-out, turn-off switching\n");
		return;
	}

	schedule_delayed_work(&chip->rcp_work, msecs_to_jiffies(200));
}

static void alarm_notify_work(struct work_struct *work)
{
	ln8000_charger_t *chip =
	    container_of(work, ln8000_charger_t, alarm_work.work);
	u32 adc_vbat, adc_vbus;

	if (ln8000_is_switching_enabled(chip)) {
		ln8000_get_adc(chip, LN8000_ADC_CH_VBAT, &adc_vbat);
		ln8000_get_adc(chip, LN8000_ADC_CH_VIN, &adc_vbus);

		if (chip->bat_ovp_alarm_th > 0
		    && adc_vbat > chip->bat_ovp_alarm_th) {
			charger_dev_notify(chip->chg_dev,
					   CHARGER_DEV_NOTIFY_VBATOVP_ALARM);
		}
		if (chip->bus_ovp_alarm_th > 0
		    && adc_vbus > chip->bus_ovp_alarm_th) {
			charger_dev_notify(chip->chg_dev,
					   CHARGER_DEV_NOTIFY_VBUSOVP_ALARM);
		}

		schedule_delayed_work(&chip->alarm_work,
				      msecs_to_jiffies(1000));
	}
}

static void chgdev_notify_work(struct work_struct *work)
{
	ln8000_charger_t *chip =
	    container_of(work, ln8000_charger_t, notify_work.work);
	u8 sys_sts, sft_sts, f1_sts, f2_sts;

	mutex_lock(&chip->notify_lock);
	ln8000_read_reg(chip, LN8000_REG_SYS_STS, &sys_sts);
	ln8000_read_reg(chip, LN8000_REG_SAFETY_STS, &sft_sts);
	ln8000_read_reg(chip, LN8000_REG_FAULT1_STS, &f1_sts);
	ln8000_read_reg(chip, LN8000_REG_FAULT2_STS, &f2_sts);
	mutex_unlock(&chip->notify_lock);
	dev_err(chip->dev,
		"%s:sys=0x%x,safety=0x%x,fault1=0x%x,fault2=0x%x\n",
		__func__, sys_sts, sft_sts, f1_sts, f2_sts);

	if (f1_sts & BIT(LN8000_VAC_OV_STS))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_VDROVP);
	if (f1_sts & BIT(LN8000_VIN_OV_STS))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_VBUS_OVP);
	if (f1_sts & BIT(LN8000_VBAT_OV_STS))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_BAT_OVP);

	if (f1_sts & BIT(LN8000_WATCHDOG_TIMER_STS))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
	if (f2_sts & BIT(LN8000_IIN_OC_DETECTED))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_IBUSOCP);

	if (sft_sts & BIT(LN8000_REV_IIN_STS)
	    || sft_sts & BIT(LN8000_REV_IIN_LATCHED))
		charger_dev_notify(chip->chg_dev,
				   CHARGER_DEV_NOTIFY_IBUSUCP_FALL);
}

static irqreturn_t ln8000_isr(int irq, void *data)
{
	ln8000_charger_t *chip = data;
	u8 int_value;
	int ret;

	mutex_lock(&chip->irq_lock);

	ret = ln8000_read_int_value(chip, &int_value);
	if (ret) {
		dev_err(chip->dev, "fail to read INT1 register (ret=%d)\n",
			ret);
		return IRQ_NONE;
	}

	if (int_value & BIT(LN8000_FAULT_INT)) {
		dev_dbg(chip->dev, "Triggered FAULT_INT\n");
	}

	if (chip->chg_en && (int_value & BIT(LN8000_REV_CURR_INT))) {
		dev_dbg(chip->dev, "Triggered REV_CURR_INT\n");
	}

	if (int_value & BIT(LN8000_MODE_INT)) {
		dev_dbg(chip->dev, "Triggered MODE_INT\n");
	}
	mutex_unlock(&chip->irq_lock);

	schedule_delayed_work(&chip->notify_work, msecs_to_jiffies(1));

	return IRQ_HANDLED;
}

static const char *ln8000_chg_name = "primary_dvchg";

static const struct charger_ops ln8000_chg_ops = {
	.enable = ln8000_enable_charge,
	.is_enabled = ln8000_check_charge_enabled,
	.get_adc = ln8000_get_adc_value,
	.set_vbusovp = ln8000_set_vbusovp,
	.set_ibusocp = ln8000_set_ibusocp,
	.set_vbatovp = ln8000_set_vbatovp,
	.set_ibatocp = ln8000_set_ibatocp,
	.init_chip = ln8000_init_chip,
	//.set_chg_mode = ln8000_set_chg_mode,
	.set_vbatovp_alarm = ln8000_set_vbatovp_alarm,
	.reset_vbatovp_alarm = ln8000_reset_vbatovp_alarm,
	.set_vbusovp_alarm = ln8000_set_vbusovp_alarm,
	.reset_vbusovp_alarm = ln8000_reset_vbusovp_alarm,
	.is_vbuslowerr = ln8000_is_vbuslowerr,
	.get_adc_accuracy = ln8000_get_adc_accuracy,
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 start*/
	.get_cp_work_mode = ln8000_ops_get_chg_work_mode,
	.set_cp_work_mode = ln8000_ops_set_chg_work_mode,
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 end*/
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 start*/
	.get_cp_device = ln8000_get_cp_device_id,
/*N17 code for HQ-309331 by xm tianye9 at 2023/07/27 end*/
};

/* ---------------------------------------------------------------------- */
/* I2C client device driver register                                      */
/* ---------------------------------------------------------------------- */

static u32 cirrus_power_read_of_u32(ln8000_charger_t * chip,
				    const char *propname, u32 dft_val)
{
	struct device *dev = chip->dev;
	struct device_node *np = dev->of_node;
	u32 prop;
	int ret;

	ret = of_property_read_u32(np, propname, &prop);
	if (ret) {
		dev_err(chip->dev, "fail to parse:%s set to %d(default)\n",
			propname, dft_val);
		prop = dft_val;
	} else {
		dev_err(chip->dev, "parse:%s set to %d\n", propname, prop);
	}

	return prop;
}

static inline int ln8000_parse_device_tree(ln8000_charger_t * chip)
{
	struct device *dev = chip->dev;
	struct ln8000_platform_data *pdata = chip->pdata;
	struct device_node *np = dev->of_node;

	if (np == NULL)
		return -EINVAL;

	pdata->vbat_ovp =
	    cirrus_power_read_of_u32(chip, "cirrus,vbatovp-microvolt",
				     LN8000_BAT_OVP_DEF_uV);
	pdata->vbus_ovp =
	    cirrus_power_read_of_u32(chip, "cirrus,vbusovp-microvolt",
				     LN8000_VAC_OVP_DEF_uV);
	pdata->ibus_ocp =
	    cirrus_power_read_of_u32(chip, "cirrus,ibusocp-microamp",
				     LN8000_BUS_OCP_DEF_uA);
	pdata->vbat_alarm =
	    cirrus_power_read_of_u32(chip, "cirrus,vbatalarm-microvolt",
				     LN8000_BUS_OCP_DEF_uA);
	pdata->vbus_alarm =
	    cirrus_power_read_of_u32(chip, "cirrus,vbusalarm-microvolt",
				     LN8000_BUS_OCP_DEF_uA);
	pdata->dualsync_func =
	    cirrus_power_read_of_u32(chip, "cirrus,dualsync-func", 0);

	return 0;
}

static const struct regmap_config ln8000_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LN8000_REG_MAX,
	.cache_type = REGCACHE_NONE,
};

static int ln8000_register_charger_class(ln8000_charger_t * chip)
{
	struct charger_properties chg_props;

	if (chip->dev_id == 0)
		chg_props.alias_name = "ln8000-main";
	else
		chg_props.alias_name = "ln8000-second";

	chip->chg_dev =
	    charger_device_register(ln8000_chg_name, chip->dev, chip,
				    &ln8000_chg_ops, &chg_props);

	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		dev_err(chip->dev, "charger device register fail\n");
		return -EINVAL;
	}

	return 0;
}

/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
static int ln8000_get_charger_property(struct power_supply *psy,
				       const enum power_supply_property
				       psp,
				       union power_supply_propval *val)
{
	ln8000_charger_t *chip = power_supply_get_drvdata(psy);
	int ret = 0;
	int tmp_ret = 0;
	struct power_supply *bat_psy = NULL;
	union power_supply_propval val_new;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ln8000_is_switching_enabled(chip);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return ln8000_get_adc(chip, LN8000_ADC_CH_VBAT, &val->intval);	//VBAT
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		bat_psy = power_supply_get_by_name("battery");
		if (IS_ERR_OR_NULL(bat_psy)) {
			dev_err(chip->dev,
				"%s: failed to get battery psy\n",
				__func__);
			return 0;
		} else {
			tmp_ret = power_supply_get_property(bat_psy,
							    POWER_SUPPLY_PROP_CURRENT_NOW,
							    &val_new);
			val->intval = val_new.intval;
		}		//IBAT
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return ln8000_get_adc(chip, LN8000_ADC_CH_IIN, &val->intval);	//IBUS
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return ln8000_get_adc(chip, LN8000_ADC_CH_VIN, &val->intval);	//VBUS
	case POWER_SUPPLY_PROP_TEMP:
		return ln8000_get_adc(chip, LN8000_ADC_CH_DIETEMP,
				      &val->intval);
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = true;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int ln8000_set_charger_property(struct power_supply *psy,
				       const enum power_supply_property
				       psp,
				       const union power_supply_propval
				       *val)
{
	ln8000_charger_t *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		dev_info(chip->dev, "POWER_SUPPLY_PROP_ONLINE: %s\n",
			 val->intval ? "enable" : "disable");
		ln8000_check_regmap_data(chip);
		return ln8000_enable_switching(chip, val->intval);
	default:
		return -EINVAL;
	}

	return 0;
}

static int ln8000_property_is_writeable(struct power_supply *psy,
					const enum power_supply_property
					psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return true;
	default:
		return false;
	}
}

static enum power_supply_property ln8000_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static struct power_supply_desc ln8000_charger_desc = {
	.name = "ln8000-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = ln8000_charger_props,
	.num_properties = ARRAY_SIZE(ln8000_charger_props),
	.property_is_writeable = ln8000_property_is_writeable,
	.get_property = ln8000_get_charger_property,
	.set_property = ln8000_set_charger_property,
};

static char *ln8000_charger_supplied_to[] = {
	"ln8000-charger",
};

static int ln8000_register_power_supply(ln8000_charger_t * chip)
{
	struct power_supply_config psy_cfg = {.drv_data = chip,
		.of_node = chip->dev->of_node,
	};

	psy_cfg.supplied_to = ln8000_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(ln8000_charger_supplied_to);

	chip->psy =
	    devm_power_supply_register(chip->dev, &ln8000_charger_desc,
				       &psy_cfg);
	if (IS_ERR(chip->psy)) {
		dev_err(chip->dev, "Failed to register charger\n");
		return PTR_ERR(chip->psy);
	}
	dev_info(chip->dev, "registered power_supply for charger\n");

	return 0;
}

/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/

static void raw_i2c_sw_reset(struct i2c_client *client)
{
	u8 reg;

	i2c_smbus_write_byte_data(client, LN8000_REG_LION_CTRL, 0xC6);

	/* SOFT_RESET_REQ = 1 */
	reg = i2c_smbus_read_byte_data(client, LN8000_REG_BC_OP_2);
	reg = reg | (0x1 << 0);
	i2c_smbus_write_byte_data(client, LN8000_REG_BC_OP_2, reg);

	msleep(60);
}

static int try_to_find_i2c_address(struct i2c_client *client)
{
	u8 addr_set[] = { 0x55, 0x5f, 0x51, 0x5b };
	u8 ori_addr = client->addr;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(addr_set); ++i) {
		client->addr = addr_set[i];
		ret =
		    i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
		if (ret == LN8000_DEVICE_ID) {
			dev_err(&client->dev,
				"find to can be access address(0x%x)(ori=0x%x)\n",
				client->addr, ori_addr);
			raw_i2c_sw_reset(client);
			break;
		} else {
			dev_err(&client->dev,
				"can't access address(0x%x)(ori=0x%x)\n",
				client->addr, ori_addr);
		}
	}

	client->addr = ori_addr;
	if (ret == LN8000_DEVICE_ID) {
		ret =
		    i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
	}

	return ret;
}

static int ln8000_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	ln8000_charger_t *chip;
	int ret = 0;

	/* detect device on connected i2c bus */
	ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
	if (ret != LN8000_DEVICE_ID) {
		ret = try_to_find_i2c_address(client);
		if (ret != LN8000_DEVICE_ID) {
			dev_err(&client->dev,
				"fail to detect ln8000 on i2c_bus(addr=0x%x)\n",
				client->addr);
			return -ENODEV;
		}
	}
	dev_err(&client->dev, "Device ID=0x%x\n", ret);

	chip =
	    devm_kzalloc(&client->dev, sizeof(ln8000_charger_t),
			 GFP_KERNEL);
	if (chip == NULL) {
		dev_err(&client->dev,
			"%s: fail to alloc devm for ln8000_charger\n",
			__func__);
		return -ENOMEM;
	}
	chip->dev_name = id->name;
	chip->dev_id = (int) id->driver_data;
	chip->pdata =
	    devm_kzalloc(&client->dev, sizeof(struct ln8000_platform_data),
			 GFP_KERNEL);
	if (chip->pdata == NULL) {
		dev_err(chip->dev,
			"fail to alloc devm for ln8000_platform_data\n");
		kfree(chip);
		return -ENOMEM;
	}
	chip->dev = &client->dev;
	chip->client = client;
	ret = ln8000_parse_device_tree(chip);
	if (ret < 0)
		return ret;
	mutex_init(&chip->irq_lock);
	mutex_init(&chip->notify_lock);
	INIT_DELAYED_WORK(&chip->notify_work, chgdev_notify_work);
	INIT_DELAYED_WORK(&chip->alarm_work, alarm_notify_work);
	INIT_DELAYED_WORK(&chip->rcp_work, rcp_control_work);

	chip->map = devm_regmap_init_i2c(client, &ln8000_regmap_config);
	if (IS_ERR(chip->map)) {
		dev_err(chip->dev, "fail to allocate register map\n");
		return -EIO;
	}
	i2c_set_clientdata(client, chip);

	chip->bus_ovp = chip->pdata->vbus_ovp;
	chip->bus_ocp = chip->pdata->ibus_ocp;
	chip->bat_ovp = chip->pdata->vbat_ovp;
	chip->bat_ovp_alarm_th = chip->pdata->vbat_alarm;
	chip->bus_ovp_alarm_th = chip->pdata->vbus_alarm;
	chip->dualsync = chip->pdata->dualsync_func;
	ret = ln8000_init_hw(chip, 1);
	if (ret < 0)
		return ret;

	ret = ln8000_register_charger_class(chip);
	if (ret < 0)
		return ret;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 start*/
	ret = ln8000_register_power_supply(chip);
	if (ret < 0)
		return ret;
/*N17 code for HQ-291625 by miaozhichao at 2023/05/05 end*/
	if (client->irq) {
		ret =
		    devm_request_threaded_irq(&client->dev, client->irq,
					      NULL, ln8000_isr,
					      IRQF_TRIGGER_FALLING |
					      IRQF_ONESHOT, chip->dev_name,
					      chip);
		if (ret < 0) {
			dev_err(chip->dev, "fail to request irq(%d)\n",
				client->irq);
			return ret;
		}
		dev_err(chip->dev, "request-irq done (irq=%d)\n",
			client->irq);
	}

	dev_err(chip->dev, "%s done\n", __func__);
	return 0;
}

static int ln8000_remove(struct i2c_client *client)
{
	ln8000_charger_t *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);

	ln8000_soft_reset(chip);

	return 0;
}

static const struct of_device_id ln8000_dt_match[] = {
	{.compatible = "cirrus,ln8000-main",
	 .data = (void *) 0,
	 },
	{.compatible = "cirrus,ln8000-second",
	 .data = (void *) 1,
	 },
	{},
};

MODULE_DEVICE_TABLE(of, ln8000_dt_match);

static const struct i2c_device_id ln8000_i2c_ids[] = {
	{"ln8000-main", 0},
	{"ln8000-second", 1},
	{}
};

MODULE_DEVICE_TABLE(i2c, ln8000_i2c_ids);

static struct i2c_driver ln8000_driver = {
	.driver = {
		   .name = "ln8000",
		   .of_match_table = of_match_ptr(ln8000_dt_match),
		   },
	.probe = ln8000_probe,
	.remove = ln8000_remove,
	.id_table = ln8000_i2c_ids,
};

module_i2c_driver(ln8000_driver);

MODULE_DESCRIPTION("LN8000 MTK Charger class device driver");
MODULE_AUTHOR("sungdae choi, <sungdae.choi@cirrus.com>");
MODULE_LICENSE("GPL v2");

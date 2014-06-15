/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/leds-qpnp-wled.h>

/* base addresses */
#define QPNP_WLED_CTRL_BASE		"qpnp-wled-ctrl-base"
#define QPNP_WLED_SINK_BASE		"qpnp-wled-sink-base"
#define QPNP_WLED_IBB_BASE		"qpnp-wled-ibb-base"
#define QPNP_WLED_LAB_BASE		"qpnp-wled-lab-base"

/* ctrl registers */
#define QPNP_WLED_EN_REG(b)		(b + 0x46)
#define QPNP_WLED_FDBK_OP_REG(b)	(b + 0x48)
#define QPNP_WLED_VREF_REG(b)		(b + 0x49)
#define QPNP_WLED_BOOST_DUTY_REG(b)	(b + 0x4B)
#define QPNP_WLED_SWITCH_FREQ_REG(b)	(b + 0x4C)
#define QPNP_WLED_OVP_REG(b)		(b + 0x4D)
#define QPNP_WLED_ILIM_REG(b)		(b + 0x4E)

#define QPNP_WLED_EN_MASK		0x7F
#define QPNP_WLED_EN_SHIFT		7
#define QPNP_WLED_FDBK_OP_MASK		0xF8
#define QPNP_WLED_VREF_MASK		0xF0
#define QPNP_WLED_VREF_STEP_MV		25
#define QPNP_WLED_VREF_MIN_MV		300
#define QPNP_WLED_VREF_MAX_MV		675
#define QPNP_WLED_DFLT_VREF_MV		350
#define QPNP_WLED_ILIM_MASK		0xF8
#define QPNP_WLED_ILIM_MIN_MA		105
#define QPNP_WLED_ILIM_MAX_MA		1980
#define QPNP_WLED_ILIM_STEP_MA		280
#define QPNP_WLED_DFLT_ILIM_MA		980
#define QPNP_WLED_BOOST_DUTY_MASK	0xFC
#define QPNP_WLED_BOOST_DUTY_STEP_NS	52
#define QPNP_WLED_BOOST_DUTY_MIN_NS	26
#define QPNP_WLED_BOOST_DUTY_MAX_NS	156
#define QPNP_WLED_DEF_BOOST_DUTY_NS	104
#define QPNP_WLED_SWITCH_FREQ_MASK	0xF0
#define QPNP_WLED_SWITCH_FREQ_800_KHZ	800
#define QPNP_WLED_SWITCH_FREQ_1600_KHZ	1600
#define QPNP_WLED_OVP_MASK		0xFC
#define QPNP_WLED_OVP_17800_MV		17800
#define QPNP_WLED_OVP_19400_MV		19400
#define QPNP_WLED_OVP_29500_MV		29500
#define QPNP_WLED_OVP_31000_MV		31000

/* sink registers */
#define QPNP_WLED_CURR_SINK_REG(b)	(b + 0x46)
#define QPNP_WLED_SYNC_REG(b)		(b + 0x47)
#define QPNP_WLED_MOD_REG(b)		(b + 0x4A)
#define QPNP_WLED_HYB_THRES_REG(b)	(b + 0x4B)
#define QPNP_WLED_MOD_EN_REG(b, n)	(b + 0x50 + (n * 0x10))
#define QPNP_WLED_SYNC_DLY_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x01)
#define QPNP_WLED_FS_CURR_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x02)
#define QPNP_WLED_CABC_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x06)
#define QPNP_WLED_BRIGHT_LSB_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x07)
#define QPNP_WLED_BRIGHT_MSB_REG(b, n)	(QPNP_WLED_MOD_EN_REG(b, n) + 0x08)

#define QPNP_WLED_MOD_FREQ_1200_KHZ	1200
#define QPNP_WLED_MOD_FREQ_2400_KHZ	2400
#define QPNP_WLED_MOD_FREQ_9600_KHZ	9600
#define QPNP_WLED_MOD_FREQ_19200_KHZ	19200
#define QPNP_WLED_MOD_FREQ_MASK		0x3F
#define QPNP_WLED_MOD_FREQ_SHIFT	6
#define QPNP_WLED_PHASE_STAG_MASK	0xDF
#define QPNP_WLED_PHASE_STAG_SHIFT	5
#define QPNP_WLED_DIM_RES_MASK		0xFD
#define QPNP_WLED_DIM_RES_SHIFT		1
#define QPNP_WLED_DIM_HYB_MASK		0xFB
#define QPNP_WLED_DIM_HYB_SHIFT		2
#define QPNP_WLED_DIM_ANA_MASK		0xFE
#define QPNP_WLED_HYB_THRES_MASK	0xF8
#define QPNP_WLED_HYB_THRES_MIN		78
#define QPNP_WLED_DEF_HYB_THRES		625
#define QPNP_WLED_HYB_THRES_MAX		10000
#define QPNP_WLED_MOD_EN_MASK		0x7F
#define QPNP_WLED_MOD_EN_SHFT		7
#define QPNP_WLED_MOD_EN		1
#define QPNP_WLED_SYNC_DLY_MASK		0xF8
#define QPNP_WLED_SYNC_DLY_MIN_US	0
#define QPNP_WLED_SYNC_DLY_MAX_US	1400
#define QPNP_WLED_SYNC_DLY_STEP_US	200
#define QPNP_WLED_DEF_SYNC_DLY_US	400
#define QPNP_WLED_FS_CURR_MASK		0xF0
#define QPNP_WLED_FS_CURR_MIN_UA	0
#define QPNP_WLED_FS_CURR_MAX_UA	30000
#define QPNP_WLED_FS_CURR_STEP_UA	2500
#define QPNP_WLED_CABC_MASK		0x7F
#define QPNP_WLED_CABC_SHIFT		7
#define QPNP_WLED_CURR_SINK_SHIFT	4
#define QPNP_WLED_BRIGHT_LSB_MASK	0xFF
#define QPNP_WLED_BRIGHT_MSB_SHIFT	8
#define QPNP_WLED_BRIGHT_MSB_MASK	0x0F
#define QPNP_WLED_SYNC			0x0F
#define QPNP_WLED_SYNC_RESET		0x00

#define QPNP_WLED_SWITCH_FREQ_800_KHZ_CODE	0x0B
#define QPNP_WLED_SWITCH_FREQ_1600_KHZ_CODE	0x05

#define QPNP_WLED_DISP_SEL_REG(b)	(b + 0x44)
#define QPNP_WLED_MODULE_RDY_REG(b)	(b + 0x45)
#define QPNP_WLED_MODULE_EN_REG(b)	(b + 0x46)
#define QPNP_WLED_MODULE_RDY_MASK	0x7F
#define QPNP_WLED_MODULE_RDY_SHIFT	7
#define QPNP_WLED_MODULE_EN_MASK	0x7F
#define QPNP_WLED_MODULE_EN_SHIFT	7
#define QPNP_WLED_DISP_SEL_MASK		0x7F
#define QPNP_WLED_DISP_SEL_SHIFT	7

#define QPNP_WLED_IBB_BIAS_REG(b)	(b + 0x58)
#define QPNP_WLED_IBB_BIAS_MASK		0x7F
#define QPNP_WLED_IBB_BIAS_SHIFT	7
#define QPNP_WLED_IBB_PWRUP_DLY_MASK	0xCF
#define QPNP_WLED_IBB_PWRUP_DLY_SHIFT	4
#define QPNP_WLED_IBB_PWRUP_DLY_MIN_MS	1
#define QPNP_WLED_IBB_PWRUP_DLY_MAX_MS	8

#define QPNP_WLED_LAB_IBB_RDY_REG(b)	(b + 0x49)
#define QPNP_WLED_LAB_FAST_PC_REG(b)	(b + 0x5E)
#define QPNP_WLED_LAB_FAST_PC_MASK	0xFB
#define QPNP_WLED_LAB_START_DLY_US	8
#define QPNP_WLED_LAB_FAST_PC_SHIFT	2

#define QPNP_WLED_SEC_ACCESS_REG(b)    (b + 0xD0)
#define QPNP_WLED_SEC_UNLOCK           0xA5

#define QPNP_WLED_MAX_STRINGS		4
#define WLED_MAX_LEVEL_511		511
#define WLED_MAX_LEVEL_4095		4095
#define QPNP_WLED_RAMP_DLY_MS		20
#define QPNP_WLED_TRIGGER_NONE		"none"
#define QPNP_WLED_STR_SIZE		20
#define QPNP_WLED_MIN_MSLEEP		20

/* output feedback mode */
enum qpnp_wled_fdbk_op {
	QPNP_WLED_FDBK_AUTO,
	QPNP_WLED_FDBK_WLED1,
	QPNP_WLED_FDBK_WLED2,
	QPNP_WLED_FDBK_WLED3,
	QPNP_WLED_FDBK_WLED4,
};

/* dimming modes */
enum qpnp_wled_dim_mode {
	QPNP_WLED_DIM_ANALOG,
	QPNP_WLED_DIM_DIGITAL,
	QPNP_WLED_DIM_HYBRID,
};

/* dimming curve shapes */
enum qpnp_wled_dim_shape {
	QPNP_WLED_DIM_SHAPE_LOG,
	QPNP_WLED_DIM_SHAPE_LINEAR,
	QPNP_WLED_DIM_SHAPE_SQUARE,
};

/* wled ctrl debug registers */
static u8 qpnp_wled_ctrl_dbg_regs[] = {
	0x44, 0x46, 0x48, 0x49, 0x4b, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x5a, 0x5b, 0x5d, 0x5e
};

/* wled sink debug registers */
static u8 qpnp_wled_sink_dbg_regs[] = {
	0x46, 0x47, 0x48, 0x4a, 0x4b,
	0x50, 0x51, 0x52, 0x53,	0x56, 0x57, 0x58,
	0x60, 0x61, 0x62, 0x63,	0x66, 0x67, 0x68,
	0x70, 0x71, 0x72, 0x73,	0x76, 0x77, 0x78,
	0x80, 0x81, 0x82, 0x83,	0x86, 0x87, 0x88
};

/* wled ibb debug registers */
static u8 qpnp_wled_ibb_dbg_regs[] = {
	0x08, 0x09, 0x0A, 0x44, 0x45, 0x46, 0x50, 0x53, 0x56, 0x57, 0x58, 0x61
};

/* wled lab debug registers */
static u8 qpnp_wled_lab_dbg_regs[] = {
	0x08, 0x44, 0x45, 0x46, 0x49, 0x5e
};

/**
 *  qpnp_wled - wed data structure
 *  @ cdev - led class device
 *  @ spmi - spmi device
 *  @ work - worker for led operation
 *  @ lock - mutex lock for exclusive access
 *  @ fdbk_op - output feedback mode
 *  @ dim_mode - dimming mode
 *  @ dim_shape - dimming curve shape
 *  @ ctrl_base - base address for wled ctrl
 *  @ sink_base - base address for wled sink
 *  @ ibb_base - base address for IBB(Inverting Buck Boost)
 *  @ lab_base - base address for LAB(LCD/AMOLED Boost)
 *  @ mod_freq_khz - modulator frequency in KHZ
 *  @ hyb_thres - threshold for hybrid dimming
 *  @ sync_dly_us - sync delay in us
 *  @ vref_mv - ref voltage in mv
 *  @ switch_freq_khz - switching frequency in KHZ
 *  @ ovp_mv - over voltage protection in mv
 *  @ ilim_ma - current limiter in ma
 *  @ boost_duty_ns - boost duty cycle in ns
 *  @ fs_curr_ua - full scale current in ua
 *  @ ramp_ms - delay between ramp steps in ms
 *  @ ramp_step - ramp step size
 *  @ strings - supported list of strings
 *  @ num_strings - number of strings
 *  @ en_9b_dim_res - enable or disable 9bit dimming
 *  @ en_phase_stag - enable or disable phase staggering
 *  @ en_cabc - enable or disable cabc
 *  @ disp_type_amoled - type of display: LCD/AMOLED
 *  @ ibb_bias_active - activate display bias
 *  @ lab_fast_precharge - fast/slow precharge
 */
struct qpnp_wled {
	struct led_classdev	cdev;
	struct spmi_device *spmi;
	struct work_struct work;
	struct mutex lock;
	enum qpnp_wled_fdbk_op fdbk_op;
	enum qpnp_wled_dim_mode dim_mode;
	enum qpnp_wled_dim_shape dim_shape;
	u16 ctrl_base;
	u16 sink_base;
	u16 ibb_base;
	u16 lab_base;
	u16 mod_freq_khz;
	u16 hyb_thres;
	u16 sync_dly_us;
	u16 vref_mv;
	u16 switch_freq_khz;
	u16 ovp_mv;
	u16 ilim_ma;
	u16 boost_duty_ns;
	u16 fs_curr_ua;
	u16 ibb_pwrup_dly_ms;
	u16 ramp_ms;
	u16 ramp_step;
	u8 strings[QPNP_WLED_MAX_STRINGS];
	u8 num_strings;
	bool en_9b_dim_res;
	bool en_phase_stag;
	bool en_cabc;
	bool disp_type_amoled;
	bool ibb_bias_active;
	bool lab_fast_precharge;
};

static struct qpnp_wled *gwled;

/* helper to read a pmic register */
static int qpnp_wled_read_reg(struct qpnp_wled *wled, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_readl(wled->spmi->ctrl, wled->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&wled->spmi->dev,
			"Error reading address: %x(%d)\n", addr, rc);

	return rc;
}

/* helper to write a pmic register */
static int qpnp_wled_write_reg(struct qpnp_wled *wled, u8 *data, u16 addr)
{
	int rc;

	rc = spmi_ext_register_writel(wled->spmi->ctrl, wled->spmi->sid,
							addr, data, 1);
	if (rc < 0)
		dev_err(&wled->spmi->dev,
			"Error writing address: %x(%d)\n", addr, rc);

	dev_dbg(&wled->spmi->dev, "write: WLED_0x%x = 0x%x\n", addr, *data);

	return rc;
}

static int qpnp_wled_sec_access(struct qpnp_wled *wled, u16 base_addr)
{
	int rc;
	u8 reg = QPNP_WLED_SEC_UNLOCK;

	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SEC_ACCESS_REG(base_addr));
	if (rc)
		return rc;

	return 0;
}

/* set wled to a level of brightness */
static int qpnp_wled_set_level(struct qpnp_wled *wled, int level)
{
	int i, rc;
	u8 reg;

	/* set brightness registers */
	for (i = 0; i < wled->num_strings; i++) {
		reg = level & QPNP_WLED_BRIGHT_LSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_LSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;

		reg = level >> QPNP_WLED_BRIGHT_MSB_SHIFT;
		reg = reg & QPNP_WLED_BRIGHT_MSB_MASK;
		rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BRIGHT_MSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
	}

	/* sync */
	reg = QPNP_WLED_SYNC;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	reg = QPNP_WLED_SYNC_RESET;
	rc = qpnp_wled_write_reg(wled, &reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	return 0;
}

static int qpnp_wled_module_en(struct qpnp_wled *wled,
				u16 base_addr, bool state)
{
	int rc;
	u8 reg;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MODULE_EN_MASK;
	reg |= (state << QPNP_WLED_MODULE_EN_SHIFT);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc)
		return rc;

	return 0;
}

/* sysfs store function for ramp */
static ssize_t qpnp_wled_ramp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int i, rc;

	mutex_lock(&wled->lock);

	if (!wled->cdev.brightness) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, true);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled enable failed\n");
			goto unlock_mutex;
		}
	}

	/* ramp up */
	for (i = 0; i <= wled->cdev.max_brightness;) {
		rc = qpnp_wled_set_level(wled, i);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			goto restore_brightness;
		}

		if (wled->ramp_ms < QPNP_WLED_MIN_MSLEEP)
			usleep_range(wled->ramp_ms * USEC_PER_MSEC,
					wled->ramp_ms * USEC_PER_MSEC);
		else
			msleep(wled->ramp_ms);

		if (i == wled->cdev.max_brightness)
			break;

		i += wled->ramp_step;
		if (i > wled->cdev.max_brightness)
			i = wled->cdev.max_brightness;
	}

	/* ramp down */
	for (i = wled->cdev.max_brightness; i >= 0;) {
		rc = qpnp_wled_set_level(wled, i);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			goto restore_brightness;
		}

		if (wled->ramp_ms < QPNP_WLED_MIN_MSLEEP)
			usleep_range(wled->ramp_ms * USEC_PER_MSEC,
					wled->ramp_ms * USEC_PER_MSEC);
		else
			msleep(wled->ramp_ms);

		if (i == 0)
			break;

		i -= wled->ramp_step;
		if (i < 0)
			i = 0;
	}

	dev_info(&wled->spmi->dev, "wled ramp complete\n");

restore_brightness:
	/* restore the old brightness */
	qpnp_wled_set_level(wled, wled->cdev.brightness);
	if (!wled->cdev.brightness) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, false);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled enable failed\n");
			return rc;
		}
	}
unlock_mutex:
	mutex_unlock(&wled->lock);

	return count;
}

static int qpnp_wled_dump_regs(struct qpnp_wled *wled, u16 base_addr,
				u8 dbg_regs[], u8 size, char *label,
				int count, char *buf)
{
	int i, rc;
	u8 reg;

	for (i = 0; i < size; i++) {
		rc = qpnp_wled_read_reg(wled, &reg,
				base_addr + dbg_regs[i]);
		if (rc < 0)
			return rc;

		count += snprintf(buf + count, PAGE_SIZE - count,
				"%s: REG_0x%x = 0x%x\n", label,
				base_addr + dbg_regs[i], reg);

		if (count >= PAGE_SIZE)
			return PAGE_SIZE - 1;
	}

	return count;
}

/* sysfs show function for debug registers */
static ssize_t qpnp_wled_dump_regs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int count = 0;

	count = qpnp_wled_dump_regs(wled, wled->ctrl_base,
			qpnp_wled_ctrl_dbg_regs,
			ARRAY_SIZE(qpnp_wled_ctrl_dbg_regs),
			"wled_ctrl", count, buf);

	if (count < 0 || count == PAGE_SIZE - 1)
		return count;

	count = qpnp_wled_dump_regs(wled, wled->sink_base,
			qpnp_wled_sink_dbg_regs,
			ARRAY_SIZE(qpnp_wled_sink_dbg_regs),
			"wled_sink", count, buf);

	if (count < 0 || count == PAGE_SIZE - 1)
		return count;

	count = qpnp_wled_dump_regs(wled, wled->ibb_base,
			qpnp_wled_ibb_dbg_regs,
			ARRAY_SIZE(qpnp_wled_ibb_dbg_regs),
			"wled_ibb", count, buf);

	if (count < 0 || count == PAGE_SIZE - 1)
		return count;

	count = qpnp_wled_dump_regs(wled, wled->lab_base,
			qpnp_wled_lab_dbg_regs,
			ARRAY_SIZE(qpnp_wled_lab_dbg_regs),
			"wled_lab", count, buf);
	return count;
}

/* sysfs show function for ramp delay in each step */
static ssize_t qpnp_wled_ramp_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->ramp_ms);
}

/* sysfs store function for ramp delay in each step */
static ssize_t qpnp_wled_ramp_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	wled->ramp_ms = data;
	return count;
}

/* sysfs show function for ramp step */
static ssize_t qpnp_wled_ramp_step_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->ramp_step);
}

/* sysfs store function for ramp step */
static ssize_t qpnp_wled_ramp_step_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	wled->ramp_step = data;
	return count;
}

/* sysfs show function for dim mode */
static ssize_t qpnp_wled_dim_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char *str;

	if (wled->dim_mode == QPNP_WLED_DIM_ANALOG)
		str = "analog";
	else if (wled->dim_mode == QPNP_WLED_DIM_DIGITAL)
		str = "digital";
	else
		str = "hybrid";

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

/* sysfs store function for dim mode*/
static ssize_t qpnp_wled_dim_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char str[QPNP_WLED_STR_SIZE + 1];
	int rc, temp;
	u8 reg;

	if (snprintf(str, QPNP_WLED_STR_SIZE, "%s", buf) > QPNP_WLED_STR_SIZE)
		return -EINVAL;

	if (strcmp(str, "analog") == 0)
		temp = QPNP_WLED_DIM_ANALOG;
	else if (strcmp(str, "digital") == 0)
		temp = QPNP_WLED_DIM_DIGITAL;
	else
		temp = QPNP_WLED_DIM_HYBRID;

	if (temp == wled->dim_mode)
		return count;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	if (temp == QPNP_WLED_DIM_HYBRID) {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (1 << QPNP_WLED_DIM_HYB_SHIFT);
	} else {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (0 << QPNP_WLED_DIM_HYB_SHIFT);
		reg &= QPNP_WLED_DIM_ANA_MASK;
		reg |= temp;
	}

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc)
		return rc;

	wled->dim_mode = temp;

	return count;
}

/* sysfs show function for dimming curve shape*/
static ssize_t qpnp_wled_dim_shape_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char *str;

	if (wled->dim_shape == QPNP_WLED_DIM_SHAPE_SQUARE)
		str = "square";
	else if (wled->dim_shape == QPNP_WLED_DIM_SHAPE_LOG)
		str = "log";
	else
		str = "linear";

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

/* sysfs store function for dimming curve shape*/
static ssize_t qpnp_wled_dim_shape_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	char str[QPNP_WLED_STR_SIZE + 1];

	if (snprintf(str, QPNP_WLED_STR_SIZE, "%s", buf) > QPNP_WLED_STR_SIZE)
		return -EINVAL;

	if (strcmp(str, "log") == 0)
		wled->dim_shape = QPNP_WLED_DIM_SHAPE_LOG;
	else if (strcmp(str, "square") == 0)
		wled->dim_shape = QPNP_WLED_DIM_SHAPE_SQUARE;
	else
		wled->dim_shape = QPNP_WLED_DIM_SHAPE_LINEAR;

	return count;
}

/* sysfs show function for full scale current in ua*/
static ssize_t qpnp_wled_fs_curr_ua_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->fs_curr_ua);
}

int qpnp_ibb_enable(bool state)
{
	int rc;
	u8 reg;

	if (!gwled) {
		pr_err("%s: wled is not initialized yet\n", __func__);
		return -EAGAIN;
	}

	/* enable lab */
	if (gwled->ibb_bias_active) {
		rc = qpnp_wled_module_en(gwled, gwled->lab_base, state);
		if (rc < 0)
			return rc;
		usleep_range(QPNP_WLED_LAB_START_DLY_US,
				QPNP_WLED_LAB_START_DLY_US + 1);
	} else {
		rc = qpnp_wled_read_reg(gwled, &reg,
				QPNP_WLED_LAB_IBB_RDY_REG(gwled->lab_base));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_MODULE_EN_MASK;
		reg |= (state << QPNP_WLED_MODULE_EN_SHIFT);
		rc = qpnp_wled_write_reg(gwled, &reg,
				QPNP_WLED_LAB_IBB_RDY_REG(gwled->lab_base));
		if (rc)
			return rc;
	}

	rc = qpnp_wled_module_en(gwled, gwled->ibb_base, state);
	if (rc < 0)
		return rc;

	return 0;
}
EXPORT_SYMBOL(qpnp_ibb_enable);

/* sysfs store function for full scale current in ua*/
static ssize_t qpnp_wled_fs_curr_ua_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);
	int data, i, rc, temp;
	u8 reg;

	if (sscanf(buf, "%d", &data) != 1)
		return -EINVAL;

	for (i = 0; i < wled->num_strings; i++) {
		if (data < QPNP_WLED_FS_CURR_MIN_UA)
			data = QPNP_WLED_FS_CURR_MIN_UA;
		else if (data > QPNP_WLED_FS_CURR_MAX_UA)
			data = QPNP_WLED_FS_CURR_MAX_UA;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
							wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_FS_CURR_MASK;
		temp = data / QPNP_WLED_FS_CURR_STEP_UA;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
							wled->strings[i]));
		if (rc)
			return rc;
	}

	wled->fs_curr_ua = data;

	return count;
}

/* sysfs attributes exported by wled */
static struct device_attribute qpnp_wled_attrs[] = {
	__ATTR(dump_regs, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_dump_regs_show,
			NULL),
	__ATTR(dim_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_dim_mode_show,
			qpnp_wled_dim_mode_store),
	__ATTR(dim_shape, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_dim_shape_show,
			qpnp_wled_dim_shape_store),
	__ATTR(fs_curr_ua, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_fs_curr_ua_show,
			qpnp_wled_fs_curr_ua_store),
	__ATTR(start_ramp, (S_IRUGO | S_IWUSR | S_IWGRP),
			NULL,
			qpnp_wled_ramp_store),
	__ATTR(ramp_ms, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_ramp_ms_show,
			qpnp_wled_ramp_ms_store),
	__ATTR(ramp_step, (S_IRUGO | S_IWUSR | S_IWGRP),
			qpnp_wled_ramp_step_show,
			qpnp_wled_ramp_step_store),
};

/* worker for setting wled brightness */
static void qpnp_wled_work(struct work_struct *work)
{
	struct qpnp_wled *wled;
	int level, rc;

	wled = container_of(work, struct qpnp_wled, work);

	level = wled->cdev.brightness;

	mutex_lock(&wled->lock);

	if (level) {
		rc = qpnp_wled_set_level(wled, level);
		if (rc) {
			dev_err(&wled->spmi->dev, "wled set level failed\n");
			return;
		}
	}

	rc = qpnp_wled_module_en(wled, wled->ctrl_base, !!level);

	if (rc) {
		dev_err(&wled->spmi->dev, "wled %sable failed\n",
					level ? "en" : "dis");
		return;
	}
	mutex_unlock(&wled->lock);
}

/* get api registered with led classdev for wled brightness */
static enum led_brightness qpnp_wled_get(struct led_classdev *led_cdev)
{
	struct qpnp_wled *wled;

	wled = container_of(led_cdev, struct qpnp_wled, cdev);

	return wled->cdev.brightness;
}

/* set api registered with led classdev for wled brightness */
static void qpnp_wled_set(struct led_classdev *led_cdev,
				enum led_brightness level)
{
	struct qpnp_wled *wled;

	wled = container_of(led_cdev, struct qpnp_wled, cdev);

	if (level < LED_OFF)
		level = LED_OFF;
	else if (level > wled->cdev.max_brightness)
		level = wled->cdev.max_brightness;

	wled->cdev.brightness = level;
	schedule_work(&wled->work);
}

static int qpnp_wled_set_disp(struct qpnp_wled *wled, u16 base_addr)
{
	int rc;
	u8 reg;

	/* display type */
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_DISP_SEL_REG(base_addr));
	if (rc < 0)
		return rc;

	reg &= QPNP_WLED_DISP_SEL_MASK;
	reg |= (wled->disp_type_amoled << QPNP_WLED_DISP_SEL_SHIFT);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_DISP_SEL_REG(base_addr));
	if (rc)
		return rc;

	return 0;
}

static int qpnp_wled_mod_rdy(struct qpnp_wled *wled, u16 base_addr, bool state)
{
	int rc;
	u8 reg;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MODULE_RDY_REG(base_addr));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MODULE_RDY_MASK;
	reg |= (state << QPNP_WLED_MODULE_RDY_SHIFT);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MODULE_RDY_REG(base_addr));
	if (rc)
		return rc;

	return 0;
}

/* Configure WLED registers */
static int qpnp_wled_config(struct qpnp_wled *wled)
{
	int rc, i, temp;
	u8 reg = 0;

	/* Configure display type */
	rc = qpnp_wled_set_disp(wled, wled->ctrl_base);
	if (rc < 0)
		return rc;

	/* Configure the FEEDBACK OUTPUT register */
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_FDBK_OP_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_FDBK_OP_MASK;
	reg |= wled->fdbk_op;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_FDBK_OP_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the VREF register */
	if (wled->vref_mv < QPNP_WLED_VREF_MIN_MV)
		wled->vref_mv = QPNP_WLED_VREF_MIN_MV;
	else if (wled->vref_mv > QPNP_WLED_VREF_MAX_MV)
		wled->vref_mv = QPNP_WLED_VREF_MAX_MV;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_VREF_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_VREF_MASK;
	temp = wled->vref_mv - QPNP_WLED_VREF_MIN_MV;
	reg |= (temp / QPNP_WLED_VREF_STEP_MV);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_VREF_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the ILIM register */
	if (wled->ilim_ma < QPNP_WLED_ILIM_MIN_MA)
		wled->ilim_ma = QPNP_WLED_ILIM_MIN_MA;
	else if (wled->ilim_ma > QPNP_WLED_ILIM_MAX_MA)
		wled->ilim_ma = QPNP_WLED_ILIM_MAX_MA;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_ILIM_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_ILIM_MASK;
	reg |= (wled->ilim_ma / QPNP_WLED_ILIM_STEP_MA);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_ILIM_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the MAX BOOST DUTY register */
	if (wled->boost_duty_ns < QPNP_WLED_BOOST_DUTY_MIN_NS)
		wled->boost_duty_ns = QPNP_WLED_BOOST_DUTY_MIN_NS;
	else if (wled->boost_duty_ns > QPNP_WLED_BOOST_DUTY_MAX_NS)
		wled->boost_duty_ns = QPNP_WLED_BOOST_DUTY_MAX_NS;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_BOOST_DUTY_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_BOOST_DUTY_MASK;
	reg |= (wled->boost_duty_ns / QPNP_WLED_BOOST_DUTY_STEP_NS);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_BOOST_DUTY_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the SWITCHING FREQ register */
	if (wled->switch_freq_khz == QPNP_WLED_SWITCH_FREQ_1600_KHZ)
		temp = QPNP_WLED_SWITCH_FREQ_1600_KHZ_CODE;
	else
		temp = QPNP_WLED_SWITCH_FREQ_800_KHZ_CODE;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_SWITCH_FREQ_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_SWITCH_FREQ_MASK;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_SWITCH_FREQ_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the OVP register */
	if (wled->ovp_mv <= QPNP_WLED_OVP_17800_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_17800_MV;
		temp = 3;
	} else if (wled->ovp_mv <= QPNP_WLED_OVP_19400_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_19400_MV;
		temp = 2;
	} else if (wled->ovp_mv <= QPNP_WLED_OVP_29500_MV) {
		wled->ovp_mv = QPNP_WLED_OVP_29500_MV;
		temp = 1;
	} else {
		wled->ovp_mv = QPNP_WLED_OVP_31000_MV;
		temp = 0;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_OVP_REG(wled->ctrl_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_OVP_MASK;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_OVP_REG(wled->ctrl_base));
	if (rc)
		return rc;

	/* Configure the MODULATION register */
	if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_1200_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_1200_KHZ;
		temp = 3;
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_2400_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_2400_KHZ;
		temp = 2;
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_9600_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
		temp = 1;
	} else {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_19200_KHZ;
		temp = 0;
	}
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MOD_FREQ_MASK;
	reg |= (temp << QPNP_WLED_MOD_FREQ_SHIFT);

	reg &= QPNP_WLED_PHASE_STAG_MASK;
	reg |= (wled->en_phase_stag << QPNP_WLED_PHASE_STAG_SHIFT);

	reg &= QPNP_WLED_DIM_RES_MASK;
	reg |= (wled->en_9b_dim_res << QPNP_WLED_DIM_RES_SHIFT);

	if (wled->dim_mode == QPNP_WLED_DIM_HYBRID) {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (1 << QPNP_WLED_DIM_HYB_SHIFT);
	} else {
		reg &= QPNP_WLED_DIM_HYB_MASK;
		reg |= (0 << QPNP_WLED_DIM_HYB_SHIFT);
		reg &= QPNP_WLED_DIM_ANA_MASK;
		reg |= wled->dim_mode;
	}

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc)
		return rc;

	/* Configure the HYBRID THRESHOLD register */
	if (wled->hyb_thres < QPNP_WLED_HYB_THRES_MIN)
		wled->hyb_thres = QPNP_WLED_HYB_THRES_MIN;
	else if (wled->hyb_thres > QPNP_WLED_HYB_THRES_MAX)
		wled->hyb_thres = QPNP_WLED_HYB_THRES_MAX;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_HYB_THRES_REG(wled->sink_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_HYB_THRES_MASK;
	temp = fls(wled->hyb_thres / QPNP_WLED_HYB_THRES_MIN) - 1;
	reg |= temp;
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_HYB_THRES_REG(wled->sink_base));
	if (rc)
		return rc;

	for (i = 0; i < wled->num_strings; i++) {
		if (wled->strings[i] >= QPNP_WLED_MAX_STRINGS) {
			dev_err(&wled->spmi->dev, "Invalid string number\n");
			return -EINVAL;
		}

		/* MODULATOR */
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_MOD_EN_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_MOD_EN_MASK;
		reg |= (QPNP_WLED_MOD_EN << QPNP_WLED_MOD_EN_SHFT);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_MOD_EN_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* SYNC DELAY */
		if (wled->sync_dly_us < QPNP_WLED_SYNC_DLY_MIN_US)
			wled->sync_dly_us = QPNP_WLED_SYNC_DLY_MIN_US;
		else if (wled->sync_dly_us > QPNP_WLED_SYNC_DLY_MAX_US)
			wled->sync_dly_us = QPNP_WLED_SYNC_DLY_MAX_US;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_SYNC_DLY_MASK;
		temp = wled->sync_dly_us / QPNP_WLED_SYNC_DLY_STEP_US;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* FULL SCALE CURRENT */
		if (wled->fs_curr_ua < QPNP_WLED_FS_CURR_MIN_UA)
			wled->fs_curr_ua = QPNP_WLED_FS_CURR_MIN_UA;
		else if (wled->fs_curr_ua > QPNP_WLED_FS_CURR_MAX_UA)
			wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_FS_CURR_MASK;
		temp = wled->fs_curr_ua / QPNP_WLED_FS_CURR_STEP_UA;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* CABC */
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_CABC_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_CABC_MASK;
		reg |= (wled->en_cabc << QPNP_WLED_CABC_SHIFT);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_CABC_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* Enable CURRENT SINK */
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_CURR_SINK_REG(wled->sink_base));
		if (rc < 0)
			return rc;
		temp = wled->strings[i] + QPNP_WLED_CURR_SINK_SHIFT;
		reg |= (1 << temp);
		rc = qpnp_wled_write_reg(wled, &reg,
				QPNP_WLED_CURR_SINK_REG(wled->sink_base));
		if (rc)
			return rc;
	}

	/* LAB fast precharge */
	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_LAB_FAST_PC_REG(wled->lab_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_LAB_FAST_PC_MASK;
	reg |= (wled->lab_fast_precharge << QPNP_WLED_LAB_FAST_PC_SHIFT);
	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_LAB_FAST_PC_REG(wled->lab_base));
	if (rc)
		return rc;

	/* Configure lab display type */
	rc = qpnp_wled_set_disp(wled, wled->lab_base);
	if (rc < 0)
		return rc;

	/* make LAB module ready */
	rc = qpnp_wled_mod_rdy(wled, wled->lab_base, true);
	if (rc < 0)
		return rc;

	/* IBB active bias */
	if (wled->ibb_pwrup_dly_ms < QPNP_WLED_IBB_PWRUP_DLY_MIN_MS)
		wled->ibb_pwrup_dly_ms = QPNP_WLED_IBB_PWRUP_DLY_MIN_MS;
	else if (wled->ibb_pwrup_dly_ms > QPNP_WLED_IBB_PWRUP_DLY_MAX_MS)
		wled->ibb_pwrup_dly_ms = QPNP_WLED_IBB_PWRUP_DLY_MAX_MS;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_IBB_BIAS_REG(wled->ibb_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_IBB_BIAS_MASK;
	reg |= (!wled->ibb_bias_active << QPNP_WLED_IBB_BIAS_SHIFT);

	temp = fls(wled->ibb_pwrup_dly_ms) - 1;
	reg &= QPNP_WLED_IBB_PWRUP_DLY_MASK;
	reg |= (temp << QPNP_WLED_IBB_PWRUP_DLY_SHIFT);

	rc = qpnp_wled_sec_access(wled, wled->ibb_base);
	if (rc)
		return rc;

	rc = qpnp_wled_write_reg(wled, &reg,
			QPNP_WLED_IBB_BIAS_REG(wled->ibb_base));
	if (rc)
		return rc;

	/* Configure ibb display type */
	rc = qpnp_wled_set_disp(wled, wled->ibb_base);
	if (rc < 0)
		return rc;

	/* make IBB module ready */
	rc = qpnp_wled_mod_rdy(wled, wled->ibb_base, true);
	if (rc < 0)
		return rc;

	return 0;
}

/* parse wled dtsi parameters */
static int qpnp_wled_parse_dt(struct qpnp_wled *wled)
{
	struct spmi_device *spmi = wled->spmi;
	struct property *prop;
	const char *temp_str;
	u32 temp_val;
	int rc, i;

	wled->cdev.name = "wled";
	rc = of_property_read_string(spmi->dev.of_node,
			"linux,name", &wled->cdev.name);
	if (rc && (rc != -EINVAL)) {
		dev_err(&spmi->dev, "Unable to read led name\n");
		return rc;
	}

	wled->cdev.default_trigger = QPNP_WLED_TRIGGER_NONE;
	rc = of_property_read_string(spmi->dev.of_node, "linux,default-trigger",
					&wled->cdev.default_trigger);
	if (rc && (rc != -EINVAL)) {
		dev_err(&spmi->dev, "Unable to read led trigger\n");
		return rc;
	}

	wled->disp_type_amoled = of_property_read_bool(spmi->dev.of_node,
				"qcom,disp-type-amoled");

	wled->fdbk_op = QPNP_WLED_FDBK_AUTO;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,fdbk-output", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "wled1") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED1;
		else if (strcmp(temp_str, "wled2") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED2;
		else if (strcmp(temp_str, "wled3") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED3;
		else if (strcmp(temp_str, "wled4") == 0)
			wled->fdbk_op = QPNP_WLED_FDBK_WLED4;
		else
			wled->fdbk_op = QPNP_WLED_FDBK_AUTO;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read feedback output\n");
		return rc;
	}

	wled->vref_mv = QPNP_WLED_DFLT_VREF_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,vref-mv", &temp_val);
	if (!rc) {
		wled->vref_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vref\n");
		return rc;
	}

	wled->switch_freq_khz = QPNP_WLED_SWITCH_FREQ_800_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,switch-freq-khz", &temp_val);
	if (!rc) {
		wled->switch_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read switch freq\n");
		return rc;
	}

	wled->ovp_mv = QPNP_WLED_OVP_29500_MV;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ovp-mv", &temp_val);
	if (!rc) {
		wled->ovp_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read vref\n");
		return rc;
	}

	wled->ilim_ma = QPNP_WLED_DFLT_ILIM_MA;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,ilim-ma", &temp_val);
	if (!rc) {
		wled->ilim_ma = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read ilim\n");
		return rc;
	}

	wled->boost_duty_ns = QPNP_WLED_DEF_BOOST_DUTY_NS;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,boost-duty-ns", &temp_val);
	if (!rc) {
		wled->boost_duty_ns = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read boost duty\n");
		return rc;
	}

	wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_19200_KHZ;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,mod-freq-khz", &temp_val);
	if (!rc) {
		wled->mod_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read modulation freq\n");
		return rc;
	}

	wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,dim-mode", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "analog") == 0)
			wled->dim_mode = QPNP_WLED_DIM_ANALOG;
		else if (strcmp(temp_str, "digital") == 0)
			wled->dim_mode = QPNP_WLED_DIM_DIGITAL;
		else
			wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read dim mode\n");
		return rc;
	}

	wled->dim_shape = QPNP_WLED_DIM_SHAPE_LINEAR;
	rc = of_property_read_string(spmi->dev.of_node,
			"qcom,dim-method", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "log") == 0)
			wled->dim_shape = QPNP_WLED_DIM_SHAPE_LOG;
		else if (strcmp(temp_str, "square") == 0)
			wled->dim_shape = QPNP_WLED_DIM_SHAPE_SQUARE;
		else
			wled->dim_shape = QPNP_WLED_DIM_SHAPE_LINEAR;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read dim method\n");
		return rc;
	}

	if (wled->dim_mode == QPNP_WLED_DIM_HYBRID) {
		wled->hyb_thres = QPNP_WLED_DEF_HYB_THRES;
		rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,hyb-thres", &temp_val);
		if (!rc) {
			wled->hyb_thres = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read hyb threshold\n");
			return rc;
		}
	}

	wled->sync_dly_us = QPNP_WLED_DEF_SYNC_DLY_US;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,sync-dly-us", &temp_val);
	if (!rc) {
		wled->sync_dly_us = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read sync delay\n");
		return rc;
	}

	wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;
	rc = of_property_read_u32(spmi->dev.of_node,
			"qcom,fs-curr-ua", &temp_val);
	if (!rc) {
		wled->fs_curr_ua = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read full scale current\n");
		return rc;
	}

	wled->en_9b_dim_res = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-9b-dim-res");
	wled->en_phase_stag = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-phase-stag");
	wled->en_cabc = of_property_read_bool(spmi->dev.of_node,
			"qcom,en-cabc");

	prop = of_find_property(spmi->dev.of_node,
			"qcom,led-strings-list", &temp_val);
	if (!prop || !temp_val || temp_val > QPNP_WLED_MAX_STRINGS) {
		dev_err(&spmi->dev, "Invalid strings info, use default");
		wled->num_strings = QPNP_WLED_MAX_STRINGS;
		for (i = 0; i < wled->num_strings; i++)
			wled->strings[i] = i;
	} else {
		wled->num_strings = temp_val;
		memcpy(wled->strings, prop->value, temp_val);
	}

	wled->ibb_bias_active = of_property_read_bool(spmi->dev.of_node,
				"qcom,ibb-bias-active");

	wled->ibb_pwrup_dly_ms = QPNP_WLED_IBB_PWRUP_DLY_MIN_MS;
	rc = of_property_read_u32(spmi->dev.of_node,
				"qcom,ibb-pwrup-dly", &temp_val);
	if (!rc) {
		wled->ibb_pwrup_dly_ms = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&spmi->dev, "Unable to read ibb pwrup delay\n");
		return rc;
	}

	wled->lab_fast_precharge = of_property_read_bool(spmi->dev.of_node,
				"qcom,lab-fast-precharge");
	return 0;
}

static int qpnp_wled_probe(struct spmi_device *spmi)
{
	struct qpnp_wled *wled;
	struct resource *wled_resource;
	int rc, i;

	wled = devm_kzalloc(&spmi->dev, sizeof(*wled), GFP_KERNEL);
	if (!wled)
		return -ENOMEM;

	wled->spmi = spmi;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_SINK_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get wled sink base address\n");
		return -EINVAL;
	}

	wled->sink_base = wled_resource->start;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_CTRL_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get wled ctrl base address\n");
		return -EINVAL;
	}

	wled->ctrl_base = wled_resource->start;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_IBB_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get IBB base address\n");
		return -EINVAL;
	}

	wled->ibb_base = wled_resource->start;

	wled_resource = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
					QPNP_WLED_LAB_BASE);
	if (!wled_resource) {
		dev_err(&spmi->dev, "Unable to get LAB base address\n");
		return -EINVAL;
	}

	wled->lab_base = wled_resource->start;

	dev_set_drvdata(&spmi->dev, wled);

	rc = qpnp_wled_parse_dt(wled);
	if (rc) {
		dev_err(&spmi->dev, "DT parsing failed\n");
		return rc;
	}

	rc = qpnp_wled_config(wled);
	if (rc) {
		dev_err(&spmi->dev, "wled config failed\n");
		return rc;
	}

	mutex_init(&wled->lock);
	INIT_WORK(&wled->work, qpnp_wled_work);
	wled->ramp_ms = QPNP_WLED_RAMP_DLY_MS;
	wled->ramp_step = 1;

	wled->cdev.brightness_set = qpnp_wled_set;
	wled->cdev.brightness_get = qpnp_wled_get;

	if (wled->en_9b_dim_res)
		wled->cdev.max_brightness = WLED_MAX_LEVEL_511;
	else
		wled->cdev.max_brightness = WLED_MAX_LEVEL_4095;

	rc = led_classdev_register(&spmi->dev, &wled->cdev);
	if (rc) {
		dev_err(&spmi->dev, "wled registration failed(%d)\n", rc);
		goto wled_register_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_wled_attrs); i++) {
		rc = sysfs_create_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);
		if (rc < 0) {
			dev_err(&spmi->dev, "sysfs creation failed\n");
			goto sysfs_fail;
		}
	}

	gwled = wled;

	return 0;

sysfs_fail:
	for (i--; i >= 0; i--)
		sysfs_remove_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);
	led_classdev_unregister(&wled->cdev);
wled_register_fail:
	cancel_work_sync(&wled->work);
	mutex_destroy(&wled->lock);
	return rc;
}

static int qpnp_wled_remove(struct spmi_device *spmi)
{
	struct qpnp_wled *wled = dev_get_drvdata(&spmi->dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(qpnp_wled_attrs); i++)
		sysfs_remove_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);

	led_classdev_unregister(&wled->cdev);
	cancel_work_sync(&wled->work);
	mutex_destroy(&wled->lock);

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-wled",},
	{ },
};

static struct spmi_driver qpnp_wled_driver = {
	.driver		= {
		.name	= "qcom,qpnp-wled",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_wled_probe,
	.remove		= qpnp_wled_remove,
};

static int __init qpnp_wled_init(void)
{
	return spmi_driver_register(&qpnp_wled_driver);
}
module_init(qpnp_wled_init);

static void __exit qpnp_wled_exit(void)
{
	spmi_driver_unregister(&qpnp_wled_driver);
}
module_exit(qpnp_wled_exit);

MODULE_DESCRIPTION("QPNP WLED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-wled");

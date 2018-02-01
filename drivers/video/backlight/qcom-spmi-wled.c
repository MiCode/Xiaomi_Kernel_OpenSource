/* Copyright (c) 2015, Sony Mobile Communications, AB.
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"WLED: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/regmap.h>

/* General definitions */
#define WLED_DEFAULT_BRIGHTNESS		2048
#define  WLED_MAX_BRIGHTNESS		4095

#define WLED_SOFT_START_DLY_US		10000

/* WLED control registers */
#define WLED_CTRL_FAULT_STATUS		0x08
#define  WLED_CTRL_ILIM_FAULT_BIT	BIT(0)
#define  WLED_CTRL_OVP_FAULT_BIT	BIT(1)
#define  WLED_CTRL_SC_FAULT_BIT		BIT(2)

#define WLED_CTRL_INT_RT_STS		0x10
#define  WLED_CTRL_OVP_FLT_RT_STS_BIT	BIT(1)

#define WLED_CTRL_MOD_ENABLE		0x46
#define  WLED_CTRL_MOD_EN_MASK		BIT(7)
#define  WLED_CTRL_MODULE_EN_SHIFT	7

#define WLED_CTRL_FDBK_OP		0x48

#define WLED_CTRL_SWITCH_FREQ		0x4c
#define  WLED_CTRL_SWITCH_FREQ_MASK	GENMASK(3, 0)

#define WLED_CTRL_OVP			0x4d
#define  WLED_CTRL_OVP_MASK		GENMASK(1, 0)

#define WLED_CTRL_ILIM			0x4e
#define  WLED_CTRL_ILIM_MASK		GENMASK(2, 0)

#define WLED_CTRL_SHORT_PROTECT		0x5e
#define  WLED_CTRL_SHORT_EN_MASK	BIT(7)

#define WLED_CTRL_SEC_ACCESS		0xd0
#define  WLED_CTRL_SEC_UNLOCK		0xa5

#define WLED_CTRL_TEST1			0xe2
#define  WLED_EXT_FET_DTEST2		0x09

/* WLED sink registers */
#define WLED_SINK_CURR_SINK_EN		0x46
#define  WLED_SINK_CURR_SINK_MASK	GENMASK(7, 4)
#define  WLED_SINK_CURR_SINK_SHFT	0x04

#define WLED_SINK_SYNC			0x47
#define  WLED_SINK_SYNC_MASK		GENMASK(3, 0)
#define  WLED_SINK_SYNC_LED1		BIT(0)
#define  WLED_SINK_SYNC_LED2		BIT(1)
#define  WLED_SINK_SYNC_LED3		BIT(2)
#define  WLED_SINK_SYNC_LED4		BIT(3)
#define  WLED_SINK_SYNC_CLEAR		0x00

#define WLED_SINK_MOD_EN_REG(n)		(0x50 + (n * 0x10))
#define  WLED_SINK_REG_STR_MOD_MASK	BIT(7)
#define  WLED_SINK_REG_STR_MOD_EN	BIT(7)

#define WLED_SINK_SYNC_DLY_REG(n)	(0x51 + (n * 0x10))
#define WLED_SINK_FS_CURR_REG(n)	(0x52 + (n * 0x10))
#define  WLED_SINK_FS_MASK		GENMASK(3, 0)

#define WLED_SINK_CABC_REG(n)		(0x56 + (n * 0x10))
#define  WLED_SINK_CABC_MASK		BIT(7)
#define  WLED_SINK_CABC_EN		BIT(7)

#define WLED_SINK_BRIGHT_LSB_REG(n)	(0x57 + (n * 0x10))
#define WLED_SINK_BRIGHT_MSB_REG(n)	(0x58 + (n * 0x10))

struct wled_config {
	u32 i_boost_limit;
	u32 ovp;
	u32 switch_freq;
	u32 fs_current;
	u32 string_cfg;
	int sc_irq;
	int ovp_irq;
	bool en_cabc;
	bool ext_pfet_sc_pro_en;
	bool auto_calib_enabled;
};

struct wled {
	const char *name;
	struct platform_device *pdev;
	struct regmap *regmap;
	struct mutex lock;
	struct wled_config cfg;
	ktime_t last_sc_event_time;
	ktime_t start_ovp_fault_time;
	u16 sink_addr;
	u16 ctrl_addr;
	u16 auto_calibration_ovp_count;
	u32 brightness;
	u32 sc_count;
	bool prev_state;
	bool ovp_irq_disabled;
	bool auto_calib_done;
	bool force_mod_disable;
};

static int wled_module_enable(struct wled *wled, int val)
{
	int rc;

	if (wled->force_mod_disable)
		return 0;

	rc = regmap_update_bits(wled->regmap, wled->ctrl_addr +
			WLED_CTRL_MOD_ENABLE, WLED_CTRL_MOD_EN_MASK,
			val << WLED_CTRL_MODULE_EN_SHIFT);
	if (rc < 0)
		return rc;
	/*
	 * Wait for at least 10ms before enabling OVP fault interrupt after
	 * enabling the module so that soft start is completed. Keep the OVP
	 * interrupt disabled when the module is disabled.
	 */
	if (val) {
		usleep_range(WLED_SOFT_START_DLY_US,
				WLED_SOFT_START_DLY_US + 1000);

		if (wled->cfg.ovp_irq > 0 && wled->ovp_irq_disabled) {
			enable_irq(wled->cfg.ovp_irq);
			wled->ovp_irq_disabled = false;
		}
	} else {
		if (wled->cfg.ovp_irq > 0 && !wled->ovp_irq_disabled) {
			disable_irq(wled->cfg.ovp_irq);
			wled->ovp_irq_disabled = true;
		}
	}

	return rc;
}

static int wled_get_brightness(struct backlight_device *bl)
{
	struct wled *wled = bl_get_data(bl);

	return wled->brightness;
}

static int wled_sync_toggle(struct wled *wled)
{
	int rc;

	rc = regmap_update_bits(wled->regmap,
			wled->sink_addr + WLED_SINK_SYNC,
			WLED_SINK_SYNC_MASK, WLED_SINK_SYNC_MASK);
	if (rc < 0)
		return rc;

	rc = regmap_update_bits(wled->regmap,
			wled->sink_addr + WLED_SINK_SYNC,
			WLED_SINK_SYNC_MASK, WLED_SINK_SYNC_CLEAR);

	return rc;
}

static int wled_set_brightness(struct wled *wled, u16 brightness)
{
	int rc, i;
	u16 low_limit = WLED_MAX_BRIGHTNESS * 4 / 1000;
	u8 string_cfg = wled->cfg.string_cfg;
	u8 v[2];

	/* WLED's lower limit of operation is 0.4% */
	if (brightness > 0 && brightness < low_limit)
		brightness = low_limit;

	v[0] = brightness & 0xff;
	v[1] = (brightness >> 8) & 0xf;

	for (i = 0; (string_cfg >> i) != 0; i++) {
		rc = regmap_bulk_write(wled->regmap, wled->sink_addr +
				WLED_SINK_BRIGHT_LSB_REG(i), v, 2);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int wled_update_status(struct backlight_device *bl)
{
	struct wled *wled = bl_get_data(bl);
	u16 brightness = bl->props.brightness;
	int rc;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK ||
	    bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	mutex_lock(&wled->lock);
	if (brightness) {
		rc = wled_set_brightness(wled, brightness);
		if (rc < 0) {
			pr_err("wled failed to set brightness rc:%d\n", rc);
			goto unlock_mutex;
		}

		if (!!brightness != wled->prev_state) {
			rc = wled_module_enable(wled, !!brightness);
			if (rc < 0) {
				pr_err("wled enable failed rc:%d\n", rc);
				goto unlock_mutex;
			}
		}
	} else {
		rc = wled_module_enable(wled, brightness);
		if (rc < 0) {
			pr_err("wled disable failed rc:%d\n", rc);
			goto unlock_mutex;
		}
	}

	wled->prev_state = !!brightness;

	rc = wled_sync_toggle(wled);
	if (rc < 0) {
		pr_err("wled sync failed rc:%d\n", rc);
		goto unlock_mutex;
	}

	wled->brightness = brightness;

unlock_mutex:
	mutex_unlock(&wled->lock);
	return rc;
}

#define WLED_SC_DLY_MS			20
#define WLED_SC_CNT_MAX			5
#define WLED_SC_RESET_CNT_DLY_US	1000000
static irqreturn_t wled_sc_irq_handler(int irq, void *_wled)
{
	struct wled *wled = _wled;
	int rc;
	u32 val;
	s64 elapsed_time;

	rc = regmap_read(wled->regmap,
		wled->ctrl_addr + WLED_CTRL_FAULT_STATUS, &val);
	if (rc < 0) {
		pr_err("Error in reading WLED_FAULT_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	wled->sc_count++;
	pr_err("WLED short circuit detected %d times fault_status=%x\n",
		wled->sc_count, val);
	mutex_lock(&wled->lock);
	rc = wled_module_enable(wled, false);
	if (rc < 0) {
		pr_err("wled disable failed rc:%d\n", rc);
		goto unlock_mutex;
	}

	elapsed_time = ktime_us_delta(ktime_get(),
				wled->last_sc_event_time);
	if (elapsed_time > WLED_SC_RESET_CNT_DLY_US) {
		wled->sc_count = 0;
	} else if (wled->sc_count > WLED_SC_CNT_MAX) {
		pr_err("SC trigged %d times, disabling WLED forever!\n",
			wled->sc_count);
		goto unlock_mutex;
	}

	wled->last_sc_event_time = ktime_get();

	msleep(WLED_SC_DLY_MS);
	rc = wled_module_enable(wled, true);
	if (rc < 0)
		pr_err("wled enable failed rc:%d\n", rc);

unlock_mutex:
	mutex_unlock(&wled->lock);

	return IRQ_HANDLED;
}

#define AUTO_CALIB_BRIGHTNESS		200
static int wled_auto_calibrate(struct wled *wled)
{
	int rc = 0, i;
	u32 sink_config = 0, int_sts;
	u8 reg = 0, sink_test = 0, sink_valid = 0;
	u8 string_cfg = wled->cfg.string_cfg;

	if (wled->auto_calib_done)
		return 0;

	/* read configured sink configuration */
	rc = regmap_read(wled->regmap, wled->sink_addr +
			WLED_SINK_CURR_SINK_EN, &sink_config);
	if (rc < 0) {
		pr_err("Failed to read SINK configuration rc=%d\n", rc);
		goto failed_calib;
	}

	/* disable the module before starting calibration */
	rc = regmap_update_bits(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_MOD_ENABLE,
			WLED_CTRL_MOD_EN_MASK, 0);
	if (rc < 0) {
		pr_err("Failed to disable WLED module rc=%d\n", rc);
		goto failed_calib;
	}

	/* set low brightness across all sinks */
	rc = wled_set_brightness(wled, AUTO_CALIB_BRIGHTNESS);
	if (rc < 0) {
		pr_err("Failed to set brightness for calibration rc=%d\n", rc);
		goto failed_calib;
	}

	if (wled->cfg.en_cabc) {
		for (i = 0; (string_cfg >> i) != 0; i++) {
			reg = 0;
			rc = regmap_update_bits(wled->regmap, wled->sink_addr +
					WLED_SINK_CABC_REG(i),
					WLED_SINK_CABC_MASK, reg);
			if (rc < 0)
				goto failed_calib;
		}
	}

	/* disable all sinks */
	rc = regmap_write(wled->regmap,
			wled->sink_addr + WLED_SINK_CURR_SINK_EN, 0);
	if (rc < 0) {
		pr_err("Failed to disable all sinks rc=%d\n", rc);
		goto failed_calib;
	}

	/* iterate through the strings one by one */
	for (i = 0; (string_cfg >> i) != 0; i++) {
		sink_test = 1 << (WLED_SINK_CURR_SINK_SHFT + i);

		/* Enable feedback control */
		rc = regmap_write(wled->regmap, wled->ctrl_addr +
				WLED_CTRL_FDBK_OP, i + 1);
		if (rc < 0) {
			pr_err("Failed to enable feedback for SINK %d rc = %d\n",
				i + 1, rc);
			goto failed_calib;
		}

		/* enable the sink */
		rc = regmap_write(wled->regmap, wled->sink_addr +
				WLED_SINK_CURR_SINK_EN, sink_test);
		if (rc < 0) {
			pr_err("Failed to configure SINK %d rc=%d\n",
						i + 1, rc);
			goto failed_calib;
		}

		/* Enable the module */
		rc = regmap_update_bits(wled->regmap, wled->ctrl_addr +
				WLED_CTRL_MOD_ENABLE,
				WLED_CTRL_MOD_EN_MASK,
				WLED_CTRL_MOD_EN_MASK);
		if (rc < 0) {
			pr_err("Failed to enable WLED module rc=%d\n", rc);
			goto failed_calib;
		}

		usleep_range(WLED_SOFT_START_DLY_US,
			     WLED_SOFT_START_DLY_US + 1000);

		rc = regmap_read(wled->regmap, wled->ctrl_addr +
				 WLED_CTRL_INT_RT_STS, &int_sts);
		if (rc < 0) {
			pr_err("Error in reading WLED_INT_RT_STS rc=%d\n", rc);
			goto failed_calib;
		}

		if (int_sts & WLED_CTRL_OVP_FAULT_BIT)
			pr_debug("WLED OVP fault detected with SINK %d\n",
						i + 1);
		else
			sink_valid |= sink_test;

		/* Disable the module */
		rc = regmap_update_bits(wled->regmap,
				wled->ctrl_addr + WLED_CTRL_MOD_ENABLE,
				WLED_CTRL_MOD_EN_MASK, 0);
		if (rc < 0) {
			pr_err("Failed to disable WLED module rc=%d\n", rc);
			goto failed_calib;
		}
	}

	if (sink_valid == sink_config) {
		pr_debug("WLED auto-calibration complete, default sink-config=%x OK!\n",
						sink_config);
	} else {
		pr_warn("Invalid WLED default sink config=%x changing it to=%x\n",
						sink_config, sink_valid);
		sink_config = sink_valid;
	}

	if (!sink_config) {
		pr_err("No valid WLED sinks found\n");
		wled->force_mod_disable = true;
		goto failed_calib;
	}

	/* write the new sink configuration */
	rc = regmap_write(wled->regmap,
			wled->sink_addr + WLED_SINK_CURR_SINK_EN,
			sink_config);
	if (rc < 0) {
		pr_err("Failed to reconfigure the default sink rc=%d\n", rc);
		goto failed_calib;
	}

	/* MODULATOR_EN setting for valid sinks */
	for (i = 0; (string_cfg >> i) != 0; i++) {
		if (wled->cfg.en_cabc) {
			reg = WLED_SINK_CABC_EN;
			rc = regmap_update_bits(wled->regmap, wled->sink_addr +
						WLED_SINK_CABC_REG(i),
						WLED_SINK_CABC_MASK, reg);
			if (rc < 0)
				goto failed_calib;
		}

		if (sink_config & (1 << (WLED_SINK_CURR_SINK_SHFT + i)))
			reg = WLED_SINK_REG_STR_MOD_EN;
		else
			reg = 0x0; /* disable modulator_en for unused sink */

		rc = regmap_write(wled->regmap, wled->sink_addr +
				WLED_SINK_MOD_EN_REG(i), reg);
		if (rc < 0) {
			pr_err("Failed to configure MODULATOR_EN rc=%d\n", rc);
			goto failed_calib;
		}
	}

	/* restore the feedback setting */
	rc = regmap_write(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_FDBK_OP, 0);
	if (rc < 0) {
		pr_err("Failed to restore feedback setting rc=%d\n", rc);
		goto failed_calib;
	}

	/* restore  brightness */
	rc = wled_set_brightness(wled, wled->brightness);
	if (rc < 0) {
		pr_err("Failed to set brightness after calibration rc=%d\n",
			rc);
		goto failed_calib;
	}

	rc = regmap_update_bits(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_MOD_ENABLE,
			WLED_CTRL_MOD_EN_MASK,
			WLED_CTRL_MOD_EN_MASK);
	if (rc < 0) {
		pr_err("Failed to enable WLED module rc=%d\n", rc);
		goto failed_calib;
	}

	/* delay for WLED soft-start */
	usleep_range(WLED_SOFT_START_DLY_US,
		     WLED_SOFT_START_DLY_US + 1000);

	wled->auto_calib_done = true;

failed_calib:
	return rc;
}

#define WLED_AUTO_CAL_OVP_COUNT		5
#define WLED_AUTO_CAL_CNT_DLY_US	1000000	/* 1 second */
static bool wled_auto_cal_required(struct wled *wled)
{
	s64 elapsed_time_us;

	/*
	 * Check if the OVP fault was an occasional one
	 * or if its firing continuously, the latter qualifies
	 * for an auto-calibration check.
	 */
	if (!wled->auto_calibration_ovp_count) {
		wled->start_ovp_fault_time = ktime_get();
		wled->auto_calibration_ovp_count++;
	} else {
		elapsed_time_us = ktime_us_delta(ktime_get(),
				wled->start_ovp_fault_time);
		if (elapsed_time_us > WLED_AUTO_CAL_CNT_DLY_US)
			wled->auto_calibration_ovp_count = 0;
		else
			wled->auto_calibration_ovp_count++;

		if (wled->auto_calibration_ovp_count >=
				WLED_AUTO_CAL_OVP_COUNT) {
			wled->auto_calibration_ovp_count = 0;
			return true;
		}
	}

	return false;
}

static int wled_auto_calibrate_at_init(struct wled *wled)
{
	int rc;
	u32 fault_status = 0, rt_status = 0;

	if (!wled->cfg.auto_calib_enabled)
		return 0;

	rc = regmap_read(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_INT_RT_STS,
			&rt_status);
	if (rc < 0)
		pr_err("Failed to read RT status rc=%d\n", rc);

	rc = regmap_read(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_FAULT_STATUS,
			&fault_status);
	if (rc < 0)
		pr_err("Failed to read fault status rc=%d\n", rc);

	if ((rt_status & WLED_CTRL_OVP_FLT_RT_STS_BIT) ||
			(fault_status & WLED_CTRL_OVP_FAULT_BIT)) {
		mutex_lock(&wled->lock);
		rc = wled_auto_calibrate(wled);
		if (!rc)
			wled->auto_calib_done = true;
		mutex_unlock(&wled->lock);
	}

	return rc;
}

static void handle_ovp_fault(struct wled *wled)
{
	int rc;

	if (!wled->cfg.auto_calib_enabled)
		return;

	mutex_lock(&wled->lock);
	if (wled->cfg.ovp_irq > 0 && !wled->ovp_irq_disabled) {
		disable_irq_nosync(wled->cfg.ovp_irq);
		wled->ovp_irq_disabled = true;
	}

	if (wled_auto_cal_required(wled))
		wled_auto_calibrate(wled);

	if (wled->cfg.ovp_irq > 0 && wled->ovp_irq_disabled) {
		enable_irq(wled->cfg.ovp_irq);
		wled->ovp_irq_disabled = false;
	}
	mutex_unlock(&wled->lock);
}

static irqreturn_t wled_ovp_irq_handler(int irq, void *_wled)
{
	struct wled *wled = _wled;
	int rc;
	u32 int_sts, fault_sts;

	rc = regmap_read(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_INT_RT_STS, &int_sts);
	if (rc < 0) {
		pr_err("Error in reading WLED_INT_RT_STS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	rc = regmap_read(wled->regmap, wled->ctrl_addr +
			WLED_CTRL_FAULT_STATUS, &fault_sts);
	if (rc < 0) {
		pr_err("Error in reading WLED_FAULT_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	if (fault_sts &
		(WLED_CTRL_OVP_FAULT_BIT | WLED_CTRL_ILIM_FAULT_BIT))
		pr_err("WLED OVP fault detected, int_sts=%x fault_sts= %x\n",
			int_sts, fault_sts);

	if (fault_sts & WLED_CTRL_OVP_FAULT_BIT)
		handle_ovp_fault(wled);

	return IRQ_HANDLED;
}

static int wled_setup(struct wled *wled)
{
	int rc, temp, i;
	u8 sink_en = 0;
	u32 val;
	u8 string_cfg = wled->cfg.string_cfg;
	int sc_irq = wled->cfg.sc_irq;
	int ovp_irq = wled->cfg.ovp_irq;

	rc = regmap_update_bits(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_OVP,
			WLED_CTRL_OVP_MASK, wled->cfg.ovp);
	if (rc < 0)
		return rc;

	rc = regmap_update_bits(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_ILIM,
			WLED_CTRL_ILIM_MASK, wled->cfg.i_boost_limit);
	if (rc < 0)
		return rc;

	rc = regmap_update_bits(wled->regmap,
			wled->ctrl_addr + WLED_CTRL_SWITCH_FREQ,
			WLED_CTRL_SWITCH_FREQ_MASK, wled->cfg.switch_freq);
	if (rc < 0)
		return rc;

	for (i = 0; (string_cfg >> i) != 0; i++) {
		if (string_cfg & BIT(i)) {
			u16 addr = wled->sink_addr +
					WLED_SINK_MOD_EN_REG(i);

			rc = regmap_update_bits(wled->regmap, addr,
					WLED_SINK_REG_STR_MOD_MASK,
					WLED_SINK_REG_STR_MOD_EN);
			if (rc < 0)
				return rc;

			addr = wled->sink_addr +
					WLED_SINK_FS_CURR_REG(i);
			rc = regmap_update_bits(wled->regmap, addr,
					WLED_SINK_FS_MASK,
					wled->cfg.fs_current);
			if (rc < 0)
				return rc;

			addr = wled->sink_addr +
					WLED_SINK_CABC_REG(i);
			rc = regmap_update_bits(wled->regmap, addr,
					WLED_SINK_CABC_MASK,
					wled->cfg.en_cabc ?
					WLED_SINK_CABC_EN : 0);
			if (rc)
				return rc;

			temp = i + WLED_SINK_CURR_SINK_SHFT;
			sink_en |= 1 << temp;
		}
	}

	rc = regmap_update_bits(wled->regmap,
			wled->sink_addr + WLED_SINK_CURR_SINK_EN,
			WLED_SINK_CURR_SINK_MASK, sink_en);
	if (rc < 0)
		return rc;

	rc = wled_sync_toggle(wled);
	if (rc < 0) {
		pr_err("Failed to toggle sync reg rc:%d\n", rc);
		return rc;
	}

	rc = wled_auto_calibrate_at_init(wled);
	if (rc < 0)
		return rc;

	if (sc_irq >= 0) {
		rc = devm_request_threaded_irq(&wled->pdev->dev, sc_irq,
				NULL, wled_sc_irq_handler, IRQF_ONESHOT,
				"wled_sc_irq", wled);
		if (rc < 0) {
			pr_err("Unable to request sc(%d) IRQ(err:%d)\n",
				sc_irq, rc);
			return rc;
		}

		rc = regmap_update_bits(wled->regmap,
				wled->ctrl_addr + WLED_CTRL_SHORT_PROTECT,
				WLED_CTRL_SHORT_EN_MASK,
				WLED_CTRL_SHORT_EN_MASK);
		if (rc < 0)
			return rc;
	}

	if (wled->cfg.ext_pfet_sc_pro_en) {
		/* unlock the secure access register */
		rc = regmap_write(wled->regmap, wled->ctrl_addr +
				WLED_CTRL_SEC_ACCESS,
				WLED_CTRL_SEC_UNLOCK);
		if (rc < 0)
			return rc;

		rc = regmap_write(wled->regmap,
				wled->ctrl_addr + WLED_CTRL_TEST1,
				WLED_EXT_FET_DTEST2);
		if (rc < 0)
			return rc;
	}

	if (ovp_irq >= 0) {
		rc = devm_request_threaded_irq(&wled->pdev->dev, ovp_irq,
				NULL, wled_ovp_irq_handler, IRQF_ONESHOT,
				"wled_ovp_irq", wled);
		if (rc < 0) {
			pr_err("Unable to request ovp(%d) IRQ(err:%d)\n",
				ovp_irq, rc);
			return rc;
		}

		rc = regmap_read(wled->regmap, wled->ctrl_addr +
				WLED_CTRL_MOD_ENABLE, &val);
		/* disable the OVP irq only if the module is not enabled */
		if (!rc && !(val & WLED_CTRL_MOD_EN_MASK)) {
			disable_irq(ovp_irq);
			wled->ovp_irq_disabled = true;
		}
	}

	return 0;
}

static const struct wled_config wled_config_defaults = {
	.i_boost_limit = 4,
	.fs_current = 10,
	.ovp = 1,
	.switch_freq = 11,
	.string_cfg = 0xf,
	.en_cabc = 0,
	.ext_pfet_sc_pro_en = 0,
	.auto_calib_enabled = 0,
};

struct wled_var_cfg {
	const u32 *values;
	u32 (*fn)(u32);
	int size;
};

static const u32 wled_i_boost_limit_values[] = {
	105, 280, 450, 620, 970, 1150, 1300, 1500,
};

static const struct wled_var_cfg wled_i_boost_limit_cfg = {
	.values = wled_i_boost_limit_values,
	.size = ARRAY_SIZE(wled_i_boost_limit_values),
};

static const u32 wled_fs_current_values[] = {
	0, 2500, 5000, 7500, 10000, 12500, 15000, 17500, 20000,
	22500, 25000, 27500, 30000,
};

static const struct wled_var_cfg wled_fs_current_cfg = {
	.values = wled_fs_current_values,
	.size = ARRAY_SIZE(wled_fs_current_values),
};

static const u32 wled_ovp_values[] = {
	31100, 29600, 19600, 18100,
};

static const struct wled_var_cfg wled_ovp_cfg = {
	.values = wled_ovp_values,
	.size = ARRAY_SIZE(wled_ovp_values),
};

static u32 wled_switch_freq_values_fn(u32 idx)
{
	return 9600 / (1 + idx);
}

static const struct wled_var_cfg wled_switch_freq_cfg = {
	.fn = wled_switch_freq_values_fn,
	.size = 16,
};

static const struct wled_var_cfg wled_string_cfg = {
	.size = 16,
};

static u32 wled_values(const struct wled_var_cfg *cfg, u32 idx)
{
	if (idx >= cfg->size)
		return UINT_MAX;
	if (cfg->fn)
		return cfg->fn(idx);
	if (cfg->values)
		return cfg->values[idx];
	return idx;
}

static int wled_configure(struct wled *wled, struct device *dev)
{
	struct wled_config *cfg = &wled->cfg;
	const __be32 *prop_addr;
	u32 val, c;
	int rc, i, j;

	const struct {
		const char *name;
		u32 *val_ptr;
		const struct wled_var_cfg *cfg;
	} u32_opts[] = {
		{
			"qcom,current-boost-limit",
			&cfg->i_boost_limit,
			.cfg = &wled_i_boost_limit_cfg,
		},
		{
			"qcom,fs-current-limit",
			&cfg->fs_current,
			.cfg = &wled_fs_current_cfg,
		},
		{
			"qcom,ovp",
			&cfg->ovp,
			.cfg = &wled_ovp_cfg,
		},
		{
			"qcom,switching-freq",
			&cfg->switch_freq,
			.cfg = &wled_switch_freq_cfg,
		},
		{
			"qcom,string-cfg",
			&cfg->string_cfg,
			.cfg = &wled_string_cfg,
		},
	};

	const struct {
		const char *name;
		bool *val_ptr;
	} bool_opts[] = {
		{ "qcom,en-cabc", &cfg->en_cabc, },
		{ "qcom,ext-pfet-sc-pro", &cfg->ext_pfet_sc_pro_en, },
		{ "qcom,auto-calibration", &cfg->auto_calib_enabled, },
	};

	prop_addr = of_get_address(dev->of_node, 0, NULL, NULL);
	if (!prop_addr) {
		pr_err("invalid IO resources\n");
		return -EINVAL;
	}
	wled->ctrl_addr = be32_to_cpu(*prop_addr);

	prop_addr = of_get_address(dev->of_node, 1, NULL, NULL);
	if (!prop_addr) {
		pr_err("invalid IO resources\n");
		return -EINVAL;
	}
	wled->sink_addr = be32_to_cpu(*prop_addr);
	rc = of_property_read_string(dev->of_node, "label", &wled->name);
	if (rc < 0)
		wled->name = dev->of_node->name;

	*cfg = wled_config_defaults;
	for (i = 0; i < ARRAY_SIZE(u32_opts); ++i) {
		rc = of_property_read_u32(dev->of_node, u32_opts[i].name, &val);
		if (rc == -EINVAL) {
			continue;
		} else if (rc < 0) {
			pr_err("error reading '%s'\n", u32_opts[i].name);
			return rc;
		}

		c = UINT_MAX;
		for (j = 0; c != val; j++) {
			c = wled_values(u32_opts[i].cfg, j);
			if (c == UINT_MAX) {
				pr_err("invalid value for '%s'\n",
					u32_opts[i].name);
				return -EINVAL;
			}

			if (c == val)
				break;
		}

		pr_debug("'%s' = %u\n", u32_opts[i].name, c);
		*u32_opts[i].val_ptr = j;
	}

	for (i = 0; i < ARRAY_SIZE(bool_opts); ++i) {
		if (of_property_read_bool(dev->of_node, bool_opts[i].name))
			*bool_opts[i].val_ptr = true;
	}

	wled->cfg.sc_irq = platform_get_irq_byname(wled->pdev, "sc-irq");
	if (wled->cfg.sc_irq < 0)
		dev_dbg(&wled->pdev->dev, "sc irq is not used\n");

	wled->cfg.ovp_irq = platform_get_irq_byname(wled->pdev, "ovp-irq");
	if (wled->cfg.ovp_irq < 0)
		dev_dbg(&wled->pdev->dev, "ovp irq is not used\n");

	return 0;
}

static const struct backlight_ops wled_ops = {
	.update_status = wled_update_status,
	.get_brightness = wled_get_brightness,
};

static int wled_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct backlight_device *bl;
	struct wled *wled;
	struct regmap *regmap;
	u32 val;
	int rc;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		pr_err("Unable to get regmap\n");
		return -EINVAL;
	}

	wled = devm_kzalloc(&pdev->dev, sizeof(*wled), GFP_KERNEL);
	if (!wled)
		return -ENOMEM;

	wled->regmap = regmap;
	wled->pdev = pdev;

	rc = wled_configure(wled, &pdev->dev);
	if (rc < 0) {
		pr_err("wled configure failed rc:%d\n", rc);
		return rc;
	}

	rc = wled_setup(wled);
	if (rc < 0) {
		pr_err("wled setup failed rc:%d\n", rc);
		return rc;
	}

	mutex_init(&wled->lock);
	val = WLED_DEFAULT_BRIGHTNESS;
	of_property_read_u32(pdev->dev.of_node, "default-brightness", &val);
	wled->brightness = val;

	platform_set_drvdata(pdev, wled);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = val;
	props.max_brightness = WLED_MAX_BRIGHTNESS;
	bl = devm_backlight_device_register(&pdev->dev, pdev->name,
					    &pdev->dev, wled,
					    &wled_ops, &props);
	return PTR_ERR_OR_ZERO(bl);
}

static const struct of_device_id wled_match_table[] = {
	{ .compatible = "qcom,pmi8998-spmi-wled",},
	{ },
};

static struct platform_driver wled_driver = {
	.probe = wled_probe,
	.driver	= {
		.name = "qcom-spmi-wled",
		.of_match_table	= wled_match_table,
	},
};

module_platform_driver(wled_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SPMI PMIC WLED driver");
MODULE_LICENSE("GPL v2");

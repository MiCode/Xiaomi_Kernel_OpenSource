/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/regmap.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/leds-qpnp-wled.h>
#include <linux/qpnp/qpnp-revid.h>

#define QPNP_IRQ_FLAGS	(IRQF_TRIGGER_RISING | \
			IRQF_TRIGGER_FALLING | \
			IRQF_ONESHOT)

/* base addresses */
#define QPNP_WLED_CTRL_BASE		"qpnp-wled-ctrl-base"
#define QPNP_WLED_SINK_BASE		"qpnp-wled-sink-base"

/* ctrl registers */
#define QPNP_WLED_INT_EN_SET(b)		(b + 0x15)
#define QPNP_WLED_INT_EN_CLR(b)		(b + 0x16)
#define QPNP_WLED_EN_REG(b)		(b + 0x46)
#define QPNP_WLED_FDBK_OP_REG(b)	(b + 0x48)
#define QPNP_WLED_VREF_REG(b)		(b + 0x49)
#define QPNP_WLED_BOOST_DUTY_REG(b)	(b + 0x4B)
#define QPNP_WLED_SWITCH_FREQ_REG(b)	(b + 0x4C)
#define QPNP_WLED_OVP_REG(b)		(b + 0x4D)
#define QPNP_WLED_ILIM_REG(b)		(b + 0x4E)
#define QPNP_WLED_SOFTSTART_RAMP_DLY(b) (b + 0x53)
#define QPNP_WLED_VLOOP_COMP_RES_REG(b)	(b + 0x55)
#define QPNP_WLED_VLOOP_COMP_GM_REG(b)	(b + 0x56)
#define QPNP_WLED_PSM_CTRL_REG(b)	(b + 0x5B)
#define QPNP_WLED_SC_PRO_REG(b)		(b + 0x5E)
#define QPNP_WLED_CTRL_SPARE_REG(b)	(b + 0xDF)
#define QPNP_WLED_TEST1_REG(b)		(b + 0xE2)
#define QPNP_WLED_TEST4_REG(b)		(b + 0xE5)
#define QPNP_WLED_REF_7P7_TRIM_REG(b)	(b + 0xF2)

#define QPNP_WLED_7P7_TRIM_MASK		GENMASK(3, 0)
#define QPNP_WLED_EN_MASK		0x7F
#define QPNP_WLED_EN_SHIFT		7
#define QPNP_WLED_FDBK_OP_MASK		0xF8
#define QPNP_WLED_VREF_MASK		0xF0
#define QPNP_WLED_VREF_STEP_MV		25
#define QPNP_WLED_VREF_MIN_MV		300
#define QPNP_WLED_VREF_MAX_MV		675
#define QPNP_WLED_DFLT_VREF_MV		350

#define QPNP_WLED_VLOOP_COMP_RES_MASK			0xF0
#define QPNP_WLED_VLOOP_COMP_RES_OVERWRITE		0x80
#define QPNP_WLED_LOOP_COMP_RES_DFLT_AMOLED_KOHM	320
#define QPNP_WLED_LOOP_COMP_RES_STEP_KOHM		20
#define QPNP_WLED_LOOP_COMP_RES_MIN_KOHM		20
#define QPNP_WLED_LOOP_COMP_RES_MAX_KOHM		320
#define QPNP_WLED_VLOOP_COMP_GM_MASK			0xF0
#define QPNP_WLED_VLOOP_COMP_GM_OVERWRITE		0x80
#define QPNP_WLED_LOOP_EA_GM_DFLT_AMOLED		0x03
#define QPNP_WLED_LOOP_EA_GM_MIN			0x0
#define QPNP_WLED_LOOP_EA_GM_MAX			0xF
#define QPNP_WLED_VREF_PSM_MASK				0xF8
#define QPNP_WLED_VREF_PSM_STEP_MV			50
#define QPNP_WLED_VREF_PSM_MIN_MV			400
#define QPNP_WLED_VREF_PSM_MAX_MV			750
#define QPNP_WLED_VREF_PSM_DFLT_AMOLED_MV		450
#define QPNP_WLED_PSM_CTRL_OVERWRITE			0x80

#define QPNP_WLED_ILIM_MASK		0xF8
#define QPNP_WLED_ILIM_MIN_MA		105
#define QPNP_WLED_ILIM_MAX_MA		1980
#define QPNP_WLED_ILIM_STEP_MA		280
#define QPNP_WLED_DFLT_ILIM_MA		980
#define QPNP_WLED_ILIM_OVERWRITE	0x80
#define QPNP_WLED_BOOST_DUTY_MASK	0xFC
#define QPNP_WLED_BOOST_DUTY_STEP_NS	52
#define QPNP_WLED_BOOST_DUTY_MIN_NS	26
#define QPNP_WLED_BOOST_DUTY_MAX_NS	156
#define QPNP_WLED_DEF_BOOST_DUTY_NS	104
#define QPNP_WLED_SWITCH_FREQ_MASK	0x70
#define QPNP_WLED_SWITCH_FREQ_800_KHZ	800
#define QPNP_WLED_SWITCH_FREQ_1600_KHZ	1600
#define QPNP_WLED_SWITCH_FREQ_OVERWRITE 0x80
#define QPNP_WLED_OVP_MASK		GENMASK(1, 0)
#define QPNP_WLED_TEST4_EN_VREF_UP	0x32
#define QPNP_WLED_INT_EN_SET_OVP_EN	0x02
#define QPNP_WLED_OVP_FLT_SLEEP_US	10
#define QPNP_WLED_TEST4_EN_IIND_UP	0x1

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
#define QPNP_WLED_SINK_TEST5_REG(b)	(b + 0xE6)

#define QPNP_WLED_MOD_FREQ_1200_KHZ	1200
#define QPNP_WLED_MOD_FREQ_2400_KHZ	2400
#define QPNP_WLED_MOD_FREQ_9600_KHZ	9600
#define QPNP_WLED_MOD_FREQ_19200_KHZ	19200
#define QPNP_WLED_MOD_FREQ_MASK		0x3F
#define QPNP_WLED_MOD_FREQ_SHIFT	6
#define QPNP_WLED_ACC_CLK_FREQ_MASK	0xE7
#define QPNP_WLED_ACC_CLK_FREQ_SHIFT	3
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
#define QPNP_WLED_GATE_DRV_MASK		0xFE
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

#define QPNP_WLED_SINK_TEST5_HYB	0x14
#define QPNP_WLED_SINK_TEST5_DIG	0x1E

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
#define QPNP_WLED_EN_SC_DEB_CYCLES_MASK	0x79
#define QPNP_WLED_EN_DEB_CYCLES_MASK	0xF9
#define QPNP_WLED_EN_SC_SHIFT		7
#define QPNP_WLED_SC_PRO_EN_DSCHGR	0x8
#define QPNP_WLED_SC_DEB_CYCLES_MIN     2
#define QPNP_WLED_SC_DEB_CYCLES_MAX     16
#define QPNP_WLED_SC_DEB_CYCLES_SUB     2
#define QPNP_WLED_SC_DEB_CYCLES_DFLT    4
#define QPNP_WLED_EXT_FET_DTEST2	0x09

#define QPNP_WLED_SEC_ACCESS_REG(b)    (b + 0xD0)
#define QPNP_WLED_SEC_UNLOCK           0xA5

#define QPNP_WLED_MAX_STRINGS		4
#define WLED_MAX_LEVEL_4095		4095
#define QPNP_WLED_RAMP_DLY_MS		20
#define QPNP_WLED_TRIGGER_NONE		"none"
#define QPNP_WLED_STR_SIZE		20
#define QPNP_WLED_MIN_MSLEEP		20
#define QPNP_WLED_SC_DLY_MS		20

#define NUM_SUPPORTED_AVDD_VOLTAGES	6
#define QPNP_WLED_DFLT_AVDD_MV		7600
#define QPNP_WLED_AVDD_MIN_TRIM_VAL	0x0
#define QPNP_WLED_AVDD_MAX_TRIM_VAL	0xF
#define QPNP_WLED_AVDD_SET_BIT		BIT(4)

#define NUM_SUPPORTED_OVP_THRESHOLDS	4

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

/* wled ctrl debug registers */
static u8 qpnp_wled_ctrl_dbg_regs[] = {
	0x44, 0x46, 0x48, 0x49, 0x4b, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57, 0x58, 0x5a, 0x5b, 0x5d, 0x5e, 0xe2
};

/* wled sink debug registers */
static u8 qpnp_wled_sink_dbg_regs[] = {
	0x46, 0x47, 0x48, 0x4a, 0x4b,
	0x50, 0x51, 0x52, 0x53,	0x56, 0x57, 0x58,
	0x60, 0x61, 0x62, 0x63,	0x66, 0x67, 0x68,
	0x70, 0x71, 0x72, 0x73,	0x76, 0x77, 0x78,
	0x80, 0x81, 0x82, 0x83,	0x86, 0x87, 0x88,
	0xe6,
};

static int qpnp_wled_avdd_target_voltages[NUM_SUPPORTED_AVDD_VOLTAGES] = {
	7900, 7600, 7300, 6400, 6100, 5800,
};

static u8 qpnp_wled_ovp_reg_settings[NUM_SUPPORTED_AVDD_VOLTAGES] = {
	0x0, 0x0, 0x1, 0x2, 0x2, 0x3,
};

static int qpnp_wled_avdd_trim_adjustments[NUM_SUPPORTED_AVDD_VOLTAGES] = {
	3, 0, -2, 7, 3, 3,
};

static int qpnp_wled_ovp_thresholds_pmi8994[NUM_SUPPORTED_OVP_THRESHOLDS] = {
	31000, 29500, 19400, 17800,
};

static int qpnp_wled_ovp_thresholds_pmicobalt[NUM_SUPPORTED_OVP_THRESHOLDS] = {
	31100, 29600, 19600, 18100,
};

/**
 *  qpnp_wled - wed data structure
 *  @ cdev - led class device
 *  @ pdev - platform device
 *  @ work - worker for led operation
 *  @ lock - mutex lock for exclusive access
 *  @ fdbk_op - output feedback mode
 *  @ dim_mode - dimming mode
 *  @ ovp_irq - over voltage protection irq
 *  @ sc_irq - short circuit irq
 *  @ sc_cnt - short circuit irq count
 *  @ avdd_target_voltage_mv - target voltage for AVDD module in mV
 *  @ ctrl_base - base address for wled ctrl
 *  @ sink_base - base address for wled sink
 *  @ ibb_base - base address for IBB(Inverting Buck Boost)
 *  @ lab_base - base address for LAB(LCD/AMOLED Boost)
 *  @ mod_freq_khz - modulator frequency in KHZ
 *  @ hyb_thres - threshold for hybrid dimming
 *  @ sync_dly_us - sync delay in us
 *  @ vref_mv - ref voltage in mv
 *  @ vref_psm_mv - ref psm voltage in mv
 *  @ loop_comp_res_kohm - control to select the compensation resistor
 *  @ loop_ea_gm - control to select the gm for the gm stage in control loop
 *  @ sc_deb_cycles - debounce time for short circuit detection
 *  @ switch_freq_khz - switching frequency in KHZ
 *  @ ovp_mv - over voltage protection in mv
 *  @ ilim_ma - current limiter in ma
 *  @ boost_duty_ns - boost duty cycle in ns
 *  @ fs_curr_ua - full scale current in ua
 *  @ ramp_ms - delay between ramp steps in ms
 *  @ ramp_step - ramp step size
 *  @ cons_sync_write_delay_us - delay between two consecutive writes to SYNC
 *  @ strings - supported list of strings
 *  @ num_strings - number of strings
 *  @ en_9b_dim_res - enable or disable 9bit dimming
 *  @ en_phase_stag - enable or disable phase staggering
 *  @ en_cabc - enable or disable cabc
 *  @ disp_type_amoled - type of display: LCD/AMOLED
 *  @ ibb_bias_active - activate display bias
 *  @ lab_fast_precharge - fast/slow precharge
 *  @ en_ext_pfet_sc_pro - enable sc protection on external pfet
 */
struct qpnp_wled {
	struct led_classdev	cdev;
	struct platform_device	*pdev;
	struct regmap		*regmap;
	struct pmic_revid_data	*pmic_rev_id;
	struct work_struct	work;
	struct mutex		lock;
	struct mutex		bus_lock;
	enum qpnp_wled_fdbk_op	fdbk_op;
	enum qpnp_wled_dim_mode	dim_mode;
	int			ovp_irq;
	int			sc_irq;
	u32			sc_cnt;
	u32			avdd_target_voltage_mv;
	u16			ctrl_base;
	u16			sink_base;
	u16			mod_freq_khz;
	u16			hyb_thres;
	u16			sync_dly_us;
	u16			vref_mv;
	u16			vref_psm_mv;
	u16			loop_comp_res_kohm;
	u16			loop_ea_gm;
	u16			sc_deb_cycles;
	u16			switch_freq_khz;
	u16			ovp_mv;
	u16			ilim_ma;
	u16			boost_duty_ns;
	u16			fs_curr_ua;
	u16			ramp_ms;
	u16			ramp_step;
	u16			cons_sync_write_delay_us;
	u8			strings[QPNP_WLED_MAX_STRINGS];
	u8			num_strings;
	bool			en_9b_dim_res;
	bool			en_phase_stag;
	bool			en_cabc;
	bool			disp_type_amoled;
	bool			en_ext_pfet_sc_pro;
	bool			prev_state;
};

/* helper to read a pmic register */
static int qpnp_wled_read_reg(struct qpnp_wled *wled, u8 *data, u16 addr)
{
	int rc;
	uint val;

	rc = regmap_read(wled->regmap, addr, &val);
	if (rc < 0) {
		dev_err(&wled->pdev->dev,
			"Error reading address: %x(%d)\n", addr, rc);
		return rc;
	}

	*data = (u8)val;
	return 0;
}

/* helper to write a pmic register */
static int qpnp_wled_write_reg(struct qpnp_wled *wled, u8 data, u16 addr)
{
	int rc;

	mutex_lock(&wled->bus_lock);
	rc = regmap_write(wled->regmap, addr, data);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Error writing address: %x(%d)\n",
			addr, rc);
		goto out;
	}

	dev_dbg(&wled->pdev->dev, "wrote: WLED_0x%x = 0x%x\n", addr, data);
out:
	mutex_unlock(&wled->bus_lock);
	return rc;
}

static int qpnp_wled_masked_write_reg(struct qpnp_wled *wled, u8 mask, u8 *data,
					u16 addr)
{
	u8 reg;
	int rc;

	rc = qpnp_wled_read_reg(wled, &reg, addr);
	if (rc < 0)
		return rc;

	reg &= ~mask;
	reg |= *data & mask;

	rc = qpnp_wled_write_reg(wled, reg, addr);

	return rc;
}

static int qpnp_wled_sec_write_reg(struct qpnp_wled *wled, u8 data, u16 addr)
{
	int rc;
	u8 reg = QPNP_WLED_SEC_UNLOCK;
	u16 base_addr = addr & 0xFF00;

	mutex_lock(&wled->bus_lock);
	rc = regmap_write(wled->regmap, QPNP_WLED_SEC_ACCESS_REG(base_addr),
			reg);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Error writing address: %x(%d)\n",
			QPNP_WLED_SEC_ACCESS_REG(base_addr), rc);
		goto out;
	}

	rc = regmap_write(wled->regmap, addr, data);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Error writing address: %x(%d)\n",
			addr, rc);
		goto out;
	}

	dev_dbg(&wled->pdev->dev, "wrote: WLED_0x%x = 0x%x\n", addr, data);
out:
	mutex_unlock(&wled->bus_lock);
	return rc;
}

static int qpnp_wled_sync_reg_toggle(struct qpnp_wled *wled)
{
	int rc;
	u8 reg;

	/* sync */
	reg = QPNP_WLED_SYNC;
	rc = qpnp_wled_write_reg(wled, reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
		return rc;

	if (wled->cons_sync_write_delay_us)
		usleep_range(wled->cons_sync_write_delay_us,
				wled->cons_sync_write_delay_us + 1);

	reg = QPNP_WLED_SYNC_RESET;
	rc = qpnp_wled_write_reg(wled, reg,
		QPNP_WLED_SYNC_REG(wled->sink_base));
	if (rc < 0)
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
		rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_BRIGHT_LSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;

		reg = level >> QPNP_WLED_BRIGHT_MSB_SHIFT;
		reg = reg & QPNP_WLED_BRIGHT_MSB_MASK;
		rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_BRIGHT_MSB_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
	}

	rc = qpnp_wled_sync_reg_toggle(wled);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Failed to toggle sync reg %d\n", rc);
		return rc;
	}

	return 0;
}

static int qpnp_wled_module_en(struct qpnp_wled *wled,
				u16 base_addr, bool state)
{
	int rc;
	u8 reg;

	/* disable OVP fault interrupt */
	if (state) {
		reg = QPNP_WLED_INT_EN_SET_OVP_EN;
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_INT_EN_CLR(base_addr));
		if (rc)
			return rc;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MODULE_EN_MASK;
	reg |= (state << QPNP_WLED_MODULE_EN_SHIFT);
	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_MODULE_EN_REG(base_addr));
	if (rc)
		return rc;

	/* enable OVP fault interrupt */
	if (state && (wled->ovp_irq > 0)) {
		udelay(QPNP_WLED_OVP_FLT_SLEEP_US);
		reg = QPNP_WLED_INT_EN_SET_OVP_EN;
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_INT_EN_SET(base_addr));
		if (rc)
			return rc;
	}

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
			dev_err(&wled->pdev->dev, "wled enable failed\n");
			goto unlock_mutex;
		}
	}

	/* ramp up */
	for (i = 0; i <= wled->cdev.max_brightness;) {
		rc = qpnp_wled_set_level(wled, i);
		if (rc) {
			dev_err(&wled->pdev->dev, "wled set level failed\n");
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
			dev_err(&wled->pdev->dev, "wled set level failed\n");
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

	dev_info(&wled->pdev->dev, "wled ramp complete\n");

restore_brightness:
	/* restore the old brightness */
	qpnp_wled_set_level(wled, wled->cdev.brightness);
	if (!wled->cdev.brightness) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, false);
		if (rc)
			dev_err(&wled->pdev->dev, "wled enable failed\n");
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

	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc)
		return rc;

	wled->dim_mode = temp;

	return count;
}

/* sysfs show function for full scale current in ua*/
static ssize_t qpnp_wled_fs_curr_ua_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qpnp_wled *wled = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wled->fs_curr_ua);
}

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
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
							wled->strings[i]));
		if (rc)
			return rc;
	}

	wled->fs_curr_ua = data;

	rc = qpnp_wled_sync_reg_toggle(wled);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Failed to toggle sync reg %d\n", rc);
		return rc;
	}

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
			dev_err(&wled->pdev->dev, "wled set level failed\n");
			goto unlock_mutex;
		}
	}

	if (!!level != wled->prev_state) {
		rc = qpnp_wled_module_en(wled, wled->ctrl_base, !!level);

		if (rc) {
			dev_err(&wled->pdev->dev, "wled %sable failed\n",
						level ? "en" : "dis");
			goto unlock_mutex;
		}
	}

	wled->prev_state = !!level;
unlock_mutex:
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

	rc = qpnp_wled_sec_write_reg(wled, reg,
			QPNP_WLED_DISP_SEL_REG(base_addr));
	if (rc)
		return rc;

	if (wled->disp_type_amoled) {
		/* Configure the PSM CTRL register for AMOLED */
		if (wled->vref_psm_mv < QPNP_WLED_VREF_PSM_MIN_MV)
			wled->vref_psm_mv = QPNP_WLED_VREF_PSM_MIN_MV;
		else if (wled->vref_psm_mv > QPNP_WLED_VREF_PSM_MAX_MV)
			wled->vref_psm_mv = QPNP_WLED_VREF_PSM_MAX_MV;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_PSM_CTRL_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VREF_PSM_MASK;
		reg |= ((wled->vref_psm_mv - QPNP_WLED_VREF_PSM_MIN_MV)/
			QPNP_WLED_VREF_PSM_STEP_MV);
		reg |= QPNP_WLED_PSM_CTRL_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_PSM_CTRL_REG(wled->ctrl_base));
		if (rc)
			return rc;

		/* Configure the VLOOP COMP RES register for AMOLED */
		if (wled->loop_comp_res_kohm < QPNP_WLED_LOOP_COMP_RES_MIN_KOHM)
			wled->loop_comp_res_kohm =
					QPNP_WLED_LOOP_COMP_RES_MIN_KOHM;
		else if (wled->loop_comp_res_kohm >
					QPNP_WLED_LOOP_COMP_RES_MAX_KOHM)
			wled->loop_comp_res_kohm =
					QPNP_WLED_LOOP_COMP_RES_MAX_KOHM;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_RES_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VLOOP_COMP_RES_MASK;
		reg |= ((wled->loop_comp_res_kohm -
				 QPNP_WLED_LOOP_COMP_RES_MIN_KOHM)/
				 QPNP_WLED_LOOP_COMP_RES_STEP_KOHM);
		reg |= QPNP_WLED_VLOOP_COMP_RES_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_VLOOP_COMP_RES_REG(wled->ctrl_base));
		if (rc)
			return rc;

		/* Configure the LOOP COMP GM register for AMOLED */
		if (wled->loop_ea_gm < QPNP_WLED_LOOP_EA_GM_MIN)
			wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_MIN;
		else if (wled->loop_ea_gm > QPNP_WLED_LOOP_EA_GM_MAX)
			wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_MAX;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_VLOOP_COMP_GM_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg &= QPNP_WLED_VLOOP_COMP_GM_MASK;
		reg |= (wled->loop_ea_gm | QPNP_WLED_VLOOP_COMP_GM_OVERWRITE);
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_VLOOP_COMP_GM_REG(wled->ctrl_base));
		if (rc)
			return rc;

		/* Configure the CTRL TEST4 register for AMOLED */
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_TEST4_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;

		reg |= QPNP_WLED_TEST4_EN_IIND_UP;
		rc = qpnp_wled_sec_write_reg(wled, reg,
				QPNP_WLED_TEST4_REG(base_addr));
		if (rc)
			return rc;
	} else {
		/*
		 * enable VREF_UP to avoid false ovp on low brightness for LCD
		 */
		reg = QPNP_WLED_TEST4_EN_VREF_UP;
		rc = qpnp_wled_sec_write_reg(wled, reg,
				QPNP_WLED_TEST4_REG(base_addr));
		if (rc)
			return rc;
	}

	return 0;
}

/* ovp irq handler */
static irqreturn_t qpnp_wled_ovp_irq(int irq, void *_wled)
{
	struct qpnp_wled *wled = _wled;

	dev_dbg(&wled->pdev->dev, "ovp detected\n");

	return IRQ_HANDLED;
}

/* short circuit irq handler */
static irqreturn_t qpnp_wled_sc_irq(int irq, void *_wled)
{
	struct qpnp_wled *wled = _wled;

	dev_err(&wled->pdev->dev,
			"Short circuit detected %d times\n", ++wled->sc_cnt);

	qpnp_wled_module_en(wled, wled->ctrl_base, false);
	msleep(QPNP_WLED_SC_DLY_MS);
	qpnp_wled_module_en(wled, wled->ctrl_base, true);

	return IRQ_HANDLED;
}

static bool is_avdd_trim_adjustment_required(struct qpnp_wled *wled)
{
	int rc;
	u8 reg = 0;

	/*
	 * AVDD trim adjustment is not required for pmicobalt/pm2falcon and not
	 * supported for pmi8994.
	 */
	if (wled->pmic_rev_id->pmic_subtype == PMICOBALT_SUBTYPE ||
		wled->pmic_rev_id->pmic_subtype == PM2FALCON_SUBTYPE ||
		wled->pmic_rev_id->pmic_subtype == PMI8994_SUBTYPE)
		return false;

	/*
	 * Configure TRIM_REG only if disp_type_amoled and it has
	 * not already been programmed by bootloader.
	 */
	if (!wled->disp_type_amoled)
		return false;

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_CTRL_SPARE_REG(wled->ctrl_base));
	if (rc < 0)
		return false;

	return !(reg & QPNP_WLED_AVDD_SET_BIT);
}

static int qpnp_wled_ovp_config(struct qpnp_wled *wled)
{
	int rc, i, *ovp_table;
	u8 reg;

	/*
	 * Configure the OVP register based on ovp_mv only if display type is
	 * not AMOLED.
	 */
	if (wled->disp_type_amoled)
		return 0;

	if (wled->pmic_rev_id->pmic_subtype == PMICOBALT_SUBTYPE ||
		wled->pmic_rev_id->pmic_subtype == PM2FALCON_SUBTYPE)
		ovp_table = qpnp_wled_ovp_thresholds_pmicobalt;
	else
		ovp_table = qpnp_wled_ovp_thresholds_pmi8994;

	for (i = 0; i < NUM_SUPPORTED_OVP_THRESHOLDS; i++) {
		if (wled->ovp_mv == ovp_table[i])
			break;
	}

	if (i == NUM_SUPPORTED_OVP_THRESHOLDS) {
		dev_err(&wled->pdev->dev,
			"Invalid ovp threshold specified in device tree\n");
		return -EINVAL;
	}

	reg = i & QPNP_WLED_OVP_MASK;
	rc = qpnp_wled_masked_write_reg(wled, QPNP_WLED_OVP_MASK, &reg,
			QPNP_WLED_OVP_REG(wled->ctrl_base));
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
	rc = qpnp_wled_write_reg(wled, reg,
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
	rc = qpnp_wled_write_reg(wled, reg,
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
	temp = (wled->ilim_ma / QPNP_WLED_ILIM_STEP_MA);
	if (temp != (reg & ~QPNP_WLED_ILIM_MASK)) {
		reg &= QPNP_WLED_ILIM_MASK;
		reg |= temp;
		reg |= QPNP_WLED_ILIM_OVERWRITE;
		rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_ILIM_REG(wled->ctrl_base));
		if (rc)
			return rc;
	}

	/* Configure the Soft start Ramp delay: for AMOLED - 0,for LCD - 2 */
	reg = (wled->disp_type_amoled) ? 0 : 2;
	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_SOFTSTART_RAMP_DLY(wled->ctrl_base));
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
	rc = qpnp_wled_write_reg(wled, reg,
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
	reg |= (temp | QPNP_WLED_SWITCH_FREQ_OVERWRITE);
	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_SWITCH_FREQ_REG(wled->ctrl_base));
	if (rc)
		return rc;

	rc = qpnp_wled_ovp_config(wled);
	if (rc < 0) {
		pr_err("Error in configuring OVP threshold, rc=%d\n", rc);
		return rc;
	}

	if (is_avdd_trim_adjustment_required(wled)) {
		for (i = 0; i < NUM_SUPPORTED_AVDD_VOLTAGES; i++) {
			if (wled->avdd_target_voltage_mv ==
					qpnp_wled_avdd_target_voltages[i])
				break;
		}

		if (i == NUM_SUPPORTED_AVDD_VOLTAGES) {
			dev_err(&wled->pdev->dev,
				"Invalid avdd target voltage specified in device tree\n");
			return -EINVAL;
		}

		/* Update WLED_OVP register based on desired target voltage */
		reg = qpnp_wled_ovp_reg_settings[i];
		rc = qpnp_wled_masked_write_reg(wled, QPNP_WLED_OVP_MASK, &reg,
				QPNP_WLED_OVP_REG(wled->ctrl_base));
		if (rc)
			return rc;

		/* Update WLED_TRIM register based on desired target voltage */
		rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_REF_7P7_TRIM_REG(wled->ctrl_base));
		if (rc)
			return rc;

		reg += qpnp_wled_avdd_trim_adjustments[i];
		if ((s8)reg < QPNP_WLED_AVDD_MIN_TRIM_VAL ||
				(s8)reg > QPNP_WLED_AVDD_MAX_TRIM_VAL) {
			dev_info(&wled->pdev->dev,
				 "adjusted trim %d is not within range, capping it\n",
				 (s8)reg);
			if ((s8)reg < QPNP_WLED_AVDD_MIN_TRIM_VAL)
				reg = QPNP_WLED_AVDD_MIN_TRIM_VAL;
			else
				reg = QPNP_WLED_AVDD_MAX_TRIM_VAL;
		}

		reg &= QPNP_WLED_7P7_TRIM_MASK;
		rc = qpnp_wled_sec_write_reg(wled, reg,
				QPNP_WLED_REF_7P7_TRIM_REG(wled->ctrl_base));
		if (rc)
			return rc;
	}

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
	} else if (wled->mod_freq_khz <= QPNP_WLED_MOD_FREQ_19200_KHZ) {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_19200_KHZ;
		temp = 0;
	} else {
		wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
		temp = 1;
	}

	rc = qpnp_wled_read_reg(wled, &reg,
			QPNP_WLED_MOD_REG(wled->sink_base));
	if (rc < 0)
		return rc;
	reg &= QPNP_WLED_MOD_FREQ_MASK;
	reg |= (temp << QPNP_WLED_MOD_FREQ_SHIFT);

	reg &= QPNP_WLED_PHASE_STAG_MASK;
	reg |= (wled->en_phase_stag << QPNP_WLED_PHASE_STAG_SHIFT);

	reg &= QPNP_WLED_ACC_CLK_FREQ_MASK;
	reg |= (temp << QPNP_WLED_ACC_CLK_FREQ_SHIFT);

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

	rc = qpnp_wled_write_reg(wled, reg,
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
	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_HYB_THRES_REG(wled->sink_base));
	if (rc)
		return rc;

	/* Configure TEST5 register */
	if (wled->dim_mode == QPNP_WLED_DIM_DIGITAL)
		reg = QPNP_WLED_SINK_TEST5_DIG;
	else
		reg = QPNP_WLED_SINK_TEST5_HYB;

	rc = qpnp_wled_sec_write_reg(wled, reg,
			QPNP_WLED_SINK_TEST5_REG(wled->sink_base));
	if (rc)
		return rc;

	/* disable all current sinks and enable selected strings */
	reg = 0x00;
	rc = qpnp_wled_write_reg(wled, reg,
			QPNP_WLED_CURR_SINK_REG(wled->sink_base));

	for (i = 0; i < wled->num_strings; i++) {
		if (wled->strings[i] >= QPNP_WLED_MAX_STRINGS) {
			dev_err(&wled->pdev->dev, "Invalid string number\n");
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

		if (wled->dim_mode == QPNP_WLED_DIM_HYBRID)
			reg &= QPNP_WLED_GATE_DRV_MASK;
		else
			reg |= ~QPNP_WLED_GATE_DRV_MASK;

		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_MOD_EN_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* SYNC DELAY */
		if (wled->sync_dly_us > QPNP_WLED_SYNC_DLY_MAX_US)
			wled->sync_dly_us = QPNP_WLED_SYNC_DLY_MAX_US;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_SYNC_DLY_MASK;
		temp = wled->sync_dly_us / QPNP_WLED_SYNC_DLY_STEP_US;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_SYNC_DLY_REG(wled->sink_base,
						wled->strings[i]));
		if (rc)
			return rc;

		/* FULL SCALE CURRENT */
		if (wled->fs_curr_ua > QPNP_WLED_FS_CURR_MAX_UA)
			wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_FS_CURR_REG(wled->sink_base,
						wled->strings[i]));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_FS_CURR_MASK;
		temp = wled->fs_curr_ua / QPNP_WLED_FS_CURR_STEP_UA;
		reg |= temp;
		rc = qpnp_wled_write_reg(wled, reg,
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
		rc = qpnp_wled_write_reg(wled, reg,
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
		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_CURR_SINK_REG(wled->sink_base));
		if (rc)
			return rc;
	}

	rc = qpnp_wled_sync_reg_toggle(wled);
	if (rc < 0) {
		dev_err(&wled->pdev->dev, "Failed to toggle sync reg %d\n", rc);
		return rc;
	}

	/* setup ovp and sc irqs */
	if (wled->ovp_irq >= 0) {
		rc = devm_request_threaded_irq(&wled->pdev->dev,
					       wled->ovp_irq,
			NULL, qpnp_wled_ovp_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_wled_ovp_irq", wled);
		if (rc < 0) {
			dev_err(&wled->pdev->dev,
				"Unable to request ovp(%d) IRQ(err:%d)\n",
				wled->ovp_irq, rc);
			return rc;
		}
	}

	if (wled->sc_irq >= 0) {
		wled->sc_cnt = 0;
		rc = devm_request_threaded_irq(&wled->pdev->dev, wled->sc_irq,
			NULL, qpnp_wled_sc_irq,
			QPNP_IRQ_FLAGS,
			"qpnp_wled_sc_irq", wled);
		if (rc < 0) {
			dev_err(&wled->pdev->dev,
				"Unable to request sc(%d) IRQ(err:%d)\n",
				wled->sc_irq, rc);
			return rc;
		}

		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_EN_SC_DEB_CYCLES_MASK;
		reg |= 1 << QPNP_WLED_EN_SC_SHIFT;

		if (wled->sc_deb_cycles < QPNP_WLED_SC_DEB_CYCLES_MIN)
			wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_MIN;
		else if (wled->sc_deb_cycles > QPNP_WLED_SC_DEB_CYCLES_MAX)
			wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_MAX;
		temp = fls(wled->sc_deb_cycles) - QPNP_WLED_SC_DEB_CYCLES_SUB;
		reg |= (temp << 1);

		if (wled->disp_type_amoled)
			reg |= QPNP_WLED_SC_PRO_EN_DSCHGR;

		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc)
			return rc;

		if (wled->en_ext_pfet_sc_pro) {
			reg = QPNP_WLED_EXT_FET_DTEST2;
			rc = qpnp_wled_sec_write_reg(wled, reg,
					QPNP_WLED_TEST1_REG(wled->ctrl_base));
			if (rc)
				return rc;
		}
	} else {
		rc = qpnp_wled_read_reg(wled, &reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc < 0)
			return rc;
		reg &= QPNP_WLED_EN_DEB_CYCLES_MASK;

		if (wled->sc_deb_cycles < QPNP_WLED_SC_DEB_CYCLES_MIN)
			wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_MIN;
		else if (wled->sc_deb_cycles > QPNP_WLED_SC_DEB_CYCLES_MAX)
			wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_MAX;
		temp = fls(wled->sc_deb_cycles) - QPNP_WLED_SC_DEB_CYCLES_SUB;
		reg |= (temp << 1);

		rc = qpnp_wled_write_reg(wled, reg,
				QPNP_WLED_SC_PRO_REG(wled->ctrl_base));
		if (rc)
			return rc;
	}

	return 0;
}

/* parse wled dtsi parameters */
static int qpnp_wled_parse_dt(struct qpnp_wled *wled)
{
	struct platform_device *pdev = wled->pdev;
	struct property *prop;
	const char *temp_str;
	u32 temp_val;
	int rc, i;
	u8 *strings;

	wled->cdev.name = "wled";
	rc = of_property_read_string(pdev->dev.of_node,
			"linux,name", &wled->cdev.name);
	if (rc && (rc != -EINVAL)) {
		dev_err(&pdev->dev, "Unable to read led name\n");
		return rc;
	}

	wled->cdev.default_trigger = QPNP_WLED_TRIGGER_NONE;
	rc = of_property_read_string(pdev->dev.of_node, "linux,default-trigger",
					&wled->cdev.default_trigger);
	if (rc && (rc != -EINVAL)) {
		dev_err(&pdev->dev, "Unable to read led trigger\n");
		return rc;
	}

	wled->disp_type_amoled = of_property_read_bool(pdev->dev.of_node,
				"qcom,disp-type-amoled");
	if (wled->disp_type_amoled) {
		wled->vref_psm_mv = QPNP_WLED_VREF_PSM_DFLT_AMOLED_MV;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,vref-psm-mv", &temp_val);
		if (!rc) {
			wled->vref_psm_mv = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&pdev->dev, "Unable to read vref-psm\n");
			return rc;
		}

		wled->loop_comp_res_kohm =
			QPNP_WLED_LOOP_COMP_RES_DFLT_AMOLED_KOHM;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,loop-comp-res-kohm", &temp_val);
		if (!rc) {
			wled->loop_comp_res_kohm = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&pdev->dev, "Unable to read loop-comp-res-kohm\n");
			return rc;
		}

		wled->loop_ea_gm = QPNP_WLED_LOOP_EA_GM_DFLT_AMOLED;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,loop-ea-gm", &temp_val);
		if (!rc) {
			wled->loop_ea_gm = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&pdev->dev, "Unable to read loop-ea-gm\n");
			return rc;
		}

		wled->avdd_target_voltage_mv = QPNP_WLED_DFLT_AVDD_MV;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,avdd-target-voltage-mv", &temp_val);
		if (!rc) {
			wled->avdd_target_voltage_mv = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&pdev->dev, "Unable to read avdd target voltage\n");
			return rc;
		}
	}

	wled->sc_deb_cycles = QPNP_WLED_SC_DEB_CYCLES_DFLT;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,sc-deb-cycles", &temp_val);
	if (!rc) {
		wled->sc_deb_cycles = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read sc debounce cycles\n");
		return rc;
	}

	wled->fdbk_op = QPNP_WLED_FDBK_AUTO;
	rc = of_property_read_string(pdev->dev.of_node,
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
		dev_err(&pdev->dev, "Unable to read feedback output\n");
		return rc;
	}

	wled->vref_mv = QPNP_WLED_DFLT_VREF_MV;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,vref-mv", &temp_val);
	if (!rc) {
		wled->vref_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read vref\n");
		return rc;
	}

	wled->switch_freq_khz = QPNP_WLED_SWITCH_FREQ_800_KHZ;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,switch-freq-khz", &temp_val);
	if (!rc) {
		wled->switch_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read switch freq\n");
		return rc;
	}

	if (wled->pmic_rev_id->pmic_subtype == PMICOBALT_SUBTYPE ||
		wled->pmic_rev_id->pmic_subtype == PM2FALCON_SUBTYPE)
		wled->ovp_mv = 29600;
	else
		wled->ovp_mv = 29500;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ovp-mv", &temp_val);
	if (!rc) {
		wled->ovp_mv = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read ovp\n");
		return rc;
	}

	wled->ilim_ma = QPNP_WLED_DFLT_ILIM_MA;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ilim-ma", &temp_val);
	if (!rc) {
		wled->ilim_ma = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read ilim\n");
		return rc;
	}

	wled->boost_duty_ns = QPNP_WLED_DEF_BOOST_DUTY_NS;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,boost-duty-ns", &temp_val);
	if (!rc) {
		wled->boost_duty_ns = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read boost duty\n");
		return rc;
	}

	wled->mod_freq_khz = QPNP_WLED_MOD_FREQ_9600_KHZ;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,mod-freq-khz", &temp_val);
	if (!rc) {
		wled->mod_freq_khz = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read modulation freq\n");
		return rc;
	}

	wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,dim-mode", &temp_str);
	if (!rc) {
		if (strcmp(temp_str, "analog") == 0)
			wled->dim_mode = QPNP_WLED_DIM_ANALOG;
		else if (strcmp(temp_str, "digital") == 0)
			wled->dim_mode = QPNP_WLED_DIM_DIGITAL;
		else
			wled->dim_mode = QPNP_WLED_DIM_HYBRID;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read dim mode\n");
		return rc;
	}

	if (wled->dim_mode == QPNP_WLED_DIM_HYBRID) {
		wled->hyb_thres = QPNP_WLED_DEF_HYB_THRES;
		rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,hyb-thres", &temp_val);
		if (!rc) {
			wled->hyb_thres = temp_val;
		} else if (rc != -EINVAL) {
			dev_err(&pdev->dev, "Unable to read hyb threshold\n");
			return rc;
		}
	}

	wled->sync_dly_us = QPNP_WLED_DEF_SYNC_DLY_US;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,sync-dly-us", &temp_val);
	if (!rc) {
		wled->sync_dly_us = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read sync delay\n");
		return rc;
	}

	wled->fs_curr_ua = QPNP_WLED_FS_CURR_MAX_UA;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,fs-curr-ua", &temp_val);
	if (!rc) {
		wled->fs_curr_ua = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read full scale current\n");
		return rc;
	}

	wled->cons_sync_write_delay_us = 0;
	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,cons-sync-write-delay-us", &temp_val);
	if (!rc)
		wled->cons_sync_write_delay_us = temp_val;

	wled->en_9b_dim_res = of_property_read_bool(pdev->dev.of_node,
			"qcom,en-9b-dim-res");
	wled->en_phase_stag = of_property_read_bool(pdev->dev.of_node,
			"qcom,en-phase-stag");
	wled->en_cabc = of_property_read_bool(pdev->dev.of_node,
			"qcom,en-cabc");

	prop = of_find_property(pdev->dev.of_node,
			"qcom,led-strings-list", &temp_val);
	if (!prop || !temp_val || temp_val > QPNP_WLED_MAX_STRINGS) {
		dev_err(&pdev->dev, "Invalid strings info, use default");
		wled->num_strings = QPNP_WLED_MAX_STRINGS;
		for (i = 0; i < wled->num_strings; i++)
			wled->strings[i] = i;
	} else {
		wled->num_strings = temp_val;
		strings = prop->value;
		for (i = 0; i < wled->num_strings; ++i)
			wled->strings[i] = strings[i];
	}

	wled->ovp_irq = platform_get_irq_byname(pdev, "ovp-irq");
	if (wled->ovp_irq < 0)
		dev_dbg(&pdev->dev, "ovp irq is not used\n");

	wled->sc_irq = platform_get_irq_byname(pdev, "sc-irq");
	if (wled->sc_irq < 0)
		dev_dbg(&pdev->dev, "sc irq is not used\n");

	wled->en_ext_pfet_sc_pro = of_property_read_bool(pdev->dev.of_node,
					"qcom,en-ext-pfet-sc-pro");

	return 0;
}

static int qpnp_wled_probe(struct platform_device *pdev)
{
	struct qpnp_wled *wled;
	struct device_node *revid_node;
	int rc = 0, i;
	const __be32 *prop;

	wled = devm_kzalloc(&pdev->dev, sizeof(*wled), GFP_KERNEL);
	if (!wled)
		return -ENOMEM;
		wled->regmap = dev_get_regmap(pdev->dev.parent, NULL);
		if (!wled->regmap) {
			dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
			return -EINVAL;
		}

	wled->pdev = pdev;

	revid_node = of_parse_phandle(pdev->dev.of_node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	wled->pmic_rev_id = get_revid_data(revid_node);
	if (IS_ERR_OR_NULL(wled->pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
			PTR_ERR(wled->pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	pr_debug("PMIC subtype %d Digital major %d\n",
		wled->pmic_rev_id->pmic_subtype, wled->pmic_rev_id->rev4);

	prop = of_get_address_by_name(pdev->dev.of_node, QPNP_WLED_SINK_BASE,
			0, 0);
	if (!prop) {
		dev_err(&pdev->dev, "Couldnt find sink's addr rc %d\n", rc);
		return rc;
	}
	wled->sink_base = be32_to_cpu(*prop);

	prop = of_get_address_by_name(pdev->dev.of_node, QPNP_WLED_CTRL_BASE,
			0, 0);
	if (!prop) {
		dev_err(&pdev->dev, "Couldnt find ctrl's addr rc = %d\n", rc);
		return rc;
	}
	wled->ctrl_base = be32_to_cpu(*prop);

	dev_set_drvdata(&pdev->dev, wled);

	rc = qpnp_wled_parse_dt(wled);
	if (rc) {
		dev_err(&pdev->dev, "DT parsing failed\n");
		return rc;
	}

	mutex_init(&wled->bus_lock);
	rc = qpnp_wled_config(wled);
	if (rc) {
		dev_err(&pdev->dev, "wled config failed\n");
		return rc;
	}

	mutex_init(&wled->lock);
	INIT_WORK(&wled->work, qpnp_wled_work);
	wled->ramp_ms = QPNP_WLED_RAMP_DLY_MS;
	wled->ramp_step = 1;

	wled->cdev.brightness_set = qpnp_wled_set;
	wled->cdev.brightness_get = qpnp_wled_get;

	wled->cdev.max_brightness = WLED_MAX_LEVEL_4095;

	rc = led_classdev_register(&pdev->dev, &wled->cdev);
	if (rc) {
		dev_err(&pdev->dev, "wled registration failed(%d)\n", rc);
		goto wled_register_fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_wled_attrs); i++) {
		rc = sysfs_create_file(&wled->cdev.dev->kobj,
				&qpnp_wled_attrs[i].attr);
		if (rc < 0) {
			dev_err(&pdev->dev, "sysfs creation failed\n");
			goto sysfs_fail;
		}
	}

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

static int qpnp_wled_remove(struct platform_device *pdev)
{
	struct qpnp_wled *wled = dev_get_drvdata(&pdev->dev);
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

static struct platform_driver qpnp_wled_driver = {
	.driver		= {
		.name		= "qcom,qpnp-wled",
		.of_match_table	= spmi_match_table,
	},
	.probe		= qpnp_wled_probe,
	.remove		= qpnp_wled_remove,
};

static int __init qpnp_wled_init(void)
{
	return platform_driver_register(&qpnp_wled_driver);
}
module_init(qpnp_wled_init);

static void __exit qpnp_wled_exit(void)
{
	platform_driver_unregister(&qpnp_wled_driver);
}
module_exit(qpnp_wled_exit);

MODULE_DESCRIPTION("QPNP WLED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-wled");

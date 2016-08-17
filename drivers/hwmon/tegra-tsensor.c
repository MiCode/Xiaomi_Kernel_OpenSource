/*
 * NVIDIA Tegra SOC - temperature sensor driver
 *
 * Copyright (C) 2011-2012 NVIDIA Corporation
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

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <mach/iomap.h>
#include <mach/clk.h>
#include <mach/delay.h>
#include <mach/tsensor.h>
#include <mach/tegra_fuse.h>

/* macro to enable tsensor hw reset */
/* FIXME: till tsensor temperature is reliable this should be 0 */
#define ENABLE_TSENSOR_HW_RESET 1

/* tsensor instance used for temperature calculation */
#define TSENSOR_FUSE_REV1	8
#define TSENSOR_FUSE_REV2	21

/* version where tsensor temperature reading is accurate */
#define STABLE_TSENSOR_FUSE_REV TSENSOR_FUSE_REV2

/* We have multiple tsensor instances with following registers */
#define SENSOR_CFG0				0x40
#define SENSOR_CFG1				0x48
#define SENSOR_CFG2				0x4c
#define SENSOR_STATUS0				0x58
#define SENSOR_TS_STATUS1			0x5c
#define SENSOR_TS_STATUS2			0x60

/* interrupt mask in tsensor status register */
#define TSENSOR_SENSOR_X_STATUS0_0_INTR_MASK	(1 << 8)

#define SENSOR_CFG0_M_MASK			0xffff
#define SENSOR_CFG0_M_SHIFT			8
#define SENSOR_CFG0_N_MASK			0xff
#define SENSOR_CFG0_N_SHIFT			24
#define SENSOR_CFG0_RST_INTR_SHIFT		6
#define SENSOR_CFG0_HW_DIV2_INTR_SHIFT		5
#define SENSOR_CFG0_OVERFLOW_INTR		4
#define SENSOR_CFG0_DVFS_INTR_SHIFT		3
#define SENSOR_CFG0_RST_ENABLE_SHIFT		2
#define SENSOR_CFG0_HW_DIV2_ENABLE_SHIFT	1
#define SENSOR_CFG0_STOP_SHIFT			0

#define SENSOR_CFG_X_TH_X_MASK			0xffff
#define SENSOR_CFG1_TH2_SHIFT			16
#define SENSOR_CFG1_TH1_SHIFT			0
#define SENSOR_CFG2_TH3_SHIFT			0
#define SENSOR_CFG2_TH0_SHIFT			16

#define SENSOR_STATUS_AVG_VALID_SHIFT		10
#define SENSOR_STATUS_CURR_VALID_SHIFT		9

#define STATE_MASK				0x7
#define STATUS0_STATE_SHIFT			0
#define STATUS0_PREV_STATE_SHIFT		4

#define LOCAL_STR_SIZE1				60
#define MAX_STR_LINE				100
#define MAX_TSENSOR_LOOP1			(1000 * 2)

#define TSENSOR_COUNTER_TOLERANCE		100

#define SENSOR_CTRL_RST_SHIFT			1
#define RST_SRC_MASK				0x7
#define RST_SRC_SENSOR				2
#define TEGRA_REV_REG_OFFSET			0x804
#define CCLK_G_BURST_POLICY_REG_REL_OFFSET	0x368
#define TSENSOR_SLOWDOWN_BIT			23

/* macros used for temperature calculations */
/* assumed get_temperature_int and get_temperature_fraction
 * calculate up to 6 decimal places. print temperature
 * in code assumes 6 decimal place formatting */
#define get_temperature_int(X)			((X) / 1000000)
#define get_temperature_fraction(X)		(((int)(abs(X))) % 1000000)

#define get_temperature_round(X)		DIV_ROUND_CLOSEST(X, 1000000)

#define MILLICELSIUS_TO_CELSIUS(i)		((i) / 1000)
#define CELSIUS_TO_MILLICELSIUS(i)		((i) * 1000)

#define TSENSOR_MILLI_CELSIUS(x) \
	DIV_ROUND_CLOSEST((x), 1000)

#define get_ts_state(data) tsensor_get_reg_field(data,\
			((data->instance << 16) | SENSOR_STATUS0), \
			STATUS0_STATE_SHIFT, STATE_MASK)

/* tsensor states */
enum ts_state {
	TS_INVALID = 0,
	TS_LEVEL0,
	TS_LEVEL1,
	TS_LEVEL2,
	TS_LEVEL3,
	TS_OVERFLOW,
	TS_MAX_STATE = TS_OVERFLOW
};

enum {
	/* temperature is sensed from 2 points on tegra */
	TSENSOR_COUNT = 2,
	TSENSOR_INSTANCE1 = 0,
	TSENSOR_INSTANCE2 = 1,
	/* divide by 2 temperature threshold */
	DIV2_CELSIUS_TEMP_THRESHOLD_DEFAULT = 70,
	/* reset chip temperature threshold */
	RESET_CELSIUS_TEMP_THRESHOLD_DEFAULT = 75,
	/* tsensor frequency in Hz for clk src CLK_M and divisor=24 */
	DEFAULT_TSENSOR_CLK_HZ = 500000,
	DEFAULT_TSENSOR_N = 255,
	DEFAULT_TSENSOR_M = 12500,
	/* tsensor instance offset */
	TSENSOR_INSTANCE_OFFSET = 0x40,
	MIN_THRESHOLD = 0x0,
	MAX_THRESHOLD = 0xffff,
	DEFAULT_THRESHOLD_TH0 = MAX_THRESHOLD,
	DEFAULT_THRESHOLD_TH1 = MAX_THRESHOLD,
	DEFAULT_THRESHOLD_TH2 = MAX_THRESHOLD,
	DEFAULT_THRESHOLD_TH3 = MAX_THRESHOLD,
};

/* constants used to implement sysfs interface */
enum tsensor_params {
	TSENSOR_PARAM_TH1 = 0,
	TSENSOR_PARAM_TH2,
	TSENSOR_PARAM_TH3,
	TSENSOR_TEMPERATURE,
	TSENSOR_STATE,
	TSENSOR_LIMITS,
};

enum tsensor_thresholds {
	TSENSOR_TH0 = 0,
	TSENSOR_TH1,
	TSENSOR_TH2,
	TSENSOR_TH3
};

/*
 * For each registered chip, we need to keep some data in memory.
 * The structure is dynamically allocated.
 */
struct tegra_tsensor_data {
	struct tegra_tsensor_platform_data plat_data;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex mutex;
	struct device *hwmon_dev;
	spinlock_t tsensor_lock;
	struct clk *dev_clk;
	/* tsensor register space */
	void __iomem		*base;
	unsigned long		phys;
	unsigned long		phys_end;
	/* pmc register space */
	void __iomem		*pmc_rst_base;
	unsigned long		pmc_phys;
	unsigned long		pmc_phys_end;
	/* clk register space */
	void __iomem		*clk_rst_base;
	int			irq;

	/* save configuration before suspend and restore after resume */
	unsigned int config0[TSENSOR_COUNT];
	unsigned int config1[TSENSOR_COUNT];
	unsigned int config2[TSENSOR_COUNT];
	/* temperature readings from instance tsensor - 0/1 */
	unsigned int instance;
	s64 A_e_minus12;
	int B_e_minus6;
	unsigned int fuse_T1;
	unsigned int fuse_F1;
	unsigned int fuse_T2;
	unsigned int fuse_F2;
	/* Quadratic fit coefficients: m=-0.003512 n=1.528943 p=-11.1 */
	int m_e_minus6;
	int n_e_minus6;
	int p_e_minus2;

	long current_hi_limit;
	long current_lo_limit;

	bool is_edp_supported;
	struct thermal_zone_device *thz;
};

enum {
	TSENSOR_COEFF_SET1 = 0,
	TSENSOR_COEFF_SET2,
	TSENSOR_COEFF_END
};

struct tegra_tsensor_coeff {
	int e_minus6_m;
	int e_minus6_n;
	int e_minus2_p;
};

static struct tegra_tsensor_coeff coeff_table[] = {
	/* Quadratic fit coefficients: m=-0.002775 n=1.338811 p=-7.30 */
	[TSENSOR_COEFF_SET1] = {
		-2775,
		1338811,
		-730
	},
	/* Quadratic fit coefficients: m=-0.003512 n=1.528943 p=-11.1 */
	[TSENSOR_COEFF_SET2] = {
		-3512,
		1528943,
		-1110
	}
	/* FIXME: add tsensor coefficients after chip characterization */
};

/* pTemperature returned in 100 * Celsius */
static int tsensor_count_2_temp(struct tegra_tsensor_data *data,
	unsigned int count, int *p_temperature);
static unsigned int tsensor_get_threshold_counter(
	struct tegra_tsensor_data *data, int temp);

/* tsensor register access functions */

static void tsensor_writel(struct tegra_tsensor_data *data, u32 val,
				unsigned long reg)
{
	unsigned int reg_offset = reg & 0xffff;
	unsigned char inst = (reg >> 16) & 0xffff;
	writel(val, data->base + (inst * TSENSOR_INSTANCE_OFFSET) +
		reg_offset);
	return;
}

static unsigned int tsensor_readl(struct tegra_tsensor_data *data,
				unsigned long reg)
{
	unsigned int reg_offset = reg & 0xffff;
	unsigned char inst = (reg >> 16) & 0xffff;
	return readl(data->base +
		(inst * TSENSOR_INSTANCE_OFFSET) + reg_offset);
}

static unsigned int tsensor_get_reg_field(
	struct tegra_tsensor_data *data, unsigned int reg,
	unsigned int shift, unsigned int mask)
{
	unsigned int reg_val;
	reg_val = tsensor_readl(data, reg);
	return (reg_val & (mask << shift)) >> shift;
}

static int tsensor_set_reg_field(
	struct tegra_tsensor_data *data, unsigned int value,
	unsigned int reg, unsigned int shift, unsigned int mask)
{
	unsigned int reg_val;
	unsigned int rd_val;
	reg_val = tsensor_readl(data, reg);
	reg_val &= ~(mask << shift);
	reg_val |= ((value & mask) << shift);
	tsensor_writel(data, reg_val, reg);
	rd_val = tsensor_readl(data, reg);
	if (rd_val == reg_val)
		return 0;
	else
		return -EINVAL;
}

/* enable argument is true to enable reset, false disables pmc reset */
static void pmc_rst_enable(struct tegra_tsensor_data *data, bool enable)
{
	unsigned int val;
	/* mapped first pmc reg is SENSOR_CTRL */
	val = readl(data->pmc_rst_base);
	if (enable)
		val |= (1 << SENSOR_CTRL_RST_SHIFT);
	else
		val &= ~(1 << SENSOR_CTRL_RST_SHIFT);
	writel(val, data->pmc_rst_base);
}

/* true returned when pmc reset source is tsensor */
static bool pmc_check_rst_sensor(struct tegra_tsensor_data *data)
{
	unsigned int val;
	unsigned char src;
	val = readl(data->pmc_rst_base + 4);
	src = (unsigned char)(val & RST_SRC_MASK);
	if (src == RST_SRC_SENSOR)
		return true;
	else
		return false;
}

/*
 * function to get chip revision specific tsensor coefficients
 * obtained after chip characterization
 */
static void get_chip_tsensor_coeff(struct tegra_tsensor_data *data)
{
	unsigned short coeff_index;

	coeff_index = TSENSOR_COEFF_SET1;
	if (data->instance == TSENSOR_INSTANCE1)
		coeff_index = TSENSOR_COEFF_SET2;
	data->m_e_minus6 = coeff_table[coeff_index].e_minus6_m;
	data->n_e_minus6 = coeff_table[coeff_index].e_minus6_n;
	data->p_e_minus2 = coeff_table[coeff_index].e_minus2_p;
	pr_info("tsensor coeff: m=%d*10^-6,n=%d*10^-6,p=%d*10^-2\n",
		data->m_e_minus6, data->n_e_minus6, data->p_e_minus2);
}

/* tsensor counter read function */
static int tsensor_read_counter(
	struct tegra_tsensor_data *data,
	unsigned int *p_counter)
{
	unsigned int status_reg;
	unsigned int config0;
	int iter_count = 0;
	const int max_loop = 50;

	do {
		config0 = tsensor_readl(data, ((data->instance << 16) |
			SENSOR_CFG0));
		if (config0 & (1 << SENSOR_CFG0_STOP_SHIFT)) {
			dev_dbg(data->hwmon_dev, "Error: tsensor "
				"counter read with STOP bit not supported\n");
			*p_counter = 0;
			return 0;
		}

		status_reg = tsensor_readl(data,
			(data->instance << 16) | SENSOR_STATUS0);
		if (status_reg & (1 <<
			SENSOR_STATUS_CURR_VALID_SHIFT)) {
			*p_counter = tsensor_readl(data, (data->instance
				<< 16) | SENSOR_TS_STATUS1);
			break;
		}
		if (!(iter_count % 10))
			dev_dbg(data->hwmon_dev, "retry %d\n", iter_count);

		msleep(21);
		iter_count++;
	} while (iter_count < max_loop);

	if (iter_count == max_loop)
		return -ENODEV;

	return 0;
}

/* tsensor threshold print function */
static void dump_threshold(struct tegra_tsensor_data *data)
{
	unsigned int TH_2_1, TH_0_3;
	unsigned int curr_avg;
	int err;

	TH_2_1 = tsensor_readl(data, (data->instance << 16) | SENSOR_CFG1);
	TH_0_3 = tsensor_readl(data, (data->instance << 16) | SENSOR_CFG2);
	dev_dbg(data->hwmon_dev, "Tsensor: TH_2_1=0x%x, "
		"TH_0_3=0x%x\n", TH_2_1, TH_0_3);
	err = tsensor_read_counter(data, &curr_avg);
	if (err < 0)
		pr_err("Error: tsensor counter read, "
			"err=%d\n", err);
	else
		dev_dbg(data->hwmon_dev, "Tsensor: "
			"curr_avg=0x%x\n", curr_avg);
}

static int tsensor_get_temperature(
	struct tegra_tsensor_data *data,
	int *pTemp, unsigned int *pCounter)
{
	int err = 0;
	unsigned int curr_avg;

	err = tsensor_read_counter(data, &curr_avg);
	if (err < 0)
		goto error;

	*pCounter = ((curr_avg & 0xFFFF0000) >> 16);
	err = tsensor_count_2_temp(data, *pCounter, pTemp);

error:
	return err;
}

static ssize_t tsensor_show_state(struct device *dev,
	struct device_attribute *da, char *buf)
{
	int state;
	struct tegra_tsensor_data *data = dev_get_drvdata(dev);

	state = get_ts_state(data);

	return snprintf(buf, 50, "%d\n", state);
}

static ssize_t tsensor_show_limits(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct tegra_tsensor_data *data = dev_get_drvdata(dev);
	return snprintf(buf, 50, "%ld %ld\n",
		data->current_lo_limit, data->current_hi_limit);
}

/* tsensor temperature show function */
static ssize_t tsensor_show_counters(struct device *dev,
	struct device_attribute *da, char *buf)
{
	unsigned int curr_avg;
	char err_str[] = "error-sysfs-counter-read\n";
	char fixed_str[MAX_STR_LINE];
	struct tegra_tsensor_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int err;
	int temp;

	if (attr->index == TSENSOR_TEMPERATURE) {
		/* use current counter value to calculate temperature */
		err = tsensor_read_counter(data, &curr_avg);
		if (err < 0)
			goto error;
		err = tsensor_count_2_temp(data,
			((curr_avg & 0xFFFF0000) >> 16), &temp);
		if (err < 0)
			goto error;

		dev_vdbg(data->hwmon_dev, "%s has curr_avg=0x%x, "
			"temp0=%d\n", __func__, curr_avg, temp);

		snprintf(buf, (((LOCAL_STR_SIZE1 << 1) + 3) +
			strlen(fixed_str)),
			"%d.%06dC %#x\n",
			get_temperature_int(temp),
			get_temperature_fraction(temp),
			((curr_avg & 0xFFFF0000) >> 16));
	}
	return strlen(buf);
error:
	return snprintf(buf, strlen(err_str), "%s", err_str);
}

/* utility function to check hw clock divide by 2 condition */
static bool cclkg_check_hwdiv2_sensor(struct tegra_tsensor_data *data)
{
	unsigned int val;
	val = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE +
		CCLK_G_BURST_POLICY_REG_REL_OFFSET));
	if ((1 << TSENSOR_SLOWDOWN_BIT) & val) {
		dev_err(data->hwmon_dev, "Warning: ***** tsensor "
			"slowdown bit detected\n");
		return true;
	} else {
		return false;
	}
}

/*
 * function with table to return register, field shift and mask
 * values for supported parameters
 */
static int get_param_values(
	struct tegra_tsensor_data *data, unsigned int indx,
	unsigned int *p_reg, unsigned int *p_sft, unsigned int *p_msk,
	char *info, size_t info_len)
{
	switch (indx) {
	case TSENSOR_PARAM_TH1:
		*p_reg = ((data->instance << 16) | SENSOR_CFG1);
		*p_sft = SENSOR_CFG1_TH1_SHIFT;
		*p_msk = SENSOR_CFG_X_TH_X_MASK;
		snprintf(info, info_len, "TH1[%d]: ",
			data->instance);
		break;
	case TSENSOR_PARAM_TH2:
		*p_reg = ((data->instance << 16) | SENSOR_CFG1);
		*p_sft = SENSOR_CFG1_TH2_SHIFT;
		*p_msk = SENSOR_CFG_X_TH_X_MASK;
		snprintf(info, info_len, "TH2[%d]: ",
			data->instance);
		break;
	case TSENSOR_PARAM_TH3:
		*p_reg = ((data->instance << 16) | SENSOR_CFG2);
		*p_sft = SENSOR_CFG2_TH3_SHIFT;
		*p_msk = SENSOR_CFG_X_TH_X_MASK;
		snprintf(info, info_len, "TH3[%d]: ",
			data->instance);
		break;
	default:
		return -ENOENT;
	}
	return 0;
}

/* tsensor driver sysfs show function */
static ssize_t show_tsensor_param(struct device *dev,
				struct device_attribute *da,
				char *buf)
{
	unsigned int val;
	struct tegra_tsensor_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	unsigned int reg;
	unsigned int sft;
	unsigned int msk;
	int err;
	int temp;
	char info[LOCAL_STR_SIZE1];

	err = get_param_values(data, attr->index, &reg, &sft, &msk,
			       info, sizeof(info));
	if (err < 0)
		goto labelErr;
	val = tsensor_get_reg_field(data, reg, sft, msk);
	if (val == MAX_THRESHOLD)
		snprintf(buf, PAGE_SIZE, "%s un-initialized threshold\n", info);
	else {
		err = tsensor_count_2_temp(data, val, &temp);
		if (err != 0)
			goto labelErr;
		snprintf(buf, PAGE_SIZE, "%s threshold: %d.%06d Celsius\n",
			info, get_temperature_int(temp),
			get_temperature_fraction(temp));
	}
	return strlen(buf);

labelErr:
	snprintf(buf, PAGE_SIZE, "ERROR:");
	return strlen(buf);
}

/* tsensor driver sysfs store function */
static ssize_t set_tsensor_param(struct device *dev,
			struct device_attribute *da,
			const char *buf, size_t count)
{
	int num;
	struct tegra_tsensor_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	unsigned int reg;
	unsigned int sft;
	unsigned int msk;
	int err;
	unsigned int counter;
	unsigned int val;
	char info[LOCAL_STR_SIZE1];

	if (kstrtoint(buf, 0, &num)) {
		dev_err(dev, "file: %s, line=%d return %s()\n",
			__FILE__, __LINE__, __func__);
		return -EINVAL;
	}

	counter = tsensor_get_threshold_counter(data, num);

	err = get_param_values(data, attr->index, &reg, &sft, &msk,
			       info, sizeof(info));
	if (err < 0)
		goto labelErr;

	err = tsensor_set_reg_field(data, counter, reg, sft, msk);
	if (err < 0)
		goto labelErr;

	/* TH2 clk divide check */
	if (attr->index == TSENSOR_PARAM_TH2) {
		msleep(21);
		(void)cclkg_check_hwdiv2_sensor(data);
	}
	val = tsensor_get_reg_field(data, reg, sft, msk);
	dev_dbg(dev, "%s 0x%x\n", info, val);
	return count;
labelErr:
	dev_err(dev, "file: %s, line=%d, %s(), error=0x%x\n", __FILE__,
		__LINE__, __func__, err);
	return 0;
}

static struct sensor_device_attribute tsensor_nodes[] = {
	SENSOR_ATTR(tsensor_TH1, S_IRUGO | S_IWUSR,
		show_tsensor_param, set_tsensor_param, TSENSOR_PARAM_TH1),
	SENSOR_ATTR(tsensor_TH2, S_IRUGO | S_IWUSR,
		show_tsensor_param, set_tsensor_param, TSENSOR_PARAM_TH2),
	SENSOR_ATTR(tsensor_TH3, S_IRUGO | S_IWUSR,
		show_tsensor_param, set_tsensor_param, TSENSOR_PARAM_TH3),
	SENSOR_ATTR(tsensor_temperature, S_IRUGO | S_IWUSR,
		tsensor_show_counters, NULL, TSENSOR_TEMPERATURE),
	SENSOR_ATTR(tsensor_state, S_IRUGO | S_IWUSR,
		tsensor_show_state, NULL, TSENSOR_STATE),
	SENSOR_ATTR(tsensor_limits, S_IRUGO | S_IWUSR,
		tsensor_show_limits, NULL, TSENSOR_LIMITS),
};

int tsensor_thermal_get_temp(struct tegra_tsensor_data *data,
				long *milli_temp)
{
	int counter, temp, err;
	int temp_state, ts_state;

	err = tsensor_get_temperature(data,
					&temp,
					&counter);
	if (err)
		return err;

	/* temperature is in milli-Celsius */
	temp = TSENSOR_MILLI_CELSIUS(temp);

	mutex_lock(&data->mutex);

	/* This section of logic is done in order to make sure that
	 * the temperature read corresponds to the current hw state.
	 * If it is not, return the nearest temperature
	 */
	if ((data->current_lo_limit != 0) ||
		(data->current_hi_limit)) {

		if (temp <= data->current_lo_limit)
			temp_state = TS_LEVEL0;
		else if (temp < data->current_hi_limit)
			temp_state = TS_LEVEL1;
		else
			temp_state = TS_LEVEL2;

		ts_state = get_ts_state(data);

		if (ts_state != temp_state) {

			switch (ts_state) {
			case TS_LEVEL0:
				temp = data->current_lo_limit - 1;
				break;
			case TS_LEVEL1:
				if (temp_state == TS_LEVEL0)
					temp = data->current_lo_limit + 1;
				else
					temp = data->current_hi_limit - 1;
				break;
			case TS_LEVEL2:
				temp = data->current_hi_limit + 1;
				break;
			}

		}

	}

	mutex_unlock(&data->mutex);

	*milli_temp = temp;

	return 0;
}

/* tsensor driver interrupt handler */
static irqreturn_t tegra_tsensor_isr(int irq, void *arg_data)
{
	struct tegra_tsensor_data *data =
		(struct tegra_tsensor_data *)arg_data;
	unsigned long flags;
	unsigned int val;
	int new_state;

	spin_lock_irqsave(&data->tsensor_lock, flags);

	val = tsensor_readl(data, (data->instance << 16) | SENSOR_STATUS0);
	if (val & TSENSOR_SENSOR_X_STATUS0_0_INTR_MASK) {
		new_state = get_ts_state(data);

		/* counter overflow check */
		if (new_state == TS_OVERFLOW)
			dev_err(data->hwmon_dev, "Warning: "
				"***** OVERFLOW tsensor\n");

		/* We only care if we go above hi or below low thresholds */
		if (data->is_edp_supported && new_state != TS_LEVEL1)
			queue_delayed_work(data->workqueue, &data->work, 0);
	}

	tsensor_writel(data, val, (data->instance << 16) | SENSOR_STATUS0);

	spin_unlock_irqrestore(&data->tsensor_lock, flags);

	return IRQ_HANDLED;
}

/*
 * function to read fuse registers and give - T1, T2, F1 and F2
 */
static int read_tsensor_fuse_regs(struct tegra_tsensor_data *data)
{
	unsigned int reg1;
	unsigned int T1 = 0, T2 = 0;
	unsigned int spare_bits;
	int err;

	/* read tsensor calibration register */
	/*
	 * High (~90 DegC) Temperature Calibration value (upper 16 bits of
	 * FUSE_TSENSOR_CALIB_0) - F2
	 * Low (~25 deg C) Temperature Calibration value (lower 16 bits of
	 * FUSE_TSENSOR_CALIB_0) - F1
	 */
	err = tegra_fuse_get_tsensor_calibration_data(&reg1);
	if (err)
		goto errLabel;
	data->fuse_F1 = reg1 & 0xFFFF;
	data->fuse_F2 = (reg1 >> 16) & 0xFFFF;

	err = tegra_fuse_get_tsensor_spare_bits(&spare_bits);
	if (err) {
		pr_err("tsensor spare bit fuse read error=%d\n", err);
		goto errLabel;
	}

	/*
	 * FUSE_TJ_ADT_LOWT = T1, FUSE_TJ_ADJ = T2
	 */

	/*
	 * Low temp is:
	 * FUSE_TJ_ADT_LOWT = bits [20:14] or’ed with bits [27:21]
	 */
	T1 = ((spare_bits >> 14) & 0x7F) |
		((spare_bits >> 21) & 0x7F);
	dev_vdbg(data->hwmon_dev, "Tsensor low temp (T1) fuse :\n");

	/*
	 * High temp is:
	 * FUSE_TJ_ADJ = bits [6:0] or’ed with bits [13:7]
	 */
	dev_vdbg(data->hwmon_dev, "Tsensor low temp (T2) fuse :\n");
	T2 = (spare_bits & 0x7F) | ((spare_bits >> 7) & 0x7F);
	pr_info("Tsensor fuse calibration F1=%d(%#x), F2=%d(%#x), T1=%d, T2=%d\n",
		data->fuse_F1, data->fuse_F1,
		data->fuse_F2, data->fuse_F2, T1, T2);
	data->fuse_T1 = T1;
	data->fuse_T2 = T2;
	return 0;
errLabel:
	return err;
}

/* function to calculate interim temperature */
static int calc_interim_temp(struct tegra_tsensor_data *data,
	unsigned int counter, s64 *p_interim_temp)
{
	s64 val1_64;
	s64 val2;
	u32 temp_rem;
	bool is_neg;
	u32 divisor;

	/*
	 * T-int = A * Counter + B
	 * (Counter is the sensor frequency output)
	 */
	if ((data->fuse_F2 - data->fuse_F1) <= (data->fuse_T2 -
		data->fuse_T1)) {
		dev_err(data->hwmon_dev, "Error: F2=%d, F1=%d "
			"difference unexpectedly low. "
			"Aborting temperature processing\n", data->fuse_F2,
			data->fuse_F1);
		return -EINVAL;
	} else {
		/* expression modified after assuming s_A is 10^6 times,
		 * s_B is 10^2 times and want end result to be 10^2 times
		 * actual value
		 */
		val1_64 = (data->A_e_minus12 * counter);
		dev_dbg(data->hwmon_dev, "A_e_-12*counter=%lld\n", val1_64);
		val2 = (s64)data->B_e_minus6 * 1000000ULL;
		dev_dbg(data->hwmon_dev, "B_e_-12=%lld\n", val2);
		val2 += val1_64;
		dev_dbg(data->hwmon_dev, "A_counter+B=%lld\n", val2);
		is_neg = false;
		if (val2 < 0) {
			is_neg = true;
			val2 *= -1;
		}
		divisor = 1000000;
		temp_rem = do_div(val2, divisor);
		if (temp_rem > (divisor >> 1))
			val2++;
		if (is_neg)
			val2 *= -1;
		*p_interim_temp = val2;
		dev_dbg(data->hwmon_dev, "counter=%d, interim_temp=%lld\n",
			counter, *p_interim_temp);
	}
	return 0;
}

/*
 * function to calculate final temperature, given
 * interim temperature
 */
static void calc_final_temp(struct tegra_tsensor_data *data,
	s64 interim_temp, int *p_final_temp)
{
	s64 temp1_64, temp2_64, temp_64, temp1_64_rem;
	u32 temp_rem_32;
	u32 divisor;
	u64 divisor_64;
	bool is_neg;
	/*
	 * T-final = m * T-int ^2 + n * T-int + p
	 * m = -0.002775
	 * n = 1.338811
	 * p = -7.3
	 */

	temp1_64 = (interim_temp * interim_temp);
	/* always positive as squaring value */
	/* losing accuracy here */
	divisor = 10000;
	/* temp1_64 contains quotient and returns remainder */
	temp_rem_32 = do_div(temp1_64, divisor);
	if (temp_rem_32 > (divisor >> 1))
		temp1_64++;
	temp1_64 *= (s64)data->m_e_minus6;
	dev_dbg(data->hwmon_dev, "m_T-interim^2_e^14=%lld\n", temp1_64);
	temp1_64_rem = (s64)data->m_e_minus6 * (s64)temp_rem_32;
	is_neg = false;
	if (temp1_64_rem < 0) {
		is_neg = true;
		temp1_64_rem *= -1;
	}
	temp_rem_32 = do_div(temp1_64_rem, divisor);
	if (temp_rem_32 > (divisor >> 1))
		temp1_64_rem++;
	if (is_neg)
		temp1_64_rem *= -1;
	/* temp1_64 is m * t-int * t-int * 10^14 */

	temp2_64 = (s64)data->n_e_minus6 * interim_temp * 100;
	dev_dbg(data->hwmon_dev, "n_T-interim_e^14=%lld\n", temp2_64);
	/* temp2_64 is n * t-int * 10^14 */

	temp_64 = ((s64)data->p_e_minus2 * (s64)1000000000000ULL);
	/* temp_64 is n * 10^14 */
	temp_64 += temp1_64 + temp2_64 + temp1_64_rem;
	is_neg = false;
	if (temp_64 < 0) {
		is_neg = true;
		temp_64 *= -1;
	}
	divisor_64 = 100000000ULL;
	temp_rem_32 = do_div(temp_64, divisor_64);
	if (temp_rem_32 > (divisor_64 >> 1))
		temp_64++;
	if (is_neg)
		temp_64 *= -1;
	/* temperature * 10^14 / 10^8 */
	/* get LS decimal digit rounding */
	*p_final_temp = (s32)temp_64;
	dev_dbg(data->hwmon_dev, "T-final stage4=%d\n", *p_final_temp);
}

/*
 * Function to compute constants A and B needed for temperature
 * calculation
 * A = (T2-T1) / (F2-F1)
 * B = T1 – A * F1
 */
static int tsensor_get_const_AB(struct tegra_tsensor_data *data)
{
	int err;
	s64 temp_val1, temp_val2;
	u32 temp_rem;
	bool is_neg;
	u32 divisor;

	/*
	 * 1. Find fusing registers for 25C (T1, F1) and 90C (T2, F2);
	 */
	err = read_tsensor_fuse_regs(data);
	if (err) {
		dev_err(data->hwmon_dev, "Fuse register read required "
			"for internal tsensor returns err=%d\n", err);
		return err;
	}

	if (data->fuse_F2 != data->fuse_F1) {
		if ((data->fuse_F2 - data->fuse_F1) <= (data->fuse_T2 -
			data->fuse_T1)) {
			dev_err(data->hwmon_dev, "Error: F2=%d, "
				"F1=%d, difference"
				" unexpectedly low. Aborting temperature"
				"computation\n", data->fuse_F2, data->fuse_F1);
			return -EINVAL;
		} else {
			temp_val1 = (s64)(data->fuse_T2 - data->fuse_T1) *
				1000000000000ULL;
			/* temp_val1 always positive as fuse_T2 > fuse_T1 */
			temp_rem = do_div(temp_val1, (data->fuse_F2 -
				data->fuse_F1));
			data->A_e_minus12 = temp_val1;
			temp_val2 = (s64)(data->fuse_T1 * 1000000000000ULL);
			temp_val2 -= (data->A_e_minus12 * data->fuse_F1);
			is_neg = false;
			if (temp_val2 < 0) {
				is_neg = true;
				temp_val2 *= -1;
			}
			divisor = 1000000;
			temp_rem = do_div(temp_val2, divisor);
			if (temp_rem > (divisor >> 1))
				temp_val2++;
			if (is_neg)
				temp_val2 *= -1;
			data->B_e_minus6 = (s32)temp_val2;
			/* B is 10^6 times now */
		}
	}
	dev_info(data->hwmon_dev, "A_e_minus12 = %lld\n", data->A_e_minus12);
	dev_info(data->hwmon_dev, "B_e_minus6 = %d\n", data->B_e_minus6);
	return 0;
}

/*
 * function calculates expected temperature corresponding to
 * given tsensor counter value
 * Value returned is 100 times calculated temperature since the
 * calculations are using fixed point arithmetic instead of floating point
 */
static int tsensor_count_2_temp(struct tegra_tsensor_data *data,
	unsigned int count, int *p_temperature)
{
	s64 interim_temp;
	int err;

	/*
	 *
	 * 2. Calculate interim temperature:
	 */
	err = calc_interim_temp(data, count, &interim_temp);
	if (err < 0) {
		dev_err(data->hwmon_dev, "tsensor: cannot read temperature\n");
		*p_temperature = -1;
		return err;
	}

	/*
	 *
	 * 3. Calculate final temperature:
	 */
	calc_final_temp(data, interim_temp, p_temperature);
	/* logs counter -> temperature conversion */
	dev_dbg(data->hwmon_dev, "tsensor: counter=0x%x, interim "
		"temp*10^6=%lld, Final temp=%d.%06d\n",
		count, interim_temp,
		get_temperature_int(*p_temperature),
		get_temperature_fraction(*p_temperature));
	return 0;
}

/*
 * function to solve quadratic roots of equation
 * used to get counter corresponding to given temperature
 */
static void get_quadratic_roots(struct tegra_tsensor_data *data,
		int temp, unsigned int *p_counter1,
		unsigned int *p_counter2)
{
	/*
	 * Equation to solve:
	 * m * A^2 * Counter^2 +
	 * A * (2 * m * B + n) * Counter +
	 * (m * B^2 + n * B + p - Temperature) = 0

	To calculate root - assume
		b = A * (2 * m * B + n)
		a = m * A^2
		c = ((m * B^2) + n * B + p - temp)
	root1 = (-b + sqrt(b^2 - (4*a*c))) / (2 * a)
	root2 = (-b - sqrt(b^2 - (4*a*c))) / (2 * a)
	sqrt(k) = sqrt(k * 10^6) / sqrt(10^6)

	Roots are :
	(-(2*m*B+n)+sqrt(((2*m*B+n)^2-4*m(m*B^2+n*B+p-temp))))/(2*m*A)
	and
	(-(2*m*B+n)-sqrt(((2*m*B+n)^2-4*m(m*B^2+n*B+p-temp))))/(2*m*A)

	After simplify ((2*m*B+n)^2-4*m(m*B^2+n*B+p-temp)),
	Roots are:
	(-(2*m*B+n)+sqrt((n^2-4*m(p-temp))))/(2*m*A)
	and
	(-(2*m*B+n)-sqrt((n^2-4*m(p-temp))))/(2*m*A)
	*/

	int v_e_minus6_2mB_n;
	int v_e_minus6_4m_p_minusTemp;
	int v_e_minus6_n2;
	int v_e_minus6_b2_minus4ac;
	int v_e_minus6_sqrt_b2_minus4ac;
	s64 v_e_minus12_2mA;
	int root1, root2;
	int temp_rem;
	bool is_neg;
	s64 temp_64;

	dev_dbg(data->hwmon_dev, "m_e-6=%d,n_e-6=%d,p_e-2=%d,A_e-6=%lld,"
		"B_e-2=%d\n", data->m_e_minus6, data->n_e_minus6,
		data->p_e_minus2, data->A_e_minus12, data->B_e_minus6);

	temp_64 = (2ULL * (s64)data->m_e_minus6 * (s64)data->B_e_minus6);
	is_neg = false;
	if (temp_64 < 0) {
		is_neg = true;
		temp_64 *= -1;
	}
	temp_rem = do_div(temp_64, 1000000);
	if (is_neg)
		temp_64 *= -1;
	v_e_minus6_2mB_n = (s32)temp_64 + data->n_e_minus6;
	/* computed 2mB + n */

	temp_64 = ((s64)data->m_e_minus6 * (s64)data->A_e_minus12);
	temp_64 *= 2;
	is_neg = false;
	if (temp_64 < 0) {
		temp_64 *= -1;
		is_neg = true;
	}
	temp_rem = do_div(temp_64, 1000000);
	if (is_neg)
		temp_64 *= -1;
	v_e_minus12_2mA = temp_64;
	/* computed 2mA */

	temp_64 = ((s64)data->n_e_minus6 * (s64)data->n_e_minus6);
	/* squaring give positive value */
	temp_rem = do_div(temp_64, 1000000);
	v_e_minus6_n2 = (s32)temp_64;
	/* computed n^2 */

	v_e_minus6_4m_p_minusTemp = data->p_e_minus2 - (temp * 100);
	v_e_minus6_4m_p_minusTemp *= 4 * data->m_e_minus6;
	v_e_minus6_4m_p_minusTemp = DIV_ROUND_CLOSEST(
		v_e_minus6_4m_p_minusTemp,100);
	/* computed 4m*(p-T)*/

	v_e_minus6_b2_minus4ac = (v_e_minus6_n2 - v_e_minus6_4m_p_minusTemp);

	/* To preserve 1 decimal digits for sqrt(v_e_minus6_b2_minus4ac),
	Make it 100 times, so
	v_e_minus6_sqrt_b2_minus4ac=(int_sqrt(v_e_minus6_b2_minus4ac *100)*10^6)
					/sqrt(10^6 * 100)
	To avoid overflow,Simplify it to be:
	v_e_minus6_sqrt_b2_minus4ac =(int_sqrt(v_e_minus6_b2_minus4ac *100)*100)
	*/

	v_e_minus6_sqrt_b2_minus4ac = (int_sqrt(v_e_minus6_b2_minus4ac * 100)
		 * 100);
	dev_dbg(data->hwmon_dev, "A_e_minus12=%lld, B_e_minus6=%d, "
		"m_e_minus6=%d, n_e_minus6=%d, p_e_minus2=%d, "
		"temp=%d\n", data->A_e_minus12, data->B_e_minus6,
		data->m_e_minus6,
		data->n_e_minus6, data->p_e_minus2, (int)temp);
	dev_dbg(data->hwmon_dev, "2mB_n=%d, 2mA=%lld, 4m_p_minusTemp=%d,"
		"b2_minus4ac=%d\n", v_e_minus6_2mB_n,
		v_e_minus12_2mA, v_e_minus6_4m_p_minusTemp,
		v_e_minus6_b2_minus4ac);

	temp_64=(s64)(-v_e_minus6_2mB_n - v_e_minus6_sqrt_b2_minus4ac) * 1000000;
	root1=(s32)div64_s64(temp_64, v_e_minus12_2mA);

	temp_64=(s64)(-v_e_minus6_2mB_n + v_e_minus6_sqrt_b2_minus4ac) * 1000000;
	root2=(s32)div64_s64(temp_64, v_e_minus12_2mA);

	dev_dbg(data->hwmon_dev, "new expr: temp=%d, root1=%d, root2=%d\n",
		temp, root1, root2);

	*p_counter1 = root1;
	*p_counter2 = root2;
	/* we find that root2 is more appropriate root */

	/* logs temperature -> counter conversion */
	dev_dbg(data->hwmon_dev, "temperature=%d, counter1=%#x, "
		"counter2=%#x\n", temp, *p_counter1, *p_counter2);
}

/*
 * function returns tsensor expected counter corresponding to input
 * temperature in degree Celsius.
 * e.g. for temperature of 35C, temp=35
 */
static void tsensor_temp_2_count(struct tegra_tsensor_data *data,
				int temp,
				unsigned int *p_counter1,
				unsigned int *p_counter2)
{
	dev_dbg(data->hwmon_dev, "Trying to calculate counter"
		" for requested temperature"
		" threshold=%d\n", temp);
	/*
	 * calculate the constants needed to get roots of
	 * following quadratic eqn:
	 * m * A^2 * Counter^2 +
	 * A * (2 * m * B + n) * Counter +
	 * (m * B^2 + n * B + p - Temperature) = 0
	 */
	get_quadratic_roots(data, temp, p_counter1, p_counter2);
	/*
	 * checked at current temperature=35 the counter=11418
	 * for 50 deg temperature: counter1=22731, counter2=11817
	 * at 35 deg temperature: counter1=23137, counter2=11411
	 * hence, for above values we are assuming counter2 has
	 * the correct value
	 */
}

/*
 * function to compare computed and expected values with
 * certain tolerance setting hard coded here
 */
static bool cmp_counter(
	struct tegra_tsensor_data *data,
	unsigned int actual, unsigned int exp)
{
	unsigned int smaller;
	unsigned int larger;
	smaller = (actual > exp) ? exp : actual;
	larger = (smaller == actual) ? exp : actual;
	if ((larger - smaller) > TSENSOR_COUNTER_TOLERANCE) {
		dev_dbg(data->hwmon_dev, "actual=%d, exp=%d, larger=%d, "
		"smaller=%d, tolerance=%d\n", actual, exp, larger, smaller,
		TSENSOR_COUNTER_TOLERANCE);
		return false;
	}
	return true;
}

/* function to print chart of counter to temperature values -
 * It uses F1, F2, T1, T2 and start data gives reading
 * for temperature in between the range
 */
static void print_counter_2_temperature_table(
	struct tegra_tsensor_data *data)
{
	int i;
	unsigned int start_counter, end_counter;
	unsigned int diff;
	int temperature;
	const unsigned int num_readings = 40;
	unsigned int index = 0;
	dev_dbg(data->hwmon_dev, "***Counter and Temperature chart **********\n");
	start_counter = data->fuse_F1;
	end_counter = data->fuse_F2;
	diff = (end_counter - start_counter) / num_readings;

	/* We want to take num_readings counter values in between
	and try to report corresponding temperature */
	for (i = start_counter; i <= (end_counter + diff);
		i += diff) {
		tsensor_count_2_temp(data, i, &temperature);
		dev_dbg(data->hwmon_dev, "[%d]: Counter=%#x, temperature=%d.%06dC\n",
			++index, i, get_temperature_int(temperature),
			get_temperature_fraction(temperature));
	}
	dev_dbg(data->hwmon_dev, "\n\n");
	tsensor_count_2_temp(data, end_counter, &temperature);
	dev_dbg(data->hwmon_dev, "[%d]: Counter=%#x, temperature=%d.%06dC\n",
		++index, end_counter, get_temperature_int(temperature),
		get_temperature_fraction(temperature));
}

static bool temp_matched(int given_temp, int calc_temp)
{
	const int temp_diff_max = 4;
	int diff;

	diff = given_temp - calc_temp;
	if (diff < 0)
		diff *= -1;
	if (diff > temp_diff_max)
		return false;
	else
		return true;
}

/* function to print chart of temperature to counter values */
static void print_temperature_2_counter_table(
	struct tegra_tsensor_data *data)
{
	int i;
	int min = -25;
	int max = 120;
	unsigned int counter1, counter2;
	int temperature;

	dev_dbg(data->hwmon_dev, "Temperature and counter1 and "
		"counter2 chart **********\n");
	for (i = min; i <= max; i++) {
		tsensor_temp_2_count(data, i,
			&counter1, &counter2);
		dev_dbg(data->hwmon_dev, "temperature=%d, "
			"counter1=0x%x, counter2=0x%x\n",
			i, counter1, counter2);
		/* verify the counter2 to temperature conversion */
		tsensor_count_2_temp(data, counter2, &temperature);
		dev_dbg(data->hwmon_dev, "Given temp=%d: counter2=%d, conv temp=%d.%06d\n",
			i, counter2, get_temperature_int(temperature),
			get_temperature_fraction(temperature));
		if (!temp_matched(i, get_temperature_round(temperature)))
			dev_dbg(data->hwmon_dev, "tsensor temp to counter to temp conversion failed for temp=%d\n",
				i);
	}
	dev_dbg(data->hwmon_dev, "\n\n");
}

static void dump_a_tsensor_reg(struct tegra_tsensor_data *data,
	unsigned int addr)
{
	dev_dbg(data->hwmon_dev, "tsensor[%d][0x%x]: 0x%x\n", (addr >> 16),
		addr & 0xFFFF, tsensor_readl(data, addr));
}

static void dump_tsensor_regs(struct tegra_tsensor_data *data)
{
	int i;
	for (i = 0; i < TSENSOR_COUNT; i++) {
		/* if STOP bit is set skip this check */
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_CFG0));
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_CFG1));
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_CFG2));
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_STATUS0));
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_TS_STATUS1));
		dump_a_tsensor_reg(data, ((i << 16) | SENSOR_TS_STATUS2));
		dump_a_tsensor_reg(data, ((i << 16) | 0x0));
		dump_a_tsensor_reg(data, ((i << 16) | 0x44));
		dump_a_tsensor_reg(data, ((i << 16) | 0x50));
		dump_a_tsensor_reg(data, ((i << 16) | 0x54));
		dump_a_tsensor_reg(data, ((i << 16) | 0x64));
		dump_a_tsensor_reg(data, ((i << 16) | 0x68));
	}
}

/*
 * function to test if conversion of counter to temperature
 * and vice-versa is working
 */
static int test_temperature_algo(struct tegra_tsensor_data *data)
{
	unsigned int actual_counter;
	unsigned int curr_avg;
	unsigned int counter1, counter2;
	int T1;
	int err = 0;
	bool result1, result2;
	bool result = false;

	/* read actual counter */
	err = tsensor_read_counter(data, &curr_avg);
	if (err < 0) {
		pr_err("Error: tsensor0 counter read, err=%d\n", err);
		goto endLabel;
	}
	actual_counter = ((curr_avg & 0xFFFF0000) >> 16);
	dev_dbg(data->hwmon_dev, "counter read=0x%x\n", actual_counter);

	/* calculate temperature */
	err = tsensor_count_2_temp(data, actual_counter, &T1);
	dev_dbg(data->hwmon_dev, "%s actual counter=0x%x, calculated "
		"temperature=%d.%06d\n", __func__,
		actual_counter, get_temperature_int(T1),
		get_temperature_fraction(T1));
	if (err < 0) {
		pr_err("Error: calculate temperature step\n");
		goto endLabel;
	}

	/* calculate counter corresponding to read temperature */
	tsensor_temp_2_count(data, get_temperature_round(T1),
		&counter1, &counter2);
	dev_dbg(data->hwmon_dev, "given temperature=%d, counter1=0x%x,"
		" counter2=0x%x\n",
		get_temperature_round(T1), counter1, counter2);

	err = tsensor_count_2_temp(data, actual_counter, &T1);
	dev_dbg(data->hwmon_dev, "%s 2nd time actual counter=0x%x, "
		"calculated temperature=%d.%d\n", __func__,
		actual_counter, get_temperature_int(T1),
		get_temperature_fraction(T1));
	if (err < 0) {
		pr_err("Error: calculate temperature step\n");
		goto endLabel;
	}

	/* compare counter calculated with actual original counter */
	result1 = cmp_counter(data, actual_counter, counter1);
	result2 = cmp_counter(data, actual_counter, counter2);
	if (result1) {
		dev_dbg(data->hwmon_dev, "counter1 matches: actual=%d,"
			" calc=%d\n", actual_counter, counter1);
		result = true;
	}
	if (result2) {
		dev_dbg(data->hwmon_dev, "counter2 matches: actual=%d,"
			" calc=%d\n", actual_counter, counter2);
		result = true;
	}
	if (!result) {
		pr_info("NO Match: actual=%d,"
			" calc counter2=%d, counter1=%d\n", actual_counter,
			counter2, counter1);
		err = -EIO;
	}

endLabel:
	return err;
}

/* tsensor threshold temperature to threshold counter conversion function */
static unsigned int tsensor_get_threshold_counter(
	struct tegra_tsensor_data *data,
	int temp_threshold)
{
	unsigned int counter1, counter2;
	unsigned int counter;

	if (temp_threshold < 0)
		return MAX_THRESHOLD;

	tsensor_temp_2_count(data, temp_threshold, &counter1, &counter2);

	counter = counter2;

	return counter;
}

/* tsensor temperature threshold setup function */
static void tsensor_threshold_setup(struct tegra_tsensor_data *data,
					unsigned char index)
{
	unsigned long config0;
	unsigned char i = index;
	unsigned int th2_count = DEFAULT_THRESHOLD_TH2;
	unsigned int th3_count = DEFAULT_THRESHOLD_TH3;
	unsigned int th1_count = DEFAULT_THRESHOLD_TH1;
	int th0_diff = 0;

	dev_dbg(data->hwmon_dev, "started tsensor_threshold_setup %d\n",
		index);
	config0 = tsensor_readl(data, ((i << 16) | SENSOR_CFG0));

	dev_dbg(data->hwmon_dev, "before threshold program TH dump:\n");
	dump_threshold(data);
	dev_dbg(data->hwmon_dev, "th3=0x%x, th2=0x%x, th1=0x%x, th0=0x%x\n",
		th3_count, th2_count, th1_count, th0_diff);
	config0 = (((th2_count & SENSOR_CFG_X_TH_X_MASK)
		<< SENSOR_CFG1_TH2_SHIFT) |
		((th1_count & SENSOR_CFG_X_TH_X_MASK) <<
		SENSOR_CFG1_TH1_SHIFT));
	tsensor_writel(data, config0, ((i << 16) | SENSOR_CFG1));
	config0 = (((th0_diff & SENSOR_CFG_X_TH_X_MASK)
		<< SENSOR_CFG2_TH0_SHIFT) |
		((th3_count & SENSOR_CFG_X_TH_X_MASK) <<
		SENSOR_CFG2_TH3_SHIFT));
	tsensor_writel(data, config0, ((i << 16) | SENSOR_CFG2));
	dev_dbg(data->hwmon_dev, "after threshold program TH dump:\n");
	dump_threshold(data);
}

/* tsensor config programming function */
static int tsensor_config_setup(struct tegra_tsensor_data *data)
{
	unsigned int config0;
	unsigned int i;
	int err = 0;

	for (i = 0; i < TSENSOR_COUNT; i++) {
		/*
		 * Pre-read setup:
		 * Set M and N values
		 * Enable HW features HW_FREQ_DIV_EN, THERMAL_RST_EN
		 */
		config0 = tsensor_readl(data, ((i << 16) | SENSOR_CFG0));
		config0 &= ~((SENSOR_CFG0_M_MASK << SENSOR_CFG0_M_SHIFT) |
			(SENSOR_CFG0_N_MASK << SENSOR_CFG0_N_SHIFT) |
			(1 << SENSOR_CFG0_OVERFLOW_INTR) |
			(1 << SENSOR_CFG0_RST_INTR_SHIFT) |
			(1 << SENSOR_CFG0_DVFS_INTR_SHIFT) |
			(1 << SENSOR_CFG0_HW_DIV2_INTR_SHIFT) |
			(1 << SENSOR_CFG0_RST_ENABLE_SHIFT) |
			(1 << SENSOR_CFG0_HW_DIV2_ENABLE_SHIFT)
			);
		/* Set STOP bit */
		/* Set M and N values */
		/* Enable HW features HW_FREQ_DIV_EN, THERMAL_RST_EN */
		config0 |= (
			((DEFAULT_TSENSOR_M & SENSOR_CFG0_M_MASK) <<
				SENSOR_CFG0_M_SHIFT) |
			((DEFAULT_TSENSOR_N & SENSOR_CFG0_N_MASK) <<
				SENSOR_CFG0_N_SHIFT) |
			(1 << SENSOR_CFG0_OVERFLOW_INTR) |
			(1 << SENSOR_CFG0_DVFS_INTR_SHIFT) |
			(1 << SENSOR_CFG0_HW_DIV2_INTR_SHIFT) |
#if ENABLE_TSENSOR_HW_RESET
			(1 << SENSOR_CFG0_RST_ENABLE_SHIFT) |
#endif
			(1 << SENSOR_CFG0_STOP_SHIFT));

		tsensor_writel(data, config0, ((i << 16) | SENSOR_CFG0));
		tsensor_threshold_setup(data, i);
	}

	/* Disable sensor stop bit */
	config0 = tsensor_readl(data, (data->instance << 16) | SENSOR_CFG0);
	config0 &= ~(1 << SENSOR_CFG0_STOP_SHIFT);
	tsensor_writel(data, config0, (data->instance << 16) | SENSOR_CFG0);

	/* initialize tsensor chip coefficients */
	get_chip_tsensor_coeff(data);

	return err;
}

/* function to enable tsensor clock */
static int tsensor_clk_enable(
	struct tegra_tsensor_data *data,
	bool enable)
{
	int err = 0;
	unsigned long rate;
	struct clk *clk_m;

	if (enable) {
		clk_prepare_enable(data->dev_clk);
		rate = clk_get_rate(data->dev_clk);
		clk_m = clk_get_sys(NULL, "clk_m");
		if (clk_get_parent(data->dev_clk) != clk_m) {
			err = clk_set_parent(data->dev_clk, clk_m);
			if (err < 0)
				goto fail;
		}
		rate = DEFAULT_TSENSOR_CLK_HZ;
		if (rate != clk_get_rate(clk_m)) {
			err = clk_set_rate(data->dev_clk, rate);
			if (err < 0)
				goto fail;
		}
	} else {
		clk_disable_unprepare(data->dev_clk);
		clk_put(data->dev_clk);
	}
fail:
	return err;
}

/*
 * function to set counter threshold corresponding to
 * given temperature
 */
static void tsensor_set_limits(
	struct tegra_tsensor_data *data,
	int temp,
	int threshold_index)
{
	unsigned int th_count;
	unsigned int config;
	unsigned short sft, offset;
	unsigned int th1_count;

	th_count = tsensor_get_threshold_counter(data, temp);
	dev_dbg(data->hwmon_dev, "%s : input temp=%d, counter=0x%x\n", __func__,
		temp, th_count);
	switch (threshold_index) {
	case TSENSOR_TH0:
		sft = 16;
		offset = SENSOR_CFG2;
		/* assumed TH1 set before TH0, else we program
		 * TH0 as TH1 which means hysteresis will be
		 * same as TH1. Also, caller expected to pass
		 * (TH1 - hysteresis) as temp argument for this case */
		th1_count = tsensor_readl(data,
			((data->instance << 16) |
			SENSOR_CFG1));
		th_count = (th1_count > th_count) ?
			(th1_count - th_count) :
			th1_count;
		break;
	case TSENSOR_TH1:
	default:
		sft = 0;
		offset = SENSOR_CFG1;
		break;
	case TSENSOR_TH2:
		sft = 16;
		offset = SENSOR_CFG1;
		break;
	case TSENSOR_TH3:
		sft = 0;
		offset = SENSOR_CFG2;
		break;
	}
	config = tsensor_readl(data, ((data->instance << 16) | offset));
	dev_dbg(data->hwmon_dev, "%s: old config=0x%x, sft=%d, offset=0x%x\n",
		__func__, config, sft, offset);
	config &= ~(SENSOR_CFG_X_TH_X_MASK << sft);
	config |= ((th_count & SENSOR_CFG_X_TH_X_MASK) << sft);
	dev_dbg(data->hwmon_dev, "new config=0x%x\n", config);
	tsensor_writel(data, config, ((data->instance << 16) | offset));
}

static int tsensor_within_limits(struct tegra_tsensor_data *data)
{
	int ts_state = get_ts_state(data);

	return (ts_state == TS_LEVEL1);
}

#ifdef CONFIG_THERMAL
static int tsensor_thermal_set_limits(struct tegra_tsensor_data *data,
					long lo_limit_milli,
					long hi_limit_milli)
{
	long lo_limit = MILLICELSIUS_TO_CELSIUS(lo_limit_milli);
	long hi_limit = MILLICELSIUS_TO_CELSIUS(hi_limit_milli);
	int i, j, hi_limit_first;

	if (lo_limit_milli == hi_limit_milli)
		return -EINVAL;

	mutex_lock(&data->mutex);

	if (data->current_lo_limit == lo_limit_milli &&
		data->current_hi_limit == hi_limit_milli) {
		goto done;
	}

	/* If going up, change hi limit first.  If going down, change lo
	   limit first */
	hi_limit_first = hi_limit_milli > data->current_hi_limit;

	for (i = 0; i < 2; i++) {
		j = (i + hi_limit_first) % 2;

		switch (j) {
		case 0:
			tsensor_set_limits(data, hi_limit, TSENSOR_TH2);
			data->current_hi_limit = hi_limit_milli;
			break;
		case 1:
			tsensor_set_limits(data, lo_limit, TSENSOR_TH1);
			data->current_lo_limit = lo_limit_milli;
			break;
		}
	}


done:
	mutex_unlock(&data->mutex);
	return 0;
}

static void tsensor_update(struct tegra_tsensor_data *data)
{
	struct thermal_zone_device *thz = data->thz;
	long temp, trip_temp, low_temp = 0, high_temp = 120000;
	int count;

	if (!thz)
		return;

	if (!thz->passive)
		thermal_zone_device_update(thz);

	thz->ops->get_temp(thz, &temp);

	for (count = 0; count < thz->trips; count++) {
		thz->ops->get_trip_temp(thz, count, &trip_temp);

		if ((trip_temp >= temp) && (trip_temp < high_temp))
			high_temp = trip_temp;

		if ((trip_temp < temp) && (trip_temp > low_temp))
			low_temp = trip_temp;
	}

	tsensor_thermal_set_limits(data, low_temp, high_temp);
}
#else
static void tsensor_update(struct tegra_tsensor_data *data)
{
}
#endif

static void tsensor_work_func(struct work_struct *work)
{
	struct tegra_tsensor_data *data = container_of(to_delayed_work(work),
		struct tegra_tsensor_data, work);

	if (!tsensor_within_limits(data)) {
		tsensor_update(data);

		if (!tsensor_within_limits(data))
			dev_dbg(data->hwmon_dev,
				"repeated work queueing state=%d\n",
				get_ts_state(data));
			queue_delayed_work(data->workqueue, &data->work,
				HZ * DEFAULT_TSENSOR_M /
				DEFAULT_TSENSOR_CLK_HZ);
	}
}

/*
 * This function enables the tsensor using default configuration
 * 1. We would need some configuration APIs to calibrate
 * the tsensor counters to right temperature
 * 2. hardware triggered divide cpu clock by 2 as well pmu reset is enabled
 * implementation. No software actions are enabled at this point
 */
static int tegra_tsensor_setup(struct platform_device *pdev)
{
	struct tegra_tsensor_data *data = platform_get_drvdata(pdev);
	struct resource *r;
	int err = 0;
	struct tegra_tsensor_platform_data *tsensor_data;
	unsigned int reg;

	data->dev_clk = clk_get(&pdev->dev, NULL);
	if ((!data->dev_clk) || ((int)data->dev_clk == -(ENOENT))) {
		dev_err(&pdev->dev, "Couldn't get the clock\n");
		err = PTR_ERR(data->dev_clk);
		goto fail;
	}

	/* Enable tsensor clock */
	err = tsensor_clk_enable(data, true);
	if (err < 0)
		goto err_irq;

	/* Reset tsensor */
	dev_dbg(&pdev->dev, "before tsensor reset %s\n", __func__);
	tegra_periph_reset_assert(data->dev_clk);
	udelay(100);
	tegra_periph_reset_deassert(data->dev_clk);
	udelay(100);

	dev_dbg(&pdev->dev, "before tsensor chk pmc reset %s\n",
		__func__);
	/* Check for previous resets in pmc */
	if (pmc_check_rst_sensor(data)) {
		dev_err(data->hwmon_dev, "Warning: ***** Last PMC "
			"Reset source: tsensor detected\n");
	}

	dev_dbg(&pdev->dev, "before tsensor pmc reset enable %s\n",
		__func__);
	/* Enable the sensor reset in PMC */
	pmc_rst_enable(data, true);

	dev_dbg(&pdev->dev, "before tsensor get platform data %s\n",
		__func__);
	dev_dbg(&pdev->dev, "tsensor platform_data=0x%x\n",
		(unsigned int)pdev->dev.platform_data);
	tsensor_data = pdev->dev.platform_data;

	/* register interrupt */
	r = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!r) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENXIO;
		goto err_irq;
	}
	data->irq = r->start;
	err = request_irq(data->irq, tegra_tsensor_isr,
			IRQF_DISABLED, pdev->name, data);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to register IRQ\n");
		goto err_irq;
	}

	dev_dbg(&pdev->dev, "tsensor platform_data=0x%x\n",
		(unsigned int)pdev->dev.platform_data);

	dev_dbg(&pdev->dev, "before tsensor_config_setup\n");
	err = tsensor_config_setup(data);
	if (err) {
		dev_err(&pdev->dev, "[%s,line=%d]: tsensor counters dead!\n",
			__func__, __LINE__);
		goto err_setup;
	}
	dev_dbg(&pdev->dev, "before tsensor_get_const_AB\n");
	/* calculate constants needed for temperature conversion */
	err = tsensor_get_const_AB(data);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to extract temperature\n"
			"const\n");
		goto err_setup;
	}

	/* test if counter-to-temperature and temperature-to-counter
	 * are matching */
	err = test_temperature_algo(data);
	if (err) {
		dev_err(&pdev->dev, "Error: read temperature\n"
			"algorithm broken\n");
		goto err_setup;
	}

	print_temperature_2_counter_table(data);

	print_counter_2_temperature_table(data);

	/* EDP and throttling support using tsensor enabled
	 * based on fuse revision */
	err = tegra_fuse_get_revision(&reg);
	if (err)
		goto err_setup;

	data->is_edp_supported = (reg >= STABLE_TSENSOR_FUSE_REV);

	if (data->is_edp_supported) {
		data->workqueue = create_singlethread_workqueue("tsensor");
		INIT_DELAYED_WORK(&data->work, tsensor_work_func);
	}

	return 0;
err_setup:
	free_irq(data->irq, data);
err_irq:
	tsensor_clk_enable(data, false);
fail:
	dev_err(&pdev->dev, "%s error=%d returned\n", __func__, err);
	return err;
}

#ifdef CONFIG_THERMAL
static int tsensor_get_temp(struct thermal_zone_device *thz,
					unsigned long *temp)
{
	struct tegra_tsensor_data *data = thz->devdata;
	return tsensor_thermal_get_temp(data, temp);
}

static int tsensor_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct tegra_tsensor_data *data = thz->devdata;

	if (cdev == data->plat_data.passive.cdev)
		return thermal_zone_bind_cooling_device(thz, 0, cdev,
							THERMAL_NO_LIMIT,
							THERMAL_NO_LIMIT);

	for (i = 0; data->plat_data.active[i].cdev; i++)
		if (cdev == data->plat_data.active[i].cdev)
			return thermal_zone_bind_cooling_device(thz, i+1, cdev,
							THERMAL_NO_LIMIT,
							THERMAL_NO_LIMIT);

	return 0;
}

static int tsensor_unbind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	int i;
	struct tegra_tsensor_data *data = thz->devdata;

	if (cdev == data->plat_data.passive.cdev)
		return thermal_zone_unbind_cooling_device(thz, 0, cdev);

	for (i = 0; data->plat_data.active[i].cdev; i++)
		if (cdev == data->plat_data.active[i].cdev)
			return thermal_zone_unbind_cooling_device(thz, i+1,
									cdev);

	return 0;
}

static int tsensor_get_trip_temp(struct thermal_zone_device *thz,
					int trip,
					unsigned long *temp)
{
	struct tegra_tsensor_data *data = thz->devdata;
	if (trip == 0)
		*temp = data->plat_data.passive.trip_temp;
	else
		*temp = data->plat_data.active[trip-1].trip_temp;
	return 0;
}

static int tsensor_get_trip_type(struct thermal_zone_device *thz,
					int trip,
					enum thermal_trip_type *type)
{
	*type = (trip == 0) ? THERMAL_TRIP_PASSIVE : THERMAL_TRIP_ACTIVE;
	return 0;
}


static struct thermal_zone_device_ops tsensor_ops = {
	.get_temp = tsensor_get_temp,
	.bind = tsensor_bind,
	.unbind = tsensor_unbind,
	.get_trip_type = tsensor_get_trip_type,
	.get_trip_temp = tsensor_get_trip_temp,
};
#endif

static int __devinit tegra_tsensor_probe(struct platform_device *pdev)
{
	struct tegra_tsensor_data *data;
	struct resource *r;
	int err;
	unsigned int reg;
	u8 i;
	struct tegra_tsensor_platform_data *tsensor_data;
#ifdef CONFIG_THERMAL
	int num_trips = 0;
#endif

	data = kzalloc(sizeof(struct tegra_tsensor_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "[%s,line=%d]: Failed to allocate "
			"memory\n", __func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}
	mutex_init(&data->mutex);
	platform_set_drvdata(pdev, data);

	/* Register sysfs hooks */
	for (i = 0; i < ARRAY_SIZE(tsensor_nodes); i++) {
		err = device_create_file(&pdev->dev,
			&tsensor_nodes[i].dev_attr);
		if (err) {
			dev_err(&pdev->dev, "device_create_file failed.\n");
			goto err0;
		}
	}

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto err1;
	}

	dev_set_drvdata(data->hwmon_dev, data);

	spin_lock_init(&data->tsensor_lock);

	/* map tsensor register space */
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "[%s,line=%d]: Failed to get io "
			"resource\n", __func__, __LINE__);
		err = -ENODEV;
		goto err2;
	}

	if (!request_mem_region(r->start, (r->end - r->start) + 1,
				dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "[%s,line=%d]: Error mem busy\n",
			__func__, __LINE__);
		err = -EBUSY;
		goto err2;
	}

	data->phys = r->start;
	data->phys_end = r->end;
	data->base = ioremap(r->start, r->end - r->start + 1);
	if (!data->base) {
		dev_err(&pdev->dev, "[%s, line=%d]: can't ioremap "
			"tsensor iomem\n", __FILE__, __LINE__);
		err = -ENOMEM;
		goto err3;
	}

	/* map pmc rst_status register */
	r = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (r == NULL) {
		dev_err(&pdev->dev, "[%s,line=%d]: Failed to get io "
			"resource\n", __func__, __LINE__);
		err = -ENODEV;
		goto err4;
	}

	if (!request_mem_region(r->start, (r->end - r->start) + 1,
				dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "[%s, line=%d]: Error mem busy\n",
			__func__, __LINE__);
		err = -EBUSY;
		goto err4;
	}

	data->pmc_phys = r->start;
	data->pmc_phys_end = r->end;
	data->pmc_rst_base = ioremap(r->start, r->end - r->start + 1);
	if (!data->pmc_rst_base) {
		dev_err(&pdev->dev, "[%s, line=%d]: can't ioremap "
			"pmc iomem\n", __FILE__, __LINE__);
		err = -ENOMEM;
		goto err5;
	}

	/* fuse revisions less than TSENSOR_FUSE_REV1
	   bypass tsensor driver init */
	/* tsensor active instance decided based on fuse revision */
	err = tegra_fuse_get_revision(&reg);
	if (err)
		goto err6;
	/* check for higher revision done first */
	/* instance 0 is used for fuse revision TSENSOR_FUSE_REV2 onwards */
	if (reg >= TSENSOR_FUSE_REV2)
		data->instance = TSENSOR_INSTANCE1;
	/* instance 1 is used for fuse revision TSENSOR_FUSE_REV1 till
	   TSENSOR_FUSE_REV2 */
	else if (reg >= TSENSOR_FUSE_REV1)
		data->instance = TSENSOR_INSTANCE2;
	pr_info("tsensor active instance=%d\n", data->instance);

	/* tegra tsensor - setup and init */
	err = tegra_tsensor_setup(pdev);
	if (err)
		goto err6;

	dump_tsensor_regs(data);
	dev_dbg(&pdev->dev, "end tegra_tsensor_probe\n");

	tsensor_data = pdev->dev.platform_data;

	memcpy(&data->plat_data, tsensor_data,
		sizeof(struct tegra_tsensor_platform_data));

	tsensor_set_limits(data, tsensor_data->shutdown_temp, TSENSOR_TH3);

#ifdef CONFIG_THERMAL
	if (tsensor_data->passive.cdev)
		num_trips++;

	for (i = 0; tsensor_data->active[i].cdev; i++)
		num_trips++;

	data->thz = thermal_zone_device_register("tsensor",
					num_trips,
					0x0,
					data,
					&tsensor_ops,
					NULL,
					tsensor_data->passive.passive_delay,
					0);
	if (IS_ERR_OR_NULL(data->thz))
		goto err6;

	tsensor_update(data);
#endif

	return 0;
err6:
	iounmap(data->pmc_rst_base);
err5:
	release_mem_region(data->pmc_phys, (data->pmc_phys_end -
		data->pmc_phys) + 1);
err4:
	iounmap(data->base);
err3:
	release_mem_region(data->phys, (data->phys_end -
		data->phys) + 1);
err2:
	hwmon_device_unregister(data->hwmon_dev);
err1:
	for (i = 0; i < ARRAY_SIZE(tsensor_nodes); i++)
		device_remove_file(&pdev->dev, &tsensor_nodes[i].dev_attr);
err0:
	kfree(data);
exit:
	dev_err(&pdev->dev, "%s error=%d returned\n", __func__, err);
	return err;
}

static int __devexit tegra_tsensor_remove(struct platform_device *pdev)
{
	struct tegra_tsensor_data *data = platform_get_drvdata(pdev);
	u8 i;

	hwmon_device_unregister(data->hwmon_dev);
	for (i = 0; i < ARRAY_SIZE(tsensor_nodes); i++)
		device_remove_file(&pdev->dev, &tsensor_nodes[i].dev_attr);

	if (data->is_edp_supported) {
		cancel_delayed_work_sync(&data->work);
		destroy_workqueue(data->workqueue);
		data->workqueue = NULL;
	}

	free_irq(data->irq, data);

	iounmap(data->pmc_rst_base);
	release_mem_region(data->pmc_phys, (data->pmc_phys_end -
		data->pmc_phys) + 1);
	iounmap(data->base);
	release_mem_region(data->phys, (data->phys_end -
		data->phys) + 1);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static void save_tsensor_regs(struct tegra_tsensor_data *data)
{
	int i;
	for (i = 0; i < TSENSOR_COUNT; i++) {
		data->config0[i] = tsensor_readl(data,
			((i << 16) | SENSOR_CFG0));
		data->config1[i] = tsensor_readl(data,
			((i << 16) | SENSOR_CFG1));
		data->config2[i] = tsensor_readl(data,
			((i << 16) | SENSOR_CFG2));
	}
}

static void restore_tsensor_regs(struct tegra_tsensor_data *data)
{
	int i;
	for (i = 0; i < TSENSOR_COUNT; i++) {
		tsensor_writel(data, data->config0[i],
			((i << 16) | SENSOR_CFG0));
		tsensor_writel(data, data->config1[i],
			((i << 16) | SENSOR_CFG1));
		tsensor_writel(data, data->config2[i],
			((i << 16) | SENSOR_CFG2));
	}
}


static int tsensor_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct tegra_tsensor_data *data = platform_get_drvdata(pdev);
	unsigned int config0;

	disable_irq(data->irq);
	cancel_delayed_work_sync(&data->work);
	/* set STOP bit, else OVERFLOW interrupt seen in LP1 */
	config0 = tsensor_readl(data, ((data->instance << 16) | SENSOR_CFG0));
	config0 |= (1 << SENSOR_CFG0_STOP_SHIFT);
	tsensor_writel(data, config0, ((data->instance << 16) | SENSOR_CFG0));

	/* save current settings before suspend, when STOP bit is set */
	save_tsensor_regs(data);
	tsensor_clk_enable(data, false);

	return 0;
}

static int tsensor_resume(struct platform_device *pdev)
{
	struct tegra_tsensor_data *data = platform_get_drvdata(pdev);
	unsigned int config0;

	tsensor_clk_enable(data, true);
	/* restore current settings before suspend, no need
	 * to clear STOP bit */
	restore_tsensor_regs(data);

	/* clear STOP bit, after restoring regs */
	config0 = tsensor_readl(data, ((data->instance << 16) | SENSOR_CFG0));
	config0 &= ~(1 << SENSOR_CFG0_STOP_SHIFT);
	tsensor_writel(data, config0, ((data->instance << 16) | SENSOR_CFG0));

	if (data->is_edp_supported)
		schedule_delayed_work(&data->work, 0);

	enable_irq(data->irq);
	return 0;
}
#endif

static struct platform_driver tegra_tsensor_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tegra-tsensor",
	},
	.probe		= tegra_tsensor_probe,
	.remove		= __devexit_p(tegra_tsensor_remove),
#ifdef CONFIG_PM
	.suspend	= tsensor_suspend,
	.resume		= tsensor_resume,
#endif
};

static int __init tegra_tsensor_init(void)
{
	return platform_driver_register(&tegra_tsensor_driver);
}
module_init(tegra_tsensor_init);

static void __exit tegra_tsensor_exit(void)
{
	platform_driver_unregister(&tegra_tsensor_driver);
}
module_exit(tegra_tsensor_exit);

MODULE_AUTHOR("nvidia");
MODULE_DESCRIPTION("Nvidia Tegra Temperature Sensor driver");
MODULE_LICENSE("GPL");

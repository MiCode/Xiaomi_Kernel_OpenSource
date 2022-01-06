// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/bits.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include "soc_temp_lvts.h"
#include "../thermal_core.h"

/*==================================================
 * LVTS debug patch
 *==================================================
 */
#define DUMP_MORE_LOG

#ifdef DUMP_MORE_LOG
#define NUM_LVTS_DEVICE_REG (9)
#define LVTS_CONTROLLER_DEBUG_NUM (10)

static const unsigned int g_lvts_device_addrs[NUM_LVTS_DEVICE_REG] = {
	RG_TSFM_DATA_0,
	RG_TSFM_CTRL_1,
	RG_TSV2F_CTRL_0,
	RG_TSV2F_CTRL_4,
	RG_TEMP_DATA_0,
	RG_RC_DATA_0,
	RG_DIV_DATA_0,
	RG_DBG_FQMTR,
	RG_DID_LVTS};

static unsigned int g_lvts_device_value_b[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_DEVICE_REG];
static unsigned int g_lvts_device_value_e[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_DEVICE_REG];

#define NUM_LVTS_CONTROLLER_REG (26)
static const unsigned int g_lvts_controller_addrs[NUM_LVTS_CONTROLLER_REG] = {
	LVTSMONCTL0_0,
	LVTSMONCTL1_0,
	LVTSMONCTL2_0,
	LVTSMONINT_0,
	LVTSMONINTSTS_0,
	LVTSMONIDET3_0,
	LVTSMSRCTL0_0,
	LVTSMSRCTL1_0,
	LVTS_ID_0,
	LVTS_CONFIG_0,
	LVTSEDATA00_0,
	LVTSEDATA01_0,
	LVTSEDATA02_0,
	LVTSEDATA03_0,
	LVTSMSR0_0,
	LVTSMSR1_0,
	LVTSMSR2_0,
	LVTSMSR3_0,
	LVTSRDATA0_0,
	LVTSRDATA1_0,
	LVTSRDATA2_0,
	LVTSRDATA3_0,
	LVTSPROTCTL_0,
	LVTSPROTTC_0,
	LVTSCLKEN_0,
	LVTSSPARE3_0};
static unsigned int g_lvts_controller_value_b[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_CONTROLLER_REG];
static unsigned int g_lvts_controller_value_e[LVTS_CONTROLLER_DEBUG_NUM]
	[NUM_LVTS_CONTROLLER_REG];
#endif


/*==================================================
 * LVTS local common code
 *==================================================
 */
static int lvts_raw_to_temp_v1(struct formula_coeff *co, unsigned int sensor_id,
	unsigned int msr_raw)
{
	/* This function returns degree mC */

	int temp;

	temp = (co->a[0] * ((unsigned long long)msr_raw)) >> 14;
	temp = temp + co->golden_temp * 500 - co->a[0];

	return temp;
}

static unsigned int lvts_temp_to_raw_v1(struct formula_coeff *co, unsigned int sensor_id,
	int temp)
{
	unsigned int msr_raw = 0;

	msr_raw = ((long long)((co->golden_temp * 500 - co->a[0] - temp)) << 14)
		/ (-1 * co->a[0]);

	return msr_raw;
}

static int lvts_raw_to_temp_v2(struct formula_coeff *co, unsigned int sensor_id,
	unsigned int msr_raw)
{
	/* This function returns degree mC */

	int temp;

	temp = ((long long)co->a[sensor_id] << 14) / msr_raw;
	temp = temp + co->golden_temp * 500 - co->a[sensor_id];

	return temp;
}

static unsigned int lvts_temp_to_raw_v2(struct formula_coeff *co, unsigned int sensor_id,
	int temp)
{
	unsigned int msr_raw = 0;

	msr_raw = ((long long)co->a[sensor_id] << 14) / (temp - (co->golden_temp * 500) +
		co->a[sensor_id]);

	return msr_raw;
}

static int lvts_read_tc_msr_raw(unsigned int *msr_reg)
{
	if (msr_reg == 0)
		return 0;

	return readl(msr_reg) & MRS_RAW_MASK;
}

static int lvts_read_all_tc_temperature(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, s_index, msr_raw;
	int max_temp, current_temp;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * j);

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;
			else
				current_temp = ops->lvts_raw_to_temp(&(tc[i].coeff), j, msr_raw);

			if (i == 0 && j == 0)
				max_temp = current_temp;
			else if (current_temp > max_temp)
				max_temp = current_temp;

			mutex_lock(&lvts_data->sen_data_lock);
			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
			mutex_unlock(&lvts_data->sen_data_lock);
		}
	}

	return max_temp;
}

static int lvts_read_tc_temperature(struct lvts_data *lvts_data, unsigned int tz_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, msr_raw;
	unsigned int s_index = tz_id - 1;
	int current_temp;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			if (s_index != tc[i].sensor_map[j])
				continue;

			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * j);

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;
			else
				current_temp = ops->lvts_raw_to_temp(&(tc[i].coeff), j, msr_raw);

			mutex_lock(&lvts_data->sen_data_lock);
			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
			mutex_unlock(&lvts_data->sen_data_lock);

			return current_temp;
		}
	}

	return THERMAL_TEMP_INVALID;
}

static int soc_temp_lvts_read_temp(void *data, int *temperature)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *) data;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;

	if (lvts_tz->id == 0)
		*temperature = lvts_read_all_tc_temperature(lvts_data);
	else if (lvts_tz->id - 1 < lvts_data->num_sensor)
		*temperature = lvts_read_tc_temperature(lvts_data, lvts_tz->id);
	else
		return -EINVAL;

	return 0;
}

static void lvts_write_device(struct lvts_data *lvts_data, unsigned int data,
	int tc_id)
{
	void __iomem *base;
	struct device *dev = lvts_data->dev;
	int ret;

	base = GET_BASE_ADDR(tc_id);

	writel(data, LVTS_CONFIG_0 + base);

	udelay(5);
	ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				 !(data & DEVICE_ACCESS_STARTUS),
				 2, 200);
	if (ret)
		dev_err(dev,
			"write device err: LVTS %d didn't ready, data 0x%x\n", tc_id, data);
}

static unsigned int lvts_read_device(struct lvts_data *lvts_data,
	unsigned int reg_idx, int tc_id)
{
	struct device *dev = lvts_data->dev;
	void __iomem *base;
	unsigned int data;
	int ret;

	base = GET_BASE_ADDR(tc_id);
	writel(READ_DEVICE_REG(reg_idx), LVTS_CONFIG_0 + base);


	ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				 !(data & DEVICE_ACCESS_STARTUS),
				 2, 200);
	if (ret)
		dev_err(dev,
			"read device err: LVTS %d didn't ready, reg_idx 0x%x\n", tc_id, reg_idx);

	data = (readl(LVTSRDATA0_0 + base));

	return data;
}

static void wait_all_tc_sensing_point_idle(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int mask, error_code, is_error;
	void __iomem *base;
	int i, cnt, ret;

	mask = BIT(10) | BIT(7) | BIT(0);

	for (cnt = 0; cnt < 2; cnt++) {
		is_error = 0;
		for (i = 0; i < lvts_data->num_tc; i++) {
			base = GET_BASE_ADDR(i);
			ret = readl_poll_timeout(LVTSMSRCTL1_0 + base, error_code,
				!(error_code & mask), 2, 200);
			/*
			 * Error code
			 * 000: IDLE
			 * 001: Write transaction
			 * 010: Waiting for read after Write
			 * 011: Disable Continue fetching on Device
			 * 100: Read transaction
			 * 101: Set Device special Register for Voltage threshold
			 * 111: Set TSMCU number for Fetch
			 */
			error_code = ((error_code & BIT(10)) >> 8) +
				((error_code & BIT(7)) >> 6) +
				(error_code & BIT(0));

			if (ret)
				dev_err(dev,
				"Error LVTS %d sensing points aren't idle, error_code %d\n",
				i, error_code);

			if (error_code != 0)
				is_error = 1;
		}

		if (is_error == 0)
			break;
	}
}

static void lvts_reset(struct lvts_data *lvts_data)
{
	int i;

	for (i = 0; i < lvts_data->num_domain; i++) {
		if (lvts_data->domain[i].reset)
			reset_control_assert(lvts_data->domain[i].reset);

		if (lvts_data->domain[i].reset)
			reset_control_deassert(lvts_data->domain[i].reset);
	}
}

static void device_identification_v1(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		writel(ENABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);

		lvts_write_device(lvts_data, READ_BACK_DEVICE_ID, i);

		/* Check LVTS device ID */
		data = (readl(LVTS_ID_0 + base) & GENMASK(7, 0));
		if (data != (0x81 + i))
			dev_err(dev, "LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				i, (0x81 + i), data);
	}
}

static void disable_all_sensing_points(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		writel(DISABLE_SENSING_POINT, LVTSMONCTL0_0 + base);
	}
}

static void enable_all_sensing_points(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, num;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		num = tc[i].num_sensor;

		if (num > ALL_SENSING_POINTS) {
			dev_err(dev,
				"%s, LVTS%d, illegal number of sensors: %d\n",
				__func__, i, tc[i].num_sensor);
			continue;
		}

		writel(ENABLE_SENSING_POINT(num), LVTSMONCTL0_0 + base);
	}
}

#ifdef DUMP_MORE_LOG
static void read_controller_reg_before_active(struct lvts_data *lvts_data)
{
	int i, j, temp;
        void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
                base = GET_BASE_ADDR(i);

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(LVTSMONCTL0_0 + g_lvts_controller_addrs[j]
				+ base);
			g_lvts_controller_value_b[i][j] = temp;
		}
	}
}

static void read_controller_reg_when_error(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;
        int temp, j;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

                for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			temp = readl(g_lvts_controller_addrs[j] + base);
			g_lvts_controller_value_e[i][j] = temp;
		}
	}
}

static int lvts_write_device_reg(struct lvts_data *lvts_data, unsigned int config,
        unsigned int dev_reg_idx, unsigned int data, int tc_id)
{
        void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	dev_reg_idx &= 0xFF;
	data &= 0xFF;

	config = config | (dev_reg_idx << 8) | data;


	writel(config, LVTS_CONFIG_0 + base);

	udelay(5);

	return 1;
}

static void read_device_reg_before_active(struct lvts_data *lvts_data)
{
	int i, j;
	unsigned int addr, data;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];
			data =  lvts_read_device(lvts_data, addr, i);
			g_lvts_device_value_b[i][j] = data;
		}
	}
}

static void read_device_reg_when_error(struct lvts_data *lvts_data)
{
	int i, j;
	unsigned int addr;
        void __iomem *base;
        int cnt;
        struct device *dev = lvts_data->dev;


	for (i = 0; i < lvts_data->num_tc; i++) {

		base = GET_BASE_ADDR(i);

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			addr = g_lvts_device_addrs[j];

			lvts_write_device_reg(lvts_data, 0x81020000, addr, 0x00, i);

			/* wait 2us + 3us buffer*/
			udelay(5);
			/* Check ASIF bus status for transaction finished
			 * Wait until DEVICE_ACCESS_START = 0
			 */
			cnt = 0;
			while ((readl(LVTS_CONFIG_0 + base) & BIT(24))) {
				cnt++;

				if (cnt == 100) {
					dev_info(dev, "Error: DEVICE_ACCESS_START didn't ready\n");
					break;
				}
				udelay(2);
			}

			g_lvts_device_value_e[i][j] =
                                (readl(LVTSRDATA0_0 + base));
		}
	}
}

void clear_lvts_register_value_array(struct lvts_data *lvts_data)
{
	int i, j;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			g_lvts_controller_value_b[i][j] = 0;
			g_lvts_controller_value_e[i][j] = 0;
		}

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			g_lvts_device_value_b[i][j] = 0;
			g_lvts_device_value_e[i][j] = 0;
		}
	}
}

static void dump_lvts_device_register_value(struct lvts_data *lvts_data)
{
	int i, j, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;

	for (i = 0; i < lvts_data->num_tc; i++) {
		dev_info(dev, "[LVTS_ERROR][BEFROE][CONTROLLER_%d][DUMP]\n", i);
		offset = snprintf(buffer, sizeof(buffer),
                        	"[LVTS_ERROR][BEFORE][DEVICE][DUMP] ");
		if (offset < 0)
			return;
		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			offset += snprintf(buffer + offset,
	                                sizeof(buffer) - offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_b[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);


		offset = snprintf(buffer, sizeof(buffer),
                        	"[LVTS_ERROR][AFTER][DEVICE][DUMP] ");
		if (offset < 0)
			return;

		for (j = 0; j < NUM_LVTS_DEVICE_REG; j++) {
			offset += snprintf(buffer + offset,
	                                sizeof(buffer) - offset, "0x%x:%x ",
					g_lvts_device_addrs[j],
					g_lvts_device_value_e[i][j]);
		}
		if (offset < 0)
			return;
		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

static void dump_lvts_controller_register_value(struct lvts_data *lvts_data)
{
	int i, j, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;


	for (i = 0; i < lvts_data->num_tc; i++) {
		dev_info(dev, "[LVTS_ERROR][BEFROE][CONTROLLER_%d]\n", i);

		offset = snprintf(buffer, sizeof(buffer),
			"[LVTS_ERROR][BEFORE][TC][DUMP] ");
		if (offset < 0)
			return;
		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			offset += snprintf(buffer + offset,
					sizeof(buffer) - offset, "(0x%x:%x)",
					tc[i].addr_offset + g_lvts_controller_addrs[j],
					g_lvts_controller_value_b[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);


		dev_info(dev, "[LVTS_ERROR][AFTER][CONTROLLER_%d]\n", i);

		offset = snprintf(buffer, sizeof(buffer),
						"[LVTS_ERROR][AFTER][TC][DUMP] ");
		if (offset < 0)
			return;

		for (j = 0; j < NUM_LVTS_CONTROLLER_REG; j++) {
			offset += snprintf(buffer + offset,
						sizeof(buffer) - offset, "(0x%x:%x)",
						tc[i].addr_offset + g_lvts_controller_addrs[j],
						g_lvts_controller_value_e[i][j]);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

static void dump_lvts_controller_temp_and_raw(struct lvts_data *lvts_data)
{
	int i, offset;
	char buffer[512];
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int j, s_index;

	for (i = 0; i < lvts_data->num_tc; i++) {

		dev_info(dev, "[LVTS_ERROR][CONTROLLER_%d] ", i);

		offset = snprintf(buffer, sizeof(buffer),
			"[LVTS_ERROR][TEMP][MSR_RAW] ");

		if (offset < 0)
			return;
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			offset += snprintf(buffer + offset,
					sizeof(buffer) - offset, "[0x%x,%d]",
					lvts_data->sen_data[s_index].msr_raw,
					lvts_data->sen_data[s_index].temp);
		}
		if (offset < 0)
			return;

		buffer[offset] = '\0';
		dev_info(dev, "%s\n", buffer);
	}
}

/*
 * lvts_thermal_check_all_sensing_point_idle -
 * Check if all sensing points are idle
 * Return: 0 if all sensing points are idle
 *         an error code if one of them is busy
 * error code[31:16]: an index of LVTS thermal controller
 * error code[2]: bit 10 of LVTSMSRCTL1
 * error code[1]: bit 7 of LVTSMSRCTL1
 * error code[0]: bit 0 of LVTSMSRCTL1
 */
static int lvts_thermal_check_all_sensing_point_idle(struct lvts_data *lvts_data)
{
	int i, temp, error_code;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		//offset = lvts_tscpu_g_tc[i].tc_offset;
                base = GET_BASE_ADDR(i);
		temp = readl(base + LVTSMSRCTL1_0);
		/* Check if bit10=bit7=bit0=0 */
		if ((temp & 0x481) != 0) {
			error_code = (i << 16) + ((temp & BIT(10)) >> 8) +
				((temp & BIT(7)) >> 6) +
				(temp & BIT(0));

			return error_code;
		}
	}

	return 0;
}

void lvts_wait_for_all_sensing_point_idle(struct lvts_data *lvts_data)
{
        struct device *dev = lvts_data->dev;
	int cnt = 0;
        int temp;


	/*
	 * Wait until all sensoring points idled.
	 * No need to check LVTS status when suspend/resume,
	 * this will spend extra 100us of suspend flow.
	 * LVTS status will be reset after resume.
	 */
	while (cnt < 50) {
		temp = lvts_thermal_check_all_sensing_point_idle(lvts_data);
		if (temp == 0)
			break;

		if ((cnt + 1) % 10 == 0) {
                        dev_info(dev, "Cnt= %d LVTS TC %d, LVTSMSRCTL1[10,7,0] = %d,%d,%d, LVTSMSRCTL1[10:0] = 0x%x\n",
					cnt + 1, (temp >> 16),
					((temp & BIT(2)) >> 2),
					((temp & BIT(1)) >> 1),
					(temp & BIT(0)),
					(temp & GENMASK(10,0)));
		}

		udelay(2);
		cnt++;
	}
}


void dump_lvts_error_info(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;

        dev_info(dev, "%s\n", __func__);

	/*dump controller registers*/
	read_controller_reg_when_error(lvts_data);
	dump_lvts_controller_temp_and_raw(lvts_data);

	/*disable controller by set LVTSMONCTL0[3:0] = 4'h0*/
	disable_all_sensing_points(lvts_data);
        lvts_wait_for_all_sensing_point_idle(lvts_data);

	dump_lvts_controller_register_value(lvts_data);

	read_device_reg_when_error(lvts_data);
	dump_lvts_device_register_value(lvts_data);

}
#endif

static void set_polling_speed(struct lvts_data *lvts_data, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int lvtsMonCtl1, lvtsMonCtl2;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	lvtsMonCtl1 = (((tc[tc_id].tc_speed.group_interval_delay
			<< 20) & GENMASK(29, 20)) |
			(tc[tc_id].tc_speed.period_unit &
			GENMASK(9, 0)));
	lvtsMonCtl2 = (((tc[tc_id].tc_speed.filter_interval_delay
			<< 16) & GENMASK(25, 16)) |
			(tc[tc_id].tc_speed.sensor_interval_delay
			& GENMASK(9, 0)));
	/*
	 * Clock source of LVTS thermal controller is 26MHz.
	 * Period unit is a base for all interval delays
	 * All interval delays must multiply it to convert a setting to time.
	 * Filter interval delay is a delay between two samples of the same sensor
	 * Sensor interval delay is a delay between two samples of differnet sensors
	 * Group interval delay is a delay between different rounds.
	 * For example:
	 *     If Period unit = C, filter delay = 1, sensor delay = 2, group delay = 1,
	 *     and two sensors, TS1 and TS2, are in a LVTS thermal controller
	 *     and then
	 *     Period unit = C * 1/26M * 256 = 12 * 38.46ns * 256 = 118.149us
	 *     Filter interval delay = 1 * Period unit = 118.149us
	 *     Sensor interval delay = 2 * Period unit = 236.298us
	 *     Group interval delay = 1 * Period unit = 118.149us
	 *
	 *     TS1    TS1 ... TS1    TS2    TS2 ... TS2    TS1...
	 *        <--> Filter interval delay
	 *                       <--> Sensor interval delay
	 *                                             <--> Group interval delay
	 */
	writel(lvtsMonCtl1, LVTSMONCTL1_0 + base);
	writel(lvtsMonCtl2, LVTSMONCTL2_0 + base);
}

static void set_hw_filter(struct lvts_data *lvts_data, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int option;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	option = tc[tc_id].hw_filter & 0x7;
	/* hw filter
	 * 000: Get one sample
	 * 001: Get 2 samples and average them
	 * 010: Get 4 samples, drop max and min, then average the rest of 2 samples
	 * 011: Get 6 samples, drop max and min, then average the rest of 4 samples
	 * 100: Get 10 samples, drop max and min, then average the rest of 8 samples
	 * 101: Get 18 samples, drop max and min, then average the rest of 16 samples
	 */
	option = (option << 9) | (option << 6) | (option << 3) | option;

	writel(option, LVTSMSRCTL0_0 + base);
}

static int get_dominator_index(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	int d_index;

	if (tc[tc_id].dominator_sensing_point == ALL_SENSING_POINTS) {
		d_index = ALL_SENSING_POINTS;
	} else if (tc[tc_id].dominator_sensing_point <
		tc[tc_id].num_sensor){
		d_index = tc[tc_id].dominator_sensing_point;
	} else {
		dev_err(dev,
			"Error: LVTS%d, dominator_sensing_point= %d should smaller than num_sensor= %d\n",
			tc_id, tc[tc_id].dominator_sensing_point,
			tc[tc_id].num_sensor);

		dev_err(dev, "Use the sensing point 0 as the dominated sensor\n");
		d_index = SENSING_POINT0;
	}

	return d_index;
}

static void disable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* LVTS thermal controller has two interrupts for thermal HW reboot
	 * One is for AP SW and the other is for RGU
	 * The interrupt of AP SW can turn off by a bit of a register, but
	 * the other for RGU cannot.
	 * To prevent rebooting device accidentally, we are going to add
	 * a huge offset to LVTS and make LVTS always report extremely low
	 * temperature.
	 */

	/* After adding the huge offset 0x3FFF, LVTS alawys adds the
	 * offset to MSR_RAW.
	 * When MSR_RAW is larger, SW will convert lower temperature/
	 */
	temp = readl(LVTSPROTCTL_0 + base);
	writel(temp | 0x3FFF, LVTSPROTCTL_0 + base);

	/* Disable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);

	temp = temp & ~(STAGE3_INT_EN);

	if (lvts_data->enable_dump_log) {
		temp = temp & ~(HIGH_OFFSET3_INT_EN |
						HIGH_OFFSET2_INT_EN |
						HIGH_OFFSET1_INT_EN |
						HIGH_OFFSET0_INT_EN);

		temp = temp & ~(LOW_OFFSET3_INT_EN |
						LOW_OFFSET2_INT_EN |
						LOW_OFFSET1_INT_EN |
						LOW_OFFSET0_INT_EN);

		pr_notice("[LVTS]%s,temp=0x%8x\n", __func__, temp);
	}

	writel(temp, LVTSMONINT_0 + base);
}

static void enable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* Enable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);

	if (lvts_data->enable_dump_log) {
		temp = temp | HIGH_OFFSET3_INT_EN |
				HIGH_OFFSET2_INT_EN |
				HIGH_OFFSET1_INT_EN |
				HIGH_OFFSET0_INT_EN;

		temp = temp | LOW_OFFSET3_INT_EN |
				LOW_OFFSET2_INT_EN |
				LOW_OFFSET1_INT_EN |
				LOW_OFFSET0_INT_EN;

		pr_notice("[LVTS]%s,temp=0x%8x\n", __func__, temp);
	} else {
		temp = temp | STAGE3_INT_EN;
	}

	writel(temp, LVTSMONINT_0 + base);

	/* Clear the offset */
	temp = readl(LVTSPROTCTL_0 + base);
	writel(temp & ~PROTOFFSET, LVTSPROTCTL_0 + base);
}

static void set_tc_hw_reboot_threshold(struct lvts_data *lvts_data,
	int trip_point, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int msr_raw, cur_msr_raw, temp, config, ts_name, d_index, i;
	void __iomem *base;
	struct platform_ops *ops = &lvts_data->ops;

	base = GET_BASE_ADDR(tc_id);
	d_index = get_dominator_index(lvts_data, tc_id);

	disable_hw_reboot_interrupt(lvts_data, tc_id);

	temp = readl(LVTSPROTCTL_0 + base);
	if (d_index == ALL_SENSING_POINTS) {
		ts_name = 0;
		/* Maximum of 4 sensing points */
		config = (0x1 << 16);
		writel(config | temp, LVTSPROTCTL_0 + base);
		msr_raw = 0;
		for (i = 0; i < tc[tc_id].num_sensor; i++) {
			cur_msr_raw = ops->lvts_temp_to_raw(&(tc[tc_id].coeff), i, trip_point);
			if (msr_raw < cur_msr_raw)
				msr_raw = cur_msr_raw;
		}
	} else {
		ts_name = tc[tc_id].sensor_map[d_index];
		/* Select protection sensor */
		config = ((d_index << 2) + 0x2) << 16;
		writel(config | temp, LVTSPROTCTL_0 + base);
		msr_raw = ops->lvts_temp_to_raw(&(tc[tc_id].coeff), d_index, trip_point);
	}

	if (lvts_data->enable_dump_log) {
		/* high offset INT */
		writel(msr_raw, LVTSOFFSETH_0 + base);

		/*
		 * lowoffset INT
		 * set a big msr_raw = 0xffff(very low temperature)
		 * to let lowoffset INT not be triggered
		 */
		writel(0xffff, LVTSOFFSETL_0 + base);
	} else {
		writel(msr_raw, LVTSPROTTC_0 + base);
	}

	enable_hw_reboot_interrupt(lvts_data, tc_id);
}

static void set_all_tc_hw_reboot(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	int i, trip_point;

	for (i = 0; i < lvts_data->num_tc; i++) {
		trip_point = tc[i].hw_reboot_trip_point;

		if (tc[i].num_sensor == 0)
			continue;

		if (trip_point == DISABLE_THERMAL_HW_REBOOT) {
			disable_hw_reboot_interrupt(lvts_data, i);
			continue;
		}

		set_tc_hw_reboot_threshold(lvts_data, trip_point, i);
	}
}

static void update_all_tc_hw_reboot_point(struct lvts_data *lvts_data,
	int trip_point)
{
	struct tc_settings *tc = lvts_data->tc;
	int i;

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].hw_reboot_trip_point = trip_point;
}

static int soc_temp_lvts_set_trip_temp(void *data, int trip, int temp)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *) data;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;
	const struct thermal_trip *trip_points;

	trip_points = of_thermal_get_trip_points(lvts_data->tz_dev);
	if (!trip_points)
		return -EINVAL;

	if (trip_points[trip].type != THERMAL_TRIP_CRITICAL || lvts_tz->id != 0)
		return 0;

	update_all_tc_hw_reboot_point(lvts_data, temp);
	set_all_tc_hw_reboot(lvts_data);

	return 0;
}

static const struct thermal_zone_of_device_ops soc_temp_lvts_ops = {
	.get_temp = soc_temp_lvts_read_temp,
	.set_trip_temp = soc_temp_lvts_set_trip_temp,
};

static bool lvts_lk_init_check(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;
        bool ret = false;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		/* Check LVTS device ID */
		data = (readl(LVTSSPARE0_0 + base) & GENMASK(11, 0));

		if (data == LK_LVTS_MAGIC) {
			writel(0x0, LVTSSPARE0_0 + base);
                        ret = true;
		} else {
                        dev_info(dev, "%s, %d\n", __func__, i);
                	ret = false;
                        break;
                }
	}
        return ret;
}

static int read_calibration_data(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, s_index;
	void __iomem *base;
        struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;



	cal_data->efuse_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->efuse_data), GFP_KERNEL);
	if (!cal_data->efuse_data)
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		for (j = 0; j < tc[i].num_sensor; j++) {
                        s_index = tc[i].sensor_map[j];

                        cal_data->efuse_data[s_index] =
                                readl(LVTSEDATA00_0 + base + 0x4 * j);

			dev_info(dev, "%s,efuse_data: 0x%x\n", __func__,
                                cal_data->efuse_data[s_index] );
		}
	}

	return 0;
}


static int lvts_init(struct lvts_data *lvts_data)
{
	struct platform_ops *ops = &lvts_data->ops;
	struct device *dev = lvts_data->dev;
	int ret;
        bool lk_init = false;

	ret = clk_prepare_enable(lvts_data->clk);
	if (ret)
		dev_err(dev,
			"Error: Failed to enable lvts controller clock: %d\n",
			ret);

	lk_init = lvts_lk_init_check(lvts_data);

	if (lk_init == true) {
		ret = read_calibration_data(lvts_data);
        	set_all_tc_hw_reboot(lvts_data);

#ifdef DUMP_MORE_LOG
		clear_lvts_register_value_array(lvts_data);
		read_controller_reg_before_active(lvts_data);
#endif
                lvts_data->init_done = true;
				dev_info(dev, "%s, LK init LVTS\n", __func__);

                return ret;
	}

	lvts_reset(lvts_data);

	if (ops->device_identification)
		ops->device_identification(lvts_data);

	if (ops->device_enable_and_init)
		ops->device_enable_and_init(lvts_data);

	if (IS_ENABLE(FEATURE_DEVICE_AUTO_RCK)) {
		if (ops->device_enable_auto_rck)
			ops->device_enable_auto_rck(lvts_data);
	} else {
		if (ops->device_read_count_rc_n)
			ops->device_read_count_rc_n(lvts_data);
	}

	if (ops->set_cal_data)
		ops->set_cal_data(lvts_data);

	disable_all_sensing_points(lvts_data);
	wait_all_tc_sensing_point_idle(lvts_data);
	if (ops->init_controller)
		ops->init_controller(lvts_data);

#ifdef DUMP_MORE_LOG
	clear_lvts_register_value_array(lvts_data);
	read_controller_reg_before_active(lvts_data);
        read_device_reg_before_active(lvts_data);
#endif

	enable_all_sensing_points(lvts_data);

        set_all_tc_hw_reboot(lvts_data);

        lvts_data->init_done = true;

	return 0;
}


static void check_cal_data_v1(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i;

	cal_data->use_fake_efuse = 1;
	if (cal_data->golden_temp != 0) {
		cal_data->use_fake_efuse = 0;
	} else {
		for (i = 0; i < lvts_data->num_sensor; i++) {
			if (cal_data->count_r[i] != 0 ||
				cal_data->count_rc[i] != 0) {
				cal_data->use_fake_efuse = 0;
				break;
			}
		}
	}

	if (cal_data->use_fake_efuse) {
		/* It means all efuse data are equal to 0 */
		dev_err(dev,
			"[lvts_cal] This sample is not calibrated, fake !!\n");

		cal_data->golden_temp = cal_data->default_golden_temp;
		for (i = 0; i < lvts_data->num_sensor; i++) {
			cal_data->count_r[i] = cal_data->default_count_r;
			cal_data->count_rc[i] = cal_data->default_count_rc;
		}
	}

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].coeff.golden_temp = cal_data->golden_temp;

}

static int prepare_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct platform_ops *ops = &lvts_data->ops;
	struct tc_settings *tc = lvts_data->tc;
	int i, offset, size;
	char buffer[512];

	cal_data->count_r = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_r), GFP_KERNEL);
	if (!cal_data->count_r)
		return -ENOMEM;

	cal_data->count_rc = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_rc), GFP_KERNEL);
	if (!cal_data->count_rc)
		return -ENOMEM;

	if (ops->efuse_to_cal_data)
		ops->efuse_to_cal_data(lvts_data);

	if (ops->check_cal_data)
		ops->check_cal_data(lvts_data);

	dev_info(dev, "[lvts_cal] cali_mode = %d\n", cal_data->cali_mode);

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[lvts_cal] num:g_count:g_count_rc ");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d:%d ",
				i, cal_data->count_r[i], cal_data->count_rc[i]);

	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	offset = snprintf(buffer, size, "[lvts_cal] num_tc:g_golden_temp");
	for (i = 0; i < lvts_data->num_tc; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, tc[i].coeff.golden_temp);

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

static int get_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	char cell_name[8] = "e_data0";
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len = 0;
	int i, j, index = 0, ret;

	lvts_data->efuse = devm_kcalloc(dev, lvts_data->num_efuse_addr,
				sizeof(*lvts_data->efuse), GFP_KERNEL);
	if (!lvts_data->efuse)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_efuse_block; i++) {
		cell_name[6] = '1' + i;
		cell = nvmem_cell_get(dev, cell_name);
		if (IS_ERR(cell)) {
			dev_err(dev, "Error: Failed to get nvmem cell %s\n",
				cell_name);
			return PTR_ERR(cell);
		}

		buf = (u32 *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);

		if (IS_ERR(buf))
			return PTR_ERR(buf);

		for (j = 0; j < (len / sizeof(u32)); j++) {
			if (index >= lvts_data->num_efuse_addr) {
				dev_err(dev, "Array efuse is going to overflow");
				kfree(buf);
				return -EINVAL;
			}

			lvts_data->efuse[index] = buf[j];
			index++;
		}

		kfree(buf);
	}

	ret = prepare_calibration_data(lvts_data);

	return ret;
}

static int of_update_lvts_data(struct lvts_data *lvts_data,
	struct platform_device *pdev)
{
	struct device *dev = lvts_data->dev;
	struct power_domain *domain;
	struct resource *res;
	struct platform_ops *ops = &lvts_data->ops;
	unsigned int i;
	int ret;

	lvts_data->clk = devm_clk_get(dev, "lvts_clk");
	if (IS_ERR(lvts_data->clk))
		return PTR_ERR(lvts_data->clk);

	domain = devm_kcalloc(dev, lvts_data->num_domain, sizeof(*domain),
			GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_domain; i++) {
		/* Get base address */
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev, "No IO resource, index %d\n", i);
			return -ENXIO;
		}

		domain[i].base = devm_ioremap_resource(dev, res);
		if (IS_ERR(domain[i].base)) {
			dev_err(dev, "Failed to remap io, index %d\n", i);
			return PTR_ERR(domain[i].base);
		}

		/* Get interrupt number */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			dev_err(dev, "No irq resource, index %d\n", i);
			return -EINVAL;
		}
		domain[i].irq_num = res->start;

		/* Get reset control */
		domain[i].reset = devm_reset_control_get_by_index(dev, i);
		if (IS_ERR(domain[i].reset)) {
			dev_err(dev, "Failed to get, index %d\n", i);
			return PTR_ERR(domain[i].reset);
		}
	}

	lvts_data->domain = domain;

	lvts_data->sen_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*lvts_data->sen_data), GFP_KERNEL);
	if (!lvts_data->sen_data)
		return -ENOMEM;

	ret = get_calibration_data(lvts_data);
	if (ret)
		return ret;

	if (ops->update_coef_data)
		ops->update_coef_data(lvts_data);

	return 0;
}

static void lvts_device_close(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);
		writel(DISABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);
	}
}

static void lvts_close(struct lvts_data *lvts_data)
{
	disable_all_sensing_points(lvts_data);
	wait_all_tc_sensing_point_idle(lvts_data);
	lvts_device_close(lvts_data);
	clk_disable_unprepare(lvts_data->clk);
}

static void tc_irq_handler(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	unsigned int ret = 0;
	void __iomem *base;
        int temp;

#ifdef DUMP_MORE_LOG
	temp = lvts_read_all_tc_temperature(lvts_data);
        dump_lvts_error_info(lvts_data);
#endif

	base = GET_BASE_ADDR(tc_id);

	ret = readl(LVTSMONINTSTS_0 + base);
	/* Write back to clear interrupt status */
	writel(ret, LVTSMONINTSTS_0 + base);

	dev_info(dev, "[Thermal IRQ] LVTS thermal controller %d, LVTSMONINTSTS=0x%08x, T=%d\n",
		tc_id, ret, temp);

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset0 interrupt triggered, Thermal sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset1 interrupt triggered, Thermal sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset2 interrupt triggered, Thermal sw reset\n");
	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Thermal high offset3 interrupt triggered, Thermal sw reset\n");


	if (ret & THERMAL_PROTECTION_STAGE_3)
		dev_info(dev, "[Thermal IRQ]: Thermal protection stage 3 interrupt triggered, Thermal HW reboot\n");

	BUG();
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct lvts_data *lvts_data = (struct lvts_data *) dev_id;
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_domain; i++) {
		base = lvts_data->domain[i].base;
		lvts_data->irq_bitmap[i] = readl(THERMINTST + base);
		dev_info(dev, "%s : THERMINTST = 0x%x\n", __func__, lvts_data->irq_bitmap[i]);
	}

	for (i = 0; i < lvts_data->num_tc; i++) {
		if ((lvts_data->irq_bitmap[tc[i].domain_index] & tc[i].irq_bit) == 0)
			tc_irq_handler(lvts_data, i);
	}

	return IRQ_HANDLED;
}

static int lvts_register_irq_handler(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, *irq_bitmap;
	int ret;

	irq_bitmap = devm_kcalloc(dev, lvts_data->num_domain, sizeof(*irq_bitmap),
			GFP_KERNEL);
	if (!irq_bitmap) {
		dev_err(dev, "Error: Failed to allocate memory for irq_bitmap\n");
		return -ENOMEM;
	}
	lvts_data->irq_bitmap = irq_bitmap;

	for (i = 0; i < lvts_data->num_domain; i++) {
		ret = devm_request_irq(dev, lvts_data->domain[i].irq_num,
			irq_handler, IRQF_TRIGGER_HIGH, "mtk_lvts", lvts_data);

		if (ret) {
			dev_err(dev,
				"Failed to register LVTS IRQ, ret %d, domain %d irq_num %d\n",
				ret, i, lvts_data->domain[i].irq_num);
			lvts_close(lvts_data);
			return ret;
		}
	}

	return 0;
}

static int lvts_register_thermal_zones(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct thermal_zone_device *tzdev;
	struct soc_temp_tz *lvts_tz;
	int i, ret;

	for (i = 0; i < lvts_data->num_sensor + 1; i++) {
		lvts_tz = devm_kzalloc(dev, sizeof(*lvts_tz), GFP_KERNEL);
		if (!lvts_tz) {
			lvts_close(lvts_data);
			return -ENOMEM;
		}

		lvts_tz->id = i;
		lvts_tz->lvts_data = lvts_data;

		tzdev = devm_thermal_zone_of_sensor_register(dev, lvts_tz->id,
				lvts_tz, &soc_temp_lvts_ops);

		if (IS_ERR(tzdev)) {
			ret = PTR_ERR(tzdev);
			dev_err(dev,
				"Error: Failed to register lvts tz %d, ret = %d\n",
				lvts_tz->id, ret);
			lvts_close(lvts_data);
			return ret;
		}

		if (i == 0)
			lvts_data->tz_dev = tzdev;
	}

	return 0;
}

static int lvts_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lvts_data *lvts_data;
	int ret;

	lvts_data = (struct lvts_data *) of_device_get_match_data(dev);

	if (!lvts_data)	{
		dev_err(dev, "Error: Failed to get lvts platform data\n");
		return -ENODATA;
	}

	lvts_data->dev = &pdev->dev;

	ret = of_update_lvts_data(lvts_data, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, lvts_data);

	ret = lvts_init(lvts_data);
	if (ret)
		return ret;

	ret = lvts_register_irq_handler(lvts_data);
	if (ret)
		return ret;

	mutex_init(&lvts_data->sen_data_lock);

	ret = lvts_register_thermal_zones(lvts_data);
	if (ret)
		return ret;

	return 0;
}

static int lvts_remove(struct platform_device *pdev)
{
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

	lvts_close(lvts_data);

	return 0;
}

static int lvts_suspend_noirq(struct device *dev)
{
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) dev_get_drvdata(dev);
	dev_info(dev, "[Thermal/LVTS]%s\n", __func__);

	lvts_close(lvts_data);

	return 0;
}

static int lvts_resume_noirq(struct device *dev)
{
	int ret;
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) dev_get_drvdata(dev);
	dev_info(dev, "[Thermal/LVTS]%s\n", __func__);


	ret = lvts_init(lvts_data);
	if (ret)
		return ret;

	return 0;
}
/*==================================================
 * LVTS v4 common code
 *==================================================
 */
#define STOP_COUNTING_V4 (DEVICE_WRITE | RG_TSFM_CTRL_0 << 8 | 0x00)
#define SET_RG_TSFM_LPDLY_V4 (DEVICE_WRITE | RG_TSFM_CTRL_4 << 8 | 0xA6)
#define SET_COUNTING_WINDOW_20US1_V4 (DEVICE_WRITE | RG_TSFM_CTRL_2 << 8 | 0x00)
#define SET_COUNTING_WINDOW_20US2_V4 (DEVICE_WRITE | RG_TSFM_CTRL_1 << 8 | 0x20)
#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0x84)
#define TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_4 << 8	\
						| 0x7C)
#define SET_TS_RSV_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_1 << 8 | 0x8D)
#define SET_TS_EN_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF4)
#define TOGGLE_RG_TSV2F_VCO_RST1_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xFC)
#define TOGGLE_RG_TSV2F_VCO_RST2_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF4)

#define SET_LVTS_AUTO_RCK_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x01)
#define SELECT_SENSOR_RCK_V4(id) (DEVICE_WRITE | RG_TSV2F_CTRL_5 << 8 | id)
#define SET_DEVICE_SINGLE_MODE_V4 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8 | 0x78)
#define KICK_OFF_RCK_COUNTING_V4 (DEVICE_WRITE | RG_TSFM_CTRL_0 << 8 | 0x02)
#define SET_SENSOR_NO_RCK_V4 (DEVICE_WRITE | RG_TSV2F_CTRL_5 << 8 | 0x10)
#define SET_DEVICE_LOW_POWER_SINGLE_MODE_V4 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8	\
						| 0xB8)
static void device_enable_and_init_v4(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++) {
		lvts_write_device(lvts_data, STOP_COUNTING_V4, i);
		lvts_write_device(lvts_data, SET_RG_TSFM_LPDLY_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US2_V4, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V4, i);
		lvts_write_device(lvts_data, TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V4, i);
		lvts_write_device(lvts_data, SET_TS_RSV_V4, i);
		lvts_write_device(lvts_data, SET_TS_EN_V4, i);
		lvts_write_device(lvts_data, TOGGLE_RG_TSV2F_VCO_RST1_V4, i);
		lvts_write_device(lvts_data, TOGGLE_RG_TSV2F_VCO_RST2_V4, i);
	}

	lvts_data->counting_window_us = 20;
}

#define TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V5 (DEVICE_WRITE | RG_TSV2F_CTRL_4 << 8	\
						| 0xFC)
#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V5 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0x8C)
#define SET_TS_CHOP_CTRL_V5 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF1)

static void device_enable_and_init_v5(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++) {
		lvts_write_device(lvts_data, STOP_COUNTING_V4, i);//0300
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US2_V4, i); //0420
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i); //0500
		lvts_write_device(lvts_data, SET_RG_TSFM_LPDLY_V4, i);//07A6
		lvts_write_device(lvts_data, TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V5, i);//0CFC
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_V5, i);//0A8C
		lvts_write_device(lvts_data, SET_TS_RSV_V4, i); //098D
		lvts_write_device(lvts_data, SET_TS_CHOP_CTRL_V5, i);//08F1
	}

	lvts_data->counting_window_us = 20;
}

static void device_enable_auto_rck_v4(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++)
		lvts_write_device(lvts_data, SET_LVTS_AUTO_RCK_V4, i);
}

static int device_read_count_rc_n_v4(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int offset, size, s_index, data;
	void __iomem *base;
	int ret, i, j;
	char buffer[512];


	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++) {
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
			lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
                }
		return 0;
	}


	cal_data->count_rc_now = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(j), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V4, i);
			udelay(10);

			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			udelay(30);

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_err(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n",
					i);

			data = lvts_read_device(lvts_data, 0x00, i);

			cal_data->count_rc_now[s_index] = (data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
	}

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

#define SET_DEVICE_SINGLE_MODE_V5 (DEVICE_WRITE | RG_TSFM_CTRL_3 << 8 | 0xf8)

#define SET_LVTS_MANUAL_RCK_OPERATION_V5 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x00)

#define SET_TS_DIS_V5 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF1)

static int device_read_count_rc_n_v5(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int offset, size, s_index, data;
	void __iomem *base;
	int ret, i, j;
	char buffer[512];
	unsigned int  rc_data;
	int refine_data_idx[4] = {0};
	int count_rc_delta = 0;


	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++) {
			lvts_write_device(lvts_data, SET_LVTS_MANUAL_RCK_OPERATION_V5, i);//0E00
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);//0d10
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V5, i);//06f8
		}
		return 0;
	}

	cal_data->count_rc_now =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_write_device(lvts_data, SET_LVTS_MANUAL_RCK_OPERATION_V5, i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			refine_data_idx[j] = 0xff;
			s_index = tc[i].sensor_map[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(j), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V5, i);

			lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i);
			lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US2_V4, i);


			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			udelay(50);

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n", i);

			/* Get RCK count data (sensor-N) */
			data = lvts_read_device(lvts_data, 0x00, i);
			udelay(5);

			rc_data = (data & GENMASK(23, 0));

			if (rc_data == 0) {
				refine_data_idx[j] = s_index;
				dev_info(dev, "rc_data %d, data_idx[%d]=%d\n", rc_data, j,
					s_index);
			} else {
				if (cal_data->count_rc[i] > rc_data)
					count_rc_delta = (cal_data->count_rc[i] * 1000) / rc_data;
				else
					count_rc_delta = (rc_data * 1000) / cal_data->count_rc[i];

				if (count_rc_delta > 1061) {
					refine_data_idx[j] = s_index;
					dev_info(dev,
						"delta %d, data_idx[%d]=%d\n",
						count_rc_delta, j, s_index);
				}
			}



			if (refine_data_idx[j] != 0xff) {
				dev_info(dev, "refine_data_idx[%d]=%d\n", j, refine_data_idx[j]);
				cal_data->count_rc_now[s_index] = cal_data->count_rc[i];
			} else
				cal_data->count_rc_now[s_index] = (rc_data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		/* Select Sensor-N without RCK */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
	}


	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

static void set_calibration_data_v4(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int i, j, s_index, e_data;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];
			if (IS_ENABLE(FEATURE_DEVICE_AUTO_RCK))
				e_data = cal_data->count_r[s_index];
			else
				e_data = cal_data->efuse_data[s_index];

			writel(e_data, LVTSEDATA00_0 + base + 0x4 * j);
		}
	}
}

static unsigned int set_sensor_idx(struct lvts_data *lvts_data, int tc_id)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i;
	bool all_device_id_is_zero = true;
	unsigned int sensor_idx = 0x0;

	for (i = 0; i < tc[tc_id].num_sensor; i++) {
		if (tc[tc_id].device_id[i] != 0) {
			all_device_id_is_zero = false;
			break;
		}
	}

	if (all_device_id_is_zero)
		return SET_SENSOR_INDEX;

	for (i = 0; i < ALL_SENSING_POINTS; i++) {
		if (tc[tc_id].device_id[i] >= ALL_SENSING_POINTS)
			return SET_SENSOR_INDEX;
		if (i >= tc[tc_id].num_sensor)
			sensor_idx = sensor_idx + (0x10 << 8*i);
		else
			sensor_idx = sensor_idx + ((0x10 + tc[tc_id].device_id[i]) << 8*i);
	}

	return sensor_idx;
}

static void init_controller_v4(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;
	unsigned int sensor_idx = 0x0;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
		sensor_idx = set_sensor_idx(lvts_data, i);
		writel(sensor_idx, LVTSTSSEL_0 + base);
		writel(SET_CALC_SCALE_RULES, LVTSCALSCALE_0 + base);

		set_polling_speed(lvts_data, i);
		set_hw_filter(lvts_data, i);
	}
}

/*==================================================
 * LVTS MT6873
 *==================================================
 */

#define MT6873_NUM_LVTS (ARRAY_SIZE(mt6873_tc_settings))
#define MT6853_NUM_LVTS (ARRAY_SIZE(mt6853_tc_settings))

enum mt6873_lvts_domain {
	MT6873_AP_DOMAIN,
	MT6873_MCU_DOMAIN,
	MT6873_NUM_DOMAIN
};

enum mt6853_lvts_domain {
	MT6853_AP_DOMAIN,
	MT6853_MCU_DOMAIN,
	MT6853_NUM_DOMAIN
};

enum mt6873_lvts_sensor_enum {
	MT6873_TS1_0,
	MT6873_TS1_1,
	MT6873_TS2_0,
	MT6873_TS2_1,
	MT6873_TS3_0,
	MT6873_TS3_1,
	MT6873_TS3_2,
	MT6873_TS3_3,
	MT6873_TS4_0,
	MT6873_TS4_1,
	MT6873_TS5_0,
	MT6873_TS5_1,
	MT6873_TS6_0,
	MT6873_TS6_1,
	MT6873_TS7_0,
	MT6873_TS7_1,
	MT6873_TS7_2,
	MT6873_NUM_TS
};

enum mt6853_lvts_sensor_enum {
	MT6853_TS1_0,
	MT6853_TS1_1,
	MT6853_TS1_2,
	MT6853_TS1_3,
	MT6853_TS2_0,
	MT6853_TS2_1,
	MT6853_TS3_0,
	MT6853_TS3_1,
	MT6853_TS3_2,
	MT6853_TS3_3,
	MT6853_TS4_0,
	MT6853_TS5_0,
	MT6853_TS5_1,
	MT6853_TS5_2,
	MT6853_NUM_TS
};


static void mt6873_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;

	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 31, 24);
	cal_data->count_r[MT6873_TS1_0] = GET_CAL_DATA_BITMASK(1, 23, 0);
	cal_data->count_r[MT6873_TS1_1] = GET_CAL_DATA_BITMASK(2, 23, 0);
	cal_data->count_r[MT6873_TS2_0] = GET_CAL_DATA_BITMASK(3, 23, 0);
	cal_data->count_r[MT6873_TS2_1] = GET_CAL_DATA_BITMASK(4, 23, 0);
	cal_data->count_r[MT6873_TS3_0] = GET_CAL_DATA_BITMASK(5, 23, 0);
	cal_data->count_r[MT6873_TS3_1] = GET_CAL_DATA_BITMASK(6, 23, 0);
	cal_data->count_r[MT6873_TS3_2] = GET_CAL_DATA_BITMASK(7, 23, 0);
	cal_data->count_r[MT6873_TS3_3] = GET_CAL_DATA_BITMASK(8, 23, 0);
	cal_data->count_r[MT6873_TS4_0] = GET_CAL_DATA_BITMASK(9, 23, 0);
	cal_data->count_r[MT6873_TS4_1] = GET_CAL_DATA_BITMASK(10, 23, 0);
	cal_data->count_r[MT6873_TS5_0] = GET_CAL_DATA_BITMASK(11, 23, 0);
	cal_data->count_r[MT6873_TS5_1] = GET_CAL_DATA_BITMASK(12, 23, 0);
	cal_data->count_r[MT6873_TS6_0] = GET_CAL_DATA_BITMASK(13, 23, 0);
	cal_data->count_r[MT6873_TS6_1] = GET_CAL_DATA_BITMASK(14, 23, 0);
	cal_data->count_r[MT6873_TS7_0] = GET_CAL_DATA_BITMASK(15, 23, 0);
	cal_data->count_r[MT6873_TS7_1] = GET_CAL_DATA_BITMASK(16, 23, 0);
	cal_data->count_r[MT6873_TS7_2] = GET_CAL_DATA_BITMASK(17, 23, 0);

	cal_data->count_rc[MT6873_TS1_0] = GET_CAL_DATA_BITMASK(21, 23, 0);

	cal_data->count_rc[MT6873_TS2_0] = (GET_CAL_DATA_BITMASK(1, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(2, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(3, 31, 24);

	cal_data->count_rc[MT6873_TS3_0] = (GET_CAL_DATA_BITMASK(4, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(5, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(6, 31, 24);

	cal_data->count_rc[MT6873_TS4_0] = (GET_CAL_DATA_BITMASK(7, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(8, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(9, 31, 24);

	cal_data->count_rc[MT6873_TS5_0] = (GET_CAL_DATA_BITMASK(10, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(11, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(12, 31, 24);

	cal_data->count_rc[MT6873_TS6_0] = (GET_CAL_DATA_BITMASK(13, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(14, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(15, 31, 24);

	cal_data->count_rc[MT6873_TS7_0] = (GET_CAL_DATA_BITMASK(16, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(17, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(18, 31, 24);
}

static void mt6853_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;

	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 31, 24);

	cal_data->count_r[MT6853_TS1_0] = GET_CAL_DATA_BITMASK(1, 23, 0);
	cal_data->count_r[MT6853_TS1_1] = GET_CAL_DATA_BITMASK(2, 23, 0);
	cal_data->count_r[MT6853_TS1_2] = GET_CAL_DATA_BITMASK(3, 23, 0);
	cal_data->count_r[MT6853_TS1_3] = GET_CAL_DATA_BITMASK(4, 23, 0);
	cal_data->count_r[MT6853_TS2_0] = GET_CAL_DATA_BITMASK(5, 23, 0);
	cal_data->count_r[MT6853_TS2_1] = GET_CAL_DATA_BITMASK(6, 23, 0);
	cal_data->count_r[MT6853_TS3_0] = GET_CAL_DATA_BITMASK(7, 23, 0);
	cal_data->count_r[MT6853_TS3_1] = GET_CAL_DATA_BITMASK(8, 23, 0);
	cal_data->count_r[MT6853_TS3_2] = GET_CAL_DATA_BITMASK(9, 23, 0);
	cal_data->count_r[MT6853_TS3_3] = GET_CAL_DATA_BITMASK(10, 23, 0);
	cal_data->count_r[MT6853_TS4_0] = GET_CAL_DATA_BITMASK(11, 23, 0);
	cal_data->count_r[MT6853_TS5_0] = GET_CAL_DATA_BITMASK(12, 23, 0);
	cal_data->count_r[MT6853_TS5_1] = GET_CAL_DATA_BITMASK(13, 23, 0);
	cal_data->count_r[MT6853_TS5_2] = GET_CAL_DATA_BITMASK(14, 23, 0);

	cal_data->count_rc[MT6853_TS1_0] = GET_CAL_DATA_BITMASK(18, 23, 0);
	cal_data->count_rc[MT6853_TS2_0] = GET_CAL_DATA_BITMASK(19, 23, 0);
	cal_data->count_rc[MT6853_TS3_0] = GET_CAL_DATA_BITMASK(20, 23, 0);
	cal_data->count_rc[MT6853_TS4_0] = GET_CAL_DATA_BITMASK(21, 23, 0);
	cal_data->count_rc[MT6853_TS5_0] = (GET_CAL_DATA_BITMASK(1, 31, 24) << 16) +
			(GET_CAL_DATA_BITMASK(2, 31, 24) << 8) +
			GET_CAL_DATA_BITMASK(3, 31, 24);
}


static struct tc_settings mt6873_tc_settings[] = {
	[0] = {
		.domain_index = MT6873_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6873_TS1_0, MT6873_TS1_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[1] = {
		.domain_index = MT6873_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6873_TS2_0, MT6873_TS2_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[2] = {
		.domain_index = MT6873_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6873_TS3_0, MT6873_TS3_1, MT6873_TS3_2, MT6873_TS3_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(5),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[3] = {
		.domain_index = MT6873_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6873_TS4_0, MT6873_TS4_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[4] = {
		.domain_index = MT6873_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6873_TS5_0, MT6873_TS5_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[5] = {
		.domain_index = MT6873_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 2,
		.sensor_map = {MT6873_TS6_0, MT6873_TS6_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(5),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[6] = {
		.domain_index = MT6873_AP_DOMAIN,
		.addr_offset = 0x300,
		.num_sensor = 3,
		.sensor_map = {MT6873_TS7_0, MT6873_TS7_1, MT6873_TS7_2},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(6),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	}
};


static struct tc_settings mt6853_tc_settings[] = {
	[0] = {
		.domain_index = MT6853_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT6853_TS1_0, MT6853_TS1_1, MT6853_TS1_2, MT6853_TS1_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[1] = {
		.domain_index = MT6853_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6853_TS2_0, MT6853_TS2_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[2] = {
		.domain_index = MT6853_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT6853_TS3_0, MT6853_TS3_1, MT6853_TS3_2, MT6853_TS3_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[3] = {
		.domain_index = MT6853_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 1,
		.sensor_map = {MT6853_TS4_0},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[4] = {
		.domain_index = MT6853_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 3,
		.sensor_map = {MT6853_TS5_0, MT6853_TS5_1, MT6853_TS5_2},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 117000,
		.irq_bit = BIT(5),
		.coeff = {
			.a = {-250460},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	}
};


static struct lvts_data mt6873_lvts_data = {
	.num_domain = MT6873_NUM_DOMAIN,
	.num_tc = MT6873_NUM_LVTS,
	.tc = mt6873_tc_settings,
	.num_sensor = MT6873_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt6873_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v4,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v4,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK,
	.num_efuse_addr = 22,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 0,
};

static struct lvts_data mt6853_lvts_data = {
	.num_domain = MT6853_NUM_DOMAIN,
	.num_tc = MT6853_NUM_LVTS,
	.tc = mt6853_tc_settings,
	.num_sensor = MT6853_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt6853_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v4,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v4,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK,
	.num_efuse_addr = 17,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 0,
};

/*==================================================
 * LVTS MT6893
 *==================================================
 */
#define SET_TS_EN_6893 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8	\
						| 0xF5)
#define SET_TS_DIS_6893 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8	\
						| 0xF1)
#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6893 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0xC4)
#define SET_LANTENCY_6893 (DEVICE_WRITE | RG_TSFM_CTRL_4 << 8	\
						| 0xA6)
#define SET_LVTS_MANUAL_RCK_OPERATION_6893 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x00)

static void mt6893_device_enable_and_init(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++) {
		lvts_write_device(lvts_data, STOP_COUNTING_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US2_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6893, i);
		lvts_write_device(lvts_data, TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_V4, i);
		lvts_write_device(lvts_data, SET_TS_RSV_V4, i);
		lvts_write_device(lvts_data, SET_TS_EN_6893, i);
		lvts_write_device(lvts_data, TOGGLE_RG_TSV2F_VCO_RST1_V4, i);
		lvts_write_device(lvts_data, SET_TS_EN_6893, i);
		lvts_write_device(lvts_data, SET_LANTENCY_6893, i);
	}

	lvts_data->counting_window_us = 20;
}


static int mt6893_device_read_count_rc_n(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int offset, size, s_index, data;
	void __iomem *base;
	int ret, i, j;
	char buffer[512];
	unsigned int  rc_data;
	int refine_data_idx[4] = {0};
	int count_rc_delta = 0;

	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++)
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);

		return 0;
	}


	cal_data->count_rc_now =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_write_device(lvts_data, SET_LVTS_MANUAL_RCK_OPERATION_6893, i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			refine_data_idx[j] = 0xff;
			s_index = tc[i].sensor_map[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(j), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V4, i);
			lvts_write_device(lvts_data, SET_RG_TSFM_LPDLY_V4, i);
			lvts_write_device(lvts_data, SET_TS_EN_6893, i);
			lvts_write_device(lvts_data, TOGGLE_RG_TSV2F_VCO_RST1_V4, i);
			lvts_write_device(lvts_data, SET_TS_EN_6893, i);
			udelay(10);

			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			udelay(30);

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n", i);

			data = lvts_read_device(lvts_data, 0x00, i);
			udelay(5);

			rc_data = (data & GENMASK(23, 0));

			if (rc_data == 0) {
				refine_data_idx[j] = s_index;
				dev_info(dev, "rc_data %d, data_idx[%d]=%d\n", rc_data, j,
					s_index);
			} else {
				if (cal_data->count_rc[i] > rc_data)
					count_rc_delta = (cal_data->count_rc[i] * 1000) / rc_data;
				else
					count_rc_delta = (rc_data * 1000) / cal_data->count_rc[i];

				if (count_rc_delta > 1061) {
					refine_data_idx[j] = s_index;
					dev_info(dev,
						"delta %d, data_idx[%d]=%d\n",
						count_rc_delta, j, s_index);
				}
			}

			lvts_write_device(lvts_data, SET_TS_DIS_6893, i);

			if (refine_data_idx[j] != 0xff) {
				dev_info(dev, "refine_data_idx[%d]=%d\n", j, refine_data_idx[j]);
				cal_data->count_rc_now[s_index] = cal_data->count_rc[i];
			} else
				cal_data->count_rc_now[s_index] = (rc_data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
	}


	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);
	if (offset < 0)
		return -EINVAL;
	if (offset >= size)
		return -ENOMEM;
	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

#define MT6893_NUM_LVTS (ARRAY_SIZE(mt6893_tc_settings))

enum mt6893_lvts_domain {
	MT6893_AP_DOMAIN,
	MT6893_MCU_DOMAIN,
	MT6893_NUM_DOMAIN
};

enum mt6893_lvts_sensor_enum {
	MT6893_TS1_0,
	MT6893_TS1_1,
	MT6893_TS2_0,
	MT6893_TS2_1,
	MT6893_TS3_0,
	MT6893_TS3_1,
	MT6893_TS3_2,
	MT6893_TS3_3,
	MT6893_TS4_0,
	MT6893_TS4_1,
	MT6893_TS5_0,
	MT6893_TS5_1,
	MT6893_TS6_0,
	MT6893_TS6_1,
	MT6893_TS7_0,
	MT6893_TS7_1,
	MT6893_TS7_2,
	MT6893_NUM_TS

};

enum mt6893_lvts_controller_enum {
	MT6893_LVTS_MCU_CTRL0,
	MT6893_LVTS_MCU_CTRL1,
	MT6893_LVTS_MCU_CTRL2,
	MT6893_LVTS_AP_CTRL0,
	MT6893_LVTS_AP_CTRL1,
	MT6893_LVTS_AP_CTRL2,
	MT6893_LVTS_AP_CTRL3,
	MT6893_LVTS_CTRL_NUM
};

static void mt6893_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;

	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 31, 24);
	cal_data->count_r[MT6893_TS1_0] = GET_CAL_DATA_BITMASK(1, 23, 0);
	cal_data->count_r[MT6893_TS1_1] = GET_CAL_DATA_BITMASK(2, 23, 0);
	cal_data->count_r[MT6893_TS2_0] = GET_CAL_DATA_BITMASK(3, 23, 0);
	cal_data->count_r[MT6893_TS2_1] = GET_CAL_DATA_BITMASK(4, 23, 0);
	cal_data->count_r[MT6893_TS3_0] = GET_CAL_DATA_BITMASK(5, 23, 0);
	cal_data->count_r[MT6893_TS3_1] = GET_CAL_DATA_BITMASK(6, 23, 0);
	cal_data->count_r[MT6893_TS3_2] = GET_CAL_DATA_BITMASK(7, 23, 0);
	cal_data->count_r[MT6893_TS3_3] = GET_CAL_DATA_BITMASK(8, 23, 0);
	cal_data->count_r[MT6893_TS4_0] = GET_CAL_DATA_BITMASK(9, 23, 0);
	cal_data->count_r[MT6893_TS4_1] = GET_CAL_DATA_BITMASK(10, 23, 0);
	cal_data->count_r[MT6893_TS5_0] = GET_CAL_DATA_BITMASK(11, 23, 0);
	cal_data->count_r[MT6893_TS5_1] = GET_CAL_DATA_BITMASK(12, 23, 0);
	cal_data->count_r[MT6893_TS6_0] = GET_CAL_DATA_BITMASK(13, 23, 0);
	cal_data->count_r[MT6893_TS6_1] = GET_CAL_DATA_BITMASK(14, 23, 0);
	cal_data->count_r[MT6893_TS7_0] = GET_CAL_DATA_BITMASK(15, 23, 0);
	cal_data->count_r[MT6893_TS7_1] = GET_CAL_DATA_BITMASK(16, 23, 0);
	cal_data->count_r[MT6893_TS7_2] = GET_CAL_DATA_BITMASK(17, 23, 0);

	cal_data->count_rc[MT6893_LVTS_MCU_CTRL0] = GET_CAL_DATA_BITMASK(21, 23, 0);

	cal_data->count_rc[MT6893_LVTS_MCU_CTRL1] = (GET_CAL_DATA_BITMASK(1, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(2, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(3, 31, 24);

	cal_data->count_rc[MT6893_LVTS_MCU_CTRL2] = (GET_CAL_DATA_BITMASK(4, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(5, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(6, 31, 24);

	cal_data->count_rc[MT6893_LVTS_AP_CTRL0] = (GET_CAL_DATA_BITMASK(7, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(8, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(9, 31, 24);

	cal_data->count_rc[MT6893_LVTS_AP_CTRL1] = (GET_CAL_DATA_BITMASK(10, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(11, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(12, 31, 24);

	cal_data->count_rc[MT6893_LVTS_AP_CTRL2] = (GET_CAL_DATA_BITMASK(13, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(14, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(15, 31, 24);

	cal_data->count_rc[MT6893_LVTS_AP_CTRL3] = (GET_CAL_DATA_BITMASK(16, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(17, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(18, 31, 24);

}

static struct tc_settings mt6893_tc_settings[] = {
	[MT6893_LVTS_MCU_CTRL0] = {
		.domain_index = MT6893_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6893_TS1_0, MT6893_TS1_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_MCU_CTRL1] = {
		.domain_index = MT6893_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6893_TS2_0, MT6893_TS2_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_MCU_CTRL2] = {
		.domain_index = MT6893_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6893_TS3_0, MT6893_TS3_1, MT6893_TS3_2, MT6893_TS3_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_AP_CTRL0] = {
		.domain_index = MT6893_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6893_TS4_0, MT6893_TS4_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_AP_CTRL1] = {
		.domain_index = MT6893_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6893_TS5_0, MT6893_TS5_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_AP_CTRL2] = {
		.domain_index = MT6893_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 2,
		.sensor_map = {MT6893_TS6_0, MT6893_TS6_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	},
	[MT6893_LVTS_AP_CTRL3] = {
		.domain_index = MT6893_AP_DOMAIN,
		.addr_offset = 0x300,
		.num_sensor = 3,
		.sensor_map = {MT6893_TS7_0, MT6893_TS7_1, MT6893_TS7_2},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_2_OF_4,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-252500},
			.golden_temp = 50,
			.cali_mode = CALI_NT,
		},
	}
};


static struct lvts_data mt6893_lvts_data = {
	.num_domain = MT6893_NUM_DOMAIN,
	.num_tc = MT6893_NUM_LVTS,
	.tc = mt6893_tc_settings,
	.num_sensor = MT6893_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt6893_efuse_to_cal_data,
		.device_enable_and_init = mt6893_device_enable_and_init,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = mt6893_device_read_count_rc_n,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = 0,
	.num_efuse_addr = 22,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 0,
};

/*==================================================
 * LVTS MT6983
 *==================================================
 */
#define SET_TS_DIS_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8	\
						| 0xF1)

#define SET_LVTS_MANUAL_RCK_OPERATION_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x00)

#define SET_COUNTING_WINDOW_47US2 (DEVICE_WRITE | RG_TSFM_CTRL_1 << 8 | 0x47)

#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6983_1 (DEVICE_WRITE | RG_TSV2F_CTRL_2 << 8	\
						| 0x30)

#define TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6983_2 (DEVICE_WRITE | RG_TSV2F_CTRL_3 << 8	\
						| 0x03)

#define TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_4 << 8	\
						| 0xFF)

#define SET_TS_RSV_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_1 << 8 | 0x94)
#define SET_TS_CHOP_CTRL_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xB1)
#define SET_TS_DIV_EN_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xF5)
#define SET_VCO_RST_6983 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xFD)
#define COF_A_T_SLP_GLD 199410
#define COF_A_COUNT_R_GLD 12052
#define COF_A_CONST_OFS 280000
#define COF_A_OFS (COF_A_T_SLP_GLD - COF_A_CONST_OFS)
#define COF_A_T_SLP_GLD_HT 254410
#define COF_A_COUNT_R_GLD_HT 15380
#define COF_A_CONST_OFS_HT 170000
#define COF_A_OFS_HT (COF_A_T_SLP_GLD_HT - COF_A_CONST_OFS_HT)

static void mt6983_device_enable_and_init(struct lvts_data *lvts_data)
{
	unsigned int i;

	for (i = 0; i < lvts_data->num_tc; i++) {
		lvts_write_device(lvts_data, STOP_COUNTING_V4, i);
		lvts_write_device(lvts_data, SET_RG_TSFM_LPDLY_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_20US1_V4, i);
		lvts_write_device(lvts_data, SET_COUNTING_WINDOW_47US2, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6983_1, i);
		lvts_write_device(lvts_data, TSV2F_CHOP_CKSEL_AND_TSV2F_EN_6983_2, i);
		lvts_write_device(lvts_data, TSBG_DEM_CKSEL_X_TSBG_CHOP_EN_6983, i);
		lvts_write_device(lvts_data, SET_TS_RSV_6983, i);
		lvts_write_device(lvts_data, SET_TS_CHOP_CTRL_6983, i);
	}

	lvts_data->counting_window_us = 20;
}

static int mt6983_device_read_count_rc_n(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int offset, size, s_index, data;
	void __iomem *base;
	int ret, i, j;
	char buffer[512];
	unsigned int  rc_data;
	int refine_data_idx[4] = {0};
	int count_rc_delta = 0;

	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++) {
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
			lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
		}
		return 0;
	}


	cal_data->count_rc_now =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_write_device(lvts_data, SET_LVTS_MANUAL_RCK_OPERATION_6983, i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			refine_data_idx[j] = 0xff;
			s_index = tc[i].sensor_map[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(j), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V4, i);
			lvts_write_device(lvts_data, SET_TS_DIV_EN_6983, i);
			lvts_write_device(lvts_data, SET_VCO_RST_6983, i);
			lvts_write_device(lvts_data, SET_TS_DIV_EN_6983, i);
			udelay(10);

			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n", i);

			lvts_write_device(lvts_data, SET_TS_DIS_6983, i);
			data = lvts_read_device(lvts_data, 0x00, i);
			udelay(5);

			rc_data = (data & GENMASK(23, 0));

			if (rc_data == 0) {
				refine_data_idx[j] = s_index;
				dev_info(dev, "rc_data %d, data_idx[%d]=%d\n", rc_data, j,
					s_index);
			} else {
				if (cal_data->count_rc[i] > rc_data)
					count_rc_delta = (cal_data->count_rc[i] * 1000) / rc_data;
				else
					count_rc_delta = (rc_data * 1000) / cal_data->count_rc[i];

				if (count_rc_delta > 1061) {
					refine_data_idx[j] = s_index;
					dev_info(dev,
						"delta %d, data_idx[%d]=%d\n",
						count_rc_delta, j, s_index);
				}
			}

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_ACCESS_STARTUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_ACCESS_START didn't ready\n",
					i);

			if (refine_data_idx[j] != 0xff) {
				dev_info(dev, "refine_data_idx[%d]=%d\n", j, refine_data_idx[j]);
				cal_data->count_rc_now[s_index] = cal_data->count_rc[i];
			} else
				cal_data->count_rc_now[s_index] = (rc_data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
	}


	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

#define MT6983_NUM_LVTS (ARRAY_SIZE(mt6983_tc_settings))

enum mt6983_lvts_domain {
	MT6983_AP_DOMAIN,
	MT6983_MCU_DOMAIN,
	MT6983_NUM_DOMAIN
};

enum mt6983_lvts_sensor_enum {
	MT6983_TS1_0,
	MT6983_TS1_1,
	MT6983_TS1_2,
	MT6983_TS1_3,
	MT6983_TS2_0,
	MT6983_TS2_1,
	MT6983_TS2_2,
	MT6983_TS2_3,
	MT6983_TS3_0,
	MT6983_TS3_1,
	MT6983_TS3_2,
	MT6983_TS3_3,
	MT6983_TS5_0,
	MT6983_TS5_1,
	MT6983_TS6_0,
	MT6983_TS6_1,
	MT6983_TS6_2,
	MT6983_TS6_3,
	MT6983_TS7_0,
	MT6983_TS7_1,
	MT6983_TS7_2,
	MT6983_TS8_0,
	MT6983_TS8_1,
	MT6983_TS8_2,
	MT6983_TS9_0,
	MT6983_TS9_1,
	MT6983_TS9_2,
	MT6983_NUM_TS

};

enum mt6983_lvts_controller_enum {
	MT6983_LVTS_MCU_CTRL0,
	MT6983_LVTS_MCU_CTRL1,
	MT6983_LVTS_MCU_CTRL2,
	MT6983_LVTS_AP_CTRL0,
	MT6983_LVTS_AP_CTRL1,
	MT6983_LVTS_AP_CTRL2,
	MT6983_LVTS_AP_CTRL3,
	MT6983_LVTS_AP_CTRL4,
	MT6983_LVTS_CTRL_NUM
};

static void mt6983_device_identification(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int lvts_dev_id;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		writel(ENABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);

		lvts_write_device(lvts_data, READ_BACK_DEVICE_ID, i);

		/* Check LVTS device ID */
		data = (readl(LVTS_ID_0 + base) & GENMASK(7, 0));

		if  (tc[i].domain_index == MT6983_AP_DOMAIN)
			lvts_dev_id = 0x82 + i;
		else
			lvts_dev_id = 0x81 + i;

		if (data != lvts_dev_id) {
			dev_err(dev, "LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				i, lvts_dev_id, data);
		}
	}
}

static void mt6983_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i = 0;

	cal_data->cali_mode = GET_CAL_DATA_BIT(0, 31);
	cal_data->golden_temp_ht = GET_CAL_DATA_BITMASK(0, 15, 8);
	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 7, 0);

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].coeff.golden_temp = cal_data->golden_temp;

	if (cal_data->cali_mode == 1) {
		for (i = 0; i < 3; i++) {
			tc[i].coeff.golden_temp = cal_data->golden_temp_ht;
			tc[i].coeff.cali_mode = CALI_HT;
		}
	}

	cal_data->count_r[MT6983_TS1_0] = GET_CAL_DATA_BITMASK(1, 15, 0);
	cal_data->count_r[MT6983_TS1_1] = GET_CAL_DATA_BITMASK(1, 31, 16);
	cal_data->count_r[MT6983_TS1_2] = GET_CAL_DATA_BITMASK(2, 15, 0);
	cal_data->count_r[MT6983_TS1_3] = GET_CAL_DATA_BITMASK(2, 31, 16);

	cal_data->count_r[MT6983_TS2_0] = GET_CAL_DATA_BITMASK(3, 15, 0);
	cal_data->count_r[MT6983_TS2_1] = GET_CAL_DATA_BITMASK(3, 31, 16);
	cal_data->count_r[MT6983_TS2_2] = GET_CAL_DATA_BITMASK(4, 15, 0);
	cal_data->count_r[MT6983_TS2_3] = GET_CAL_DATA_BITMASK(4, 31, 16);

	cal_data->count_r[MT6983_TS3_0] = GET_CAL_DATA_BITMASK(5, 15, 0);
	cal_data->count_r[MT6983_TS3_1] = GET_CAL_DATA_BITMASK(5, 31, 16);
	cal_data->count_r[MT6983_TS3_2] = GET_CAL_DATA_BITMASK(6, 15, 0);
	cal_data->count_r[MT6983_TS3_3] = GET_CAL_DATA_BITMASK(6, 31, 16);

	cal_data->count_r[MT6983_TS5_0] = GET_CAL_DATA_BITMASK(9, 15, 0);
	cal_data->count_r[MT6983_TS5_1] = GET_CAL_DATA_BITMASK(9, 31, 16);

	cal_data->count_r[MT6983_TS6_0] = GET_CAL_DATA_BITMASK(10, 15, 0);
	cal_data->count_r[MT6983_TS6_1] = GET_CAL_DATA_BITMASK(10, 31, 16);
	cal_data->count_r[MT6983_TS6_2] = GET_CAL_DATA_BITMASK(11, 15, 0);
	cal_data->count_r[MT6983_TS6_3] = GET_CAL_DATA_BITMASK(11, 31, 16);

	cal_data->count_r[MT6983_TS7_0] = GET_CAL_DATA_BITMASK(12, 15, 0);
	cal_data->count_r[MT6983_TS7_1] = GET_CAL_DATA_BITMASK(12, 31, 16);
	cal_data->count_r[MT6983_TS7_2] = GET_CAL_DATA_BITMASK(13, 15, 0);

	cal_data->count_r[MT6983_TS8_0] = GET_CAL_DATA_BITMASK(13, 31, 16);
	cal_data->count_r[MT6983_TS8_1] = GET_CAL_DATA_BITMASK(14, 15, 0);
	cal_data->count_r[MT6983_TS8_2] = GET_CAL_DATA_BITMASK(14, 31, 16);

	cal_data->count_r[MT6983_TS9_0] = GET_CAL_DATA_BITMASK(15, 15, 0);
	cal_data->count_r[MT6983_TS9_1] = GET_CAL_DATA_BITMASK(15, 31, 16);
	cal_data->count_r[MT6983_TS9_2] = GET_CAL_DATA_BITMASK(16, 15, 0);

	cal_data->count_rc[MT6983_LVTS_MCU_CTRL0] = GET_CAL_DATA_BITMASK(18, 23, 0);
	cal_data->count_rc[MT6983_LVTS_MCU_CTRL1] = GET_CAL_DATA_BITMASK(19, 23, 0);
	cal_data->count_rc[MT6983_LVTS_MCU_CTRL2] = GET_CAL_DATA_BITMASK(20, 23, 0);
	cal_data->count_rc[MT6983_LVTS_AP_CTRL0] = GET_CAL_DATA_BITMASK(22, 23, 0);
	cal_data->count_rc[MT6983_LVTS_AP_CTRL1] = GET_CAL_DATA_BITMASK(23, 23, 0);
	cal_data->count_rc[MT6983_LVTS_AP_CTRL2] = GET_CAL_DATA_BITMASK(24, 23, 0);
	cal_data->count_rc[MT6983_LVTS_AP_CTRL3] = GET_CAL_DATA_BITMASK(25, 23, 0);
	cal_data->count_rc[MT6983_LVTS_AP_CTRL4] = GET_CAL_DATA_BITMASK(26, 23, 0);
}

static void mt6983_check_cal_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i;

	cal_data->use_fake_efuse = 1;
	if ((cal_data->golden_temp != 0) || (cal_data->golden_temp_ht != 0)) {
		cal_data->use_fake_efuse = 0;
	} else {
		for (i = 0; i < lvts_data->num_sensor; i++) {
			if (cal_data->count_r[i] != 0 ||
				cal_data->count_rc[i] != 0) {
				cal_data->use_fake_efuse = 0;
				break;
			}
		}
	}

	if (cal_data->use_fake_efuse) {
		/* It means all efuse data are equal to 0 */
		dev_info(dev,
			"[lvts_cal] This sample is not calibrated, fake !!\n");
		for (i = 0; i < lvts_data->num_sensor; i++)
			cal_data->count_r[i] = cal_data->default_count_r;

		for (i = 0; i < lvts_data->num_tc; i++)
			cal_data->count_rc[i] = cal_data->default_count_rc;

		for (i = 0; i < lvts_data->num_tc; i++) {
			if (tc[i].coeff.cali_mode == CALI_HT)
				tc[i].coeff.golden_temp = cal_data->default_golden_temp_ht;
			else
				tc[i].coeff.golden_temp = cal_data->default_golden_temp;
		}
	}
}

static void mt6983_update_coef_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, s_index;

	for (i = 0; i < lvts_data->num_tc; i++) {
		for  (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];
			if (tc[i].coeff.cali_mode == CALI_HT)
				tc[i].coeff.a[j] = COF_A_OFS_HT + (COF_A_CONST_OFS_HT *
					cal_data->count_r[s_index] / COF_A_COUNT_R_GLD_HT);
			else
				tc[i].coeff.a[j] = COF_A_OFS + (COF_A_CONST_OFS *
					cal_data->count_r[s_index] / COF_A_COUNT_R_GLD);
		}
	}
}

static struct tc_settings mt6983_tc_settings[] = {
	[MT6983_LVTS_MCU_CTRL0] = {
		.domain_index = MT6983_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT6983_TS1_0, MT6983_TS1_1, MT6983_TS1_2, MT6983_TS1_3},
		.tc_speed = SET_TC_SPEED_IN_US(360, 360, 360, 360),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(1),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_MCU_CTRL1] = {
		.domain_index = MT6983_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6983_TS2_0, MT6983_TS2_1, MT6983_TS2_2, MT6983_TS2_3},
		.tc_speed = SET_TC_SPEED_IN_US(10, 10, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(2),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_MCU_CTRL2] = {
		.domain_index = MT6983_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6983_TS3_0, MT6983_TS3_1, MT6983_TS3_2, MT6983_TS3_3},
		.tc_speed = SET_TC_SPEED_IN_US(857, 857, 857, 857),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(3),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_AP_CTRL0] = {
		.domain_index = MT6983_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6983_TS5_0, MT6983_TS5_1},
		.tc_speed = SET_TC_SPEED_IN_US(2345, 2345, 2345, 2345),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(1),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_AP_CTRL1] = {
		.domain_index = MT6983_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6983_TS6_0, MT6983_TS6_1, MT6983_TS6_2, MT6983_TS6_3},
		.tc_speed = SET_TC_SPEED_IN_US(2345, 2345, 2345, 2345),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(2),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_AP_CTRL2] = {
		.domain_index = MT6983_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 3,
		.sensor_map = {MT6983_TS7_0, MT6983_TS7_1, MT6983_TS7_2},
		.tc_speed = SET_TC_SPEED_IN_US(2345, 2345, 2345, 2345),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(3),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_AP_CTRL3] = {
		.domain_index = MT6983_AP_DOMAIN,
		.addr_offset = 0x300,
		.num_sensor = 3,
		.sensor_map = {MT6983_TS8_0, MT6983_TS8_1, MT6983_TS8_2},
		.tc_speed = SET_TC_SPEED_IN_US(2345, 2345, 2345, 2345),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(4),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6983_LVTS_AP_CTRL4] = {
		.domain_index = MT6983_AP_DOMAIN,
		.addr_offset = 0x400,
		.num_sensor = 3,
		.sensor_map = {MT6983_TS9_0, MT6983_TS9_1, MT6983_TS9_2},
		.tc_speed = SET_TC_SPEED_IN_US(2345, 2345, 2345, 2345),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 116500,
		.irq_bit = BIT(5),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	}
};

static struct lvts_data mt6983_lvts_data = {
	.num_domain = MT6983_NUM_DOMAIN,
	.num_tc = MT6983_LVTS_CTRL_NUM,
	.tc = mt6983_tc_settings,
	.num_sensor = MT6983_NUM_TS,
	.ops = {
		.device_identification = mt6983_device_identification,
		.efuse_to_cal_data = mt6983_efuse_to_cal_data,
		.device_enable_and_init = mt6983_device_enable_and_init,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = mt6983_device_read_count_rc_n,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v2,
		.lvts_raw_to_temp = lvts_raw_to_temp_v2,
		.check_cal_data = mt6983_check_cal_data,
		.update_coef_data = mt6983_update_coef_data,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK,
	.num_efuse_addr = 28,
	.num_efuse_block = 3,
	.cal_data = {
		.default_golden_temp = 60,
		.default_golden_temp_ht = 170,
		.default_count_r = 12084,
		.default_count_rc = 13799,
	},
	.init_done = false,
	.enable_dump_log = 0,
};

/*==================================================
 * LVTS MT6895
 *==================================================
 */
#define SET_TS_DIS_6895 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8	\
						| 0xB1)

#define SET_LVTS_MANUAL_RCK_OPERATION_6895 (DEVICE_WRITE | RG_TSV2F_CTRL_6 << 8 | 0x00)
#define SET_TS_DIV_EN_6895 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xB5)
#define SET_VCO_RST_6895 (DEVICE_WRITE | RG_TSV2F_CTRL_0 << 8 | 0xBD)

static int mt6895_device_read_count_rc_n(struct lvts_data *lvts_data)
{

	/* Resistor-Capacitor Calibration */
	/* count_RC_N: count RC now */
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	unsigned int offset, size, s_index, data, device_id;
	void __iomem *base;
	int ret, i, j;
	char buffer[512];
	unsigned int  rc_data;
	int refine_data_idx[4] = {0};
	int count_rc_delta = 0;

	if (lvts_data->init_done) {

		for (i = 0; i < lvts_data->num_tc; i++) {
			lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
			lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
		}
		return 0;
	}


	cal_data->count_rc_now =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->count_rc_now), GFP_KERNEL);

	cal_data->efuse_data =
		devm_kcalloc(dev, lvts_data->num_sensor,
			sizeof(*cal_data->efuse_data), GFP_KERNEL);

	if ((!cal_data->count_rc_now) || (!cal_data->efuse_data))
		return -ENOMEM;


	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_write_device(lvts_data, SET_LVTS_MANUAL_RCK_OPERATION_6895, i);

		for (j = 0; j < tc[i].num_sensor; j++) {
			refine_data_idx[j] = 0xff;
			s_index = tc[i].sensor_map[j];
			device_id = tc[i].device_id[j];

			lvts_write_device(lvts_data, SELECT_SENSOR_RCK_V4(device_id), i);
			lvts_write_device(lvts_data, SET_DEVICE_SINGLE_MODE_V4, i);
			lvts_write_device(lvts_data, SET_TS_DIV_EN_6895, i);
			lvts_write_device(lvts_data, SET_VCO_RST_6895, i);
			lvts_write_device(lvts_data, SET_TS_DIV_EN_6895, i);
			udelay(10);

			lvts_write_device(lvts_data, KICK_OFF_RCK_COUNTING_V4, i);
			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_SENSING_STATUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n", i);

			lvts_write_device(lvts_data, SET_TS_DIS_6895, i);
			data = lvts_read_device(lvts_data, 0x00, i);
			udelay(5);

			rc_data = (data & GENMASK(23, 0));

			if (rc_data == 0) {
				refine_data_idx[j] = s_index;
				dev_info(dev, "rc_data %d, data_idx[%d]=%d\n", rc_data, j,
					s_index);
			} else {
				if (cal_data->count_rc[i] > rc_data)
					count_rc_delta = (cal_data->count_rc[i] * 1000) / rc_data;
				else
					count_rc_delta = (rc_data * 1000) / cal_data->count_rc[i];

				if (count_rc_delta > 1061) {
					refine_data_idx[j] = s_index;
					dev_info(dev,
						"delta %d, data_idx[%d]=%d\n",
						count_rc_delta, j, s_index);
				}
			}

			ret = readl_poll_timeout(LVTS_CONFIG_0 + base, data,
				!(data & DEVICE_ACCESS_STARTUS),
				2, 200);
			if (ret)
				dev_info(dev,
					"Error: LVTS %d DEVICE_ACCESS_START didn't ready\n",
					i);

			if (refine_data_idx[j] != 0xff) {
				dev_info(dev, "refine_data_idx[%d]=%d\n", j, refine_data_idx[j]);
				cal_data->count_rc_now[s_index] = cal_data->count_rc[i];
			} else
				cal_data->count_rc_now[s_index] = (rc_data & GENMASK(23, 0));

			/*count data here that want to set to efuse later*/
			cal_data->efuse_data[s_index] = (((unsigned long long)
				cal_data->count_rc_now[s_index]) *
				cal_data->count_r[s_index]) >> 14;
		}

		/* Recover Setting for Normal Access on
		 * temperature fetch
		 */
		lvts_write_device(lvts_data, SET_SENSOR_NO_RCK_V4, i);
		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);
	}


	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[COUNT_RC_NOW] ");
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d ",
				i, cal_data->count_rc_now[i]);

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

#define MT6895_NUM_LVTS (ARRAY_SIZE(mt6895_tc_settings))

enum mt6895_lvts_domain {
	MT6895_AP_DOMAIN,
	MT6895_MCU_DOMAIN,
	MT6895_NUM_DOMAIN
};

enum mt6895_lvts_sensor_enum {
	MT6895_TS1_0,
	MT6895_TS1_1,
	MT6895_TS1_2,
	MT6895_TS1_3,
	MT6895_TS2_0,
	MT6895_TS2_1,
	MT6895_TS2_2,
	MT6895_TS2_3,
	MT6895_TS3_0,
	MT6895_TS3_1,
	MT6895_TS3_2,
	MT6895_TS3_3,
	MT6895_TS5_0,
	MT6895_TS5_1,
	MT6895_TS6_0,
	MT6895_TS6_2,
	MT6895_TS7_0,
	MT6895_TS7_1,
	MT6895_TS7_2,
	MT6895_TS8_0,
	MT6895_TS8_1,
	MT6895_TS8_2,
	MT6895_TS9_0,
	MT6895_TS9_1,
	MT6895_TS9_2,
	MT6895_NUM_TS

};

enum mt6895_lvts_controller_enum {
	MT6895_LVTS_MCU_CTRL0,
	MT6895_LVTS_MCU_CTRL1,
	MT6895_LVTS_MCU_CTRL2,
	MT6895_LVTS_AP_CTRL0,
	MT6895_LVTS_AP_CTRL1,
	MT6895_LVTS_AP_CTRL2,
	MT6895_LVTS_AP_CTRL3,
	MT6895_LVTS_AP_CTRL4,
	MT6895_LVTS_CTRL_NUM
};

static void mt6895_device_identification(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int lvts_dev_id;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		writel(ENABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES, i);

		lvts_write_device(lvts_data, READ_BACK_DEVICE_ID, i);

		/* Check LVTS device ID */
		data = (readl(LVTS_ID_0 + base) & GENMASK(7, 0));

		if  (tc[i].domain_index == MT6895_AP_DOMAIN)
			lvts_dev_id = 0x82 + i;
		else
			lvts_dev_id = 0x81 + i;

		if (data != lvts_dev_id) {
			dev_err(dev, "LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				i, lvts_dev_id, data);
		}
	}
}

static void mt6895_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct tc_settings *tc = lvts_data->tc;
	int i = 0;

	cal_data->cali_mode = GET_CAL_DATA_BIT(0, 31);
	cal_data->golden_temp_ht = GET_CAL_DATA_BITMASK(0, 15, 8);
	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 7, 0);

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].coeff.golden_temp = cal_data->golden_temp;

	if (cal_data->cali_mode == 1) {
		for (i = 0; i < 3; i++) {
			tc[i].coeff.golden_temp = cal_data->golden_temp_ht;
			tc[i].coeff.cali_mode = CALI_HT;
		}
	}

	cal_data->count_r[MT6895_TS1_0] = GET_CAL_DATA_BITMASK(1, 15, 0);
	cal_data->count_r[MT6895_TS1_1] = GET_CAL_DATA_BITMASK(1, 31, 16);
	cal_data->count_r[MT6895_TS1_2] = GET_CAL_DATA_BITMASK(2, 15, 0);
	cal_data->count_r[MT6895_TS1_3] = GET_CAL_DATA_BITMASK(2, 31, 16);

	cal_data->count_r[MT6895_TS2_0] = GET_CAL_DATA_BITMASK(3, 15, 0);
	cal_data->count_r[MT6895_TS2_1] = GET_CAL_DATA_BITMASK(3, 31, 16);
	cal_data->count_r[MT6895_TS2_2] = GET_CAL_DATA_BITMASK(4, 15, 0);
	cal_data->count_r[MT6895_TS2_3] = GET_CAL_DATA_BITMASK(4, 31, 16);

	cal_data->count_r[MT6895_TS3_0] = GET_CAL_DATA_BITMASK(5, 15, 0);
	cal_data->count_r[MT6895_TS3_1] = GET_CAL_DATA_BITMASK(5, 31, 16);
	cal_data->count_r[MT6895_TS3_2] = GET_CAL_DATA_BITMASK(6, 15, 0);
	cal_data->count_r[MT6895_TS3_3] = GET_CAL_DATA_BITMASK(6, 31, 16);

	cal_data->count_r[MT6895_TS5_0] = GET_CAL_DATA_BITMASK(9, 15, 0);
	cal_data->count_r[MT6895_TS5_1] = GET_CAL_DATA_BITMASK(9, 31, 16);

	cal_data->count_r[MT6895_TS6_0] = GET_CAL_DATA_BITMASK(10, 15, 0);
	cal_data->count_r[MT6895_TS6_2] = GET_CAL_DATA_BITMASK(11, 15, 0);

	cal_data->count_r[MT6895_TS7_0] = GET_CAL_DATA_BITMASK(12, 15, 0);
	cal_data->count_r[MT6895_TS7_1] = GET_CAL_DATA_BITMASK(12, 31, 16);
	cal_data->count_r[MT6895_TS7_2] = GET_CAL_DATA_BITMASK(13, 15, 0);

	cal_data->count_r[MT6895_TS8_0] = GET_CAL_DATA_BITMASK(13, 31, 16);
	cal_data->count_r[MT6895_TS8_1] = GET_CAL_DATA_BITMASK(14, 15, 0);
	cal_data->count_r[MT6895_TS8_2] = GET_CAL_DATA_BITMASK(14, 31, 16);

	cal_data->count_r[MT6895_TS9_0] = GET_CAL_DATA_BITMASK(15, 15, 0);
	cal_data->count_r[MT6895_TS9_1] = GET_CAL_DATA_BITMASK(15, 31, 16);
	cal_data->count_r[MT6895_TS9_2] = GET_CAL_DATA_BITMASK(16, 15, 0);

	cal_data->count_rc[MT6895_LVTS_MCU_CTRL0] = GET_CAL_DATA_BITMASK(18, 23, 0);
	cal_data->count_rc[MT6895_LVTS_MCU_CTRL1] = GET_CAL_DATA_BITMASK(19, 23, 0);
	cal_data->count_rc[MT6895_LVTS_MCU_CTRL2] = GET_CAL_DATA_BITMASK(20, 23, 0);
	cal_data->count_rc[MT6895_LVTS_AP_CTRL0] = GET_CAL_DATA_BITMASK(22, 23, 0);
	cal_data->count_rc[MT6895_LVTS_AP_CTRL1] = GET_CAL_DATA_BITMASK(23, 23, 0);
	cal_data->count_rc[MT6895_LVTS_AP_CTRL2] = GET_CAL_DATA_BITMASK(24, 23, 0);
	cal_data->count_rc[MT6895_LVTS_AP_CTRL3] = GET_CAL_DATA_BITMASK(25, 23, 0);
	cal_data->count_rc[MT6895_LVTS_AP_CTRL4] = GET_CAL_DATA_BITMASK(26, 23, 0);
}

static struct tc_settings mt6895_tc_settings[] = {
	[MT6895_LVTS_MCU_CTRL0] = {
		.domain_index = MT6895_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT6895_TS1_0, MT6895_TS1_1, MT6895_TS1_2, MT6895_TS1_3},
		.device_id = {0, 1, 2, 3},
		.tc_speed = SET_TC_SPEED_IN_US(857, 857, 857, 857),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(1),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_MCU_CTRL1] = {
		.domain_index = MT6895_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6895_TS2_0, MT6895_TS2_1, MT6895_TS2_2, MT6895_TS2_3},
		.device_id = {0, 1, 2, 3},
		.tc_speed = SET_TC_SPEED_IN_US(360, 360, 360, 360),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(2),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_MCU_CTRL2] = {
		.domain_index = MT6895_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6895_TS3_0, MT6895_TS3_1, MT6895_TS3_2, MT6895_TS3_3},
		.device_id = {0, 1, 2, 3},
		.tc_speed = SET_TC_SPEED_IN_US(10, 10, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(3),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_AP_CTRL0] = {
		.domain_index = MT6895_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6895_TS5_0, MT6895_TS5_1},
		.device_id = {0, 1},
		.tc_speed = SET_TC_SPEED_IN_US(4816, 4816, 4816, 4816),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(1),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_AP_CTRL1] = {
		.domain_index = MT6895_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6895_TS6_0, MT6895_TS6_2},
		.device_id = {0, 2},
		.tc_speed = SET_TC_SPEED_IN_US(4816, 4816, 4816, 4816),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(2),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_AP_CTRL2] = {
		.domain_index = MT6895_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 3,
		.sensor_map = {MT6895_TS7_0, MT6895_TS7_1, MT6895_TS7_2},
		.device_id = {0, 1, 2},
		.tc_speed = SET_TC_SPEED_IN_US(3162, 3162, 3162, 3162),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(3),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_AP_CTRL3] = {
		.domain_index = MT6895_AP_DOMAIN,
		.addr_offset = 0x300,
		.num_sensor = 3,
		.sensor_map = {MT6895_TS8_0, MT6895_TS8_1, MT6895_TS8_2},
		.device_id = {0, 1, 2},
		.tc_speed = SET_TC_SPEED_IN_US(3162, 3162, 3162, 3162),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(4),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	},
	[MT6895_LVTS_AP_CTRL4] = {
		.domain_index = MT6895_AP_DOMAIN,
		.addr_offset = 0x400,
		.num_sensor = 3,
		.sensor_map = {MT6895_TS9_0, MT6895_TS9_1, MT6895_TS9_2},
		.device_id = {0, 1, 2},
		.tc_speed = SET_TC_SPEED_IN_US(3162, 3162, 3162, 3162),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 118500,
		.irq_bit = BIT(5),
		.coeff = {
			.cali_mode = CALI_NT,
		},
	}
};

static struct lvts_data mt6895_lvts_data = {
	.num_domain = MT6895_NUM_DOMAIN,
	.num_tc = MT6895_LVTS_CTRL_NUM,
	.tc = mt6895_tc_settings,
	.num_sensor = MT6895_NUM_TS,
	.ops = {
		.device_identification = mt6895_device_identification,
		.efuse_to_cal_data = mt6895_efuse_to_cal_data,
		.device_enable_and_init = mt6983_device_enable_and_init,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = mt6895_device_read_count_rc_n,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v2,
		.lvts_raw_to_temp = lvts_raw_to_temp_v2,
		.check_cal_data = mt6983_check_cal_data,
		.update_coef_data = mt6983_update_coef_data,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK,
	.num_efuse_addr = 28,
	.num_efuse_block = 3,
	.cal_data = {
		.default_golden_temp = 60,
		.default_golden_temp_ht = 170,
		.default_count_r = 12052,
		.default_count_rc = 13000,
	},
	.init_done = false,
	.enable_dump_log = 0,
};

/*==================================================
 * LVTS MT6879
 *==================================================
 */

enum mt6879_lvts_domain {
	MT6879_AP_DOMAIN,
	MT6879_MCU_DOMAIN,
	MT6879_NUM_DOMAIN
};

enum mt6879_lvts_sensor_enum {
	MT6879_TS1_0,//(CPU_6_B)
	MT6879_TS1_1,//(CPU_7_B)
	MT6879_TS2_0,//(CPU_5_L)
	MT6879_TS2_1,//(CPU_0_L,CPU_1_L)
	MT6879_TS2_2,//(CPU_2_L,CPU-3_L)
	MT6879_TS2_3,//(CPU_4_L,CPU_7_B)
	MT6879_TS3_0,//(GPU)
	MT6879_TS3_1,//(GPU)
	MT6879_TS4_0,//(APU)
	MT6879_TS4_1,//(APU)
	MT6879_TS5_0,//(SOC_CAM)
	MT6879_TS5_1,//(SOC_DRAMC)
	MT6879_TS5_2,//(SOC_VLP)
	MT6879_TS5_3,//(SOC_CONNSYS)
	MT6879_TS6_0,//(MD)
	MT6879_TS6_1,//(MD)
	MT6879_TS6_2,//(MD)
	MT6879_TS6_3,//(MD)
	MT6879_NUM_TS

};


enum mt6879_lvts_controller_enum {
	MT6879_LVTS_MCU_CTRL0,
	MT6879_LVTS_MCU_CTRL1,
	MT6879_LVTS_AP_CTRL0,
	MT6879_LVTS_AP_CTRL1,
	MT6879_LVTS_AP_CTRL2,
	MT6879_LVTS_AP_CTRL3,
	MT6879_LVTS_CTRL_NUM
};

static void mt6879_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;

	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 31, 24);
	cal_data->count_r[MT6879_TS1_0] = GET_CAL_DATA_BITMASK(1, 23, 0);
	cal_data->count_r[MT6879_TS1_1] = GET_CAL_DATA_BITMASK(2, 23, 0);

	cal_data->count_r[MT6879_TS2_0] = GET_CAL_DATA_BITMASK(3, 23, 0);
	cal_data->count_r[MT6879_TS2_1] = GET_CAL_DATA_BITMASK(4, 23, 0);
	cal_data->count_r[MT6879_TS2_2] = GET_CAL_DATA_BITMASK(5, 23, 0);
	cal_data->count_r[MT6879_TS2_3] = GET_CAL_DATA_BITMASK(6, 23, 0);

	cal_data->count_r[MT6879_TS3_0] = GET_CAL_DATA_BITMASK(7, 23, 0);
	cal_data->count_r[MT6879_TS3_1] = GET_CAL_DATA_BITMASK(8, 23, 0);

	cal_data->count_r[MT6879_TS4_0] = GET_CAL_DATA_BITMASK(9, 23, 0);
	cal_data->count_r[MT6879_TS4_1] = GET_CAL_DATA_BITMASK(10, 23, 0);

	cal_data->count_r[MT6879_TS5_0] = GET_CAL_DATA_BITMASK(11, 23, 0);
	cal_data->count_r[MT6879_TS5_1] = GET_CAL_DATA_BITMASK(12, 23, 0);
	cal_data->count_r[MT6879_TS5_2] = GET_CAL_DATA_BITMASK(13, 23, 0);
	cal_data->count_r[MT6879_TS5_3] = GET_CAL_DATA_BITMASK(14, 23, 0);

	cal_data->count_r[MT6879_TS6_0] = GET_CAL_DATA_BITMASK(15, 23, 0);
	cal_data->count_r[MT6879_TS6_1] = GET_CAL_DATA_BITMASK(16, 23, 0);
	cal_data->count_r[MT6879_TS6_2] = GET_CAL_DATA_BITMASK(17, 23, 0);
	cal_data->count_r[MT6879_TS6_3] = GET_CAL_DATA_BITMASK(18, 23, 0);


	cal_data->count_rc[MT6879_LVTS_MCU_CTRL0] = GET_CAL_DATA_BITMASK(23, 23, 0);

	cal_data->count_rc[MT6879_LVTS_MCU_CTRL1] =
		(GET_CAL_DATA_BITMASK(1, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(2, 31, 24) << 8) +
		 GET_CAL_DATA_BITMASK(3, 31, 24);

	cal_data->count_rc[MT6879_LVTS_AP_CTRL0] =
		(GET_CAL_DATA_BITMASK(4, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(5, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(6, 31, 24);

	cal_data->count_rc[MT6879_LVTS_AP_CTRL1] =
		(GET_CAL_DATA_BITMASK(7, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(8, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(9, 31, 24);

	cal_data->count_rc[MT6879_LVTS_AP_CTRL2] =
		(GET_CAL_DATA_BITMASK(10, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(11, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(12, 31, 24);

	cal_data->count_rc[MT6879_LVTS_AP_CTRL3] =
		(GET_CAL_DATA_BITMASK(13, 31, 24) << 16) +
		(GET_CAL_DATA_BITMASK(14, 31, 24) << 8) +
		GET_CAL_DATA_BITMASK(15, 31, 24);
}

static struct tc_settings mt6879_tc_settings[] = {
	[MT6879_LVTS_MCU_CTRL0] = {
		.domain_index = MT6879_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6879_TS1_0, MT6879_TS1_1},
		.tc_speed = SET_TC_SPEED_IN_US(10, 10, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6879_LVTS_MCU_CTRL1] = {
		.domain_index = MT6879_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6879_TS2_0, MT6879_TS2_1, MT6879_TS2_2, MT6879_TS2_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6879_LVTS_AP_CTRL0] = {
		.domain_index = MT6879_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6879_TS3_0, MT6879_TS3_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6879_LVTS_AP_CTRL1] = {
		.domain_index = MT6879_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 2,
		.sensor_map = {MT6879_TS4_0, MT6879_TS4_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6879_LVTS_AP_CTRL2] = {
		.domain_index = MT6879_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6879_TS5_0, MT6879_TS5_1, MT6879_TS5_2, MT6879_TS5_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6879_LVTS_AP_CTRL3] = {
		.domain_index = MT6879_AP_DOMAIN,
		.addr_offset = 0x300,
		.num_sensor = 4,
		.sensor_map = {MT6879_TS6_0, MT6879_TS6_1, MT6879_TS6_2, MT6879_TS6_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT0,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(4),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	}
};

static struct lvts_data mt6879_lvts_data = {
	.num_domain = MT6879_NUM_DOMAIN,
	.num_tc = MT6879_LVTS_CTRL_NUM,
	.tc = mt6879_tc_settings,
	.num_sensor = MT6879_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt6879_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v5,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v5,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = 0,
	.num_efuse_addr = 24,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 1,
};


/*==================================================
 * LVTS MT6855
 *==================================================
 */

enum mt6855_lvts_domain {
	MT6855_AP_DOMAIN,
	MT6855_MCU_DOMAIN,
	MT6855_NUM_DOMAIN
};

enum mt6855_lvts_sensor_enum {
	MT6855_TS1_0,
	MT6855_TS1_1,
	MT6855_TS1_2,
	MT6855_TS1_3,
	MT6855_TS2_0,
	MT6855_TS2_1,
	MT6855_TS2_2,
	MT6855_TS2_3,
	MT6855_TS3_0,
	MT6855_TS3_1,
	MT6855_TS3_2,
	MT6855_TS3_3,
	MT6855_TS4_0,
	MT6855_TS4_1,
	MT6855_TS5_0,
	MT6855_TS5_1,
	MT6855_TS5_2,
	MT6855_TS5_3,
	MT6855_TS6_0,
	MT6855_TS6_1,
	MT6855_TS6_2,
	MT6855_NUM_TS

};

enum mt6855_lvts_controller_enum {
	MT6855_LVTS_MCU_CTRL0,
	MT6855_LVTS_MCU_CTRL1,
	MT6855_LVTS_MCU_CTRL2,
	MT6855_LVTS_AP_CTRL0,
	MT6855_LVTS_AP_CTRL1,
	MT6855_LVTS_AP_CTRL2,
	MT6855_LVTS_CTRL_NUM
};

static void mt6855_efuse_to_cal_data(struct lvts_data *lvts_data)
{
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;

	cal_data->golden_temp = GET_CAL_DATA_BITMASK(0, 31, 24);
	cal_data->count_r[MT6855_TS1_0] = GET_CAL_DATA_BITMASK(1, 31, 16);
	cal_data->count_r[MT6855_TS1_1] = GET_CAL_DATA_BITMASK(1, 15, 0);
	cal_data->count_r[MT6855_TS1_2] = GET_CAL_DATA_BITMASK(2, 31, 16);
	cal_data->count_r[MT6855_TS1_3] = GET_CAL_DATA_BITMASK(2, 15, 0);

	cal_data->count_r[MT6855_TS2_0] = GET_CAL_DATA_BITMASK(3, 31, 16);
	cal_data->count_r[MT6855_TS2_1] = GET_CAL_DATA_BITMASK(3, 15, 0);
	cal_data->count_r[MT6855_TS2_2] = GET_CAL_DATA_BITMASK(4, 31, 16);
	cal_data->count_r[MT6855_TS2_3] = GET_CAL_DATA_BITMASK(4, 15, 0);

	cal_data->count_r[MT6855_TS3_0] = GET_CAL_DATA_BITMASK(5, 31, 16);
	cal_data->count_r[MT6855_TS3_1] = GET_CAL_DATA_BITMASK(5, 15, 0);
	cal_data->count_r[MT6855_TS3_2] = GET_CAL_DATA_BITMASK(6, 31, 16);
	cal_data->count_r[MT6855_TS3_3] = GET_CAL_DATA_BITMASK(6, 15, 0);

	cal_data->count_r[MT6855_TS4_0] = GET_CAL_DATA_BITMASK(7, 31, 16);
	cal_data->count_r[MT6855_TS4_1] = GET_CAL_DATA_BITMASK(7, 15, 0);

	cal_data->count_r[MT6855_TS5_0] = GET_CAL_DATA_BITMASK(8, 31, 16);
	cal_data->count_r[MT6855_TS5_1] = GET_CAL_DATA_BITMASK(8, 15, 0);
	cal_data->count_r[MT6855_TS5_2] = GET_CAL_DATA_BITMASK(9, 31, 16);
	cal_data->count_r[MT6855_TS5_3] = GET_CAL_DATA_BITMASK(9, 15, 0);

	cal_data->count_r[MT6855_TS6_0] = GET_CAL_DATA_BITMASK(10, 31, 16);
	cal_data->count_r[MT6855_TS6_1] = GET_CAL_DATA_BITMASK(10, 15, 0);
	cal_data->count_r[MT6855_TS6_2] = GET_CAL_DATA_BITMASK(11, 31, 16);


	cal_data->count_rc[MT6855_LVTS_MCU_CTRL0] = GET_CAL_DATA_BITMASK(13, 23, 0);

	cal_data->count_rc[MT6855_LVTS_MCU_CTRL1] = GET_CAL_DATA_BITMASK(14, 23, 0);

	cal_data->count_rc[MT6855_LVTS_MCU_CTRL2] = GET_CAL_DATA_BITMASK(15, 23, 0);

	cal_data->count_rc[MT6855_LVTS_AP_CTRL0] =  GET_CAL_DATA_BITMASK(16, 23, 0);

	cal_data->count_rc[MT6855_LVTS_AP_CTRL1] =  GET_CAL_DATA_BITMASK(17, 23, 0);

	cal_data->count_rc[MT6855_LVTS_AP_CTRL2] =  GET_CAL_DATA_BITMASK(18, 23, 0);

}

static struct tc_settings mt6855_tc_settings[] = {
	[MT6855_LVTS_MCU_CTRL0] = {
		.domain_index = MT6855_MCU_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 4,
		.sensor_map = {MT6855_TS1_0, MT6855_TS1_1, MT6855_TS1_2, MT6855_TS1_3},
		.tc_speed = SET_TC_SPEED_IN_US(10, 10, 10, 10),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6855_LVTS_MCU_CTRL1] = {
		.domain_index = MT6855_MCU_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6855_TS2_0, MT6855_TS2_1, MT6855_TS2_2, MT6855_TS2_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6855_LVTS_MCU_CTRL2] = {
		.domain_index = MT6855_MCU_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 4,
		.sensor_map = {MT6855_TS3_0, MT6855_TS3_1, MT6855_TS3_2, MT6855_TS3_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6855_LVTS_AP_CTRL0] = {
		.domain_index = MT6855_AP_DOMAIN,
		.addr_offset = 0x0,
		.num_sensor = 2,
		.sensor_map = {MT6855_TS4_0, MT6855_TS4_1},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT1,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(1),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6855_LVTS_AP_CTRL1] = {
		.domain_index = MT6855_AP_DOMAIN,
		.addr_offset = 0x100,
		.num_sensor = 4,
		.sensor_map = {MT6855_TS5_0, MT6855_TS5_1, MT6855_TS5_2, MT6855_TS5_3},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT3,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(2),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
	[MT6855_LVTS_AP_CTRL2] = {
		.domain_index = MT6855_AP_DOMAIN,
		.addr_offset = 0x200,
		.num_sensor = 3,
		.sensor_map = {MT6855_TS6_0, MT6855_TS6_1, MT6855_TS6_2},
		.tc_speed = SET_TC_SPEED_IN_US(118, 118, 118, 118),
		.hw_filter = LVTS_FILTER_1,
		.dominator_sensing_point = SENSING_POINT2,
		.hw_reboot_trip_point = 113500,
		.irq_bit = BIT(3),
		.coeff = {
			.a = {-250460},
			.cali_mode = CALI_NT,
		},
	},
};

static struct lvts_data mt6855_lvts_data = {
	.num_domain = MT6855_NUM_DOMAIN,
	.num_tc = MT6855_LVTS_CTRL_NUM,
	.tc = mt6855_tc_settings,
	.num_sensor = MT6855_NUM_TS,
	.ops = {
		.device_identification = device_identification_v1,
		.efuse_to_cal_data = mt6855_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v5,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v5,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
		.lvts_temp_to_raw = lvts_temp_to_raw_v1,
		.lvts_raw_to_temp = lvts_raw_to_temp_v1,
		.check_cal_data = check_cal_data_v1,
	},
	.feature_bitmap = 0,
	.num_efuse_addr = 24,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 60,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.init_done = false,
	.enable_dump_log = 0,
};
/*==================================================
 * Support chips
 *==================================================
 */
static const struct dev_pm_ops lvts_pm_ops = {
	.suspend_noirq = lvts_suspend_noirq,
	.resume_noirq = lvts_resume_noirq,
};

static const struct of_device_id lvts_of_match[] = {
	{
		.compatible = "mediatek,mt6873-lvts",
		.data = (void *)&mt6873_lvts_data,
	},
	{
		.compatible = "mediatek,mt6853-lvts",
		.data = (void *)&mt6853_lvts_data,
	},
	{
		.compatible = "mediatek,mt6893-lvts",
		.data = (void *)&mt6893_lvts_data,
	},
	{
		.compatible = "mediatek,mt6983-lvts",
		.data = (void *)&mt6983_lvts_data,
	},
	{
		.compatible = "mediatek,mt6879-lvts",
		.data = (void *)&mt6879_lvts_data,
	},
	{
		.compatible = "mediatek,mt6895-lvts",
		.data = (void *)&mt6895_lvts_data,
	},
	{
		.compatible = "mediatek,mt6855-lvts",
		.data = (void *)&mt6855_lvts_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, lvts_of_match);
/*==================================================*/
static struct platform_driver soc_temp_lvts = {
	.probe = lvts_probe,
	.remove = lvts_remove,
	.driver = {
		.name = "mtk-soc-temp-lvts",
		.of_match_table = lvts_of_match,
		.pm = &lvts_pm_ops,
	},
};

module_platform_driver(soc_temp_lvts);
MODULE_AUTHOR("Yu-Chia Chang <ethan.chang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek soc temperature driver");
MODULE_LICENSE("GPL v2");

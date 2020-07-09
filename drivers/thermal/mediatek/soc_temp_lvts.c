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
/*==================================================
 * LVTS local common code
 *==================================================
 */
static int lvts_raw_to_temp(struct formula_coeff *co, unsigned int msr_raw)
{
	/* This function returns degree mC */

	int temp;

	temp = (co->a * ((unsigned long long)msr_raw)) >> 14;
	temp = temp + co->golden_temp * 500 + co->b;

	return temp;
}

static unsigned int lvts_temp_to_raw(struct formula_coeff *co, int temp)
{
	unsigned int msr_raw = 0;

	msr_raw = ((long long)((co->golden_temp * 500 + co->b - temp)) << 14)
		/ (-1 * co->a);

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

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * j);
			current_temp = lvts_raw_to_temp(&lvts_data->coeff, msr_raw);

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;

			if (i == 0 && j == 0)
				max_temp = current_temp;
			else if (current_temp > max_temp)
				max_temp = current_temp;

			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
		}
	}

	return max_temp;
}

static int soc_temp_lvts_read_temp(void *data, int *temperature)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *) data;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;

	if (lvts_tz->id == 0)
		*temperature = lvts_read_all_tc_temperature(lvts_data);
	else if (lvts_tz->id - 1 < lvts_data->num_sensor)
		*temperature = lvts_data->sen_data[lvts_tz->id - 1].temp;
	else
		return -EINVAL;

	return 0;
}

static const struct thermal_zone_of_device_ops soc_temp_lvts_ops = {
	.get_temp = soc_temp_lvts_read_temp,
};

static void lvts_write_device(struct lvts_data *lvts_data, unsigned int data,
	int tc_id)
{
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	writel(data, LVTS_CONFIG_0 + base);

	udelay(5);
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
			"Error: LVTS %d DEVICE_ACCESS_START didn't ready\n", tc_id);

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

static void device_identification(struct lvts_data *lvts_data)
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

static void set_polling_speed(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
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

	dev_info(dev, "%s %d, LVTSMONCTL1_0= 0x%x,LVTSMONCTL2_0= 0x%x\n",
		__func__, tc_id,
		readl(LVTSMONCTL1_0 + base),
		readl(LVTSMONCTL2_0 + base));
}

static void set_hw_filter(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
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
	dev_info(dev, "%s %d, LVTSMSRCTL0_0= 0x%x\n",
		__func__, tc_id, readl(LVTSMSRCTL0_0 + base));
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
	writel(temp & ~(STAGE3_INT_EN), LVTSMONINT_0 + base);
}

static void enable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* Enable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);
	writel(temp | STAGE3_INT_EN, LVTSMONINT_0 + base);
	/* Clear the offset */
	temp = readl(LVTSPROTCTL_0 + base);
	writel(temp & ~PROTOFFSET, LVTSPROTCTL_0 + base);
}

static void set_tc_hw_reboot_threshold(struct lvts_data *lvts_data,
	int trip_point, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int msr_raw, temp, config, ts_name, d_index;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	d_index = get_dominator_index(lvts_data, tc_id);

	dev_info(dev, "%s: LVTS%d, the dominator sensing point= %d\n",
		__func__, tc_id, d_index);

	disable_hw_reboot_interrupt(lvts_data, tc_id);

	temp = readl(LVTSPROTCTL_0 + base);
	if (d_index == ALL_SENSING_POINTS) {
		ts_name = 0;
		/* Maximum of 4 sensing points */
		config = (0x1 << 16);
		writel(config | temp, LVTSPROTCTL_0 + base);
	} else {
		ts_name = tc[tc_id].sensor_map[d_index];
		/* Select protection sensor */
		config = ((d_index << 2) + 0x2) << 16;
		writel(config | temp, LVTSPROTCTL_0 + base);
	}

	msr_raw = lvts_temp_to_raw(&lvts_data->coeff, trip_point);
	writel(msr_raw, LVTSPROTTC_0 + base);

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

		if (trip_point == DISABLE_THERMAL_HW_REBOOT)
			continue;

		set_tc_hw_reboot_threshold(lvts_data, trip_point, i);
	}
}

static int lvts_init(struct lvts_data *lvts_data)
{
	struct platform_ops *ops = &lvts_data->ops;
	struct device *dev = lvts_data->dev;
	int ret;

	ret = clk_prepare_enable(lvts_data->clk);
	if (ret)
		dev_err(dev,
			"Error: Failed to enable lvts controller clock: %d\n",
			ret);

	lvts_reset(lvts_data);

	device_identification(lvts_data);
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
	enable_all_sensing_points(lvts_data);

	set_all_tc_hw_reboot(lvts_data);

	return 0;
}

static int prepare_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct platform_ops *ops = &lvts_data->ops;
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

	lvts_data->coeff.golden_temp = cal_data->golden_temp;

	dev_info(dev, "[lvts_cal] golden_temp = %d\n", cal_data->golden_temp);

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[lvts_cal] num:g_count:g_count_rc ");
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d:%d ",
				i, cal_data->count_r[i], cal_data->count_rc[i]);

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
	size_t len;
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

	base = GET_BASE_ADDR(tc_id);

	ret = readl(LVTSMONINTSTS_0 + base);
	/* Write back to clear interrupt status */
	writel(ret, LVTSMONINTSTS_0 + base);

	dev_info(dev, "[Thermal IRQ] LVTS thermal controller %d, LVTSMONINTSTS=0x%08x\n",
		tc_id, ret);

	if (ret & THERMAL_PROTECTION_STAGE_3)
		dev_info(dev, "[Thermal IRQ]: Thermal protection stage 3 interrupt triggered, Thermal HW reboot\n");
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct lvts_data *lvts_data = (struct lvts_data *) dev_id;
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, *irq_bitmap;
	void __iomem *base;

	irq_bitmap = kcalloc(lvts_data->num_domain,
				sizeof(*irq_bitmap), GFP_KERNEL);

	if (!irq_bitmap) {
		dev_err(dev, "Error: Failed to allocate memory for irq_bitmap\n");
		return IRQ_NONE;
	}

	for (i = 0; i < lvts_data->num_domain; i++) {
		base = lvts_data->domain[i].base;
		irq_bitmap[i] = readl(THERMINTST + base);
		dev_info(dev, "%s : THERMINTST = 0x%x\n", __func__, irq_bitmap[i]);
	}

	for (i = 0; i < lvts_data->num_tc; i++) {
		if ((irq_bitmap[tc[i].domain_index] & tc[i].irq_bit) == 0)
			tc_irq_handler(lvts_data, i);
	}

	kfree(irq_bitmap);

	return IRQ_HANDLED;
}

static int lvts_register_irq_handler(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i;
	int ret;

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

static int lvts_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

	lvts_close(lvts_data);

	return 0;
}

static int lvts_resume(struct platform_device *pdev)
{
	int ret;
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

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

	cal_data->count_rc_now = devm_kcalloc(dev, lvts_data->num_sensor,
					sizeof(*cal_data->count_rc_now), GFP_KERNEL);
	if (!cal_data->count_rc_now)
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
				e_data = (((unsigned long long)
					cal_data->count_rc_now[s_index]) *
					cal_data->count_r[s_index]) >> 14;

			writel(e_data, LVTSEDATA00_0 + base + 0x4 * j);
		}
	}
}

static void init_controller_v4(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_write_device(lvts_data, SET_DEVICE_LOW_POWER_SINGLE_MODE_V4, i);

		writel(SET_SENSOR_INDEX, LVTSTSSEL_0 + base);
		writel(SET_CALC_SCALE_RULES, LVTSCALSCALE_0 + base);

		set_polling_speed(lvts_data, i);
		set_hw_filter(lvts_data, i);

		dev_info(dev, "lvts%d: read all %d sensors in %d us, one in %d us\n",
			i, GET_TC_SENSOR_NUM(i), GROUP_LATENCY_US(i),
			SENSOR_LATENCY_US(i));
	}

}

/*==================================================
 * LVTS MT6873
 *==================================================
 */

#define MT6873_NUM_LVTS (ARRAY_SIZE(mt6873_tc_settings))

enum mt6873_lvts_domain {
	MT6873_AP_DOMAIN,
	MT6873_MCU_DOMAIN,
	MT6873_NUM_DOMAIN
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
	}
};

static struct lvts_data mt6873_lvts_data = {
	.num_domain = MT6873_NUM_DOMAIN,
	.num_tc = MT6873_NUM_LVTS,
	.tc = mt6873_tc_settings,
	.num_sensor = MT6873_NUM_TS,
	.ops = {
		.efuse_to_cal_data = mt6873_efuse_to_cal_data,
		.device_enable_and_init = device_enable_and_init_v4,
		.device_enable_auto_rck = device_enable_auto_rck_v4,
		.device_read_count_rc_n = device_read_count_rc_n_v4,
		.set_cal_data = set_calibration_data_v4,
		.init_controller = init_controller_v4,
	},
	.feature_bitmap = FEATURE_DEVICE_AUTO_RCK,
	.num_efuse_addr = 22,
	.num_efuse_block = 2,
	.cal_data = {
		.default_golden_temp = 50,
		.default_count_r = 35000,
		.default_count_rc = 2750,
	},
	.coeff = {
		.a = -205460,
		.b = 205460,
	},
};
/*==================================================
 * Support chips
 *==================================================
 */
static const struct of_device_id lvts_of_match[] = {
	{
		.compatible = "mediatek,mt6873-lvts",
		.data = (void *)&mt6873_lvts_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, lvts_of_match);
/*==================================================*/
static struct platform_driver soc_temp_lvts = {
	.probe = lvts_probe,
	.remove = lvts_remove,
	.suspend = lvts_suspend,
	.resume = lvts_resume,
	.driver = {
		.name = "mtk-soc-temp-lvts",
		.of_match_table = lvts_of_match,
	},
};

module_platform_driver(soc_temp_lvts);
MODULE_AUTHOR("Yu-Chia Chang <ethan.chang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek soc temperature driver");
MODULE_LICENSE("GPL v2");

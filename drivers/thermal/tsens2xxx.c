/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/vmalloc.h>
#include "tsens.h"

#define TSENS_DRIVER_NAME			"msm-tsens"

#define TSENS_TM_INT_EN(n)			((n) + 0x1004)
#define TSENS_TM_CRITICAL_INT_STATUS(n)		((n) + 0x1014)
#define TSENS_TM_CRITICAL_INT_CLEAR(n)		((n) + 0x1018)
#define TSENS_TM_CRITICAL_INT_MASK(n)		((n) + 0x101c)
#define TSENS_TM_CRITICAL_WD_BARK		BIT(31)
#define TSENS_TM_CRITICAL_CYCLE_MONITOR		BIT(30)
#define TSENS_TM_CRITICAL_INT_EN		BIT(2)
#define TSENS_TM_UPPER_INT_EN			BIT(1)
#define TSENS_TM_LOWER_INT_EN			BIT(0)
#define TSENS_TM_SN_UPPER_LOWER_THRESHOLD(n)	((n) + 0x1020)
#define TSENS_TM_SN_ADDR_OFFSET			0x4
#define TSENS_TM_UPPER_THRESHOLD_SET(n)		((n) << 12)
#define TSENS_TM_UPPER_THRESHOLD_VALUE_SHIFT(n)	((n) >> 12)
#define TSENS_TM_LOWER_THRESHOLD_VALUE(n)	((n) & 0xfff)
#define TSENS_TM_UPPER_THRESHOLD_VALUE(n)	(((n) & 0xfff000) >> 12)
#define TSENS_TM_UPPER_THRESHOLD_MASK		0xfff000
#define TSENS_TM_LOWER_THRESHOLD_MASK		0xfff
#define TSENS_TM_UPPER_THRESHOLD_SHIFT		12
#define TSENS_TM_SN_CRITICAL_THRESHOLD(n)	((n) + 0x1060)
#define TSENS_STATUS_ADDR_OFFSET		2
#define TSENS_TM_UPPER_INT_MASK(n)		(((n) & 0xffff0000) >> 16)
#define TSENS_TM_LOWER_INT_MASK(n)		((n) & 0xffff)
#define TSENS_TM_UPPER_LOWER_INT_STATUS(n)	((n) + 0x1008)
#define TSENS_TM_UPPER_LOWER_INT_CLEAR(n)	((n) + 0x100c)
#define TSENS_TM_UPPER_LOWER_INT_MASK(n)	((n) + 0x1010)
#define TSENS_TM_UPPER_INT_SET(n)		(1 << (n + 16))
#define TSENS_TM_SN_CRITICAL_THRESHOLD_MASK	0xfff
#define TSENS_TM_SN_STATUS_VALID_BIT		BIT(21)
#define TSENS_TM_SN_STATUS_CRITICAL_STATUS	BIT(19)
#define TSENS_TM_SN_STATUS_UPPER_STATUS		BIT(18)
#define TSENS_TM_SN_STATUS_LOWER_STATUS		BIT(17)
#define TSENS_TM_SN_LAST_TEMP_MASK		0xfff
#define TSENS_TM_CODE_BIT_MASK			0xfff
#define TSENS_TM_CODE_SIGN_BIT			0x800

#define TSENS_EN				BIT(0)

static void msm_tsens_convert_temp(int last_temp, int *temp)
{
	int code_mask = ~TSENS_TM_CODE_BIT_MASK;

	if (last_temp & TSENS_TM_CODE_SIGN_BIT) {
		/* Sign extension for negative value */
		last_temp |= code_mask;
	}

	*temp = last_temp * 100;
}

static int tsens2xxx_get_temp(struct tsens_sensor *sensor, int *temp)
{
	struct tsens_device *tmdev = NULL;
	unsigned int code;
	void __iomem *sensor_addr;
	int last_temp = 0, last_temp2 = 0, last_temp3 = 0;

	if (!sensor)
		return -EINVAL;

	tmdev = sensor->tmdev;
	sensor_addr = TSENS_TM_SN_STATUS(tmdev->tsens_addr);

	code = readl_relaxed_no_log(sensor_addr +
			(sensor->hw_id << TSENS_STATUS_ADDR_OFFSET));
	last_temp = code & TSENS_TM_SN_LAST_TEMP_MASK;

	if (code & TSENS_TM_SN_STATUS_VALID_BIT) {
		msm_tsens_convert_temp(last_temp, temp);
		return 0;
	}

	code = readl_relaxed_no_log(sensor_addr +
		(sensor->hw_id << TSENS_STATUS_ADDR_OFFSET));
	last_temp2 = code & TSENS_TM_SN_LAST_TEMP_MASK;
	if (code & TSENS_TM_SN_STATUS_VALID_BIT) {
		last_temp = last_temp2;
		msm_tsens_convert_temp(last_temp, temp);
		return 0;
	}

	code = readl_relaxed_no_log(sensor_addr +
			(sensor->hw_id <<
			TSENS_STATUS_ADDR_OFFSET));
	last_temp3 = code & TSENS_TM_SN_LAST_TEMP_MASK;
	if (code & TSENS_TM_SN_STATUS_VALID_BIT) {
		last_temp = last_temp3;
		msm_tsens_convert_temp(last_temp, temp);
		return 0;
	}

	if (last_temp == last_temp2)
		last_temp = last_temp2;
	else if (last_temp2 == last_temp3)
		last_temp = last_temp3;

	msm_tsens_convert_temp(last_temp, temp);

	if (tmdev->ops->dbg)
		tmdev->ops->dbg(tmdev, (u32) sensor->hw_id,
					TSENS_DBG_LOG_TEMP_READS, temp);

	return 0;
}

static int tsens_tm_activate_trip_type(struct tsens_sensor *tm_sensor,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_device *tmdev = NULL;
	unsigned int reg_cntl, mask;
	unsigned long flags;
	int rc = 0;

	/* clear the interrupt and unmask */
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tmdev;
	if (!tmdev)
		return -EINVAL;

	spin_lock_irqsave(&tmdev->tsens_upp_low_lock, flags);
	mask = (tm_sensor->hw_id);
	switch (trip) {
	case THERMAL_TRIP_CRITICAL:
		tmdev->sensor[tm_sensor->hw_id].
			thr_state.crit_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_CRITICAL_INT_MASK
							(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl | (1 << mask),
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl & ~(1 << mask),
				(TSENS_TM_CRITICAL_INT_MASK
				(tmdev->tsens_addr)));
		break;
	case THERMAL_TRIP_ACTIVE:
		tmdev->sensor[tm_sensor->hw_id].
			thr_state.high_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_UPPER_LOWER_INT_MASK
						(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl |
				(TSENS_TM_UPPER_INT_SET(mask)),
				(TSENS_TM_UPPER_LOWER_INT_MASK
				(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl &
				~(TSENS_TM_UPPER_INT_SET(mask)),
				(TSENS_TM_UPPER_LOWER_INT_MASK
				(tmdev->tsens_addr)));
		break;
	case THERMAL_TRIP_PASSIVE:
		tmdev->sensor[tm_sensor->hw_id].
			thr_state.low_th_state = mode;
		reg_cntl = readl_relaxed(TSENS_TM_UPPER_LOWER_INT_MASK
						(tmdev->tsens_addr));
		if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
			writel_relaxed(reg_cntl | (1 << mask),
			(TSENS_TM_UPPER_LOWER_INT_MASK(tmdev->tsens_addr)));
		else
			writel_relaxed(reg_cntl & ~(1 << mask),
			(TSENS_TM_UPPER_LOWER_INT_MASK(tmdev->tsens_addr)));
		break;
	default:
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&tmdev->tsens_upp_low_lock, flags);
	/* Activate and enable the respective trip threshold setting */
	mb();

	return rc;
}

static int tsens2xxx_set_trip_temp(struct tsens_sensor *tm_sensor,
							int trip, int temp)
{
	unsigned int reg_cntl;
	unsigned long flags;
	struct tsens_device *tmdev = NULL;
	int rc = 0;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	tmdev = tm_sensor->tmdev;
	if (!tmdev)
		return -EINVAL;

	spin_lock_irqsave(&tmdev->tsens_upp_low_lock, flags);
	switch (trip) {
	case THERMAL_TRIP_CRITICAL:
		tmdev->sensor[tm_sensor->hw_id].
				thr_state.crit_temp = temp;
		temp &= TSENS_TM_SN_CRITICAL_THRESHOLD_MASK;
		writel_relaxed(temp,
			(TSENS_TM_SN_CRITICAL_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->hw_id * TSENS_TM_SN_ADDR_OFFSET)));
		break;
	case THERMAL_TRIP_ACTIVE:
		tmdev->sensor[tm_sensor->hw_id].
				thr_state.high_temp = temp;
		reg_cntl = readl_relaxed((TSENS_TM_SN_UPPER_LOWER_THRESHOLD
				(tmdev->tsens_addr)) +
				(tm_sensor->hw_id *
				TSENS_TM_SN_ADDR_OFFSET));
		temp = TSENS_TM_UPPER_THRESHOLD_SET(temp);
		temp &= TSENS_TM_UPPER_THRESHOLD_MASK;
		reg_cntl &= ~TSENS_TM_UPPER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | temp,
			(TSENS_TM_SN_UPPER_LOWER_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->hw_id * TSENS_TM_SN_ADDR_OFFSET)));
		break;
	case THERMAL_TRIP_PASSIVE:
		tmdev->sensor[tm_sensor->hw_id].
				thr_state.low_temp = temp;
		reg_cntl = readl_relaxed((TSENS_TM_SN_UPPER_LOWER_THRESHOLD
				(tmdev->tsens_addr)) +
				(tm_sensor->hw_id *
				TSENS_TM_SN_ADDR_OFFSET));
		temp &= TSENS_TM_LOWER_THRESHOLD_MASK;
		reg_cntl &= ~TSENS_TM_LOWER_THRESHOLD_MASK;
		writel_relaxed(reg_cntl | temp,
			(TSENS_TM_SN_UPPER_LOWER_THRESHOLD(tmdev->tsens_addr) +
			(tm_sensor->hw_id * TSENS_TM_SN_ADDR_OFFSET)));
		break;
	default:
		pr_err("Invalid trip to TSENS: %d\n", trip);
		rc = -EINVAL;
	}

	spin_unlock_irqrestore(&tmdev->tsens_upp_low_lock, flags);
	/* Set trip temperature thresholds */
	mb();

	rc = tsens_tm_activate_trip_type(tm_sensor, trip,
				THERMAL_TRIP_ACTIVATION_ENABLED);
	if (rc)
		pr_err("Error during trip activation :%d\n", rc);

	return rc;
}

static irqreturn_t tsens_tm_critical_irq_thread(int irq, void *data)
{
	struct tsens_device *tm = data;
	unsigned int i, status;
	unsigned long flags;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_int_mask_addr;
	void __iomem *sensor_critical_addr;
	void __iomem *wd_critical_addr;
	int wd_mask;

	sensor_status_addr = TSENS_TM_SN_STATUS(tm->tsens_addr);
	sensor_int_mask_addr =
		TSENS_TM_CRITICAL_INT_MASK(tm->tsens_addr);
	sensor_critical_addr =
		TSENS_TM_SN_CRITICAL_THRESHOLD(tm->tsens_addr);
	wd_critical_addr =
		TSENS_TM_CRITICAL_INT_STATUS(tm->tsens_addr);

	if (tm->ctrl_data->wd_bark) {
		wd_mask = readl_relaxed(wd_critical_addr);
		if (wd_mask & TSENS_TM_CRITICAL_WD_BARK) {
			/*
			 * Clear watchdog interrupt and
			 * increment global wd count
			 */
			writel_relaxed(wd_mask | TSENS_TM_CRITICAL_WD_BARK,
				(TSENS_TM_CRITICAL_INT_CLEAR
				(tm->tsens_addr)));
			writel_relaxed(wd_mask & ~(TSENS_TM_CRITICAL_WD_BARK),
				(TSENS_TM_CRITICAL_INT_CLEAR
				(tm->tsens_addr)));
			tm->tsens_dbg.tsens_critical_wd_cnt++;
			return IRQ_HANDLED;
		}
	}

	for (i = 0; i < tm->num_sensors; i++) {
		int int_mask, int_mask_val;
		u32 addr_offset;

		spin_lock_irqsave(&tm->tsens_crit_lock, flags);
		addr_offset = tm->sensor[i].hw_id *
						TSENS_TM_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		int_mask = readl_relaxed(sensor_int_mask_addr);

		if ((status & TSENS_TM_SN_STATUS_CRITICAL_STATUS) &&
			!(int_mask & (1 << tm->sensor[i].hw_id))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = (1 << tm->sensor[i].hw_id);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_CRITICAL_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_CRITICAL_INT_CLEAR(tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_CRITICAL_INT_CLEAR(
					tm->tsens_addr));
			tm->sensor[i].thr_state.
					crit_th_state = THERMAL_DEVICE_DISABLED;
		}
		spin_unlock_irqrestore(&tm->tsens_crit_lock, flags);
	}

	/* Mask critical interrupt */
	mb();

	return IRQ_HANDLED;
}

static irqreturn_t tsens_tm_irq_thread(int irq, void *data)
{
	struct tsens_device *tm = data;
	unsigned int i, status, threshold;
	unsigned long flags;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_int_mask_addr;
	void __iomem *sensor_upper_lower_addr;
	u32 addr_offset = 0;

	sensor_status_addr = TSENS_TM_SN_STATUS(tm->tsens_addr);
	sensor_int_mask_addr =
		TSENS_TM_UPPER_LOWER_INT_MASK(tm->tsens_addr);
	sensor_upper_lower_addr =
		TSENS_TM_SN_UPPER_LOWER_THRESHOLD(tm->tsens_addr);

	for (i = 0; i < tm->num_sensors; i++) {
		bool upper_thr = false, lower_thr = false;
		int int_mask, int_mask_val = 0;

		spin_lock_irqsave(&tm->tsens_upp_low_lock, flags);
		addr_offset = tm->sensor[i].hw_id *
						TSENS_TM_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		threshold = readl_relaxed(sensor_upper_lower_addr +
								addr_offset);
		int_mask = readl_relaxed(sensor_int_mask_addr);

		if ((status & TSENS_TM_SN_STATUS_UPPER_STATUS) &&
			!(int_mask &
				(1 << (tm->sensor[i].hw_id + 16)))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = TSENS_TM_UPPER_INT_SET(
					tm->sensor[i].hw_id);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			upper_thr = true;
			tm->sensor[i].thr_state.
					high_th_state = THERMAL_DEVICE_DISABLED;
		}

		if ((status & TSENS_TM_SN_STATUS_LOWER_STATUS) &&
			!(int_mask &
				(1 << tm->sensor[i].hw_id))) {
			int_mask = readl_relaxed(sensor_int_mask_addr);
			int_mask_val = (1 << tm->sensor[i].hw_id);
			/* Mask the corresponding interrupt for the sensors */
			writel_relaxed(int_mask | int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_MASK(
					tm->tsens_addr));
			/* Clear the corresponding sensors interrupt */
			writel_relaxed(int_mask_val,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			writel_relaxed(0,
				TSENS_TM_UPPER_LOWER_INT_CLEAR(
					tm->tsens_addr));
			lower_thr = true;
			tm->sensor[i].thr_state.
					low_th_state = THERMAL_DEVICE_DISABLED;
		}
		spin_unlock_irqrestore(&tm->tsens_upp_low_lock, flags);

		if (upper_thr || lower_thr) {
			int temp;
			enum thermal_trip_type trip =
					THERMAL_TRIP_CONFIGURABLE_LOW;

			if (upper_thr)
				trip = THERMAL_TRIP_CONFIGURABLE_HI;
			tsens2xxx_get_temp(&tm->sensor[i], &temp);
			/* Use id for multiple controllers */
			pr_debug("sensor:%d trigger temp (%d degC)\n",
				tm->sensor[i].hw_id,
				(status & TSENS_TM_SN_LAST_TEMP_MASK));
		}
	}

	/* Disable monitoring sensor trip threshold for triggered sensor */
	mb();

	if (tm->ops->dbg)
		tm->ops->dbg(tm, 0, TSENS_DBG_LOG_INTERRUPT_TIMESTAMP, NULL);

	return IRQ_HANDLED;
}

static int tsens2xxx_hw_init(struct tsens_device *tmdev)
{
	void __iomem *srot_addr;
	void __iomem *sensor_int_mask_addr;
	unsigned int srot_val;
	int crit_mask;

	srot_addr = TSENS_CTRL_ADDR(tmdev->tsens_addr + 0x4);
	srot_val = readl_relaxed(srot_addr);
	if (!(srot_val & TSENS_EN)) {
		pr_err("TSENS device is not enabled\n");
		return -ENODEV;
	}

	if (tmdev->ctrl_data->cycle_monitor) {
		sensor_int_mask_addr =
			TSENS_TM_CRITICAL_INT_MASK(tmdev->tsens_addr);
		crit_mask = readl_relaxed(sensor_int_mask_addr);
		writel_relaxed(
			crit_mask | tmdev->ctrl_data->cycle_compltn_monitor_val,
			(TSENS_TM_CRITICAL_INT_MASK
			(tmdev->tsens_addr)));
		/*Update critical cycle monitoring*/
		mb();
	}
	writel_relaxed(TSENS_TM_CRITICAL_INT_EN |
		TSENS_TM_UPPER_INT_EN | TSENS_TM_LOWER_INT_EN,
		TSENS_TM_INT_EN(tmdev->tsens_addr));

	spin_lock_init(&tmdev->tsens_crit_lock);
	spin_lock_init(&tmdev->tsens_upp_low_lock);

	return 0;
}

static const struct tsens_irqs tsens2xxx_irqs[] = {
	{ "tsens-upper-lower", tsens_tm_irq_thread},
	{ "tsens-critical", tsens_tm_critical_irq_thread},
};

static int tsens2xxx_register_interrupts(struct tsens_device *tmdev)
{
	struct platform_device *pdev;
	int i, rc;

	if (!tmdev)
		return -EINVAL;

	pdev = tmdev->pdev;

	for (i = 0; i < ARRAY_SIZE(tsens2xxx_irqs); i++) {
		int irq;

		irq = platform_get_irq_byname(pdev, tsens2xxx_irqs[i].name);
		if (irq < 0) {
			dev_err(&pdev->dev, "failed to get irq %s\n",
					tsens2xxx_irqs[i].name);
			return irq;
		}

		rc = devm_request_threaded_irq(&pdev->dev, irq, NULL,
				tsens2xxx_irqs[i].handler,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				tsens2xxx_irqs[i].name, tmdev);
		if (rc) {
			dev_err(&pdev->dev, "failed to get irq %s\n",
					tsens2xxx_irqs[i].name);
			return rc;
		}
		enable_irq_wake(irq);
	}

	return 0;
}

static const struct tsens_ops ops_tsens2xxx = {
	.hw_init	= tsens2xxx_hw_init,
	.get_temp	= tsens2xxx_get_temp,
	.set_trip_temp	= tsens2xxx_set_trip_temp,
	.interrupts_reg	= tsens2xxx_register_interrupts,
	.dbg		= tsens2xxx_dbg,
};

const struct tsens_data data_tsens2xxx = {
	.cycle_monitor			= false,
	.cycle_compltn_monitor_val	= 0,
	.wd_bark			= false,
	.wd_bark_val			= 0,
	.ops				= &ops_tsens2xxx,
};

const struct tsens_data data_tsens23xx = {
	.cycle_monitor			= true,
	.cycle_compltn_monitor_val	= 0,
	.wd_bark			= true,
	.wd_bark_val			= 0,
	.ops				= &ops_tsens2xxx,
};

const struct tsens_data data_tsens24xx = {
	.cycle_monitor			= true,
	.cycle_compltn_monitor_val	= 0,
	.wd_bark			= true,
	.wd_bark_val			= 1,
	.ops				= &ops_tsens2xxx,
};

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

#include "tsens.h"
#include "tsens-mtc.h"

struct tsens_device *tsens_controller_is_present(void)
{
	struct tsens_device *tmdev_chip = NULL;

	if (list_empty(&tsens_device_list)) {
		pr_err("%s: TSENS controller not available\n", __func__);
		return tmdev_chip;
	}

	list_for_each_entry(tmdev_chip, &tsens_device_list, list)
		return tmdev_chip;

	return tmdev_chip;
}
EXPORT_SYMBOL(tsens_controller_is_present);

static int tsens_mtc_reset_history_counter(unsigned int zone)
{
	unsigned int reg_cntl, is_valid;
	void __iomem *sensor_addr;
	struct tsens_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_SW_MASK_ADDR(tmdev->tsens_tm_addr);
	reg_cntl = readl_relaxed((sensor_addr +
		(zone * TSENS_SN_ADDR_OFFSET)));
	is_valid = (reg_cntl & TSENS_RESET_HISTORY_MASK)
				>> TSENS_RESET_HISTORY_SHIFT;
	if (!is_valid) {
		/*Enable the bit to reset counter*/
		writel_relaxed(reg_cntl | (1 << TSENS_RESET_HISTORY_SHIFT),
				(sensor_addr + (zone * TSENS_SN_ADDR_OFFSET)));
		reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
		pr_debug("tsens : zone =%d reg=%x\n", zone, reg_cntl);
	}

	/*Disble the bit to start counter*/
	writel_relaxed(reg_cntl & ~(1 << TSENS_RESET_HISTORY_SHIFT),
				(sensor_addr + (zone * TSENS_SN_ADDR_OFFSET)));
	reg_cntl = readl_relaxed((sensor_addr +
			(zone * TSENS_SN_ADDR_OFFSET)));
	pr_debug("tsens : zone =%d reg=%x\n", zone, reg_cntl);

	return 0;
}
EXPORT_SYMBOL(tsens_mtc_reset_history_counter);

int tsens_set_mtc_zone_sw_mask(unsigned int zone, unsigned int th1_enable,
				unsigned int th2_enable)
{
	unsigned int reg_cntl;
	void __iomem *sensor_addr;
	struct tsens_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_SW_MASK_ADDR
					(tmdev->tsens_tm_addr);

	if (th1_enable && th2_enable)
		writel_relaxed(TSENS_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (!th1_enable && !th2_enable)
		writel_relaxed(TSENS_MTC_DISABLE,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (th1_enable && !th2_enable)
		writel_relaxed(TSENS_TH1_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	if (!th1_enable && th2_enable)
		writel_relaxed(TSENS_TH2_MTC_IN_EFFECT,
				(sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	pr_debug("tsens : zone =%d th1=%d th2=%d reg=%x\n",
		zone, th1_enable, th2_enable, reg_cntl);

	return 0;
}
EXPORT_SYMBOL(tsens_set_mtc_zone_sw_mask);

int tsens_get_mtc_zone_log(unsigned int zone, void *zone_log)
{
	unsigned int i, reg_cntl, is_valid, log[TSENS_MTC_ZONE_LOG_SIZE];
	int *zlog = (int *)zone_log;
	void __iomem *sensor_addr;
	struct tsens_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_LOG(tmdev->tsens_tm_addr);

	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));
	is_valid = (reg_cntl & TSENS_LOGS_VALID_MASK)
				>> TSENS_LOGS_VALID_SHIFT;
	if (is_valid) {
		log[0] = (reg_cntl & TSENS_LOGS_LATEST_MASK);
		log[1] = (reg_cntl & TSENS_LOGS_LOG1_MASK)
				>> TSENS_LOGS_LOG1_SHIFT;
		log[2] = (reg_cntl & TSENS_LOGS_LOG2_MASK)
				>> TSENS_LOGS_LOG2_SHIFT;
		log[3] = (reg_cntl & TSENS_LOGS_LOG3_MASK)
				>> TSENS_LOGS_LOG3_SHIFT;
		log[4] = (reg_cntl & TSENS_LOGS_LOG4_MASK)
				>> TSENS_LOGS_LOG4_SHIFT;
		log[5] = (reg_cntl & TSENS_LOGS_LOG5_MASK)
				>> TSENS_LOGS_LOG5_SHIFT;
		for (i = 0; i < (TSENS_MTC_ZONE_LOG_SIZE); i++) {
			*(zlog+i) = log[i];
			pr_debug("Log[%d]=%d\n", i, log[i]);
		}
	} else {
		pr_debug("tsens: Valid bit disabled\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(tsens_get_mtc_zone_log);

int tsens_get_mtc_zone_history(unsigned int zone, void *zone_hist)
{
	unsigned int i, reg_cntl, hist[TSENS_MTC_ZONE_HISTORY_SIZE];
	int *zhist = (int *)zone_hist;
	void __iomem *sensor_addr;
	struct tsens_device *tmdev = NULL;

	if (zone > TSENS_NUM_MTC_ZONES_SUPPORT)
		return -EINVAL;

	tmdev = tsens_controller_is_present();
	if (!tmdev) {
		pr_err("No TSENS controller present\n");
		return -EPROBE_DEFER;
	}

	sensor_addr = TSENS_TM_MTC_ZONE0_HISTORY(tmdev->tsens_tm_addr);
	reg_cntl = readl_relaxed((sensor_addr +
				(zone * TSENS_SN_ADDR_OFFSET)));

	hist[0] = (reg_cntl & TSENS_PS_COOL_CMD_MASK);
	hist[1] = (reg_cntl & TSENS_PS_YELLOW_CMD_MASK)
			>> TSENS_PS_YELLOW_CMD_SHIFT;
	hist[2] = (reg_cntl & TSENS_PS_RED_CMD_MASK)
			>> TSENS_PS_RED_CMD_SHIFT;
	for (i = 0; i < (TSENS_MTC_ZONE_HISTORY_SIZE); i++) {
		*(zhist+i) = hist[i];
		pr_debug("tsens : %d\n", hist[i]);
	}

	return 0;
}
EXPORT_SYMBOL(tsens_get_mtc_zone_history);

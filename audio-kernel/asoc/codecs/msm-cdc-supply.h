/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __CODEC_POWER_SUPPLY_H__
#define __CODEC_POWER_SUPPLY_H__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

struct cdc_regulator {
	const char *name;
	int min_uV;
	int max_uV;
	int optimum_uA;
	bool ondemand;
	struct regulator *regulator;
};

extern int msm_cdc_get_power_supplies(struct device *dev,
				      struct cdc_regulator **cdc_vreg,
				      int *total_num_supplies);
extern int msm_cdc_disable_static_supplies(struct device *dev,
					struct regulator_bulk_data *supplies,
					struct cdc_regulator *cdc_vreg,
					int num_supplies);
extern int msm_cdc_release_supplies(struct device *dev,
				    struct regulator_bulk_data *supplies,
				    struct cdc_regulator *cdc_vreg,
				    int num_supplies);
extern int msm_cdc_enable_static_supplies(struct device *dev,
					  struct regulator_bulk_data *supplies,
					  struct cdc_regulator *cdc_vreg,
					  int num_supplies);
extern int msm_cdc_init_supplies(struct device *dev,
				 struct regulator_bulk_data **supplies,
				 struct cdc_regulator *cdc_vreg,
				 int num_supplies);
#endif

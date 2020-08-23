// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/platform_device.h>

#include "reviser_cmn.h"
#include "reviser_power.h"
#include "reviser_drv.h"
#include "apusys_power.h"



bool reviser_is_power(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	unsigned long flags;
	bool is_power = false;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return is_power;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	spin_lock_irqsave(&rdv->lock_power, flags);
	if (rdv->power) {
		//LOG_ERR("Can Not Read when power disable\n");
		is_power = true;
	}
	spin_unlock_irqrestore(&rdv->lock_power, flags);

	return is_power;
}

int reviser_power_on(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->mutex_power);
	if (rdv->power_count == 0) {

		ret = apu_device_power_on(REVISER);
		if (ret < 0)
			LOG_ERR("PowerON Fail (%d)\n", ret);

	}
	rdv->power_count++;
	mutex_unlock(&rdv->mutex_power);

	return ret;
}

int reviser_power_off(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->mutex_power);
	rdv->power_count--;

	if (rdv->power_count == 0) {

		ret = apu_device_power_off(REVISER);
		if (ret < 0)
			LOG_ERR("PowerON Fail (%d)\n", ret);

	}
	mutex_unlock(&rdv->mutex_power);

	return ret;
}

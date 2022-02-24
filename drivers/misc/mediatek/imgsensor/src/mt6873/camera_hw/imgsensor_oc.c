// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "upmu_common.h"

#include "imgsensor_oc.h"

#ifdef IMGSENSOR_OC_ENABLE

static void imgsensor_oc_handler(void)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	PK_DBG("[regulator]%s\n", __func__);
	pimgsensor->status.oc = 1;
}

enum IMGSENSOR_RETURN imgsensor_oc_init(void)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	/* Register your interrupt handler of OC interrupt at first */
	pmic_register_interrupt_callback(INT_VCAMIO_OC, imgsensor_oc_handler);

	pimgsensor->status.oc = 0;

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_oc_interrupt(
	enum IMGSENSOR_HW_POWER_STATUS pwr_status)
{
	struct IMGSENSOR *pimgsensor = &gimgsensor;

	mdelay(5);

	pimgsensor->status.oc = 0;

	if (pwr_status == IMGSENSOR_HW_POWER_STATUS_ON) {
		/* enable interrupt after power on */
		/* At least delay 3ms after power for recommendation */
		pmic_enable_interrupt(INT_VCAMIO_OC, 1, "camera");
	} else {
		/* Disable interrupt before power off */
		pmic_enable_interrupt(INT_VCAMIO_OC, 0, "camera");
	}

	return IMGSENSOR_RETURN_SUCCESS;
}

#endif


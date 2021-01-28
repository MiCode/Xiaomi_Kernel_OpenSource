// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/module.h>

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ACCELEROMETER)
#include "accel.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
#include "gyroscope.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_MAGNETOMETER)
#include "mag.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ALSPS)
#include "alsps.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
#include "barometer.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_STEP_COUNTER)
#include "step_counter.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SITUATION)
#include "situation.h"
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SENSOR_FUSION)
#include "fusion.h"
#endif

static int __init sensor_init(void)
{
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ACCELEROMETER)
	if (acc_probe())
		pr_err("failed to register acc driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
	if (gyro_probe())
		pr_err("failed to register gyro driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_MAGNETOMETER)
	if (mag_probe())
		pr_err("failed to register mag driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ALSPS)
	if (alsps_probe())
		pr_err("failed to register alsps driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
	if (baro_probe())
		pr_err("failed to register baro driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_STEP_COUNTER)
	if (step_c_probe())
		pr_err("failed to register step_c driver\n");
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SITUATION)
	if (situation_probe()) {
		pr_err("failed to register situ driver\n");
		return -ENODEV;
	}
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SENSOR_FUSION)
	if (fusion_probe()) {
		pr_err("failed to register fusion driver\n");
		return -ENODEV;
	}
#endif

	return 0;
}

static void __exit sensor_exit(void)
{
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ACCELEROMETER)
	acc_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
	gyro_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_MAGNETOMETER)
	mag_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_ALSPS)
	alsps_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
	baro_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_STEP_COUNTER)
	step_c_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SITUATION)
	situation_remove();
#endif

#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_SENSOR_FUSION)
	fusion_remove();
#endif

}

late_initcall(sensor_init);
module_exit(sensor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SensorProbe driver");
MODULE_AUTHOR("Mediatek");


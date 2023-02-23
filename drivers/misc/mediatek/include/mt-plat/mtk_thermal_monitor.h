/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_THERMAL_MONITOR_H
#define _MTK_THERMAL_MONITOR_H

#include <linux/thermal.h>

/*
 *  MTK_THERMAL_WRAPPER_BYPASS = 1 (use original Linux Thermal API)
 *  MTK_THERMAL_WRAPPER_BYPASS = 0 (use MTK Thermal API Monitor)
 */
#define MTK_THERMAL_WRAPPER_BYPASS 0

#if MTK_THERMAL_WRAPPER_BYPASS
/* Original LTF API */
#define mtk_thermal_zone_device_register      thermal_zone_device_register
#define mtk_thermal_zone_device_unregister    thermal_zone_device_unregister
#define mtk_thermal_cooling_device_unregister thermal_cooling_device_unregister
#define mtk_thermal_cooling_device_register   thermal_cooling_device_register
#define mtk_thermal_zone_bind_cooling_device  thermal_zone_bind_cooling_device

#else

struct thermal_cooling_device_ops_extra {
	int (*set_cur_temp)(struct thermal_cooling_device *cdev,
		unsigned long temperature);
};

extern
struct thermal_zone_device *mtk_thermal_zone_device_register_wrapper
(char *type, int trips, void *devdata,
const struct thermal_zone_device_ops *ops,
int tc1, int tc2, int passive_delay, int polling_delay);

extern
void mtk_thermal_zone_device_unregister_wrapper
(struct thermal_zone_device *tz);

extern
struct thermal_cooling_device *mtk_thermal_cooling_device_register_wrapper
(char *type, void *devdata, const struct thermal_cooling_device_ops *ops);

extern
struct thermal_cooling_device
*mtk_thermal_cooling_device_register_wrapper_extra
(char *type, void *devdata, const struct thermal_cooling_device_ops *ops,
const struct thermal_cooling_device_ops_extra *ops_ext);

extern
int mtk_thermal_cooling_device_add_exit_point
(struct thermal_cooling_device *cdev, int exit_point);

extern
void mtk_thermal_cooling_device_unregister_wrapper
(struct thermal_cooling_device *cdev);

extern int mtk_thermal_zone_bind_cooling_device_wrapper
(struct thermal_zone_device *tz, int trip,
struct thermal_cooling_device *cdev);

extern int mtk_thermal_zone_bind_trigger_trip
(struct thermal_zone_device *tz, int trip, int mode);
#define mtk_thermal_zone_device_register	\
		mtk_thermal_zone_device_register_wrapper

#define mtk_thermal_zone_device_unregister	\
		mtk_thermal_zone_device_unregister_wrapper

#define mtk_thermal_cooling_device_unregister	\
		mtk_thermal_cooling_device_unregister_wrapper

#define mtk_thermal_cooling_device_register	\
		mtk_thermal_cooling_device_register_wrapper

#define mtk_thermal_zone_bind_cooling_device	\
		mtk_thermal_zone_bind_cooling_device_wrapper

#endif

enum mtk_thermal_sensor_id {
	MTK_THERMAL_SENSOR_CPU = 0,
	MTK_THERMAL_SENSOR_ABB,
	MTK_THERMAL_SENSOR_PMIC,
	MTK_THERMAL_SENSOR_BATTERY,
	MTK_THERMAL_SENSOR_MD1,
	MTK_THERMAL_SENSOR_MD2,
	MTK_THERMAL_SENSOR_WIFI,
	MTK_THERMAL_SENSOR_BATTERY2,
	MTK_THERMAL_SENSOR_BUCK,
	MTK_THERMAL_SENSOR_AP,
	MTK_THERMAL_SENSOR_PCB1,
	MTK_THERMAL_SENSOR_PCB2,
	MTK_THERMAL_SENSOR_SKIN,
	MTK_THERMAL_SENSOR_XTAL,
	MTK_THERMAL_SENSOR_MD_PA,
	MTK_THERMAL_SENSOR_NR_PA,
	MTK_THERMAL_SENSOR_DCTM,
	MTK_THERMAL_SENSOR_CHARGER,

	MTK_THERMAL_SENSOR_COUNT
};

extern int mtk_thermal_get_temp(enum mtk_thermal_sensor_id id);
extern struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void);

/* This API function is implemented in mediatek/kernel/drivers/leds/leds.c */
#if defined(CONFIG_LEDS_MTK_DISP) || defined(CONFIG_LEDS_MTK_PWM) || defined(CONFIG_LEDS_MTK_I2C)
extern int mt_leds_max_brightness_set(char *name, int percent, bool enable);
#else
extern int setMaxbrightness(int max_level, int enable);
#endif

extern void machine_power_off(void);
#endif

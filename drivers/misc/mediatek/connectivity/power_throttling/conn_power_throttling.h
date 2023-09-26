// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define CONN_PWR_SET_CUSTOMER_POWER_LEVEL(value, index, lv) \
((value) = (((lv) << ((index) * 8)) | ((value) & ~(0xFF << ((index) * 8)))))

#define CONN_PWR_GET_CUSTOMER_POWER_LEVEL(value, index) ((value >> (index * 8)) & 0xFF)

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
#define CONFIG_CONN_PWR_DEBUG 1
#endif

struct conn_pwr_plat_info {
	int chip_id;		/* platform chip id */
	int adie_id;		/* a die chip id */
	int (*get_temp)(int *temp); /* callback for getting connsys temperature */
};

enum conn_pwr_drv_type {
	CONN_PRW_DRV_ALL = -1,
	CONN_PWR_DRV_BT = 0,
	CONN_PWR_DRV_FM = 1,
	CONN_PWR_DRV_GPS = 2,
	CONN_PWR_DRV_WIFI = 3,
	CONN_PWR_DRV_MAX
};

enum conn_pwr_drv_status {
	CONN_PWR_DRV_STATUS_OFF = 0,
	CONN_PWR_DRV_STATUS_ON = 1,
	CONN_PWR_DRV_STATUS_MAX
};

enum conn_pwr_plat_type {
	CONN_PWR_PLAT_LOW_BATTERY = 0,
	CONN_PWR_PLAT_THERMAL = 1,
	CONN_PWR_PLAT_CUSTOMER = 2,
	CONN_PWR_PLAT_MAX
};

enum conn_pwr_msg_type {
	CONN_PWR_MSG_TEMP_TOO_HIGH = 0,
	CONN_PWR_MSG_TEMP_RECOVERY = 1,
	CONN_PWR_MSG_GET_TEMP = 2,
	CONN_PWR_MSG_MAX
};

enum conn_pwr_low_battery_level {
	CONN_PWR_THR_LV_0 = 0,
	CONN_PWR_THR_LV_1 = 1,
	CONN_PWR_THR_LV_2 = 2,
	CONN_PWR_THR_LV_3 = 3,
	CONN_PWR_THR_LV_4 = 4,
	CONN_PWR_THR_LV_5 = 5,
	CONN_PWR_LOW_BATTERY_MAX
};

enum conn_pwr_arb_reason {
	CONN_PWR_ARB_SUBSYS_ON_OFF = 0,
	CONN_PWR_ARB_LOW_BATTERY = 1,
	CONN_PWR_ARB_THERMAL = 2,
	CONN_PWR_ARB_CUSTOMER = 3,
	CONN_PWR_ARB_TEMP_CHECK = 4,
	CONN_PWR_ARB_MAX
};

struct conn_pwr_update_info {
	enum conn_pwr_arb_reason reason;
	enum conn_pwr_drv_type   drv;
	enum conn_pwr_drv_status status;
};

enum conn_pwr_event_type {
	CONN_PWR_EVENT_LEVEL = 0,
	CONN_PWR_EVENT_MAX_TEMP = 1,
	CONN_PWR_EVENT_MAX
};

struct conn_pwr_event_max_temp {
	int max_temp;
	int recovery_temp;
};

typedef int (*CONN_PWR_EVENT_CB)(enum conn_pwr_event_type type, void *data);

/* called by conn_pwr_core */
int conn_pwr_get_chip_id(void);
int conn_pwr_get_adie_id(void);
int conn_pwr_get_temp(int *temp, int cached);
int conn_pwr_get_plat_level(enum conn_pwr_plat_type type, int *data);
int conn_pwr_get_drv_status(enum conn_pwr_drv_type type);
int conn_pwr_notify_event(enum conn_pwr_drv_type drv, enum conn_pwr_event_type event, void *data);

/* called by customer */
int conn_pwr_set_customer_level(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level level);

/* called by conn_pwr_adapter */
int conn_pwr_get_drv_level(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level *level);

/* called by conn_pwr_adapter */
int conn_pwr_get_platform_level(enum conn_pwr_plat_type type, int *level);

/* called by conn_pwr_adapter */
int conn_pwr_get_thermal(struct conn_pwr_event_max_temp *temp);

/* called by conn_pwr_adapter */
int conn_pwr_arbitrate(struct conn_pwr_update_info *info);

/* called by subsys driver */
int conn_pwr_register_event_cb(enum conn_pwr_drv_type type,
				CONN_PWR_EVENT_CB cb);

/* called by subsys driver */
int conn_pwr_send_msg(enum conn_pwr_drv_type drv, enum conn_pwr_msg_type msg, void *data);

/* called by adapter */
int conn_pwr_core_init(void);
int conn_pwr_core_resume(void);
int conn_pwr_core_suspend(void);

/* called by adapter */
int conn_pwr_core_enable(int enable);

/* called by WMT/conninfra */
int conn_pwr_init(struct conn_pwr_plat_info *data);
int conn_pwr_deinit(void);
int conn_pwr_resume(void);
int conn_pwr_suspend(void);

/* called by subsys before function on */
int conn_pwr_drv_pre_on(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level *level);

/* called by subsys after function off */
int conn_pwr_drv_post_off(enum conn_pwr_drv_type type);

/* called by subsys to report min. level required */
int conn_pwr_report_level_required(enum conn_pwr_drv_type type,
					enum conn_pwr_low_battery_level level);

/* called by enable/disable */
int conn_pwr_enable(int enable);

/* called by UT */
int conn_pwr_set_max_temp(unsigned long arg);
int conn_pwr_set_battery_level(int level);

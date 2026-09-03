/* SPDX-License-Identifier: GPL-2.0 */
/*
 * File:shub_core.h
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 */

#ifndef SPRD_SHUB_CORE_H
#define SPRD_SHUB_CORE_H

#include "shub_common.h"

#define MAG_CHARGING_MODE 1

#if MAG_CHARGING_MODE
#include <linux/power_supply.h>
#include <linux/notifier.h>
#endif

#define SENSOR_TYPE_CALIBRATION_CFG	26
#define SHUB_NAME			"sprd-sensor"
#define SHUB_RD				"shub_rd"
#define SHUB_RD_NWU			"shub_rd_nwu"
#define SHUB_SENSOR_NUM			7
#define SHUB_NODATA			0xff
#define HOST_REQUEST_WRITE		0x74
#define HOST_REQUEST_READ		0x75
#define HOST_REQUEST_LEN		2
#define RECEIVE_TIMEOUT_MS		100
#define MAX_SENSOR_LOG_CTL_FLAG_LEN	8
#define MAX_CALI_LEN			128
#define LOG_CTL_OUTPUT_FLAG		5
#define SIPC_PM_BUFID0			0
#define SIPC_PM_BUFID1			1
#define SHUB_IIO_CHN_BITS		64

#define MAX_STRING_SIZE			1200

/* ms,-1 is wait  forever */
#define SIPC_WRITE_TIMEOUT		-1

enum calib_cmd {
	CALIB_EN,
	CALIB_CHECK_STATUS,
	CALIB_DATA_WRITE,
	CALIB_DATA_READ,
	CALIB_FLASH_WRITE,
	CALIB_FLASH_READ,
};

enum calib_type {
	CALIB_TYPE_NON,
	CALIB_TYPE_DEFAULT,
	CALIB_TYPE_SELFTEST,
	CALIB_TYPE_SENSORS_ENABLE,
	CALIB_TYPE_SENSORS_DISABLE,
};

enum calib_status {
	CALIB_STATUS_OUT_OF_MINRANGE = -3,
	CALIB_STATUS_OUT_OF_RANGE = -2,
	CALIB_STATUS_FAIL = -1,
	CALIB_STATUS_NON = 0,
	CALIB_STATUS_INPROCESS = 1,
	CALIB_STATUS_PASS = 2,
};

struct  sensor_info {
	u8 en;
	u8 mode;
	u8 rate;
	u16 timeout;
};

struct sensor_report_format {
	u8 cmd;
	u8 length;
	u16 id;
	u8 data[];
};

/*
 * Description:
 * Control Handle
 */
enum handle_id {
	NON_WAKEUP_HANDLE,
	WAKEUP_HANDLE,
	INTERNAL_HANDLE,
	HANDLE_ID_END
};

enum scan_status {
	SHUB_SCAN_ID,
	SHUB_SCAN_RAW_0,
	SHUB_SCAN_RAW_1,
	SHUB_SCAN_RAW_2,
	SHUB_SCAN_TIMESTAMP,
};

enum shub_mode {
	SHUB_BOOT,
	SHUB_CALIDOWNLOAD,
	SHUB_NORMAL,
	SHUB_SLEEP,
	SHUB_NO_SLEEP,
	SHUB_ASSERT,
};

enum shub_cmd {
	/* MCU Internal Cmd: Range 0~63 */
	/* Driver Control enable/disable/set rate */
	MCU_CMD = 0,
	MCU_SENSOR_EN,
	MCU_SENSOR_DSB,
	MCU_SENSOR_SET_RATE,
	/* Sensors Calibrator */
	MCU_SENSOR_CALIB,
	MCU_SENSOR_SELFTEST,
	/* User profile */
	MCU_SENSOR_SETTING,
	KNL_CMD = 64,
	KNL_SEN_DATA,
	 /* for multi package use */
	KNL_SEN_DATAPAC,
	KNL_EN_LIST,
	KNL_LOG,
	KNL_RESET_SENPWR,
	KNL_CALIBINFO,
	KNL_CALIBDATA,
	HAL_CMD = 128,
	HAL_SEN_DATA,
	HAL_FLUSH,
	HAL_LOG,
	HAL_SENSOR_INFO,
	HAL_LOG_CTL,
	HAL_CALI_STORE,
	HAL_SHUB_MODE_STORE,
	SPECIAL_CMD = 192,
	MCU_RDY,
	COMM_DRV_RDY,
};

struct sensor_batch_cmd {
	int handle;
	int report_rate;
	s64 batch_timeout;
};

struct sensor_log_control {
	u8 cmd;
	u8 length;
	u8 debug_data[5];
	u32 udata[MAX_SENSOR_LOG_CTL_FLAG_LEN];
};

struct customer_data_get {
	u8 type;
	u8 customer_data[30];
	u8 length;
};

struct shub_mode_info {
	u8 cmd;
	u8 info;
};

/*add for write cali data back to file node*/
struct sensor_cali_store {
	u8 cmd;
	u8 length;
	u16 type;
	u8 udata[CALIBRATION_DATA_LENGTH];
};

struct sois_cmd {
	u8 cmd;
	u8 mode;
	u16 gyro_gain_x;
	u16 gyro_gain_y;
};

#pragma pack(1)
struct sensor_info_t {
	char name[21];
	char vendor[20];
	int32_t version;
	int32_t handle;
	float maxrange;
	float resolution;
	float power;
	int32_t mindelay_us;
	uint32_t fifo_reserved_event_count;
	uint32_t fifo_max_event_count;
	int32_t maxdelay_us;
};
#pragma pack()

struct shub_data {
	struct platform_device *sensor_pdev;
	enum shub_mode mcu_mode;
	/* enable & batch list */
	u32 enabled_list[HANDLE_ID_END];
	u32 interrupt_status;
	/* sipc sensorhub id value */
	u32 sipc_sensorhub_id;
	/* Calibrator status */
	int cal_cmd;
	int cal_type;
	int cal_id;
	int golden_sample;
	int cal_savests;
	s32 iio_data[6];
	struct iio_dev *indio_dev;
	struct irq_work iio_irq_work;
	struct iio_trigger  *trig;
	atomic_t pseudo_irq_enable;
	struct class *sensor_class;
	struct device *sensor_dev;
	wait_queue_head_t rxwq;
	struct mutex mutex_lock;	/* mutex for trigger and raw read */
	struct mutex mutex_read;	/* mutex for sipc read */
	struct mutex mutex_send;	/* mutex for sending event */
	bool rx_status;
	u8 *rx_buf;
	u32 rx_len;
	/* sprd begin */
	wait_queue_head_t  rw_wait_queue;
	struct sent_cmd  sent_cmddata;
	struct mutex send_command_mutex;	/* mutex for sending command */
	u8 *regs_addr_buf;
	u8 *regs_value_buf;
	u8 regs_num;
	struct file *filep;/* R/W interface */
	unsigned char readbuff[SERIAL_READ_BUFFER_MAX];
	unsigned char readbuff_nwu[SERIAL_READ_BUFFER_MAX];
	unsigned char writebuff[MAX_MSG_BUFF_SIZE];
	void (*save_mag_offset)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*data_callback)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*readcmd_callback)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*cm4_read_callback)(struct shub_data *sensor,
				  enum shub_subtype_id subtype,
				  u8 *buff, u32 len);
	struct sensor_log_control log_control;
	struct sensor_cali_store cali_store;
	struct workqueue_struct *driver_wq;
	struct delayed_work time_sync_work;
	atomic_t delay;
	struct notifier_block early_suspend;
	struct notifier_block shub_reboot_notifier;
	u8 cm4_operate_data[6];
	struct sensor_info_t sensor_info_list[DRV_SENSOR_COUNT];
#if MAG_CHARGING_MODE
	struct notifier_block current_now_nb;
	int mag_current_now;
	struct delayed_work mag_current_now_work;
#endif
};

/* hw sensor id */
enum _id_status {
	_IDSTA_NOT = 0,
	_IDSTA_OK  = 1,
	_IDSTA_FAIL = 2,
};

enum show_order {
	ORDER_ACC = SENSOR_ACCELEROMETER,
	ORDER_MAG = SENSOR_GEOMAGNETIC_FIELD,
	ORDER_GYRO = SENSOR_GYROSCOPE,
	ORDER_LIGHT = SENSOR_LIGHT,
	ORDER_PRS = SENSOR_PRESSURE,
	ORDER_PROX = SENSOR_PROXIMITY,
	ORDER_COLOR_TEMP = SENSOR_COLOR_TEMP,
};

#define _HW_SENSOR_TOTAL 7
#define VENDOR_ID_OFFSET 256
struct hw_sensor_id_tag {
	u8 id_status;
	u8 vendor_id;
	u8 chip_id;
	int sensor_type;
	char pname[128];
};

struct id_to_name_tag {
	u32 id;
	char *pname;
};

#pragma pack(1)

struct shub_sensor_event {
	u16 sensor_handle;
	float fdata[6];
	u64 timestamp;
};

struct sensor_event_data_t {
	u8 cmd;
	struct shub_sensor_event shub_sensor_event_t;
};
#pragma pack()
extern struct shub_data *g_sensor;
#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef SCP_SENSOR_HUB_H
#define SCP_SENSOR_HUB_H

#include <linux/ioctl.h>
#include <linux/atomic.h>
#include <linux/init.h>

#if defined(CONFIG_MTK_SCP_SENSORHUB_V1)
#error CONFIG_MTK_SCP_SENSORHUB_V1 should not configed
#elif defined(CONFIG_NANOHUB)

#define EVT_NO_SENSOR_CONFIG_EVENT       0x00000300
#define SENSOR_RATE_ONCHANGE    0xFFFFFF01UL
#define SENSOR_RATE_ONESHOT     0xFFFFFF02UL

enum {
	CONFIG_CMD_DISABLE      = 0,
	CONFIG_CMD_ENABLE       = 1,
	CONFIG_CMD_FLUSH        = 2,
	CONFIG_CMD_CFG_DATA     = 3,
	CONFIG_CMD_CALIBRATE    = 4,
	CONFIG_CMD_SELF_TEST    = 5,
};

struct ConfigCmd {
	uint32_t evtType;
	uint64_t latency;
	uint32_t rate;
	uint8_t sensorType;
	uint8_t cmd;
	uint16_t flags;
	uint8_t data[];
} __packed;

struct SensorState {
	uint64_t latency;
	uint32_t rate;
	uint8_t sensorType;
	uint8_t alt;
	bool enable;
	bool timestamp_filter;
	atomic_t flushCnt;
	atomic64_t enableTime;
};

#define SCP_SENSOR_HUB_TEMP_BUFSIZE     256

//#define SCP_SENSOR_HUB_FIFO_SIZE        0x800000
#define SCP_KFIFO_BUFFER_SIZE			(2048)
#define SCP_DIRECT_PUSH_FIFO_SIZE       8192

#define SCP_SENSOR_HUB_SUCCESS          0
#define SCP_SENSOR_HUB_FAILURE          (-1)

#define SCP_SENSOR_HUB_X				0
#define SCP_SENSOR_HUB_Y				1
#define SCP_SENSOR_HUB_Z				2
#define SCP_SENSOR_HUB_AXES_NUM			3

/* SCP_ACTION */
#define    SENSOR_HUB_ACTIVATE		0
#define    SENSOR_HUB_SET_DELAY		1
#define    SENSOR_HUB_GET_DATA		2
#define    SENSOR_HUB_BATCH			3
#define    SENSOR_HUB_SET_CONFIG	4
#define    SENSOR_HUB_SET_CUST		5
#define    SENSOR_HUB_NOTIFY		6
#define    SENSOR_HUB_BATCH_TIMEOUT 7
#define    SENSOR_HUB_SET_TIMESTAMP	8
#define    SENSOR_HUB_POWER_NOTIFY	9

/* SCP_NOTIFY EVENT */
#define    SCP_INIT_DONE			0
#define    SCP_FIFO_FULL			1
#define    SCP_NOTIFY				2
#define    SCP_BATCH_TIMEOUT		3
#define	   SCP_DIRECT_PUSH          4

struct sensor_vec_t {
	union {
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
			int32_t x_bias;
			int32_t y_bias;
			int32_t z_bias;
			int32_t reserved : 14;
			int32_t temp_result : 2;
			int32_t temperature : 16;
		};
		struct {
			int32_t azimuth;
			int32_t pitch;
			int32_t roll;
			int32_t scalar;
		};
	};
	uint32_t status;
};

struct heart_rate_event_t {
	int32_t bpm;
	int32_t status;
};

struct significant_motion_event_t {
	int32_t state;
};

struct step_counter_event_t {
	uint32_t accumulated_step_count;
};

struct step_detector_event_t {
	uint32_t step_detect;
};

struct floor_counter_event_t {
	uint32_t accumulated_floor_count;
};

enum gesture_type_t {
	GESTURE_NONE,
	SHAKE,
	TAP,
	TWIST,
	FLIP,
	SNAPSHOT,
	PICKUP,
	CHECK
};

struct gesture_t {
	int32_t probability;
};

struct pedometer_event_t {
	uint32_t accumulated_step_count;
	uint32_t accumulated_step_length;
	uint32_t step_frequency;
	uint32_t step_length;
};

struct pressure_vec_t {
	int32_t pressure;	/* Pa, i.e. hPa * 100 */
	int32_t temperature;
	uint32_t status;
};

struct proximity_vec_t {
	uint32_t steps;
	int32_t oneshot;
};

struct relative_humidity_vec_t {
	int32_t relative_humidity;
	int32_t temperature;
	uint32_t status;
};

struct sleepmonitor_event_t {
	int32_t state;		/* sleep, restless, awake */
};

enum fall_type {
	FALL_NONE,
	FALL,
	FLOP,
	FALL_MAX
};

struct fall_t {
	uint8_t probability[FALL_MAX];	/* 0~100 */
};

struct tilt_event_t {
	int32_t state;		/* 0,1 */
};

struct in_pocket_event_t {
	int32_t state;		/* 0,1 */
};

struct geofence_event_t {
	uint32_t state;  /* geofence [source, result, operation_mode] */
};

struct sar_event_t {
	struct {
		int32_t data[3];
		int32_t x_bias;
		int32_t y_bias;
		int32_t z_bias;
	};
	uint32_t status;
};

enum activity_type_t {
	STILL,
	STANDING,
	SITTING,
	LYING,
	ON_FOOT,
	WALKING,
	RUNNING,
	CLIMBING,
	ON_BICYCLE,
	IN_VEHICLE,
	TILTING,
	UNKNOWN,
	ACTIVITY_MAX
};

struct activity_t {
	uint8_t probability[ACTIVITY_MAX];	/* 0~100 */
};

struct data_unit_t {
	uint8_t sensor_type;
	uint8_t flush_action;
	uint8_t reserve[2];
	uint64_t time_stamp;
	union {
		struct sensor_vec_t accelerometer_t;
		struct sensor_vec_t gyroscope_t;
		struct sensor_vec_t magnetic_t;
		struct sensor_vec_t orientation_t;
		struct sensor_vec_t pdr_event;

		int32_t light;
		struct proximity_vec_t proximity_t;
		int32_t temperature;
		struct pressure_vec_t pressure_t;
		struct relative_humidity_vec_t relative_humidity_t;

		struct sensor_vec_t uncalibrated_acc_t;
		struct sensor_vec_t uncalibrated_mag_t;
		struct sensor_vec_t uncalibrated_gyro_t;

		struct pedometer_event_t pedometer_t;

		struct heart_rate_event_t heart_rate_t;
		struct significant_motion_event_t smd_t;
		struct step_detector_event_t step_detector_t;
		struct step_counter_event_t step_counter_t;
		struct floor_counter_event_t floor_counter_t;
		struct activity_t activity_data_t;
		struct gesture_t gesture_data_t;
		struct fall_t fall_data_t;
		struct tilt_event_t tilt_event;
		struct in_pocket_event_t inpocket_event;
		struct geofence_event_t geofence_data_t;
		struct sar_event_t sar_event;
		int32_t data[8];
	};
} __packed;

struct sensorFIFO {
	uint32_t rp;	/* use int for store DRAM FIFO LSB 32bit read pointer */
	uint32_t wp;
	uint32_t FIFOSize;
	uint32_t reserve;
	struct data_unit_t data[0];
};

struct SCP_SENSOR_HUB_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t data[11];
};

struct SCP_SENSOR_HUB_RSP {
	uint8_t sensorType;
	uint8_t action;
	int8_t errCode;
	uint8_t reserve[1];
	/* uint32_t    reserved[9]; */
};

struct SCP_SENSOR_HUB_ACTIVATE_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t enable;	/* 0 : disable ; 1 : enable */
	/* uint32_t    reserved[9]; */
};

#define SCP_SENSOR_HUB_ACTIVATE_RSP SCP_SENSOR_HUB_RSP
/* typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_ACTIVATE_RSP; */

struct SCP_SENSOR_HUB_SET_DELAY_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t delay;		/* ms */
	/* uint32_t    reserved[9]; */
};

#define SCP_SENSOR_HUB_SET_DELAY_RSP  SCP_SENSOR_HUB_RSP
/* typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_DELAY_RSP; */

struct SCP_SENSOR_HUB_GET_DATA_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* uint32_t    reserved[10]; */
};

struct SCP_SENSOR_HUB_GET_DATA_RSP {
	uint8_t sensorType;
	uint8_t action;
	int8_t errCode;
	uint8_t reserve[1];
	/* struct data_unit_t data_t; */
	union {
		int8_t int8_Data[0];
		int16_t int16_Data[0];
		int32_t int32_Data[0];
	} data;
};

struct SCP_SENSOR_HUB_BATCH_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t flag;
	uint8_t reserve[1];
	uint32_t period_ms;	/* batch reporting time in ms */
	uint32_t timeout_ms;	/* sampling time in ms */
	/* uint32_t    reserved[7]; */
};

#define SCP_SENSOR_HUB_BATCH_RSP SCP_SENSOR_HUB_RSP
/* typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_BATCH_RSP; */

struct SCP_SENSOR_HUB_SET_CONFIG_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* struct sensorFIFO   *bufferBase; */
	uint32_t bufferBase;/* use int to store buffer DRAM base LSB 32 bits */
	uint32_t bufferSize;
	uint64_t ap_timestamp;
	uint64_t arch_counter;
	/* uint32_t    reserved[8]; */
};

#define SCP_SENSOR_HUB_SET_CONFIG_RSP  SCP_SENSOR_HUB_RSP
/* typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_CONFIG_RSP; */

enum CUST_ACTION {
	CUST_ACTION_SET_CUST = 1,
	CUST_ACTION_SET_CALI,
	CUST_ACTION_RESET_CALI,
	CUST_ACTION_SET_TRACE,
	CUST_ACTION_SET_DIRECTION,
	CUST_ACTION_SHOW_REG,
	CUST_ACTION_GET_RAW_DATA,
	CUST_ACTION_SET_PS_THRESHOLD,
	CUST_ACTION_SHOW_ALSLV,
	CUST_ACTION_SHOW_ALSVAL,
	CUST_ACTION_SET_FACTORY,
	CUST_ACTION_GET_SENSOR_INFO,
};

struct SCP_SENSOR_HUB_CUST {
	enum CUST_ACTION action;
};

struct SCP_SENSOR_HUB_SET_CUST {
	enum CUST_ACTION action;
	int32_t data[0];
};

struct SCP_SENSOR_HUB_SET_TRACE {
	enum CUST_ACTION action;
	int trace;
};

struct SCP_SENSOR_HUB_SET_DIRECTION {
	enum CUST_ACTION action;
	int direction;
};

struct SCP_SENSOR_HUB_SET_FACTORY {
	enum CUST_ACTION	action;
	unsigned int	factory;
};

struct SCP_SENSOR_HUB_SET_CALI {
	enum CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
};

#define SCP_SENSOR_HUB_RESET_CALI   SCP_SENSOR_HUB_CUST
/* typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_RESET_CALI; */

struct SCP_SENSOR_HUB_SETPS_THRESHOLD {
	enum CUST_ACTION action;
	int32_t threshold[2];
};

#define SCP_SENSOR_HUB_SHOW_REG    SCP_SENSOR_HUB_CUST
#define SCP_SENSOR_HUB_SHOW_ALSLV  SCP_SENSOR_HUB_CUST
#define SCP_SENSOR_HUB_SHOW_ALSVAL SCP_SENSOR_HUB_CUST
/*
 * typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_REG;
 * typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSLV;
 * typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSVAL;
 */

struct SCP_SENSOR_HUB_GET_RAW_DATA {
	enum CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
};

struct mag_dev_info_t {
	char libname[16];
	int8_t layout;
	int8_t deviceid;
};

struct sensorInfo_t {
	char name[16];
	struct mag_dev_info_t mag_dev_info;
};

struct scp_sensor_hub_get_sensor_info {
	enum CUST_ACTION action;
	union {
		int32_t int32_data[0];
		struct sensorInfo_t sensorInfo;
	};
};

enum {
	USE_OUT_FACTORY_MODE = 0,
	USE_IN_FACTORY_MODE
};

struct SCP_SENSOR_HUB_SET_CUST_REQ {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	union {
		uint32_t custData[11];
		struct SCP_SENSOR_HUB_CUST cust;
		struct SCP_SENSOR_HUB_SET_CUST setCust;
		struct SCP_SENSOR_HUB_SET_CALI setCali;
		struct SCP_SENSOR_HUB_RESET_CALI resetCali;
		struct SCP_SENSOR_HUB_SET_TRACE setTrace;
		struct SCP_SENSOR_HUB_SET_DIRECTION setDirection;
		struct SCP_SENSOR_HUB_SHOW_REG showReg;
		struct SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
		struct SCP_SENSOR_HUB_SETPS_THRESHOLD setPSThreshold;
		struct SCP_SENSOR_HUB_SHOW_ALSLV showAlslv;
		struct SCP_SENSOR_HUB_SHOW_ALSVAL showAlsval;
		struct SCP_SENSOR_HUB_SET_FACTORY setFactory;
		struct scp_sensor_hub_get_sensor_info getInfo;
	};
};

struct SCP_SENSOR_HUB_SET_CUST_RSP {
	uint8_t sensorType;
	uint8_t action;
	uint8_t errCode;
	uint8_t reserve[1];
	union {
		uint32_t custData[11];
		struct SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
		struct scp_sensor_hub_get_sensor_info getInfo;
	};
};

struct SCP_SENSOR_HUB_NOTIFY_RSP {
	uint8_t sensorType;
	uint8_t action;
	uint8_t event;
	uint8_t reserve[1];
	union {
		int8_t		int8_Data[0];
		int16_t		int16_Data[0];
		int32_t		int32_Data[0];
		struct {
			uint32_t	currWp;
			uint64_t	scp_timestamp;
			uint64_t	arch_counter;
		};
	};
};

union SCP_SENSOR_HUB_DATA {
	struct SCP_SENSOR_HUB_REQ req;
	struct SCP_SENSOR_HUB_RSP rsp;
	struct SCP_SENSOR_HUB_ACTIVATE_REQ activate_req;
	struct SCP_SENSOR_HUB_ACTIVATE_RSP activate_rsp;
	struct SCP_SENSOR_HUB_SET_DELAY_REQ set_delay_req;
	struct SCP_SENSOR_HUB_SET_DELAY_RSP set_delay_rsp;
	struct SCP_SENSOR_HUB_GET_DATA_REQ get_data_req;
	struct SCP_SENSOR_HUB_GET_DATA_RSP get_data_rsp;
	struct SCP_SENSOR_HUB_BATCH_REQ batch_req;
	struct SCP_SENSOR_HUB_BATCH_RSP batch_rsp;
	struct SCP_SENSOR_HUB_SET_CONFIG_REQ set_config_req;
	struct SCP_SENSOR_HUB_SET_CONFIG_RSP set_config_rsp;
	struct SCP_SENSOR_HUB_SET_CUST_REQ set_cust_req;
	struct SCP_SENSOR_HUB_SET_CUST_RSP set_cust_rsp;
	struct SCP_SENSOR_HUB_NOTIFY_RSP notify_rsp;
};

typedef int (*SCP_sensorHub_handler)(struct data_unit_t *event,
	void *reserved);

int scp_sensorHub_req_send(union SCP_SENSOR_HUB_DATA *data,
	uint *len, unsigned int wait);
int scp_sensorHub_data_registration(uint8_t sensor,
	SCP_sensorHub_handler handler);
int sensor_enable_to_hub(uint8_t sensorType, int enabledisable);
int sensor_set_delay_to_hub(uint8_t sensorType, unsigned int delayms);
int sensor_get_data_from_hub(uint8_t sensorType,
	struct data_unit_t *data);
int sensor_set_cmd_to_hub(uint8_t sensorType,
	enum CUST_ACTION action, void *data);
int sensor_batch_to_hub(uint8_t sensorType,
	int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
int sensor_flush_to_hub(uint8_t sensorType);
int sensor_cfg_to_hub(uint8_t sensorType, uint8_t *data, uint8_t count);
int sensor_calibration_to_hub(uint8_t sensorType);
int sensor_selftest_to_hub(uint8_t sensorType);

extern int __init nanohub_init(void);
extern void __exit nanohub_cleanup(void);

#endif
#endif

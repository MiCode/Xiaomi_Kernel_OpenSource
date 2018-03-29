/* SCP sensor hub driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#ifndef SCP_SENSOR_HUB_H
#define SCP_SENSOR_HUB_H

#include <linux/ioctl.h>

#if defined(CONFIG_MTK_SCP_SENSORHUB_V1)
#define SCP_SENSOR_HUB_TEMP_BUFSIZE     256

#define SCP_SENSOR_HUB_FIFO_SIZE        0x800000
#define SCP_KFIFO_BUFFER_SIZE           (2048)
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
#define    SENSOR_HUB_MASK_NOTIFY	10
/* SCP_NOTIFY EVENT */
#define    SCP_INIT_DONE			0
#define    SCP_FIFO_FULL			1
#define    SCP_NOTIFY				2
#define    SCP_BATCH_TIMEOUT		3
#define	   SCP_DIRECT_PUSH          4


struct SCP_sensorData {
	uint8_t dataLength;
	uint8_t sensorType;
	uint8_t reserve[2];
	uint32_t timeStampH;
	uint32_t timeStampL;
	int16_t data[8];
};
typedef struct {
	union {
		int32_t v[3];
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
		};
		struct {
			int32_t azimuth;
			int32_t pitch;
			int32_t roll;
		};
	};
	union {
		int32_t scalar;
		union {
			int16_t bias[3];
			struct {
				int16_t x_bias;
				int16_t y_bias;
				int16_t z_bias;
			};
		};
	};
	uint16_t status;
} sensor_vec_t;

typedef struct {
	union {
		int32_t uncali[3];
		struct {
			int32_t x_uncali;
			int32_t y_uncali;
			int32_t z_uncali;
		};
	};
	union {
		int32_t bias[3];
		struct {
			int32_t x_bias;
			int32_t y_bias;
			int32_t z_bias;
		};
	};
} uncalibrated_event_t;

typedef struct {
	int32_t bpm;
	int32_t status;
} heart_rate_event_t;

typedef struct {
	int32_t state;
} significant_motion_event_t;

typedef struct {
	uint32_t accumulated_step_count;
} step_counter_event_t;

typedef struct {
	uint32_t step_detect;
} step_detector_event_t;

typedef enum {
	GESTURE_NONE,
	SHAKE,
	TAP,
	TWIST,
	FLIP,
	SNAPSHOT,
	PICKUP,
	CHECK
} gesture_type_t;

typedef struct {
	int32_t probability;
} gesture_t;

typedef struct {
	uint32_t accumulated_step_count;
	uint32_t accumulated_step_length;
	uint32_t step_frequency;
	uint32_t step_length;
} pedometer_event_t;

typedef struct {
	int32_t pressure;	/* Pa, i.e. hPa * 100 */
	int32_t temperature;
	uint32_t status;
} pressure_vec_t;

typedef struct {
	uint32_t steps;
	int32_t oneshot;
} proximity_vec_t;

typedef struct {
	int32_t relative_humidity;
	int32_t temperature;
	uint32_t status;
} relative_humidity_vec_t;


typedef struct {
	int32_t state;		/* sleep, restless, awake */
} sleepmonitor_event_t;

typedef enum {
	FALL_NONE,
	FALL,
	FLOP,
	FALL_MAX
} fall_type;

typedef struct {
	uint8_t probability[FALL_MAX];	/* 0~100 */
} fall_t;

typedef struct {
	int32_t state;		/* 0,1 */
} tilt_event_t;

typedef struct {
	int32_t state;		/* 0,1 */
} in_pocket_event_t;

typedef enum {
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
} activity_type_t;

typedef struct {
	uint8_t probability[ACTIVITY_MAX];	/* 0~100 */
} activity_t;


struct data_unit_t {
	uint8_t sensor_type;
	uint8_t flush_action;
	uint8_t reserve[2];
	uint64_t time_stamp;	/* ms on CM4 time kick */
	int64_t time_stamp_gpt;	/* ms for sensor GPT AP SCP sync time */
	union {
		sensor_vec_t accelerometer_t;
		sensor_vec_t gyroscope_t;
		sensor_vec_t magnetic_t;
		sensor_vec_t orientation_t;
		sensor_vec_t pdr_event;

		int32_t light;
		proximity_vec_t proximity_t;
		int32_t temperature;
		pressure_vec_t pressure_t;
		relative_humidity_vec_t relative_humidity_t;

		sensor_vec_t uncalibrated_acc_t;
		sensor_vec_t uncalibrated_mag_t;
		sensor_vec_t uncalibrated_gyro_t;

		pedometer_event_t pedometer_t;

		heart_rate_event_t heart_rate_t;
		significant_motion_event_t smd_t;
		step_detector_event_t step_detector_t;
		step_counter_event_t step_counter_t;
		activity_t activity_data_t;
		gesture_t gesture_data_t;
		fall_t fall_data_t;
		tilt_event_t tilt_event;
		in_pocket_event_t inpocket_event;
		int32_t data[7];
	};
} __packed;


struct sensorFIFO {
/* volatile struct SCP_sensorData * volatile rp; */
/* volatile struct SCP_sensorData * volatile wp; */
	uint32_t rp;			/* use int for store DRAM FIFO LSB 32bit read pointer */
	uint32_t wp;
	uint32_t FIFOSize;
	uint32_t reserve;
	struct data_unit_t data[0];
};

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t data[11];
} SCP_SENSOR_HUB_REQ;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	int8_t errCode;
	uint8_t reserve[1];
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t enable;	/* 0 : disable ; 1 : enable */
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_ACTIVATE_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_ACTIVATE_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t delay;		/* ms */
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_SET_DELAY_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_DELAY_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* uint32_t    reserved[10]; */
} SCP_SENSOR_HUB_GET_DATA_REQ;

typedef struct {
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
} SCP_SENSOR_HUB_GET_DATA_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t flag;
	uint8_t reserve[1];
	uint32_t period_ms;	/* batch reporting time in ms */
	uint32_t timeout_ms;	/* sampling time in ms */
	/* uint32_t    reserved[7]; */
} SCP_SENSOR_HUB_BATCH_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_BATCH_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* struct sensorFIFO   *bufferBase; */
	uint32_t bufferBase;/* use int to store buffer DRAM base LSB 32 bits */
	uint32_t bufferSize;
	uint32_t directPushbufferBase;/* use int to store buffer DRAM base LSB 32 bits */
	uint32_t directPushbufferSize;
	uint64_t ap_timestamp;
	/* uint32_t    reserved[8]; */
} SCP_SENSOR_HUB_SET_CONFIG_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_CONFIG_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* struct sensorFIFO   *bufferBase; */
	uint32_t if_mask_or_not;/* use int to store buffer DRAM base LSB 32 bits */
} SCP_SENSOR_HUB_MASK_NOTIFY_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_MASK_NOTIFY_RSP;

typedef enum {
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
} CUST_ACTION;

typedef struct {
	CUST_ACTION action;
} SCP_SENSOR_HUB_CUST;

typedef struct {
	CUST_ACTION action;
	int32_t data[0];
} SCP_SENSOR_HUB_SET_CUST;

typedef struct {
	CUST_ACTION action;
	int trace;
} SCP_SENSOR_HUB_SET_TRACE;

typedef struct {
	CUST_ACTION action;
	int direction;
} SCP_SENSOR_HUB_SET_DIRECTION;

typedef struct {
	CUST_ACTION		action;
	unsigned int	factory;
} SCP_SENSOR_HUB_SET_FACTORY;

typedef struct {
	CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
} SCP_SENSOR_HUB_SET_CALI;

typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_RESET_CALI;
typedef struct {
	CUST_ACTION action;
	int32_t threshold[2];
} SCP_SENSOR_HUB_SETPS_THRESHOLD;

typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_REG;
typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSLV;
typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSVAL;

typedef struct {
	CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
} SCP_SENSOR_HUB_GET_RAW_DATA;

enum {
	USE_OUT_FACTORY_MODE = 0,
	USE_IN_FACTORY_MODE
};

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	union {
		uint32_t custData[10];
		SCP_SENSOR_HUB_CUST cust;
		SCP_SENSOR_HUB_SET_CUST setCust;
		SCP_SENSOR_HUB_SET_CALI setCali;
		SCP_SENSOR_HUB_RESET_CALI resetCali;
		SCP_SENSOR_HUB_SET_TRACE setTrace;
		SCP_SENSOR_HUB_SET_DIRECTION setDirection;
		SCP_SENSOR_HUB_SHOW_REG showReg;
		SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
		SCP_SENSOR_HUB_SETPS_THRESHOLD setPSThreshold;
		SCP_SENSOR_HUB_SHOW_ALSLV showAlslv;
		SCP_SENSOR_HUB_SHOW_ALSVAL showAlsval;
		SCP_SENSOR_HUB_SET_FACTORY	setFactory;
	};
} SCP_SENSOR_HUB_SET_CUST_REQ;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t errCode;
	uint8_t reserve[1];
	union {
		uint32_t custData[9];
		SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
	};
} SCP_SENSOR_HUB_SET_CUST_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t event;
	uint8_t reserve[1];
	union {
		int8_t		int8_Data[0];
		int16_t		int16_Data[0];
		int32_t		int32_Data[0];
	} data;
} SCP_SENSOR_HUB_NOTIFY_RSP;

typedef union {
	SCP_SENSOR_HUB_REQ req;
	SCP_SENSOR_HUB_RSP rsp;
	SCP_SENSOR_HUB_ACTIVATE_REQ activate_req;
	SCP_SENSOR_HUB_ACTIVATE_RSP activate_rsp;
	SCP_SENSOR_HUB_SET_DELAY_REQ set_delay_req;
	SCP_SENSOR_HUB_SET_DELAY_RSP set_delay_rsp;
	SCP_SENSOR_HUB_GET_DATA_REQ get_data_req;
	SCP_SENSOR_HUB_GET_DATA_RSP get_data_rsp;
	SCP_SENSOR_HUB_BATCH_REQ batch_req;
	SCP_SENSOR_HUB_BATCH_RSP batch_rsp;
	SCP_SENSOR_HUB_SET_CONFIG_REQ set_config_req;
	SCP_SENSOR_HUB_SET_CONFIG_RSP set_config_rsp;
	SCP_SENSOR_HUB_MASK_NOTIFY_REQ mask_notify_req;
	SCP_SENSOR_HUB_MASK_NOTIFY_RSP mask_notify_rsp;
	SCP_SENSOR_HUB_SET_CUST_REQ set_cust_req;
	SCP_SENSOR_HUB_SET_CUST_RSP set_cust_rsp;
	SCP_SENSOR_HUB_NOTIFY_RSP notify_rsp;
} SCP_SENSOR_HUB_DATA, *SCP_SENSOR_HUB_DATA_P;

typedef int (*SCP_sensorHub_handler)(struct data_unit_t *event, void *reserved);

int SCP_sensorHub_req_send(SCP_SENSOR_HUB_DATA_P data, uint *len, unsigned int wait);
int SCP_sensorHub_data_registration(uint8_t sensor, SCP_sensorHub_handler handler);
int sensor_enable_to_hub(uint8_t sensorType, int enabledisable);
int sensor_set_delay_to_hub(uint8_t sensorType, unsigned int delayms);
int sensor_get_data_from_hub(uint8_t sensorType, struct data_unit_t *data);
int sensor_set_cmd_to_hub(uint8_t sensorType, CUST_ACTION action, void *data);
int sensor_batch_to_hub(uint8_t sensorType, int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
int sensor_flush_to_hub(uint8_t sensorType);
int sensor_cfg_to_hub(uint8_t sensorType, uint8_t *data, uint8_t count);
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
};

#define SCP_SENSOR_HUB_TEMP_BUFSIZE     256

#define SCP_SENSOR_HUB_FIFO_SIZE        0x800000
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


struct SCP_sensorData {
	uint8_t dataLength;
	uint8_t sensorType;
	uint8_t reserve[2];
	uint32_t timeStampH;
	uint32_t timeStampL;
	int16_t data[8];
};
typedef struct {
	union {
		int32_t v[3];
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
		};
		struct {
			int32_t azimuth;
			int32_t pitch;
			int32_t roll;
		};
	};
	union {
		int32_t scalar;
		union {
			int16_t bias[3];
			struct {
				int16_t x_bias;
				int16_t y_bias;
				int16_t z_bias;
			};
		};
	};
	uint16_t status;
} sensor_vec_t;

typedef struct {
	union {
		int32_t uncali[3];
		struct {
			int32_t x_uncali;
			int32_t y_uncali;
			int32_t z_uncali;
		};
	};
	union {
		int32_t bias[3];
		struct {
			int32_t x_bias;
			int32_t y_bias;
			int32_t z_bias;
		};
	};
} uncalibrated_event_t;

typedef struct {
	int32_t bpm;
	int32_t status;
} heart_rate_event_t;

typedef struct {
	int32_t state;
} significant_motion_event_t;

typedef struct {
	uint32_t accumulated_step_count;
} step_counter_event_t;

typedef struct {
	uint32_t step_detect;
} step_detector_event_t;

typedef enum {
	GESTURE_NONE,
	SHAKE,
	TAP,
	TWIST,
	FLIP,
	SNAPSHOT,
	PICKUP,
	CHECK
} gesture_type_t;

typedef struct {
	int32_t probability;
} gesture_t;

typedef struct {
	uint32_t accumulated_step_count;
	uint32_t accumulated_step_length;
	uint32_t step_frequency;
	uint32_t step_length;
} pedometer_event_t;

typedef struct {
	int32_t pressure;	/* Pa, i.e. hPa * 100 */
	int32_t temperature;
	uint32_t status;
} pressure_vec_t;

typedef struct {
	uint32_t steps;
	int32_t oneshot;
} proximity_vec_t;

typedef struct {
	int32_t relative_humidity;
	int32_t temperature;
	uint32_t status;
} relative_humidity_vec_t;


typedef struct {
	int32_t state;		/* sleep, restless, awake */
} sleepmonitor_event_t;

typedef enum {
	FALL_NONE,
	FALL,
	FLOP,
	FALL_MAX
} fall_type;

typedef struct {
	uint8_t probability[FALL_MAX];	/* 0~100 */
} fall_t;

typedef struct {
	int32_t state;		/* 0,1 */
} tilt_event_t;

typedef struct {
	int32_t state;		/* 0,1 */
} in_pocket_event_t;

typedef enum {
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
} activity_type_t;

typedef struct {
	uint8_t probability[ACTIVITY_MAX];	/* 0~100 */
} activity_t;


struct data_unit_t {
	uint8_t sensor_type;
	uint8_t flush_action;
	uint8_t reserve[2];
	uint64_t time_stamp;	/* ms on CM4 time kick */
	int64_t time_stamp_gpt;	/* ms for sensor GPT AP SCP sync time */
	union {
		sensor_vec_t accelerometer_t;
		sensor_vec_t gyroscope_t;
		sensor_vec_t magnetic_t;
		sensor_vec_t orientation_t;
		sensor_vec_t pdr_event;

		int32_t light;
		proximity_vec_t proximity_t;
		int32_t temperature;
		pressure_vec_t pressure_t;
		relative_humidity_vec_t relative_humidity_t;

		sensor_vec_t uncalibrated_acc_t;
		sensor_vec_t uncalibrated_mag_t;
		sensor_vec_t uncalibrated_gyro_t;

		pedometer_event_t pedometer_t;

		heart_rate_event_t heart_rate_t;
		significant_motion_event_t smd_t;
		step_detector_event_t step_detector_t;
		step_counter_event_t step_counter_t;
		activity_t activity_data_t;
		gesture_t gesture_data_t;
		fall_t fall_data_t;
		tilt_event_t tilt_event;
		in_pocket_event_t inpocket_event;
		int32_t data[7];
	};
} __packed;

struct sensorFIFO {
/* volatile struct SCP_sensorData * volatile rp; */
/* volatile struct SCP_sensorData * volatile wp; */
	uint32_t rp;			/* use int for store DRAM FIFO LSB 32bit read pointer */
	uint32_t wp;
	uint32_t FIFOSize;
	uint32_t reserve;
	struct data_unit_t data[0];
};

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t data[11];
} SCP_SENSOR_HUB_REQ;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	int8_t errCode;
	uint8_t reserve[1];
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t enable;	/* 0 : disable ; 1 : enable */
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_ACTIVATE_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_ACTIVATE_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	uint32_t delay;		/* ms */
	/* uint32_t    reserved[9]; */
} SCP_SENSOR_HUB_SET_DELAY_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_DELAY_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* uint32_t    reserved[10]; */
} SCP_SENSOR_HUB_GET_DATA_REQ;

typedef struct {
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
} SCP_SENSOR_HUB_GET_DATA_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t flag;
	uint8_t reserve[1];
	uint32_t period_ms;	/* batch reporting time in ms */
	uint32_t timeout_ms;	/* sampling time in ms */
	/* uint32_t    reserved[7]; */
} SCP_SENSOR_HUB_BATCH_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_BATCH_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	/* struct sensorFIFO   *bufferBase; */
	uint32_t bufferBase;/* use int to store buffer DRAM base LSB 32 bits */
	uint32_t bufferSize;
	uint64_t ap_timestamp;
	/* uint32_t    reserved[8]; */
} SCP_SENSOR_HUB_SET_CONFIG_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_CONFIG_RSP;

typedef enum {
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
} CUST_ACTION;

typedef struct {
	CUST_ACTION action;
} SCP_SENSOR_HUB_CUST;

typedef struct {
	CUST_ACTION action;
	int32_t data[0];
} SCP_SENSOR_HUB_SET_CUST;

typedef struct {
	CUST_ACTION action;
	int trace;
} SCP_SENSOR_HUB_SET_TRACE;

typedef struct {
	CUST_ACTION action;
	int direction;
} SCP_SENSOR_HUB_SET_DIRECTION;

typedef struct {
	CUST_ACTION		action;
	unsigned int	factory;
} SCP_SENSOR_HUB_SET_FACTORY;

typedef struct {
	CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
} SCP_SENSOR_HUB_SET_CALI;

typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_RESET_CALI;
typedef struct {
	CUST_ACTION action;
	int32_t threshold[2];
} SCP_SENSOR_HUB_SETPS_THRESHOLD;

typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_REG;
typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSLV;
typedef SCP_SENSOR_HUB_CUST SCP_SENSOR_HUB_SHOW_ALSVAL;

typedef struct {
	CUST_ACTION action;
	union {
		int8_t int8_data[0];
		uint8_t uint8_data[0];
		int16_t int16_data[0];
		uint16_t uint16_data[0];
		int32_t int32_data[0];
		uint32_t uint32_data[SCP_SENSOR_HUB_AXES_NUM];
	};
} SCP_SENSOR_HUB_GET_RAW_DATA;

enum {
	USE_OUT_FACTORY_MODE = 0,
	USE_IN_FACTORY_MODE
};

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t reserve[2];
	union {
		uint32_t custData[10];
		SCP_SENSOR_HUB_CUST cust;
		SCP_SENSOR_HUB_SET_CUST setCust;
		SCP_SENSOR_HUB_SET_CALI setCali;
		SCP_SENSOR_HUB_RESET_CALI resetCali;
		SCP_SENSOR_HUB_SET_TRACE setTrace;
		SCP_SENSOR_HUB_SET_DIRECTION setDirection;
		SCP_SENSOR_HUB_SHOW_REG showReg;
		SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
		SCP_SENSOR_HUB_SETPS_THRESHOLD setPSThreshold;
		SCP_SENSOR_HUB_SHOW_ALSLV showAlslv;
		SCP_SENSOR_HUB_SHOW_ALSVAL showAlsval;
		SCP_SENSOR_HUB_SET_FACTORY	setFactory;
	};
} SCP_SENSOR_HUB_SET_CUST_REQ;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t errCode;
	uint8_t reserve[1];
	union {
		uint32_t custData[9];
		SCP_SENSOR_HUB_GET_RAW_DATA getRawData;
	};
} SCP_SENSOR_HUB_SET_CUST_RSP;

typedef struct {
	uint8_t sensorType;
	uint8_t action;
	uint8_t event;
	uint8_t reserve[1];
	union {
		int8_t		int8_Data[0];
		int16_t		int16_Data[0];
		int32_t		int32_Data[0];
		uint32_t	currWp;
	} data;
} SCP_SENSOR_HUB_NOTIFY_RSP;

typedef union {
	SCP_SENSOR_HUB_REQ req;
	SCP_SENSOR_HUB_RSP rsp;
	SCP_SENSOR_HUB_ACTIVATE_REQ activate_req;
	SCP_SENSOR_HUB_ACTIVATE_RSP activate_rsp;
	SCP_SENSOR_HUB_SET_DELAY_REQ set_delay_req;
	SCP_SENSOR_HUB_SET_DELAY_RSP set_delay_rsp;
	SCP_SENSOR_HUB_GET_DATA_REQ get_data_req;
	SCP_SENSOR_HUB_GET_DATA_RSP get_data_rsp;
	SCP_SENSOR_HUB_BATCH_REQ batch_req;
	SCP_SENSOR_HUB_BATCH_RSP batch_rsp;
	SCP_SENSOR_HUB_SET_CONFIG_REQ set_config_req;
	SCP_SENSOR_HUB_SET_CONFIG_RSP set_config_rsp;
	SCP_SENSOR_HUB_SET_CUST_REQ set_cust_req;
	SCP_SENSOR_HUB_SET_CUST_RSP set_cust_rsp;
	SCP_SENSOR_HUB_NOTIFY_RSP notify_rsp;
} SCP_SENSOR_HUB_DATA, *SCP_SENSOR_HUB_DATA_P;

typedef int (*SCP_sensorHub_handler)(struct data_unit_t *event, void *reserved);

int SCP_sensorHub_req_send(SCP_SENSOR_HUB_DATA_P data, uint *len, unsigned int wait);
int SCP_sensorHub_data_registration(uint8_t sensor, SCP_sensorHub_handler handler);
int sensor_enable_to_hub(uint8_t sensorType, int enabledisable);
int sensor_set_delay_to_hub(uint8_t sensorType, unsigned int delayms);
int sensor_get_data_from_hub(uint8_t sensorType, struct data_unit_t *data);
int sensor_set_cmd_to_hub(uint8_t sensorType, CUST_ACTION action, void *data);
int sensor_batch_to_hub(uint8_t sensorType, int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs);
int sensor_flush_to_hub(uint8_t sensorType);
int sensor_cfg_to_hub(uint8_t sensorType, uint8_t *data, uint8_t count);
#endif
#endif

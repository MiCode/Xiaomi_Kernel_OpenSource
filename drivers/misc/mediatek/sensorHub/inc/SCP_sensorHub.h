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
#include <cust_acc.h>
#include <cust_mag.h>

#define SCP_SENSOR_HUB_TEMP_BUFSIZE     256

#define SCP_SENSOR_HUB_FIFO_SIZE        32768

#define SCP_SENSOR_HUB_SUCCESS          0
#define SCP_SENSOR_HUB_FAILURE          (-1)

struct scp_acc_hw {
	int i2c_num;		/*!< the i2c bus used by the chip */
	int direction;		/*!< the direction of the chip */
	int power_id;		/*!< the VDD LDO ID of the chip,
				MT6516_POWER_NONE means the power is always on */
	int power_vol;		/*!< the VDD Power Voltage used by the chip */
	int firlen;		/*!< the length of low pass filter */
	int reserved;
	unsigned char i2c_addr[G_CUST_I2C_ADDR_NUM];
	int power_vio_id;	/*!< the VIO LDO ID of the chip,
				MT6516_POWER_NONE means the power is always on */
	int power_vio_vol;	/*!< the VIO Power Voltage used by the chip */
	bool is_batch_supported;
};

struct scp_mag_hw {
	int i2c_num;		/*!< the i2c bus used by the chip */
	int direction;		/*!< the direction of the chip */
	int power_id;		/*!< the VDD LDO ID of the chip,
				MT6516_POWER_NONE means the power is always on */
	int power_vol;		/*!< the VDD Power Voltage used by the chip */
	int reserved;
	unsigned char i2c_addr[M_CUST_I2C_ADDR_NUM];
	int power_vio_id;	/*!< the VIO LDO ID of the chip,
				MT6516_POWER_NONE means the power is always on */
	int power_vio_vol;	/*!< the VIO Power Voltage used by the chip */
	bool is_batch_supported;
};

typedef enum {
	SENSOR_HUB_ACTIVATE = 0,
	SENSOR_HUB_SET_DELAY,
	SENSOR_HUB_GET_DATA,
	SENSOR_HUB_BATCH,
	SENSOR_HUB_SET_CONFIG,
	SENSOR_HUB_SET_CUST,
	SENSOR_HUB_NOTIFY,
} SCP_ACTION;

typedef enum {
	SCP_INIT_DONE = 0,
	SCP_FIFO_FULL,
	SCP_NOTIFY,
} SCP_NOTIFY_EVENT;

struct SCP_sensorData {
	uint8_t dataLength;
	uint8_t sensorType;
	uint8_t reserve[2];
	uint32_t timeStampH;
	uint32_t timeStampL;
	int16_t data[8];
};

struct sensorFIFO {
	int rp;/* use int for store DRAM FIFO LSB 32bit read pointer */
	int wp;
	uint32_t FIFOSize;
	struct SCP_sensorData data[0];
};

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t data[10];
} SCP_SENSOR_HUB_REQ;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t errCode;
} SCP_SENSOR_HUB_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t enable;	/* 0 : disable ; 1 : enable */
} SCP_SENSOR_HUB_ACTIVATE_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_ACTIVATE_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t delay;		/*ms*/
} SCP_SENSOR_HUB_SET_DELAY_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_DELAY_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
} SCP_SENSOR_HUB_GET_DATA_REQ;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t errCode;
	union {
		int8_t int8_Data[0];
		int16_t int16_Data[0];
		int32_t int32_Data[0];
	};
} SCP_SENSOR_HUB_GET_DATA_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t flag;/*see SENSORS_BATCH_WAKE_UPON_FIFO_FULL
			definition in hardware/libhardware/include/hardware/sensors.h*/
	uint32_t period_ms;/*batch reporting time in ms*/
	uint32_t timeout_ms;/*sampling time in ms*/
} SCP_SENSOR_HUB_BATCH_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_BATCH_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	int bufferBase;/*use int to store buffer DRAM base LSB 32 bits*/
	uint32_t bufferSize;
} SCP_SENSOR_HUB_SET_CONFIG_REQ;

typedef SCP_SENSOR_HUB_RSP SCP_SENSOR_HUB_SET_CONFIG_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t custData[10];
} SCP_SENSOR_HUB_SET_CUST_REQ;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	uint32_t errCode;
	uint32_t custData[0];
} SCP_SENSOR_HUB_SET_CUST_RSP;

typedef struct {
	uint32_t sensorType;
	SCP_ACTION action;
	SCP_NOTIFY_EVENT event;
	uint32_t data[0];
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

typedef int (*SCP_sensorHub_handler) (void *data, uint len);

int SCP_sensorHub_req_send(SCP_SENSOR_HUB_DATA_P data, uint *len, unsigned int wait);
int SCP_sensorHub_rsp_registration(int sensor, SCP_sensorHub_handler handler);

#endif

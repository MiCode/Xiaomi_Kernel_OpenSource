/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_FLP_DEF_H_
#define _MTK_FLP_DEF_H_

#define FLP_TECH_MASK_GNSS      (1U<<0)
#define FLP_TECH_MASK_WIFI      (1U<<1)
#define FLP_TECH_MASK_SENSORS   (1U<<2)
#define FLP_TECH_MASK_CELL      (1U<<3)
#define FLP_TECH_MASK_BLUETOOTH (1U<<4)


#define FLP_BATCH_WAKEUP_ON_FIFO_FULL			0x01
#define FLP_BATCH_CALLBACK_ON_LOCATION_FIX		0x02

#define FLP_RESULT_SUCCESS                       0
#define FLP_RESULT_ERROR                        -1
#define FLP_RESULT_INSUFFICIENT_MEMORY          -2
#define FLP_RESULT_TOO_MANY_GEOFENCES           -3
#define FLP_RESULT_ID_EXISTS                    -4
#define FLP_RESULT_ID_UNKNOWN                   -5
#define FLP_RESULT_INVALID_GEOFENCE_TRANSITION  -6

#define FLP_STATUS_LOCATION_AVAILABLE         0
#define FLP_STATUS_LOCATION_UNAVAILABLE       1


/** FlpLocation has valid latitude and longitude. */
#define FLP_LOCATION_HAS_LAT_LONG   (1U<<0)
/** FlpLocation has valid altitude. */
#define FLP_LOCATION_HAS_ALTITUDE   (1U<<1)
/** FlpLocation has valid speed. */
#define FLP_LOCATION_HAS_SPEED      (1U<<2)
/** FlpLocation has valid bearing. */
#define FLP_LOCATION_HAS_BEARING    (1U<<4)
/** FlpLocation has valid accuracy. */
#define FLP_LOCATION_HAS_ACCURACY   (1U<<8)

struct conn_flp_batch_options {
	double max_power_allocation_mW;
	uint32_t sources_to_use;
	/*
	 * reference FLP_BATCH_WAKEUP_ON_FIFO_FULL & FLP_BATCH_CALLBACK_ON_LOCATION_FIX
	 */
	uint32_t flags;
	int64_t period_ns;
	float smallest_displacement_meters;
};

typedef uint16_t FlpLocationFlags;
typedef int64_t FlpUtcTime;

struct conn_flp_location {
	/** set to sizeof(FlpLocation) */
	uint32_t size;
	/** Flags associated with the location object. */
	FlpLocationFlags flags;
	/** Represents latitude in degrees. */
	double          latitude;
	/** Represents longitude in degrees. */
	double          longitude;
	/* Represents altitude in meters above the WGS 84 reference */
	/* ellipsoid. */
	double          altitude;
	/** Represents speed in meters per second. */
	float           speed;
	/** Represents heading in degrees. */
	float           bearing;
	/** Represents expected accuracy in meters. */
	float           accuracy;
	/** Timestamp for the location fix. */
	FlpUtcTime      timestamp;
	/** Sources used, will be Bitwise OR of the FLP_TECH_MASK bits. */
	uint32_t         sources_used;
};

struct conn_flp_data_init {
	int status;
	uint32_t batch_size;
};

struct conn_flp_data_option {
	uint32_t id;
	struct conn_flp_batch_options options;
};


/************************************************/
/* MSG DEFINITION                               */
/************************************************/
enum conn_flp_hal2scp_msg_id {
	FLP_HAL2SCP_NONE = 0,
	FLP_HAL2SCP_INIT,
	FLP_HAL2SCP_START_BATCHING,
	FLP_HAL2SCP_UPDATE_BATCHING,
	FLP_HAL2SCP_STOP_BATCHING,
	FLP_HAL2SCP_CLEANUP,
	FLP_HAL2SCP_GET_BATCHED_LOCATION,
	FLP_HAL2SCP_FLUSH_BATCHED_LOCATION,

	FLP_KERN2SCP_LOCATION_ACK
};

enum conn_flp_scp2hal_msg_id {
	FLP_SCP2HAL_NONE = 0,
	FLP_SCP2HAL_INIT_ACK,
	FLP_SCP2HAL_START_BATCHING_ACK,
	FLP_SCP2HAL_UPDATE_BATCHING_ACK,
	FLP_SCP2HAL_STOP_BATCHING_ACK,
	FLP_SCP2HAL_LOCATION,
	FLP_SCP2HAL_STATUS,
	FLP_SCP2HAL_REQUEST_NLP,
	FLP_SCP2HAL_SCP_RESTART
};


#endif // _MTK_FLP_DEF_H_
